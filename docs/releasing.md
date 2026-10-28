# Signing and publishing a release

Release artifacts currently target Linux (Arch, Debian, Ubuntu and Fedora) and
Android arm64. Windows and macOS remain CI builds, outside this release set.
The scripts require Python 3.11+, Git, GitHub CLI, GnuPG and Android `apksigner`.

## Version and verification

```sh
python3 scripts/set-version.py 0.9.0
```

This updates `VERSION`, the source Plasma metadata and installation examples.
CMake reads `VERSION` for the app, About page, installed widget and packages.
Android uses `major * 1000000 + minor * 1000 + patch` as its version code;
the version script rejects values that would collide or exceed Android's limit.
Write release notes, review and commit these changes, then push `main`.

Wait for both **Build, test and package** and **Windows macOS and Android
packages** to succeed for that exact commit. Linux packages must pass tests and
installation in a clean runtime container. Android must use the persistent
release keystore described in [portable packaging](portable-packages.md).
A development-signed APK cannot become a release artifact.

## Personal signature

Use a personal GPG key for the annotated Git tag and the detached signature on
`SHA256SUMS`. Keep its private part on the maintainer's machine or hardware
token. The publishing script exports only the public key. Back up the Android
keystore and its password separately; Android updates depend on that identity.

Obtain the Android release certificate SHA-256 digest with:

```sh
apksigner verify --verbose --print-certs meteo-pl-arm64-v8a-signed.apk
```

Prepare and inspect a release without publishing:

```sh
python3 scripts/publish-release.py \
  --linux-run LINUX_RUN_ID --portable-run PORTABLE_RUN_ID \
  --signing-key FULL_GPG_FINGERPRINT \
  --android-certificate-sha256 ANDROID_CERTIFICATE_SHA256 \
  --notes-file docs/releases/0.9.0.md \
  --dist-dir dist/signed-0.9.0
```

Use `--apksigner /path/to/Android/Sdk/build-tools/VERSION/apksigner` if needed.
The script downloads artifacts from the two successful `push` runs on `main`,
checks every checksum and report, and compares source archives across Linux
jobs. Debian and Ubuntu binaries retain separate names. It verifies the APK
signature and expected certificate, signs the checksum manifest and creates
the signed local tag. Existing tags must point to the same commit and carry
the same GPG identity.

Add `--publish` and use a fresh output directory to publish. The script checks CI
again, pushes the signed tag, creates a draft release, uploads the verified
files, downloads them again and compares all filenames and bytes. Only then
does it publish the release. An upload or verification failure leaves a draft.
It does not upload private keys or publish Windows/macOS binaries.

## Verify a downloaded release

Check the signing key's full fingerprint against an independently trusted copy
before importing it. A key shipped beside a download alone does not establish
the maintainer's identity.

```sh
gpg --show-keys --fingerprint SIGNING-KEY.asc
gpg --import SIGNING-KEY.asc
gpg --verify SHA256SUMS.asc SHA256SUMS
sha256sum --check SHA256SUMS
```

`BUILD-PROVENANCE.json` identifies the source commit, CI run IDs and Android
certificate. It is covered by the signed checksum manifest, together with
packages, source archives and distribution build reports.
