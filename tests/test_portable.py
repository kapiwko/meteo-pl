#!/usr/bin/env python3
"""Check portable packaging configuration without downloading an SDK."""
import configparser
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location("portable", Path(__file__).resolve().parents[1]/"scripts/build-portable.py")
portable = importlib.util.module_from_spec(spec)
spec.loader.exec_module(portable)


class PortablePackagingTest(unittest.TestCase):
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
