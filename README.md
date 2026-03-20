# PROJECT: Mars Rover Mission Control

**Készítette:** Rikovszky Regina (Kernel Overloards)

**Iskola Neve:** Váci SZC Boronkay György Műszaki Technikum és Gimnázium

**Felkészítő Tanár:** Baksza Dávid 

**E-mail:** rikovszkyregina@gmail.com

## FONTOS

A csapat többi tagja úgy döntött, hogy nem folytatja a részvételt a versenyen, ezért a projektet végül egyedül készítettem el.
Kérem a tisztelt zsűri megértését a változásokkal kapcsolatban.

- (kérem, hogy a programot a GitHubról, ZIP fájlba csomagolva töltsék le. A csomag tartalmazza a videófelvételt, a README fájlt, valamint az algoritmust bemutató PDF dokumentumot)

## Programfejlesztői környezet

- Visual Studio Code 1.112

## Követelmények

### Windows
- [MinGW-w64](https://www.mingw-w64.org/) (gcc, g++, mingw32-make)
- [CMake](https://cmake.org/download/) (3.16+)

### Linux
- `gcc`, `g++`, `cmake`, `make`
- `libglfw3-dev`, `libgl-dev`

```bash
sudo apt install build-essential cmake libglfw3-dev libgl-dev
```

## Fordítás és futtatás

### Windows

```powershell
cd C:\...\KernelOverloards
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --config Release
```

Ezután az alkalmazás a `launcher.exe`-vel indítható:

```powershell
.\build-mingw\launcher.exe
```

> a launchert mindig a projekt gyökérkönyvtárából (`KernelOverloards/`) futtassák, ne a `src/` mappából

### Linux

```bash
cd .../KernelOverloards
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/launcher
```

## Kimenetek

| Fájl | Tartalom |
|---|---|
| `output/rover_log.csv` | Minden kör adatai (pozíció, akku, ásványok, műveletek) |
| `output/ai_route.txt` | Ugyanez, a GUI által olvasott formátumban |

---

## Gyakori hibák

| Hiba | Megoldás |
|---|---|
| `CMakeLists.txt not found` | Nem a projekt gyökérből futtatják a launchert |
| `mars_map_50x50.csv nem olvasható` | A `data/` mappa hiányzik vagy nincs benne a CSV |
| `log fajl nem nyithato meg` | Az `output/` mappa nem létezik –> hozzák létre kézzel |
| GUI nem indul el | Ellenőrizzék, hogy a `build-mingw/gui.exe` létezik-e |
| Régi CMakeCache hiba | Töröljék a `build-mingw/CMakeCache.txt`-t és a `CMakeFiles/` mappát, majd fordítsák újra |

## Ellenőrzés

- Helyesen letöltött mappa szerkezete így néz ki:

Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
d-----        18/03/2026     14:34                .vscode
d-----        18/03/2026     18:25                build-mingw
d-----        18/03/2026     14:46                data
d-----        18/03/2026     14:36                glfw
d-----        18/03/2026     14:34                glfw-3.4.bin.WIN64
d-----        18/03/2026     17:30                imgui
d-----        20/03/2026     12:19                KernelOverloards_Public
d-----        18/03/2026     14:37                New folder
d-----        18/03/2026     18:09                output
d-----        18/03/2026     18:06                src
-a----        18/03/2026     17:31           1963 CMakeLists.txt
-a----        18/03/2026     18:31            153 imgui.ini
-a----        20/03/2026     12:00           1824 README.md
