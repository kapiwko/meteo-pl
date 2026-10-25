#!/usr/bin/env python3
"""Install the reported native package in an image without build dependencies."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from linux_targets import TARGETS

ROOT = Path(__file__).resolve().parents[1]


def current_package(directory, distro, expected_version):
    report = json.loads((directory / "BUILD-REPORT.json").read_text())
    if report.get("version") != expected_version:
        raise ValueError("The package report belongs to another application version")
    if report.get("system",{}).get("ID") != distro:
        raise ValueError("The package report belongs to another distribution")
    suffix = {"arch": ".pkg.tar.zst", "debian": ".deb", "ubuntu": ".deb", "fedora": ".rpm"}[distro]
    matches = [entry for entry in report["artifacts"] if entry["file"].endswith(suffix)]
    if len(matches) != 1:
        raise ValueError(f"Expected one reported {suffix} artifact")
    entry = matches[0]
    if Path(entry["file"]).name != entry["file"]:
        raise ValueError("Invalid package filename in report")
    package = directory / entry["file"]
    with package.open("rb") as contents:
        actual = hashlib.file_digest(contents,"sha256").hexdigest()
    if actual != entry["sha256"]:
        raise ValueError("The package checksum does not match its build report")
    return package


def record_runtime_result(directory, result):
    path=directory/"BUILD-REPORT.json"
    report=json.loads(path.read_text())
    report["runtimeInstallationCheck"]=result
    path.write_text(json.dumps(report,indent=2)+"\n")
    sums=[]
    for entry in report["artifacts"]:
        name=entry["file"]
        if Path(name).name != name: raise ValueError("Invalid artifact filename in report")
        sums.append(f"{entry['sha256']}  {name}\n")
    with path.open("rb") as contents:
        digest=hashlib.file_digest(contents,"sha256").hexdigest()
    sums.append(f"{digest}  {path.name}\n")
    (directory/"SHA256SUMS").write_text("".join(sums))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--distro",choices=TARGETS,required=True)
    parser.add_argument("--dist-dir",type=Path,default=ROOT/"dist")
    args = parser.parse_args()
    directory = args.dist_dir.resolve()/args.distro
    package = current_package(directory,args.distro,(ROOT/"VERSION").read_text().strip())
    recipe, base = TARGETS[args.distro]
    tag = "meteo-pl-runtime:"+args.distro
    command = ["docker","build","--target","runtime","-f",ROOT/f"packaging/containers/{recipe}.Dockerfile","-t",tag]
    if base: command += ["--build-arg","BASE_IMAGE="+base]
    subprocess.run([*map(str,command),str(ROOT/"packaging/containers")],check=True)
    image=subprocess.check_output(["docker","image","inspect","--format","{{.Id}}",tag],text=True).strip()
    result={"status":"running","environment":"clean-runtime-image","image":image,"package":package.name}
    record_runtime_result(directory,result)
    script = '''
        case "$(. /etc/os-release; echo "$ID")" in
            debian|ubuntu) apt-get update; apt-get install -y "$1" ;;
            fedora) dnf install -y "$1" ;;
            arch) pacman -U --noconfirm "$1" ;;
        esac
        cd /out
        sha256sum --check SHA256SUMS
        xvfb-run --auto-servernum dbus-run-session -- python3 /source/scripts/check-installed.py
    '''
    try:
        subprocess.run(["docker","run","--rm","--init","--shm-size=512m","-v",f"{ROOT}:/source:ro",
            "-v",f"{directory}:/out:ro",image,"bash","-euc",script,"package-check","/out/"+package.name],check=True)
    except Exception:
        record_runtime_result(directory,result|{"status":"failed"})
        raise
    record_runtime_result(directory,result|{"status":"passed","checks":["version","app-startup","plasma-startup","desktop-entry"]})
    return 0


if __name__ == "__main__": raise SystemExit(main())
