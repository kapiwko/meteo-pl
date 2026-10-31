#!/usr/bin/env python3
"""Prepare a source-built AUR package from a personally signed GitHub release."""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
SIGNER = "1C0C43EF3001F658C11971D1A4A884E5B7A2A8A1"


def prepare(version: str, release: Path, output: Path) -> None:
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", version):
        raise ValueError("Expected a stable MAJOR.MINOR.PATCH version")
    archive = f"meteo-pl-{version}.tar.gz"
    with tempfile.TemporaryDirectory(prefix="meteo-aur-gpg-") as temporary:
        command = ["gpg", "--homedir", temporary, "--batch"]
        subprocess.run([*command, "--import", release / "SIGNING-KEY.asc"], check=True, capture_output=True)
        verification = subprocess.run([*command, "--status-fd", "1", "--verify", release / "SHA256SUMS.asc", release / "SHA256SUMS"], check=True, capture_output=True, text=True)
        fingerprints = re.findall(r"^\[GNUPG:\] VALIDSIG ([A-F0-9]+) ", verification.stdout, re.M)
        if fingerprints != [SIGNER]:
            raise ValueError("Checksum manifest has an unexpected signer")
    entries = {}
    for line in (release / "SHA256SUMS").read_text().splitlines():
        digest, name = line.split("  ", 1)
        if name in entries or not re.fullmatch(r"[a-f0-9]{64}", digest) or Path(name).name != name:
            raise ValueError("Invalid checksum manifest")
        entries[name] = digest
    digest = lambda name: hashlib.sha256((release / name).read_bytes()).hexdigest()
    if entries.get(archive) != digest(archive):
        raise ValueError("Source archive does not match the signed manifest")
    replacements = {"@VERSION@": version, "@SOURCE_SHA256@": digest(archive),
                    "@MANIFEST_SHA256@": digest("SHA256SUMS"), "@SIGNATURE_SHA256@": digest("SHA256SUMS.asc")}
    template = (ROOT / "packaging/aur/PKGBUILD.in").read_text()
    for key, value in replacements.items():
        template = template.replace(key, value)
    if re.search(r"@[A-Z_]+@", template):
        raise ValueError("Unresolved PKGBUILD template value")
    output.mkdir(parents=True, exist_ok=True)
    if (output / "PKGBUILD").exists() or (output / ".SRCINFO").exists():
        raise ValueError("Choose a fresh output directory")
    (output / "PKGBUILD").write_text(template)
    srcinfo = subprocess.check_output(["makepkg", "--printsrcinfo"], cwd=output, text=True)
    (output / ".SRCINFO").write_text(srcinfo)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default=(ROOT / "VERSION").read_text().strip())
    parser.add_argument("--release-dir", type=Path, help="Use a previously downloaded signed release")
    parser.add_argument("--output", type=Path, required=True, help="Fresh AUR checkout/staging directory")
    args = parser.parse_args()
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", args.version):
        parser.error("Expected a stable MAJOR.MINOR.PATCH version")
    if args.release_dir:
        prepare(args.version, args.release_dir.resolve(), args.output.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="meteo-aur-release-") as temporary:
            release = Path(temporary)
            base = f"https://github.com/kapiwko/meteo-pl/releases/download/v{args.version}"
            for name in (f"meteo-pl-{args.version}.tar.gz", "SHA256SUMS", "SHA256SUMS.asc", "SIGNING-KEY.asc"):
                with urllib.request.urlopen(f"{base}/{name}", timeout=60) as response, (release / name).open("wb") as output:
                    shutil.copyfileobj(response, output)
            prepare(args.version, release, args.output.resolve())
    print(f"Prepared {args.output.resolve()}: PKGBUILD and .SRCINFO (no upload)")


if __name__ == "__main__":
    main()
