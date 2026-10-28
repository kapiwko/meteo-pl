#!/usr/bin/env python3
"""Prepare a personally signed Linux/Android release from successful main CI runs.

Only --publish pushes the signed tag and publishes a GitHub release. GPG signing
stays local; Android must already have its persistent release signature in CI.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
REPO = "kapiwko/meteo-pl"
LINUX = ("arch", "debian", "ubuntu", "fedora")


def run(*command, **kwargs):
    return subprocess.check_output(list(map(str, command)), text=True, **kwargs)


def gh_json(*args):
    return json.loads(run("gh", *args))


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def verify_files(directory):
    entries = {}
    for line in (directory / "SHA256SUMS").read_text().splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  ([^/\\\r\n]+)", line)
        if not match or match[2] in entries or match[2] in (".", "..", "SHA256SUMS"):
            raise ValueError("Invalid or duplicate checksum entry")
        name = match[2]
        path = directory / name
        if path.is_symlink() or not path.is_file() or digest(path) != match[1]:
            raise ValueError(f"Artifact checksum mismatch: {name}")
        entries[name] = match[1]
    actual = {p.name for p in directory.iterdir()}
    if not entries or actual != set(entries) | {"SHA256SUMS"}:
        raise ValueError("Artifact set does not match SHA256SUMS")
    return entries


def verify_run(info, sha, workflow):
    if (info.get("headSha") != sha or info.get("headBranch") != "main"
            or info.get("event") != "push" or info.get("name") != workflow
            or info.get("status") != "completed" or info.get("conclusion") != "success"):
        raise ValueError(f"Require successful {workflow} push CI for {sha}")


def ci_runs(linux_run, portable_run, sha):
    for number, workflow in ((linux_run, "Build, test and package"),
                             (portable_run, "Windows macOS and Android packages")):
        info = gh_json("run", "view", str(number), "--repo", REPO,
                       "--json", "headSha,headBranch,event,name,status,conclusion,url")
        verify_run(info, sha, workflow)


def validate_report(report, version, target):
    if report.get("version") != version:
        raise ValueError("Build report has a different release version")
    if target == "android":
        if (report.get("target") != "android" or report.get("releaseSigned") is not True
                or report.get("architecture") != "arm64-v8a"):
            raise ValueError("Android requires a persistent release signing key and arm64 APK")
    elif (report.get("tests") != "passed"
          or report.get("system", {}).get("ID") != target
          or report.get("runtimeInstallationCheck", {}).get("status") != "passed"):
        raise ValueError(f"{target}: tests and clean installation must both pass")


def source_snapshot(path):
    # Different zlib/Python versions can compress identical trees differently.
    # Compare file contents and normalized metadata, not the compressed stream.
    result = {}
    if path.suffix == ".zip":
        with zipfile.ZipFile(path) as archive:
            for item in archive.infolist():
                if item.is_dir() or item.filename in result:
                    raise ValueError("Unsupported or duplicate source archive entry")
                result[item.filename] = (item.external_attr, item.date_time,
                                         hashlib.sha256(archive.read(item)).hexdigest())
    else:
        with tarfile.open(path) as archive:
            for item in archive:
                if not item.isfile() or item.name in result:
                    raise ValueError("Unsupported or duplicate source archive entry")
                with archive.extractfile(item) as stream:
                    result[item.name] = (item.mode, item.uid, item.gid, item.mtime,
                                         hashlib.file_digest(stream, "sha256").hexdigest())
    if not result:
        raise ValueError("Empty source archive")
    return result


def collect(inputs, destination, version):
    destination.mkdir(parents=True, exist_ok=False)
    for target in (*LINUX, "android"):
        directory = inputs / target
        checksums = verify_files(directory)
        report = json.loads((directory / "BUILD-REPORT.json").read_text())
        validate_report(report, version, target)
        shutil.copyfile(directory / "BUILD-REPORT.json", destination / f"BUILD-REPORT-{target}.json")
        suffix = {"arch": ".pkg.tar.zst", "debian": ".deb", "ubuntu": ".deb",
                  "fedora": ".rpm", "android": ".apk"}[target]
        packages = [name for name in checksums if name.endswith(suffix)]
        if len(packages) != 1 or (target == "android" and not packages[0].endswith("-signed.apk")):
            raise ValueError(f"Expected exactly one release package for {target}")
        arch = "arm64-v8a" if target == "android" else report["architecture"]
        name = f"meteo-pl-{version}-{'android' if target == 'android' else 'linux-' + target}-{arch}{suffix}"
        shutil.copyfile(directory / packages[0], destination / name)
        if target in LINUX:
            for name in (f"meteo-pl-{version}.tar.gz", f"meteo-pl-{version}-source.zip"):
                if name not in checksums:
                    raise ValueError(f"Missing release source archive: {name}")
                existing = destination / name
                if existing.exists():
                    if source_snapshot(existing) != source_snapshot(directory / name):
                        raise ValueError("Linux jobs built different source snapshots")
                else:
                    shutil.copyfile(directory / name, existing)


def verify_android(apk, apksigner, expected):
    output = run(apksigner, "verify", "--verbose", "--print-certs", apk)
    found = re.findall(r"^Signer #\d+ certificate SHA-256 digest: ([0-9a-fA-F]+)$", output, re.M)
    if [value.lower() for value in found] != [expected.lower()]:
        raise ValueError("APK signing certificate does not match the expected release identity")


def prepare(args, sha, version):
    ci_runs(args.linux_run, args.portable_run, sha)
    destination = args.dist_dir.resolve()
    if destination.exists():
        raise ValueError(f"Use a new output directory: {destination}")
    with tempfile.TemporaryDirectory(prefix="meteo-release-") as temporary:
        inputs = Path(temporary)
        for target in (*LINUX, "android"):
            run_id = args.portable_run if target == "android" else args.linux_run
            artifact = f"meteo-pl-{'android-arm64' if target == 'android' else target}-{sha}"
            run("gh", "run", "download", run_id, "--repo", REPO, "--name", artifact,
                "--dir", inputs / target)
        collect(inputs, destination, version)
    apk, = destination.glob("*.apk")
    verify_android(apk, args.apksigner, args.android_certificate_sha256)
    provenance = {"repository": REPO, "commit": sha, "version": version,
                  "linuxRun": args.linux_run, "portableRun": args.portable_run,
                  "androidCertificateSha256": args.android_certificate_sha256.lower(),
                  "gpgSigningKey": args.signing_key.upper()}
    (destination / "BUILD-PROVENANCE.json").write_text(json.dumps(provenance, indent=2) + "\n")
    (destination / "SIGNING-KEY.asc").write_text(run("gpg", "--armor", "--export", args.signing_key))
    artifacts = sorted(destination.iterdir())
    (destination / "SHA256SUMS").write_text("".join(f"{digest(p)}  {p.name}\n" for p in artifacts))
    run("gpg", "--armor", "--detach-sign", "--local-user", args.signing_key,
        "--output", destination / "SHA256SUMS.asc", destination / "SHA256SUMS")
    run("gpg", "--verify", destination / "SHA256SUMS.asc", destination / "SHA256SUMS")
    tag = f"v{version}"
    exists = subprocess.run(["git", "show-ref", "--verify", "--quiet", f"refs/tags/{tag}"], cwd=ROOT).returncode == 0
    if not exists:
        run("git", "tag", "--sign", "--local-user", args.signing_key, "--file", args.notes_file, tag, sha, cwd=ROOT)
    if run("git", "rev-parse", f"{tag}^{{}}", cwd=ROOT).strip() != sha:
        raise ValueError("Existing release tag points at a different commit")
    verification = subprocess.run(["git", "verify-tag", "--raw", tag], cwd=ROOT,
                                  capture_output=True, text=True, check=True)
    valid = [line.split() for line in verification.stderr.splitlines() if line.startswith("[GNUPG:] VALIDSIG ")]
    if not any(args.signing_key.upper() in (line[2].upper(), line[-1].upper()) for line in valid):
        raise ValueError("Release tag was not signed with the expected GPG key")
    return destination, tag


def publish(destination, tag, notes, sha, args):
    ci_runs(args.linux_run, args.portable_run, sha)
    run("git", "push", "origin", f"refs/tags/{tag}", cwd=ROOT)
    run("gh", "release", "create", tag, "--repo", REPO, "--verify-tag", "--draft",
        "--title", f"Meteo {tag[1:]}", "--notes-file", notes)
    files = sorted(destination.iterdir())
    run("gh", "release", "upload", tag, "--repo", REPO, *files)
    with tempfile.TemporaryDirectory(prefix="meteo-published-") as temporary:
        run("gh", "release", "download", tag, "--repo", REPO, "--dir", temporary)
        downloaded = Path(temporary)
        if {p.name for p in downloaded.iterdir()} != {p.name for p in files}:
            raise ValueError("Uploaded release asset set differs from the verified local set")
        if any(digest(downloaded / p.name) != digest(p) for p in files):
            raise ValueError("Uploaded release asset bytes differ; leaving the release as a draft")
    run("gh", "release", "edit", tag, "--repo", REPO, "--draft=false", "--latest")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--linux-run", required=True)
    parser.add_argument("--portable-run", required=True)
    parser.add_argument("--signing-key", required=True, help="Full personal GPG fingerprint")
    parser.add_argument("--android-certificate-sha256", required=True)
    parser.add_argument("--apksigner", default="apksigner")
    parser.add_argument("--notes-file", required=True, type=Path)
    parser.add_argument("--dist-dir", required=True, type=Path)
    parser.add_argument("--publish", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-fA-F0-9]{40}|[a-fA-F0-9]{64}", args.signing_key):
        parser.error("Provide the full GPG fingerprint")
    if not re.fullmatch(r"[a-fA-F0-9]{64}", args.android_certificate_sha256):
        parser.error("Provide the Android release certificate SHA-256 digest without colons")
    if run("git", "status", "--porcelain", cwd=ROOT).strip():
        parser.error("Commit all release changes before preparing artifacts")
    if not args.notes_file.is_file():
        parser.error("Release notes file does not exist")
    version = (ROOT / "VERSION").read_text().strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        parser.error("VERSION must contain MAJOR.MINOR.PATCH")
    sha = run("git", "rev-parse", "HEAD", cwd=ROOT).strip()
    destination, tag = prepare(args, sha, version)
    if args.publish:
        publish(destination, tag, args.notes_file, sha, args)
    print(f"Verified signed release: {destination}")


if __name__ == "__main__":
    main()
