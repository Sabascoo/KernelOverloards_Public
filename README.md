# PROJECT: Mars Rover Mission Control

**Készítette:** Rikovszky Regina (Kernel Overloards)  
**Iskola neve:** Váci SZC Boronkay György Műszaki Technikum és Gimnázium  
**Felkészítő tanár:** Baksza Dávid  
**E-mail:** rikovszkyregina@gmail.com

---

## FONTOS

A csapat többi tagja úgy döntött, hogy nem folytatja a részvételt a versenyen, ezért a projektet végül egyedül készítettem el.
Kérem a tisztelt zsűri megértését a változásokkal kapcsolatban.

- Kérem, hogy a programot a GitHubról, ZIP fájlba csomagolva töltsék le. A csomag tartalmazza a videófelvételt, a README fájlt, valamint az algoritmust bemutató PDF dokumentumot.
- Letöltés és kicsomagolás után nevezzék át a mappát a neve legyen: **KernelOverloards**

---

## Programfejlesztői környezet

- Visual Studio Code 1.112.0

---

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

---

## Fordítás és futtatás

### Windows

```powershell
cd C:\...\KernelOverloards
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --config Release
```

Ezután az alkalmazás a projekt gyökérkönyvtárában lévő `launcher.exe`-vel indítható:

```powershell
.\launcher.exe
```

> **Fontos:** a launchert mindig a projekt gyökérkönyvtárából (`KernelOverloards/`) futtassák,
> ne a `src/` vagy `build-mingw/` mappából.

### Linux

```bash
cd ~/Documents/KernelOverloards
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./launcher
```

---

## A program használata

1. Indítsák el a `launcher`-t a fenti paranccsal.
2. A launcher automatikusan elindítja a szimulációt (`main_logic`).
3. A program bekéri a **küldetés hosszát órában** (minimum 24):
   ```
   Kuldes hossza (ora, >=24): 48
   ```
4. A szimuláció lefut, és létrehozza a következő fájlokat:
   - `output/rover_log.csv`
   - `output/ai_route.txt`
5. A launcher ezután automatikusan megnyitja a **GUI**-t, amely visszajátssza az AI útvonalát.

---

## Kimenetek

| Fájl | Tartalom |
|---|---|
| `output/rover_log.csv` | Minden kör adatai (pozíció, akku, ásványok, műveletek) |
| `output/ai_route.txt` | Ugyanez, a GUI által olvasott formátumban |

---

## Gyakori hibák és megoldásuk

### Windows

| Hiba | Megoldás |
|---|---|
| `CMakeLists.txt not found` | Nem a projekt gyökérkönyvtárából futtatják a CMake-et |
| `mars_map_50x50.csv nem olvasható` | A `data/` mappa hiányzik, vagy nincs benne a CSV fájl |
| `log fajl nem nyithato meg` | Az `output/` mappa nem létezik — hozzák létre kézzel |
| GUI nem indul el | Ellenőrizzék, hogy a `build-mingw/gui.exe` létezik-e |
| `Permission denied` a `launcher.exe`-nél | A launchert a `build-mingw/` mappából futtatják — mindig a projekt gyökérkönyvtárából kell indítani: `.\launcher.exe` |
| Régi CMakeCache hiba (mappa átnevezés után) | Lásd az alábbi parancsokat |

Ha a CMakeCache hibát kapnak (pl. mappa átnevezés vagy áthelyezés után):

```powershell
del build-mingw\CMakeCache.txt
rmdir build-mingw\CMakeFiles
(... If you continue, all children will be removed with the item. Are you sure you want to continue?
[Y] Yes  [A] Yes to All  [N] No  [L] No to All  [S] Suspend  [?] Help (default is "Y"): y)
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --config Release
```

---

### Linux

| Hiba | Megoldás |
|---|---|
| `Cannot find source file: imgui/imgui.cpp` | Az `imgui/` mappából hiányoznak a `.cpp` forrásfájlok — töltsék le a teljes repót újra |
| `Could not find glfw3` | A GLFW nincs telepítve: `sudo apt install libglfw3-dev` |
| `Could not find OpenGL` | Hiányzó OpenGL csomag: `sudo apt install libgl-dev` |
| `Permission denied: ./launcher` | Adjanak futtatási jogot: `chmod +x ./launcher` |
| `mars_map_50x50.csv nem olvasható` | A `data/` mappa hiányzik, vagy nincs benne a CSV fájl |
| `log fajl nem nyithato meg` | Az `output/` mappa nem létezik — hozzák létre: `mkdir -p output` |
| GUI nem indul el | Ellenőrizzék, hogy a `build/gui` létezik-e, és van-e futtatási joga: `chmod +x build/gui` |
| Régi CMakeCache hiba (mappa átnevezés után) | Lásd az alábbi parancsokat |

Ha a CMakeCache hibát kapnak:

```bash
rm -rf build/CMakeCache.txt build/CMakeFiles
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
