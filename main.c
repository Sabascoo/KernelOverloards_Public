#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* --- CONSTANTS --- */
#define MAP_SIZE 50
#define DAY_ROUNDS 32    // 16 hours
#define NIGHT_ROUNDS 16  // 8 hours
#define FULL_DAY (DAY_ROUNDS + NIGHT_ROUNDS)
#define SCAN_RADIUS 6

/* --- DATA STRUCTURES --- */
typedef enum { STANDING = 0, MOVING = 1, DIGGING = 2 } RoverState;

typedef struct {
    int8_t x, y;
    float battery;
    int diamonds[3];    // [0]=A (Gold), [1]=B (Yellow), [2]=C (Blue)
    uint16_t total_rounds;
} Rover;

typedef struct {
    bool is_day;
    bool is_endgame;
    int remaining_rounds;
} GameState;

typedef struct {
    int8_t x, y;
    float score;
    uint8_t speed;
} Target;

/* --- GLOBAL MAP --- */
uint8_t map[MAP_SIZE][MAP_SIZE]; // 0:empty, 1:wall, 2:G, 3:Y, 4:B

/* --- GUI LOGGING FUNCTION --- */
// Added this to bridge to your C++ GUI
/* --- UPDATED GUI LOGGING --- */
void log_for_gui(Rover *r, GameState *g, RoverState state, int speed, int path_count) {
    static bool first_run = true;
    FILE *f = fopen("ai_route.txt", first_run ? "w" : "a");
    if (f) {
        // Calculate total minerals
        int total_minerals = r->diamonds[0] + r->diamonds[1] + r->diamonds[2];
        
        // Exact time calculation (assuming 1 round = 0.5 hours)
        float exact_time = r->total_rounds * 0.5f;

        // CSV Format: 
        // round, x, y, battery, speed, path, total, green, yellow, blue, is_day, exact_time, state
        fprintf(f, "%d,%d,%d,%.2f,%d,%d,%d,%d,%d,%d,%s,%.1f,%s\n",
                r->total_rounds,           // Round number
                r->x, r->y,                // Rover position
                r->battery,                // Battery
                speed,                     // Speed (1-3)
                path_count,                // Overall path (blocks passed)
                total_minerals,            // Collected minerals (overall)
                r->diamonds[0],            // Collected green
                r->diamonds[1],            // Collected yellow
                r->diamonds[2],            // Collected blue
                g->is_day ? "DAY" : "NIGHT", // Time (day or night)
                exact_time,                // Exact time (float)
                state == MOVING ? "MOVING" : (state == DIGGING ? "DIGGING" : "STANDING") // State
        );
        fclose(f);
    }
    first_run = false;
}
/* --- HELPER FUNCTIONS --- */

bool isOnDiamond(int8_t x, int8_t y) {
    uint8_t cell = map[y][x];
    return (cell >= 2 && cell <= 4);
}

int fajlBeolvas(const char *filename, Rover *r) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 1;

    char sor[1024];
    int row = 0;
    while (fgets(sor, sizeof(sor), fp) && row < MAP_SIZE) {
        int col = 0;
        char *token = strtok(sor, ",\n\r");
        while (token && col < MAP_SIZE) {
            char c = token[0];
            if (c == '#') map[row][col] = 1;
            else if (c == 'G') map[row][col] = 2;
            else if (c == 'Y') map[row][col] = 3;
            else if (c == 'B') map[row][col] = 4;
            else if (c == 'S') {
                r->x = col; r->y = row;
                map[row][col] = 0;
            } else map[row][col] = 0;
            col++;
            token = strtok(NULL, ",\n\r");
        }
        row++;
    }
    fclose(fp);
    return 0;
}

int bfs_distance_8dir(int8_t sx, int8_t sy, int8_t tx, int8_t ty) {
    if (sx == tx && sy == ty) return 0;
    uint8_t dist[MAP_SIZE][MAP_SIZE];
    memset(dist, 255, sizeof(dist));

    int8_t qx[MAP_SIZE * MAP_SIZE], qy[MAP_SIZE * MAP_SIZE];
    int head = 0, tail = 0;

    dist[sy][sx] = 0;
    qx[tail] = sx; qy[tail++] = sy;

    int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

    while (head < tail) {
        int8_t cx = qx[head];
        int8_t cy = qy[head++];

        for (int i = 0; i < 8; i++) {
            int8_t nx = cx + dx[i];
            int8_t ny = cy + dy[i];

            if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE && 
                map[ny][nx] != 1 && dist[ny][nx] == 255) {
                dist[ny][nx] = dist[cy][cx] + 1;
                if (nx == tx && ny == ty) return dist[ny][nx];
                qx[tail] = nx; qy[tail++] = ny;
            }
        }
    }
    return -1;
}

/* --- AI BRAIN FUNCTIONS --- */

float calculate_cluster_value(int8_t x, int8_t y) {
    float total_val = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int8_t nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < MAP_SIZE && ny >= 0 && ny < MAP_SIZE) {
                uint8_t cell = map[ny][nx];
                if (cell == 2) total_val += 3.0; // Gold
                if (cell == 3) total_val += 2.0; // Yellow
                if (cell == 4) total_val += 1.0; // Blue
            }
        }
    }
    return total_val;
}

uint8_t decide_speed(float dist, float battery, bool is_day, bool is_endgame) {
    if (is_endgame) return (battery >= 18.0) ? 3 : (battery >= 8.0 ? 2 : 1);
    if (is_day) {
        if (dist > 6 && battery > 60.0) return 3;
        if (dist > 2) return 2;
        return 1;
    }
    if (battery > 40.0 && dist > 3) return 2;
    return 1;
}

Target find_best_target(Rover *r, GameState *g) {
    Target best = {-1, -1, -1.0f, 1};
    for (int8_t dy = -SCAN_RADIUS; dy <= SCAN_RADIUS; dy++) {
        for (int8_t dx = -SCAN_RADIUS; dx <= SCAN_RADIUS; dx++) {
            int8_t tx = r->x + dx, ty = r->y + dy;
            if (tx < 0 || tx >= MAP_SIZE || ty < 0 || ty >= MAP_SIZE || !isOnDiamond(tx, ty)) continue;

            int dist = bfs_distance_8dir(r->x, r->y, tx, ty);
            if (dist == -1) continue;

            float cluster_score = calculate_cluster_value(tx, ty);
            uint8_t speed = decide_speed((float)dist, r->battery, g->is_day, g->is_endgame);
            float energy_cost = 2.0f * (speed * speed);
            float time_cost = ceilf((float)dist / speed);

            float final_score = cluster_score / (time_cost * energy_cost);
            if (final_score > best.score) {
                best = (Target){tx, ty, final_score, speed};
            }
        }
    }
    return best;
}

/* --- GAME ENGINE --- */

void update_game_state(Rover *r, GameState *g, int total_limit) {
    int current_cycle_pos = r->total_rounds % FULL_DAY;
    g->is_day = (current_cycle_pos < DAY_ROUNDS);
    g->remaining_rounds = total_limit - r->total_rounds;
    g->is_endgame = (g->remaining_rounds <= 2);
}

void ai_decision_step(Rover *r, GameState *g) {
    static int path_count = 0; // Tracks cumulative blocks passed
    float hourly_gain = g->is_day ? 10.0f : 0.0f;

    // 1. DIGGING
    if (isOnDiamond(r->x, r->y) && (r->battery >= 2.0 || g->is_endgame)) {
        uint8_t type = map[r->y][r->x];
        r->diamonds[type - 2]++;
        r->battery -= 2.0;
        map[r->y][r->x] = 0; 
        
        printf("Action: DIGGING at (%d, %d)\n", r->x, r->y);
        log_for_gui(r, g, DIGGING, 0, path_count); 
    } 
    // 2. SURVIVAL
    else if (!g->is_day && r->battery < 19.0 && !g->is_endgame) {
        r->battery -= 1.0;
        printf("Action: STANDING (Low Battery Survival)\n");
        log_for_gui(r, g, STANDING, 0, path_count);
    }
    // 3. MOVEMENT / IDLE
    else {
        Target goal = find_best_target(r, g);
        if (goal.x != -1) {
            // Calculate distance for path tracking before moving
            int dist = bfs_distance_8dir(r->x, r->y, goal.x, goal.y);
            path_count += dist; 

            r->x = goal.x; r->y = goal.y; 
            r->battery -= (2.0f * goal.speed * goal.speed);
            
            printf("Action: MOVING to (%d, %d) Speed %d\n", r->x, r->y, goal.speed);
            log_for_gui(r, g, MOVING, goal.speed, path_count);
        } else {
            r->battery -= 1.0;
            printf("Action: STANDING (No targets in range)\n");
            log_for_gui(r, g, STANDING, 0, path_count);
        }
    }

    r->battery += hourly_gain;
    if (r->battery > 100.0) r->battery = 100.0;
    if (r->battery < 0) r->battery = 0;
    r->total_rounds++;
}

int main() {
    Rover rover = { .battery = 100.0, .total_rounds = 0 };
    GameState state = {0};
    int total_hours;

    printf("Add meg az idot (ora): ");
    if(scanf("%d", &total_hours) != 1) return 1;
    int total_limit_rounds = total_hours * 2;

    if (fajlBeolvas("mars_map_50x50.csv", &rover) != 0) {
        printf("Hiba a fajl beolvasasakor!\n");
        return 1;
    }

    // CLEAR FILE AT START
    remove("ai_route.txt");

    while (rover.total_rounds < total_limit_rounds) {
        update_game_state(&rover, &state, total_limit_rounds);
        
        printf("[KOR %d] Poz: (%d,%d) | Akku: %.1f%% | %s\n", 
                rover.total_rounds, rover.x, rover.y, rover.battery, 
                state.is_day ? "NAPPAL" : "EJSZAKA");

        ai_decision_step(&rover, &state);

        if (rover.battery <= 0 && !state.is_endgame) {
            printf("CRITICAL ERROR: Battery Depleted.\n");
            break;
        }
    }

    printf("\n--- MISSZIO VEGE ---\nGold: %d | Yellow: %d | Blue: %d\n", 
            rover.diamonds[0], rover.diamonds[1], rover.diamonds[2]);
    return 0;
}