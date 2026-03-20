#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <deque>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glfw3.h>

struct Cella { char tipus = '.'; };
static const int TERKEP_SZELES = 50;
static const int TERKEP_MAGAS  = 50;
Cella racs[TERKEP_MAGAS][TERKEP_SZELES];

enum RoverAllapot { ALL, MOZOG, BANYASZ };

struct Rover {
    int x = 0, y = 0;
    float akku = 100.0f;
    int zold = 0, sarga = 0, kek = 0;
    int seb = 0;
    float irany = 0.0f;
    float tavolsag = 0.0f;
    RoverAllapot allapot = ALL;
} rover;

struct Vilag {
    float ora = 6.0f;
    float idoseb = 0.001f;
} vilag;

int indX = 0, indY = 0;

struct Lepes {
    int kor;
    int x, y;
    float akku;
    int seb;
    int utDb;
    int osszesMinta;
    int zold, sarga, kek;
    std::string idoszak;
    float pontosIdo;
    std::string allapotSzov;
    RoverAllapot allapot;
};

std::vector<Lepes> utvonal;
int aktualisLepes = 0;
float lepesIdo = 0.0f;

std::vector<ImVec2> nyomvonal;

static bool kuldesVegePopup = false;

static std::deque<std::string> konzolSorok;
static const size_t MAX_KONZOL_SOR = 300;
static bool autoGorget = true;

static int celZold = 0, celSarga = 0, celKek = 0, celOsszes = 0;

static void konzolHozzaad(const std::string& s) {
    konzolSorok.push_back(s);
    if (konzolSorok.size() > MAX_KONZOL_SOR) konzolSorok.pop_front();
}

static void konzolRajzol(float magassag = 140.0f) {
    ImGui::BeginChild("MissionConsole", ImVec2(0, magassag), true);
    ImGui::Text("MISSION CONSOLE");
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoGorget);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) konzolSorok.clear();
    ImGui::Separator();
    for (const auto& sor : konzolSorok)
        ImGui::TextUnformatted(sor.c_str());
    if (autoGorget && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

ImU32 egboltSzin(float h) {
    if (h >= 6.5f  && h < 10.0f) return IM_COL32(255, 180, 100, 255); // hajnal
    if (h >= 10.0f && h < 16.0f) return IM_COL32(100, 180, 255, 255); // deli nap
    if (h >= 16.0f && h < 20.0f) return IM_COL32(255, 100,  50, 255); // alkony
    if (h >= 20.0f && h < 22.5f) return IM_COL32( 40,  40, 100, 255);
    return IM_COL32(10, 10, 25, 255);
}

void terkepBetolt(const std::string& fajl) {
    std::ifstream f(fajl);
    if (!f.is_open()) return;
    std::string sor;
    int y = 0;
    while (std::getline(f, sor) && y < TERKEP_MAGAS) {
        std::stringstream ss(sor);
        std::string c;
        int x = 0;
        while (std::getline(ss, c, ',') && x < TERKEP_SZELES) {
            if (!c.empty()) {
                racs[y][x].tipus = c[0];
                if (c[0] == 'S') {
                    rover.x = x; rover.y = y;
                    indX = x;    indY = y;
                }
            }
            x++;
        }
        y++;
    }
    f.close();
}

void iranytRajzol(ImVec2 p, float sugar) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 kozep = ImVec2(p.x - sugar - 20, p.y + sugar + 20);

    dl->AddCircleFilled(kozep, sugar, IM_COL32(20, 20, 20, 200));
    dl->AddCircle(kozep, sugar, IM_COL32(57, 255, 20, 255), 32, 2.0f);

    float ld = sugar - 8.0f;
    dl->AddText(ImVec2(kozep.x - 5, kozep.y - ld - 5), IM_COL32_WHITE,               "N");
    dl->AddText(ImVec2(kozep.x - 5, kozep.y + ld - 5), IM_COL32(200, 200, 200, 255), "S");
    dl->AddText(ImVec2(kozep.x + ld - 5, kozep.y - 5), IM_COL32(200, 200, 200, 255), "E");
    dl->AddText(ImVec2(kozep.x - ld - 5, kozep.y - 5), IM_COL32(200, 200, 200, 255), "W");

    // 0 fok = Kelet a matban, nekunk 0 = Eszak, ezert -90
    float rad = (rover.irany - 90.0f) * (float)(M_PI / 180.0f);
    ImVec2 hegy = ImVec2(kozep.x + cosf(rad) * (sugar - 5), kozep.y + sinf(rad) * (sugar - 5));
    ImVec2 bal  = ImVec2(kozep.x + cosf(rad + 1.57f) * 4,   kozep.y + sinf(rad + 1.57f) * 4);
    ImVec2 jobb = ImVec2(kozep.x + cosf(rad - 1.57f) * 4,   kozep.y + sinf(rad - 1.57f) * 4);
    dl->AddTriangleFilled(hegy, bal, jobb, IM_COL32(255, 0, 0, 255));
    dl->AddCircleFilled(kozep, 2.0f, IM_COL32_WHITE);
}

void fpvRajzol(ImVec2 meret) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p  = ImGui::GetCursorScreenPos();
    float ido = (float)glfwGetTime();
    ImVec2 kozep = ImVec2(p.x + meret.x * 0.5f, p.y + meret.y * 0.5f);
    float horizY = p.y + meret.y * 0.45f;

    struct Irany { int dx, dy, rx, ry; };
    static const Irany iranyok[] = {
        { 0,-1, 1, 0},  // eszak
        { 1, 0, 0, 1},  // kelet
        { 0, 1,-1, 0},  // del
        {-1, 0, 0,-1}   // nyugat
    };
    int idx = (int)(fmodf(rover.irany + 45.0f, 360.0f) / 90.0f);
    Irany ir = iranyok[idx % 4];

    float razX = 0, razY = 0, intenzitas = 0.0f;
    if (rover.allapot == MOZOG) {
        intenzitas = (float)rover.seb * 2.5f;
        razX = sinf(ido * 20.0f) * (intenzitas * 0.5f);
        razY = cosf(ido * 22.0f) * intenzitas;
    } else if (rover.allapot == BANYASZ) {
        intenzitas = 6.0f;
        razX = sinf(ido * 80.0f) * 1.5f;
        razY = cosf(ido * 85.0f) * 1.5f;
    }

    bool ejjel = (vilag.ora >= 22.0f || vilag.ora < 6.5f);
    ImU32 egSzin = ejjel ? IM_COL32(5, 25, 5, 255) : egboltSzin(vilag.ora);
    dl->AddRectFilled(p, ImVec2(p.x + meret.x, horizY), egSzin);
    dl->AddRectFilled(ImVec2(p.x, horizY), ImVec2(p.x + meret.x, p.y + meret.y), IM_COL32(35, 20, 10, 255));

    // hatulrol elore rajzolunk (painter's algorithm)
    for (int tav = 8; tav >= 1; tav--) {
        for (int oldal = -4; oldal <= 4; oldal++) {
            int cx = rover.x + (ir.dx * tav) + (ir.rx * oldal);
            int cy = rover.y + (ir.dy * tav) + (ir.ry * oldal);
            if (cx < 0 || cx >= TERKEP_SZELES || cy < 0 || cy >= TERKEP_MAGAS) continue;

            char t = racs[cy][cx].tipus;
            if (t == '.' || t == 'S') continue;

            float sk   = 1.0f / (float)tav;
            float xPos = kozep.x + (oldal * (meret.x * 0.45f) * sk) + razX;
            float yPos = horizY  + (meret.y * 0.5f * sk) + razY;
            float bSz  = meret.x * 0.40f * sk;
            float bMag = meret.y * 0.50f * sk;

            if (xPos + bSz/2 < p.x || xPos - bSz/2 > p.x + meret.x) continue;

            if (t == 'G' || t == 'Y' || t == 'B' || t == 'D') {
                ImU32 szin = (t=='G') ? IM_COL32(0,255,0,220) :
                             (t=='Y') ? IM_COL32(255,255,0,220) :
                             (t=='B') ? IM_COL32(0,150,255,220) : IM_COL32(255,255,255,220);
                ImVec2 pts[4] = {
                    ImVec2(xPos,         yPos - bMag),
                    ImVec2(xPos - bSz/3, yPos - bMag/2),
                    ImVec2(xPos,         yPos),
                    ImVec2(xPos + bSz/3, yPos - bMag/2)
                };
                dl->AddConvexPolyFilled(pts, 4, szin);
                dl->AddPolyline(pts, 4, IM_COL32(255,255,255,180), ImDrawFlags_Closed, 1.0f);
            } else if (t == '#') {
                float fade   = 1.0f - ((float)tav / 9.0f);
                ImU32 falSzin = IM_COL32((int)(100*fade),(int)(60*fade),(int)(40*fade),255);
                dl->AddRectFilled(ImVec2(xPos-bSz/2, yPos-bMag), ImVec2(xPos+bSz/2, yPos), falSzin, 2.0f);
                dl->AddRect      (ImVec2(xPos-bSz/2, yPos-bMag), ImVec2(xPos+bSz/2, yPos), IM_COL32(0,0,0,100));
            }
        }
    }

    float aljY  = p.y + meret.y;
    float dashM = 55.0f;

    auto antenna = [&](float xElto, float mag, float fazis) {
        float len  = sinf(ido * 10.0f + fazis) * (intenzitas * 0.5f);
        ImVec2 alap  = ImVec2(p.x + xElto + razX, aljY - dashM);
        ImVec2 hegy2 = ImVec2(p.x + xElto + len + (razX*1.2f), aljY - dashM - mag + razY);
        dl->AddLine(alap, hegy2, IM_COL32(100,100,105,255), 3.0f);
        ImU32 tc = (fmodf(ido,0.4f)>0.2f && rover.allapot!=ALL) ? IM_COL32(255,50,50,255) : IM_COL32(150,0,0,255);
        dl->AddCircleFilled(hegy2, 3.0f, tc);
    };
    antenna(50.0f, 110.0f, 0.0f);
    antenna(meret.x - 50.0f, 90.0f, 1.5f);

    ImVec2 bTL = ImVec2(kozep.x - meret.x*0.22f + razX, aljY - dashM - 25 + razY);
    ImVec2 bTR = ImVec2(kozep.x + meret.x*0.22f + razX, aljY - dashM - 25 + razY);
    ImVec2 bBL = ImVec2(p.x + 30,           aljY);
    ImVec2 bBR = ImVec2(p.x + meret.x - 30, aljY);
    dl->AddQuadFilled(bBL, bBR, bTR, bTL, IM_COL32(35,37,40,255));
    dl->AddQuad      (bBL, bBR, bTR, bTL, IM_COL32(57,255,20,150), 1.5f);

    dl->AddRectFilled(ImVec2(p.x, aljY-dashM), ImVec2(p.x+meret.x, aljY), IM_COL32(20,20,25,255));
    dl->AddLine(ImVec2(p.x, aljY-dashM), ImVec2(p.x+meret.x, aljY-dashM), IM_COL32(80,80,90,255), 3.0f);

    if (rover.allapot == BANYASZ) {
        ImVec2 fb = ImVec2(kozep.x + razX, aljY - dashM - 5 + razY);
        dl->AddLine(ImVec2(kozep.x-30, aljY), fb, IM_COL32(60,60,65,255), 10.0f);
        dl->AddLine(ImVec2(kozep.x+30, aljY), fb, IM_COL32(60,60,65,255), 10.0f);
        for (int i = 0; i < 7; i++) {
            float sp = fmodf(ido*25.0f + (i*0.5f), 1.0f);
            float ty = fb.y - (i*10) - (sp*5);
            float tw = 25.0f - (i*3.5f);
            if (ty < fb.y - 70) continue;
            dl->AddEllipseFilled(ImVec2(fb.x, ty), ImVec2(tw, 4.0f), IM_COL32(180,180,190,255));
        }
    }

    dl->AddRectFilledMultiColor(p, ImVec2(p.x+meret.x, p.y+40),
        IM_COL32(255,255,255,20), IM_COL32(255,255,255,20),
        IM_COL32(255,255,255,0),  IM_COL32(255,255,255,0));
    if (ejjel) dl->AddRectFilled(p, ImVec2(p.x+meret.x, p.y+meret.y), IM_COL32(0,255,0,15));
    dl->AddRect(p, ImVec2(p.x+meret.x, p.y+meret.y), IM_COL32(57,255,20,255), 0, 0, 1.5f);

    iranytRajzol(ImVec2(p.x + meret.x, p.y), 25.0f);
    ImGui::Dummy(meret);
}

void koordinataKoveto(ImVec2 meret) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(p, ImVec2(p.x+meret.x, p.y+meret.y), IM_COL32(10,15,10,255));
    dl->AddRect      (p, ImVec2(p.x+meret.x, p.y+meret.y), IM_COL32(57,255,20,255));

    float mar  = 25.0f;
    float rSz  = meret.x - mar*2;
    float rMag = meret.y - mar*2;
    float skX  = rSz  / 50.0f;
    float skY  = rMag / 50.0f;
    ImVec2 alap = ImVec2(p.x + mar, p.y + mar);

    for (int i = 0; i <= 50; i += 10) {
        float xO = i * skX, yO = i * skY;
        dl->AddLine(ImVec2(alap.x+xO, alap.y), ImVec2(alap.x+xO, alap.y+rMag), IM_COL32(57,255,20,40));
        dl->AddLine(ImVec2(alap.x, alap.y+yO), ImVec2(alap.x+rSz, alap.y+yO),  IM_COL32(57,255,20,40));
        char buf[4]; snprintf(buf, 4, "%d", i);
        dl->AddText(ImVec2(alap.x+xO-5, alap.y+rMag+5), IM_COL32(57,255,20,200), buf);
        dl->AddText(ImVec2(p.x+5, alap.y+yO-7),         IM_COL32(57,255,20,200), buf);
    }

    if (nyomvonal.size() >= 2) {
        for (size_t i = 0; i < nyomvonal.size()-1; i++) {
            ImVec2 p1 = ImVec2(alap.x + nyomvonal[i].x   * skX, alap.y + nyomvonal[i].y   * skY);
            ImVec2 p2 = ImVec2(alap.x + nyomvonal[i+1].x * skX, alap.y + nyomvonal[i+1].y * skY);
            dl->AddLine(p1, p2, IM_COL32(255,165,0,255), 2.0f);
        }
    }

    ImVec2 aktPos = ImVec2(alap.x + rover.x*skX, alap.y + rover.y*skY);
    dl->AddCircleFilled(aktPos, 4.0f, IM_COL32_WHITE);
    dl->AddCircle      (aktPos, 6.0f, IM_COL32(255,255,255,100), 12, 1.0f);
    ImGui::Dummy(meret);
}

void diagramRajzol(ImVec2 meret) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(pos, ImVec2(pos.x+meret.x, pos.y+meret.y), IM_COL32(15,15,15,255));
    float ido = (float)glfwGetTime();

    float freq = 1.0f, amp = 2.0f;
    if (rover.allapot == MOZOG) {
        freq = (rover.seb==1) ? 2.0f : (rover.seb==2) ? 6.0f : 15.0f;
        amp  = meret.y * 0.3f;
    } else if (rover.allapot == BANYASZ) {
        freq = 45.0f; // magas frekvencia furásnal
        amp  = meret.y * 0.15f;
    }

    ImVec2 prev;
    for (int i = 0; i < (int)meret.x; i += 2) {
        float py = pos.y + (meret.y*0.5f) + sinf(ido*10.0f + (i*0.05f*freq)) * amp;
        if (i > 0) dl->AddLine(prev, ImVec2(pos.x+i, py), IM_COL32(57,255,20,255), 1.5f);
        prev = ImVec2(pos.x+i, py);
    }
    ImGui::Dummy(meret);
}

void balPanelRajzol() {
    ImGui::BeginChild("RendszerPanel", ImVec2(0,0), true);
    ImVec2 ter     = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float ido      = (float)glfwGetTime();

    // akkumulátor kördiagram
    ImVec2 cp    = ImGui::GetCursorScreenPos();
    ImVec2 kozep = ImVec2(cp.x + ter.x*0.5f, cp.y + 70);
    float r      = 55.0f;
    bool kritikus  = (rover.akku < 20.0f);
    float villog   = kritikus ? (0.4f + 0.6f*(0.5f + 0.5f*sinf(ido*12.0f))) : 1.0f;
    ImU32 akkuSzin = kritikus ? IM_COL32(255,0,0,(int)(255*villog)) : IM_COL32(57,255,20,255);

    dl->AddCircle(kozep, r, IM_COL32(40,40,40,255), 64, 10.0f);
    dl->PathArcTo(kozep, r, -M_PI/2, -M_PI/2 + (M_PI*2*(rover.akku/100.0f)), 64);
    dl->PathStroke(akkuSzin, 0, 10.0f);

    char akkuStr[16]; snprintf(akkuStr, sizeof(akkuStr), "%d%%", (int)rover.akku);
    ImGui::SetWindowFontScale(1.8f);
    ImVec2 tsz = ImGui::CalcTextSize(akkuStr);
    dl->AddText(ImVec2(kozep.x - tsz.x/2.0f, kozep.y - tsz.y/2.0f),
                kritikus ? akkuSzin : IM_COL32_WHITE, akkuStr);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 140);

    ImGui::Text("POWER ALLOCATION");
    float motorTerh = 0.05f;
    if      (rover.allapot == MOZOG)   motorTerh = (rover.seb==1)?0.40f:(rover.seb==2)?0.70f:0.95f;
    else if (rover.allapot == BANYASZ) motorTerh = 0.85f;

    auto teljesitmenyBar = [&](const char* cimke, float szaz, ImU32 szin) {
        ImGui::Text("%s", cimke); ImGui::SameLine(80);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::ColorConvertU32ToFloat4(szin));
        ImGui::ProgressBar(szaz + sinf(ido*5.0f)*0.01f, ImVec2(-1,10), "");
        ImGui::PopStyleColor();
    };
    teljesitmenyBar("ENG", motorTerh, IM_COL32(57,255,20,200));
    teljesitmenyBar("SEN", 0.20f,     IM_COL32(0,200,255,200));
    teljesitmenyBar("COM", 0.15f,     IM_COL32(255,200,0,200));

    float jel = 0.8f + 0.15f*sinf(ido*0.5f);
    ImGui::Text("COMMS:"); ImGui::SameLine();
    ImGui::ProgressBar(jel, ImVec2(80,15), ""); ImGui::SameLine();
    ImGui::Text("LATENCY: %dms", (int)(420 + sinf(ido)*10));

    ImGui::Separator();

    ImGui::Text("LIVE FEED [HD-X2]");
    fpvRajzol(ImVec2(ter.x, 200));

    ImGui::Text("NAVIGATION GRID");
    koordinataKoveto(ImVec2(ter.x, 180));

    ImGui::Text("VIBRATION ANALYSIS");
    diagramRajzol(ImVec2(ter.x, 60));

    ImGui::BeginChild("Adatok", ImVec2(0,0), true);
    ImGui::Columns(2, "stats", false);
    int h = (int)vilag.ora, perc = (int)((vilag.ora - h)*60);
    ImGui::Text("TIME: %02d:%02d", h, perc);
    ImGui::Text("POS: [%d, %d]", rover.x, rover.y);
    ImGui::NextColumn();
    const char* alTxt = (rover.allapot==ALL) ? "STANDING" : (rover.allapot==MOZOG) ? "MOVING" : "DIGGING";
    ImGui::Text("STATE: %s", alTxt);
    ImGui::Text("DIST: %.1f", rover.tavolsag);
    ImGui::Columns(1);
    ImGui::Separator();

    int mostOsszes = rover.zold + rover.sarga + rover.kek;
    ImGui::Text("TOTAL SAMPLES COLLECTED: %d / %d", mostOsszes, celOsszes);
    ImGui::Spacing();
    ImGui::Columns(3, "res_cols", false);

    // egy oszlopot rajzol az adott nyersanyagbol
    auto nyersanyagBar = [&](int db, ImU32 szin, const char* cimke) {
        ImVec2 bp  = ImGui::GetCursorScreenPos();
        float maxM = 85.0f, szeles = 45.0f;
        float arany = (celOsszes > 0) ? (float)db / (float)celOsszes : 0.0f;
        float toltM = maxM * arany;

        ImGui::GetWindowDrawList()->AddRectFilled(bp, ImVec2(bp.x+szeles, bp.y+maxM), IM_COL32(30,30,30,255));
        if (toltM > 0.0f)
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(bp.x, bp.y+maxM-toltM), ImVec2(bp.x+szeles, bp.y+maxM), szin);

        char pct[16]; snprintf(pct, sizeof(pct), "%d%%", (int)(arany*100.0f));
        ImVec2 ts2 = ImGui::CalcTextSize(pct);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(bp.x+(szeles-ts2.x)*0.5f, bp.y-ts2.y-2.0f), IM_COL32(200,200,200,255), pct);

        ImGui::Dummy(ImVec2(szeles, maxM+8));
        ImGui::Text("%s: %d / %d", cimke, db, celOsszes);
    };

    nyersanyagBar(rover.kek,   IM_COL32(0,120,255,255), "BLU"); ImGui::NextColumn();
    nyersanyagBar(rover.sarga, IM_COL32(255,255,0,255), "YLW"); ImGui::NextColumn();
    nyersanyagBar(rover.zold,  IM_COL32(0,255,0,255),   "GRN"); ImGui::NextColumn();

    ImGui::Columns(1);
    ImGui::EndChild();
    ImGui::EndChild();
}

void terkepRajzol() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::BeginChild("Terkep", ImVec2(0,0), false, ImGuiWindowFlags_NoScrollbar);

    ImVec2 elerheto = ImGui::GetContentRegionAvail();
    float oldal = std::min(elerheto.x, elerheto.y);
    float cs    = oldal / 50.0f;

    ImGui::Dummy(ImVec2(oldal, oldal));
    ImVec2 p = ImGui::GetItemRectMin();
    p.x += (elerheto.x - oldal) / 2.0f;
    p.y += (elerheto.y - oldal) / 2.0f;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int y = 0; y < 50; y++) {
        for (int x = 0; x < 50; x++) {
            char t = racs[y][x].tipus;
            ImU32 szin;
            if      (t=='S') szin = IM_COL32(255,255,255,255);
            else if (t=='G') szin = IM_COL32(0,255,0,255);
            else if (t=='Y') szin = IM_COL32(255,255,0,255);
            else if (t=='B') szin = IM_COL32(0,120,255,255);
            else if (t=='#') szin = IM_COL32(80,40,30,255);
            else             szin = IM_COL32(140+(x%2*5),60,35,255);
            dl->AddRectFilled(ImVec2(p.x+x*cs,p.y+y*cs), ImVec2(p.x+(x+1)*cs,p.y+(y+1)*cs), szin);
        }
    }

    // AI utvonal kirajzolasa
    if (!utvonal.empty()) {
        ImVec2 kezdo = ImVec2(p.x + indX*cs + cs/2.0f, p.y + indY*cs + cs/2.0f);
        dl->AddCircleFilled(kezdo, cs*0.4f, IM_COL32(255,50,50,255));
        ImVec2 elozo = kezdo;

        int maxIdx = std::min(aktualisLepes, (int)utvonal.size());
        for (int i = 0; i < maxIdx; i++) {
            ImVec2 kov = ImVec2(p.x + utvonal[i].x*cs + cs/2.0f, p.y + utvonal[i].y*cs + cs/2.0f);
            dl->AddLine(elozo, kov, IM_COL32(255,140,0,200), 2.5f);
            dl->AddCircleFilled(kov, cs*0.15f, IM_COL32(255,200,0,255));

            if (utvonal[i].allapot == BANYASZ) {
                dl->AddLine(ImVec2(kov.x-cs*0.3f,kov.y-cs*0.3f), ImVec2(kov.x+cs*0.3f,kov.y+cs*0.3f), IM_COL32(255,50,50,255), 2.0f);
                dl->AddLine(ImVec2(kov.x+cs*0.3f,kov.y-cs*0.3f), ImVec2(kov.x-cs*0.3f,kov.y+cs*0.3f), IM_COL32(255,50,50,255), 2.0f);
            }
            elozo = kov;
        }
    }

    // rover pozicio a terkepen
    ImVec2 rPos = ImVec2(p.x + rover.x*cs + cs/2, p.y + rover.y*cs + cs/2);
    float rMer  = cs * 1.2f;
    float rad   = (rover.irany - 90.0f) * (float)(M_PI / 180.0f);
    ImVec2 rp1  = ImVec2(rPos.x + cosf(rad)*rMer,        rPos.y + sinf(rad)*rMer);
    ImVec2 rp2  = ImVec2(rPos.x + cosf(rad+2.3f)*rMer,   rPos.y + sinf(rad+2.3f)*rMer);
    ImVec2 rp3  = ImVec2(rPos.x + cosf(rad-2.3f)*rMer,   rPos.y + sinf(rad-2.3f)*rMer);

    dl->AddCircleFilled(rPos, rMer*1.5f, IM_COL32(255,255,255,60));
    dl->AddTriangleFilled(rp1, rp2, rp3, IM_COL32_WHITE);
    dl->AddTriangle      (rp1, rp2, rp3, IM_COL32_BLACK, 1.5f);
    dl->AddRect(p, ImVec2(p.x+oldal, p.y+oldal), IM_COL32(57,255,20,255), 0, 0, 2.0f);

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void aiLepes(float dt) {
    if (utvonal.empty() || aktualisLepes >= (int)utvonal.size()) {
        rover.allapot = ALL;
        return;
    }

    lepesIdo += dt;
    const float LEPES_MASODPERC = 3.0f;
    if (lepesIdo < LEPES_MASODPERC) return;
    lepesIdo = 0.0f;

    Lepes& l = utvonal[aktualisLepes];

    {
        std::ostringstream uzenet;
        uzenet << "[#" << l.kor << "] ";
        if      (l.allapot == MOZOG)   uzenet << "MOZOG -> (" << l.x << "," << l.y << ") seb=" << l.seb;
        else if (l.allapot == BANYASZ) uzenet << "BANYASZ @ (" << l.x << "," << l.y << ")";
        else                           uzenet << "ALL @ (" << l.x << "," << l.y << ")";
        uzenet << " | akku=" << (int)l.akku << "% | t=" << l.pontosIdo;
        konzolHozzaad(uzenet.str());
    }

    int dx = l.x - rover.x, dy = l.y - rover.y;
    if (dx != 0 || dy != 0) {
        float fok = atan2f((float)dy, (float)dx) * 180.0f / (float)M_PI;
        rover.irany = fok + 90.0f;
        if (rover.irany <    0.0f) rover.irany += 360.0f;
        if (rover.irany >= 360.0f) rover.irany -= 360.0f;
    }

    rover.x       = l.x;      rover.y       = l.y;
    rover.akku    = l.akku;   rover.allapot = l.allapot;
    rover.seb     = l.seb;    rover.zold    = l.zold;
    rover.sarga   = l.sarga;  rover.kek     = l.kek;
    rover.tavolsag = (float)l.utDb;
    vilag.ora     = l.pontosIdo;

    ImVec2 ujPoz((float)rover.x, (float)rover.y);
    if (nyomvonal.empty() || nyomvonal.back().x != ujPoz.x || nyomvonal.back().y != ujPoz.y)
        nyomvonal.push_back(ujPoz);

    aktualisLepes++;
}

void utvonalBetolt(const std::string& fajl) {
    std::ifstream f(fajl);
    if (!f.is_open()) return;

    utvonal.clear();
    aktualisLepes = 0;
    lepesIdo      = 0.0f;
    nyomvonal.clear();

    std::string sor;
    while (std::getline(f, sor)) {
        if (sor.empty()) continue;
        std::stringstream ss(sor);
        std::string c;
        std::vector<std::string> mezok;
        while (std::getline(ss, c, ',')) mezok.push_back(c);

        if (mezok.size() >= 13) {
            try {
                Lepes l;
                l.kor         = std::stoi(mezok[0]);
                if (l.kor < 0) continue;
                l.x           = std::stoi(mezok[1]);
                l.y           = std::stoi(mezok[2]);
                l.akku        = std::stof(mezok[3]);
                l.seb         = std::stoi(mezok[4]);
                l.utDb        = std::stoi(mezok[5]);
                l.osszesMinta = std::stoi(mezok[6]);
                l.zold        = std::stoi(mezok[7]);
                l.sarga       = std::stoi(mezok[8]);
                l.kek         = std::stoi(mezok[9]);
                l.idoszak     = mezok[10];
                l.pontosIdo   = std::stof(mezok[11]);
                l.allapotSzov = mezok[12];

                if      (l.allapotSzov == "MOVING")  l.allapot = MOZOG;
                else if (l.allapotSzov == "DIGGING") l.allapot = BANYASZ;
                else                                 l.allapot = ALL;

                utvonal.push_back(l);
            } catch (...) { continue; }
        }
    }
    f.close();

    if (!utvonal.empty()) {
        const Lepes& ut = utvonal.back();
        celZold   = ut.zold;
        celSarga  = ut.sarga;
        celKek    = ut.kek;
        celOsszes = celZold + celSarga + celKek;
    } else {
        celZold = celSarga = celKek = celOsszes = 0;
    }

    nyomvonal.push_back(ImVec2((float)indX, (float)indY));
    rover.x = indX; rover.y = indY;
    rover.akku = 100.0f;
    rover.zold = rover.sarga = rover.kek = 0;
    rover.seb  = 0; rover.irany = 0.0f; rover.tavolsag = 0.0f;
    rover.allapot = ALL;
    vilag.ora = 6.0f;
}

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* ablak = glfwCreateWindow(1400, 800, "Rover Mission Control v2.0", NULL, NULL);
    glfwMakeContextCurrent(ablak);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(ablak, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    terkepBetolt("data/mars_map_50x50.csv");
    utvonalBetolt("output/ai_route.txt");

    nyomvonal.clear();
    nyomvonal.push_back(ImVec2((float)rover.x, (float)rover.y));

    float utolsoIdo = (float)glfwGetTime();

    while (!glfwWindowShouldClose(ablak)) {
        glfwPollEvents();
        float mostIdo = (float)glfwGetTime();
        float dt      = mostIdo - utolsoIdo;
        utolsoIdo     = mostIdo;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        aiLepes(dt);

        if (!utvonal.empty() && aktualisLepes >= (int)utvonal.size()) {
            if (!kuldesVegePopup) {
                kuldesVegePopup = true;
                ImGui::OpenPopup("Mission Complete");
            }
        }

        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MainControl", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(57,255,20,255));
        ImGui::SetWindowFontScale(1.5f);
        float ablakSz  = ImGui::GetWindowSize().x;
        float szovegSz = ImGui::CalcTextSize("PROJECT: ARES - MARS ROVER MISSION CONTROL").x;
        ImGui::SetCursorPosX((ablakSz - szovegSz) * 0.5f);
        ImGui::Text("PROJECT: ARES - MARS ROVER MISSION CONTROL");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        float teljesSz = ImGui::GetContentRegionAvail().x;
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, teljesSz * 0.35f);
        balPanelRajzol();
        ImGui::NextColumn();
        terkepRajzol();
        ImGui::Columns(1);
        ImGui::Separator();
        konzolRajzol(160.0f);

        ImGui::End();

        if (kuldesVegePopup) {
            ImVec2 kozep = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(kozep, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(450,220), ImGuiCond_Appearing);

            if (ImGui::BeginPopupModal("Mission Complete", NULL,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
            {
                bool hazaert = false;
                if (!utvonal.empty()) {
                    const Lepes& ut = utvonal.back();
                    if (ut.y>=0 && ut.y<TERKEP_MAGAS && ut.x>=0 && ut.x<TERKEP_SZELES)
                        hazaert = (racs[ut.y][ut.x].tipus == 'S');
                }

                if (hazaert)
                    ImGui::TextWrapped("A rover korbeerett es visszaert a bazisra (S).");
                else
                    ImGui::TextWrapped("A lejatstszas veget ert. (A rover nem feltetlenul ert vissza az S-re.)");

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                int ossz = rover.zold + rover.sarga + rover.kek;
                ImGui::Text("Osszes minta: %d  (GRN=%d, YLW=%d, BLU=%d)", ossz, rover.zold, rover.sarga, rover.kek);
                ImGui::Text("Akkumulator: %d%%", (int)rover.akku);
                ImGui::Text("Utolso pozicio: [%d, %d]", rover.x, rover.y);
                ImGui::Spacing(); ImGui::Spacing();

                if (ImGui::Button("Kilepes", ImVec2(140,0))) glfwSetWindowShouldClose(ablak, 1);
                ImGui::SameLine();
                if (ImGui::Button("Bezar", ImVec2(140,0))) {
                    kuldesVegePopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        ImGui::Render();
        int sw, sh;
        glfwGetFramebufferSize(ablak, &sw, &sh);
        glViewport(0, 0, sw, sh);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(ablak);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(ablak);
    glfwTerminate();
    return 0;
}
