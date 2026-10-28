#!/usr/bin/env python3
"""Update the release version without committing, tagging or publishing it."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def set_version(root, value):
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", value):
        raise ValueError("Use MAJOR.MINOR.PATCH without leading zeroes")
    major, minor, patch = map(int, value.split("."))
    if minor >= 1000 or patch >= 1000 or major * 1000000 + minor * 1000 + patch > 2100000000:
        raise ValueError("Version cannot be represented by the Android versionCode")
    previous = (root / "VERSION").read_text().strip()
    metadata_path = root / "integrations/plasma/package/metadata.json"
    metadata = json.loads(metadata_path.read_text())
    metadata["KPlugin"]["Version"] = value
    readme = (root / "README.md").read_text().replace(f"meteo-pl-{previous}-", f"meteo-pl-{value}-")
    readme = readme.replace(f"meteo-pl_{previous}-", f"meteo-pl_{value}-")
    (root / "VERSION").write_text(value + "\n")
    metadata_path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + "\n")
    (root / "README.md").write_text(readme)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version")
    args = parser.parse_args()
    try:
        set_version(ROOT, args.version)
    except ValueError as error:
        parser.error(str(error))
