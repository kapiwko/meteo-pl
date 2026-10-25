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

SPEC=importlib.util.spec_from_file_location("release",Path(__file__).resolve().parents[1]/"scripts/release.py")
release=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(release)
RUNTIME_SPEC=importlib.util.spec_from_file_location("runtime_check",release.ROOT/"scripts/check-package-runtime.py")
runtime=importlib.util.module_from_spec(RUNTIME_SPEC)
with patch.object(sys,"path",[str(release.ROOT/"scripts"),*sys.path]): RUNTIME_SPEC.loader.exec_module(runtime)

class ReleaseTest(unittest.TestCase):
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
