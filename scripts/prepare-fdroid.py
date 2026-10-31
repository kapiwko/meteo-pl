#!/usr/bin/env python3
"""Stage upstream metadata for an F-Droid request for packaging, without submitting it."""
import argparse
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="Fresh staging directory")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        parser.error("Choose a fresh staging directory")
    metadata = output / "metadata"
    metadata.mkdir(parents=True)
    shutil.copy2(ROOT / ".fdroid.yml", metadata / "pl.meteo.desktop.yml")
    description = metadata / "pl.meteo.desktop/en-US"
    shutil.copytree(ROOT / "fastlane/metadata/android/en-US", description)
    images = description / "images"
    images.mkdir()
    shutil.copy2(ROOT / "packaging/icons/512-apps-meteo-pl.png", images / "icon.png")
    phones = images / "phoneScreenshots"
    phones.mkdir()
    shutil.copy2(ROOT / "docs/images/android.png", phones / "1.png")
    shutil.copy2(ROOT / "packaging/fdroid/request-for-packaging.md", output / "request-for-packaging.md")
    print(f"Prepared {output}: metadata, artwork and request draft (no enabled build or upload)")


if __name__ == "__main__":
    main()
