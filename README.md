<p align="center">
  <img src="integrations/plasma/package/contents/images/meteo.svg" width="88" alt="Meteo icon">
</p>

# Meteo

[![Build, test and package](https://github.com/kapiwko/meteo-pl/actions/workflows/tests.yml/badge.svg)](https://github.com/kapiwko/meteo-pl/actions/workflows/tests.yml)
[![Windows, macOS and Android](https://github.com/kapiwko/meteo-pl/actions/workflows/portable-packages.yml/badge.svg)](https://github.com/kapiwko/meteo-pl/actions/workflows/portable-packages.yml)
[![License: GPL v3 or later](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)

Explore ICM UW weather forecasts from [meteo.pl](https://app.meteo.pl/meteogram)
in a Qt 6 / Kirigami application and a KDE Plasma 6 widget. Read detailed
meteograms, compare hourly and daily forecasts, and animate weather maps
without leaving your desktop.

[![Meteo displaying a forecast with temperature and precipitation charts](docs/images/meteogram.png)](docs/images/meteogram.png)

## What You Get

- Interactive charts for temperature, precipitation, wind, pressure, clouds,
  visibility, fog and storm indicators, with an optional parameter probe.
- UM4 forecasts for Poland and Central Europe, and global GFS forecasts.
- A shared list of saved places: search for a town, choose a point on the map,
  enter coordinates or use the system location service.
- Custom chart profiles with mixed series, axis ranges, colors, drawing styles,
  chart order and individual heights. Import and export profiles as JSON.
- PNG export of selected charts as one image, plus PNG and CSV export of readings.
- Interactive map layers and thematic maps with buffered loading and animation
  preloading, so the previous map stays visible while the next frame loads.
- Synoptic commentary and articles from meteo.pl.
- Persistent caches for forecasts, map tiles, articles, illustrations and place
  searches, with disk usage and clearing controls in Settings.
- GPU rendering by default, an optional CPU renderer, and automated visual
  comparisons between both renderers.

## Install

This project is under active development. GitHub Actions builds downloadable
artifacts; there is no stable release yet. Open a successful run in
[Actions](https://github.com/kapiwko/meteo-pl/actions) and download the artifact
for your system. Each package directory includes SHA-256 checksums and a build
report.

| System | Package | Includes the Plasma widget |
| --- | --- | --- |
| Arch Linux | `.pkg.tar.zst` | Yes |
| Debian 13 / Ubuntu 26.04 | `.deb` | Yes |
| Fedora 44 | `.rpm` | Yes |
| Windows | Craft installer / archive | No |
| macOS | Craft application bundle | No |
| Android arm64 | APK | No |

Windows and macOS bundles include Qt and KDE dependencies. Android builds
include an unsigned release APK and a development APK signed with a temporary
test key. Development APKs built in different runs may require uninstalling the
previous build. The Android package has been built and inspected; device testing
is still needed.

### Linux Packages

```bash
# Arch Linux
sudo pacman -U ./meteo-pl-0.1.0-1-x86_64.pkg.tar.zst

# Debian / Ubuntu
sudo apt install ./meteo-pl_0.1.0-1_amd64.deb

# Fedora
sudo dnf install ./meteo-pl-0.1.0-1.x86_64.rpm
```

Start **Meteo** from your application launcher, or run `meteo-pl`.

### Plasma Widget

After installing a Linux package, open **Add Widgets…**, find **Meteo** and drag
it onto your desktop or panel. Its panel icon follows the forecast weather;
click it for a compact daily forecast, then choose **Show meteogram** for charts.

You can also preview the installed widget in its own window:

```bash
plasmawindowed pl.meteo.desktop
```

The widget uses the shared C++ QML module. Installing the QML package with
`kpackagetool6` alone is insufficient. A new Plasma session may be needed after
the first installation for Plasma to discover the module.

## Quick Start

1. Open **Location** and search for your town or choose a point on the map.
2. Choose **UM4** for Central Europe or **GFS** for worldwide coverage.
3. Switch between **Meteogram**, **Hourly**, **Days**, **Maps**, **Synoptic** and
   **Articles** using the navigation tabs.
4. Open **Settings** to choose a chart profile, forecast run, units or renderer.
5. Enable the pipette in the chart status bar to inspect values. Click a chart
   to pin the reading; use **Export…** to save selected charts together.

Forecasts refresh hourly. **Refresh** requests fresh data immediately. If the
network is unavailable, saved data remains visible with its retrieval time.
Availability of individual parameters depends on the forecast model.

## Build From Source

Requirements: CMake 3.25+, Ninja, a C++23 compiler, Qt 6.5+ and KDE Frameworks 6.
Qt modules: Core, Network, Qml, Quick, QuickControls2, Positioning, Widgets and
Test. KDE dependencies: Kirigami, Notifications and QQC2 Desktop Style. The
widget additionally requires Plasma 6 and its QML modules.

```bash
git clone https://github.com/kapiwko/meteo-pl.git
cd meteo-pl
cmake --preset dev
cmake --build --preset dev --parallel 2
ctest --preset dev
./build/dev/apps/meteo/meteo-pl
```

For a system installation, use a separate release build:

```bash
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF
cmake --build build/release --parallel 2
sudo cmake --install build/release
```

`METEO_BUILD_APP` and `METEO_BUILD_PLASMOID` control the two frontends.
`METEO_QML_INSTALL_DIR` overrides the shared module installation directory.

## Documentation

- [User guide — Polish](docs/user-guide.pl.md): profiles, maps, exports and cache
  behavior.
- [Linux packaging and CI](docs/packaging.md): isolated distribution builds and
  installation checks.
- [Windows, macOS and Android builds](docs/portable-packages.md): Craft bundles,
  SDK setup and package limitations.
- [CPU/GPU visual tests](docs/renderer-tests.md): comparison coverage and how to
  run the tests.
- [Chart parity](docs/chart-parity.md): correspondence with the web client and
  parameter availability.
- [Architecture](docs/architecture.md): shared backend and frontend structure.

Preferences and saved places are stored in
`$XDG_CONFIG_HOME/meteo-pl/desktop.ini` (normally
`~/.config/meteo-pl/desktop.ini`). Cached data lives in
`$XDG_CACHE_HOME/meteo-pl` (normally `~/.cache/meteo-pl`). Each widget keeps its
selected location in the standard Plasma configuration.

## Forecast Sources

Forecasts and map images come from **ICM UW / meteo.pl**; the UM model is
provided by the **Met Office**. This is an independent client using the web
application's data interface, which may change. Weather data and third-party
libraries retain their own terms. See the
[meteo.pl terms](https://www.meteo.pl/regulamin-aplikacji-meteo-icm).

The project does not yet implement every feature of the Android app: observed
radar, FLOPRES, weather alerts and account/subscription integration are not
available. Precipitation maps show model forecasts. The numeric interface does
not expose a separate convective precipitation total.

## License

Copyright © 2026 **Kamil Piwowarski** · [kapiwko@gmail.com](mailto:kapiwko@gmail.com).

Licensed under **[GNU GPL version 3 or later](LICENSE)**
(`GPL-3.0-or-later`).
