# Meteo Desktop

Aplikacja Qt 6 / Kirigami oraz plasmoid Plasma 6 do przeglądania prognoz
ICM UW z meteo.pl. Wspólny interfejs oferuje rozbudowane meteogramy, prognozę
godzinową i dzienną, mapy oraz komentarz synoptyka i artykuły.

## Uruchomienie

Wymagania: CMake ≥ 3.25, kompilator C++23, Ninja, Qt ≥ 6.5
(Core, Network, Qml, Quick, QuickControls2, Positioning, Test), Kirigami 6 i moduł
QQC2 Desktop Style (`org.kde.desktop`).
Plasmoid wymaga środowiska Plasma 6 i modułów QML Plasma.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/apps/meteo/meteo-pl
```

Przycisk **Lokalizacja** otwiera wyszukiwarkę miast i wsi z opisem regionu,
ulubione i ostatnio wybrane miejsca. Można także wskazać punkt na mapie,
pobrać pozycję przez systemową usługę lokalizacji lub wpisać współrzędne.
GPS wymaga dostępnego dostawcy Qt Positioning; bez niego można użyć wyszukiwarki
lub mapy. Wyszukiwanie domyślnie obejmuje Polskę, opcjonalnie cały świat.
Dostępne modele: UM4 60 h, UM4 120 h i GFS 240 h.
UM4 obejmuje obszar 44,5–66,482°N i 8,21–30,176°E; GFS jest globalny.
Współrzędne przyjmują kropkę lub przecinek dziesiętny.

Podsumowanie pokazuje prognozę na oznaczoną godzinę, a nie pomiar stacji.
Oś czasu używa strefy systemu lub UTC. Wskaźnik czasu bieżącego jest przerywany.
Najechanie pokazuje wspólny odczyt parametrów dla wybranej godziny; kliknięcie
przypina odczyt, a Escape go zamyka. Wykresy mają zakresy min/max, temperaturę
odczuwalną, rodzaj i prawdopodobieństwo opadów, kierunki i porywy wiatru,
warstwy oraz podstawy chmur, wilgotność, mgłę, widzialność i wskaźniki burz.
Brakujące wartości pozostają lukami. Dostępność parametrów zależy od modelu.

Domyślny profil **Klasyczny ICM** zawiera osiem wykresów: temperaturę, opady
z wilgotnością, szansę opadu i burze, ciśnienie, prędkość wiatru, kierunek
wiatru, pionowy zasięg chmur z widzialnością oraz zachmurzenie z mgłą.
Temperatura powietrza i odczuwalna mają półprzezroczyste zakresy min–max.
Zakres temperatury przy gruncie przedstawiają pionowe kreski od minimum do
maksimum dla każdej godziny prognozy. Błękitne tło wyróżnia obszar poniżej 0°C.
Kolory linii odpowiadają palecie klienta Meteo.
Siatka pozostaje widoczna nad wypełnieniami, a mgła nad chmurami.
Linie siatki są delikatne. Etykiety czasu na wąskich wykresach pomijają minuty
i w razie potrzeby pojawiają się rzadziej, bez nakładania się godzin i dat.

Domyślny renderer **GPU** przekazuje geometrię wykresów bezpośrednio do sceny
Qt Quick (RHI), z wygładzaniem MSAA. W ustawieniach **Wygląd → Rysowanie
wykresów** można od razu przełączyć na **CPU · Canvas**. Oba renderery używają
tych samych obliczeń, kolorów, warstw i obsługi kursora. Bez akceleracji sceny
Qt Quick aplikacja automatycznie korzysta z CPU.

W ustawieniach widoku można tworzyć, nazywać i usuwać własne profile,
tworzyć i usuwać wykresy, zmieniać ich nazwy oraz kolejność. Każdy wykres może
łączyć dowolne serie z całego katalogu, np. temperaturę z opadem na prawej osi.
Przy każdej serii jest próbka legendy i edytor wyglądu: sposób rysowania,
kolory dla jasnego i ciemnego motywu, grubość i wzór linii, rozmiar punktów,
krycie oraz przypisanie do osi. Kolor można wybrać z palety lub wpisać kod.
Wygląd i opcjonalne granice obu osi zapisują się oddzielnie w każdym profilu.
Domyślne wykresy są gotową konfiguracją używającą tego samego renderera.
Przyciski **Importuj…**, **Eksportuj profil…** i **Eksportuj zapisane…**
przenoszą profile w pliku JSON, razem z wyglądem serii i zakresami osi.
Import dodaje nowe profile; zbieżne nazwy otrzymują numer, a istniejące
profile pozostają zachowane. Eksport zapisanych obejmuje nazwane własne
profile i zmodyfikowany „Własny układ”. Ulubione lokalizacje i ustawienia
globalne, takie jak jednostka wiatru i UTC, nie są częścią pliku profili.
Dotyczy to również wyboru renderera, wspólnego dla aplikacji i plasmoidu.
Można też wybrać jednostkę wiatru i cieniowanie nocy. Legenda przełącza serie
bez zmiany skali osi. Ctrl + kółko myszy przybliża czas; przyciski i suwak
pozwalają przesuwać zakres. Wybory na pasku są opisane jako **Profil wykresów**,
**Zakres czasu** i **Start modelu**. Przyciski **Więcej godzin** / **Mniej godzin**
zmieniają długość widocznego przedziału, a **Wcześniej** / **Później** go przesuwają.
Lista startów modelu udostępnia krótkoterminowe archiwum publikowane przez serwer.

Obecny interfejs danych liczbowych nie udostępnia osobnego pola opadu
konwekcyjnego. Opad średni, maksymalny, prawdopodobieństwo opadu i wskaźniki
burz są prezentowane zgodnie z ich znaczeniem. Czerwona kropka przy górnej
krawędzi oznacza przekroczenie skali przez opad maksymalny lub poryw wiatru;
nie jest oznaczeniem konwekcji. Szczegóły porównania wykresów z klientem webowym:
[docs/chart-parity.md](docs/chart-parity.md).

Zakładka **Mapy** zawiera interaktywne warstwy UM4/GFS i 19 map tematycznych
UM/WAM. Można zmieniać warstwę, start, godzinę i animować prognozę, a z mapy
otworzyć meteogram wskazanego punktu. Mapy UM4 korzystają z publikacji UM4
serwisu mapowego, niezależnej od listy startów meteogramów.

Podczas ładowania pozostaje poprzednia kompletna klatka, również przy błędzie
sieci. Animacja czeka na kolejną klatkę i pobiera następną z wyprzedzeniem.
Kafelki zachowują współrzędne podczas przesuwania i skalowania; nowa klatka
pojawia się jednocześnie po przygotowaniu wszystkich jej kafelków.
Kółko myszy i przewijanie touchpadem przybliżają mapę pod kursorem.
Wskaźnik ładowania jest nakładany na mapę.

Prognoza odświeża się co godzinę. Aplikacja i widget korzystają ze wspólnego
cache w `$XDG_CACHE_HOME/meteo-pl` (domyślnie `~/.cache/meteo-pl`).
Niedawno zapisana prognoza ogranicza powtórne pobrania przy otwieraniu kolejnego
widoku. Ręczne **Odśwież** zawsze pyta serwer. Gdy pobieranie się nie powiedzie,
widoczna pozostaje zapisana prognoza z datą pobrania i komunikatem.

Mapy mają dodatkowy cache: do 96 MiB zdekodowanych obrazów w RAM i do 256 MiB
cache HTTP na dysku. Powtórne żądania tego samego obrazu w procesie są łączone.
Klucz zawiera pełny URL, w tym warstwę, start, krok i współrzędne kafelka;
ważność odpowiedzi dyskowych wynika z nagłówków serwera.
Cache map znajduje się w `$XDG_CACHE_HOME/meteo-pl/maps/<nazwa-procesu>`.
Aplikacja i Plasma mają osobne katalogi cache HTTP, aby dwa procesy nie
modyfikowały równocześnie magazynu `QNetworkDiskCache`.
Ulubione i preferencje widoku są wspólne, w `~/.config/meteo-pl/desktop.ini`.
Ścieżka jest też widoczna na dole ustawień wyglądu; respektuje `XDG_CONFIG_HOME`.

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

Nie jest to jeszcze pełna zgodność funkcji z Androidem: brak radaru obserwacyjnego,
FLOPRES, powiadomień i automatycznego GPS przy starcie oraz integracji konta
i subskrypcji. Mapy opadów w tej aplikacji przedstawiają prognozę modelu.
