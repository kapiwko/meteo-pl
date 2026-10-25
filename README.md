# Meteo Desktop


Backend C++23 / Qt Network pobiera uruchomienia UM4 i GFS oraz waliduje dokumenty prognozy.

Budowa: `cmake -S . -B build/dev -G Ninja && cmake --build build/dev`. Testy: `ctest --test-dir build/dev`.

Aplikacja Kirigami pokazuje meteogram, wybór lokalizacji i cache prognozy.
Uruchomienie: `cmake --preset dev && cmake --build --preset dev`, następnie `./build/dev/apps/meteo/meteo-pl`.
