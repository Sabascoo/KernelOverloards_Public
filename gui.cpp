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




// Represents one action/step in the AI's route with all 13 data points
struct RouteStep {
    int round;
    int x, y;
    float battery;
    int speed;
    int pathCount;
    int totalMinerals;
    int green, yellow, blue;
    std::string timePeriod;
    float exactTime;
    std::string stateStr;
    RoverState state; // Helper to map the string back to enum
};

// Global variables for the AI routing
std::vector<RouteStep> aiRoute;
int currentRouteIndex = 0;
float timeSinceLastStep = 0.0f; 
const float STEP_INTERVAL = 0.5f; // 0.5 simulation hours per round

// This holds the "breadcrumb trail" of coordinates
std::vector<ImVec2> roverHistory;

enum CameraView { VIEW_TOP, VIEW_BACK, VIEW_SIDE };
CameraView currentView = VIEW_TOP;

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
    // Center the compass in the top right of the FPV area
    ImVec2 center = ImVec2(p.x - sugar - 20, p.y + sugar + 20);
    
    // Outer Ring
    drawList->AddCircleFilled(center, sugar, IM_COL32(20, 20, 20, 200));
    drawList->AddCircle(center, sugar, IM_COL32(57, 255, 20, 255), 32, 2.0f);
    
    // Compass Rose Markers (N, S, E, W)
    float labelDist = sugar - 8.0f; //8.0f;
    drawList->AddText(ImVec2(center.x - 5, center.y - labelDist - 5), IM_COL32_WHITE, "N");
    drawList->AddText(ImVec2(center.x - 5, center.y + labelDist - 5), IM_COL32(200, 200, 200, 255), "S");
    drawList->AddText(ImVec2(center.x + labelDist - 5, center.y - 5), IM_COL32(200, 200, 200, 255), "E");
    drawList->AddText(ImVec2(center.x - labelDist - 5, center.y - 5), IM_COL32(200, 200, 200, 255), "W");

    // Direction Needle
    // Subtracting 90 degrees because 0 rad points Right (East) in math, but we want 0 deg to be Up (North)
    float rad = (rover.irany - 90.0f) * (float)(M_PI / 180.0f);
    ImVec2 needleTip = ImVec2(center.x + cosf(rad) * (sugar - 5), center.y + sinf(rad) * (sugar - 5));
    ImVec2 needleBaseLeft = ImVec2(center.x + cosf(rad + 1.57f) * 4, center.y + sinf(rad + 1.57f) * 4);
    ImVec2 needleBaseRight = ImVec2(center.x + cosf(rad - 1.57f) * 4, center.y + sinf(rad - 1.57f) * 4);

    drawList->AddTriangleFilled(needleTip, needleBaseLeft, needleBaseRight, IM_COL32(255, 0, 0, 255));
    drawList->AddCircleFilled(center, 2.0f, IM_COL32_WHITE);
}

void DrawFPV(ImVec2 size) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float time = (float)glfwGetTime();
    ImVec2 center = ImVec2(p.x + size.x * 0.5f, p.y + size.y * 0.5f);
    float horizonY = p.y + size.y * 0.45f;

    // --- 1. OPTIMIZED DIRECTION VECTORS ---
    // Define a lookup for N, E, S, W (0, 90, 180, 270 degrees)
    struct Dir { int dx, dy, rx, ry; };
    static const Dir dirs[] = {
        {0, -1, 1, 0},  // North (0 deg)
        {1, 0, 0, 1},   // East  (90 deg)
        {0, 1, -1, 0},  // South (180 deg)
        {-1, 0, 0, -1}  // West  (270 deg)
    };

    int dirIdx = (int)(fmodf(rover.irany + 45.0f, 360.0f) / 90.0f);
    Dir d = dirs[dirIdx % 4];

    // --- 2. DYNAMIC SHAKE LOGIC ---
    float shakeX = 0, shakeY = 0;
    float intensity = 0.0f;

    if (rover.state == MOVING) {
        intensity = (float)rover.sebesseg * 2.5f; // Scale by speed
        shakeX = sinf(time * 20.0f) * (intensity * 0.5f);
        shakeY = cosf(time * 22.0f) * intensity;
    } else if (rover.state == DIGGING) {
        intensity = 6.0f; // High intensity for drilling
        shakeX = sinf(time * 80.0f) * 1.5f;
        shakeY = cosf(time * 85.0f) * 1.5f;
    }

    // --- 3. SKY AND GROUND ---
    bool ejjellato = (vilag.ora >= 22.0f || vilag.ora < 6.5f);
    ImU32 egSzin = ejjellato ? IM_COL32(5, 25, 5, 255) : GetEgboltSzin(vilag.ora);
    drawList->AddRectFilled(p, ImVec2(p.x + size.x, horizonY), egSzin);
    drawList->AddRectFilled(ImVec2(p.x, horizonY), ImVec2(p.x + size.x, p.y + size.y), IM_COL32(35, 20, 10, 255));

    // --- 4. PERSPEKTIVIKUS BLOKKOK ÉS GYÉMÁNTOK (8 UNIT RANGE) ---
    // We render from distance 8 down to 1 (Back to Front)
    for (int dist = 8; dist >= 1; dist--) {
    for (int side = -4; side <= 4; side++) { 
        // USE THE STRUCT MEMBERS HERE:
        int cx = rover.x + (d.dx * dist) + (d.rx * side);
        int cy = rover.y + (d.dy * dist) + (d.ry * side);

            // Boundary check
            if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) continue;
            
            char type = mapGrid[cy][cx].type;
            if (type == '.' || type == 'S') continue;

            // Simple linear scale (closer to your original version)
            float scale = 1.0f / (float)dist; 
            
            // Horizontal spread: scale side position by window width
            float xPos = center.x + (side * (size.x * 0.45f) * scale) + shakeX;
            
            // Vertical position: Ensure they sit on the horizon
            // We use a fixed multiplier (0.5) to keep them aligned with the ground
            float yPos = horizonY + (size.y * 0.5f * scale) + shakeY; 
            
            // Block size
            float bW = size.x * 0.40f * scale;
            float bH = size.y * 0.50f * scale;

            // Draw only if it is within the FPV window bounds
            if (xPos + bW/2 < p.x || xPos - bW/2 > p.x + size.x) continue;

            if (type == 'G' || type == 'Y' || type == 'B' || type == 'D') {
                // --- DIAMOND DRAWING ---
                ImU32 dCol = (type == 'G') ? IM_COL32(0, 255, 0, 220) : 
                            (type == 'Y') ? IM_COL32(255, 255, 0, 220) : 
                            (type == 'B') ? IM_COL32(0, 150, 255, 220) : IM_COL32(255, 255, 255, 220);
                
                ImVec2 pts[4] = {
                    ImVec2(xPos, yPos - bH),          // Top
                    ImVec2(xPos - bW/3, yPos - bH/2), // Left
                    ImVec2(xPos, yPos),               // Bottom
                    ImVec2(xPos + bW/3, yPos - bH/2)  // Right
                };
                drawList->AddConvexPolyFilled(pts, 4, dCol);
                drawList->AddPolyline(pts, 4, IM_COL32(255, 255, 255, 180), ImDrawFlags_Closed, 1.0f);
                
            } else if (type == '#') {
                // --- WALL DRAWING ---
                // Fade color based on distance (Atmospheric perspective)
                float fade = 1.0f - ((float)dist / 9.0f); 
                ImU32 wallCol = IM_COL32((int)(100 * fade), (int)(60 * fade), (int)(40 * fade), 255);
                
                drawList->AddRectFilled(ImVec2(xPos - bW/2, yPos - bH), ImVec2(xPos + bW/2, yPos), wallCol, 2.0f);
                drawList->AddRect(ImVec2(xPos - bW/2, yPos - bH), ImVec2(xPos + bW/2, yPos), IM_COL32(0, 0, 0, 100));
            }
        }
    }

    // --- 5. ENHANCED ROVER CHASSIS & ANTENNAS ---
    float roverBottom = p.y + size.y;
    float dashH = 55.0f; 

    auto DrawAntenna = [&](float xOff, float height, float swayOffset) {
        float sway = sinf(time * 10.0f + swayOffset) * (intensity * 0.5f);
        ImVec2 base = ImVec2(p.x + xOff + shakeX, roverBottom - dashH);
        ImVec2 tip = ImVec2(p.x + xOff + sway + (shakeX * 1.2f), roverBottom - dashH - height + shakeY);
        drawList->AddLine(base, tip, IM_COL32(100, 100, 105, 255), 3.0f);
        ImU32 tipCol = (fmodf(time, 0.4f) > 0.2f && (rover.state != STANDING)) ? IM_COL32(255, 50, 50, 255) : IM_COL32(150, 0, 0, 255);
        drawList->AddCircleFilled(tip, 3.0f, tipCol);
    };

    DrawAntenna(50.0f, 110.0f, 0.0f);
    DrawAntenna(size.x - 50.0f, 90.0f, 1.5f);

    ImVec2 hTL = ImVec2(center.x - size.x * 0.22f + shakeX, roverBottom - dashH - 25 + shakeY);
    ImVec2 hTR = ImVec2(center.x + size.x * 0.22f + shakeX, roverBottom - dashH - 25 + shakeY);
    ImVec2 hBL = ImVec2(p.x + 30, roverBottom);
    ImVec2 hBR = ImVec2(p.x + size.x - 30, roverBottom);

    drawList->AddQuadFilled(hBL, hBR, hTR, hTL, IM_COL32(35, 37, 40, 255));
    drawList->AddQuad(hBL, hBR, hTR, hTL, IM_COL32(57, 255, 20, 150), 1.5f);

    // Dashboard
    drawList->AddRectFilled(ImVec2(p.x, roverBottom - dashH), ImVec2(p.x + size.x, roverBottom), IM_COL32(20, 20, 25, 255));
    drawList->AddLine(ImVec2(p.x, roverBottom - dashH), ImVec2(p.x + size.x, roverBottom - dashH), IM_COL32(80, 80, 90, 255), 3.0f);

    // --- 6. DRILL ASSEMBLY ---
    if (rover.state == DIGGING) {
        ImVec2 dBase = ImVec2(center.x + shakeX, roverBottom - dashH - 5 + shakeY);
        drawList->AddLine(ImVec2(center.x - 30, roverBottom), dBase, IM_COL32(60, 60, 65, 255), 10.0f);
        drawList->AddLine(ImVec2(center.x + 30, roverBottom), dBase, IM_COL32(60, 60, 65, 255), 10.0f);
        for (int i = 0; i < 7; i++) {
            float spiral = fmodf(time * 25.0f + (i * 0.5f), 1.0f);
            float ty = dBase.y - (i * 10) - (spiral * 5);
            float tw = 25.0f - (i * 3.5f);
            if (ty < dBase.y - 70) continue;
            drawList->AddEllipseFilled(ImVec2(dBase.x, ty), ImVec2(tw, 4.0f), IM_COL32(180, 180, 190, 255));
        }
    }

    // --- HUD & REFLECTION ---
    drawList->AddRectFilledMultiColor(p, ImVec2(p.x + size.x, p.y + 40), IM_COL32(255,255,255,20), IM_COL32(255,255,255,20), IM_COL32(255,255,255,0), IM_COL32(255,255,255,0));
    if (ejjellato) drawList->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(0, 255, 0, 15));
    drawList->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(57, 255, 20, 255), 0, 0, 1.5f);

    // --- COMPASS OVERLAY ---
    // Pass the top-right corner of the FPV box (p.x + size.x)
    DrawIranytu(ImVec2(p.x + size.x, p.y), 25.0f);

    ImGui::Dummy(size);
}

void DrawCoordinateTracker(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    
    // Background and Border
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(10, 15, 10, 255));
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(57, 255, 20, 255));

    float margin = 25.0f; // Increased slightly for text labels
    float gridWidth = size.x - (margin * 2);
    float gridHeight = size.y - (margin * 2);
    
    float scaleX = gridWidth / 50.0f;
    float scaleY = gridHeight / 50.0f;
    
    ImVec2 mapBase = ImVec2(p.x + margin, p.y + margin);

    // Draw Grid Lines and Coordinate Labels
    for (int i = 0; i <= 50; i += 10) {
        float xOff = i * scaleX;
        float yOff = i * scaleY;
        
        // Lines
        dl->AddLine(ImVec2(mapBase.x + xOff, mapBase.y), ImVec2(mapBase.x + xOff, mapBase.y + gridHeight), IM_COL32(57, 255, 20, 40));
        dl->AddLine(ImVec2(mapBase.x, mapBase.y + yOff), ImVec2(mapBase.x + gridWidth, mapBase.y + yOff), IM_COL32(57, 255, 20, 40));

        // Labels
        char buf[4]; snprintf(buf, 4, "%d", i);
        dl->AddText(ImVec2(mapBase.x + xOff - 5, mapBase.y + gridHeight + 5), IM_COL32(57, 255, 20, 200), buf); // X labels
        dl->AddText(ImVec2(p.x + 5, mapBase.y + yOff - 7), IM_COL32(57, 255, 20, 200), buf);                   // Y labels
    }

    // Connect Logged Points (The Trail)
    if (roverHistory.size() >= 2) {
        for (size_t i = 0; i < roverHistory.size() - 1; i++) {
            ImVec2 p1 = ImVec2(mapBase.x + roverHistory[i].x * scaleX, mapBase.y + roverHistory[i].y * scaleY);
            ImVec2 p2 = ImVec2(mapBase.x + roverHistory[i+1].x * scaleX, mapBase.y + roverHistory[i+1].y * scaleY);
            
            // Draw path line
            dl->AddLine(p1, p2, IM_COL32(255, 165, 0, 255), 2.0f);
        }
    }

    // Draw Current Position Dot
    ImVec2 currentPos = ImVec2(mapBase.x + (float)rover.x * scaleX, mapBase.y + (float)rover.y * scaleY);
    dl->AddCircleFilled(currentPos, 4.0f, IM_COL32_WHITE);
    dl->AddCircle(currentPos, 6.0f, IM_COL32(255, 255, 255, 100), 12, 1.0f); // Pulsing ring effect

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

    // --- VIEW SELECTOR ---

    if (ImGui::RadioButton("TOP", currentView == VIEW_TOP)) currentView = VIEW_TOP; ImGui::SameLine();
    if (ImGui::RadioButton("BACK", currentView == VIEW_BACK)) currentView = VIEW_BACK; ImGui::SameLine();
    if (ImGui::RadioButton("SIDE", currentView == VIEW_SIDE)) currentView = VIEW_SIDE;

    // --- 3. FPV CAMERA ---
    ImGui::Text("LIVE FEED [HD-X2]");
    DrawFPV(ImVec2(region.x, 200));

    // --- 4. COMMS STATUS ---
    ImGui::Separator();
    float signal = 0.8f + 0.15f * sinf(time * 0.5f);
    ImGui::Text("COMMS:"); ImGui::SameLine();
    ImGui::ProgressBar(signal, ImVec2(80, 15), ""); ImGui::SameLine();
    ImGui::Text("LATENCY: %dms", (int)(420 + sinf(time) * 10));

    //NEW COORDINATE TRACKER (Placed after FPV) ---
    ImGui::Text("NAVIGATION GRID (REL)");
    DrawCoordinateTracker(ImVec2(region.x, 180)); // Fixed height of 180

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

    // --- Inside DrawFPV function ---


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

    // --- 3. RÉTEG: DYNAMIC ROVER DRAWING ---
    ImVec2 rPos = ImVec2(p.x + rover.x * cs + cs/2, p.y + rover.y * cs + cs/2);
    float rSize = cs * 1.2f;
    float shake = (rover.state != STANDING) ? sinf((float)glfwGetTime() * 25.0f) * 0.8f : 0.0f;

    // Glow effect
    dl->AddCircleFilled(rPos, rSize * 1.5f, IM_COL32(255, 255, 255, 60));

    switch (currentView) {
        case VIEW_TOP: {
            float rad = (rover.irany - 90.0f) * (M_PI / 180.0f);
            ImVec2 p1 = ImVec2(rPos.x + cosf(rad) * rSize, rPos.y + sinf(rad) * rSize);
            ImVec2 p2 = ImVec2(rPos.x + cosf(rad + 2.3f) * rSize, rPos.y + sinf(rad + 2.3f) * rSize);
            ImVec2 p3 = ImVec2(rPos.x + cosf(rad - 2.3f) * rSize, rPos.y + sinf(rad - 2.3f) * rSize);
            dl->AddTriangleFilled(p1, p2, p3, IM_COL32_WHITE);
            dl->AddTriangle(p1, p2, p3, IM_COL32_BLACK, 1.5f);
            break;
        }
        case VIEW_BACK: {
            // Chase view representation
            dl->AddRectFilled(ImVec2(rPos.x - cs, rPos.y - cs/2 + shake), ImVec2(rPos.x + cs, rPos.y + cs/2 + shake), IM_COL32(180, 180, 180, 255));
            dl->AddRectFilled(ImVec2(rPos.x - cs, rPos.y + cs/4 + shake), ImVec2(rPos.x - cs/2, rPos.y + cs/2 + shake), IM_COL32(255, 0, 0, 255)); // Tail light
            break;
        }
        case VIEW_SIDE: {
            // Profile view representation
            dl->AddRectFilled(ImVec2(rPos.x - cs, rPos.y - cs + shake), ImVec2(rPos.x + cs, rPos.y + shake), IM_COL32(160, 160, 160, 255));
            dl->AddCircleFilled(ImVec2(rPos.x - cs/1.5f, rPos.y + cs/4 + shake), cs/3, IM_COL32_BLACK); // Wheel
            dl->AddCircleFilled(ImVec2(rPos.x + cs/1.5f, rPos.y + cs/4 + shake), cs/3, IM_COL32_BLACK); // Wheel
            break;
        }
    }

    // --- 4. RÉTEG: KERET ---
    dl->AddRect(p, ImVec2(p.x+side, p.y+side), IM_COL32(57, 255, 20, 255), 0, 0, 2.0f);
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
}


void UpdateAILogic() {
    if (aiRoute.empty() || currentRouteIndex >= (int)aiRoute.size()) {
        rover.state = STANDING;
        return;
    }

    // Controls how fast the simulation plays back in the GUI
    // 0.016f assumes 60FPS. Increase the 0.1f threshold to slow it down.
    timeSinceLastStep += 0.016f; 

    if (timeSinceLastStep >= 5.0f) {  //change 0.1f to higher value to set a higher second
        timeSinceLastStep = 0.0f;
        
        RouteStep& s = aiRoute[currentRouteIndex];

        // Update direction based on movement
        if (s.x > rover.x) rover.irany = 90.0f;
        else if (s.x < rover.x) rover.irany = 270.0f;
        else if (s.y > rover.y) rover.irany = 180.0f;
        else if (s.y < rover.y) rover.irany = 0.0f;

        // Sync Rover Object
        rover.x = s.x;
        rover.y = s.y;
        rover.battery = s.battery;
        rover.state = s.state;
        rover.sebesseg = s.speed;
        rover.zold = s.green;
        rover.sarga = s.yellow;
        rover.kek = s.blue;
        rover.tavolsag = (float)s.pathCount;

        // Sync Global World Time
        vilag.ora = s.exactTime;

        // Update Nav Trail
        ImVec2 newPos = ImVec2((float)rover.x, (float)rover.y);
        if (roverHistory.empty() || roverHistory.back().x != newPos.x || roverHistory.back().y != newPos.y) {
            roverHistory.push_back(newPos);
        }

        currentRouteIndex++;
    }
}

void LoadAIRoute(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;

    aiRoute.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue; 

        std::stringstream ss(line);
        std::string cell;
        RouteStep step;
        std::vector<std::string> row;

        while (std::getline(ss, cell, ',')) {
            row.push_back(cell);
        }

        if (row.size() >= 13) {
            try {
                step.round = std::stoi(row[0]);
                step.x = std::stoi(row[1]);
                step.y = std::stoi(row[2]);
                step.battery = std::stof(row[3]);
                step.speed = std::stoi(row[4]);
                step.pathCount = std::stoi(row[5]);
                step.totalMinerals = std::stoi(row[6]);
                step.green = std::stoi(row[7]);
                step.yellow = std::stoi(row[8]);
                step.blue = std::stoi(row[9]);
                step.timePeriod = row[10];
                step.exactTime = std::stof(row[11]); // Correct conversion
                step.stateStr = row[12];

                if (step.stateStr == "MOVING") step.state = MOVING;
                else if (step.stateStr == "DIGGING") step.state = DIGGING;
                else step.state = STANDING;

                aiRoute.push_back(step);
            } catch (...) {
                continue; // Skip lines with bad data
            }
        }
    }
    file.close();
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
    LoadAIRoute("ai_route.txt");

    float lastTime = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        float currentTime = (float)glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        vilag.ora += vilag.idosebesseg;
        if (vilag.ora >= 24.0f) vilag.ora = 0.0f;

        // 1. START THE FRAME
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        UpdateAILogic();

        // 2. SETUP THE MAIN WINDOW
        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MainControl", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        
        // --- PROJECT TITLE (MOVE THIS HERE) ---
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(57, 255, 20, 255)); 
        ImGui::SetWindowFontScale(1.5f);
        
        float windowWidth = ImGui::GetWindowSize().x;
        float textWidth = ImGui::CalcTextSize("PROJECT: ARES - MARS ROVER MISSION CONTROL").x;
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f); 
        
        ImGui::Text("PROJECT: ARES - MARS ROVER MISSION CONTROL");
        
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();
        // --------------------------------------

        // 3. COLUMNS AND PANELS
        float totalWidth = ImGui::GetContentRegionAvail().x;
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, totalWidth * 0.35f);
        
        DrawBalPanel();
        
        ImGui::NextColumn();
        
        DrawMap();
        
        ImGui::End(); // End MainControl

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