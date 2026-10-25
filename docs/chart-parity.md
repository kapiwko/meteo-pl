# Wykresy Meteo ICM

Porównanie wykonano 25.10.2026 z aktualnym
[klientem webowym](https://app.meteo.pl/meteogram) oraz jego publicznym kodem
[main.dart.js](https://app.meteo.pl/main.dart.js). Sprawdzono także rzeczywiste
odpowiedzi dla Kielc w UM4 60 h, UM4 120 h i GFS 240 h.
W repozytorium testy wykorzystują wyłącznie dane syntetyczne.

| Wykres domyślny | Serie i sposób rysowania | Osie i oznaczenia |
| --- | --- | --- |
| Temperatura | Czerwona temperatura powietrza, granatowa odczuwalna, fioletowe punkty rosy; półprzezroczyste zakresy min–max; brązowe pionowe kreski min–max powierzchni gruntu | °C, zapas 1°C względem ekstremów, błękitne tło poniżej zera |
| Opady i wilgotność | Stykające się słupki średniej, cienkie szpilki maksimum, ciągła linia wilgotności; zielony deszcz, niebieski śnieg, fioletowy deszcz ze śniegiem | mm w przedziale prognozy; skala 5/10/20/30 mm; osobna dopasowana oś wilgotności; czerwona kropka dla maksimum powyżej skali |
| Szansa opadu i burze | Zielona linia prawdopodobieństwa, znaczniki możliwości burzy i wyładowań w środku przedziału | 0–100%; pierścień dla możliwości burzy; żółty, pomarańczowy i czerwony dla odpowiednio dodatniej liczby, ≥10 i ≥30 wyładowań |
| Ciśnienie | Czarna linia z kolorowym wypełnieniem według tabeli ICM | hPa i mm Hg, zakres co najmniej 15 hPa oraz marginesy |
| Prędkość i porywy wiatru | Granatowa linia prędkości z paletą ICM; czerwone poziome kreski porywów obejmujące przedział | m/s i km/h; progi skali 5/10/15/20/25/30/40/50 m/s, ograniczenie wpływu porywu do czterokrotności maksymalnej prędkości; kropka przy przekroczeniu skali |
| Kierunek wiatru | Strzałki wskazujące ruch powietrza, obliczone z kierunku, z którego wieje | Wspólna oś czasu |
| Pionowy zasięg chmur i widzialność | Punkty podstaw od pełnego pokrycia do rzadkich chmur, na nich brązowe wierzchołki i pomarańczowa linia widzialności | Nieliniowe poziomy 0/0,5/2/7/15 km oraz 0/1/5/20/100 km dla widzialności |
| Zachmurzenie i mgła | Kolejno wypełnienia chmur wysokich, średnich, niskich i bardzo niskich, linie pokrycia, pomarańczowa linia całkowitego zachmurzenia, żółte kreski mgły | 0–8 oktantów; mgła jako frakcja 0–1 |

Palety linii, obszarów i wypełnień mają wariant jasny i ciemny zgodny z klientem
webowym. Czarne punkty podstaw pełnego zachmurzenia pozostają czarne także
w ciemnym motywie. Siatka i ramka są ponad wypełnieniami; noc nakłada wspólne
cieniowanie, a wskaźniki kursora pozostają na wierzchu.

Te osiem wykresów to definicje domyślnego profilu. Własne wykresy korzystają
z tego samego formatu: serie, rodzaj rysowania, kolory, krycie, linie, punkty
i przypisanie do dwóch osi są zapisane w profilu. Można łączyć różne rodzaje
danych; granice osi mogą być automatyczne lub własne. Wyłączenie serii w
legendzie utrzymuje zakres osi. Etykiety czasu dostosowują gęstość do długości
wyświetlanego przedziału i szerokości wykresu. Przy małej szerokości pomijają
minuty, a potem zwiększają odstęp; daty również nie nachodzą na siebie.
Daty są wyśrodkowane pomiędzy liniami północy, przedłużonymi nad wykres;
podział uwzględnia strefę czasu i zmianę czasu letniego na zimowy.
Jednostki są umieszczone przy odpowiednich osiach.
Siatka zachowuje odstęp trzech godzin, lecz ma delikatniejsze kreski i ramkę
zgodnie z ustawieniem wyglądu tej aplikacji.

Maksimum opadu opisuje maksimum w obszarze. Przekroczenie skali nie jest
wskaźnikiem konwekcji. Objaśnienia źródłowe:
[Jak czytać meteogramy](https://www.meteo.pl/jak-czytac-prognoze-pogody-czesc-i-meteogramy),
[Opad lokalny i konwekcja](https://www.meteo.pl/gdzie-ten-deszcz-czyli-dlaczego-prognoza-pogody-sie-nie-sprawdzila).
