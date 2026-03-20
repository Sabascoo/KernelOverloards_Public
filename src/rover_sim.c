// rover_sim.c  (C11)
// gcc -O2 -std=c11 rover_sim.c -o rover_sim
// Beolvassa:  mars_map_50x50.csv
// Kiírja:     rover_log.csv, ai_route.txt

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define TERKEP_MERET 50
#define W TERKEP_MERET
#define H TERKEP_MERET

#define UTES_PER_ORA   2
#define NAP_UTES       32
#define ESZAKA_UTES    16
#define CIKLUS_UTES    (NAP_UTES + ESZAKA_UTES)

#define MAX_AKKU  100.0
#define K_ALLANDO 2.0
#define MAX_UT    (W * H)
#define NAGY_INT  99999

typedef struct { int8_t x, y; } Pont;

typedef struct {
    int8_t x, y;
    double akku;
    int zold, sarga, kek;
    int utes;
    int mozgott_cellak;
} Rover;

typedef struct {
    int osszes_utes;
    int8_t sx, sy;
} Kuldes;

typedef struct {
    double g, f;
    int8_t px, py;
    bool nyitott, zarolt;
} Csucs;

typedef struct {
    bool ok;
    int veg_utes;
    double veg_akku;
} TervEredmeny;

static uint8_t terkep[H][W];
static char    terkep_kar[H][W];
static uint8_t haz_tav[H][W];

static inline int ervenyesE(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

static inline double rogzit(double e, double al, double fl) {
    if (e < al) return al;
    if (e > fl) return fl;
    return e;
}

static inline double mozgasKoltseg(int v) { return K_ALLANDO * (double)(v * v); }
static inline double napiToltes(bool nap)  { return nap ? 10.0 : 0.0; }

static inline bool mozoghat(double akku, int v)  { return akku >= mozgasKoltseg(v); }
static inline bool banyaszhat(double akku)        { return akku >= 2.0; }

static double mozogFunc(double akku, bool nap, int v) {
    return rogzit(akku - mozgasKoltseg(v) + napiToltes(nap), 0.0, MAX_AKKU);
}

static double banyaszFunc(double akku, bool nap) {
    return rogzit(akku - 2.0 + napiToltes(nap), 0.0, MAX_AKKU);
}

static double allFunc(double akku, bool nap) {
    if (nap) return rogzit(akku - 1.0 + 10.0, 0.0, MAX_AKKU);
    if (akku >= 1.0) return rogzit(akku - 1.0, 0.0, MAX_AKKU);
    return 0.0;
}

static inline bool nappal_van(int utes) {
    return (utes % CIKLUS_UTES) < NAP_UTES;
}

static inline float napCiklus(int utes) {
    float t = 6.0f + 0.5f * (float)utes;
    while (t >= 24.0f) t -= 24.0f;
    return t;
}

static double chebyshev(int x1, int y1, int x2, int y2) {
    int dx = abs(x1 - x2), dy = abs(y1 - y2);
    return (double)(dx > dy ? dx : dy);
}

static int minUtes(int lepesek, int v) {
    if (lepesek <= 0) return 0;
    return (lepesek + v - 1) / v;
}

static int teljesUtUtes(int oda, bool banyasz, int vissza) {
    return minUtes(oda, 3) + (banyasz ? 1 : 0) + minUtes(vissza, 3);
}

static void naploz(FILE *f, int kor, const Rover *r, int v, int megtett,
                   const char *ido, float ora, const char *muvelet) {
    fprintf(f, "%d,%d,%d,%.2f,%d,%d,%d,%d,%d,%d,%s,%.2f,%s\n",
            kor, (int)r->x, (int)r->y, (float)r->akku,
            v, megtett, r->zold + r->sarga + r->kek,
            r->zold, r->sarga, r->kek,
            ido, ora, muvelet);
}

static void hazaTavBFS(int8_t bx, int8_t by) {
    memset(haz_tav, 255, sizeof(haz_tav));
    static int16_t qx[MAX_UT], qy[MAX_UT];
    int eleje = 0, vege = 0;

    haz_tav[by][bx] = 0;
    qx[vege] = bx; qy[vege] = by; vege++;

    static const int dx[8] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static const int dy[8] = {-1, 0, 1,-1, 1,-1, 0, 1};

    while (eleje < vege) {
        int cx = qx[eleje], cy = qy[eleje]; eleje++;
        uint8_t d = haz_tav[cy][cx];
        for (int i = 0; i < 8; i++) {
            int nx = cx + dx[i], ny = cy + dy[i];
            if (!ervenyesE(nx, ny) || terkep[ny][nx] == 1 || haz_tav[ny][nx] != 255) continue;
            haz_tav[ny][nx] = d + 1;
            qx[vege] = nx; qy[vege] = ny; vege++;
        }
    }
}

static int utvonalVisszaepites(Csucs cs[H][W], int sx, int sy, int cx, int cy, Pont *ut, int max) {
    int n = 0, ax = cx, ay = cy;
    while (!(ax == sx && ay == sy)) {
        if (n >= max) return -1;
        ut[n++] = (Pont){(int8_t)ax, (int8_t)ay};
        int px = cs[ay][ax].px, py = cs[ay][ax].py;
        if (px < 0 || py < 0) return -1;
        ax = px; ay = py;
    }
    if (n >= max) return -1;
    ut[n++] = (Pont){(int8_t)sx, (int8_t)sy};
    for (int i = 0; i < n / 2; i++) {
        Pont tmp = ut[i]; ut[i] = ut[n-1-i]; ut[n-1-i] = tmp;
    }
    return n;
}

static int astar(int sx, int sy, int cx, int cy, Pont *ut, int max) {
    Csucs cs[H][W];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            cs[y][x].g = cs[y][x].f = 1e100;
            cs[y][x].px = cs[y][x].py = -1;
            cs[y][x].nyitott = cs[y][x].zarolt = false;
        }

    cs[sy][sx].g = 0.0;
    cs[sy][sx].f = chebyshev(sx, sy, cx, cy);
    cs[sy][sx].nyitott = true;

    static const int dx[8] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static const int dy[8] = {-1, 0, 1,-1, 1,-1, 0, 1};

    for (;;) {
        int bx = -1, by = -1;
        double bf = 1e100;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (cs[y][x].nyitott && !cs[y][x].zarolt && cs[y][x].f < bf) {
                    bf = cs[y][x].f; bx = x; by = y;
                }

        if (bx < 0) return -1;
        if (bx == cx && by == cy) return utvonalVisszaepites(cs, sx, sy, cx, cy, ut, max);

        cs[by][bx].zarolt = true;
        for (int i = 0; i < 8; i++) {
            int nx = bx + dx[i], ny = by + dy[i];
            if (!ervenyesE(nx, ny) || terkep[ny][nx] == 1) continue;
            double ug = cs[by][bx].g + 1.0;
            if (ug < cs[ny][nx].g) {
                cs[ny][nx].g = ug;
                cs[ny][nx].f = ug + chebyshev(nx, ny, cx, cy);
                cs[ny][nx].px = (int8_t)bx;
                cs[ny][nx].py = (int8_t)by;
                cs[ny][nx].nyitott = true;
            }
        }
    }
}

static TervEredmeny tervSzimulaciо(int ind_utes, double ind_akku, int oda, bool banyasz, int vissza, int max_utes) {
    int t = ind_utes;
    double a = ind_akku;

    while (t < max_utes) {
        if (oda == 0 && !banyasz && vissza == 0)
            return (TervEredmeny){true, t, a};

        bool nap = nappal_van(t);
        int hatra = max_utes - (t + 1);
        bool talalt = false;
        double legjobb_a = -1e100;
        int legjobb_h = -1;

        if (nap || a >= 1.0 || a <= 0.0) {
            double ua = allFunc(a, nap);
            if (teljesUtUtes(oda, banyasz, vissza) <= hatra) {
                talalt = true; legjobb_a = ua; legjobb_h = 0;
            }
        }

        if (oda > 0 || (!banyasz && vissza > 0)) {
            int szakasz = oda > 0 ? oda : vissza;
            for (int v = 1; v <= 3; v++) {
                if (v > szakasz || !mozoghat(a, v)) continue;
                int uo = oda > 0 ? oda - v : oda;
                int uv = oda > 0 ? vissza : vissza - v;
                double ua = mozogFunc(a, nap, v);
                if (teljesUtUtes(uo, banyasz, uv) <= hatra) {
                    if (!talalt || ua > legjobb_a || (fabs(ua - legjobb_a) < 1e-9 && v > legjobb_h)) {
                        talalt = true; legjobb_a = ua; legjobb_h = v;
                    }
                }
            }
        }

        if (oda == 0 && banyasz && banyaszhat(a)) {
            double ua = banyaszFunc(a, nap);
            if (teljesUtUtes(0, false, vissza) <= hatra) {
                if (!talalt || ua > legjobb_a || (fabs(ua - legjobb_a) < 1e-9 && 1 > legjobb_h)) {
                    talalt = true; legjobb_a = ua; legjobb_h = 1;
                }
            }
        }

        if (!talalt) return (TervEredmeny){false, t, a};

        if (legjobb_h == 0) {
            a = allFunc(a, nap);
        } else if (oda > 0 || (!banyasz && vissza > 0)) {
            int szakasz = oda > 0 ? oda : vissza;
            if (legjobb_h > szakasz || !mozoghat(a, legjobb_h))
                return (TervEredmeny){false, t, a};
            if (oda > 0) oda -= legjobb_h; else vissza -= legjobb_h;
            a = mozogFunc(a, nap, legjobb_h);
        } else if (oda == 0 && banyasz && legjobb_h == 1) {
            if (!banyaszhat(a)) return (TervEredmeny){false, t, a};
            banyasz = false;
            a = banyaszFunc(a, nap);
        } else {
            return (TervEredmeny){false, t, a};
        }

        t++;
    }

    return (TervEredmeny){false, t, a};
}

static bool celpontValasztas(const Rover *r, const Kuldes *k, int *ki_x, int *ki_y) {
    int legjobb_x = -1, legjobb_y = -1;
    int legjobb_koru = NAGY_INT, legjobb_oda = NAGY_INT;
    double legjobb_akku = -1.0;
    Pont tmp[MAX_UT];

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            if (terkep[y][x] != 2 || haz_tav[y][x] == 255) continue;
            int hossz = astar(r->x, r->y, x, y, tmp, MAX_UT);
            if (hossz < 0) continue;
            int oda = hossz - 1;
            int vissza = (int)haz_tav[y][x];
            TervEredmeny te = tervSzimulaciо(r->utes, r->akku, oda, true, vissza, k->osszes_utes);
            if (!te.ok) continue;
            int koru = oda + vissza;
            if (koru < legjobb_koru ||
                (koru == legjobb_koru && oda < legjobb_oda) ||
                (koru == legjobb_koru && oda == legjobb_oda && te.veg_akku > legjobb_akku)) {
                legjobb_koru = koru; legjobb_oda = oda;
                legjobb_akku = te.veg_akku;
                legjobb_x = x; legjobb_y = y;
            }
        }

    if (legjobb_x < 0) return false;
    *ki_x = legjobb_x; *ki_y = legjobb_y;
    return true;
}

static int biztSebesseg(const Rover *r, const Kuldes *k, int lepesek, bool cel_asvany, int tx, int ty) {
    bool nap = nappal_van(r->utes);
    int sorrend[3];

    if (nap) { sorrend[0] = 2; sorrend[1] = 1; sorrend[2] = 3; }
    else     { sorrend[0] = 1; sorrend[1] = 2; sorrend[2] = 3; }

    int marad = k->osszes_utes - r->utes;
    int szuks = cel_asvany
        ? teljesUtUtes(lepesek, true, (int)haz_tav[ty][tx])
        : teljesUtUtes(lepesek, false, 0);
    if (marad <= szuks) { sorrend[0] = 3; sorrend[1] = 2; sorrend[2] = 1; }

    for (int i = 0; i < 3; i++) {
        int v = sorrend[i];
        if (v > lepesek || !mozoghat(r->akku, v)) continue;
        double a2 = mozogFunc(r->akku, nap, v);
        TervEredmeny te = cel_asvany
            ? tervSzimulaciо(r->utes + 1, a2, lepesek - v, true,  (int)haz_tav[ty][tx], k->osszes_utes)
            : tervSzimulaciо(r->utes + 1, a2, lepesek - v, false, 0,                    k->osszes_utes);
        if (te.ok) return v;
    }
    return 0;
}

static void szimulacioLepes(Rover *r, const Kuldes *k, FILE *csv, FILE *txt) {
    bool nap = nappal_van(r->utes);
    const char *tp = nap ? "nap" : "NIGHT";
    float et = napCiklus(r->utes);

    int haza = (int)haz_tav[r->y][r->x];
    if (haza == 255) goto haza_megy;
    if (k->osszes_utes - r->utes <= (haza + 2) / 3 + 1) goto haza_megy;

    if (terkep[r->y][r->x] == 2 && banyaszhat(r->akku)) {
        double a2 = banyaszFunc(r->akku, nap);
        TervEredmeny te = tervSzimulaciо(r->utes + 1, a2, 0, false, haza, k->osszes_utes);
        if (te.ok) {
            char c = terkep_kar[r->y][r->x];
            if      (c == 'G') r->zold++;
            else if (c == 'Y') r->sarga++;
            else if (c == 'B') r->kek++;
            terkep[r->y][r->x] = 0;
            terkep_kar[r->y][r->x] = '.';
            r->akku = a2;
            naploz(csv, r->utes, r, 0, r->mozgott_cellak, tp, et, "DIGGING");
            naploz(txt, r->utes, r, 0, r->mozgott_cellak, tp, et, "DIGGING");
            r->utes++; return;
        }
    }

    {
        int tx = -1, ty = -1;
        if (celpontValasztas(r, k, &tx, &ty)) {
            Pont ut[MAX_UT];
            int hn = astar(r->x, r->y, tx, ty, ut, MAX_UT);
            if (hn >= 0 && hn - 1 > 0) {
                int v = biztSebesseg(r, k, hn - 1, true, tx, ty);
                if (v > 0) {
                    Pont d = ut[v];
                    r->x = d.x; r->y = d.y;
                    r->mozgott_cellak += v;
                    r->akku = mozogFunc(r->akku, nap, v);
                    naploz(csv, r->utes, r, v, r->mozgott_cellak, tp, et, "MOVING");
                    naploz(txt, r->utes, r, v, r->mozgott_cellak, tp, et, "MOVING");
                    r->utes++; return;
                }
            }
        }
    }

haza_megy:
    if (r->x != k->sx || r->y != k->sy) {
        Pont ut[MAX_UT];
        int hn = astar(r->x, r->y, k->sx, k->sy, ut, MAX_UT);
        if (hn >= 0 && hn - 1 > 0) {
            int v = biztSebesseg(r, k, hn - 1, false, k->sx, k->sy);
            if (v > 0) {
                Pont d = ut[v];
                r->x = d.x; r->y = d.y;
                r->mozgott_cellak += v;
                r->akku = mozogFunc(r->akku, nap, v);
                naploz(csv, r->utes, r, v, r->mozgott_cellak, tp, et, "MOVING");
                naploz(txt, r->utes, r, v, r->mozgott_cellak, tp, et, "MOVING");
                r->utes++; return;
            }
        }
    }

    r->akku = allFunc(r->akku, nap);
    naploz(csv, r->utes, r, 0, r->mozgott_cellak, tp, et, "STANDING");
    naploz(txt, r->utes, r, 0, r->mozgott_cellak, tp, et, "STANDING");
    r->utes++;
}

static int terkepBetoltes(const char *fajl, Kuldes *k) {
    FILE *f = fopen(fajl, "r");
    if (!f) return 0;
    char buf[2048];
    bool vanS = false;

    for (int y = 0; y < H; y++) {
        if (!fgets(buf, sizeof(buf), f)) { fclose(f); return 0; }
        char *t = strtok(buf, ",\r\n");
        for (int x = 0; x < W; x++) {
            if (!t) { fclose(f); return 0; }
            char c = t[0];
            terkep_kar[y][x] = c;
            if (c == 'S') {
                k->sx = (int8_t)x; k->sy = (int8_t)y;
                terkep[y][x] = 0; vanS = true;
            } else if (c == '#') {
                terkep[y][x] = 1;
            } else if (c == 'B' || c == 'Y' || c == 'G') {
                terkep[y][x] = 2;
            } else {
                terkep[y][x] = 0;
                terkep_kar[y][x] = '.';
            }
            t = strtok(NULL, ",\r\n");
        }
    }
    fclose(f);
    return vanS ? 1 : 0;
}

int main(void) {
    Kuldes k;
    int orak;

    printf("Kuldes hossza (ora, >=24): ");
    if (scanf("%d", &orak) != 1) return 1;
    if (orak < 24) orak = 24;
    k.osszes_utes = orak * UTES_PER_ORA;

#ifdef _WIN32
    #define TERKEP_FAJL "data\\mars_map_50x50.csv"
    #define LOG_CSV     "output\\rover_log.csv"
    #define LOG_TXT     "output\\ai_route.txt"
#else
    #define TERKEP_FAJL "data/mars_map_50x50.csv"
    #define LOG_CSV     "output/rover_log.csv"
    #define LOG_TXT     "output/ai_route.txt"
#endif

    if (!terkepBetoltes(TERKEP_FAJL, &k)) {
        printf("Hiba: %s nem olvasható (vagy nincs 'S')\n", TERKEP_FAJL);
        return 1;
    }

    hazaTavBFS(k.sx, k.sy);

    Rover r = { k.sx, k.sy, 100.0, 0, 0, 0, 0, 0 };

    FILE *csv = fopen(LOG_CSV, "w");
    FILE *txt = fopen(LOG_TXT, "w");
    if (!csv || !txt) {
        printf("Hiba: log fajl nem nyithato meg (%s, %s).\n", LOG_CSV, LOG_TXT);
        if (csv) fclose(csv);
        if (txt) fclose(txt);
        return 1;
    }

    naploz(csv, -1, &r, 0, 0, "nap", napCiklus(0), "STANDING");
    naploz(txt, -1, &r, 0, 0, "nap", napCiklus(0), "STANDING");

    while (r.utes < k.osszes_utes)
        szimulacioLepes(&r, &k, csv, txt);

    fclose(csv); fclose(txt);

    bool siker = (r.x == k.sx && r.y == k.sy);
    printf("\n--- Kuldes vege ---\n");
    printf("Eredmeny: %s\n", siker ? "SIKERES (hazatert)" : "NEM (nem ert haza)");
    printf("Asványok: total=%d (G=%d, Y=%d, B=%d)\n",
           r.zold + r.sarga + r.kek, r.zold, r.sarga, r.kek);
    printf("Akkumulátor: %.1f%%\n", r.akku);
    printf("Kimenetek: %s  +  %s\n", LOG_CSV, LOG_TXT);

    return 0;
}
