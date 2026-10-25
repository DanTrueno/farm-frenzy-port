// Level map, shop, level start.
#include "game_flow.h"
#include "game.h"
#include "game_windows.h"
#include "level.h"
#include "profile.h"
#include "awards.h"
#include "portable.h"
#include <cmath>

// ---------------- Map ----------------
class MapScreen : public Screen {
public:
    UiDoc ui;
    struct LayerPic {
        Vec2 pos, size;
        int deep;
        struct L { int num; TexPtr tex; float alpha; };
        std::vector<L> levels;
    };
    std::vector<LayerPic> layers;
    std::vector<Vec2> nodePos;
    std::vector<std::pair<int, int>> arcs;
    Button nodeBtn;
    TexPtr unavail;
    Vec2 medalOff;
    TexPtr medals[3];
    Vec2 medalSize{32, 32};
    float scroll = 0, scrollVel = 0, mapWidth = 1600;
    float moveSpeed = 210;
    int selected = 0;
    Sprite arrowL, arrowR;
    std::vector<LevelInfo> infos;
    // hint for locked levels
    XNode hint;
    Text levelText;

    MapScreen() {
        ui.load("UI/Map/map.xml");
        XNode r = ui.root();
        mapWidth = xFloat(r, "MapWidth", 1600);
        moveSpeed = xFloat(r, "MoveSpeed", 210);
        for (XNode p : r.child("LevelsBack").children("Picture")) {
            LayerPic lp;
            lp.pos = xVec(p, "position");
            lp.size = xVec(p, "size");
            lp.deep = xInt(p, "deep");
            for (XNode l : p.child("Levels").children("Level"))
                lp.levels.push_back({xInt(l, "Num"), g_plat.texture(xStr(l, "Picture")), xFloat(l, "Alpha", 1)});
            layers.push_back(lp);
        }
        XNode np = r.child("NodesPosition");
        int cnt = xInt(np, "count", 48);
        for (int i = 0; i < cnt; ++i) nodePos.push_back(xVec(np, ("p" + std::to_string(i)).c_str()));
        for (XNode a : r.child("Arcs").children()) {
            if (a.type() != pugi::node_element) continue;
            arcs.push_back({xInt(a, "n0"), xInt(a, "n1")});
        }
        auto nb = loadXml("UI/Map/NodeButton.xml");
        nodeBtn.load(nb->root());
        nodeBtn.visible = true;
        unavail = g_plat.texture(xStr(nb->root(), "UnavalPicture"));
        medalOff = xVec(nb->root(), "MedalOffset", {0, -30});
        int i = 0;
        for (XNode m : r.child("Medals").children("Picture")) {
            if (i < 3) medals[i] = g_plat.texture(xStr(m, "Picture"));
            medalSize = xVec(m, "size", {32, 32});
            ++i;
        }
        arrowL.load(r.child("MovePicLeft"));
        arrowR.load(r.child("MovePicRight"));
        ui.sprites.erase("MovePicLeft");
        ui.sprites.erase("MovePicRight");
        ui.sprites.erase("MedalProto");
        hint = r.child("MapHint");
        int n = std::min((int)nodePos.size(), levelCount());
        infos.resize(n);
        for (int k = 0; k < n; ++k) loadLevelInfo(k + 1, infos[k]);
        // default selection: the first available unfinished level
        Profile* p = g_profiles.current();
        selected = 0;
        for (int k = 0; k < n; ++k)
            if (available(k) && p && !p->medal[k]) { selected = k; break; }
        if (p && p->lastLevel > 0 && p->lastLevel <= n && !available(selected)) selected = p->lastLevel - 1;
        scroll = std::max(0.f, std::min(mapWidth - 800, nodePos[selected].x - 400));
        levelText.font = getFont("UI/menu_font.fnt", "UI/menu_font.dds");
        levelText.mul = 0.6f;
        levelText.deep = 530;
        levelText.pos = {400, 530};
        playMenuMusic();
    }

    bool completed(int n) const {
        Profile* p = g_profiles.current();
        return p && n >= 0 && n < MAX_LEVELS && p->medal[n] > 0;
    }
    bool available(int n) const {
        if (n == 0) return true;
        if (getenv("FF_UNLOCK_ALL")) return true;
        for (auto& a : arcs)
            if ((a.second == n && completed(a.first)) || (a.first == n && completed(a.second) && a.second < n)) return true;
        return completed(n);
    }

    void update(float dt, bool input) override {
        std::string c = ui.update(dt, input);
        if (c == "OkButton") {
            if (selected < (int)infos.size() && !levelAccessible(infos[selected])) {
                g_audio.playSound("Sounds/Foul.ogg");
                goShop();
                return;
            }
            startLevel(selected + 1);
            return;
        }
        if (c == "ShopButton") { goShop(); return; }
        if (c == "MainMenuButton") { goMainMenu(); return; }
        for (auto k : g_plat.keysPressed) if (input && k == SDLK_ESCAPE) { goMainMenu(); return; }
        // scrolling with arrows / edges
        Vec2 m = Input::mouse();
        float dir = 0;
        if (input && m.y < 520 && m.x < 60) dir = -1;
        if (input && m.y < 520 && m.x > 740) dir = 1;
        scroll = std::max(0.f, std::min(mapWidth - 800, scroll + dir * moveSpeed * 2 * dt));
        arrowL.update(dt);
        arrowR.update(dt);
        // nodes
        if (input && Input::released() && m.y < 520) {
            for (size_t i = 0; i < infos.size(); ++i) {
                Vec2 p = nodePos[i] - Vec2(scroll, 0);
                if (std::fabs(m.x - p.x) < 24 && std::fabs(m.y - p.y) < 20 && available((int)i)) {
                    g_audio.playSound("Sounds/ButtonClick.ogg");
                    if (selected == (int)i) { c = "OkButton"; startLevel(selected + 1); return; }
                    selected = (int)i;
                }
            }
        }
    }

    void draw() override {
        for (auto& s : ui.background) s.draw();
        for (auto& kv : ui.buttons) kv.second.draw();
        // layered pictures
        for (auto& lp : layers) {
            const LayerPic::L* best = nullptr;
            for (auto& l : lp.levels)
                if (l.num == 0 || completed(l.num - 1)) best = &l;
            if (!best || !best->tex) continue;
            DrawCmd d;
            d.tex = best->tex;
            d.x = lp.pos.x - scroll; d.y = lp.pos.y;
            if (d.x < -200 || d.x > 1000) continue;
            d.w = lp.size.x; d.h = lp.size.y;
            d.deep = lp.deep;
            d.color.a = (uint8_t)(best->alpha * 255);
            g_plat.draw(d);
        }
        // nodes
        for (size_t i = 0; i < infos.size(); ++i) {
            Vec2 p = nodePos[i] - Vec2(scroll, 0);
            if (p.x < -40 || p.x > 840) continue;
            bool av = available((int)i);
            Button b = nodeBtn;
            b.spr.pos = p;
            b.spr.deep = 480;
            if (!av && unavail) {
                b.spr.anim.tex = unavail;
                b.spr.drawAt(p, 0);
            } else {
                Vec2 m = Input::mouse();
                bool hov = std::fabs(m.x - p.x) < 24 && std::fabs(m.y - p.y) < 20;
                int f = (int)i == selected ? b.fPressed : hov ? b.fHover : b.fNormal;
                b.spr.drawAt(p, f);
            }
            Profile* pr = g_profiles.current();
            int md = pr ? pr->medal[i] : 0;
            TexPtr mt = md == 3 ? medals[0] : md == 2 ? medals[1] : md == 1 ? medals[2] : nullptr;
            if (mt) {
                DrawCmd d;
                d.tex = mt;
                d.x = p.x + medalOff.x; d.y = p.y + medalOff.y; d.w = medalSize.x; d.h = medalSize.y;
                d.deep = 526;
                g_plat.draw(d);
            }
            if ((int)i == selected) {
                Text t = levelText;
                t.set(std::to_string(i + 1));
                t.pos = p + Vec2(0, 1);
                t.deep = 490;
                t.draw();
            }
        }
        if (scroll > 0) arrowL.draw();
        if (scroll < mapWidth - 800) arrowR.draw();
        // selected level info / requirements
        if (selected < (int)infos.size()) {
            auto& li = infos[selected];
            Text t = levelText;
            std::string s = tr("Уровень") + " " + std::to_string(selected + 1);
            Profile* p = g_profiles.current();
            if (p && p->bestTime[selected] > 0) {
                int sec = (int)p->bestTime[selected];
                char b[32];
                snprintf(b, sizeof b, "  %d:%02d", sec / 60, sec % 60);
                s += b;
            }
            // requirements of the selected level (MapHint layout from map.xml)
            XNode pos = hint.child("Positions");
            XNode proto = hint.child("IconProto");
            Vec2 iconOff = xVec(proto, "position"), iconSize = xVec(proto, "size", {52, 52});
            int k = 0;
            for (auto& c : li.access) {
                int sh = critToShop(c.first);
                if (sh < 0) continue;
                XNode rec = hint.child("Records").child(("rec" + std::to_string(c.first)).c_str());
                Vec2 at = xVec(pos, ("pos" + std::to_string(k)).c_str(), {242.f + 190 * k, 560});
                XNode ic = rec.child("Icon");
                DrawCmd d;
                d.tex = g_plat.texture(xStr(ic, "Picture"));
                if (d.tex) {
                    // house icons are strips of 5 frames, one per shop level
                    int cols = std::max(1, xInt(ic, "anim_columns", 1));
                    int f = std::max(0, std::min(cols - 1, c.second - 1));
                    d.u0 = (float)f / cols;
                    d.u1 = (float)(f + 1) / cols;
                    d.x = at.x + iconOff.x; d.y = at.y + iconOff.y; d.w = iconSize.x; d.h = iconSize.y;
                    d.deep = xInt(proto, "deep", 520);
                    g_plat.draw(d);
                }
                Text ex;
                ex.load(hint.child("ExplainText"));
                ex.pos = at + xVec(hint.child("ExplainText"), "position");
                ex.set(tr(xStr(rec.child("Text"), "TextString")));
                ex.draw();
                Text num;
                num.load(hint.child("NumberText"));
                num.pos = at + xVec(hint.child("NumberText"), "position");
                num.set(std::to_string(c.second));
                num.draw();
                XNode ch = hint.child("Checked");
                bool ok = p && p->shop[sh] >= c.second;
                DrawCmd tk;
                tk.tex = g_plat.texture(ok ? xStr(ch, "Picture") : xStr(ch, "UnChecked"));
                if (tk.tex) {
                    Vec2 cp = at + xVec(ch, "position"), cs = xVec(ch, "size", {20, 20});
                    tk.x = cp.x; tk.y = cp.y; tk.w = cs.x; tk.h = cs.y;
                    tk.deep = xInt(ch, "deep", 520);
                    g_plat.draw(tk);
                }
                ++k;
            }
            // without requirements the panel shows the level number and best time
            if (k == 0) {
                t.set(s);
                t.pos = {600, 560};
                t.draw();
            }
        }
    }
};

void goMap() { g_screens.replaceAll(std::make_shared<MapScreen>()); }

// ---------------- Shop ----------------
class ShopScreen : public Screen {
public:
    UiDoc ui;
    struct Item {
        Button btn;
        Sprite icon, arrow;
        Text caption, price, level;
        struct St { TexPtr tex; long long price; };
        std::vector<St> states;
        std::vector<Text> tipTexts;
        std::vector<Sprite> tipPics;
    };
    Item items[SHOP_COUNT];
    Text money;

    ShopScreen() {
        ui.load("UI/Shop.xml");
        for (int i = 0; i < SHOP_COUNT; ++i) {
            XNode n = ui.node("Items").child(("i" + std::to_string(i)).c_str());
            Item& it = items[i];
            it.btn.load(n);
            it.btn.hasText = false;
            Vec2 o = it.btn.spr.pos;
            it.icon.load(n.child("Icon"), o);
            it.arrow.load(n.child("ArrowPointer"), o);
            it.caption.load(n.child("CaptionText"), o);
            it.price.load(n.child("PriceText"), o);
            it.level.load(n.child("LevelText"), o);
            for (XNode s : n.child("States").children()) {
                if (s.type() != pugi::node_element) continue;
                it.states.push_back({g_plat.texture(xStr(s, "Picture")), atoll(xStr(s, "Price", "0").c_str())});
            }
            for (XNode t : n.child("Icon").child("Tip").child("Texts").children()) { Text tx; tx.load(t); it.tipTexts.push_back(tx); }
            for (const char* pn : {"Pics", "Pics1"})
                for (XNode p : n.child("Icon").child("Tip").child(pn).children()) { Sprite s; s.load(p); it.tipPics.push_back(s); }
        }
        money = ui.txt("MoneyText");
        ui.texts.erase("MoneyText");
        ui.texts.erase("Title1");
        ui.sprites.erase("Items");
    }

    void update(float dt, bool input) override {
        std::string c = ui.update(dt, input);
        bool esc = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_ESCAPE) esc = true;
        if (c == "OkButton" || (input && esc)) { g_profiles.save(); goMap(); return; }
        Profile* p = g_profiles.current();
        if (!p) return;
        for (int i = 0; i < SHOP_COUNT; ++i) {
            Item& it = items[i];
            int lvl = p->shop[i];
            bool maxed = lvl >= (int)it.states.size();
            it.btn.enabled = !maxed && p->stars >= it.states[std::min(lvl, (int)it.states.size() - 1)].price;
            it.arrow.update(dt);
            if (it.btn.update(dt, input)) {
                long long price = it.states[lvl].price;
                if (p->stars >= price) {
                    p->stars -= price;
                    p->shop[i]++;
                    g_profiles.save();
                    checkCounterAwards();
                }
            }
        }
    }

    void draw() override {
        ui.draw();
        Profile* p = g_profiles.current();
        Text m = money;
        m.set(std::to_string(p ? p->stars : 0));
        m.draw();
        for (int i = 0; i < SHOP_COUNT; ++i) {
            Item& it = items[i];
            int lvl = p ? p->shop[i] : 0;
            bool maxed = lvl >= (int)it.states.size();
            Sprite ic = it.icon;
            int si = std::min(lvl, (int)it.states.size() - 1);
            if (si >= 0) ic.anim.tex = it.states[si].tex;
            ic.anim.frames = ic.anim.cols = ic.anim.rows = 1;
            ic.color.a = maxed ? 255 : 255;
            ic.draw();
            it.btn.draw();
            it.caption.draw();
            Text pr = it.price;
            pr.set(maxed ? "OK" : std::to_string(it.states[lvl].price));
            pr.draw();
            Text lv = it.level;
            lv.set(std::to_string(lvl));
            lv.draw();
            if (!maxed && p && p->stars >= it.states[lvl].price) it.arrow.draw();
            if (it.btn.hovered || (Input::mouse() - ic.pos).len() < 50) {
                for (auto& t : it.tipTexts) t.draw();
                for (auto& s : it.tipPics) s.draw();
            }
        }
    }
};

void goShop() { g_screens.replaceAll(std::make_shared<ShopScreen>()); }

// ---------------- Level start ----------------
bool hasSavedGame() {
    Profile* p = g_profiles.current();
    return p && Game::savedLevel(g_profiles.savePath(*p)) > 0;
}

void saveRunningGame() {
    Profile* p = g_profiles.current();
    auto g = g_screens.find<Game>();
    if (p && g && !g->finished) g->saveState(g_profiles.savePath(*p));
}

void continueSavedGame() {
    Profile* p = g_profiles.current();
    if (!p) return;
    std::string path = g_profiles.savePath(*p);
    int lvl = Game::savedLevel(path);
    if (lvl <= 0) { goMap(); return; }
    auto g = std::make_shared<Game>(lvl);
    g->loadState(path);
    g_screens.replaceAll(g);
}

void startLevel(int level) {
    if (!g_profiles.current()) return;
    removeFile(g_profiles.savePath(*g_profiles.current()));
    auto g = std::make_shared<Game>(level);
    g_screens.replaceAll(g);
    showPreGameAim(*g);
}
