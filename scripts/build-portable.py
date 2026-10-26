#!/usr/bin/env python3
"""Bundle Qt/KDE with Meteo for Windows, macOS or Android using KDE Craft."""
from __future__ import annotations
import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import ssl
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
import zipfile

ROOT = Path(__file__).resolve().parents[1]
CRAFT_REVISION = "ce3702673ea4f9dde99f6f3b70b276a3e6a7e396"
BLUEPRINTS_REVISION = "51848c5646799a62016caf9e3f344d9351a7071c"
ANDROID_IMAGE = "invent-registry.kde.org/sysadmin/ci-images/android-qt611@sha256:11759d824824228d952a3dbb5f75aa85cd4237da1f1148b0b0d4330c3484911a"
QML_ERRORS = re.compile(r"TypeError|ReferenceError|Binding loop|failed to load component|is not installed|is not a type|Invalid image provider", re.IGNORECASE)


def run(command, **kwargs):
    print("[portable] " + " ".join(map(str, command)), flush=True)
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def checkout(repository, revision, destination):
    if not (destination / ".git").is_dir():
        destination.mkdir(parents=True, exist_ok=True)
        run(["git", "init", destination])
        run(["git", "-C", destination, "remote", "add", "origin", repository])
        run(["git", "-C", destination, "fetch", "--depth", "1", "origin", revision])
        run(["git", "-C", destination, "checkout", "--detach", "FETCH_HEAD"])
    actual = subprocess.check_output(["git", "-C", str(destination), "rev-parse", "HEAD"], text=True).strip()
    if actual != revision:
        raise RuntimeError(f"Expected {revision} in {destination}; choose a fresh build directory")


def configure_craft(craft_root, cache_url=None):
    path = craft_root / "etc/CraftSettings.ini"
    settings = configparser.ConfigParser(interpolation=None)
    settings.optionxform = str
    settings.read(path)
    for section in ("Blueprints", "BlueprintVersions", "Compile", "Packager", "CodeSigning"):
        if not settings.has_section(section): settings.add_section(section)
    locations = settings["Blueprints"].get("Locations", "").split(";")
    location = str(ROOT / "packaging/craft")
    settings["Blueprints"]["Locations"] = ";".join(dict.fromkeys([p for p in locations if p] + [location]))
    settings["BlueprintVersions"]["EnableDailyUpdates"] = "False"
    settings["Compile"]["Jobs"] = "2"
    settings["Packager"]["7ZipArchiveType"] = "zip"
    if cache_url: settings["Packager"]["RepositoryUrl"] = cache_url
    # No release credentials are required for local/CI builds.
    settings["CodeSigning"]["Enabled"] = "False"
    with path.open("w", encoding="utf-8") as output: settings.write(output)


def build_environment(target):
    environment = os.environ.copy()
    if target != "android":
        # GitHub's desktop runners also ship an Android SDK. Craft treats the
        # presence of its variables as an explicit cross-compilation request.
        for key in list(environment):
            if key.startswith("ANDROID_"): environment.pop(key)
    environment["METEO_SOURCE_DIR"] = str(ROOT)
    return environment


def sign_android(apk, dist):
    sdk = Path(os.environ.get("ANDROID_SDK_ROOT", os.environ.get("ANDROID_HOME", "")))
    version = os.environ.get("ANDROID_BUILD_TOOLS_REVISION")
    tools = sdk / "build-tools" / version if version else sorted((sdk / "build-tools").iterdir())[-1]
    # CI creates an installable development APK, never a release signing key.
    # Store the key outside artifacts; it is deliberately temporary.
    with tempfile.TemporaryDirectory(prefix="meteo-apk-sign-") as temporary:
        key = Path(temporary) / "debug.keystore"
        run(["keytool", "-genkeypair", "-noprompt", "-keystore", key, "-alias", "androiddebugkey",
             "-storepass", "android", "-keypass", "android", "-dname", "CN=Meteo Development",
             "-keyalg", "RSA", "-keysize", "2048", "-validity", "365"])
        aligned = Path(temporary) / "aligned.apk"
        run([tools / "zipalign", "-p", "-f", "4", apk, aligned])
        result = dist / (apk.stem + "-development.apk")
        run([tools / "apksigner", "sign", "--ks", key, "--ks-key-alias", "androiddebugkey",
             "--ks-pass", "pass:android", "--key-pass", "pass:android", "--out", result, aligned])
        run([tools / "apksigner", "verify", "--verbose", result])
        return result


def check_packaged_desktop(target, packages, craft_root, environment):
    clean = environment.copy()
    for key in ("KDEROOT", "QML_IMPORT_PATH", "QML2_IMPORT_PATH", "QT_PLUGIN_PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
        clean.pop(key, None)
    clean["PATH"] = os.pathsep.join(p for p in clean.get("PATH", "").split(os.pathsep)
                                   if not p.lower().startswith(str(craft_root).lower()))
    clean.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software", QT_FORCE_STDERR_LOGGING="1")
    with tempfile.TemporaryDirectory(prefix="meteo-bundle-test-") as temporary:
        directory = Path(temporary)
        clean.update(XDG_CONFIG_HOME=str(directory / "config"), XDG_CACHE_HOME=str(directory / "cache"))
        mounted = False
        try:
            if target == "windows":
                archives = [path for path in packages if path.suffix == ".zip"]
                if len(archives) != 1: raise RuntimeError(f"Expected one current Windows ZIP, found {archives}")
                with zipfile.ZipFile(archives[0]) as archive: archive.extractall(directory / "app")
                executables = list((directory / "app").rglob("meteo-pl.exe"))
            else:
                mount = directory / "dmg"; mount.mkdir()
                images = [path for path in packages if path.suffix == ".dmg"]
                if len(images) != 1: raise RuntimeError(f"Expected one current macOS DMG, found {images}")
                run(["hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", mount, images[0]])
                mounted = True
                executables = list(mount.rglob("meteo-pl.app/Contents/MacOS/meteo-pl"))
            if len(executables) != 1: raise RuntimeError(f"Expected one bundled executable, found {executables}")
            version = run([executables[0], "--version"], env=clean, capture_output=True, text=True, timeout=30)
            expected_version = (ROOT / "VERSION").read_text().strip()
            if version.stdout.strip() != f"meteo-pl {expected_version}":
                raise RuntimeError(f"Unexpected bundled version: {version.stdout!r}; expected {expected_version}")
            result = run([executables[0], "--smoke-test"], env=clean, capture_output=True, text=True, timeout=60)
            print(result.stdout + result.stderr)
            if QML_ERRORS.search(result.stdout + result.stderr): raise RuntimeError("Bundled QML smoke test failed")
        finally:
            if mounted: run(["hdiutil", "detach", mount])


def package_current_build(command, target, work, dist, craft_root, environment, prepare=None):
    # Keep previous releases outside the artifact selection and smoke test.
    # Publish only after the package from this invocation has passed verification.
    with tempfile.TemporaryDirectory(prefix="packages-", dir=work) as temporary:
        staging = Path(temporary)
        staged_command = [*command, "--options", f"[Packager]Destination={staging}"]
        # Android embeds ANDROID_APK_OUTPUT_DIR at CMake configure time.
        if prepare is not None: prepare(staged_command)
        run([*staged_command, "--package", "meteo-pl"], env=environment)
        artifacts = sorted(p for p in staging.iterdir() if p.suffix in (".exe", ".zip", ".dmg", ".apk"))
        suffix = {"windows": ".zip", "macos": ".dmg", "android": ".apk"}[target]
        packages = [p for p in artifacts if p.suffix == suffix]
        if len(packages) != 1:
            raise RuntimeError(f"Expected one current {suffix} package, found {packages}")
        if target == "android":
            artifacts.append(sign_android(packages[0], staging))
        else:
            check_packaged_desktop(target, artifacts, craft_root, environment)
        published = []
        for artifact in artifacts:
            destination = dist / artifact.name
            shutil.copy2(artifact, destination)
            published.append(destination)
        return published


def build(args):
    expected = {"windows": "Windows", "macos": "Darwin"}.get(args.target)
    if expected and platform.system() != expected:
        raise ValueError(f"Build {args.target} on {expected}; use the portable-packages GitHub workflow")
    if args.target == "android" and not os.environ.get("ANDROID_NDK"):
        raise ValueError("Android builds require the KDE Android SDK container")
    work, craft_root, dist = args.work_dir.resolve(), args.craft_root.resolve(), args.dist_dir.resolve()
    work.mkdir(parents=True, exist_ok=True); dist.mkdir(parents=True, exist_ok=True)
    bootstrap = work / "craft-bootstrap"
    checkout("https://github.com/KDE/craft.git", CRAFT_REVISION, bootstrap)
    environment = build_environment(args.target)
    environment["METEO_BUNDLE_REPORT"] = str(work / "bundle-runtime.json")
    # Craft's CA bundle is installed during bootstrap, so the first download
    # must use an existing trust store. Keep certificate verification enabled.
    certificates = ssl.get_default_verify_paths().cafile
    if not certificates or not Path(certificates).is_file():
        import certifi
        certificates = certifi.where()
    environment.setdefault("SSL_CERT_FILE", certificates)
    environment.setdefault("REQUESTS_CA_BUNDLE", certificates)
    craft = craft_root / "craft/bin/craft.py"
    if not craft.is_file():
        bootstrap_script = bootstrap / "setup/CraftBootstrap.py"
        if args.cache_url:
            # The pinned bootstrap has no CLI option for its repository URL.
            # Configure only that literal in a generated copy, retaining its
            # license headers and leaving the checkout untouched.
            source = bootstrap_script.read_text()
            default_url = "https://files.kde.org/craft/Qt6/"
            if source.count(default_url) != 1: raise RuntimeError("Craft bootstrap URL setting changed")
            bootstrap_script = work / "CraftBootstrap-configured.py"
            bootstrap_script.write_text(source.replace(default_url, args.cache_url))
        run([sys.executable, bootstrap_script, "--use-defaults", "--localDev", bootstrap,
             "--prefix", craft_root], env=environment)
    configure_craft(craft_root, args.cache_url)
    blueprints = craft_root / "etc/blueprints/locations/craft-blueprints-kde"
    # Bootstrap initializes this repository itself. Pin it without discarding
    # modifications in a previously used developer environment.
    if blueprints.is_dir():
        dirty = subprocess.check_output(["git", "-C", str(blueprints), "status", "--porcelain"], text=True)
        if dirty.strip(): raise RuntimeError(f"Blueprint checkout has local changes: {blueprints}")
        run(["git", "-C", blueprints, "fetch", "--depth", "1", "origin", BLUEPRINTS_REVISION])
        run(["git", "-C", blueprints, "checkout", "--detach", BLUEPRINTS_REVISION])
    else:
        checkout("https://github.com/KDE/craft-blueprints-kde.git", BLUEPRINTS_REVISION, blueprints)
    command = [sys.executable, craft, "--options", f"meteo-pl.srcDir={ROOT}"]
    smoke = "not-run"
    def prepare(staged_command):
        nonlocal smoke
        run([*staged_command, "--ignoreInstalled", "meteo-pl"], env=environment)
        if args.target != "android":
            executable = craft_root / ("bin/meteo-pl.exe" if args.target == "windows" else "bin/meteo-pl.app/Contents/MacOS/meteo-pl")
            smoke_env = environment | {"QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software", "QT_FORCE_STDERR_LOGGING": "1"}
            with tempfile.TemporaryDirectory(prefix="meteo-smoke-") as temporary:
                smoke_env.update(XDG_CONFIG_HOME=temporary+"/config", XDG_CACHE_HOME=temporary+"/cache")
                result = run([*staged_command, "--run", executable, "--smoke-test"], env=smoke_env, capture_output=True, text=True, timeout=60)
                print(result.stdout + result.stderr)
                if QML_ERRORS.search(result.stdout + result.stderr): raise RuntimeError("QML smoke test failed")
            smoke = "passed"
    artifacts = package_current_build(command, args.target, work, dist, craft_root, environment, prepare)
    report = {"version": (ROOT / "VERSION").read_text().strip(), "target": args.target,
              "architecture": "arm64-v8a" if args.target == "android" else platform.machine(),
              "smokeTest": smoke, "releaseSigned": False, "bootstrapRevision": CRAFT_REVISION,
              "blueprintsRevision": BLUEPRINTS_REVISION, "artifacts": [p.name for p in artifacts]}
    report["cacheUrl"] = args.cache_url or "https://files.kde.org/craft/Qt6/"
    report["bundleSmokeTest"] = "not-run" if args.target == "android" else "passed"
    bundle_report = work / "bundle-runtime.json"
    if bundle_report.exists(): report["bundleContents"] = json.loads(bundle_report.read_text())
    (dist / "BUILD-REPORT.json").write_text(json.dumps(report, indent=2) + "\n")
    artifacts.append(dist / "BUILD-REPORT.json")
    sums = []
    for path in artifacts:
        with path.open("rb") as contents: digest = hashlib.file_digest(contents, "sha256").hexdigest()
        sums.append(f"{digest}  {path.name}\n")
    (dist / "SHA256SUMS").write_text("".join(sums))


def android_container(args):
    if platform.system() != "Linux": raise ValueError("The Android Docker build requires a Linux host")
    work, dist = args.work_dir.resolve(), args.dist_dir.resolve()
    work.mkdir(parents=True, exist_ok=True); dist.mkdir(parents=True, exist_ok=True)
    image = "meteo-pl-build:android"
    run(["docker", "build", "-f", ROOT / "packaging/containers/android.Dockerfile", "--build-arg",
         f"BASE_IMAGE={ANDROID_IMAGE}", "-t", image, ROOT / "packaging/containers"])
    # ECM scans the source tree recursively for Android manifests. Mount a
    # clean snapshot so it cannot pick up templates inside the SDK/build cache.
    from release import source_files
    with tempfile.TemporaryDirectory(prefix="source-", dir=work) as temporary:
        source = Path(temporary)
        for path in source_files():
            destination = source / path.relative_to(ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, destination)
        command = ["docker", "run", "--rm", "--init", "--shm-size=512m", "--user", f"{os.getuid()}:{os.getgid()}",
             "-e", "HOME=/work/home", "-v", f"{source}:/source:ro", "-v", f"{work}:/work", "-v", f"{dist}:/out", "-w", "/source",
             image, "python3", "scripts/build-portable.py", "--target", "android", "--inside-sdk",
             "--work-dir", "/work", "--craft-root", "/work/CraftRoot", "--dist-dir", "/out"]
        if args.cache_url: command += ["--cache-url", args.cache_url]
        run(command)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("windows", "macos", "android"), required=True)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--craft-root", type=Path)
    parser.add_argument("--dist-dir", type=Path)
    parser.add_argument("--cache-url", default=os.environ.get("METEO_CRAFT_CACHE_URL"), help="Alternative HTTPS mirror of the KDE craft/Qt6/ cache")
    parser.add_argument("--inside-sdk", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.cache_url:
        url = urlsplit(args.cache_url)
        if url.scheme != "https" or not url.netloc or url.username or url.password or url.query or url.fragment:
            parser.error("--cache-url must be a public HTTPS directory URL")
        args.cache_url = args.cache_url.rstrip("/") + "/"
    args.work_dir = args.work_dir or ROOT / "build/portable" / args.target
    args.craft_root = args.craft_root or args.work_dir / "CraftRoot"
    args.dist_dir = args.dist_dir or ROOT / "dist" / args.target
    if args.target == "android" and not args.inside_sdk:
        android_container(args)
    else:
        build(args)
    return 0


if __name__ == "__main__": raise SystemExit(main())
