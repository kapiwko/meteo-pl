* [ ] The app complies with the inclusion criteria: native dependency build provenance and the existing application ID namespace need review; see below.
* [x] The app is not already listed in the repository or issue tracker.
* [x] The app has not already been requested.
* [x] The upstream source repository contains Fastlane descriptions, artwork and release notes.
* [x] The original app author supports inclusion. I am the author.
* [ ] Optionally donated to support maintenance in F-Droid.

### APPLICATION ID: pl.meteo.desktop

```yaml
{{METADATA}}
```

### Summary

{{SUMMARY}}

### Description

{{DESCRIPTION}}

### Release and upstream metadata

* Release: https://github.com/kapiwko/meteo-pl/releases/tag/v{{VERSION}}
* Version: {{VERSION}} / versionCode {{VERSION_CODE}}
* Source: https://github.com/kapiwko/meteo-pl
* Store metadata: https://github.com/kapiwko/meteo-pl/tree/main/fastlane/metadata/android/en-US
* Build documentation: https://github.com/kapiwko/meteo-pl/blob/main/docs/portable-packages.md
* F-Droid preparation: https://github.com/kapiwko/meteo-pl/blob/main/docs/store-publication.md

### Why add this app?

I would like users to install Meteo and receive updates without Google services.
The project is young, but already has Android and Linux users providing feedback
and testing releases. I support inclusion and can maintain the upstream metadata
and application build integration.

### Native build integration requiring review

Meteo is a C++23/Qt 6 Quick/KDE Kirigami application. Its Android runtime uses
Qt Base, Declarative, Positioning and SVG; KDE Frameworks I18n, Kirigami,
IconThemes, Notifications and Breeze icons; and their dependencies. Valijson is
a pinned source dependency with a SHA-256 checksum. The entry points are
`CMakeLists.txt`, `src/CMakeLists.txt`,
`packaging/craft/meteo-pl/meteo-pl.py` and `scripts/build-portable.py`.

The existing GitHub Android workflow uses a pinned KDE Android SDK container and
Craft binary caches. This is **not an enabled F-Droid source-build recipe**.
Qt/KDE runtime dependencies need approved source recipes and an offline build
verified with fdroidserver. I would appreciate guidance on a suitable maintained
Qt 6/KDE dependency recipe. No blanket scanner exceptions are proposed.

The app uses Android's foreground location API. It has no Google Play Services,
Firebase, advertisements, analytics, account requirements or API keys. Location
is optional; manual place selection works without location permission.

### Network services, namespace and signing

Forecasts and articles depend on ICM UW/meteo.pl services, and their content is
subject to the provider's terms. The proposed metadata declare `NonFreeNet`.
Place lookups use meteo.pl-hosted geocoding; map attribution and third-party
content notices are documented in the source repository.

This is an independent application, not an official ICM UW/meteo.pl client.
The existing application ID `pl.meteo.desktop` is also used by the GitHub release
APKs. Please advise whether a namespace change is needed before first inclusion.

GitHub APKs use a persistent upstream signing key. Reproducibility against those
APKs has not been established, so there is no `Binaries` claim in the metadata.
The upstream Android certificate SHA-256 is
`ab1256bc07bf28d768f922b6734798e6c067c735d8e8111fe2f1f9ec91510c9b`.
