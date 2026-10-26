"""Reduce Craft's runtime collection without changing the installed SDK.

Keep native dependency closures, dynamically loaded platform/QML plugins, and
the supported UI languages. Run before Craft signs or archives the bundle.
"""
from __future__ import annotations

from collections import deque
from pathlib import Path
import re


def native_dependencies(path, target):
    if target == "windows":
        import pefile
        pe = pefile.PE(str(path), fast_load=True)
        try:
            pe.parse_data_directories(directories=[
                pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
                pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT"],
            ])
            return {entry.dll.decode("ascii").lower()
                    for table in ("DIRECTORY_ENTRY_IMPORT", "DIRECTORY_ENTRY_DELAY_IMPORT")
                    for entry in getattr(pe, table, [])}
        finally:
            pe.close()
    from macholib.MachO import MachO
    return {name.rsplit("/", 1)[-1].lower()
            for header in MachO(str(path)).headers
            for _, _, name in header.walkRelocatables()}


def is_binary(path, target):
    with path.open("rb") as source:
        magic = source.read(4)
    if target == "windows":
        return magic[:2] == b"MZ"
    return magic in (b"\xcf\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xce\xfa\xed\xfe",
                     b"\xfe\xed\xfa\xce", b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",
                     b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca")


def library(path, root, target):
    relative = path.relative_to(root)
    if target == "windows":
        return relative.parent.as_posix() == "bin" and path.suffix.lower() == ".dll"
    parts = relative.parts
    return ((relative.parent.as_posix() == "lib" and path.suffix == ".dylib")
            or ("Frameworks" in parts and path.parent.name == "Frameworks")
            or any(part.endswith(".framework") for part in parts))


def unused_data(relative, languages):
    name = relative.as_posix()
    # BreezeIcons embeds the actual icons. These two large directories contain
    # desktop cursor themes, not application icons.
    if re.search(r"(?:^|/)(?:share|data)/icons/(?:Breeze_Light|breeze_cursors)(?:/|$)", name):
        return "cursor-themes"
    if re.search(r"(?:^|/)(?:share|data)/(?:vpl|ffmpeg)/examples/", name):
        return "examples"
    locale = re.search(r"(?:^|/)(?:locale|locale-bundle)/([^/]+)/", name)
    if locale and locale[1].split("_")[0].split("@")[0] not in languages:
        return "unsupported-translations"
    translation = re.search(r"(?:^|/)(?:translations|Translations)/[^/]+_([a-z]{2,3})(?:_[A-Z]{2})?\.qm$", name)
    if translation and translation[1] not in languages:
        return "unsupported-translations"
    # These modules are not imported by Meteo, Kirigami or the desktop style.
    if re.search(r"(?:^|/)(?:qml|bin)/(?:QtMultimedia|QtTextToSpeech|QtTest|Qt/test)(?:/|$)", name):
        return "unused-qml-modules"
    if re.search(r"(?:^|/)(?:plugins|bin)/(?:qmlls|qmltooling|designer)(?:/|$)", name):
        return "development-plugins"
    return None


def plugin_gate(relative, target):
    name = relative.as_posix()
    for category, module in (("multimedia", "Multimedia"), ("sqldrivers", "Sql"), ("texttospeech", "TextToSpeech")):
        if re.search(rf"(?:^|/)(?:plugins|bin|PlugIns)/{category}/", name):
            return ("qt6" + module + ".dll" if target == "windows" else "qt" + module).lower()
    return None


def optimize_bundle(root, target, languages=("en", "pl"), dependencies=None):
    if target not in ("windows", "macos", "android"):
        raise ValueError(f"Unsupported bundle target: {target}")
    root = Path(root).resolve()
    files = [p for p in root.rglob("*") if p.is_file() and not p.is_symlink()]
    sizes = {p: p.stat().st_size for p in files}
    removed = {p: reason for p in files if (reason := unused_data(p.relative_to(root), languages))}
    libraries_removed = []
    if target != "android":
        # Resolve every dependency before deleting anything. Import/format errors
        # must fail packaging rather than create a silently incomplete bundle.
        binaries = {p for p in files if p not in removed and is_binary(p, target)}
        candidates = {p for p in binaries if library(p, root, target)}
        by_name = {}
        for p in binaries:
            by_name.setdefault(p.name.lower(), set()).add(p)
        aliases = []
        for p in root.rglob("*"):
            if not p.is_symlink():
                continue
            real = p.resolve()
            # Craft may copy an absolute SDK symlink before its macOS packager
            # relocates it. Associate that alias with the staged copy, never
            # inspect or modify the library outside our collection directory.
            targets = {real} if real in binaries else by_name.get(real.name.lower(), set())
            if targets:
                aliases.append((p, set(targets)))
                by_name.setdefault(p.name.lower(), set()).update(targets)
        read = dependencies or native_dependencies
        imports = {p: read(p, target) for p in binaries}
        gates = {p: plugin_gate(p.relative_to(root), target) for p in binaries - candidates}
        roots = {p for p in binaries - candidates if not gates[p]}
        # Qt loads TLS and optional software graphics libraries by name.
        roots.update(p for p in candidates if re.match(
            r"(?:lib)?(?:ssl|crypto)[.-]|(?:opengl32sw|d3dcompiler.*|dxcompiler|dxil)\.dll$", p.name, re.I))
        needed = set()
        queue = deque(roots)
        while queue:
            p = queue.popleft()
            if p in needed:
                continue
            needed.add(p)
            for name in imports[p]:
                queue.extend(by_name.get(name.lower(), ()))
            # A Qt module needs its runtime backends even though it loads them
            # dynamically. Unused modules do not pull in their entire SDK.
            for plugin, gate in gates.items():
                if gate and gate == p.name.lower():
                    queue.append(plugin)
        for p in candidates - needed:
            removed[p] = "unreferenced-libraries"
            libraries_removed.append(str(p.relative_to(root)))
        for p, gate in gates.items():
            if gate and p not in needed:
                removed[p] = "unused-backends"
        for alias, targets in aliases:
            if targets.issubset(removed):
                removed[alias] = "unreferenced-aliases"
        # Remove the resources and symlinks of unused frameworks as a unit.
        for p in candidates - needed:
            framework = next((parent for parent in p.parents if parent.name.endswith(".framework")), None)
            if framework and not any(framework in used.parents for used in needed):
                for item in framework.rglob("*"):
                    if item.is_file() or item.is_symlink():
                        removed[item] = "unreferenced-libraries"

    totals = {}
    for p, reason in removed.items():
        totals[reason] = totals.get(reason, 0) + sizes.get(p, 0)
    for p in sorted(removed, key=lambda p: len(p.parts), reverse=True):
        if p.is_symlink() or p.is_file():
            p.unlink()
    for directory in sorted((p for p in root.rglob("*") if p.is_dir() and not p.is_symlink()),
                            key=lambda p: len(p.parts), reverse=True):
        if not any(directory.iterdir()):
            directory.rmdir()
    before = sum(sizes.values())
    return {"beforeBytes": before, "afterBytes": before - sum(totals.values()),
            "removedBytes": sum(totals.values()), "removedFiles": len(removed),
            "byReason": totals, "removedLibraries": sorted(libraries_removed),
            "languages": list(languages)}
