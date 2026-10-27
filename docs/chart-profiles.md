# Chart profiles

Factory profiles live in [`resources/profiles`](../resources/profiles).
`index.json` defines their order. Each profile uses the same
`pl.meteo.chart-profiles` version 1 format as import and export.
Its positive `revision` is the factory content version; increment it whenever
the bundled profile's charts or appearance change. The document's `version`
describes the file format separately.

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
At least one profile remains available. Changes are saved in the shared
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
