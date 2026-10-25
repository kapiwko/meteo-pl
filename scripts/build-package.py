#!/usr/bin/env python3
"""Build and test a package in its target Linux distribution using Docker."""
import argparse
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
TARGETS={"debian":("debian","debian:trixie-slim"),"ubuntu":("debian","ubuntu:26.04"),"fedora":("fedora","fedora:44"),"arch":("arch",None)}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--distro",choices=TARGETS,required=True)
    parser.add_argument("--jobs",type=int,default=2)
    parser.add_argument("--test",action="store_true")
    parser.add_argument("--dist-dir",type=Path,default=ROOT/"dist")
    args=parser.parse_args()
    if args.jobs<1: parser.error("--jobs must be positive")
    recipe,base=TARGETS[args.distro]
    tag=f"meteo-pl-build:{args.distro}"
    command=["docker","build","-f",str(ROOT/f"packaging/containers/{recipe}.Dockerfile"),"-t",tag]
    if base:command += ["--build-arg",f"BASE_IMAGE={base}"]
    subprocess.run([*command,str(ROOT/"packaging/containers")],check=True)
    build=ROOT/"build/packages"/args.distro;build.mkdir(parents=True,exist_ok=True)
    dist=args.dist_dir.resolve()/args.distro;dist.mkdir(parents=True,exist_ok=True)
    command=["docker","run","--rm","--init","--shm-size=512m","-v",f"{ROOT}:/source:ro","-v",f"{build}:/work","-v",f"{dist}:/out",tag,
        "python3","scripts/release.py","--build-dir","/work/build","--dist-dir","/out","--jobs",str(args.jobs)]
    if args.test:command.append("--test")
    subprocess.run(command,check=True)
    return 0

if __name__=="__main__":raise SystemExit(main())
