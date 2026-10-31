import os
import importlib.util
import json
from pathlib import Path
import info
from CraftCore import CraftCore
from Package.CMakePackageBase import CMakePackageBase


class subinfo(info.infoclass):
    def setTargets(self):
        source = Path(os.environ["METEO_SOURCE_DIR"])
        version = (source / "VERSION").read_text().strip()
        self.targets[version] = ""  # Sources are provided via meteo-pl.srcDir.
        self.defaultTarget = version
        self.displayName = "Meteo"
        self.description = "ICM UW meteograms"

    def setDependencies(self):
        self.runtimeDependencies["virtual/base"] = None
        self.buildDependencies["kde/frameworks/extra-cmake-modules"] = None
        if not CraftCore.compiler.platform.isAndroid:
            self.runtimeDependencies["kde/frameworks/tier2/kcolorscheme"] = None
            self.buildDependencies["libs/gettext"] = None
        self.runtimeDependencies["kde/frameworks/tier1/ki18n"] = None
        self.runtimeDependencies["libs/qt6/qtbase"] = None
        self.runtimeDependencies["libs/qt6/qtdeclarative"] = None
        self.runtimeDependencies["libs/qt6/qtpositioning"] = None
        self.runtimeDependencies["libs/qt6/qtsvg"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kirigami"] = None
        self.runtimeDependencies["kde/frameworks/tier1/breeze-icons"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kiconthemes"] = None
        self.runtimeDependencies["kde/frameworks/tier3/knotifications"] = None
        if not CraftCore.compiler.platform.isAndroid:
            self.runtimeDependencies["kde/frameworks/tier2/kcolorscheme"] = None
            self.runtimeDependencies["kde/frameworks/tier3/qqc2-desktop-style"] = None
            self.runtimeDependencies["kde/plasma/breeze"] = None
        if CraftCore.compiler.platform.isWindows:
            self.buildDependencies["dev-utils/icoutils"] = None


class Package(CMakePackageBase):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.subinfo.options.configure.args += ["-DMETEO_BUILD_PLASMOID=OFF", "-DBUILD_TESTING=OFF"]

    def preArchive(self):
        source = Path(os.environ["METEO_SOURCE_DIR"])
        spec = importlib.util.spec_from_file_location("meteo_bundle_runtime", source / "scripts/bundle_runtime.py")
        runtime = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(runtime)
        platform = CraftCore.compiler.platform
        target = "android" if platform.isAndroid else "windows" if platform.isWindows else "macos"
        languages = sorted({"en", *(p.parent.name for p in (source / "po").glob("*/meteo-pl.po"))})
        report = runtime.optimize_bundle(self.archiveDir(), target, languages)
        print("[bundle] " + json.dumps(report, sort_keys=True), flush=True)
        if path := os.environ.get("METEO_BUNDLE_REPORT"):
            Path(path).write_text(json.dumps(report, indent=2) + "\n")
        return True

    def createPackage(self):
        self.defines["appname"] = "meteo-pl"
        self.defines["company"] = "Kamil Piwowarski"
        self.defines["license"] = str(Path(os.environ["METEO_SOURCE_DIR"]) / "LICENSE")
        self.defines["executable"] = r"bin\meteo-pl.exe"
        self.defines["shortcuts"] = [{"name": "Meteo", "target": "bin/meteo-pl.exe"}]
        self.addExecutableFilter(r"(bin|libexec)/(?!(meteo-pl|snoretoast|update-mime-database)([.-]|$)).*")
        if CraftCore.compiler.platform.isWindows:
            self.defines["icon"] = str(self.buildDir() / "apps/meteo/meteo-pl.ico")
        return super().createPackage()
