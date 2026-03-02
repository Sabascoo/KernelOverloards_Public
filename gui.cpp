#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> 

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Adatszerkezetek ---
struct Cell { char type = '.'; };
static const int MAP_W = 50;
static const int MAP_H = 50;
Cell mapGrid[MAP_H][MAP_W];

enum RoverState { STANDING, MOVING, DIGGING };

struct Rover {
    int x = 0, y = 0;
    float battery = 100.0f; 
    int zold = 0, sarga = 0, kek = 0;
    int sebesseg = 0; 
    float irany = 0.0f; 
    float tavolsag = 0.0f; 
    RoverState state = STANDING; 
} rover;

struct Vilag {
    float ora = 6.5f; 
    float idosebesseg = 0.001f; // Kicsit lassabbra véve (0.001f volt)
} vilag;

float moveTimer = 0.0f;

// Represents one action/step in the AI's route
struct RouteStep {
    int x, y;
    RoverState state;
    int speed;
};

// Global variables for the AI routing
std::vector<RouteStep> aiRoute;
int currentRouteIndex = 0;
float timeSinceLastStep = 0.0f; 
const float STEP_INTERVAL = 0.5f; // 0.5 simulation hours per round

// --- Segédfüggvények ---

ImU32 GetEgboltSzin(float h) {
    if (h >= 6.5f && h < 10.0f) return IM_COL32(255, 180, 100, 255); 
    if (h >= 10.0f && h < 16.0f) return IM_COL32(100, 180, 255, 255); 
    if (h >= 16.0f && h < 20.0f) return IM_COL32(255, 100, 50, 255);  
    if (h >= 20.0f && h < 22.5f) return IM_COL32(40, 40, 100, 255);  
    return IM_COL32(10, 10, 25, 255); 
}

void LoadMap(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    std::string line;
    int row = 0;
    while (std::getline(file, line) && row < MAP_H) {
        std::stringstream ss(line);
        std::string cell;
        int col = 0;
        while (std::getline(ss, cell, ',') && col < MAP_W) {
            if (!cell.empty()) {
                mapGrid[row][col].type = cell[0];
                if (cell[0] == 'S') { rover.x = col; rover.y = row; }
            }
            col++;
        }
        row++;
    }
}

void DrawIranytu(ImVec2 p, float sugar) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImVec2(p.x + sugar + 10, p.y + sugar + 10);
    drawList->AddCircleFilled(center, sugar, IM_COL32(20, 20, 20, 230));
    drawList->AddCircle(center, sugar, IM_COL32(57, 255, 20, 255), 32, 2.0f);
    drawList->AddText(ImVec2(center.x - 5, center.y - sugar + 2), IM_COL32_WHITE, "É");
    float rad = (rover.irany - 90.0f) * (M_PI / 180.0f);
    ImVec2 mutatoVeg = ImVec2(center.x + cosf(rad) * (sugar - 5), center.y + sinf(rad) * (sugar - 5));
    drawList->AddLine(center, mutatoVeg, IM_COL32(255, 0, 0, 255), 3.0f);
}

void DrawFPV(ImVec2 size) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float time = (float)glfwGetTime();
    ImVec2 center = ImVec2(p.x + size.x * 0.5f, p.y + size.y * 0.5f);
    float horizonY = p.y + size.y * 0.45f;

    // --- 1. IRÁNYVEKTOROK KISZÁMÍTÁSA (Ez hiányzott a hibához) ---
    int dx = 0, dy = 0, rx = 0, ry = 0;
    float angle = fmodf(rover.irany, 360.0f); if (angle < 0) angle += 360.0f;
    if (angle >= 315 || angle < 45)       { dy = -1; rx = 1; ry = 0; }
    else if (angle >= 45 && angle < 135)  { dx = 1;  rx = 0; ry = 1; }
    else if (angle >= 135 && angle < 225) { dy = 1;  rx = -1; ry = 0; }
    else                                  { dx = -1; rx = 0; ry = -1; }

    // --- 2. DYNAMIKUS REZGÉS ---
    float shakeX = 0, shakeY = 0;
    if (rover.state == MOVING) {
        shakeY = sinf(time * 15.0f) * 3.0f;
    } else if (rover.state == DIGGING) {
        shakeX = sinf(time * 60.0f) * 1.8f;
        shakeY = cosf(time * 60.0f) * 1.8f;
    }

    // --- 3. ÉGBOLT ÉS TALAJ ---
    bool ejjellato = (vilag.ora >= 22.0f || vilag.ora < 6.5f);
    ImU32 egSzin = ejjellato ? IM_COL32(5, 25, 5, 255) : GetEgboltSzin(vilag.ora);
    drawList->AddRectFilled(p, ImVec2(p.x + size.x, horizonY), egSzin); // Ég
    drawList->AddRectFilled(ImVec2(p.x, horizonY), ImVec2(p.x + size.x, p.y + size.y), IM_COL32(35, 20, 10, 255)); // Talaj alap

    // --- 4. PERSPEKTIVIKUS BLOKKOK ÉS GYÉMÁNTOK ---
    for (int dist = 3; dist >= 1; dist--) {
        for (int side = -1; side <= 1; side++) {
            int cx = rover.x + (dx * dist) + (rx * side);
            int cy = rover.y + (dy * dist) + (ry * side);

            if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) continue;
            char type = mapGrid[cy][cx].type;
            if (type == '.' || type == 'S') continue;

            float scale = 1.0f / (float)dist; 
            float xPos = center.x + (side * size.x * 0.55f * scale) + shakeX;
            float yPos = horizonY + (size.y * 0.12f * scale) + shakeY; // Fixálva a lebegés ellen
            float bW = size.x * 0.35f * scale;
            float bH = size.y * 0.45f * scale;

            if (type == 'G' || type == 'Y' || type == 'B' || type == 'D') {
                // DRÁGAKŐ FORMA (Diamond shape)
                ImU32 dCol = (type == 'G') ? IM_COL32(0, 255, 0, 220) : 
                            (type == 'Y') ? IM_COL32(255, 255, 0, 220) : 
                            (type == 'B') ? IM_COL32(0, 150, 255, 220) : IM_COL32(255, 255, 255, 220);
                
                ImVec2 pts[4] = {
                    ImVec2(xPos, yPos - bH),         // Teteje
                    ImVec2(xPos - bW/2, yPos - bH/2), // Bal
                    ImVec2(xPos, yPos),              // Alja
                    ImVec2(xPos + bW/2, yPos - bH/2)  // Jobb
                };
                
                // Kitöltés
                drawList->AddConvexPolyFilled(pts, 4, dCol);
                
                // Keret - Itt cseréltem ki a flag-et ImDrawFlags_Closed-re!
                drawList->AddPolyline(pts, 4, IM_COL32(255, 255, 255, 200), ImDrawFlags_Closed, 1.5f * scale);
                
                // Belső csillogás (egy függőleges és egy vízszintes halvány vonal)
                drawList->AddLine(pts[0], pts[2], IM_COL32(255, 255, 255, 100), 1.0f);
                drawList->AddLine(pts[1], pts[3], IM_COL32(255, 255, 255, 100), 1.0f);
            } else {
                // SIMA FAL
                drawList->AddRectFilled(ImVec2(xPos - bW/2, yPos - bH), ImVec2(xPos + bW/2, yPos), IM_COL32(100, 60, 40, 255), 2.0f);
                drawList->AddRect(ImVec2(xPos - bW/2, yPos - bH), ImVec2(xPos + bW/2, yPos), IM_COL32(0, 0, 0, 150));
            }
        }
    }

    // --- 5. LETISZTULT ROVER COCKPIT ---
    float roverBottom = p.y + size.y;
    float dashH = 45.0f; // Vékonyabb dashboard
    
    // Alap panel
    drawList->AddRectFilled(ImVec2(p.x, roverBottom - dashH), ImVec2(p.x + size.x, roverBottom), IM_COL32(20, 20, 25, 255));
    // Oldalsó váz (Sokkal vékonyabb pillérek)
    drawList->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + 10, roverBottom), IM_COL32(30, 30, 35, 255));
    drawList->AddRectFilled(ImVec2(p.x + size.x - 10, p.y), ImVec2(p.x + size.x, roverBottom), IM_COL32(30, 30, 35, 255));

    // --- 6. PROFI ANIMÁLT FÚRÓ ---
    if (rover.state == DIGGING) {
        ImVec2 dPos = ImVec2(center.x, roverBottom - dashH + 5);
        // Fúrófej alap (Sötét fém)
        drawList->AddRectFilled(ImVec2(dPos.x - 25, dPos.y - 15), ImVec2(dPos.x + 25, dPos.y + 10), IM_COL32(50, 50, 55, 255));
        
        // Spirális menet (Screw effect)
        for (int i = 0; i < 6; i++) {
            float move = fmodf(time * 12.0f + (i * 0.25f), 1.0f);
            float ty = dPos.y - (i * 10) - (move * 8);
            float tw = 24.0f - (i * 3.5f); // Szűkülő forma
            if (ty < dPos.y - 55) continue; // Ne lógjon túl a hegyén

            drawList->AddEllipseFilled(ImVec2(dPos.x, ty), ImVec2(tw, 4.0f), IM_COL32(140, 140, 150, 255));
            drawList->AddEllipse(ImVec2(dPos.x, ty), ImVec2(tw, 4.0f), IM_COL32(200, 200, 210, 255), 0, 1.0f);
        }
    }

    // --- 7. HUD ---
    if (ejjellato) drawList->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(0, 255, 0, 15));
    drawList->AddCircle(center, 12.0f, IM_COL32(0, 255, 0, 100), 16, 1.0f);
    drawList->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(57, 255, 20, 255), 0, 0, 1.5f);

    ImGui::Dummy(size);
}

void DrawDiagram(ImVec2 size) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(15, 15, 15, 255));
    float time = (float)glfwGetTime();
    
    float freq = 0.0f;
    float amp = 0.0f;

    if (rover.state == MOVING) {
        freq = (rover.sebesseg == 1) ? 2.0f : (rover.sebesseg == 2) ? 6.0f : 15.0f;
        amp = size.y * 0.3f;
    } else if (rover.state == DIGGING) {
        freq = 45.0f; // Magas frekvenciájú zaj fúrásnál
        amp = size.y * 0.15f;
    } else {
        freq = 1.0f; // Alapzaj álló helyzetben
        amp = 2.0f;
    }

    ImVec2 prev;
    for (int i = 0; i < (int)size.x; i += 2) {
        float py = pos.y + (size.y * 0.5f) + sinf(time * 10.0f + (i * 0.05f * freq)) * amp;
        if (i > 0) drawList->AddLine(prev, ImVec2(pos.x + i, py), IM_COL32(57, 255, 20, 255), 1.5f);
        prev = ImVec2(pos.x + i, py);
    }
    ImGui::Dummy(size);
}

void DrawBalPanel() {
    ImGui::BeginChild("RendszerPanel", ImVec2(0,0), true);
    ImVec2 region = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float time = (float)glfwGetTime();

    // --- 1. BATTERY HUB ---
    ImVec2 cp = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(cp.x + region.x * 0.5f, cp.y + 70);
    float rad = 55.0f; 
    bool kritikus = (rover.battery < 20.0f);
    float flash = kritikus ? (0.4f + 0.6f * (0.5f + 0.5f * sinf(time * 12.0f))) : 1.0f;
    ImU32 batColor = kritikus ? IM_COL32(255, 0, 0, (int)(255 * flash)) : IM_COL32(57, 255, 20, 255);
    drawList->AddCircle(center, rad, IM_COL32(40, 40, 40, 255), 64, 10.0f);
    drawList->PathArcTo(center, rad, -M_PI/2, -M_PI/2 + (M_PI * 2 * (rover.battery / 100.0f)), 64);
    drawList->PathStroke(batColor, 0, 10.0f);
    char batStr[16]; snprintf(batStr, sizeof(batStr), "%d%%", (int)rover.battery);
    ImGui::SetWindowFontScale(1.8f);
    ImVec2 textSize = ImGui::CalcTextSize(batStr);
    drawList->AddText(ImVec2(center.x - textSize.x / 2.0f, center.y - textSize.y / 2.0f), (kritikus ? batColor : IM_COL32_WHITE), batStr);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 140);

    // --- 2. POWER DISTRIBUTION ---
    ImGui::Text("POWER ALLOCATION");
    float engineLoad = 0.05f;
    if (rover.state == MOVING) engineLoad = (rover.sebesseg == 1) ? 0.40f : (rover.sebesseg == 2) ? 0.70f : 0.95f;
    else if (rover.state == DIGGING) engineLoad = 0.85f;

    auto DrawPowerBar = [&](const char* label, float percent, ImU32 col) {
        ImGui::Text("%s", label); ImGui::SameLine(80);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::ColorConvertU32ToFloat4(col));
        ImGui::ProgressBar(percent + (sinf(time * 5.0f) * 0.01f), ImVec2(-1, 10), "");
        ImGui::PopStyleColor();
    };
    DrawPowerBar("ENG", engineLoad, IM_COL32(57, 255, 20, 200));
    DrawPowerBar("SEN", 0.20f, IM_COL32(0, 200, 255, 200));
    DrawPowerBar("COM", 0.15f, IM_COL32(255, 200, 0, 200));

    ImGui::Separator();

    // --- 3. FPV CAMERA ---
    ImGui::Text("LIVE FEED [HD-X2]");
    DrawFPV(ImVec2(region.x, 200));

    // --- 4. COMMS STATUS ---
    ImGui::Separator();
    float signal = 0.8f + 0.15f * sinf(time * 0.5f);
    ImGui::Text("COMMS:"); ImGui::SameLine();
    ImGui::ProgressBar(signal, ImVec2(80, 15), ""); ImGui::SameLine();
    ImGui::Text("LATENCY: %dms", (int)(420 + sinf(time) * 10));

    // --- 5. TELEMETRY DIAGRAM ---
    ImGui::Text("VIBRATION ANALYSIS");
    DrawDiagram(ImVec2(region.x, 60));

    // --- 6. STATS & RESOURCES BOX ---
    ImGui::BeginChild("Adatok", ImVec2(0,0), true);
    
    ImGui::Columns(2, "stats", false);
    int h = (int)vilag.ora; int m = (int)((vilag.ora - h) * 60);
    ImGui::Text("TIME: %02d:%02d", h, m);
    ImGui::Text("POS: [%d, %d]", rover.x, rover.y);
    ImGui::NextColumn();
    const char* stTxt = (rover.state == STANDING) ? "STANDING" : (rover.state == MOVING) ? "MOVING" : "DIGGING";
    ImGui::Text("STATE: %s", stTxt);
    ImGui::Text("DIST: %.1f", rover.tavolsag);
    ImGui::Columns(1);
    
    ImGui::Separator();

    // --- MEGNÖVELT GYÉMÁNT OSZLOPOK ---
    int total = rover.zold + rover.sarga + rover.kek;
    ImGui::Text("TOTAL SAMPLES COLLECTED: %d", total);
    ImGui::Spacing();

    ImGui::Columns(3, "res_cols", false);
    auto DrawResourceBar = [&](int count, ImU32 color, const char* label) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        float maxHeight = 85.0f; // Magasabb oszlopok
        float barWidth = 45.0f;  // Szélesebb oszlopok
        float currentHeight = std::min((float)count * 7.0f, maxHeight); 
        
        // Háttér
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + barWidth, p.y + maxHeight), IM_COL32(30, 30, 30, 255));
        // Oszlop
        if(currentHeight > 0)
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y + maxHeight - currentHeight), ImVec2(p.x + barWidth, p.y + maxHeight), color);
        
        ImGui::Dummy(ImVec2(barWidth, maxHeight + 5));
        ImGui::Text("%s: %d", label, count);
    };

    DrawResourceBar(rover.kek, IM_COL32(0, 120, 255, 255), "BLU"); ImGui::NextColumn();
    DrawResourceBar(rover.sarga, IM_COL32(255, 255, 0, 255), "YLW"); ImGui::NextColumn();
    DrawResourceBar(rover.zold, IM_COL32(0, 255, 0, 255), "GRN"); ImGui::NextColumn();
    
    ImGui::Columns(1);
    ImGui::EndChild();

    ImGui::EndChild();
}

void DrawMap() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    // Adjunk neki egy fix azonosítót és méretet
    ImGui::BeginChild("Terkep", ImVec2(0,0), false, ImGuiWindowFlags_NoScrollbar);
    
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float side = std::min(avail.x, avail.y);
    float cs = side / 50.0f;
    
    // Itt a trükk: Elhelyezünk egy láthatatlan elemet, ami kitölti a teret.
    // Ez megakadályozza, hogy az ImGui levágja a rajzunkat (Clipping).
    ImGui::Dummy(ImVec2(side, side)); 
    
    // A p-t a Dummy után/alatt kérjük le, de korrigáljuk a kezdőpontra
    ImVec2 p = ImGui::GetItemRectMin(); 
    p.x += (avail.x - side) / 2.0f; 
    p.y += (avail.y - side) / 2.0f;
    
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // --- 1. RÉTEG: TEREP ---
    for(int y=0; y<50; y++) {
        for(int x=0; x<50; x++) {
            ImU32 col;
            char type = mapGrid[y][x].type;
            if (type == 'S') col = IM_COL32(255, 255, 255, 255);
            else if (type == 'G') col = IM_COL32(0, 255, 0, 255);
            else if (type == 'Y') col = IM_COL32(255, 255, 0, 255);
            else if (type == 'B') col = IM_COL32(0, 120, 255, 255);
            else if (type == '#') col = IM_COL32(80, 40, 30, 255);
            else col = IM_COL32(140 + (x%2 * 5), 60, 35, 255); 
            dl->AddRectFilled(ImVec2(p.x+x*cs, p.y+y*cs), ImVec2(p.x+(x+1)*cs, p.y+(y+1)*cs), col);
        }
    }

    // --- 2. RÉTEG: AI ÚTVONAL ---
    if (!aiRoute.empty()) {
        ImVec2 startPos = ImVec2(p.x + aiRoute[0].x * cs + cs/2, p.y + aiRoute[0].y * cs + cs/2);
        dl->AddCircleFilled(startPos, cs * 0.4f, IM_COL32(255, 50, 50, 255));
        ImVec2 prevPoint = startPos;
        for (int i = 1; i <= currentRouteIndex && i < (int)aiRoute.size(); i++) {
            ImVec2 nextPoint = ImVec2(p.x + aiRoute[i].x * cs + cs/2, p.y + aiRoute[i].y * cs + cs/2);
            dl->AddLine(prevPoint, nextPoint, IM_COL32(255, 140, 0, 200), 2.5f);
            dl->AddCircleFilled(nextPoint, cs * 0.15f, IM_COL32(255, 200, 0, 255));
            if (aiRoute[i].state == DIGGING) {
                dl->AddLine(ImVec2(nextPoint.x-cs*0.3f, nextPoint.y-cs*0.3f), ImVec2(nextPoint.x+cs*0.3f, nextPoint.y+cs*0.3f), IM_COL32(255, 50, 50, 255), 2.0f);
                dl->AddLine(ImVec2(nextPoint.x+cs*0.3f, nextPoint.y-cs*0.3f), ImVec2(nextPoint.x-cs*0.3f, nextPoint.y+cs*0.3f), IM_COL32(255, 50, 50, 255), 2.0f);
            }
            prevPoint = nextPoint;
        }
    }

    // --- 3. RÉTEG: ROVER (Vastagabb, láthatóbb verzió) ---
    ImVec2 rPos = ImVec2(p.x + rover.x * cs + cs/2, p.y + rover.y * cs + cs/2);
    
    // Erősebb szenzor lüktetés
    float pulse = 0.15f + 0.10f * sinf((float)glfwGetTime() * 5.0f);
    dl->AddCircleFilled(rPos, 8.0f * cs * pulse, IM_COL32(0, 255, 255, 80));

    // Rover háromszög (Kicsit szélesebb szögek a jobb láthatóságért)
    float rad = (rover.irany - 90.0f) * (M_PI / 180.0f);
    float rSize = cs * 1.2f; 
    ImVec2 p1 = ImVec2(rPos.x + cosf(rad) * rSize, rPos.y + sinf(rad) * rSize);
    ImVec2 p2 = ImVec2(rPos.x + cosf(rad + 2.3f) * rSize, rPos.y + sinf(rad + 2.3f) * rSize);
    ImVec2 p3 = ImVec2(rPos.x + cosf(rad - 2.3f) * rSize, rPos.y + sinf(rad - 2.3f) * rSize);
    
    // "Glow" effekt a rover alatt, hogy elváljon a talajtól
    dl->AddCircleFilled(rPos, rSize, IM_COL32(255, 255, 255, 100));
    dl->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 255, 255, 255));
    dl->AddTriangle(p1, p2, p3, IM_COL32(0, 0, 0, 255), 2.0f);

    // --- 4. RÉTEG: KERET ---
    dl->AddRect(p, ImVec2(p.x+side, p.y+side), IM_COL32(57, 255, 20, 255), 0, 0, 2.0f);
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void UpdateAILogic() {
    if (aiRoute.empty() || currentRouteIndex >= aiRoute.size() - 1) {
        rover.state = STANDING;
        rover.sebesseg = 0;
        return;
    }

    // Az időzítő a világ idejéhez van kötve
    timeSinceLastStep += vilag.idosebesseg;

    // Ha eltelt 0.5 szimulációs óra, jöhet a következő lépés
    if (timeSinceLastStep >= STEP_INTERVAL) {
        timeSinceLastStep -= STEP_INTERVAL; 
        currentRouteIndex++;
        
        RouteStep currentStep = aiRoute[currentRouteIndex];
        
        // Kamera forgatása a mozgás iránya alapján
        if (currentStep.x > rover.x) rover.irany = 90.0f;       // Kelet
        else if (currentStep.x < rover.x) rover.irany = 270.0f; // Nyugat
        else if (currentStep.y > rover.y) rover.irany = 180.0f; // Dél
        else if (currentStep.y < rover.y) rover.irany = 0.0f;   // Észak

        // Új pozíció és állapot beállítása
        rover.x = currentStep.x;
        rover.y = currentStep.y;
        rover.state = currentStep.state;
        rover.sebesseg = currentStep.speed;
        
        if (rover.state == MOVING) rover.tavolsag += 1.0f;

        // Bányászat: Pontosan egyszer hajtódik végre az adott lépésben!
        if (rover.state == DIGGING) {
            char& currentTile = mapGrid[rover.y][rover.x].type;
            if (currentTile == 'G') { rover.zold++; currentTile = '.'; }
            else if (currentTile == 'Y') { rover.sarga++; currentTile = '.'; }
            else if (currentTile == 'B') { rover.kek++; currentTile = '.'; }
        }
    }
}

void InitFakeRoute() {
    int startX = rover.x;
    int startY = rover.y;
    
    // Garantált gyémántok elhelyezése a teszthez
    if (startX + 3 < MAP_W) mapGrid[startY][startX + 3].type = 'B';
    if (startY + 3 < MAP_H) mapGrid[startY + 3][startX + 3].type = 'Y';
    if (startY + 5 < MAP_H) mapGrid[startY + 5][startX + 2].type = 'G';
    if (startY + 7 < MAP_H) mapGrid[startY + 7][startX + 5].type = 'B';

    aiRoute.push_back({startX, startY, STANDING, 0}); // 0. lépés
    
    // Irány az 1. gyémánt
    aiRoute.push_back({startX + 1, startY, MOVING, 1}); // 1
    aiRoute.push_back({startX + 2, startY, MOVING, 1}); // 2
    aiRoute.push_back({startX + 3, startY, MOVING, 1}); // 3
    aiRoute.push_back({startX + 3, startY, DIGGING, 0}); // 4
    
    // Irány a 2. gyémánt
    aiRoute.push_back({startX + 3, startY + 1, MOVING, 2}); // 5
    aiRoute.push_back({startX + 3, startY + 2, MOVING, 2}); // 6
    aiRoute.push_back({startX + 3, startY + 3, MOVING, 1}); // 7
    aiRoute.push_back({startX + 3, startY + 3, DIGGING, 0}); // 8
    
    // Irány a 3. gyémánt
    aiRoute.push_back({startX + 2, startY + 3, MOVING, 1}); // 9
    aiRoute.push_back({startX + 2, startY + 4, MOVING, 1}); // 10
    aiRoute.push_back({startX + 2, startY + 5, MOVING, 1}); // 11
    aiRoute.push_back({startX + 2, startY + 5, DIGGING, 0}); // 12

    // Séta és kutatás tovább
    aiRoute.push_back({startX + 3, startY + 5, MOVING, 1}); // 13
    aiRoute.push_back({startX + 4, startY + 5, MOVING, 1}); // 14
    aiRoute.push_back({startX + 5, startY + 5, MOVING, 1}); // 15
    aiRoute.push_back({startX + 5, startY + 6, MOVING, 1}); // 16
    aiRoute.push_back({startX + 5, startY + 7, MOVING, 1}); // 17
    aiRoute.push_back({startX + 5, startY + 7, DIGGING, 0}); // 18 (4. gyémánt)

    // Hazatérés gyors sebességgel
    aiRoute.push_back({startX + 4, startY + 7, MOVING, 3}); // 19
    aiRoute.push_back({startX + 3, startY + 7, MOVING, 3}); // 20
    aiRoute.push_back({startX + 2, startY + 7, MOVING, 3}); // 21
    aiRoute.push_back({startX + 1, startY + 7, MOVING, 3}); // 22
    aiRoute.push_back({startX + 0, startY + 7, MOVING, 3}); // 23
    aiRoute.push_back({startX + 0, startY + 6, MOVING, 3}); // 24
    aiRoute.push_back({startX + 0, startY + 5, MOVING, 3}); // 25
    aiRoute.push_back({startX + 0, startY + 4, MOVING, 3}); // 26
    
    // Megérkezés
    aiRoute.push_back({startX, startY, STANDING, 0});       // 27
}



int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(1400, 800, "Rover Mission Control v2.0", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    LoadMap("mars_map_50x50.csv");
    InitFakeRoute(); // <-- EZ MARADT KI!
    
    float lastTime = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        vilag.ora += vilag.idosebesseg;
        if (vilag.ora >= 24.0f) vilag.ora = 0.0f;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // // --- INPUT KEZELÉS (Most már a NewFrame után) ---
        // if (ImGui::IsKeyDown(ImGuiKey_0)) { rover.state = STANDING; rover.sebesseg = 0; }
        // if (ImGui::IsKeyDown(ImGuiKey_D)) { rover.state = DIGGING; }
        // if (ImGui::IsKeyDown(ImGuiKey_1)) { rover.state = MOVING; rover.sebesseg = 1; }
        // if (ImGui::IsKeyDown(ImGuiKey_2)) { rover.state = MOVING; rover.sebesseg = 2; }
        // if (ImGui::IsKeyDown(ImGuiKey_3)) { rover.state = MOVING; rover.sebesseg = 3; }
        
        // if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) rover.irany -= 80.0f * deltaTime;
        // if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) rover.irany += 80.0f * deltaTime;

        // Frissítés
        UpdateAILogic();

        // --- UI RAJZOLÁS ---
        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MainControl", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        
        float tw = ImGui::GetContentRegionAvail().x;
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, tw * 0.35f);
        DrawBalPanel();
        ImGui::NextColumn();
        DrawMap();
        
        ImGui::End();

        // Rendering
        ImGui::Render();
        int dw, dh; glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0,0,dw,dh);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}