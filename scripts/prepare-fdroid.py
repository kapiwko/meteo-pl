#!/usr/bin/env python3
"""Stage upstream metadata for an F-Droid request for packaging, without submitting it."""
import argparse
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]


def prepare(root, output):
    output = output.resolve()
    if output.exists():
        raise ValueError("Choose a fresh staging directory")
    version = (root / "VERSION").read_text().strip()
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("VERSION must contain MAJOR.MINOR.PATCH without leading zeroes")
    major, minor, patch = map(int, version.split("."))
    code = major * 1000000 + minor * 1000 + patch
    if minor >= 1000 or patch >= 1000 or code > 2100000000:
        raise ValueError("Version cannot be represented by the Android versionCode")
    app_metadata = (root / ".fdroid.yml").read_text()
    for field, value in (("CurrentVersion", version), ("CurrentVersionCode", str(code))):
        if re.findall(rf"^{field}: (.+)$", app_metadata, re.M) != [value]:
            raise ValueError(f"{field} must match VERSION ({value})")

    store = root / "fastlane/metadata/android"
    english = store / "en-US"
    summary = (english / "short_description.txt").read_text().strip()
    if not summary or len(summary) >= 80 or summary.endswith("."):
        raise ValueError("Store summary must be under 80 characters without a trailing dot")
    description = (english / "full_description.txt").read_text().strip()
    if not description:
        raise ValueError("Store description must not be empty")
    changelog = (english / f"changelogs/{code}.txt").read_text().strip()
    if not changelog or len(changelog) > 500:
        raise ValueError("The current release needs a changelog of at most 500 characters")
    for asset in ("images/icon.png", "images/phoneScreenshots/1.png"):
        if not (english / asset).is_file():
            raise ValueError(f"Missing upstream store artwork: {asset}")
    request = (root / "packaging/fdroid/request-for-packaging.md").read_text()
    for name, value in {"VERSION": version, "VERSION_CODE": str(code),
                        "METADATA": app_metadata.rstrip(), "SUMMARY": summary,
                        "DESCRIPTION": description}.items():
        request = request.replace("{{" + name + "}}", value)
    if re.search(r"\{\{[A-Z_]+\}\}", request):
        raise ValueError("The request template contains unresolved placeholders")

    metadata = output / "metadata"
    metadata.mkdir(parents=True)
    (metadata / "pl.meteo.desktop.yml").write_text(app_metadata)
    shutil.copytree(store, metadata / "pl.meteo.desktop")
    (output / "request-for-packaging.md").write_text(request)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="Fresh staging directory")
    args = parser.parse_args()
    try:
        prepare(ROOT, args.output)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    output = args.output.resolve()
    print(f"Prepared {output}: metadata, artwork and request draft (no enabled build or upload)")


if __name__ == "__main__":
    main()
