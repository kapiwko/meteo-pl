#!/usr/bin/env python3
import importlib.util
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SPEC=importlib.util.spec_from_file_location("release",Path(__file__).resolve().parents[1]/"scripts/release.py")
release=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(release)

class ReleaseTest(unittest.TestCase):
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
