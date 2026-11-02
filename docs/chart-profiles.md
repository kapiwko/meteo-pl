# Chart profiles

The secondary axis normally shares the primary axis's range and may show
alternate units, such as mm Hg beside hPa. Setting a right-axis limit gives the
secondary axis its own range. Limits use the chart's data units; alternate-unit
labels are converted afterwards. Tick positions and series coordinates always
use the range of their assigned axis.

Factory profiles live in [`resources/profiles`](../resources/profiles).
`index.json` defines their order. Each profile uses the same
`pl.meteo.chart-profiles` version 1 format as import and export.
Its positive `revision` is the factory content version; increment it whenever
the bundled profile's charts or appearance change. The document's `version`
describes the file format separately.

The bundled choices are **Classic ICM**, **Quick forecast** (the Basic profile)
and **ICM — new colors**. Both ICM variants contain the complete seven-chart
forecast. The new variant uses refreshed light and dark colors for air,
feels-like and ground temperatures and cloud tops. Quick forecast keeps three
shorter charts for temperature, precipitation and wind. Colors are stored
directly in each JSON definition; editing one profile leaves the others unchanged.
Other combinations can be created with
**Duplicate profile** and **Add chart**. The former All charts, Aviation and
Outdoors presets have been retired because they duplicated the classic view.
Edited snapshots of retired presets stay available as user profiles, including
in widgets that selected them by ID; untouched retired presets fall back to
Classic ICM.

Every factory chart specifies its series, drawing types, light and dark colors,
opacity, line styles and widths, point sizes, painter layers, axes and plot
appearance. Wind and pressure fills include their color palettes; precipitation
and storm symbols include their categorical colors. These values are read by
both renderers from JSON. There is no second set of preset values in C++ or QML.

[`resources/charts/catalog.json`](../resources/charts/catalog.json) supplies the
templates offered by **Add chart** and defaults for older imported files.
[`resources/charts/fields.json`](../resources/charts/fields.json) defines series
metadata, units, source-value conversions and supported drawing types.
The renderer implements drawing algorithms and the importer checks semantic
compatibility with the selected weather quantity.

The [JSON Schema](../resources/schemas/chart-data.schema.json) uses Draft 7.
The application validates bundled data and imports against this schema using
Valijson's Qt adapter. It uses the bundled schema without fetching references
from imported files. Exported documents include a schema URL for JSON editors.
The same checks run in CTest, including round trips, invalid styles and complete
factory definitions.

In **Settings → Profiles**, all profiles can be edited, renamed or deleted.
**Restore defaults** resets the selected factory profile. **Restore deleted
presets** recovers removed factory profiles without resetting named user copies.
At least one profile remains available. Editing, duplicating or importing a profile
does not switch the forecast; choose **Use for forecast** to apply it. Deleting a
profile, chart or series and restoring defaults offer **Undo**. Undo only restores
the affected profile and refuses to overwrite any subsequent changes to it.

**Export…** lets you select any combination of profiles, including untouched
presets, before opening the platform file picker. Empty axis bounds use automatic
scaling; invalid ranges remain in the editor with an explanation. Decimal input
accepts both a dot and a comma. Height fields show the effective value and whether
it comes from the application, profile or individual chart.

Changes are saved in the shared
`~/.config/meteo-pl/desktop.ini` preference store, preserving atomic updates
between the application and Plasma widgets. Existing custom layouts remain
available.

Linux packages also install the factory files and schema under
`/usr/share/meteo-pl`. Embedded copies provide factory restore on every platform.
Use export, edit the JSON, and import it to create a named profile from a file.
English labels in bundled JSON are extracted into the gettext catalogs by
`scripts/update-translations.py`; user-created names remain unchanged.

CMake downloads Valijson v1.1.3 at a pinned commit and verifies its SHA-256.
Its sources stay in the build directory. For an offline build, reuse the CMake
dependency cache or set `FETCHCONTENT_SOURCE_DIR_METEO_VALIJSON` to an existing
checkout of that revision. The runtime package includes the library's BSD
license.

On an application upgrade, untouched presets immediately use the new bundled
revision. The first edit snapshots the complete preset and its base revision,
so a later release preserves that edited content. A standard inline notice in
the forecast, widget and profile settings offers **Update and keep old copy**.
Updating installs the new factory content and saves the edited snapshot as a
named profile with an `(old)` suffix, translated to `(stary)` in Polish. Name
collisions receive a number. The backup keeps its series, appearance and height.

## Rendering limits and capabilities

Renderer and fill choices are validated together with the quantity. Pressure and
wind palettes require their matching scale and complete palette data; other
series use a solid fill. ARGB colors preserve their alpha channel on import and
export. Grid-dash elements are zero (ignored) or between 0.5 and 100 pixels.
Normalization clamps legacy settings, and the geometry builder also limits work
independently of the importer. Numeric wind direction has a 0–360° axis; angular
wraps break the line rather than drawing through the whole scale. Horizontal
segments retain signed and zero values and use the quantity's native time policy.
