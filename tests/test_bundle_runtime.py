"""Check dependency pruning, dynamic backends, and supported bundle resources."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("bundle_runtime", Path(__file__).resolve().parents[1]/"scripts/bundle_runtime.py")
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class BundleRuntimeTest(unittest.TestCase):
    def test_windows_keeps_transitive_and_dynamic_dependencies_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def binary(name):
                path = root/name; path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(b"MZtest")
                return path
            files = {name:binary(name) for name in (
                "bin/meteo-pl.exe", "bin/Qt6Core.dll", "bin/Qt6Network.dll", "bin/Qt6Multimedia.dll",
                "bin/avcodec.dll", "bin/libssl-3-x64.dll", "bin/libcrypto-3-x64.dll",
                "bin/tls/qopenssl.dll", "bin/multimedia/ffmpeg.dll", "bin/unused.dll",
                "bin/pl/meteo/desktop/plugin.dll", "bin/meteo_ui.dll")}
            imports = {name:set() for name in files}
            imports["bin/meteo-pl.exe"] = {"qt6core.dll", "qt6network.dll"}
            imports["bin/pl/meteo/desktop/plugin.dll"] = {"meteo_ui.dll"}
            imports["bin/meteo_ui.dll"] = {"Qt6Core.dll"}
            imports["bin/multimedia/ffmpeg.dll"] = {"Qt6Multimedia.dll", "avcodec.dll"}
            imports["bin/libssl-3-x64.dll"] = {"libcrypto-3-x64.dll"}
            report = runtime.optimize_bundle(root,"windows",dependencies=lambda p,t:imports[p.relative_to(root).as_posix()])
            self.assertFalse(files["bin/unused.dll"].exists())
            self.assertFalse(files["bin/avcodec.dll"].exists())
            self.assertFalse(files["bin/multimedia/ffmpeg.dll"].exists())
            self.assertTrue(files["bin/meteo_ui.dll"].exists())
            self.assertTrue(files["bin/Qt6Core.dll"].exists())
            self.assertTrue(files["bin/libssl-3-x64.dll"].exists()) # QLibrary, not a PE import.
            self.assertEqual(report["beforeBytes"]-report["afterBytes"],report["removedBytes"])

    def test_reachable_qt_module_retains_its_dynamic_backends(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            for name in ("bin/meteo-pl.exe","bin/Qt6Multimedia.dll","bin/avcodec.dll","bin/multimedia/ffmpeg.dll"):
                path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b"MZtest")
            imports={"meteo-pl.exe":{"qt6multimedia.dll"},"Qt6Multimedia.dll":set(),
                     "ffmpeg.dll":{"avcodec.dll"},"avcodec.dll":set()}
            runtime.optimize_bundle(root,"windows",dependencies=lambda p,t:imports[p.name])
            self.assertTrue((root/"bin/multimedia/ffmpeg.dll").exists())
            self.assertTrue((root/"bin/avcodec.dll").exists())

    def test_macos_resolves_versioned_aliases_and_removes_unused_frameworks(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            files=("bin/meteo-pl.app/Contents/MacOS/meteo-pl","lib/libused.1.2.dylib",
                   "lib/libunused.2.dylib","lib/QtTest.framework/Versions/A/QtTest",
                   "lib/QtTest.framework/Versions/A/Resources/data")
            for name in files:
                path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b"\xcf\xfa\xed\xfetest")
            alias=root/"lib/libused.1.dylib";alias.symlink_to("libused.1.2.dylib")
            imports={"meteo-pl":{"libused.1.dylib"}}
            runtime.optimize_bundle(root,"macos",dependencies=lambda p,t:imports.get(p.name,set()))
            self.assertTrue(alias.exists())
            self.assertTrue((root/"lib/libused.1.2.dylib").exists())
            self.assertFalse((root/"lib/libunused.2.dylib").exists())
            self.assertFalse((root/"lib/QtTest.framework").exists())

    def test_android_preserves_icons_licenses_and_supported_locales(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            keep=("share/locale/pl/LC_MESSAGES/ki18n6.mo","share/locale/en_GB/LC_MESSAGES/ki18n6.mo",
                  "share/icons/breeze/index.theme","share/licenses/Qt/LICENSE","lib/arm64-v8a/libQt6Core.so")
            drop=("share/locale/sr/LC_SCRIPTS/ki18n6/script","share/icons/breeze_cursors/cursors/arrow",
                  "share/vpl/examples/content/data","translations/qtbase_de.qm")
            for name in (*keep,*drop):
                path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b"data")
            runtime.optimize_bundle(root,"android")
            self.assertTrue(all((root/name).exists() for name in keep))
            self.assertTrue(all(not (root/name).exists() for name in drop))

    def test_absolute_sdk_alias_keeps_the_staged_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory);root=base/"bundle";sdk=base/"sdk";sdk.mkdir();(root/"lib").mkdir(parents=True)
            (root/"bin").mkdir();(root/"bin/meteo-pl").write_bytes(b"\xcf\xfa\xed\xfetest")
            library=sdk/"libused.1.2.dylib";library.write_bytes(b"\xcf\xfa\xed\xfetest")
            staged=root/"lib/libused.1.2.dylib";staged.write_bytes(library.read_bytes())
            (root/"lib/libused.1.dylib").symlink_to(library)
            runtime.optimize_bundle(root,"macos",dependencies=lambda p,t:{"libused.1.dylib"} if p.name=="meteo-pl" else set())
            self.assertTrue(staged.exists())
            self.assertTrue(library.exists())

    def test_inspection_failure_leaves_the_entire_bundle_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/"bin").mkdir();(root/"bin/meteo-pl.exe").write_bytes(b"MZtest")
            path=root/"share/locale/de/messages";path.parent.mkdir(parents=True);path.write_bytes(b"data")
            def fail(*args): raise ValueError("Invalid executable")
            with self.assertRaises(ValueError):runtime.optimize_bundle(root,"windows",dependencies=fail)
            self.assertTrue(path.exists())


if __name__ == "__main__":unittest.main()
