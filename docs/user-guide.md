# Meteo user guide

Meteo is a Qt 6/Kirigami application and Plasma 6 widget for ICM UW forecasts
from meteo.pl. It includes interactive meteograms, hourly and daily forecasts,
maps, synoptic commentary and articles. Navigation tabs have icons; narrow
layouts retain icons with tooltips.

Application and widget code: © 2026 Kamil Piwowarski,
[GPL-3.0-or-later](../LICENSE). Libraries and weather data retain their own terms.

## Build and start

Requirements: CMake 3.25+, a C++23 compiler, Ninja, Qt 6.5+ (Core, Network, Qml,
Quick, QuickControls2, Positioning and Test), Kirigami 6 and QQC2 Desktop Style
(`org.kde.desktop`). Export notifications use KDE Frameworks 6 Notifications;
About uses standard Kirigami components. The desktop application also requires
Qt Widgets for native KDE dialogs. The widget requires Plasma 6 and its QML
modules.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/apps/meteo/meteo-pl
```

## Places and forecast models

First launch asks you to add a place instead of selecting a default city. Search
by name, choose a map point, use system positioning or expand the coordinate
fields. A name alone is insufficient: a geographic point must be selected.
Coordinates accept a decimal dot or comma.

**My places** opens the saved-place list. **Add location** opens a blank form;
the edit action changes an existing name or position. Selecting a row opens its
forecast. Deletion can be undone. Each application window and widget remembers
its own selection from the shared list. With several saved places, the header
name becomes a selector, including in the compact widget. Renaming or moving a
place preserves its identity. Deleting a selected place switches the view to
another available place; deleting the last leaves an empty-state action.
Older favorites and recent places survive upgrades. Temporary forecasts opened
from KRunner or a widget do not become saved places merely by being previewed.

Saved places appear with labels on the interactive map and the map in the place
form. The selected place is highlighted; clicking another label opens its
forecast. Search covers Poland by default, with optional worldwide search.
System positioning needs a working Qt Positioning provider; search and map
selection remain available without one.

**Use my location** is a one-time lookup: the form opens a map at the detected
point and looks up its town or village name. Review the marker and name before
pressing **Add**.

Choose **Current location** in **My places**, or use the location button beside
the desktop/widget place selector, to keep following the device. Meteo updates
its forecast after a movement of at least one kilometre and pauses location
updates while the application is in the background. GPS points are not added to
saved or recent places. Choosing a saved place switches back to a fixed forecast.
Location access can be denied; the last forecast stays available and the status
explains how to enable access or retry. Models outside UM4 coverage use GFS.

Choose the model separately in **Settings → Forecast**: UM4 60 h, UM4 120 h or
GFS 240 h. UM4 covers 44.5–66.482°N and 8.21–30.176°E; GFS is global.
**Settings → About** shows authorship, data sources and the retrieved forecast.
Expand **Technical details** to find the configuration-file path.

## Forecast header and chart readings

An activity indicator distinguishes locating the device, resolving its place name
and downloading a forecast. Refreshing already cached data also shows progress;
the existing forecast remains visible.

The header groups the place, forecast timestamp, temperature, wind, pressure and
sunrise/sunset icons. Solar times describe the current forecast day or the
labeled historical date. Model/run details are in settings and About. The summary
is a forecast for its labeled time, not a weather-station observation. The time
axis uses the system zone or UTC; the current-time marker is dashed.

Hover readings are off by default. Enable identification with the eyedropper in
the chart status bar. Hovering then shows parameters, clicking pins a reading
and Escape closes it. Turning identification off also closes pinned readings.
Settings select the hovered chart's parameters or all parameters. The latter
have category tabs and use two columns when space permits.

The reading's export action saves the current category, or all parameters, as
PNG or CSV, including rows outside the scroll viewport. CSV contains zoned time,
place, model, parameter keys, numeric values and units. It uses semicolons,
decimal dots and empty cells for missing values.

Charts include temperature ranges and feels-like values, precipitation type and
chance, wind direction and gusts, cloud layers/bases, humidity, fog, visibility
and storm indicators. Missing values remain gaps. Parameter availability depends
on the model.

## Default charts and rendering

**Classic ICM** contains seven charts: temperature; precipitation and humidity;
precipitation chance and storms; pressure; wind speed and direction; cloud
extent and visibility; cloud cover and fog. Air and feels-like temperatures have
translucent min–max ranges. Ground temperature uses vertical min–max stems at
each forecast timestamp. Light-blue shading marks temperatures below 0°C.
Series colors follow the Meteo web client. The grid stays above fills and fog
above clouds. Grid strokes are light; narrow charts omit minutes and reduce
label density to avoid overlapping hours and dates.

The default **GPU** renderer submits geometry directly to Qt Quick RHI with
MSAA. **Settings → Application → Charts** switches immediately to CPU rendering.
Both paths share calculations, colors, layer order and cursor behavior. Qt
Quick's software backend automatically uses the CPU path.

## Profiles and chart appearance

The profile editor creates, renames, duplicates and deletes profiles, adds or
removes charts and changes their names and order. Each chart can combine catalog
series, such as temperature with precipitation on the right axis. Every series
has a legend preview and an appearance editor for renderer, light/dark colors,
line width/style, point size, opacity and axis assignment. Colors can be chosen
from a palette or entered as codes. Appearance and axis limits belong to each
profile.

Factory profiles are JSON files. Each contains its own charts with complete
series styles, palettes, backgrounds and axes. **Add chart** offers a template
or a blank chart. Changes affect the profile being edited. Automatic axes group
compatible units onto two axes; a third incompatible unit produces a notice
asking for explicit assignments or separate charts. Special symbols, such as
lightning or wind direction, are available only for compatible quantities,
including after import.

Editing and importing do not switch the forecast's active profile. Select
**Use for forecast** to apply the edited profile. **Duplicate profile** makes an
independent copy with an editable name. At least one profile must remain.
**Restore defaults** reloads a factory profile from JSON; **Restore deleted
presets** recovers removed presets. User copies remain independent.

Deleting a profile, chart or series and restoring defaults can be undone. If
another window subsequently changed that profile, undo preserves those later
changes instead of overwriting them.

Factory profiles have content revisions. Unmodified profiles automatically use
new revisions after an application upgrade. Modified profiles show an update
notice. **Update and keep old copy** saves the existing layout as a separately
named old copy, then loads the new factory definition.

Chart height follows the first explicit value: chart → profile → global setting.
Height controls show the effective value and its source. Axis bounds accept a
decimal dot or comma; invalid ranges remain in the form with an explanation.

## Import and export profiles

**Import…** and **Export…** transfer JSON profiles including series appearance,
axis limits and profile/chart heights. Before exporting, select profiles from a
list, including untouched presets. Standard platform file dialogs provide KDE
selection or XDG Desktop Portal integration. Native KDE dialogs require the
application's `QApplication` integration.

Import creates new profiles and numbers conflicting names without replacing
existing profiles. It selects an imported profile for editing while keeping the
forecast's current profile. Failed preference writes show an error and preserve
the previous configuration so import can be retried.

Profile files exclude saved places and global settings such as wind units, UTC
and renderer choice. Those preferences are shared by the app and widget.

## Legends, time range and settings

The **ⓘ** action beside a chart title toggles that chart's legend. The global
**Show legends by default** option controls initial visibility; legends start
hidden. The widget has one shared legend action beside its forecast period.
Legend items toggle series without changing axis scales. Right-clicking a chart
opens **Save chart as…** and checked series actions synchronized with its legend.
This also works in the widget and through Menu or Shift+F10. PNG output preserves
the visible series and selected time range.

The status bar shows the visible time interval, movement arrows, zoom slider
and fit-entire-forecast action. Choosing 24/48/72 hours starts at the first
forecast hour. Mobile defaults to the first 72 hours, or the whole forecast when
shorter. A custom interval starts at the chosen date. Zoom stops at 24 hours,
clamped to the whole range of shorter forecasts. Ctrl+mouse wheel also zooms;
a second slider appears when only part of the forecast is visible.

**Settings** opens the shared settings interface:

- **Forecast:** model, chart profile, time interval and model run. A custom
  interval opens bounded start/end calendars and hour controls.
- **Profiles:** chart/series editing and profile import/export.
- **Readings:** identification scope and initial legend visibility.
- **Application:** renderer, units, time zone, global chart height and cache.

Forms use `Kirigami.FormLayout` and standard Qt Quick controls with insets around
tabs, content and footer buttons. Profile actions use `Kirigami.ActionToolBar`,
which provides overflow actions at narrow widths. Range and model run affect the
current view; profile selection is remembered. Available model runs expose the
server's short-term archive.

## Export charts

**Export…** in the status bar selects one, several or all charts for a single
PNG, including charts below the visible window. Export preserves the current
time range, hidden series, legends, colors and heights. The caption identifies
the place, model, dates and data source. Choose the destination in the system
save dialog. A blue Kirigami message provides an action to open the image.
Chart and profile exports also send a KDE notification with file/folder actions,
subject to desktop notification settings. Both CPU and GPU renderers support
export.

The numeric API has no separate convective-precipitation field. Mean and maximum
precipitation, probability and storm indicators retain their documented meanings.
A red top-edge dot marks off-scale maximum precipitation or a gust, not
convection. See [chart parity](chart-parity.md).

## Maps and editorial content

**Maps** includes interactive UM4/GFS layers and 19 UM/WAM thematic maps. Change
layer, model run and forecast step, animate the forecast or open a meteogram at a
map point. UM4 maps use their own published map runs independently of meteogram
run catalogs. **Legend** shows the selected layer's colors, thresholds and units
on the map; mobile starts with it collapsed.

The previous complete frame remains visible while loading and on network errors.
Interactive animation starts once nearby frames are ready while later frames
load in the background; thematic animation prepares its full loop. Progress and
cancellation stay on the map. At most four tasks run concurrently within cache
limits. Changing the area, layer or run invalidates preparation. Playback also
prepares the next frame in RAM. Tiles retain their geographic coordinates while
panning/zooming; a replacement appears only when all its tiles are ready.
Mouse wheel and touchpad zoom around the cursor. Thematic maps fit both viewport
dimensions by default; **Fit** restores that view after manual zooming.

Choose synoptic-commentary dates in the calendar; future dates are disabled.
Android also supports left/right swipes to change the day.

## Refresh and caches

Forecasts refresh hourly. The app and widget share a forecast cache in
`$XDG_CACHE_HOME/meteo-pl` (normally `~/.cache/meteo-pl`). Recent forecasts reduce
repeat downloads when opening another view. Manual **Refresh** always contacts
the server. Failed retrieval retains cached data with a retrieval timestamp and
notice.

Articles and synoptic commentary are cached separately per page/date for
30 minutes. Manual refresh checks the server and preserves cached content on
failure. Illustrations and map images use the persistent image cache. Place
queries are cached for 24 hours and available-run catalogs for five minutes,
with a 32 MiB/512-entry metadata limit. A new search still needs internet access.

Map images have up to 96 MiB decoded RAM cache and 256 MiB HTTP disk cache.
Identical in-process image requests are shared. Keys include the full URL with
layer, run, step and tile coordinates. Disk freshness follows server headers;
images without explicit freshness receive 30 minutes on disk. Restarting or
resuming animation reuses prepared frames even after eviction from RAM.
Map HTTP caches are under `$XDG_CACHE_HOME/meteo-pl/maps/<process-name>`.

**Settings → Application → Cache** shows disk use by category. Clearing removes
disk caches and decoded image RAM while preserving saved places, profiles and
currently displayed data. The app and Plasma use separate HTTP directories to
avoid concurrent `QNetworkDiskCache` ownership. Shared preferences normally live
in `~/.config/meteo-pl/desktop.ini`, respecting `XDG_CONFIG_HOME`; find the exact
path in **About → Technical details**.

## Install the application and widget

For development builds, `python3 scripts/register-dev-desktop.py` registers a
user desktop entry and icon pointing to the built executable, allowing KDE
portals to identify it. The script does not replace an existing installed entry.
The `dev` preset installs into `build/stage` for inspection:

```sh
cmake --install build/dev
QML_IMPORT_PATH="$PWD/build/stage/lib/qt6/qml" ./build/stage/bin/meteo-pl
```

For a system installation, use a separate build directory:

```sh
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF
cmake --build build/release
sudo cmake --install build/release
```

Add **Meteo · ICM meteogram** through Plasma's **Add Widgets**. A new Plasma
session ensures discovery of the new QML module. Each widget has independent
settings. The header pin action keeps the popup open after another window gains
focus; click again to restore automatic closing. The widget menu offers the
same action.

The expanded widget defaults to all Classic ICM charts over the next 24 hours.
**Widget view** selects a profile and 12/24/48/72 hours or the entire forecast.
Every enabled chart appears in an independently collapsible section. A compact
daily view is also available. Wind, pressure and solar times appear directly in
the header. The popup uses the standard Plasma container; **Show meteogram**
opens a separate full window for the selected place. The panel icon follows
forecast conditions and falls back to the Meteo icon when data are unavailable.

`kpackagetool6 -i integrations/plasma/package` alone does not install the C++
library: install the `pl.meteo.desktop` module with CMake too. Override
`METEO_QML_INSTALL_DIR` for distributions using another path, such as `lib64`.
`METEO_BUILD_APP` and `METEO_BUILD_PLASMOID` disable individual frontends;
`BUILD_TESTING=OFF` removes the Qt Test dependency.

## Packages, CI and data sources

Build distribution packages in containers, for example
`python3 scripts/build-package.py --distro debian --test`. Outputs include source
archives, SHA-256 checksums and a build report. GitHub Actions runs tests, CPU/GPU
comparisons and clean package installations. See [Linux packaging](packaging.md),
[portable builds](portable-packages.md) and [renderer tests](renderer-tests.md).
The shared-backend/KDE-integration split follows `btrfs-backup`; see
[architecture](architecture.md).

References:

- [meteo.pl web client](https://app.meteo.pl/meteogram): data format and models.
- [Meteo ICM on Google Play](https://play.google.com/store/apps/details?id=pl.janpogocki.icm.pogoda): features and meteogram presentation.
- [Meteo API](https://api.meteo.pl/): a separate data-access service.

The adapter uses the web client's undocumented
`https://devmgramapi.meteo.pl/meteorograms/…` interface, checked on 2026-10-25.
It can change and does not provide account or paid-API integration. Data use is
subject to the [meteo.pl terms](https://www.meteo.pl/regulamin-aplikacji-meteo-icm).
Test forecasts are synthetic; the UI identifies the forecast source.

Parity with the reference Android app is incomplete: observational radar,
FLOPRES, weather alerts, automatic startup positioning, accounts and subscriptions
are not implemented. Precipitation maps here show model forecasts.
