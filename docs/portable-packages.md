# Windows, macOS i Android

`scripts/build-portable.py` buduje aplikację z bibliotekami Qt, Kirigami,
ikonami Breeze i zależnościami KDE przy użyciu KDE Craft. Plasmoid jest
wyłączony na tych platformach.

| Platforma | Środowisko budowania | Wynik |
| --- | --- | --- |
| Windows x64 | Windows, Visual Studio 2022, Python ≥ 3.11 | instalator NSIS `.exe` |
| macOS | macOS, Xcode z macOS SDK ≥ 15, Python ≥ 3.11 | `.dmg` z aplikacją `.app`, architektura komputera budującego |
| Android arm64 | Linux z Dockerem, oficjalny obraz SDK KDE/Qt 6.11 | instalowalny `-signed.apk` z własnym kluczem lub testowy `-development.apk` |

## Budowanie

Na Windows lub macOS, w katalogu projektu:

```sh
python -m pip install certifi
python -m pip install pefile==2024.8.26 macholib==1.16.3
python scripts/build-portable.py --target windows
# Na macOS:
python scripts/build-portable.py --target macos
```

Na Linuksie z Dockerem:

```sh
python3 scripts/build-portable.py --target android
```

Wyniki znajdują się w `dist/windows`, `dist/macos` lub `dist/android`, wraz
z `SHA256SUMS` i `BUILD-REPORT.json`. Katalog roboczy jest wielokrotnie używany.
Opcja `--include-alternatives` dołącza także przenośny ZIP Windows albo
niepodpisany APK Androida. ZIP jest zawsze budowany i sprawdzany przed
publikacją instalatora; domyślnie nie dubluje go w pobieranym zestawie.
Gdy automatycznie wybrany mirror nie odpowiada, można podać np.
`--cache-url https://ftp.gwdg.de/pub/linux/kde/extrafiles/craft/Qt6/`
lub zmienną `METEO_CRAFT_CACHE_URL`.
Można podać `--work-dir`, `--craft-root` oraz `--dist-dir`. Na Windows warto
wybrać krótką ścieżkę, np. `--craft-root C:/MeteoCraft`, ze względu na długość
ścieżek kompilatora. Pierwszy build pobiera duży zestaw Qt/KDE i SDK;
potrzebuje kilku–kilkunastu GB oraz dostępu do cache `files.kde.org`.

Natywne aplikacje okienkowe są uruchamiane z `--smoke-test` przed pakowaniem
i ponownie z rozpakowanego ZIP lub zamontowanego DMG, bez ścieżek bibliotek
środowiska Craft.
Test odrzuca błędy ładowania QML. Raport Androida zapisuje brak testu uruchomienia
na urządzeniu; samo utworzenie APK nie oznacza sprawdzenia działania na telefonie.
Porównania CPU/GPU z [testów wizualnych](renderer-tests.md) są obecnie wykonywane
na Linuksie, a nie na backendach D3D/Metal ani na telefonie.

## Rozmiar paczek

Przed archiwizacją `bundle_runtime.py` usuwa przykłady SDK, motywy kursorów,
pluginy deweloperskie i nieużywane moduły QML. Dla Windows i macOS odczytuje
zależności PE/Mach-O i zachowuje ich domknięcie, pluginy QML i platformy oraz
biblioteki TLS ładowane dynamicznie. Backend multimediów, SQL lub syntezy mowy
pozostaje, jeśli wymaga go zachowana biblioteka Qt. Usuwany jest wyłącznie
roboczy zbiór plików paczki, a nie SDK Craft.

Tłumaczenia zależności odpowiadają językom aplikacji: obecnie angielski i polski.
Ikony Breeze, licencje i zasoby zachowanych bibliotek pozostają w paczce.
`BUILD-REPORT.json` zawiera rozmiary przed i po tej operacji oraz listę usuniętych
bibliotek. Gotowe archiwum nadal musi przejść test uruchomienia.
Raport podaje również rzeczywisty rozmiar każdego publikowanego pliku.

Artefakt GitHub Androida korzysta z kompresji ZIP. Nie zmienia to bajtów ani
podpisu APK, którego biblioteki natywne pozostają w formie umożliwiającej
bezpośrednie mapowanie do pamięci przez Androida. Rozmiar pobieranego artefaktu
może więc być mniejszy niż rozmiar wyodrębnionego APK
([opis pakowania bibliotek Androida](https://developer.android.com/topic/performance/reduce-apk-size#reduce-native-binaries)).

## Podpisy i publikowanie

Instalator Windows nie jest podpisany Authenticode. Aplikacja macOS nie jest
notaryzowana i może wymagać lokalnego podpisu oraz zezwolenia Gatekeeper.
Nie są potrzebne konta ani klucze sklepów do utworzenia tych artefaktów.

Android obsługuje własny, stały klucz podpisujący, dzięki któremu kolejne APK
można instalować jako aktualizacje. Bez konfiguracji klucza powstaje testowy
APK z nowym, tymczasowym podpisem przy każdym budowaniu. Zmiana takiego
podpisu na stały może wymagać jednorazowego odinstalowania poprzedniej wersji,
co usuwa jej ustawienia. Narzędzie nie publikuje w sklepach z aplikacjami.

### Stały podpis Androida

Klucz przechowuj **poza repozytorium**, z kopią zapasową w bezpiecznym miejscu.
Nie można go odtworzyć z APK. Dla dystrybucji własnych APK utrata klucza oznacza
utratę możliwości podpisania zgodnej aktualizacji
([podpisywanie aplikacji Android](https://developer.android.com/studio/publish/app-signing)).
Warto używać osobnego klucza dla każdej aplikacji.

Jeśli nie masz jeszcze klucza, utwórz go interaktywnie poleceniem `keytool`
z JDK. Hasło wpisujesz w terminalu, a nie w argumentach polecenia:

```bash
install -d -m 700 "$HOME/.local/share/meteo-signing"
export METEO_ANDROID_KEYSTORE="$HOME/.local/share/meteo-signing/meteo.p12"
export METEO_ANDROID_KEY_ALIAS=meteo
keytool -genkeypair -keystore "$METEO_ANDROID_KEYSTORE" -storetype PKCS12 \
  -alias "$METEO_ANDROID_KEY_ALIAS" -keyalg RSA -keysize 3072 -validity 10000 \
  -dname "CN=Kamil Piwowarski"
chmod 600 "$METEO_ANDROID_KEYSTORE"
```

Przed budowaniem ustaw trzy zmienne. Dla istniejącego klucza podaj jego
rzeczywistą ścieżkę i alias. Opcjonalna `METEO_ANDROID_KEY_PASSWORD` określa
inne hasło klucza; domyślnie używane jest hasło magazynu:

```bash
export METEO_ANDROID_KEYSTORE="$HOME/.local/share/meteo-signing/meteo.p12"
export METEO_ANDROID_KEY_ALIAS=meteo
read -r -s -p 'Hasło magazynu: ' METEO_ANDROID_STORE_PASSWORD
printf '\n'
export METEO_ANDROID_STORE_PASSWORD
python3 scripts/build-portable.py --target android --require-release-signing
unset METEO_ANDROID_STORE_PASSWORD METEO_ANDROID_KEY_PASSWORD
```

Powstaje `-signed.apk`. Magazyn jest montowany do kontenera tylko do odczytu;
nie trafia do źródeł, cache budowania ani artefaktów. Hasła są przekazywane
przez środowisko, bez wypisywania ich w poleceniach. Niepełna konfiguracja
przerywa budowanie zamiast użyć innego klucza. Opcja `--require-release-signing`
wymaga stałego podpisu także wtedy, gdy zapomnisz ustawić wszystkie zmienne.

`apksigner verify --verbose --print-certs` sprawdza wynik i wypisuje publiczny
odcisk certyfikatu. `BUILD-REPORT.json` podaje `releaseSigned: true` dla APK
podpisanego skonfigurowanym kluczem. Publiczny certyfikat można udostępniać,
ale plik magazynu i jego hasła muszą pozostać prywatne.

### Podpis Androida w GitHub Actions

W ustawieniach repozytorium **Secrets and variables → Actions** skonfiguruj:

| Secret | Zawartość |
| --- | --- |
| `ANDROID_KEYSTORE_BASE64` | Magazyn klucza zakodowany Base64 |
| `ANDROID_KEY_ALIAS` | Alias klucza, np. `meteo` |
| `ANDROID_STORE_PASSWORD` | Hasło magazynu |
| `ANDROID_KEY_PASSWORD` | Opcjonalne inne hasło klucza |

Przykład przesłania magazynu przez standardowe wejście, bez wyświetlania go:

```bash
base64 -w 0 "$METEO_ANDROID_KEYSTORE" | gh secret set ANDROID_KEYSTORE_BASE64 --repo kapiwko/meteo-pl
```

Pozostałe wartości można wpisać interaktywnie przez `gh secret set NAZWA`.
Base64 nie jest szyfrowaniem; ochronę zapewniają GitHub Actions secrets.
Workflow udostępnia klucz wyłącznie przy budowaniu zaufanej gałęzi lub ręcznym
uruchomieniu. Pull requesty zawsze budują z kluczem tymczasowym, także PR-y
z tego samego repozytorium. Plik magazynu jest usuwany po budowaniu.
Bez sekretów dotychczasowe paczki testowe nadal powstają.
Więcej: [sekrety GitHub Actions](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/use-secrets).

Windows wymaga osobnego podpisu Authenticode. Dla projektu open source warto
sprawdzić program SignPath Foundation; dla zwykłej dystrybucji potrzebny jest
zaufany certyfikat lub usługa podpisywania. Sam podpis nie gwarantuje braku
ostrzeżeń SmartScreen
([opcje podpisu Windows](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options)).
macOS wymaga certyfikatu Developer ID z Apple Developer Program i notaryzacji
do standardowej dystrybucji poza App Store
([Developer ID](https://developer.apple.com/developer-id/)).
Te tożsamości nie są jeszcze skonfigurowane w workflow projektu.

Port Androida korzysta ze stylu Material (Material 3 w Qt Quick Controls),
z jasnym lub ciemnym motywem systemowym. Paczka sprawdza obecność pluginu
Material przed publikacją APK. Ikony nadal pochodzą z Breeze.
Manifest zawiera uprawnienia sieci,
lokalizacji i powiadomień w manifeście. Eksport i import przez Android Storage
Access Framework oraz uprawnienia czasu wykonania wymagają osobnej weryfikacji
na urządzeniu; manifest nie zastępuje takiego testu.

Ikony używane przez aplikację i standardowe komponenty są dołączane przez
`kirigami_package_breeze_icons()` z przypiętego wydania Breeze. Pakowanie sprawdza
obecność motywu i podstawowych ikon w gotowym APK, przed jego publikacją.

## Lokalne uruchamianie Androida

Uruchom istniejący emulator (nazwy sprawdzisz przez `emulator -list-avds`):

```sh
~/Android/Sdk/emulator/emulator -avd Medium_Phone_API_35 -gpu host -qemu -cpu host
```

Opcja procesora jest przydatna dla APK ARM64 uruchamianego przez translację
w obrazie x86_64: domyślny procesor wirtualny może nie udostępniać AES.
Obraz musi obsługiwać ARM64; sprawdź `adb shell getprop ro.product.cpu.abilist`.
Telefon ARM64 z włączonym debugowaniem USB nie wymaga emulatora.

Po zbudowaniu APK:

```sh
~/Android/Sdk/platform-tools/adb install -r dist/android/meteo-pl-arm64-v8a-development.apk
~/Android/Sdk/platform-tools/adb shell am start -n pl.meteo.desktop/org.qtproject.qt.android.bindings.QtActivity
```

Przy zmianie klucza podpisującego poprzedni testowy APK wymaga odinstalowania;
usuwa to jego lokalne ustawienia. Przy kilku urządzeniach dodaj `adb -s SERIAL`.

Na wykresie rozsuń lub zsuń dwa palce, aby zmienić skalę czasu; przesuwaj palcem
w poziomie, aby zmienić okres. Ruch pionowy przewija listę wykresów.
Podwójne dotknięcie przywraca całą prognozę. Minimalny zakres pozostaje równy
24 godzinom (lub całej prognozie, jeśli jest krótsza).

W Synoptyku gest w prawo otwiera poprzedni dzień, a w lewo następny, do dnia
dzisiejszego. Ruch pionowy przewija treść. Systemowe Wstecz zamyka menu,
cofa strony ustawień i wraca z Map lub Aktualności do prognozy; z treści
artykułu wraca najpierw do listy.

Przycisk „Użyj mojej pozycji” prosi o systemową zgodę na przybliżoną lokalizację
podczas używania aplikacji. Po odmowie zgodę można zmienić w ustawieniach
systemowych aplikacji. Wyszukiwanie miejsca i wybór punktu na mapie działają
bez tego uprawnienia.

## GitHub Actions

Workflow `portable-packages.yml` uruchamia natywne zadania Windows i macOS oraz
budowę Androida w przypiętym obrazie SDK. Artefakty są udostępniane po poprawnym
zakończeniu zadania. Bootstrap Craft i blueprinty są wskazane przez rewizje
w skrypcie. Cache binarny zależności KDE jest zewnętrzny i może się zmieniać.
Zadanie macOS używa mirrora GWDG, aby korzystać z pełnego indeksu zależności
podczas przebudowy głównego cache KDE.
Przy ręcznym uruchomieniu opcja **include_alternatives** dołącza dodatkowe formaty.

Podstawa konfiguracji: [KDE Craft](https://develop.kde.org/docs/getting-started/building/craft/),
[portowanie Qt/Kirigami na Androida](https://develop.kde.org/docs/packaging/android/porting_applications/basic/)
oraz [budowanie APK w oficjalnym kontenerze KDE](https://develop.kde.org/docs/packaging/android/building_applications/).
