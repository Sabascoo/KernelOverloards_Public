// rover_sim.c  (C11)
// Compile:  gcc -O2 -std=c11 rover_sim.c -o rover_sim
// Run:      ./rover_sim
//
// Reads:    mars_map_50x50.csv   (same format as your GUI expects: 50 lines, comma-separated cells)
// Writes:   rover_log.csv        (GUI-compatible 13 columns, NO header)
//           ai_route.txt         (same content as rover_log.csv, for your current GUI LoadAIRoute)
//
// Log columns (13):
// round,x,y,battery,speed,pathCount,totalMinerals,green,yellow,blue,timePeriod,exactTime,stateStr
// stateStr in {STANDING, MOVING, DIGGING}

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define MAP_SIZE 50
#define W MAP_SIZE
#define H MAP_SIZE

#define TICKS_PER_HOUR 2
#define DAY_TICKS 32
#define NIGHT_TICKS 16
#define CYCLE_TICKS (DAY_TICKS + NIGHT_TICKS)

#define BAT_MAX 100.0
#define K_MOVE 2.0

#define MAX_PATH (W * H)

typedef struct { int8_t x, y; } Pt;

typedef struct {
    int8_t x, y;
    double bat;

    int green, yellow, blue;  // collected minerals by color
    int tick;                 // half-hour rounds since start
    int moved_cells;          // total moved cells (pathCount)

} Rover;

typedef struct {
    int total_ticks;
    int8_t sx, sy;
} Mission;

typedef struct {
    double g, f;
    int8_t px, py;
    bool open, closed;
} Node;

static uint8_t mapv[H][W];      // 0 = free, 1 = obstacle, 2 = mineral
static char    mapc[H][W];      // '.', '#', 'B','Y','G','S'
static uint8_t home_dist[H][W]; // 8-dir BFS distance to home, 255 = unreachable

static inline int inb(int x, int y) { return x >= 0 && x < W && y >= 0 && y < H; }

static inline bool is_day_tick(int tick) {
    // Tick 0 starts at 06:30 and is daytime; daytime lasts 32 ticks (16h)
    return (tick % CYCLE_TICKS) < DAY_TICKS;
}

static inline float exact_time_hours_0630(int tick) {
    // 06:30 = 6.5 hours; each tick = 0.5 hours
    float t = 6.5f + 0.5f * (float)tick;
    while (t >= 24.0f) t -= 24.0f;
    while (t < 0.0f)   t += 24.0f;
    return t;
}

static inline double clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline double move_cost(int v) { return K_MOVE * (double)(v * v); } // k * v^2, k=2
static inline double idle_cost(bool mining) { return mining ? 2.0 : 1.0; }
static inline double solar_charge(bool day) { return day ? 10.0 : 0.0; }

static double chebyshev(int x0, int y0, int x1, int y1) {
    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    return (double)((dx > dy) ? dx : dy);
}

static int min_ticks_to_cover_steps(int steps, int v) {
    if (steps <= 0) return 0;
    return (steps + v - 1) / v;
}

// -------------------- GUI-compatible logger (13 columns, no header) --------------------
static void log_gui_line(FILE *f, int round, const Rover *r,
                         int speed, int pathCount,
                         const char *timePeriod, float exactTime,
                         const char *stateStr)
{
    int total = r->green + r->yellow + r->blue;
    fprintf(f, "%d,%d,%d,%.2f,%d,%d,%d,%d,%d,%d,%s,%.2f,%s\n",
            round,
            (int)r->x, (int)r->y,
            (float)r->bat,
            speed,
            pathCount,
            total,
            r->green, r->yellow, r->blue,
            timePeriod,
            exactTime,
            stateStr);
}

// -------------------- BFS home distance (8-dir, unit cost) --------------------
static void precompute_home_dist(int8_t sx, int8_t sy) {
    memset(home_dist, 255, sizeof(home_dist));

    static int16_t qx[MAX_PATH], qy[MAX_PATH];
    int head = 0, tail = 0;

    home_dist[sy][sx] = 0;
    qx[tail] = sx; qy[tail] = sy; tail++;

    static const int dx8[8] = {-1,-1,-1, 0,0, 1,1,1};
    static const int dy8[8] = {-1, 0, 1,-1,1,-1,0,1};

    while (head < tail) {
        int x = qx[head], y = qy[head]; head++;
        uint8_t d = home_dist[y][x];

        for (int i = 0; i < 8; i++) {
            int nx = x + dx8[i], ny = y + dy8[i];
            if (!inb(nx, ny)) continue;
            if (mapv[ny][nx] == 1) continue;           // obstacle
            if (home_dist[ny][nx] != 255) continue;    // visited
            home_dist[ny][nx] = (uint8_t)(d + 1);
            qx[tail] = nx; qy[tail] = ny; tail++;
        }
    }
}

// -------------------- A* pathfinding (8-dir, unit cost) --------------------
static int reconstruct(Node nodes[H][W], int sx, int sy, int gx, int gy, Pt *path, int maxlen) {
    int len = 0;
    int cx = gx, cy = gy;
    while (!(cx == sx && cy == sy)) {
        if (len >= maxlen) return -1;
        path[len++] = (Pt){(int8_t)cx, (int8_t)cy};

        int px = nodes[cy][cx].px;
        int py = nodes[cy][cx].py;
        if (px < 0 || py < 0) return -1;
        cx = px; cy = py;
    }
    if (len >= maxlen) return -1;
    path[len++] = (Pt){(int8_t)sx, (int8_t)sy};

    for (int i = 0; i < len / 2; i++) {
        Pt t = path[i];
        path[i] = path[len - 1 - i];
        path[len - 1 - i] = t;
    }
    return len;
}

static int astar_path(int sx, int sy, int gx, int gy, Pt *out, int outmax) {
    Node nodes[H][W];
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        nodes[y][x].g = 1e100;
        nodes[y][x].f = 1e100;
        nodes[y][x].px = -1;
        nodes[y][x].py = -1;
        nodes[y][x].open = false;
        nodes[y][x].closed = false;
    }

    nodes[sy][sx].g = 0.0;
    nodes[sy][sx].f = chebyshev(sx, sy, gx, gy);
    nodes[sy][sx].open = true;

    static const int dx8[8] = {-1,-1,-1, 0,0, 1,1,1};
    static const int dy8[8] = {-1, 0, 1,-1,1,-1,0,1};

    while (1) {
        int bestx = -1, besty = -1;
        double bestf = 1e100;

        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
            if (nodes[y][x].open && !nodes[y][x].closed && nodes[y][x].f < bestf) {
                bestf = nodes[y][x].f;
                bestx = x; besty = y;
            }
        }
        if (bestx < 0) return -1; // no path

        if (bestx == gx && besty == gy) {
            return reconstruct(nodes, sx, sy, gx, gy, out, outmax);
        }

        nodes[besty][bestx].closed = true;

        for (int i = 0; i < 8; i++) {
            int nx = bestx + dx8[i], ny = besty + dy8[i];
            if (!inb(nx, ny)) continue;
            if (mapv[ny][nx] == 1) continue;

            double ng = nodes[besty][bestx].g + 1.0;
            if (ng < nodes[ny][nx].g) {
                nodes[ny][nx].g = ng;
                nodes[ny][nx].f = ng + chebyshev(nx, ny, gx, gy);
                nodes[ny][nx].px = (int8_t)bestx;
                nodes[ny][nx].py = (int8_t)besty;
                nodes[ny][nx].open = true;
            }
        }
    }
}

// -------------------- Target selection --------------------
static bool pick_target(const Rover *r, const Mission *m, int ticks_left,
                        int *out_tx, int *out_ty)
{
    int best_tx = -1, best_ty = -1;
    double best_score = -1e100;

    Pt tmp[MAX_PATH];

    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        if (mapv[y][x] != 2) continue; // not a mineral

        int len = astar_path(r->x, r->y, x, y, tmp, MAX_PATH);
        if (len < 0) continue;

        int steps_to = len - 1;

        if (home_dist[y][x] == 255) continue;
        int steps_home_from = (int)home_dist[y][x];

        // optimistic feasibility with vmax=3
        int min_ticks_need = min_ticks_to_cover_steps(steps_to, 3) + 1
                           + min_ticks_to_cover_steps(steps_home_from, 3);

        if (min_ticks_need > ticks_left) continue;

        // Simple score (all minerals equal) - prefer closer + not too far from home
        double score = 1000.0 - 2.0 * steps_to - 1.0 * steps_home_from;

        if (score > best_score) {
            best_score = score;
            best_tx = x; best_ty = y;
        }
    }

    if (best_tx < 0) return false;
    *out_tx = best_tx; *out_ty = best_ty;
    return true;
}

static int choose_speed(bool day, int ticks_left, int steps_to_home, double bat) {
    // Day: v=2 default (net +2). Night: v=1 default (cheap).
    int v = day ? 2 : 1;

    int min_home_fast = min_ticks_to_cover_steps(steps_to_home, 3);
    int min_home_med  = min_ticks_to_cover_steps(steps_to_home, 2);
    int min_home_slow = min_ticks_to_cover_steps(steps_to_home, 1);

    // If time is tight, go faster
    if (ticks_left <= min_home_fast + 1) v = 3;
    else if (ticks_left <= min_home_med + 2) v = 2;
    else if (!day && ticks_left <= min_home_slow + 2) v = 2;

    // Battery guard
    if (v == 3 && bat < move_cost(3)) v = 2;
    if (v == 2 && bat < move_cost(2)) v = 1;

    return v;
}

// -------------------- Simulation step --------------------
static void sim_step(Rover *r, const Mission *m, FILE *log_csv, FILE *log_ai) {
    bool day = is_day_tick(r->tick);
    const char *tp = day ? "DAY" : "NIGHT";
    float et = exact_time_hours_0630(r->tick);

    int ticks_left = m->total_ticks - r->tick;

    // If no path home from current position, safest: stand
    if (home_dist[r->y][r->x] == 255) {
        double cons = idle_cost(false);
        if (r->bat >= cons) r->bat = clamp(r->bat - cons + solar_charge(day), 0.0, BAT_MAX);
        else r->bat = 0.0;

        log_gui_line(log_csv, r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
        log_gui_line(log_ai,  r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
        r->tick++;
        return;
    }

    int steps_home = (int)home_dist[r->y][r->x];

    // If on mineral: mine if time-feasible + battery-feasible
    if (mapv[r->y][r->x] == 2) {
        int need_ticks = 1 + min_ticks_to_cover_steps(steps_home, 3);
        double cons = idle_cost(true);

        if (need_ticks <= ticks_left && r->bat >= cons) {
            // Count color from mapc
            char mineral = mapc[r->y][r->x]; // 'G'/'Y'/'B'
            if (mineral == 'G') r->green++;
            else if (mineral == 'Y') r->yellow++;
            else if (mineral == 'B') r->blue++;

            mapv[r->y][r->x] = 0;
            mapc[r->y][r->x] = '.';

            r->bat = clamp(r->bat - cons + solar_charge(day), 0.0, BAT_MAX);

            log_gui_line(log_csv, r->tick, r, 0, r->moved_cells, tp, et, "DIGGING");
            log_gui_line(log_ai,  r->tick, r, 0, r->moved_cells, tp, et, "DIGGING");
            r->tick++;
            return;
        }
        // else: fall through to go home or move elsewhere
    }

    // Decide if must go home now (time pressure)
    int min_home_fast = min_ticks_to_cover_steps(steps_home, 3);
    bool must_home = (ticks_left <= min_home_fast + 1);

    int tx = -1, ty = -1;
    bool have_target = false;
    if (!must_home) {
        have_target = pick_target(r, m, ticks_left, &tx, &ty);
    }

    int goalx = (must_home || !have_target) ? m->sx : tx;
    int goaly = (must_home || !have_target) ? m->sy : ty;

    // If already at goal and no target: stand
    if (r->x == goalx && r->y == goaly) {
        double cons = idle_cost(false);
        if (r->bat >= cons) r->bat = clamp(r->bat - cons + solar_charge(day), 0.0, BAT_MAX);
        else r->bat = 0.0;

        log_gui_line(log_csv, r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
        log_gui_line(log_ai,  r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
        r->tick++;
        return;
    }

    // Build A* path to goal
    Pt path[MAX_PATH];
    int plen = astar_path(r->x, r->y, goalx, goaly, path, MAX_PATH);
    if (plen < 0) {
        // can't reach goal -> stand
        double cons = idle_cost(false);
        if (r->bat >= cons) r->bat = clamp(r->bat - cons + solar_charge(day), 0.0, BAT_MAX);
        else r->bat = 0.0;

        log_gui_line(log_csv, r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
        log_gui_line(log_ai,  r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
        r->tick++;
        return;
    }

    int steps_to_goal = plen - 1;

    // Choose speed
    int v = choose_speed(day, ticks_left, steps_home, r->bat);

    // If goal is closer than v, still spend energy for chosen v (per rules: speed chosen per round)
    int step_count = v;
    if (step_count > steps_to_goal) step_count = steps_to_goal;
    if (step_count < 1) step_count = 0;

    // Battery check; downgrade if needed
    double cons = (step_count > 0) ? move_cost(v) : idle_cost(false);
    if (step_count > 0 && r->bat < cons) {
        if (v == 3) { v = 2; cons = move_cost(v); }
        if (v == 2 && r->bat < cons) { v = 1; cons = move_cost(v); }
    }

    if (step_count > 0 && r->bat < cons) {
        // can't move -> stand
        cons = idle_cost(false);
        if (r->bat >= cons) r->bat = clamp(r->bat - cons + solar_charge(day), 0.0, BAT_MAX);
        else r->bat = 0.0;

        log_gui_line(log_csv, r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
        log_gui_line(log_ai,  r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
        r->tick++;
        return;
    }

    // Move along path by step_count cells
    if (step_count > 0) {
        Pt dest = path[step_count]; // path[0]=current
        r->x = dest.x;
        r->y = dest.y;
        r->moved_cells += step_count;
        r->bat = clamp(r->bat - move_cost(v) + solar_charge(day), 0.0, BAT_MAX);

        log_gui_line(log_csv, r->tick, r, v, r->moved_cells, tp, et, "MOVING");
        log_gui_line(log_ai,  r->tick, r, v, r->moved_cells, tp, et, "MOVING");
        r->tick++;
        return;
    }

    // No step (should be rare): stand
    cons = idle_cost(false);
    if (r->bat >= cons) r->bat = clamp(r->bat - cons + solar_charge(day), 0.0, BAT_MAX);
    else r->bat = 0.0;

    log_gui_line(log_csv, r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
    log_gui_line(log_ai,  r->tick, r, 0, r->moved_cells, tp, et, "STANDING");
    r->tick++;
}

// -------------------- Map reading (CSV like GUI) --------------------
static int load_map_csv(const char *fname, Mission *m) {
    FILE *f = fopen(fname, "r");
    if (!f) return 0;

    char buf[2048];
    bool foundS = false;

    for (int y = 0; y < H; y++) {
        if (!fgets(buf, sizeof(buf), f)) { fclose(f); return 0; }

        char *t = strtok(buf, ",\r\n");
        for (int x = 0; x < W; x++) {
            if (!t) { fclose(f); return 0; }
            char c = t[0];
            mapc[y][x] = c;

            if (c == 'S') {
                m->sx = (int8_t)x;
                m->sy = (int8_t)y;
                mapv[y][x] = 0;
                foundS = true;
            } else if (c == '#') {
                mapv[y][x] = 1;
            } else if (c == 'B' || c == 'Y' || c == 'G') {
                mapv[y][x] = 2;
            } else {
                mapv[y][x] = 0;
                mapc[y][x] = '.';
            }

            t = strtok(NULL, ",\r\n");
        }
    }

    fclose(f);
    return foundS ? 1 : 0;
}

// -------------------- main --------------------
int main(void) {
    Mission m;
    int hours;

    printf("Küldetés hossza (óra, >=24): ");
    if (scanf("%d", &hours) != 1) return 1;
    if (hours < 24) hours = 24;

    m.total_ticks = hours * TICKS_PER_HOUR;

    if (!load_map_csv("mars_map_50x50.csv", &m)) {
        printf("Hiba: nem tudtam beolvasni a mars_map_50x50.csv térképet (vagy nincs 'S').\n");
        return 1;
    }

    precompute_home_dist(m.sx, m.sy);

    Rover r;
    r.x = m.sx; r.y = m.sy;
    r.bat = 100.0;
    r.green = r.yellow = r.blue = 0;
    r.tick = 0;
    r.moved_cells = 0;

    FILE *log_csv = fopen("rover_log.csv", "w");
    FILE *log_ai  = fopen("ai_route.txt", "w");
    if (!log_csv || !log_ai) {
        printf("Hiba: nem tudok log fájl(oka)t megnyitni írásra.\n");
        if (log_csv) fclose(log_csv);
        if (log_ai) fclose(log_ai);
        return 1;
    }

    while (r.tick < m.total_ticks && r.bat > 0.0) {
        sim_step(&r, &m, log_csv, log_ai);
    }

    fclose(log_csv);
    fclose(log_ai);

    bool success = (r.x == m.sx && r.y == m.sy);
    printf("\n--- Küldetés vége ---\n");
    printf("Eredmény: %s\n", success ? "SIKERES (hazatért)" : "NEM (nem ért haza / lemerült)");
    printf("Ásványok: total=%d (G=%d, Y=%d, B=%d)\n",
           r.green + r.yellow + r.blue, r.green, r.yellow, r.blue);
    printf("Akkumulátor: %.1f%%\n", r.bat);
    printf("Kimenetek: rover_log.csv  +  ai_route.txt\n");

    return 0;
}