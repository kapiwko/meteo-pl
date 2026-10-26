# Meteo — przewodnik użytkownika

Aplikacja Qt 6 / Kirigami oraz plasmoid Plasma 6 do przeglądania prognoz
ICM UW z meteo.pl. Wspólny interfejs oferuje rozbudowane meteogramy, prognozę
godzinową i dzienną, mapy oraz komentarz synoptyka i artykuły.
Karty nawigacji mają ikony; w wąskim widoku pozostają same ikony z podpowiedziami.

Kod aplikacji i plasmoidu: © 2026 Kamil Piwowarski, licencja
[GPL-3.0-or-later](../LICENSE) — GNU GPL w wersji 3 lub dowolnej późniejszej.
Biblioteki i dane pogodowe zachowują własne warunki udostępniania.

## Uruchomienie

Wymagania: CMake ≥ 3.25, kompilator C++23, Ninja, Qt ≥ 6.5
(Core, Network, Qml, Quick, QuickControls2, Positioning, Test), Kirigami 6 i moduł
QQC2 Desktop Style (`org.kde.desktop`).
Powiadomienia po eksporcie korzystają z KDE Frameworks 6 Notifications;
informacje o aplikacji korzystają z gotowych komponentów Kirigami.
Aplikacja okienkowa wymaga również Qt Widgets do natywnych dialogów KDE.
Plasmoid wymaga środowiska Plasma 6 i modułów QML Plasma.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/apps/meteo/meteo-pl
```

Przycisk **Lokalizacja** otwiera wyszukiwarkę miast i wsi z opisem regionu
oraz wspólną listę **Moje miejsca**. Można także wskazać punkt na mapie,
pobrać pozycję przez systemową usługę lokalizacji lub wpisać współrzędne.
GPS wymaga dostępnego dostawcy Qt Positioning; bez niego można użyć wyszukiwarki
lub mapy. Wyszukiwanie domyślnie obejmuje Polskę, opcjonalnie cały świat.
Każda otwarta lokalizacja jest zapisywana w **Moich miejscach**. Przy kilku miejscach
nazwa w nagłówku staje się polem wyboru, także w kompaktowym widoku widgetu.
Listą zarządzasz w oknie lokalizacji; usunięcie wybranego miejsca przełącza dany
widok na następne, a usunięcie ostatniego pokazuje wybór nowej lokalizacji.
Aplikacja i każdy widget zachowują własny wybór z tej wspólnej listy.
Dotychczasowe ulubione i ostatnie miejsca są zachowane przy aktualizacji.
Mapa interaktywna oraz mapa w formularzu pokazują zapisane miejsca z etykietami.
Wybrane miejsce jest wyróżnione; kliknięcie etykiety wybiera jego prognozę.
Dostępne modele: UM4 60 h, UM4 120 h i GFS 240 h.
UM4 obejmuje obszar 44,5–66,482°N i 8,21–30,176°E; GFS jest globalny.
Współrzędne przyjmują kropkę lub przecinek dziesiętny.
**Ustawienia → O aplikacji** pokazuje autorstwo Kamila Piwowarskiego,
źródła danych i informacje o aktualnie pobranej prognozie.

Nagłówek grupuje lokalizację i prognozę na oznaczoną godzinę, temperaturę,
wiatr i ciśnienie oraz parę godzin wschodu i zachodu słońca z ikonami.
Godziny słońca dotyczą bieżącego dnia prognozy, a przy archiwum — oznaczonej daty.
Współrzędne i rozwijane szczegóły nie zajmują miejsca nad wykresami;
model i czas obliczeń są w ustawieniach i informacjach o aplikacji.
Podsumowanie pokazuje prognozę na oznaczoną godzinę, a nie pomiar stacji.
Oś czasu używa strefy systemu lub UTC. Wskaźnik czasu bieżącego jest przerywany.
Domyślnie wykresy nie pokazują odczytu pod kursorem. Próbnik (ikona pipety)
na pasku pod wykresami włącza tryb identyfikacji. W tym trybie najechanie
pokazuje parametry, kliknięcie przypina odczyt, a Escape go zamyka.
Wyłączenie próbnika zamyka także przypięty odczyt.
W ustawieniach można wybrać parametry wskazanego wykresu lub wszystkie parametry.
Wszystkie parametry mają karty kategorii; przy szerokim oknie odczyt używa dwóch kolumn.
Przycisk eksportu w odczycie zapisuje aktualną kategorię (lub wszystkie parametry)
do PNG albo CSV, także wiersze poza widocznym obszarem przewijania.
CSV zawiera czas ze strefą, lokalizację, model, klucze, wartości liczbowe i jednostki;
separator to średnik, liczby używają kropki, a brakujące wartości pozostają puste. Wykresy mają zakresy min/max, temperaturę
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
Qt Quick (RHI), z wygładzaniem MSAA. W **Ustawienia → Aplikacja → Rysowanie wykresów** można od razu przełączyć na **CPU · Canvas**. Oba renderery używają
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
Wybór pliku korzysta ze standardowego okna środowiska: w KDE z dialogu KDE,
a w środowisku używającym portalu z XDG Desktop Portal. Aplikacja używa
`QApplication`, wymaganego przez natywną integrację dialogów KDE.
Import dodaje nowe profile; zbieżne nazwy otrzymują numer, a istniejące
profile pozostają zachowane. Eksport zapisanych obejmuje nazwane własne
profile i zmodyfikowany „Własny układ”. Ulubione lokalizacje i ustawienia
globalne, takie jak jednostka wiatru i UTC, nie są częścią pliku profili.
Dotyczy to również wyboru renderera, wspólnego dla aplikacji i plasmoidu.
Wysokość można ustawić ogólnie, nadpisać w profilu i osobno dla wykresu.
Obowiązuje pierwsza ustawiona wartość: wykres → profil → ustawienie ogólne.
Wysokości profilu i wykresów są zawarte w eksporcie. Przycisk **ⓘ** bezpośrednio
przy tytule pokazuje lub ukrywa legendę tego wykresu; globalna opcja
**Domyślnie pokazuj legendę** określa stan początkowy.
Można też wybrać jednostkę wiatru i cieniowanie nocy. Legenda przełącza serie
bez zmiany skali osi. Kliknięcie prawym przyciskiem na wykresie otwiera menu
**Zapisz wykres jako…** oraz listę serii z checkboxami zsynchronizowanymi z legendą.
Menu działa również w plasmoidzie i jest dostępne z klawiatury klawiszem Menu
lub Shift+F10. Zapis PNG zachowuje widoczne serie i wybrany zakres czasu.
Dolny pasek statusu pokazuje widoczny przedział czasu,
strzałki przesuwania, suwak powiększenia i przycisk dopasowania całej prognozy.
Powiększanie zatrzymuje się na 24 godzinach, a dla krótszej prognozy na jej
pełnym zakresie. Ctrl + kółko myszy również zmienia powiększenie. Podczas
oglądania fragmentu pojawia się drugi suwak do przesuwania czasu.
Przycisk **Ustawienia** w górnym pasku otwiera wspólne okno. Zakładka
**Prognoza** zawiera **Profil wykresów**, **Zakres czasu** i **Start modelu**,
**Profile** zawiera edytor wykresów i serii oraz import i eksport profili.
**Duplikuj profil** tworzy niezależną kopię z gotową nazwą, którą można od razu zmienić.
Zapisane własne profile można usuwać, a roboczy „Własny układ” wyczyścić.
Gotowe profile pozostają dostępne jako szablony.
**Odczyt** — zakres parametrów próbnika i domyślną widoczność legendy,
a **Aplikacja** — renderer, jednostki, strefę czasu i ogólną wysokość wykresów.
Formularze korzystają z `Kirigami.FormLayout`, standardowych grup opcji i kontrolek Qt Quick.
Padding jest wewnątrz kart; pasek kart dochodzi do krawędzi okna. Działania profili
korzystają z `Kirigami.ActionToolBar`, który przy małej szerokości pokazuje menu nadmiarowe.
Zakres i start dotyczą bieżącego widoku; wybór profilu jest zapamiętywany.
Lista startów modelu udostępnia krótkoterminowe archiwum publikowane przez serwer.

**Eksportuj…** w pasku statusu otwiera wybór wykresów: jeden, kilka lub cały
meteogram można zapisać jako wspólny PNG. Obraz zawiera także wykresy poniżej
widocznej części okna. Zachowuje bieżący zakres czasu, ukryte serie, legendy,
kolory i wysokości. Podpis podaje lokalizację, model, daty i źródło danych.
Plik wybiera się w systemowym oknie zapisu. Po zapisie można otworzyć obraz
przyciskiem w niebieskim pasku informacji Kirigami. Eksport wykresów i profili
wysyła również systemowe powiadomienie KDE z akcjami otwarcia pliku i folderu;
respektuje ustawienia powiadomień pulpitu. Eksport działa z rendererem GPU i CPU.

Obecny interfejs danych liczbowych nie udostępnia osobnego pola opadu
konwekcyjnego. Opad średni, maksymalny, prawdopodobieństwo opadu i wskaźniki
burz są prezentowane zgodnie z ich znaczeniem. Czerwona kropka przy górnej
krawędzi oznacza przekroczenie skali przez opad maksymalny lub poryw wiatru;
nie jest oznaczeniem konwekcji. Szczegóły porównania wykresów z klientem webowym:
[docs/chart-parity.md](chart-parity.md).

Zakładka **Mapy** zawiera interaktywne warstwy UM4/GFS i 19 map tematycznych
UM/WAM. Można zmieniać warstwę, start, godzinę i animować prognozę, a z mapy
otworzyć meteogram wskazanego punktu. Mapy UM4 korzystają z publikacji UM4
serwisu mapowego, niezależnej od listy startów meteogramów.

Podczas ładowania pozostaje poprzednia kompletna klatka, również przy błędzie
sieci. Przed odtwarzaniem animacja przygotowuje cały zestaw klatek w cache,
z postępem na mapie i możliwością anulowania. Pobieranie jest ograniczone do
czterech równoległych zadań i respektuje limity pamięci cache. Zmiana obszaru,
warstwy lub startu unieważnia poprzednie przygotowanie. W trakcie odtwarzania
następna klatka jest dodatkowo przygotowywana w pamięci RAM.
Kafelki zachowują współrzędne podczas przesuwania i skalowania; nowa klatka
pojawia się jednocześnie po przygotowaniu wszystkich jej kafelków.
Kółko myszy i przewijanie touchpadem przybliżają mapę pod kursorem.
Wskaźnik ładowania jest nakładany na mapę.
Mapy tematyczne domyślnie mieszczą się w obu wymiarach widoku; **Dopasuj**
przywraca pełny obraz po ręcznym powiększeniu.

Prognoza odświeża się co godzinę. Aplikacja i widget korzystają ze wspólnego
cache w `$XDG_CACHE_HOME/meteo-pl` (domyślnie `~/.cache/meteo-pl`).
Niedawno zapisana prognoza ogranicza powtórne pobrania przy otwieraniu kolejnego
widoku. Ręczne **Odśwież** zawsze pyta serwer. Gdy pobieranie się nie powiedzie,
widoczna pozostaje zapisana prognoza z datą pobrania i komunikatem.

Artykuły i komentarze synoptyka są zapisywane na dysku osobno dla każdej strony
listy i daty. Przez 30 minut ponowne otwarcie korzysta z zapisu bez sieci.
Ręczne odświeżenie sprawdza serwer; w razie błędu zachowuje zapisaną treść.
Ilustracje artykułów i legendy map korzystają z tego samego trwałego cache obrazów.
Wyniki wyszukiwania miejsc są zapamiętywane na 24 godziny, a listy dostępnych
uruchomień modeli na 5 minut. Cache tych odpowiedzi ma limit 32 MiB / 512 wpisów;
wyszukiwanie nowej nazwy nadal wymaga internetu.

Mapy mają dodatkowy cache: do 96 MiB zdekodowanych obrazów w RAM i do 256 MiB
cache HTTP na dysku. Powtórne żądania tego samego obrazu w procesie są łączone.
Klucz zawiera pełny URL, w tym warstwę, start, krok i współrzędne kafelka;
ważność odpowiedzi dyskowych wynika z nagłówków serwera. Obrazom bez jawnego
czasu ważności (m.in. mapom tematycznym) aplikacja nadaje 30 minut także na dysku.
Ponowne przygotowanie animacji i wznowienie po anulowaniu wykorzystują gotowe
klatki; usunięcie obrazu z RAM nie powoduje jego ponownego pobrania z sieci.
Cache map znajduje się w `$XDG_CACHE_HOME/meteo-pl/maps/<nazwa-procesu>`.
W **Ustawienia → Aplikacja → Pamięć podręczna** widać zajęte miejsce na dysku
z podziałem na prognozy, treści i obrazy. Przycisk czyszczenia usuwa cache dyskowy
i obrazy z RAM; pozostawia zapisane miejsca, profile i aktualnie widoczne dane.
Aplikacja i Plasma mają osobne katalogi cache HTTP, aby dwa procesy nie
modyfikowały równocześnie magazynu `QNetworkDiskCache`.
Ulubione i preferencje widoku są wspólne, w `~/.config/meteo-pl/desktop.ini`.
Ścieżka jest też widoczna na dole ustawień wyglądu; respektuje `XDG_CONFIG_HOME`.

## Instalacja

Do uruchamiania wersji z `build/dev` można zarejestrować wpis aplikacji i ikonę:
`python3 scripts/register-dev-desktop.py`. Wpis użytkownika wskazuje bezpośrednio
na zbudowany program i umożliwia rozpoznawanie aplikacji przez portal KDE.
Skrypt nie zastępuje istniejącego wpisu zwykłej instalacji.

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
Widok po rozwinięciu domyślnie pokazuje wszystkie wykresy profilu Klasyczny ICM na najbliższe 24 godziny.
W konfiguracji plasmoidu, w karcie **Widok widgetu**, wybierz profil
i okres 12/24/48/72 godzin lub całą prognozę. Widget pokazuje wszystkie włączone
wykresy profilu, każdy w osobno zwijanej sekcji. Możesz też wybrać zwarty
widok prognozy dziennej. Każdy widget zachowuje własną konfigurację;
Widok widgetu jest osadzony w standardowym kontenerze Plasmy.
**Pokaż meteogram** otwiera osobne, pełne okno aplikacji dla wybranej lokalizacji. Ikona panelu
odzwierciedla pogodę; przy braku danych używana jest własna ikona Meteo.
Samo `kpackagetool6 -i integrations/plasma/package` nie instaluje biblioteki C++;
potrzebna jest także instalacja modułu `pl.meteo.desktop` przez CMake.
Ścieżkę modułu można ustawić przez `METEO_QML_INSTALL_DIR`, np. dla dystrybucji
używających `lib64` zamiast `lib`.

Opcje `METEO_BUILD_APP` i `METEO_BUILD_PLASMOID` wyłączają odpowiednie frontendy.
`BUILD_TESTING=OFF` usuwa zależność od Qt Test.

## Paczki i CI

Paczki dla Arch Linux, Debiana 13, Ubuntu 26.04 i Fedory 44 można budować
w kontenerach: `python3 scripts/build-package.py --distro debian --test`.
Wyniki zawierają źródła, sumy SHA-256 i raport budowy.
GitHub Actions uruchamia testy, porównuje wygląd CPU/GPU i sprawdza instalację
paczek. Szczegóły: [budowanie paczek i CI](packaging.md).

## Architektura i źródła

Podział na wspólny backend i opcjonalną integrację KDE nawiązuje do lokalnego
projektu `../btrfs-backup`. Szczegóły: [docs/architecture.md](architecture.md).

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

Paczki Windows, macOS i Android: [instrukcja budowania z Qt/KDE](portable-packages.md).
Testy porównania wyglądu CPU/GPU: [zakres i uruchamianie](renderer-tests.md).
