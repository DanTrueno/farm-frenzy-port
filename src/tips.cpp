#include "tips.h"
#include "portable.h"
#include "game.h"
#include "screen.h"
#include "ui.h"
#include <cmath>
#include <fstream>
#include <map>

static std::map<int, std::string> g_tipText;

void tipsLoad(const std::string& dataDir) {
    std::ifstream f(u8path(dataDir + "/tips.txt"));
    std::string line;
    while (std::getline(f, line)) {
        size_t a = line.find('\t'), b = line.find('\t', a + 1);
        if (a == std::string::npos || b == std::string::npos) continue;
        std::string s = line.substr(b + 1), r;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') { r += '\n'; ++i; }
            else r += s[i];
        }
        g_tipText[atoi(line.c_str())] = tr(r);
    }
}

static Profile* prof() { return g_profiles.current(); }

bool tipAvailable(int id) {
    Profile* p = prof();
    if (!p || id < 0 || id >= TIP_COUNT || p->tipSeen[id]) return false;
    return id == TIP_WATER || g_tipText.count(id);
}

void tipMarkSeen(int id) {
    if (Profile* p = prof()) p->tipSeen[id] = 1;
}

static std::string trimText(std::string s) {
    while (!s.empty() && (s[0] == '\n' || s[0] == ' ')) s.erase(0, 1);
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ')) s.pop_back();
    return s;
}

// ---------------- modal help tip ----------------
class HelpTipScreen : public Screen {
public:
    UiDoc ui;
    Text text;
    Sprite podlogka, arrow;
    Vec2 shift;
    bool flipX = false, flipY = false;
    Vec2 arrowPos;
    HelpTipScreen(const std::string& s, Vec2 target) {
        ui.load("UI/Tips.xml");
        text.load(ui.node("Text"));
        text.set(trimText(s));
        podlogka = ui.sprites["Podlogka"];
        arrow = ui.sprites["Arrow"];
        ui.sprites.clear();
        ui.texts.clear();
        // place the window beside the target, on the side with more room (cHelpTips::show)
        Vec2 half = podlogka.size * 0.5f;
        Vec2 center;
        flipX = target.x > 400;
        flipY = target.y > 300;
        center.x = flipX ? target.x - half.x - 20 : target.x + half.x + 20;
        center.y = flipY ? target.y - half.y - 20 : target.y + half.y + 20;
        center.x = std::max(half.x, std::min(800 - half.x, center.x));
        center.y = std::max(half.y, std::min(600 - half.y, center.y));
        shift = center - podlogka.pos;
        podlogka.pos = center;
        text.pos = text.pos + shift;
        for (auto& kv : ui.buttons) {
            kv.second.spr.pos = kv.second.spr.pos + shift;
            kv.second.text.pos = kv.second.text.pos + shift;
        }
        arrow.anim.play = true;
        arrow.anim.length = 0.6f;
        arrow.flipX = flipX;
        Vec2 ah = arrow.size * 0.5f;
        arrowPos = {target.x + (flipX ? -ah.x : ah.x), target.y + (flipY ? -ah.y : ah.y)};
    }
    bool opaque() const override { return false; }
    void update(float dt, bool input) override {
        arrow.update(dt);
        std::string c = ui.update(dt, input);
        bool enter = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_RETURN || k == SDLK_ESCAPE) enter = true;
        if (c == "OkButton" || (input && enter)) closed = true;
    }
    void draw() override {
        podlogka.draw();
        text.draw();
        ui.draw();
        // the arrow sheet points down-left; mirror it to point at the target
        DrawCmd d;
        d.tex = arrow.anim.tex;
        if (d.tex) {
            arrow.anim.uv(-1, d.u0, d.v0, d.u1, d.v1);
            if (!flipX) std::swap(d.u0, d.u1);
            if (flipY) std::swap(d.v0, d.v1);
            d.x = arrowPos.x; d.y = arrowPos.y; d.w = arrow.size.x; d.h = arrow.size.y;
            d.deep = podlogka.deep + 5;
            g_plat.draw(d);
        }
    }
};

bool showHelpTip(Game& g, int id, Vec2 target) {
    if (!tipAvailable(id) || g.tipBusy()) return false;
    tipMarkSeen(id);
    g.tipsThisLevel++;
    g_screens.push(std::make_shared<HelpTipScreen>(g_tipText[id], target));
    return true;
}

// ---------------- watering tip ----------------
class WaterTipScreen : public Screen {
public:
    UiDoc ui;
    Text text;
    explicit WaterTipScreen() {
        ui.load("UI/WaterTips.xml");
        XNode t = ui.node("Text");
        text.load(t);
        std::string s = t.child("Text").text().get();
        text.set(trimText(xBool(t, "Localized") ? tr(s) : s));
        ui.texts.clear();
    }
    bool opaque() const override { return false; }
    void update(float dt, bool input) override {
        std::string c = ui.update(dt, input);
        bool enter = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_RETURN || k == SDLK_ESCAPE) enter = true;
        if (c == "OkButton" || (input && enter)) closed = true;
    }
    void draw() override {
        ui.draw();
        text.draw();
    }
};

void showWaterTip(Game& g) {
    if (!tipAvailable(TIP_WATER) || g.tipBusy()) return;
    tipMarkSeen(TIP_WATER);
    g.tipsThisLevel++;
    g_screens.push(std::make_shared<WaterTipScreen>());
}

// ---------------- two-page help ----------------
class RoundHelpScreen : public Screen {
public:
    UiDoc ui;
    struct Page { std::vector<Sprite> pics; std::vector<Text> texts; };
    std::vector<Page> pages;
    int page = 0;
    RoundHelpScreen() {
        ui.load("UI/RoundHelpWindow.xml");
        for (XNode p : ui.node("Pages").children()) {
            if (p.type() != pugi::node_element) continue;
            Page pg;
            for (XNode s : p.child("Pictures").children()) { Sprite sp; sp.load(s); pg.pics.push_back(sp); }
            for (XNode t : p.child("Texts").children()) {
                Text tx;
                tx.load(t);
                std::string s = t.child("Text").text().get();
                tx.set(trimText(xBool(t, "Localized") ? tr(s) : s));
                pg.texts.push_back(tx);
            }
            pages.push_back(pg);
        }
        ui.sprites.erase("Pages");
    }
    bool opaque() const override { return false; }
    void update(float dt, bool input) override {
        ui.btn("PrevButton").visible = page > 0;
        ui.btn("NextButton").visible = page + 1 < (int)pages.size();
        std::string c = ui.update(dt, input);
        bool esc = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_ESCAPE) esc = true;
        if (c == "OkButton" || (input && esc)) closed = true;
        else if (c == "NextButton" && page + 1 < (int)pages.size()) ++page;
        else if (c == "PrevButton" && page > 0) --page;
    }
    void draw() override {
        ui.draw();
        if (page < (int)pages.size()) {
            for (auto& p : pages[page].pics) p.draw();
            for (auto& t : pages[page].texts) t.draw();
        }
    }
};

void showRoundHelp() { g_screens.push(std::make_shared<RoundHelpScreen>()); }

// ---------------- flying hint ----------------
bool startFlyTip(Game& g, FlyTip& ft, int id, Vec2 target, bool once) {
    if (ft.active() || g.tipBusy()) return false;
    Profile* p = prof();
    if (!p || !g_tipText.count(id)) return false;
    if (once && p->tipSeen[id]) return false;
    if (!once && p->tipSeen[id] >= 3) return false;
    p->tipSeen[id]++;
    g.tipsThisLevel++;
    ft.id = id;
    ft.target = target;
    ft.t = 0;
    ft.text = utf8to32(trimText(g_tipText[id]));
    return true;
}

void FlyTip::update(float dt) {
    if (!active()) return;
    t += dt;
    if (t > life) id = -1;
}

void FlyTip::draw() const {
    if (!active()) return;
    static TexPtr bubble = g_plat.texture("UI/GameUI/podskazka.dds");
    static TexPtr arr = g_plat.texture("UI/GameUI/arr.dds");
    static auto font = getFont("UI/menu_font.fnt", "UI/menu_font.dds");
    float a = std::min(1.f, std::min(t / 0.3f, (life - t) / 0.5f));
    if (a <= 0 || !font) return;
    // measure text
    float mul = 0.42f, w = 0;
    int lines = 1;
    size_t st = 0;
    for (size_t i = 0; i <= text.size(); ++i)
        if (i == text.size() || text[i] == U'\n') {
            w = std::max(w, font->width(text.substr(st, i - st), mul, 10));
            st = i + 1;
            if (i < text.size()) ++lines;
        }
    float lh = font->lineHeight(mul) * 0.85f;
    Vec2 size{std::max(160.f, w + 30), std::max(70.f, lines * lh + 30)};
    bool left = target.x > 400, up = target.y > 300;
    Vec2 c{target.x + (left ? -1 : 1) * (size.x * 0.5f + 22), target.y + (up ? -1 : 1) * (size.y * 0.5f + 22)};
    c.x = std::max(size.x * 0.5f, std::min(800 - size.x * 0.5f, c.x));
    c.y = std::max(size.y * 0.5f + 90, std::min(600 - size.y * 0.5f, c.y));
    uint8_t al = (uint8_t)(a * 255);
    DrawCmd d;
    d.tex = bubble;
    d.x = c.x; d.y = c.y; d.w = size.x; d.h = size.y;
    d.deep = 2400;
    d.color.a = al;
    if (d.tex) g_plat.draw(d);
    DrawCmd ar;
    ar.tex = arr;
    if (ar.tex) {
        ar.u0 = 0; ar.u1 = 1.f / 3; ar.v0 = 0; ar.v1 = 1;
        if (!left) std::swap(ar.u0, ar.u1);
        if (up) std::swap(ar.v0, ar.v1);
        ar.x = target.x + (left ? -20 : 20); ar.y = target.y + (up ? -20 : 20);
        ar.w = ar.h = 40;
        ar.deep = 2401;
        ar.color.a = al;
        g_plat.draw(ar);
    }
    Text tx;
    tx.font = font;
    tx.mul = mul;
    tx.str = text;
    tx.deep = 2402;
    tx.align = 0;
    tx.drawAt(c, a);
}
