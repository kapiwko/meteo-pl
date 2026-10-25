# Paczki i GitHub Actions

Aplikacja i plasmoid są pakowane razem jako `meteo-pl`. W środku znajduje się
program, moduł C++/QML, plasmoid, ikona, wpis menu i obsługa powiadomień KDE.
Paczki używają bibliotek Qt/KDE danej dystrybucji. Nie zawierają danych prognoz
ani ustawień użytkownika. Do użycia widgetu potrzebna jest Plasma 6;
aplikacja okienkowa może działać również w innych środowiskach.

| System budowania i docelowy | Paczka |
| --- | --- |
| Arch Linux / zgodne aktualne dystrybucje | `.pkg.tar.zst` |
| Debian 13 (trixie) | `.deb` |
| Ubuntu 26.04 LTS | `.deb` |
| Fedora 44 | `.rpm` |
| Źródła | `.tar.gz`, `-source.zip` |

Każda paczka binarna powstaje w odpowiedniej dystrybucji, z jej wersją Qt,
KDE, glibc i kompilatora. Paczki Debiana i Ubuntu są oddzielnymi artefaktami.
Paczki Linuksa korzystają z bibliotek systemowych. Budowanie Windows, macOS
i Androida opisano w [osobnej instrukcji](portable-packages.md).
Ubuntu 24.04 i Debian 12 nie mają w domyślnych repozytoriach wymaganej kombinacji
Qt i KDE Frameworks 6.

## Budowanie lokalnie

Wymagania hosta dla kontenerów: Python ≥ 3.11 i działający Docker.
Kompilacja nie instaluje aplikacji w systemie hosta.

```sh
python3 scripts/build-package.py --distro arch --test
python3 scripts/build-package.py --distro debian --test
python3 scripts/build-package.py --distro ubuntu --test
python3 scripts/build-package.py --distro fedora --test
```

Wyniki są w `dist/<system>/`, a wielokrotnie używane katalogi budowania
w `build/packages/<system>/`. Domyślnie używane są dwa zadania kompilacji;
`--jobs 4` zmienia ten limit. `--test` uruchamia qmllint, testy CTest oraz
porównania CPU/GPU przy pomocy Mesa i Xvfb. Brak `--test` pomija testy.

Na samym docelowym systemie, po zainstalowaniu zależności z odpowiedniego
Dockerfile, można budować bez kontenera:

```sh
python3 scripts/release.py --target native --test
python3 scripts/release.py --target source
python3 scripts/release.py --target tar-install
```

Można też podać `--target arch`, `deb` albo `rpm`. Narzędzie odrzuca
budowanie natywnego formatu na obcej dystrybucji. `tar-install` to archiwum
drzewa instalacyjnego z zależnościami systemowymi, a nie przenośny AppImage.
`--build-dir` i `--dist-dir` wskazują inne katalogi wynikowe.

W każdym zestawie są źródła, `SHA256SUMS` i `BUILD-REPORT.json`, który zapisuje
wersję, system, architekturę, wynik testów i sumy artefaktów. Sprawdzenie:

```sh
cd dist/debian
sha256sum --check SHA256SUMS
```

Źródła są archiwizowane ze stałą kolejnością i znacznikami czasu z
`SOURCE_DATE_EPOCH` (domyślnie data ostatniego commitu). Wykluczone są katalogi
budowania, `.git`, dane cache i pliki IDE. `VERSION` określa wersję aplikacji,
widoku informacji, instalowanego plasmoidu i paczek. Binarne paczki zależą też
od wersji kompilatora i bibliotek w danym obrazie kontenera.
Repozytorium nie deklaruje obecnie licencji; metadane paczek oznaczają ten stan
jako `LicenseRef-Unspecified`, bez przypisywania domyślnej licencji.

## GitHub

Workflow `.github/workflows/tests.yml` działa na push, pull request i ręczne
uruchomienie. Cztery niezależne zadania budują aplikację, widget i testy,
uruchamiają qmllint i wszystkie testy, budują paczkę, instalują ją wewnątrz
kontenera i sprawdzają zainstalowany program oraz widget bez ścieżek do
bibliotek z katalogu budowania. Starsze wersje Plasma 6 bez `--smoke-test`
są testowane przez krótkie uruchomienie `plasmawindowed`.

W zakładce **Actions → wybrane uruchomienie → Artifacts** dostępne są paczki
z sumami SHA-256, wyniki JUnit, logi CTest oraz obrazy CPU, GPU i ich różnice.
Błąd testów blokuje udostępnienie paczki z tego zadania. Testy renderera wymagają
RHI/OpenGL; przejście na zapasowy renderer CPU oznacza błąd testu.

Akcje są przypięte do konkretnych commitów, a Dependabot zgłasza aktualizacje.
Workflow ma tylko prawo odczytu repozytorium i nie publikuje automatycznie
wydań ani paczek do zewnętrznych repozytoriów.
