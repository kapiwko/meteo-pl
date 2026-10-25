# Architektura

Wspólny backend C++ obsługuje aplikację okienkową i plasmoid. Inspiracją jest
podział odpowiedzialności z `btrfs-backup`, zwłaszcza wspólne modele prezentacji
i osobny pakiet integracji KDE. Pogoda nie wymaga procesu systemowego,
uprawnień roota ani pośrednictwa D-Bus.

```mermaid
flowchart BT
    API[Interfejs prognoz meteo.pl] --> Network[MeteoClient · Qt Network]
    Network --> Core[Forecast · walidacja i szeregi czasowe]
    Core --> Model[WeatherModel · stan i cache]
    Model --> QML[pl.meteo.desktop · wspólne wykresy QML]
    QML --> App[Aplikacja Kirigami]
    QML --> Plasma[Plasmoid Plasma 6]
```

| Katalog | Odpowiedzialność |
| --- | --- |
| `src/core` | Dokument prognozy, walidacja współrzędnych i obszaru modelu, wybór najnowszego uruchomienia |
| `src/network` | Asynchroniczne HTTP, timeout, limity, anulowanie i cache obrazów |
| `src/presentation` | Modele prognoz, wyszukiwarki, map i treści redakcyjnych; jednostki i preferencje |
| `src/qml` | Wykresy z inspektorem, edytor lokalizacji, mapy, prognoza godzinowa i dzienna |
| `apps/meteo` | Okno Kirigami, preferencje użytkownika i wpis desktop |
| `integrations/plasma` | Pakiet Plasma 6, konfiguracja instancji i kompaktowa reprezentacja |
| `tests` | Parser, lokalny serwer HTTP, cache, renderowanie QML i start aplikacji |

## Przepływ danych

1. Model sprawdza cache przypisany do modelu i dokładnych współrzędnych.
2. Dla cache młodszego niż godzina, obejmującego bieżącą chwilę, nie powtarza
   automatycznego pobrania. Ręczne odświeżenie pomija ten warunek.
3. `GET /meteorograms/available` zwraca daty uruchomień. Adapter wybiera maksimum
   lub żądany start archiwalny, po sprawdzeniu jego dostępności.
4. `POST /meteorograms/{model}` z `point.lat`, `point.lon` i `date` zwraca prognozę.
5. Parser sprawdza dane i osobne osie czasu wszystkich szeregów. Braki danych
   zachowuje jako `null`; nie zamienia ich w zero.
6. `WeatherModel` przelicza ciśnienie z Pa na hPa i zachmurzenie z ułamka na %,
   wiatr na wybraną jednostkę i widzialność na km. Podstawy chmur sprawdza według
   jednostki źródłowej (serwis zwraca też km).
   Opad jest podawany w mm dla przedziału prognozy. Nie zakłada godzinnego
   przedziału dla modeli o innej rozdzielczości czasowej.
7. Dopiero poprawna odpowiedź zastępuje stan i cache przez `QSaveFile`.

Zmiana lokalizacji anuluje poprzednie żądanie i usuwa poprzednie podsumowanie,
zanim rozpocznie się nowe pobranie. Licznik generacji odrzuca spóźnione odpowiedzi.
Jedna instancja `WeatherModel` obsługuje wszystkie wykresy danego widoku.

`ForecastFields` definiuje serie, palety i katalog wykresów. `UserPreferences`
przechowuje nazwane profile z uporządkowaną listą wykresów i wybranymi seriami.
`ForecastPanel` składa widok według profilu; kolejność malowania danych w
`Meteogram` jest od niego niezależna. Tła i zakresy min–max poprzedzają linie
i znaczniki, mgła jest ponad pokryciem chmur, siatka ponad wypełnieniami,
a odczyt kursora na wierzchu. Ukrycie serii przez legendę nie przelicza osi.
Tło temperatur poniżej zera jest ograniczone położeniem 0°C, także przy
całkowicie ujemnym lub dodatnim zakresie.

`LegacyMeteogramModel` przekazuje współrzędne i start modelu do selektora
`old.meteo.pl`, sprawdza przekierowanie i buduje adres oryginalnego obrazu UM.
Indeksy starej siatki nie są wyliczane z indeksów nowego API. `OriginalMeteogram`
wyświetla obraz przez wspólny cache i umożliwia otwarcie legendy. Pola opadu
konwekcyjnego nie ma w obecnym dokumencie liczbowym; jego występowanie jest
dostępne w oznaczeniach oryginalnego obrazu, bez heurystyk w adapterze.

Cache prognoz jest wspólny dla procesów i zawiera model, współrzędne, żądany
start, czas pobrania oraz oryginalny dokument. Klucz to SHA-256 modelu
i współrzędnych, z osobnym sufiksem dla archiwum.
Odczyt sprawdza również zawartość nagłówka i dokumentu. Awaria cache nie blokuje
pobrania ani wyświetlenia prognozy. Instancje uruchomione równocześnie mogą
wykonać osobne żądania; ta wersja nie współdzieli trwających pobrań między procesami.

## Mapy i wyszukiwanie

`PlaceSearchModel` odpytuje NOM dla Polski i Photon dla świata, z opóźnieniem
350 ms i odrzucaniem spóźnionych wyników. `Locations` waliduje obydwa formaty.
`UserPreferences` zapisuje ulubione, ostatnie miejsca i ustawienia wykresów.

`WeatherLayersModel` pobiera opublikowane starty z `/json/UM4_min_max.json`
lub `/json/GFS_min_max.json`, a następnie tworzy żądania WMS w EPSG:3857.
`TileMap` zamraża adresy i współrzędne kafelków dla każdej klatki.
`MapTileFrame` przechowuje poprzednią kompletną klatkę do przygotowania nowej;
położenie starej nadal wynika z bieżącego środka i skali mapy. Odrzucone lub
niekompletne klatki nie zastępują widocznej. `BufferedImage` zapewnia taką samą
podmianę pojedynczych map tematycznych. Zegar animacji pracuje tylko przy
gotowej klatce, a podpis czasu dotyczy obrazu widocznego na ekranie.

Singleton QML `MapImages` rejestruje asynchronicznego dostawcę obrazów w każdym
silniku QML. `ImageCache` współdzieli trwające pobrania i cache LRU obrazów
(96 MiB) w procesie. `QNetworkDiskCache` ma limit 256 MiB i osobny katalog
dla aplikacji oraz Plasma. Odpowiedzi mają deadline 25 s, limit 8 MiB i limit
16 megapikseli; dekodowanie odbywa się w puli dwóch wątków. Prefetch kolejnej
klatki używa tego samego magazynu i nie zmienia kroku aktualnie wyświetlanego.

`EditorialModel` pobiera komentarz synoptyka (także według daty) i stronicowane
artykuły. Daty przyszłe nie powodują żądania; HTTP 404 dla komentarza jest
prezentowane jako brak publikacji. Adaptery są oddzielone od QML, a ich adresy
można wstrzyknąć w testach.

## Weryfikacja

`ctest --preset dev` nie potrzebuje dostępu do serwisu meteo.pl. Testy HTTP
korzystają z lokalnego serwera TCP; testy interfejsu uruchamiają Qt w trybie
`offscreen`, z syntetyczną prognozą. Test aplikacji używa `--smoke-test`, który
wyłącza pobieranie. Test renderowania obejmuje wspólny panel używany przez obydwa
frontendy. Jeśli dostępny jest `plasmawindowed`, zestaw zawiera także test
uruchomienia pakietu plasmoidu. Zachowanie widgetu w panelu i zasobniku wymaga
osobnego sprawdzenia w sesji Plasma.

Testy map używają lokalnego serwera PNG z kontrolowanym opóźnieniem. Sprawdzają
zachowanie starej klatki do załadowania wszystkich kafelków, błędy i spóźnione
odpowiedzi, cache po restarcie bez serwera, przeliczanie współrzędnych w obu
półkulach oraz rzeczywiste przeciąganie i zoom pod kursorem.
Testy renderowania sprawdzają także stykanie się słupków opadu, widoczność mgły
nad chmurami i tło poniżej 0°C. Testy profili obejmują wybór serii, kolejność,
zapis i odczyt; selektor oryginalnego meteogramu jest testowany lokalnie.
