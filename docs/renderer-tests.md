# CPU and GPU appearance comparisons

`renderer_parity` renders the same meteogram with the CPU Canvas and native GPU
renderer, then compares the entire component: series, fills, layer order, axes,
labels, legend and reading cursor.

It checks all 12 default chart types in three configurations:

- A wide view of the entire forecast, also used for export.
- A narrow 24-hour view with a reading cursor.
- A wide view with the dark palette enabled.

This produces 36 image pairs. Local fixtures include negative temperatures,
min–max ranges, off-scale precipitation, different precipitation types, changing
wind directions, dense cloud-base points, fog, storms and missing samples.
Tests do not download forecasts or change user settings.

## Run

After building the project:

```sh
METEO_RENDER_ARTIFACTS="$PWD/build/renderer-diffs" \
    ctest --test-dir build/dev -R renderer_parity --output-on-failure
```

Headless machines need Mesa and Xvfb:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run --auto-servernum dbus-run-session -- \
    ctest --test-dir build/dev -R renderer_parity --output-on-failure
```

Mesa may execute OpenGL in software, but the test still requires the
application's native RHI/GPU rendering path. A CPU Canvas fallback does not count
as a GPU renderer. CI runs comparisons independently on four distributions and
preserves CPU, GPU and difference images as artifacts.

## Acceptance criteria

Images must have identical dimensions. The comparison allows small differences
in antialiasing between Canvas, drivers and Qt:

- Mean color-channel difference below 1.5% of the full range.
- Fewer than 1.2% unmatched pixels using a one-pixel edge search and a channel
  tolerance of 40/255.
- Fewer than 2.5% unmatched colored pixels, or at most 64 such pixels per image,
  to accommodate small edges on sparse symbols.

For antialiased colored edges, the comparison also checks hue so that coverage
changes alone do not count as different series colors. A control test verifies
that missing geometry and replacing red with blue are rejected. A separate
presentation test requires visible wind arrows. These thresholds do not imply
bit-for-bit image equality.

On failure, images are written to `/tmp/meteo-renderer-diffs` by default.
`METEO_RENDER_ARTIFACTS` selects another directory and also preserves successful
comparisons. Inspect all three images before changing thresholds.
