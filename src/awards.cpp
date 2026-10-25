#include "awards.h"
#include "profile.h"
#include "screen.h"
#include "ui.h"
#include <cmath>

static std::shared_ptr<XmlDoc> awardsXml() {
    static std::shared_ptr<XmlDoc> d = loadXml("UI/Awards.xml");
    return d;
}

static XNode awardNode(int a) {
    return awardsXml()->root().child("Awards").child(("a" + std::to_string(a)).c_str());
}

static std::string trimText(std::string s) {
    std::string r;
    for (char c : s) if (c != '\r') r += c;
    while (!r.empty() && (r[0] == '\n' || r[0] == ' ')) r.erase(0, 1);
    while (!r.empty() && (r.back() == '\n' || r.back() == ' ')) r.pop_back();
    return r;
}

static std::string nodeText(XNode n, bool localized) {
    std::string s = n.text().get();
    return trimText(localized ? tr(s) : s);
}

long long awardData(int a) { return atoll(xStr(awardNode(a), "AwardData", "1").c_str()); }

// ---------------- "award received" window ----------------
class AwardWindowScreen : public Screen {
public:
    UiDoc ui;
    Sprite back, pic;
    Text text, congrats;
    float t = 0;
    explicit AwardWindowScreen(int a) {
        ui.load("ui/AwardWindow.xml");
        XNode r = ui.root();
        back.load(r);
        XNode an = awardNode(a);
        bool loc = xBool(an, "Localized", true);
        text.load(r.child("Text"));
        text.set(nodeText(an.child("Caption"), loc));
        congrats.load(r.child("CongratulationText"));
        congrats.set(nodeText(an.child("CongratulationCaption"), loc));
        ui.texts.clear();
        pic.load(an);
        pic.pos = xVec(r, "AwardPicPosition", {400, 300});
        pic.deep = back.deep + 10;
        g_audio.playSound(xStr(r, "Sound"));
    }
    bool opaque() const override { return false; }
    void update(float dt, bool input) override {
        t += dt;
        std::string c = ui.update(dt, input);
        bool enter = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_RETURN || k == SDLK_ESCAPE) enter = true;
        if (c == "OkButton" || (input && enter)) closed = true;
    }
    void draw() override {
        drawDim(back.deep - 1, 0.35f);
        back.draw();
        float k = std::min(1.f, t / 0.4f);
        Sprite p = pic;
        p.size = pic.size * (0.6f + 0.4f * k);
        p.draw();
        text.draw();
        congrats.draw();
        ui.draw();
    }
};

bool giveAward(int a) {
    Profile* p = g_profiles.current();
    if (!p || a < 0 || a >= AWARD_COUNT || p->awards[a]) return false;
    p->awards[a] = 1;
    g_profiles.save();
    g_screens.push(std::make_shared<AwardWindowScreen>(a));
    // collecting every other award grants the last one
    bool all = true;
    for (int i = 0; i < AWARD_COUNT; ++i)
        if (i != A_ALL_AWARDS && !p->awards[i]) all = false;
    if (all) giveAward(A_ALL_AWARDS);
    return true;
}

void checkCounterAwards() {
    Profile* p = g_profiles.current();
    if (!p) return;
    if (p->stat[ST_BEARS] >= awardData(A_BEARS)) giveAward(A_BEARS);
    if (p->stat[ST_PRODUCTS] >= awardData(A_PRODUCTS)) giveAward(A_PRODUCTS);
    if (p->stat[ST_PETS_BOUGHT] >= awardData(A_PETS)) giveAward(A_PETS);
    if (p->stars >= awardData(A_MILLION)) giveAward(A_MILLION);
    int shopSum = 0;
    for (int v : p->shop) shopSum += v;
    if (shopSum >= awardData(A_SHOP_FULL)) giveAward(A_SHOP_FULL);
}

// ---------------- awards gallery ----------------
class AwardsScreen : public Screen {
public:
    UiDoc ui;
    struct Item { Sprite spr; TexPtr hover; std::string caption; bool owned; };
    std::vector<Item> items;
    Text caption;
    AwardsScreen() {
        ui.load("UI/Awards.xml");
        Profile* p = g_profiles.current();
        for (int a = 0; a < AWARD_COUNT; ++a) {
            XNode n = awardNode(a);
            Item it;
            it.spr.load(n);
            if (xHas(n, "texture_hover")) it.hover = g_plat.texture(xStr(n, "texture_hover"));
            it.caption = nodeText(n.child("Caption"), xBool(n, "Localized", true));
            it.owned = p && p->awards[a];
            items.push_back(it);
        }
        caption.load(ui.node("AwardCaption"));
        ui.texts.erase("AwardCaption");
        ui.sprites.erase("Awards");
    }
    void update(float dt, bool input) override {
        std::string c = ui.update(dt, input);
        bool esc = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_ESCAPE) esc = true;
        if (!c.empty() || (input && esc)) closed = true;
    }
    void draw() override {
        ui.draw();
        Vec2 m = Input::mouse();
        const Item* hov = nullptr;
        for (auto& it : items) {
            if (!it.owned) continue;
            bool h = it.spr.hit(m);
            Sprite s = it.spr;
            if (h && it.hover) s.anim.tex = it.hover;
            s.draw();
            if (h) hov = &it;
        }
        if (hov) {
            Text t = caption;
            t.set(hov->caption);
            t.draw();
        }
    }
};

void showAwards() { g_screens.push(std::make_shared<AwardsScreen>()); }
