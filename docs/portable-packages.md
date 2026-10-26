# Windows, macOS i Android

`scripts/build-portable.py` buduje aplikację z bibliotekami Qt, Kirigami,
ikonami Breeze i zależnościami KDE przy użyciu KDE Craft. Plasmoid jest
wyłączony na tych platformach.

| Platforma | Środowisko budowania | Wynik |
| --- | --- | --- |
| Windows x64 | Windows, Visual Studio 2022, Python ≥ 3.11 | instalator NSIS `.exe` |
| macOS | macOS, Xcode z macOS SDK ≥ 15, Python ≥ 3.11 | `.dmg` z aplikacją `.app`, architektura komputera budującego |
| Android arm64 | Linux z Dockerem, oficjalny obraz SDK KDE/Qt 6.11 | instalowalny `-development.apk` |

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

Android dostarcza APK podpisany tymczasowym kluczem deweloperskim, zweryfikowany przez
`apksigner verify`. Klucz nie trafia do artefaktów. Nowy build ma nowy klucz,
więc aktualizacja zainstalowanej wersji deweloperskiej może wymagać jej
odinstalowania. Do stałych aktualizacji trzeba użyć własnego, niezmiennego
klucza wydania. Niepodpisany plik do własnego podpisu jest dostępny przez
`--include-alternatives`. Narzędzie nie publikuje w Google Play, Microsoft Store ani App Store.

Port Androida korzysta z mobilnego stylu Breeze i zawiera uprawnienia sieci,
lokalizacji i powiadomień w manifeście. Eksport i import przez Android Storage
Access Framework oraz uprawnienia czasu wykonania wymagają osobnej weryfikacji
na urządzeniu; manifest nie zastępuje takiego testu.

## GitHub Actions

Workflow `portable-packages.yml` uruchamia natywne zadania Windows i macOS oraz
budowę Androida w przypiętym obrazie SDK. Artefakty są udostępniane po poprawnym
zakończeniu zadania. Bootstrap Craft i blueprinty są wskazane przez rewizje
w skrypcie. Cache binarny zależności KDE jest zewnętrzny i może się zmieniać.
Przy ręcznym uruchomieniu opcja **include_alternatives** dołącza dodatkowe formaty.

Podstawa konfiguracji: [KDE Craft](https://develop.kde.org/docs/getting-started/building/craft/),
[portowanie Qt/Kirigami na Androida](https://develop.kde.org/docs/packaging/android/porting_applications/basic/)
oraz [budowanie APK w oficjalnym kontenerze KDE](https://develop.kde.org/docs/packaging/android/building_applications/).
