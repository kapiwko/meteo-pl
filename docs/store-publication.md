# AUR and F-Droid publication

## AUR: source package

`scripts/prepare-aur.py` generates a `PKGBUILD` and `.SRCINFO` from a published
release. It verifies the maintainer's GPG signature in a temporary keyring and
checks the source archive against the signed checksum manifest. The package
builds from source; it does not repackage the GitHub Arch binary. Valijson is a
separate, checksummed source input, so CMake does not fetch it during compilation.

On Arch Linux, after publishing a release:

```sh
python3 scripts/prepare-aur.py --version 0.9.0 --output dist/aur/meteo-pl
```

To reuse locally downloaded release assets:

```sh
python3 scripts/prepare-aur.py --version 0.9.0 \
  --release-dir dist/signed-0.9.0 --output dist/aur/meteo-pl
```

Use a fresh output directory. For local `makepkg` verification, import the public
release key and confirm its fingerprint using a separately trusted copy:

```sh
gpg --import dist/signed-0.9.0/SIGNING-KEY.asc
gpg --fingerprint 1C0C43EF3001F658C11971D1A4A884E5B7A2A8A1
cd dist/aur/meteo-pl
makepkg --verifysource
makepkg --syncdeps
namcap PKGBUILD *.pkg.tar.zst
```

Before submission, build in a clean Arch chroot using `devtools`. The `check()`
phase runs the test suite under D-Bus and Xvfb. Review optional dependencies and
package contents as well as the build result.

An AUR account and an SSH public key registered to that account are required.
Check whether the package name already exists and follow the
[AUR submission guidelines](https://wiki.archlinux.org/title/AUR_submission_guidelines).
For a new package, prepare an AUR checkout and review the two generated files:

```sh
git clone ssh://aur@aur.archlinux.org/meteo-pl.git /tmp/meteo-pl-aur
cp dist/aur/meteo-pl/PKGBUILD dist/aur/meteo-pl/.SRCINFO /tmp/meteo-pl-aur/
git -C /tmp/meteo-pl-aur add PKGBUILD .SRCINFO
git -C /tmp/meteo-pl-aur commit -m 'Initial package: Meteo 0.9.0'
git -C /tmp/meteo-pl-aur push
```

The preparation script never pushes to AUR. Do not put SSH private keys or
signing passwords in this repository. Regenerate the files for each release;
the version, source hash and signature hash must change together.

## F-Droid: request for the main repository

The [submission guide](https://f-droid.org/docs/Submitting_to_F-Droid_Quick_Start_Guide/)
describes a source-build review, rather than an APK upload. Upstream descriptions
and release notes are in `fastlane/metadata/android/en-US/`; the application
metadata are in `.fdroid.yml`.

Prepare a reviewable request with the existing real Android screenshot and icon:

```sh
python3 scripts/prepare-fdroid.py --output dist/fdroid-submission
```

This creates `metadata/pl.meteo.desktop.yml`, localized store text, artwork and a
request draft. It does not contact F-Droid or enable an unverified build.
The draft is ready for the [Requests For Packaging](https://gitlab.com/fdroid/rfp)
process. It explicitly requests help coordinating the native Qt/KDE dependency
recipes. Once those recipes are verified, submit the build metadata as a merge
request to [fdroiddata](https://gitlab.com/fdroid/fdroiddata).

### Remaining build integration

The existing GitHub Android workflow uses KDE's SDK container and Craft binary
caches. A passing upstream APK build does not establish compliance with the
main repository's [inclusion policy](https://f-droid.org/en/docs/Inclusion_Policy/).
Qt/KDE runtime libraries must come from approved sources or be built from source.
The dependency entry points are `scripts/build-portable.py`,
`packaging/craft/meteo-pl/meteo-pl.py` and `src/CMakeLists.txt`.

An enabled F-Droid build needs all of the following:

- Pinned source recipes for the Qt/KDE runtime dependencies and host tools, with
  hashes and licenses; no silent fallback to the Craft binary cache.
- An offline native build after the declared inputs have been fetched, using
  the Android SDK/NDK and Gradle inputs accepted by fdroidserver.
- A scanner review and successful `fdroid lint` and `fdroid build --on-server`.
  Do not add blanket scanner exceptions to make prebuilt runtime libraries pass.
- A release tag containing those recipes and store metadata. The templated
  Android manifest currently does not provide an automatic tag-version check,
  so auto-update checks remain disabled in the draft.

The draft declares `NonFreeNet` because forecasts depend on ICM UW/meteo.pl
services. The application's GPL license does not change ownership of forecast
or article content. Optional location access sends coordinates to the selected
weather and geocoding services. There is no tracking SDK or account requirement.

The Android application ID is currently `pl.meteo.desktop`. Discuss namespace
ownership with the F-Droid reviewer before first inclusion; the app is independent
of the meteo.pl service. Changing the ID creates a separate Android application.

By default F-Droid signs its own builds. They cannot update an installation signed
with the GitHub release key. Keeping the upstream signature requires a verified
reproducible-build arrangement; it has not been established here. Do not add a
`Binaries` field claiming upstream reproducibility until that verification passes.
