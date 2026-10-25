#!/usr/bin/env python3
"""Build source archives and native Linux packages from the CMake install tree."""
from __future__ import annotations
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORIES = ("apps", "cmake", "docs", "integrations", "packaging", "scripts", "src", "tests", ".github")
FILES = ("CMakeLists.txt", "CMakePresets.json", "VERSION", "README.md", ".gitignore", ".dockerignore", "LICENSE", "COPYING")
ARCH_DEPENDS = ("qt6-base", "qt6-declarative", "qt6-positioning", "qt6-svg", "qt6-wayland", "breeze-icons", "kirigami", "knotifications", "qqc2-desktop-style")


def run(args, **kwargs):
    print("[release] " + " ".join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def sha256(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def version():
    value = (ROOT / "VERSION").read_text().strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", value):
        raise ValueError("VERSION must contain MAJOR.MINOR.PATCH")
    return value


def epoch():
    if "SOURCE_DATE_EPOCH" in os.environ:
        result = int(os.environ["SOURCE_DATE_EPOCH"])
        if result < 1:
            raise ValueError("SOURCE_DATE_EPOCH must be positive")
        return result
    result = subprocess.run(["git", "-c", "safe.directory="+str(ROOT), "-C", str(ROOT), "log", "-1", "--format=%ct"], capture_output=True, text=True)
    return int(result.stdout.strip()) if result.returncode == 0 else int((ROOT / "VERSION").stat().st_mtime)


def source_files():
    result = [ROOT / name for name in FILES if (ROOT / name).is_file()]
    for directory in DIRECTORIES:
        if (ROOT / directory).exists():
            result += [p for p in (ROOT / directory).rglob("*") if p.is_file() and "__pycache__" not in p.parts and p.suffix != ".pyc"]
    for path in result:
        if path.is_symlink():
            raise ValueError(f"Source symlink is not supported: {path}")
    return sorted(result)


def add_tar(archive, path, name, timestamp):
    info = archive.gettarinfo(str(path), arcname=name)
    info.uid = info.gid = 0
    info.uname = info.gname = "root"
    info.mtime = timestamp
    info.mode = 0o755 if path.is_dir() or os.access(path, os.X_OK) else 0o644
    if info.isfile():
        with path.open("rb") as contents:
            archive.addfile(info, contents)
    else:
        archive.addfile(info)


def source_archives(dist, timestamp):
    name = f"meteo-pl-{version()}"
    tar_path, zip_path = dist / f"{name}.tar.gz", dist / f"{name}-source.zip"
    paths = source_files()
    with tar_path.open("wb") as raw, gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=timestamp) as compressed:
        with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
            for path in paths:
                add_tar(archive, path, name + "/" + path.relative_to(ROOT).as_posix(), timestamp)
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in paths:
            info = zipfile.ZipInfo(name + "/" + path.relative_to(ROOT).as_posix(), time.gmtime(max(timestamp,315532800))[:6])
            info.external_attr = (0o100755 if os.access(path,os.X_OK) else 0o100644) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())
    return [tar_path, zip_path]


def os_release():
    values = {}
    for line in Path("/etc/os-release").read_text().splitlines():
        if "=" in line:
            key, value = line.split("=",1); values[key] = value.strip('"')
    return values


def native_target():
    system = os_release()
    names = [system.get("ID", ""), *system.get("ID_LIKE", "").split()]
    if "debian" in names or "ubuntu" in names: return "deb"
    if "arch" in names: return "arch"
    if "fedora" in names: return "rpm"
    return "tar-install"


def arch_package(stage, destination, timestamp):
    paths = sorted(stage.rglob("*"))
    size = sum(p.stat().st_size for p in paths if p.is_file())
    info = ["pkgname = meteo-pl", f"pkgbase = meteo-pl", f"pkgver = {version()}-1",
            "pkgdesc = ICM meteograms: Qt desktop application and Plasma 6 widget",
            f"builddate = {timestamp}", "packager = Kamil Piwowarski <kapiwko@gmail.com>",
            f"size = {size}", f"arch = {platform.machine()}", "license = LicenseRef-Unspecified"]
    info += [f"depend = {name}" for name in ARCH_DEPENDS]
    info += ["optdepend = plasma-desktop: Plasma 6 widget"]
    (stage / ".PKGINFO").write_text("\n".join(info)+"\n")
    # Native pacman integrity metadata, with the same deterministic timestamps as the package.
    for path in [stage, *stage.rglob("*")]:
        os.utime(path,(timestamp,timestamp),follow_symlinks=False)
    with tempfile.TemporaryDirectory(prefix="meteo-arch-") as temporary:
        mtree = Path(temporary) / "mtree.gz"
        run(["bsdtar", "-czf", mtree, "--format=mtree", "--options=!all,type,uid,gid,mode,time,size,sha256,link", "--uid=0", "--gid=0", "-C", stage, "."])
        shutil.copyfile(mtree,stage / ".MTREE")
        archive_path = Path(temporary) / "package.tar"
        with tarfile.open(archive_path,"w",format=tarfile.PAX_FORMAT) as archive:
            for path in sorted(stage.rglob("*")):
                add_tar(archive,path,path.relative_to(stage).as_posix(),timestamp)
        run(["zstd", "--quiet", "-T1", "-19", "--force", archive_path, "-o", destination])


def build_package(target, build, dist, timestamp, jobs, tests):
    if target != "tar-install" and target != native_target():
        raise ValueError(f"Build {target} in its distribution; use scripts/build-package.py --distro …")
    environment = os.environ.copy(); environment["SOURCE_DATE_EPOCH"] = str(timestamp)
    command = ["cmake", "-S", ROOT, "-B", build, "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=/usr",
               f"-DBUILD_TESTING={'ON' if tests else 'OFF'}", "-DMETEO_BUILD_APP=ON", "-DMETEO_BUILD_PLASMOID=ON"]
    if not (build / "CMakeCache.txt").exists(): command += ["-G","Ninja"]
    if shutil.which("ccache"): command += ["-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"]
    run(command,env=environment)
    run(["cmake","--build",build,"--parallel",jobs],env=environment)
    if tests:
        run(["cmake","--build",build,"--target","meteo_ui_qmllint","meteo-pl_qmllint","--parallel",1],env=environment)
        test_command = ["ctest","--test-dir",build,"--output-on-failure","--output-junit",build/"test-results.xml"]
        if shutil.which("dbus-run-session"): test_command = ["dbus-run-session","--",*test_command]
        if shutil.which("xvfb-run"): test_command = ["xvfb-run","--auto-servernum",*test_command]
        environment["LIBGL_ALWAYS_SOFTWARE"] = "1"
        environment["METEO_RENDER_ARTIFACTS"] = str(build/"renderer-diffs")
        run(test_command,env=environment)
    with tempfile.TemporaryDirectory(prefix="meteo-package-") as temporary:
        temporary = Path(temporary)
        if target == "arch":
            stage = temporary / "stage"; stage.mkdir()
            environment["DESTDIR"] = str(stage)
            run(["cmake","--install",build],env=environment)
            destination = dist / f"meteo-pl-{version()}-1-{platform.machine()}.pkg.tar.zst"
            arch_package(stage,destination,timestamp)
        else:
            generator, suffix = {"deb":("DEB",".deb"),"rpm":("RPM",".rpm"),"tar-install":("TGZ",".tar.gz")}[target]
            run(["cpack","--config",build/"CPackConfig.cmake","-G",generator,"-B",temporary],env=environment)
            candidates=list(temporary.glob("*"+suffix))
            if len(candidates)!=1: raise RuntimeError(f"Expected one {target} package, found {candidates}")
            destination=dist/candidates[0].name
            shutil.copyfile(candidates[0],destination)
    return destination


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("native","source","arch","deb","rpm","tar-install"),default="native")
    parser.add_argument("--build-dir",type=Path,default=ROOT/"build/release")
    parser.add_argument("--dist-dir",type=Path,default=ROOT/"dist")
    parser.add_argument("--jobs",type=int,default=int(os.environ.get("BUILD_JOBS","2")))
    parser.add_argument("--test",action="store_true",help="Run QML lint, the test suite, and CPU/GPU image comparisons")
    args=parser.parse_args()
    if args.jobs<1: parser.error("--jobs must be positive")
    target=native_target() if args.target=="native" else args.target
    dist=args.dist_dir.resolve();dist.mkdir(parents=True,exist_ok=True)
    timestamp=epoch()
    artifacts=source_archives(dist,timestamp)
    if target!="source": artifacts.append(build_package(target,args.build_dir.resolve(),dist,timestamp,args.jobs,args.test))
    report=dist/"BUILD-REPORT.json"
    report.write_text(json.dumps({"package":"meteo-pl","version":version(),"target":target,"system":os_release(),
        "architecture":platform.machine(),"source_date_epoch":timestamp,"tests":"passed" if args.test and target!="source" else "not-run",
        "artifacts":[{"file":p.name,"sha256":sha256(p),"bytes":p.stat().st_size} for p in artifacts]},indent=2)+"\n")
    artifacts.append(report)
    (dist/"SHA256SUMS").write_text("".join(f"{sha256(p)}  {p.name}\n" for p in artifacts))
    for path in artifacts: print(path)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
