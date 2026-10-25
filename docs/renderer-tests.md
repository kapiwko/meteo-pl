# Porównanie wyglądu CPU i GPU

Test `renderer_parity` rysuje ten sam meteogram przez Canvas CPU i natywny
renderer GPU, a następnie porównuje obrazy całego komponentu: serie, wypełnienia,
kolejność warstw, osie, podpisy, legendę i kursor odczytu.

Sprawdzane są wszystkie 12 domyślnych typów wykresów, każdy w trzech wariantach:

- szeroki widok całej prognozy, również używany przy eksporcie;
- wąski widok 24 godzin z kursorem odczytu;
- szeroki widok z rzeczywiście włączoną ciemną paletą.

To 36 par obrazów. Lokalne dane obejmują m.in. temperatury ujemne, zakresy
min–max, opad powyżej skali i różne jego rodzaje, zmienne kierunki wiatru,
gęste punkty podstaw chmur, mgłę, burze oraz brakujące próbki.
Testy nie pobierają prognoz z internetu i nie zmieniają ustawień użytkownika.

## Uruchomienie

Po zbudowaniu projektu:

```sh
METEO_RENDER_ARTIFACTS="$PWD/build/renderer-diffs" \
    ctest --test-dir build/dev -R renderer_parity --output-on-failure
```

Na maszynie bez ekranu potrzebne są Mesa i Xvfb:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run --auto-servernum dbus-run-session -- \
    ctest --test-dir build/dev -R renderer_parity --output-on-failure
```

Mesa może wykonywać OpenGL programowo, ale test nadal wymaga natywnej ścieżki
RHI/GPU aplikacji. Zapasowy Canvas CPU nie jest akceptowany jako renderer GPU.
CI uruchamia porównanie niezależnie na czterech dystrybucjach; zapisuje obrazy
CPU, GPU i różnic jako artefakty.

## Kryterium zgodności

Obrazy muszą mieć identyczne wymiary. Porównanie dopuszcza niewielkie różnice
wygładzania krawędzi między Canvas, sterownikami i Qt:

- średnia różnica kanałów koloru poniżej 1,5% pełnego zakresu;
- mniej niż 1,2% niedopasowanych pikseli przy poszukiwaniu krawędzi w promieniu
  jednego piksela i tolerancji kanału 40/255;
- mniej niż 2,5% niedopasowanych kolorowych pikseli albo maksymalnie 64 takie
  piksele w całym obrazie, dla drobnych krawędzi rzadkich symboli.

Przy wygładzanych kolorowych krawędziach porównywany jest również odcień,
aby sama różnica pokrycia piksela nie oznaczała innego koloru serii.
Test kontrolny sprawdza, że zniknięcie geometrii i zamiana czerwieni na niebieski
są odrzucane. Osobny test prezentacji wymaga widocznych strzałek wiatru.
Progi nie oznaczają zgodności obrazów bit po bicie.

Przy błędzie obrazy trafiają domyślnie do `/tmp/meteo-renderer-diffs`.
`METEO_RENDER_ARTIFACTS` ustawia katalog i zapisuje obrazy także dla poprawnych
porównań. Warto obejrzeć wszystkie trzy obrazy przed zmianą progów.
