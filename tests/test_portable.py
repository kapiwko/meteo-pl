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
    def test_android_output_is_set_before_cmake_configures_the_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary); dist=root/"dist"; dist.mkdir()
            configured=[]
            def prepare(command):
                configured.append(Path(next(arg.split("=",1)[1] for arg in command if str(arg).startswith("[Packager]Destination="))))
            def package(command,**kwargs):
                self.assertEqual(len(configured),1)
                destination=Path(next(arg.split("=",1)[1] for arg in command if str(arg).startswith("[Packager]Destination=")))
                self.assertEqual(destination,configured[0])
                (configured[0]/"meteo.apk").write_bytes(b"current")
            def sign(apk,stage):
                result=stage/"meteo-development.apk"; result.write_bytes(apk.read_bytes()+b"signed"); return result
            with patch.object(portable,"run",side_effect=package), patch.object(portable,"sign_android",side_effect=sign), patch.object(portable,"check_android_icons") as check, patch.object(portable,"check_android_style") as style:
                artifacts=portable.package_current_build(["craft"],"android",root,dist,root/"CraftRoot",{},prepare)
                check.assert_called_once_with(configured[0]/"meteo-development.apk")
                style.assert_called_once_with(configured[0]/"meteo-development.apk")
            self.assertEqual([p.name for p in artifacts],["meteo-development.apk"])
            self.assertFalse(configured[0].exists())

    def test_android_cannot_publish_an_apk_without_its_icon_theme(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory)/"meteo.apk"
            with zipfile.ZipFile(apk,"w") as archive:
                archive.writestr("lib/arm64-v8a/libKF6IconThemes.so",b"library without icons")
            with self.assertRaisesRegex(RuntimeError,"missing its icon theme"):
                portable.check_android_icons(apk)

    def test_android_requires_the_material_style_plugin(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory)/"meteo.apk"
            with zipfile.ZipFile(apk,"w") as archive:
                archive.writestr("lib/arm64-v8a/libQt6QuickControls2.so", b"controls")
            with self.assertRaisesRegex(RuntimeError,"missing the Material"):
                portable.check_android_style(apk)
            with zipfile.ZipFile(apk,"a") as archive:
                archive.writestr("lib/arm64-v8a/libqml_QtQuick_Controls_Material_qtquickcontrols2materialstyleplugin_arm64-v8a.so", b"style")
            portable.check_android_style(apk)

    def test_android_requires_dynamically_selected_weather_icons(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory)/"meteo.apk"
            prefix = "assets/qml/org/kde/kirigami/breeze-internal/"
            icons = {"configure", "mark-location", "view-refresh", "office-chart-line", "edit-undo",
                     "chronometer", "view-calendar", "globe", "dialog-messages",
                     "view-list-text", "view-list-details", "window-close", "dialog-close", "go-next", "open-menu-symbolic",
                     "weather-clear", "weather-clear-night", "weather-clouds",
                     "weather-few-clouds", "weather-none-available", "weather-overcast",
                     "weather-snow", "weather-snow-rain", "weather-storm"}
            with zipfile.ZipFile(apk,"w") as archive:
                archive.writestr(prefix+"index.theme", "[Icon Theme]")
                for icon in icons: archive.writestr(prefix+"icons/48/"+icon+".svg", "<svg/>")
            with self.assertRaisesRegex(RuntimeError,"weather-showers"):
                portable.check_android_icons(apk)
            with zipfile.ZipFile(apk,"a") as archive:
                archive.writestr(prefix+"icons/48/weather-showers.svg", "<svg/>")
            self.assertEqual(portable.check_android_icons(apk),len(icons)+1)

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
                (directory/"meteo-0.2.exe").write_bytes(b"installer")
            def check(target, packages, craft_root, environment):
                self.assertEqual([p.name for p in packages], ["meteo-0.2.exe", "meteo-0.2.zip"])
                self.assertFalse((dist/"meteo-0.2.zip").exists())
                verified.extend(packages)
            with patch.object(portable, "run", side_effect=run), patch.object(portable, "check_packaged_desktop", side_effect=check):
                artifacts = portable.package_current_build(["craft"], "windows", root, dist, root/"CraftRoot", {})
            self.assertEqual(artifacts, [dist/"meteo-0.2.exe"])
            self.assertEqual(artifacts[0].read_bytes(), b"installer")
            self.assertEqual(old.read_bytes(), b"old")
            self.assertFalse(verified[0].exists())

    def test_alternative_packages_remain_available_explicitly(self):
        for target in ("windows", "android"):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as temporary:
                root=Path(temporary); dist=root/"dist"; dist.mkdir()
                def package(command,**kwargs):
                    destination=Path(next(arg.split("=",1)[1] for arg in command if str(arg).startswith("[Packager]Destination=")))
                    for name in (("meteo.exe","meteo.zip") if target=="windows" else ("meteo.apk",)):
                        (destination/name).write_bytes(b"package")
                def sign(apk,stage):
                    result=stage/"meteo-development.apk";result.write_bytes(b"signed");return result
                with patch.object(portable,"run",side_effect=package), patch.object(portable,"sign_android",side_effect=sign), patch.object(portable,"check_packaged_desktop"), patch.object(portable,"check_android_icons"), patch.object(portable,"check_android_style"):
                    artifacts=portable.package_current_build(["craft"],target,root,dist,root/"CraftRoot",{},include_alternatives=True)
                self.assertEqual(len(artifacts),2)
                self.assertTrue(all(p.exists() for p in artifacts))

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

    def test_persistent_android_key_is_reused_without_logging_passwords(self):
        with tempfile.TemporaryDirectory() as temporary:
            key = Path(temporary)/"release.keystore"; key.write_bytes(b"private")
            environment = {"ANDROID_SDK_ROOT": "/sdk", "ANDROID_BUILD_TOOLS_REVISION": "36.0.0",
                           "METEO_ANDROID_KEYSTORE": str(key), "METEO_ANDROID_KEY_ALIAS": "meteo",
                           "METEO_ANDROID_STORE_PASSWORD": "secret-store-password",
                           "METEO_ANDROID_KEY_PASSWORD": "secret-key-password"}
            with patch.dict(portable.os.environ, environment, clear=True), patch.object(portable,"run") as run:
                for _ in range(2):
                    output = portable.sign_android(Path(temporary)/"meteo.apk",Path(temporary))
                commands = [list(map(str,call.args[0])) for call in run.call_args_list]
                self.assertFalse(any(c[0] == "keytool" for c in commands))
                signing = [c for c in commands if c[1] == "sign"]
                self.assertEqual(len(signing), 2)
                for command in signing:
                    self.assertEqual(command[command.index("--ks")+1],str(key))
                    self.assertIn("env:METEO_ANDROID_STORE_PASSWORD",command)
                    self.assertIn("env:METEO_ANDROID_KEY_PASSWORD",command)
                    self.assertNotIn("secret-store-password", " ".join(command))
                    self.assertNotIn("secret-key-password", " ".join(command))
                self.assertTrue(str(output).endswith("-signed.apk"))
                self.assertTrue(key.exists())

    def test_partial_signing_configuration_never_falls_back_to_a_temporary_key(self):
        with patch.dict(portable.os.environ, {"METEO_ANDROID_KEY_ALIAS": "meteo"}, clear=True), patch.object(portable,"run") as run:
            with self.assertRaisesRegex(ValueError,"Incomplete Android signing"):
                portable.sign_android(Path("meteo.apk"),Path("dist"))
            run.assert_not_called()
        with patch.dict(portable.os.environ, {}, clear=True):
            self.assertIsNone(portable.android_signing_key())
            with self.assertRaisesRegex(ValueError,"Incomplete Android signing"):
                portable.android_signing_key(required=True)

    def test_signing_key_outside_docker_is_mounted_read_only_and_secrets_are_not_arguments(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary); key=root/"release.keystore"; key.write_bytes(b"private")
            environment={"METEO_ANDROID_KEYSTORE": str(key), "METEO_ANDROID_KEY_ALIAS": "meteo",
                         "METEO_ANDROID_STORE_PASSWORD": "secret-store-password"}
            args=SimpleNamespace(work_dir=root/"work",dist_dir=root/"dist",require_release_signing=True,
                                 cache_url=None,include_alternatives=False)
            with patch.dict(portable.os.environ,environment,clear=True), patch.dict("sys.modules",{"release":SimpleNamespace(source_files=lambda:[])}), patch.object(portable.platform,"system",return_value="Linux"), patch.object(portable,"run") as run:
                portable.android_container(args)
            command=list(map(str,run.call_args_list[-1].args[0]))
            self.assertIn(f"type=bind,source={key},target=/run/meteo-signing.keystore,readonly",command)
            self.assertIn("METEO_ANDROID_STORE_PASSWORD",command)
            self.assertIn("--require-release-signing",command)
            self.assertNotIn("secret-store-password", " ".join(command))
            self.assertEqual(list((root/"dist").iterdir()),[])


if __name__ == "__main__": unittest.main()
