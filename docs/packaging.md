# Packages and GitHub Actions

The application and Plasma widget are packaged together as `meteo-pl`. The
package contains the executable, shared C++/QML module, widget, icons, desktop
entry and KDE notification integration. It uses the distribution's Qt/KDE
libraries. Forecast data and user settings are stored separately. The widget
requires Plasma 6; the desktop application can also run in other environments.

| Build and target system | Package |
| --- | --- |
| Arch Linux / compatible current distributions | `.pkg.tar.zst` |
| Debian 13 (trixie) | `.deb` |
| Ubuntu 26.04 LTS | `.deb` |
| Fedora 44 | `.rpm` |
| Source | `.tar.gz`, `-source.zip` |

Each binary package is built in its target distribution with that system's Qt,
KDE, glibc and compiler. Debian and Ubuntu packages are separate artifacts.
Linux packages use system libraries. See [portable packages](portable-packages.md)
for Windows, macOS and Android builds. Ubuntu 24.04 and Debian 12 do not provide
the required combination of Qt and KDE Frameworks 6 in their default repositories.

## Build locally

Container builds require Python 3.11 or later and a working Docker installation.
They do not install the application on the host.

```sh
python3 scripts/build-package.py --distro arch --test
python3 scripts/build-package.py --distro debian --test
python3 scripts/build-package.py --distro ubuntu --test
python3 scripts/build-package.py --distro fedora --test
```

Check installation in a separate, clean runtime image:

```sh
python3 scripts/check-package-runtime.py --distro arch
```

The same option accepts `debian`, `ubuntu` and `fedora`. Runtime images contain
Plasma and test tools without the builder's compiler or development libraries.
The script selects the exact package from its report, verifies its checksum and
application version, and checks application and widget startup. It records the
result and image ID in `BUILD-REPORT.json` and updates the report's checksum in
`SHA256SUMS`. CI performs this check for every distribution.

Outputs are in `dist/<system>/`; reusable build directories are in
`build/packages/<system>/`. Builds use two jobs by default; `--jobs 4` changes
that limit. `--test` runs qmllint, CTest and CPU/GPU comparisons through Mesa and
Xvfb. Omitting it skips tests.

On the target system, install the dependencies listed in its Dockerfile and
build without a container:

```sh
python3 scripts/release.py --target native --test
python3 scripts/release.py --target source
python3 scripts/release.py --target tar-install
```

Explicit targets also include `arch`, `deb` and `rpm`. The script rejects native
package formats built on another distribution. `tar-install` archives the
installation tree and requires system dependencies; it is not a portable
AppImage. `--build-dir` and `--dist-dir` select output directories.

Every set contains source archives, `SHA256SUMS` and `BUILD-REPORT.json`. The
report records the version, system, architecture, test results and artifact
checksums. Verify them with:

```sh
cd dist/debian
sha256sum --check SHA256SUMS
```

Source archives have a stable file order and timestamps from `SOURCE_DATE_EPOCH`
(the latest commit time by default). Build directories, `.git`, caches and IDE
files are excluded. `VERSION` controls the application, About page, installed
widget and package versions. Binary packages also depend on the compiler and
libraries in the container image. The source uses `GPL-3.0-or-later`, as stated in
`LICENSE`; package metadata use the same license identifier.

## GitHub

`.github/workflows/tests.yml` runs on pushes, pull requests and manual dispatch.
Four independent jobs build the application, widget and tests, run qmllint and
the test suite, create the package, install it in a runtime container and check
startup without build-directory library paths. Older Plasma 6 versions without
`--smoke-test` are checked by briefly running `plasmawindowed`.

**Actions → selected run → Artifacts** contains packages, SHA-256 checksums,
JUnit results, CTest logs and CPU/GPU comparison images. A test failure prevents
that job from uploading a package. Renderer tests require RHI/OpenGL; falling
back to the CPU renderer fails the test.

Actions are pinned to commits and Dependabot proposes updates. The build
workflow has read-only repository access and does not automatically publish
releases or packages to external repositories.

See [releasing](releasing.md) for version updates, personal GPG signatures and
publication of verified Linux/Android artifacts.
