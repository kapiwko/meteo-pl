#!/usr/bin/env python3
import importlib.util
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import sys
import json
import io

SPEC=importlib.util.spec_from_file_location("release",Path(__file__).resolve().parents[1]/"scripts/release.py")
release=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(release)
RUNTIME_SPEC=importlib.util.spec_from_file_location("runtime_check",release.ROOT/"scripts/check-package-runtime.py")
runtime=importlib.util.module_from_spec(RUNTIME_SPEC)
with patch.object(sys,"path",[str(release.ROOT/"scripts"),*sys.path]): RUNTIME_SPEC.loader.exec_module(runtime)
PUBLISH_SPEC=importlib.util.spec_from_file_location("publish_release",release.ROOT/"scripts/publish-release.py")
publisher=importlib.util.module_from_spec(PUBLISH_SPEC);PUBLISH_SPEC.loader.exec_module(publisher)
VERSION_SPEC=importlib.util.spec_from_file_location("set_version",release.ROOT/"scripts/set-version.py")
versions=importlib.util.module_from_spec(VERSION_SPEC);VERSION_SPEC.loader.exec_module(versions)

class ReleaseTest(unittest.TestCase):
    def test_release_rejects_failed_pr_stale_and_unfinished_ci(self):
        good={"headSha":"abc", "headBranch":"main", "event":"push", "name":"Linux CI",
              "status":"completed", "conclusion":"success"}
        publisher.verify_run(good,"abc","Linux CI")
        for patch_data in ({"headSha":"old"},{"event":"pull_request"},{"status":"in_progress"},
                           {"conclusion":"failure"},{"conclusion":"cancelled"},{"name":"Other workflow"}):
            with self.subTest(patch=patch_data),self.assertRaises(ValueError):
                publisher.verify_run(good|patch_data,"abc","Linux CI")

    def test_release_rejects_development_keys_and_untested_packages(self):
        publisher.validate_report({"version":"1.0.0","target":"android", "releaseSigned":True,
                                   "architecture":"arm64-v8a"},"1.0.0","android")
        for report in ({"version":"1.0.0","target":"android","releaseSigned":False,"architecture":"arm64-v8a"},
                       {"version":"0.1.0","target":"android","releaseSigned":True,"architecture":"arm64-v8a"}):
            with self.assertRaises(ValueError):publisher.validate_report(report,"1.0.0","android")
        good={"version":"1.0.0","tests":"passed","system":{"ID":"debian"},"runtimeInstallationCheck":{"status":"passed"}}
        publisher.validate_report(good,"1.0.0","debian")
        for changes in ({"tests":"not-run"},{"runtimeInstallationCheck":{"status":"failed"}},{"system":{"ID":"ubuntu"}}):
            with self.assertRaises(ValueError):publisher.validate_report(good|changes,"1.0.0","debian")

    def test_release_checks_the_entire_downloaded_artifact_set(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);apk=directory/"release.apk";apk.write_bytes(b"original")
            sums=directory/"SHA256SUMS";line=f"{publisher.digest(apk)}  release.apk\n";sums.write_text(line)
            publisher.verify_files(directory)
            apk.write_bytes(b"modified")
            with self.assertRaisesRegex(ValueError,"checksum mismatch"):publisher.verify_files(directory)
            apk.write_bytes(b"original");sums.write_text(line+line)
            with self.assertRaises(ValueError):publisher.verify_files(directory)
            sums.write_text(line);(directory/"unexpected.exe").write_bytes(b"unreported")
            with self.assertRaisesRegex(ValueError,"Artifact set"):publisher.verify_files(directory)
            (directory/"unexpected.exe").unlink();sums.write_text("0"*64+"  ../outside\n")
            with self.assertRaises(ValueError):publisher.verify_files(directory)

    def test_release_checks_the_apk_certificate_identity(self):
        expected="a"*64
        with patch.object(publisher,"run",return_value=f"Signer #1 certificate SHA-256 digest: {expected}\n"):
            publisher.verify_android(Path("signed.apk"),"apksigner",expected)
            with self.assertRaises(ValueError):publisher.verify_android(Path("signed.apk"),"apksigner","b"*64)
        with patch.object(publisher,"run",return_value=""):
            with self.assertRaises(ValueError):publisher.verify_android(Path("unsigned.apk"),"apksigner",expected)

    def test_release_keeps_debian_and_ubuntu_packages_distinct_and_sources_identical(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);inputs=root/"inputs";inputs.mkdir()
            def source_archive(path, contents, level):
                if path.suffix==".zip":
                    with zipfile.ZipFile(path,"w") as archive:
                        item=zipfile.ZipInfo("meteo-pl-1.0.0/source.txt",(2026,10,28,9,0,0))
                        item.external_attr=0o100644<<16;item.compress_type=zipfile.ZIP_DEFLATED
                        archive.writestr(item,contents,compresslevel=level)
                else:
                    with tarfile.open(path,"w:gz",compresslevel=level) as archive:
                        item=tarfile.TarInfo("meteo-pl-1.0.0/source.txt")
                        item.size=len(contents);item.mode=0o644;item.mtime=1700000000
                        archive.addfile(item,io.BytesIO(contents))
            for target in (*publisher.LINUX,"android"):
                directory=inputs/target;directory.mkdir()
                report={"version":"1.0.0","architecture":"x86_64","tests":"passed",
                        "system":{"ID":target},"runtimeInstallationCheck":{"status":"passed"}}
                suffix={"arch":".pkg.tar.zst","debian":".deb","ubuntu":".deb","fedora":".rpm","android":"-signed.apk"}[target]
                (directory/("meteo"+suffix)).write_bytes(target.encode())
                if target=="android":report.update(target="android",releaseSigned=True,architecture="arm64-v8a")
                else:
                    for suffix in (".tar.gz","-source.zip"):
                        source_archive(directory/("meteo-pl-1.0.0"+suffix),b"same sources"*3000,1 if target=="fedora" else 9)
                (directory/"BUILD-REPORT.json").write_text(json.dumps(report))
                (directory/"SHA256SUMS").write_text("".join(f"{publisher.digest(p)}  {p.name}\n" for p in sorted(directory.iterdir())))
            output=root/"output";publisher.collect(inputs,output,"1.0.0")
            self.assertEqual((output/"meteo-pl-1.0.0-linux-debian-x86_64.deb").read_bytes(),b"debian")
            self.assertEqual((output/"meteo-pl-1.0.0-linux-ubuntu-x86_64.deb").read_bytes(),b"ubuntu")
            self.assertEqual(len(list(output.glob("*.apk"))),1)
            self.assertEqual(len(list(output.glob("*source.zip"))),1)
            self.assertNotEqual(publisher.digest(inputs/"arch/meteo-pl-1.0.0.tar.gz"),
                                publisher.digest(inputs/"fedora/meteo-pl-1.0.0.tar.gz"))
            source=inputs/"ubuntu/meteo-pl-1.0.0.tar.gz";old=publisher.digest(source)
            source_archive(source,b"different source",9)
            sums=inputs/"ubuntu/SHA256SUMS";sums.write_text(sums.read_text().replace(
                f"{old}  {source.name}",f"{publisher.digest(source)}  {source.name}"))
            with self.assertRaisesRegex(ValueError,"different source snapshots"):
                publisher.collect(inputs,root/"invalid-output","1.0.0")

    def test_version_updates_plasma_and_installation_examples_together(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/"VERSION").write_text("0.1.0\n")
            metadata=root/"integrations/plasma/package/metadata.json";metadata.parent.mkdir(parents=True)
            metadata.write_text('{"KPlugin":{"Version":"0.1.0"}}')
            (root/"README.md").write_text("meteo-pl-0.1.0-1-x86_64.pkg.tar.zst\nmeteo-pl_0.1.0-1_amd64.deb\n")
            for invalid in ("v1.0.0","1.0","01.0.0","1.1000.0","1.0.1000","2101.0.0"):
                with self.assertRaises(ValueError):versions.set_version(root,invalid)
                self.assertEqual((root/"VERSION").read_text(),"0.1.0\n")
            versions.set_version(root,"1.0.0")
            self.assertEqual((root/"VERSION").read_text(),"1.0.0\n")
            self.assertEqual(json.loads(metadata.read_text())["KPlugin"]["Version"],"1.0.0")
            self.assertNotIn("0.1.0",(root/"README.md").read_text())
    def test_runtime_check_selects_only_the_reported_package_and_verifies_its_hash(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            (root/"meteo-0.1.deb").write_bytes(b"old")
            current=root/"meteo-0.2.deb";current.write_bytes(b"current")
            report={"version":"0.2", "system":{"ID":"debian"},
                "artifacts":[{"file":current.name,"sha256":release.sha256(current)}]}
            (root/"BUILD-REPORT.json").write_text(json.dumps(report))
            self.assertEqual(runtime.current_package(root,"debian","0.2"),current)
            report_path=root/"BUILD-REPORT.json"
            old_inode=report_path.stat().st_ino
            (root/"SHA256SUMS").write_text("old report checksum\n")
            if os.name == "posix":
                report_path.chmod(0o444)
                (root/"SHA256SUMS").chmod(0o444)
            runtime.record_runtime_result(root,{"status":"passed","image":"sha256:synthetic"})
            if os.name == "posix": self.assertNotEqual(report_path.stat().st_ino,old_inode)
            updated=json.loads((root/"BUILD-REPORT.json").read_text())
            self.assertEqual(updated["runtimeInstallationCheck"]["status"],"passed")
            sums=(root/"SHA256SUMS").read_text()
            self.assertIn(release.sha256(root/"BUILD-REPORT.json")+"  BUILD-REPORT.json",sums)
            self.assertIn(release.sha256(current)+"  "+current.name,sums)
            with self.assertRaises(ValueError): runtime.current_package(root,"debian","0.3")
            with self.assertRaises(ValueError): runtime.current_package(root,"ubuntu","0.2")
            current.write_bytes(b"changed")
            with self.assertRaises(ValueError): runtime.current_package(root,"debian","0.2")
    def test_source_archives_are_reproducible_and_exclude_local_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)/"source";root.mkdir()
            (root/"VERSION").write_text("1.2.3\n")
            (root/"CMakeLists.txt").write_text("# synthetic project\n")
            (root/"src").mkdir();(root/"src/test.cpp").write_text("// synthetic source\n")
            (root/"po/pl").mkdir(parents=True);(root/"po/pl/meteo-pl.po").write_text('msgid "Weather"\nmsgstr "Pogoda"\n')
            (root/"scripts").mkdir();script=root/"scripts/release.py";script.write_text("#!/usr/bin/env python3\n");script.chmod(0o755)
            for name in ("build", ".git", ".idea", "src/__pycache__"):
                path=root/name;path.mkdir(parents=True,exist_ok=True);(path/"secret").write_text("private")
            first=Path(directory)/"first";first.mkdir()
            second=Path(directory)/"second";second.mkdir()
            with patch.object(release,"ROOT",root):
                a=release.source_archives(first,1700000000)
                os.utime(root/"src/test.cpp",(1800000000,1800000000))
                b=release.source_archives(second,1700000000)
                self.assertEqual([release.sha256(p) for p in a],[release.sha256(p) for p in b])
            with tarfile.open(a[0]) as archive:
                names=archive.getnames()
                self.assertIn("meteo-pl-1.2.3/src/test.cpp",names)
                self.assertIn("meteo-pl-1.2.3/po/pl/meteo-pl.po",names)
                self.assertFalse(any("secret" in name for name in names))
                self.assertTrue(all(p.uid==0 and p.gid==0 and p.mtime==1700000000 for p in archive.getmembers()))
                self.assertEqual(archive.getmember("meteo-pl-1.2.3/scripts/release.py").mode,0o755)
            with zipfile.ZipFile(a[1]) as archive:
                self.assertEqual(set(archive.namelist()),set(names))
    def test_rejects_invalid_versions(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/"VERSION").write_text("../bad-version\n")
            with patch.object(release,"ROOT",root),self.assertRaises(ValueError):release.version()
    def test_rejects_building_another_distributions_binary_format(self):
        with patch.object(release,"native_target",return_value="arch"),self.assertRaises(ValueError):
            release.build_package("deb",Path("unused"),Path("unused"),1700000000,2,False)
    def test_rejects_source_symlinks(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/"src").mkdir();(root/"src/file.cpp").symlink_to("/etc/passwd")
            with patch.object(release,"ROOT",root),self.assertRaises(ValueError):release.source_files()

if __name__=="__main__":unittest.main()
