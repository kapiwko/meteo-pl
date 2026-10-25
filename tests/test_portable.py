#!/usr/bin/env python3
"""Check portable packaging configuration without downloading an SDK."""
import configparser
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile
from unittest.mock import patch
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location("portable", Path(__file__).resolve().parents[1]/"scripts/build-portable.py")
portable = importlib.util.module_from_spec(spec)
spec.loader.exec_module(portable)


class PortablePackagingTest(unittest.TestCase):
    def test_windows_smoke_uses_only_the_explicit_current_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            old = directory / "meteo-0.1.zip"
            current = directory / "meteo-0.2.zip"
            for path, contents in ((old, "old"), (current, "current")):
                with zipfile.ZipFile(path, "w") as archive:
                    archive.writestr("bin/meteo-pl.exe", contents)
            verified = []
            def run(command, **kwargs):
                executable = Path(command[0])
                verified.append(executable.read_text())
                self.assertEqual(kwargs["env"]["QT_FORCE_STDERR_LOGGING"], "1")
                return SimpleNamespace(stdout="meteo-pl " + (portable.ROOT/"VERSION").read_text().strip()
                                       if command[-1] == "--version" else "", stderr="")
            with patch.object(portable, "run", side_effect=run):
                portable.check_packaged_desktop("windows", [current], directory/"CraftRoot", {})
            self.assertEqual(verified, ["current", "current"])

    def test_packaging_isolates_outputs_and_publishes_only_verified_artifacts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            dist = root / "dist"; dist.mkdir()
            old = dist / "meteo-0.1.zip"; old.write_bytes(b"old")
            verified = []
            def run(command, **kwargs):
                setting = next(str(arg) for arg in command if str(arg).startswith("[Packager]Destination="))
                directory = Path(setting.split("=", 1)[1])
                self.assertNotEqual(directory, dist)
                (directory/"meteo-0.2.zip").write_bytes(b"current")
            def check(target, packages, craft_root, environment):
                self.assertEqual([p.name for p in packages], ["meteo-0.2.zip"])
                self.assertFalse((dist/"meteo-0.2.zip").exists())
                verified.extend(packages)
            with patch.object(portable, "run", side_effect=run), patch.object(portable, "check_packaged_desktop", side_effect=check):
                artifacts = portable.package_current_build(["craft"], "windows", root, dist, root/"CraftRoot", {})
            self.assertEqual(artifacts, [dist/"meteo-0.2.zip"])
            self.assertEqual(artifacts[0].read_bytes(), b"current")
            self.assertEqual(old.read_bytes(), b"old")
            self.assertFalse(verified[0].exists())

    def test_desktop_build_ignores_preinstalled_android_sdk(self):
        sdk={"ANDROID_SDK_ROOT":"/sdk","ANDROID_NDK":"/ndk","ANDROID_HOME":"/sdk","PATH":"/native/bin"}
        with patch.dict(portable.os.environ,sdk,clear=True):
            for target in ("macos","windows"):
                environment=portable.build_environment(target)
                self.assertFalse(any(key.startswith("ANDROID_") for key in environment))
                self.assertEqual(environment["PATH"],"/native/bin")
            environment=portable.build_environment("android")
            self.assertEqual(environment["ANDROID_NDK"],"/ndk")
            self.assertEqual(portable.os.environ["ANDROID_NDK"],"/ndk")

    def test_craft_keeps_existing_paths_and_adds_local_blueprints_once(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); (root/"etc").mkdir()
            config = root/"etc/CraftSettings.ini"
            config.write_text("[Paths]\nPython = /custom/python\n[Blueprints]\nLocations = /existing\n")
            portable.configure_craft(root,"https://mirror.example/craft/Qt6/")
            portable.configure_craft(root,"https://mirror.example/craft/Qt6/")
            result = configparser.ConfigParser(interpolation=None); result.read(config)
            self.assertEqual(result["Paths"]["Python"],"/custom/python")
            locations = result["Blueprints"]["Locations"].split(";")
            self.assertEqual(locations.count(str(portable.ROOT/"packaging/craft")),1)
            self.assertIn("/existing",locations)
            self.assertEqual(result["Packager"]["RepositoryUrl"],"https://mirror.example/craft/Qt6/")
            self.assertFalse(result["CodeSigning"].getboolean("Enabled"))

    def test_foreign_native_target_is_rejected_before_any_download(self):
        with patch.object(portable.platform,"system",return_value="Linux"), patch.object(portable,"run") as run:
            for target in ("windows","macos"):
                with self.assertRaises(ValueError): portable.build(SimpleNamespace(target=target))
            run.assert_not_called()

    def test_checkout_refuses_to_replace_an_existing_revision(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary); (directory/".git").mkdir()
            with patch.object(portable.subprocess,"check_output",return_value="another-revision\n"), patch.object(portable,"run") as run:
                with self.assertRaises(RuntimeError): portable.checkout("https://example/repo","wanted-revision",directory)
                run.assert_not_called()

    def test_unsigned_apk_is_aligned_then_signed_and_verified(self):
        with tempfile.TemporaryDirectory() as temporary, patch.dict(portable.os.environ,
                {"ANDROID_SDK_ROOT": "/sdk", "ANDROID_BUILD_TOOLS_REVISION":"36.0.0"}), patch.object(portable,"run") as run:
            output=portable.sign_android(Path(temporary)/"meteo.apk",Path(temporary))
            commands=[list(map(str,call.args[0])) for call in run.call_args_list]
            self.assertEqual([Path(c[0]).name for c in commands],["keytool","zipalign","apksigner","apksigner"])
            self.assertEqual(commands[-1][1],"verify")
            self.assertTrue(str(output).endswith("-development.apk"))
            self.assertFalse(Path(commands[0][commands[0].index("-keystore")+1]).exists())


if __name__ == "__main__": unittest.main()
