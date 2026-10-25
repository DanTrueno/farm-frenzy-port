#include "game_windows.h"
#include "game_flow.h"
#include "awards.h"
#include "tips.h"
#include <cmath>

// ---------------- goal icons (GameAim.xml AimRecords) ----------------
static std::shared_ptr<XmlDoc> aimXml() {
    static std::shared_ptr<XmlDoc> d = loadXml("UI/GameAim.xml");
    return d;
}

static XNode aimRecord(int type) {
    return aimXml()->root().child("Aim").child("AimRecords").child(("rec" + std::to_string(type)).c_str());
}

TexPtr goalIcon(int type, bool small) {
    XNode r = aimRecord(type);
    XNode i = r.child(small ? "Small_Icon" : "Icon");
    if (!i) i = r.child("Icon");
    std::string p = xStr(i, "Picture");
    return p.empty() ? nullptr : g_plat.texture(p);
}

std::string goalName(int type) { return tr(xStr(aimRecord(type).child("Text"), "TextString")); }

static std::string fmtTime(float t) {
    int s = (int)t;
    char b[16];
    snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
    return b;
}

static bool escPressed() {
    for (auto k : g_plat.keysPressed) if (k == SDLK_ESCAPE) return true;
    return false;
}

// ---------------- Market ----------------
class MarketScreen : public Screen {
public:
    Game& g;
    UiDoc ui;
    struct Line {
        bool pet = false;
        int type = 0;
        Vec2 at;              // current slot (ProductPlaces / PetPlaces tag)
        bool shown = false;
        Sprite icon, x, money;  // loaded relative to the line origin
        Text count, cost;
        Button sell1, sellAll;
        Vec2 rel1, relAll;
    };
    std::vector<Line> lines;
    std::vector<Vec2> prodTags, petTags;

    // cToMarket::update: only lines that have something to sell are shown,
    // packed one after another into the slot positions
    void layout() {
        size_t pi = 0, ai = 0;
        for (auto& l : lines) {
            l.shown = available(l) > 0;
            if (!l.shown) continue;
            auto& tags = l.pet ? petTags : prodTags;
            size_t& k = l.pet ? ai : pi;
            if (k >= tags.size()) { l.shown = false; continue; }
            l.at = tags[k++];
            l.sell1.spr.pos = l.at + l.rel1;
            l.sell1.text.pos = l.sell1.spr.pos;
            l.sellAll.spr.pos = l.at + l.relAll;
            l.sellAll.text.pos = l.sellAll.spr.pos;
        }
    }
    Sprite truckPic[5];
    std::vector<std::vector<Vec2>> places;
    Vec2 truckCenter;
    std::vector<Truck::Box> boxes;
    int depotBackup[PROD_COUNT];
    std::vector<int> petsTaken[3];  // not used for restore: pets removed on OK only
    int petsLoaded[3] = {0, 0, 0};
    Text sellMoney, moneyText;

    explicit MarketScreen(Game& game) : g(game) {
        ui.load("UI/ToMarket.xml");
        for (int i = 0; i < PROD_COUNT; ++i) depotBackup[i] = g.depot.count[i];
        boxes = g.truck.boxes;
        auto loadLines = [&](XNode parent, bool pet) {
            int idx = 0;
            for (XNode n : parent.children()) {
                if (n.type() != pugi::node_element) continue;
                Line l;
                l.pet = pet;
                l.type = idx++;
                l.icon.load(n.child("Icon"));
                l.x.load(n.child("PictureX"));
                l.money.load(n.child("IconMoney_"));
                l.money.anim.play = true;
                l.count.load(n.child("CountText"));
                l.cost.load(n.child("CostText"));
                l.sell1.load(n.child("Sell1Button"));
                l.sellAll.load(n.child("SellAllButton"));
                l.rel1 = l.sell1.spr.pos;
                l.relAll = l.sellAll.spr.pos;
                lines.push_back(l);
            }
        };
        loadLines(ui.node("ProductLines"), false);
        loadLines(ui.node("PetLines"), true);
        for (XNode t : ui.node("ProductPlaces").children("Tag")) prodTags.push_back(xVec(t, "pos"));
        for (XNode t : ui.node("PetPlaces").children("Tag")) petTags.push_back(xVec(t, "pos"));
        ui.sprites.erase("ProductPlaces");
        ui.sprites.erase("PetPlaces");
        layout();
        ui.texts.erase("SellMoneyText");
        sellMoney = ui.txt("SellMoneyText");
        if (!sellMoney.font) { sellMoney.load(ui.node("SellMoneyText")); }
        moneyText.load(ui.node("MoneyText"));
        ui.texts.erase("MoneyText");
        auto t = loadXml("UI/ToMarket/telegaUI.xml");
        truckCenter = xVec(t->root(), "CenterPosition", {555, 375});
        int i = 0;
        for (XNode u : t->root().child("Upgrades").children()) {
            if (u.type() != pugi::node_element || i >= 5) continue;
            truckPic[i].load(u.child("Picture"));
            std::vector<Vec2> pl;
            for (XNode p : u.child("ProductPlaces").children()) {
                if (p.type() != pugi::node_element) continue;
                Vec2 v;
                sscanf(p.text().get(), "%f %f", &v.x, &v.y);
                pl.push_back(v);
            }
            places.push_back(pl);
            ++i;
        }
    }
    bool opaque() const override { return false; }

    int slots() const { return g.truck.slots(); }

    // try to put one item into the truck; returns false when no room
    bool load(bool pet, int type) {
        int cap = pet ? g_data.pets[type].boxCapacity : g_data.prod[type].boxCapacity;
        for (auto& b : boxes)
            if (b.pet == pet && b.type == type && b.count < cap) { b.count++; return true; }
        if ((int)boxes.size() < slots()) {
            boxes.push_back({pet, type, 1});
            return true;
        }
        return false;
    }

    long long cargoValue() const {
        long long v = 0;
        for (auto& b : boxes)
            v += (long long)b.count * (b.pet ? g_data.pets[b.type].cost / 2 : g_data.prod[b.type].cost);
        return v;
    }

    int available(const Line& l) const {
        if (!l.pet) return g.depot.count[l.type];
        return g.petCount((PetType)l.type) - petsLoaded[l.type];
    }

    void sell(Line& l, bool all) {
        int n = all ? available(l) : 1;
        int done = 0;
        for (int i = 0; i < n && available(l) > 0; ++i) {
            if (!load(l.pet, l.type)) break;
            if (l.pet) petsLoaded[l.type]++;
            else g.depot.remove((ProdType)l.type, 1);
            ++done;
        }
        if (!done) g_audio.playSound(g_data.foulSound);
    }

    void unload(size_t bi) {
        auto& b = boxes[bi];
        if (b.pet) petsLoaded[b.type] -= b.count;
        else g.depot.add((ProdType)b.type, b.count);
        boxes.erase(boxes.begin() + bi);
    }

    void update(float dt, bool input) override {
        std::string c = ui.update(dt, input);
        layout();
        for (auto& l : lines) {
            if (!l.shown) continue;
            l.money.update(dt);
            if (l.sell1.update(dt, input)) sell(l, false);
            if (l.sellAll.update(dt, input)) sell(l, true);
        }
        // click a box in the truck to take it back
        if (input && Input::released()) {
            int lvl = std::min(g.truck.level, (int)places.size() - 1);
            for (size_t i = 0; i < boxes.size() && i < places[lvl].size(); ++i) {
                Vec2 p = truckCenter + places[lvl][i];
                Vec2 m = Input::mouse();
                if (std::fabs(m.x - p.x) < 24 && std::fabs(m.y - p.y) < 24) {
                    g_audio.playSound("Sounds/shop_return.ogg");
                    unload(i);
                    break;
                }
            }
        }
        if (c == "OkButton") {
            if (!boxes.empty()) {
                // remove sold pets from the field
                for (int t = 0; t < 3; ++t) {
                    int n = petsLoaded[t];
                    for (auto& e : g.ents) {
                        if (n <= 0) break;
                        if (auto* p = dynamic_cast<ProductionPet*>(e.get()))
                            if (!p->dead && p->type == t && p->state != ProductionPet::DYING) { p->dead = true; --n; }
                    }
                }
                for (auto& b : boxes) if (!b.pet && b.type == P_BEAR) g.bearsSold += b.count;
                g.truck.boxes = boxes;
                g.truck.cargoValue = cargoValue();
                g.truck.away = true;
                g.truck.paid = false;
                g.truck.tripT = 0;
            }
            closed = true;
        } else if (c == "CancelButton" || (input && escPressed())) {
            for (int i = 0; i < PROD_COUNT; ++i) g.depot.count[i] = depotBackup[i];
            closed = true;
        }
    }

    void draw() override {

        drawDim(501, 0.3f);
        ui.draw();
        Text mt = moneyText;
        mt.set(std::to_string(g.money));
        mt.align = 2;
        mt.pos = {700, 30};
        mt.draw();
        layout();
        for (auto& l : lines) {
            if (!l.shown) continue;
            int n = available(l);
            l.icon.drawAt(l.at + l.icon.pos);
            l.x.drawAt(l.at + l.x.pos);
            l.money.drawAt(l.at + l.money.pos);
            Text c = l.count;
            c.set(std::to_string(n));
            c.drawAt(l.at + l.count.pos);
            Text p = l.cost;
            p.set(std::to_string(l.pet ? g_data.pets[l.type].cost / 2 : g_data.prod[l.type].cost));
            p.drawAt(l.at + l.cost.pos);
            l.sell1.draw();
            l.sellAll.draw();
        }
        int lvl = std::min(g.truck.level, 4);
        truckPic[lvl].draw();
        if (lvl < (int)places.size())
            for (size_t i = 0; i < boxes.size() && i < places[lvl].size(); ++i) {
                auto& b = boxes[i];
                TexPtr t = b.pet ? g_data.pets[b.type].box : g_data.prod[b.type].box;
                Vec2 p = truckCenter + places[lvl][i];
                DrawCmd d;
                d.tex = t;
                if (!t) continue;
                d.x = p.x; d.y = p.y; d.w = 56; d.h = 56;
                d.deep = 1020;
                g_plat.draw(d);
                int cap = b.pet ? g_data.pets[b.type].boxCapacity : g_data.prod[b.type].boxCapacity;
                Progress pr;
                pr.tex = g_plat.texture("Pets/progres_a.dds");
                pr.size = {40, 8};
                pr.side = 1;
                pr.maxValue = (float)cap;
                pr.value = (float)b.count;
                pr.deep = 1021;
                pr.drawAt(p + Vec2(0, 22));
            }
        Text sm = sellMoney;
        sm.set(std::to_string(cargoValue()));
        sm.draw();
    }
};

void showMarket(Game& g) { g_screens.push(std::make_shared<MarketScreen>(g)); }

// ---------------- Pause menu ----------------
class GameMenuScreen : public Screen {
public:
    Game& g;
    UiDoc ui;
    explicit GameMenuScreen(Game& game) : g(game) { ui.load("UI/GameMenu.xml"); }
    bool opaque() const override { return false; }
    void update(float dt, bool input) override {
        std::string c = ui.update(dt, input);
        if (c == "ResumeButton" || (input && escPressed())) closed = true;
        else if (c == "RestartButton") {
            int n = g.li.number;
            showYesNo(tr("Вы уверены?"), [n](bool y) { if (y) startLevel(n); });
        } else if (c == "OptionsButton") showOptions();
        else if (c == "MainMenuButton") {
            showYesNo(tr("Вы уверены?"), [](bool y) {
                if (!y) return;
                saveRunningGame();
                goMainMenu();
            });
        } else if (c == "TutorialButton") {
            closed = true;
            showRoundHelp();
        }
    }
    void draw() override {
        drawDim(3000, 0.4f);
        ui.draw();
    }
};

void showGameMenu(Game& g) { g_screens.push(std::make_shared<GameMenuScreen>(g)); }

// ---------------- Goals window (in game and before the level) ----------------
class AimScreen : public Screen {
public:
    Game& g;
    UiDoc ui;
    bool pre;
    Vec2 aimPos;
    float offY = 50;
    XNode aim;
    explicit AimScreen(Game& game, bool preGame) : g(game), pre(preGame) {
        ui.load(pre ? "UI/PreGameAim.xml" : "UI/GameAim.xml");
        aim = ui.node("Aim");
        aimPos = xVec(aim, "position");
        offY = xFloat(aim, "OffsetY", 50);
        if (pre) {
            ui.txt("GoldBonus").set(std::to_string(g.li.goldPrize));
            ui.txt("SilverBonus").set(std::to_string(g.li.silverPrize));
            ui.txt("LevelBonus").set(std::to_string(g.li.prize));
            ui.txt("GoldTile").set(fmtTime(g.li.goldTime));
            ui.txt("SilverTile").set(fmtTime(g.li.silverTime));
        }
    }
    bool opaque() const override { return false; }
    void update(float dt, bool input) override {
        std::string c = ui.update(dt, input);
        bool enter = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_RETURN || k == SDLK_ESCAPE) enter = true;
        if (c == "OkButton" || (input && enter)) closed = true;
    }
    void draw() override {

        drawDim(3000, 0.35f);
        ui.draw();
        Text num;
        num.load(aim.child("NumberText"));
        Vec2 iconOff = xVec(aim.child("IconProto"), "position");
        Vec2 iconSize = xVec(aim.child("IconProto"), "size", {32, 32});
        int i = 0;
        for (auto& gl : g.li.goals) {
            Vec2 p = aimPos + Vec2(0, offY * i);
            TexPtr t = goalIcon(gl.first, false);
            if (t) {
                DrawCmd d;
                d.tex = t;
                d.x = p.x + iconOff.x; d.y = p.y + iconOff.y; d.w = iconSize.x; d.h = iconSize.y;
                d.deep = 3201;
                g_plat.draw(d);
            }
            Text n = num;
            n.pos = p + xVec(aim.child("NumberText"), "position");
            if (pre) {
                n.set(std::to_string(gl.second));
                Sprite x;
                x.load(aim.child("X"), p);
                x.draw();
            } else {
                long long cur = std::min(g.goalCurrent(gl.first), gl.second);
                n.set(std::to_string(cur) + "/" + std::to_string(gl.second));
                if (XNode ch = aim.child("Checked")) {
                    Sprite s;
                    s.load(ch, p);
                    if (cur >= gl.second) s.draw();
                }
            }
            n.draw();
            ++i;
        }
    }
};

void showGameAim(Game& g) { g_screens.push(std::make_shared<AimScreen>(g, false)); }
void showPreGameAim(Game& g) { g_screens.push(std::make_shared<AimScreen>(g, true)); }

// ---------------- Level results ----------------
class ResultsScreen : public Screen {
public:
    Game& g;
    UiDoc ui;
    int medal = 1;  // 1 passed, 2 silver, 3 gold
    long long bonus = 0, bank = 0;
    float t = 0;
    Sprite prize;
    explicit ResultsScreen(Game& game) : g(game) {
        ui.load("UI/LevelResults.xml");
        float tm = g.time;
        bonus = g.li.prize;
        if (tm <= g.li.goldTime) { medal = 3; bonus = g.li.goldPrize; }
        else if (tm <= g.li.silverTime) { medal = 2; bonus = g.li.silverPrize; }
        Profile* p = g.prof;
        if (p) {
            int idx = g.li.number - 1;
            p->stars += bonus;
            bank = p->stars;
            if (medal > p->medal[idx]) p->medal[idx] = medal;
            if (p->bestTime[idx] == 0 || tm < p->bestTime[idx]) p->bestTime[idx] = tm;
            p->lastLevel = g.li.number;
            g_profiles.save();

        }
        ui.txt("MoneyTextValue").set(std::to_string(g.money));
        ui.txt("TimeTextValue").set(fmtTime(tm));
        ui.txt("BonusTextValue").set(std::to_string(bonus));
        ui.txt("BankmoneyTextValue").set(std::to_string(bank));
        // medal picture
        XNode pp = ui.node("PrizeProto");
        int i = 0;
        for (XNode pic : ui.node("Prizes").children("Picture")) {
            if ((medal == 3 && i == 0) || (medal == 2 && i == 1)) {
                prize.load(pp);
                prize.anim.tex = g_plat.texture(xStr(pic, "Picture"));
            }
            ++i;
        }
        if (medal >= 2) g_audio.playSound(xStr(ui.root(), "MedalSound"));
        else g_audio.playSound(xStr(ui.root(), "LevelCompleteSound"));
        ui.sprites.erase("PrizeProto");
        // StarPic appears twice in the XML: keep both by re-reading
        ui.sprites.erase("StarPic");
        for (XNode s : ui.root().children("StarPic")) {
            Sprite sp;
            sp.load(s);
            ui.background.push_back(sp);
        }
    }
    bool opaque() const override { return false; }
    void update(float dt, bool input) override {
        t += dt;
        std::string c = ui.update(dt, input);
        bool enter = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_RETURN) enter = true;
        if (c == "OkButton" || (input && enter)) {
            closed = true;
            goMap();
        }
    }
    void draw() override {

        drawDim(3000, 0.4f);
        ui.draw();
        if (prize.valid()) {
            float k = std::min(1.f, t / 0.5f);
            Sprite p = prize;
            p.size = prize.size * (0.5f + 0.5f * k);
            p.draw();
        }
    }
};

void showLevelResults(Game& g) {
    g_screens.push(std::make_shared<ResultsScreen>(g));
    // awards appear above the results window
    g.onLevelComplete();
    if (Profile* p = g.prof) {
        int done = 0, gold = 0, n = levelCount();
        for (int i = 0; i < n; ++i) { done += p->medal[i] > 0; gold += p->medal[i] == 3; }
        if (done >= std::min<long long>(n, awardData(A_GAME_COMPLETE))) giveAward(A_GAME_COMPLETE);
        if (gold >= std::min<long long>(n, awardData(A_GOLD_COMPLETE))) giveAward(A_GOLD_COMPLETE);
        checkCounterAwards();
        g_profiles.save();
    }
}
