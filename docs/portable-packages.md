# Windows, macOS and Android

`scripts/build-portable.py` bundles the application with Qt, Kirigami, Breeze
icons and KDE dependencies using KDE Craft. The Plasma widget is disabled on
these platforms.

| Platform | Build environment | Output |
| --- | --- | --- |
| Windows x64 | Windows, Visual Studio 2022, Python 3.11+ | NSIS `.exe` installer |
| macOS | macOS, Xcode with macOS SDK 15+, Python 3.11+ | `.dmg` containing an `.app`, using the builder's architecture |
| Android arm64 | Linux with Docker and the official KDE/Qt 6.11 SDK image | Persistent-key `-signed.apk` or test-only `-development.apk` |

## Build

On Windows or macOS, from the project directory:

```sh
python -m pip install certifi
python -m pip install pefile==2024.8.26 macholib==1.16.3
python scripts/build-portable.py --target windows
# On macOS:
python scripts/build-portable.py --target macos
```

On Linux with Docker:

```sh
python3 scripts/build-portable.py --target android
```

Outputs are in `dist/windows`, `dist/macos` or `dist/android`, together with
`SHA256SUMS` and `BUILD-REPORT.json`. The working directory is reusable.
`--include-alternatives` also includes the Windows portable ZIP or unsigned
Android APK. The ZIP is always built and checked before the installer is
published, but is omitted from the default download set to avoid duplication.
If the default mirror is unavailable, select one with
`--cache-url https://ftp.gwdg.de/pub/linux/kde/extrafiles/craft/Qt6/` or
`METEO_CRAFT_CACHE_URL`.

`--work-dir`, `--craft-root` and `--dist-dir` override directories. On Windows,
choose a short path such as `--craft-root C:/MeteoCraft` to avoid compiler path
limits. The first build downloads a substantial Qt/KDE SDK and needs several GB
of disk space and access to the `files.kde.org` cache.

Desktop applications run `--smoke-test` before packaging and again from the
extracted ZIP or mounted DMG without Craft's library search paths. The check
rejects QML loading errors. Android reports that device startup is untested;
a successful APK build alone does not establish phone compatibility.
[CPU/GPU visual comparisons](renderer-tests.md) currently run on Linux rather
than D3D, Metal or phone hardware.

## Package size

Before archiving, `bundle_runtime.py` removes SDK examples, cursor themes,
development plugins and unused QML modules. Windows/macOS trimming reads PE and
Mach-O dependencies and retains their transitive closure, QML/platform plugins
and dynamically loaded TLS libraries. Multimedia, SQL or speech backends remain
if a retained Qt library needs them. Only the staged package tree is trimmed;
the Craft SDK stays intact.

Dependency translations follow the application's languages, currently English
and Polish. Breeze icons, licenses and resources for retained libraries remain
bundled. `BUILD-REPORT.json` records sizes before/after trimming, removed
libraries and the actual size of each published file. Final bundles must still
pass startup checks.

The GitHub Android artifact uses ZIP transport compression without altering APK
bytes or signatures. APK native libraries remain suitable for direct Android
memory mapping, so the downloaded artifact can be smaller than the extracted APK.
See [Android native-library packaging](https://developer.android.com/topic/performance/reduce-apk-size#reduce-native-binaries).

## Signing and distribution

The Windows installer currently has no Authenticode signature. The macOS app is
not notarized and may need local signing and Gatekeeper approval. Producing
these artifacts does not require store accounts or keys.

Android supports a persistent release key so later APKs install as updates.
Without configured signing, each build receives a new temporary development
signature. Moving from a development signature to the persistent key may require
uninstalling the old test app once, which removes its settings. The build script
does not publish to app stores.

### Persistent Android signing

Keep the keystore outside the repository and maintain a secure backup. It cannot
be recovered from an APK. Losing a self-managed APK signing key prevents signing
a compatible update. See [Android app signing](https://developer.android.com/studio/publish/app-signing).
A separate key per application is recommended.

Create a key interactively with the JDK's `keytool`. Enter its password in the
terminal rather than a command-line argument:

```bash
install -d -m 700 "$HOME/.local/share/meteo-signing"
export METEO_ANDROID_KEYSTORE="$HOME/.local/share/meteo-signing/meteo.p12"
export METEO_ANDROID_KEY_ALIAS=meteo
keytool -genkeypair -keystore "$METEO_ANDROID_KEYSTORE" -storetype PKCS12 \
  -alias "$METEO_ANDROID_KEY_ALIAS" -keyalg RSA -keysize 3072 -validity 10000 \
  -dname "CN=Kamil Piwowarski"
chmod 600 "$METEO_ANDROID_KEYSTORE"
```

Set the signing variables before building. For an existing key, use its actual
path and alias. Optional `METEO_ANDROID_KEY_PASSWORD` supplies a different key
password; otherwise the store password is used:

```bash
export METEO_ANDROID_KEYSTORE="$HOME/.local/share/meteo-signing/meteo.p12"
export METEO_ANDROID_KEY_ALIAS=meteo
read -r -s -p 'Keystore password: ' METEO_ANDROID_STORE_PASSWORD
printf '\n'
export METEO_ANDROID_STORE_PASSWORD
python3 scripts/build-portable.py --target android --require-release-signing
unset METEO_ANDROID_STORE_PASSWORD METEO_ANDROID_KEY_PASSWORD
```

The output ends in `-signed.apk`. The keystore is mounted read-only into the
container and excluded from source, build caches and artifacts. Passwords travel
through environment variables without appearing in logged commands. Incomplete
configuration fails rather than silently selecting another key.
`--require-release-signing` also rejects an entirely missing configuration.

`apksigner verify --verbose --print-certs` checks the result and prints the public
certificate fingerprint. `BUILD-REPORT.json` records `releaseSigned: true` for
an APK using the configured key. The certificate is public; the keystore and
passwords must remain private.

### Android signing in GitHub Actions

Under **Secrets and variables → Actions**, configure:

| Secret | Contents |
| --- | --- |
| `ANDROID_KEYSTORE_BASE64` | Base64-encoded keystore |
| `ANDROID_KEY_ALIAS` | Key alias, for example `meteo` |
| `ANDROID_STORE_PASSWORD` | Keystore password |
| `ANDROID_KEY_PASSWORD` | Optional different key password |

Upload the keystore through standard input without printing it:

```bash
base64 -w 0 "$METEO_ANDROID_KEYSTORE" | gh secret set ANDROID_KEYSTORE_BASE64 --repo kapiwko/meteo-pl
```

Enter other values interactively with `gh secret set SECRET_NAME`. Base64 is an
encoding, not encryption; GitHub Actions secrets provide storage protection.
The workflow supplies the key only for trusted-branch builds or manual runs.
All pull requests, including same-repository PRs, use temporary keys. The
keystore is removed after building. Development packages continue to build when
secrets are absent. See [GitHub Actions secrets](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets).

Windows needs its own Authenticode identity. Open-source projects can consider
SignPath Foundation; ordinary distribution needs a trusted certificate or
signing service. A signature does not itself guarantee that SmartScreen warnings
disappear. See [Windows signing options](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options).
Standard macOS distribution outside the App Store uses an Apple Developer ID
certificate and notarization; see [Developer ID](https://developer.apple.com/developer-id/).
Those identities are not configured in this project's workflow.

## Android interface and local testing

The Android port uses Material styling (Material 3 in Qt Quick Controls), with
the system's light or dark theme. Packaging checks for the Material plugin and
bundles Breeze icons. The manifest requests network, location and notification
permissions. Storage Access Framework import/export and runtime permissions
still need device testing; manifest declarations alone are insufficient.
`kirigami_package_breeze_icons()` embeds icons from a pinned Breeze release;
packaging verifies the theme and required icons in the final APK.

Start an existing emulator; list names with `emulator -list-avds`:

```sh
~/Android/Sdk/emulator/emulator -avd Medium_Phone_API_35 -gpu host -qemu -cpu host
```

The CPU option helps ARM64 translation on an x86_64 image whose default virtual
CPU may lack AES. The image must support ARM64; check
`adb shell getprop ro.product.cpu.abilist`. An ARM64 phone with USB debugging
needs no emulator.

After building a development APK:

```sh
~/Android/Sdk/platform-tools/adb install -r dist/android/meteo-pl-arm64-v8a-development.apk
~/Android/Sdk/platform-tools/adb shell am start -n pl.meteo.desktop/org.qtproject.qt.android.bindings.QtActivity
```

Changing signing keys requires removing the previous incompatible test APK,
which deletes its local settings. Add `adb -s SERIAL` when several devices are
connected.

Pinch charts to change the time scale and drag horizontally to move the interval.
Vertical gestures scroll the chart list. Double-tap restores the whole forecast.
The minimum interval is 24 hours, clamped to the full forecast when shorter.
Mobile initially shows the first 72 hours.

In **Synoptic**, swipe right for the previous day or left for the next, bounded
by today. Vertical gestures scroll the content. System Back closes menus,
unwinds settings pages and returns from Maps or News to the forecast. An open
article first returns to its list. The keyboard is dismissed before navigating
away from a form.

**Use my location** requests approximate location permission while using the app.
After denial, change permission in the system's application settings. Place
search and map selection do not require this permission.

## GitHub Actions

`portable-packages.yml` runs native Windows/macOS jobs and builds Android in a
pinned SDK image. Artifacts are uploaded after the job succeeds. Craft bootstrap
and blueprints are pinned by revision in the script; the external KDE binary
cache can change. The macOS job uses GWDG's mirror to obtain a complete dependency
index while KDE's primary cache is rebuilding. Manual dispatch offers
**include_alternatives** for additional formats.

The setup follows [KDE Craft](https://develop.kde.org/docs/getting-started/building/craft/),
[Qt/Kirigami Android porting](https://develop.kde.org/docs/packaging/android/porting_applications/basic/)
and [building APKs in KDE's official container](https://develop.kde.org/docs/packaging/android/building_applications/).
See [releasing](releasing.md) for personally signed Linux/Android publication.
