# PROJECT: Mars Rover Mission Control

**Készítette:** Rikovszky Regina (Kernel Overloards)
**Iskola Neve:** Váci SZC Boronkay György Műszaki Technikum és Gimnázium
**Felkészítő Tanár:** Baksza Dávid 
**E-mail:** rikovszkyregina@gmail.com

## FONTOS

A csapat többi tagja úgy döntött, hogy nem folytatja a részvételt a versenyen, ezért a projektet végül egyedül készítettem el.
Kérem a tisztelt zsűri megértését a változásokkal kapcsolatban.

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

> a launchert mindig a projekt gyökérkönyvtárából (`KernelOverloards/`) futtasd, ne a `src/` mappából

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
| `CMakeLists.txt not found` | Nem a projekt gyökérből futtatod a launcher-t |
| `mars_map_50x50.csv nem olvasható` | A `data/` mappa hiányzik vagy nincs benne a CSV |
| `log fajl nem nyithato meg` | Az `output/` mappa nem létezik – hozd létre kézzel |
| GUI nem indul el | Ellenőrizd, hogy a `build-mingw/gui.exe` létezik-e |
| Régi CMakeCache hiba | Töröld a `build-mingw/CMakeCache.txt`-t és a `CMakeFiles/` mappát, majd fordíts újra |

