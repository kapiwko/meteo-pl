import os
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
        self.buildDependencies["dev-utils/gettext-tools"] = None
        self.runtimeDependencies["kde/frameworks/tier1/ki18n"] = None
        self.runtimeDependencies["libs/qt6/qtbase"] = None
        self.runtimeDependencies["libs/qt6/qtdeclarative"] = None
        self.runtimeDependencies["libs/qt6/qtpositioning"] = None
        self.runtimeDependencies["libs/qt6/qtsvg"] = None
        self.runtimeDependencies["kde/frameworks/tier1/kirigami"] = None
        self.runtimeDependencies["kde/frameworks/tier1/breeze-icons"] = None
        self.runtimeDependencies["kde/frameworks/tier3/kiconthemes"] = None
        self.runtimeDependencies["kde/frameworks/tier3/knotifications"] = None
        if CraftCore.compiler.platform.isAndroid:
            self.runtimeDependencies["kde/plasma/qqc2-breeze-style"] = None
        else:
            self.runtimeDependencies["kde/frameworks/tier3/qqc2-desktop-style"] = None
            self.runtimeDependencies["kde/plasma/breeze"] = None
        if CraftCore.compiler.platform.isWindows:
            self.buildDependencies["dev-utils/icoutils"] = None


class Package(CMakePackageBase):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.subinfo.options.configure.args += ["-DMETEO_BUILD_PLASMOID=OFF", "-DBUILD_TESTING=OFF"]

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
