# Request for packaging: Meteo

- Application ID: `pl.meteo.desktop`
- Author: Kamil Piwowarski <kapiwko@gmail.com>
- Source: https://github.com/kapiwko/meteo-pl
- License: GPL-3.0-or-later
- Release: https://github.com/kapiwko/meteo-pl/releases/tag/v0.9.0
- Version: 0.9.0 / versionCode 9000
- Upstream metadata: `.fdroid.yml` and `fastlane/metadata/android/en-US/`

I am the author and support inclusion in F-Droid. Meteo provides native Qt
Quick/Kirigami meteograms, hourly/daily forecasts and animated weather maps using
ICM UW/meteo.pl data. It has users on Android and Linux. It has no advertisements,
analytics, login, API keys, Google Play Services or Firebase. System location is
optional and uses Android's location API.

The app depends on ICM UW/meteo.pl network services; the forecast data have their
own terms. The proposed metadata declare `NonFreeNet` for that dependency.

The existing Android build is documented in `docs/portable-packages.md`. It uses
a pinned KDE Android SDK container and Craft binary caches, so I am **not**
claiming that it already satisfies the main repository's source-build process.
Qt/KDE runtime dependencies still need approved source recipes and an offline
build in fdroidserver. There are no broad scanner exceptions in this proposal.

I would like to coordinate the Qt/KDE native dependency recipes with F-Droid's
maintainers before enabling a build. Upstream can maintain the application
metadata and the required build integration. The GitHub APK uses my persistent
signing key; reproducibility against that APK has not been established.
