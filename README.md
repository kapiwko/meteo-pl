# Meteo Desktop

Aplikacja Qt 6 / Kirigami oraz plasmoid Plasma 6 do przeglądania prognoz
ICM UW z meteo.pl. Pierwsza wersja wyświetla natywne wykresy temperatury
i punktu rosy, opadów, wiatru i porywów, ciśnienia oraz zachmurzenia.

## Uruchomienie

Wymagania: CMake ≥ 3.25, kompilator C++23, Ninja, Qt ≥ 6.5
(Core, Network, Qml, Quick, QuickControls2, Test), Kirigami 6 i moduł
QQC2 Desktop Style (`org.kde.desktop`).
Plasmoid wymaga środowiska Plasma 6 i modułów QML Plasma.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/apps/meteo/meteo-pl
```

Przycisk **Lokalizacja** pozwala wybrać jedno z kilku miast lub wpisać nazwę
i współrzędne własnego punktu. Dostępne modele: UM4 60 h, UM4 120 h i GFS 240 h.
UM4 obejmuje obszar 44,5–66,482°N i 8,21–30,176°E; GFS jest globalny.
Współrzędne przyjmują kropkę lub przecinek dziesiętny.

Podsumowanie pokazuje prognozę na oznaczoną godzinę, a nie pomiar stacji.
Oś czasu używa strefy czasowej systemu. Wskaźnik czasu bieżącego jest przerywany,
a wskazanie kursorem ujawnia wartości punktów. Brakujące wartości pozostają
lukami na wykresach.

Prognoza odświeża się co godzinę. Aplikacja i widget korzystają ze wspólnego
cache w `$XDG_CACHE_HOME/meteo-pl` (domyślnie `~/.cache/meteo-pl`).
Niedawno zapisana prognoza ogranicza powtórne pobrania przy otwieraniu kolejnego
widoku. Ręczne **Odśwież** zawsze pyta serwer. Gdy pobieranie się nie powiedzie,
widoczna pozostaje zapisana prognoza z datą pobrania i komunikatem.

## Instalacja

Preset `dev` instaluje do `build/stage`, aby umożliwić sprawdzenie pakietu:

```sh
cmake --install build/dev
QML_IMPORT_PATH="$PWD/build/stage/lib/qt6/qml" ./build/stage/bin/meteo-pl
```

Do normalnej instalacji aplikacji i plasmoidu skonfiguruj osobny katalog:

```sh
cmake -S . -B build/release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF
cmake --build build/release
sudo cmake --install build/release
```

Następnie dodaj **Meteo · meteogram ICM** w oknie „Dodaj elementy interfejsu”
Plasma. Nowa sesja Plasma zapewni wykrycie nowego modułu QML.
Ustawienia widgetu są niezależne dla każdej instancji.
Samo `kpackagetool6 -i integrations/plasma/package` nie instaluje biblioteki C++;
potrzebna jest także instalacja modułu `pl.meteo.desktop` przez CMake.
Ścieżkę modułu można ustawić przez `METEO_QML_INSTALL_DIR`, np. dla dystrybucji
używających `lib64` zamiast `lib`.

Opcje `METEO_BUILD_APP` i `METEO_BUILD_PLASMOID` wyłączają odpowiednie frontendy.
`BUILD_TESTING=OFF` usuwa zależność od Qt Test.

## Architektura i źródła

Podział na wspólny backend i opcjonalną integrację KDE nawiązuje do lokalnego
projektu `../btrfs-backup`. Szczegóły: [docs/architecture.md](docs/architecture.md).

Punkty odniesienia:

- [Klient webowy meteo.pl](https://app.meteo.pl/meteogram) — format danych i modele.
- [Meteo ICM w Google Play](https://play.google.com/store/apps/details?id=pl.janpogocki.icm.pogoda&hl=pl) — funkcje i prezentacja meteogramów.
- [Meteo API](https://api.meteo.pl/) — osobna usługa dostępu do danych.

Aktualny adapter używa interfejsu klienta webowego
`https://devmgramapi.meteo.pl/meteorograms/…`, sprawdzonego 25.10.2026.
To nieudokumentowany interfejs, który może się zmienić. Nie jest to integracja
z kontem ani płatnym API. Zasady wykorzystania prognoz określa
[regulamin meteo.pl](https://www.meteo.pl/regulamin-aplikacji-meteo-icm).
Repozytorium zawiera wyłącznie syntetyczne dane testowe, a interfejs podaje źródło prognozy.

Poza zakresem tej wersji: radar, mapa lokalizacji, wyszukiwanie wszystkich
miejscowości, GPS, archiwum i profile parametrów meteogramu.

Modele QML udostępniają wyszukiwanie nazw miejscowości, regiony, ulubione, ostatnie miejsca oraz zapisywane profile wykresów.

Model prezentacji udostępnia archiwalne starty, dane godzinowe i dzienne, inspektor parametrów oraz pory wschodu i zachodu słońca.

Mapy UM4/GFS i obrazy tematyczne mają buforowane klatki, cache obrazów, przeciąganie oraz powiększanie kółkiem myszy.
