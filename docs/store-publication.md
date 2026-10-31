# AUR publication

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

