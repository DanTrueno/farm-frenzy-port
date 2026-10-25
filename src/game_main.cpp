// Game screen: level setup, input routing, HUD.
#include "game.h"
#include "portable.h"
#include "game_flow.h"
#include "game_windows.h"
#include "vfs.h"
#include "awards.h"
#include <algorithm>
#include <cmath>


int Game::shopLevel(int item) const { return prof ? prof->shop[item] : 0; }

long long Game::upgradeCost(int item, int level) const {
    if (item < 0 || item >= (int)g_data.costs.size()) return 0;
    auto& v = g_data.costs[item];
    if (level < 0 || level >= (int)v.size()) return 0;
    return v[level];
}

Game::Game(int level) {
    g_data.load();
    prof = g_profiles.current();
    loadLevelInfo(level, li);
    money = li.startMoney;

    // grass grid over the moving zone
    cols = (int)((g_data.zoneX1 - g_data.zoneX0) / g_data.cellW);
    rows = (int)((g_data.zoneY1 - g_data.zoneY0) / g_data.cellH);
    grass.resize(cols);
    for (auto& c : grass) c.resize(rows);

    // buildings: in-level upgrade limit comes from the shop purchases
    well.load();
    well.maxLevel = std::min(4, shopLevel(SHOP_WELL));
    depot.load();
    depot.maxLevel = std::min(4, shopLevel(SHOP_DEPOT));
    truck.load();
    truck.maxLevel = std::min(4, shopLevel(SHOP_TRUCK));
    for (auto& p : g_data.places) {
        Sprite s;
        s.anim.tex = p.pic;
        s.pos = p.pos;
        s.size = p.size;
        s.deep = p.deep;
        placePics.push_back(s);
    }
    static const int houseShop[HOUSE_COUNT] = {SHOP_POWDER, SHOP_CAKE, SHOP_SPINNERY, SHOP_TEXTILE, SHOP_BUTTER, SHOP_CHEESE};
    for (auto& h : li.houses) {
        int sl = shopLevel(houseShop[h.type]);
        if (h.upgrade > 0) {
            auto ph = std::make_unique<ProdHouse>();
            ph->load((HouseType)h.type, h.pos);
            ph->level = std::min(4, h.upgrade - 1);
            ph->maxLevel = std::max(ph->level, std::min(4, sl - 1));
            houses.push_back(std::move(ph));
        } else {
            HousePlace hp;
            hp.load((HouseType)h.type, h.pos);
            hp.price = upgradeCost(4 + h.type, 0);
            hp.cost.set(std::to_string(hp.price));
            hp.allowed = sl > 0;
            housePlaces.push_back(hp);
        }
    }
    for (int p : li.pets) spawnPet((PetType)p, false);

    // HUD
    hud.load("UI/GameUI.xml");
    moneyText = hud.txt("MoneyText");
    timeText = hud.txt("TimeText");
    hud.texts.erase("MoneyText");
    hud.texts.erase("TimeText");
    hud.texts.erase("PerfectText");
    hud.texts.erase("PerfectTimeText");
    const char* names[5] = {"BuyGooseButton", "BuySheepButton", "BuyCowButton", "BuyDogButton", "BuyCatButton"};
    for (int i = 0; i < 5; ++i) {
        XNode n = hud.node(names[i]);
        BuyBtn b;
        b.b.load(n);
        b.b.hasText = false;
        b.pet = (PetType)i;
        b.cost = xInt(n, "Cost");
        if (XNode t = n.child("Text")) {
            b.price.load(t);
            b.price.set(std::to_string(b.cost));
            if (XNode c = t.child("Coin")) { b.coin.load(c); b.coin.pos = b.price.pos + b.coin.pos; }
        }
        for (XNode t : n.child("Tip").child("Texts").children()) { Text tx; tx.load(t); b.tipTexts.push_back(tx); }
        for (XNode p : n.child("Tip").child("Pics").children()) { Sprite s; s.load(p); b.tipPics.push_back(s); }
        buy.push_back(b);
        hud.buttons.erase(names[i]);
    }
    hud.sprites.erase("ArrowPointer");
    // truck minimap
    {
        auto mm = loadXml("UI/GameUI/HorseMinimap.xml");
        XNode r = mm->root();
        minimap.load(r);
        farmPos = xVec(r, "FarmPos");
        marketPos = xVec(r, "MarketPos");
        XNode h = r.child("HorseToMarket");
        horseTo.load(h);
        horseFrom.load(r.child("HorseToFarm"));
        for (const char* a : {"Picture", "Picture1", "Picture2", "Picture3", "Picture4"})
            if (xHas(h, a)) horseFrames.push_back(g_plat.texture(xStr(h, a)));
        if (XNode c = r.child("CostText")) minimapCost.load(c, minimap.pos);
    }
    tipLine = hud.txt("PerfectText");
    tipLine.pos = {400, 160};
    tipLine.deep = 2600;
    tipLine.align = 0;
    tipLine.mul = 0.7f;
    tipLine.font = getFont("UI/menu_font.fnt", "UI/menu_font.dds");

    for (auto& s : li.back) s.anim.play = true;
    loadClouds();
}

Game::~Game() {}

// ---------------- helpers ----------------
Vec2 Game::cellPos(int c, int r) const {
    float x = c * g_data.cellW + g_data.zoneX0 + g_data.cellW * 0.5f;
    if (r & 1) x += g_data.cellW * 0.5f;
    return {x, r * g_data.cellH + g_data.zoneY0 + g_data.cellH * 0.5f};
}

Grass* Game::closestGrass(Vec2 p) {
    Grass* best = nullptr;
    float bd = 1e10f;
    for (auto& col : grass)
        for (auto& g : col)
            if (g && !g->dead && g->value > 0) {
                float d = (g->pos.x - p.x) * (g->pos.x - p.x) + (g->pos.y - p.y) * (g->pos.y - p.y);
                if (d < bd) { bd = d; best = g.get(); }
            }
    return best;
}

static float ellipseWeight(Vec2 c, Vec2 p, float a, float b) {
    float dx = p.x - c.x, dy = p.y - c.y;
    float d2 = dx * dx + dy * dy;
    if (d2 == 0) return 1;
    float r = a * b * std::sqrt(d2) / std::sqrt(a * a * dy * dy + b * b * dx * dx);
    return std::max(0.f, 1 - std::sqrt(d2) / r);
}

void Game::water(Vec2 p) {
    g_audio.playSound(g_data.grassSound, 0.7f);
    // splash
    for (int i = 0; i < 10; ++i) {
        Sprite s;
        s.anim.tex = g_plat.texture("Game/Effects/BigWaterParticle.dds");
        s.size = {5, 5};
        s.deep = 700;
        s.pos = p + Vec2(frand(-g_data.grassA, g_data.grassA) * 0.8f, frand(-g_data.grassB, g_data.grassB) * 0.8f - 25);
        s.color = {200, 230, 255, 220};
        s.angle = frand(0.4f, 0.8f);  // used as lifetime
        waterFx.push_back(s);
    }
    int c0 = (int)((p.x - g_data.zoneX0 - g_data.grassA) / g_data.cellW) - 1;
    int c1 = (int)((p.x - g_data.zoneX0 + g_data.grassA) / g_data.cellW) + 1;
    int r0 = (int)((p.y - g_data.zoneY0 - g_data.grassB) / g_data.cellH) - 1;
    int r1 = (int)((p.y - g_data.zoneY0 + g_data.grassB) / g_data.cellH) + 1;
    XNode gx = g_data.grassXml->root();
    for (int c = std::max(0, c0); c <= std::min(cols - 1, c1); ++c)
        for (int r = std::max(0, r0); r <= std::min(rows - 1, r1); ++r) {
            Vec2 cp = cellPos(c, r);
            float w = ellipseWeight(p, cp, g_data.grassA, g_data.grassB);
            if (w <= 0) continue;
            auto& g = grass[c][r];
            if (!g || g->dead) {
                g = std::make_unique<Grass>();
                g->pos = cp;
                g->anim.load(gx, false);
                g->anim.play = false;
                g->size = xVec(gx, "size", {48, 48});
                g->maxValue = xFloat(gx, "ProgressMaxValue", 30);
            }
            float nv = g->maxValue * w;
            if (g->value <= nv) { g->target = nv; g->flowering = true; }
        }
}

void Game::spawnPet(PetType t, bool fallFromSky, Vec2 at) {
    std::unique_ptr<Pet> p;
    if (t <= PET_COW) { auto pp = std::make_unique<ProductionPet>(); pp->load(t); p = std::move(pp); }
    else if (t == PET_DOG) { auto d = std::make_unique<Dog>(); d->load(); p = std::move(d); }
    else if (t == PET_CAT) { auto c = std::make_unique<Cat>(); c->load(); p = std::move(c); }
    else return;
    p->pos = at.x >= 0 ? at : p->randomPoint();
    if (!fallFromSky) p->fall.falling = false;
    ents.push_back(std::move(p));
}

void Game::spawnProduct(ProdType t, Vec2 at, bool drop) {
    auto p = std::make_unique<Product>();
    p->type = t;
    p->pos = {std::max(g_data.zoneX0 - 30, std::min(g_data.zoneX1 + 30, at.x)), std::max(g_data.zoneY0 - 20, std::min(g_data.zoneY1 + 40, at.y))};
    p->size = g_data.prod[t].size;
    if (drop) { p->height = -25; p->heightVel = -150; }
    if (t != P_BEAR) startFlyTip(*this, flyTip, TIP_PRODUCT, p->pos - Vec2(0, 20));
    ents.push_back(std::move(p));
}

void Game::spawnBear() {
    auto b = std::make_unique<Bear>();
    b->load(*this);
    if (!g_data.bearFall.empty()) {
        Vec2 base = g_data.bearFall[irand((int)g_data.bearFall.size())];
        b->pos = base + Vec2(frand(-40, 40) + 100, frand(-20, 20));
    } else b->pos = b->randomPoint();
    b->pos.x = std::max(g_data.zoneX0, std::min(g_data.zoneX1, b->pos.x));
    b->pos.y = std::max(g_data.zoneY0, std::min(g_data.zoneY1, b->pos.y));
    ents.push_back(std::move(b));
}

void Game::addMoney(long long v, Vec2 at, bool popup) {
    money += v;
    if (popup && v) {
        FloatText f;
        f.text = moneyText;
        f.text.mul = 0.6f;
        f.text.align = 0;
        f.text.deep = 2000;
        f.text.set("+" + std::to_string(v));
        f.pos = at;
        floats.push_back(f);
    }
}

bool Game::spend(long long v) {
    if (money < v) {
        g_audio.playSound(g_data.foulSound);
        showTip(tr("Недостаточно денег"), 2);
        warnings++;
        showHelpTip(*this, TIP_NO_MONEY, Input::mouse());
        return false;
    }
    money -= v;
    if (getenv("FF_DEBUG")) fprintf(stderr, "[t=%.1f] spend %lld -> %lld\n", time, v, money);
    return true;
}

int Game::petCount(PetType t) const {
    int n = 0;
    for (auto& e : ents)
        if (auto* p = dynamic_cast<const Pet*>(e.get()))
            if (!p->dead && p->type == t) {
                if (auto* pp = dynamic_cast<const ProductionPet*>(p))
                    if (pp->state == ProductionPet::DYING) continue;
                ++n;
            }
    return n;
}

long long Game::goalCurrent(int type) const {
    if (type == G_MONEY) return money;
    if (type >= 1 && type <= 9) return harvested[type - 1];
    if (type == G_GOOSE) return petCount(PET_GOOSE);
    if (type == G_SHEEP) return petCount(PET_SHEEP);
    if (type == G_COW) return petCount(PET_COW);
    return 0;
}

bool Game::goalsDone() const {
    for (auto& g : li.goals)
        if (goalCurrent(g.first) < g.second) return false;
    return !li.goals.empty();
}

void Game::fly(TexPtr t, Vec2 from, Vec2 to, Vec2 size, float dur, std::function<void()> done, Vec2 sizeTo) {
    FlyEffect f;
    f.tex = t;
    f.from = from;
    f.to = to;
    f.size = size;
    f.sizeTo = sizeTo.x < 0 ? size : sizeTo;
    f.dur = dur;
    f.arc = std::min(80.f, (to - from).len() * 0.25f);
    f.done = std::move(done);
    flies.push_back(f);
}

void Game::showTip(const std::string& s, float t) {
    tip = s;
    tipT = t;
}

bool Game::uiBlocks(Vec2 p) const {
    // HUD areas (top button bar, aim frame, bottom frames)
    if (p.y < 90 && (p.x < 300 || p.x > 650)) return true;
    if (p.y > 535 && (p.x < 110 || p.x > 560)) return true;
    return false;
}

void Game::onFieldClick(Vec2 p) {
    if (p.x < g_data.zoneX0 - 20 || p.x > g_data.zoneX1 + 20 || p.y < g_data.zoneY0 - 20 || p.y > g_data.zoneY1 + 30) return;
    if (well.refilling || well.water.value <= 0.001f) {
        g_audio.playSound(g_data.foulSound);
        showTip(tr("В колодце нет воды"), 2);
        warnings++;
        startFlyTip(*this, flyTip, TIP_WELL_EMPTY, well.pos, false);
        return;
    }
    float d = well.ups[std::min(well.level, (int)well.ups.size() - 1)].delta;
    well.water.value = std::max(0.f, well.water.value - d);
    water(p);
}

void Game::checkFinish() {
    if (finished || !goalsDone()) return;
    finished = true;
    g_audio.playSound("Sounds/LevelComplete.ogg");
    showLevelResults(*this);
}

void Game::togglePause() { showGameMenu(*this); }

// ---------------- update ----------------
void Game::update(float dt, bool input) {
    g_audio.playMusic(g_data.music);
    inputNow = input;
    updateClouds(dt);
    if (!input && !finished) {
        // a modal window is open: freeze the field
        return;
    }
    time += finished ? 0 : dt;

    // bears by schedule
    while (nextBear < li.bearTimes.size() && time >= li.bearTimes[nextBear]) {
        spawnBear();
        ++nextBear;
    }

    for (auto& s : li.back) s.update(dt);
    for (auto& col : grass)
        for (auto& g : col)
            if (g) {
                g->update(*this, dt);
                if (g->dead) g.reset();
            }
    well.update(*this, dt);
    depot.update(*this, dt);
    truck.update(*this, dt);
    for (auto& h : houses) h->update(*this, dt);
    // entities (iterate by index: updates can append)
    for (auto it = ents.begin(); it != ents.end(); ++it) (*it)->update(*this, dt);
    ents.remove_if([](const std::unique_ptr<Entity>& e) { return e->dead; });

    for (auto& f : flies) {
        f.t += dt;
        if (f.t >= f.dur && f.done) { auto d = f.done; f.done = nullptr; d(); }
    }
    flies.erase(std::remove_if(flies.begin(), flies.end(), [](const FlyEffect& f) { return f.t >= f.dur; }), flies.end());
    for (auto& f : floats) { f.t += dt; f.pos.y -= 30 * dt; }
    floats.erase(std::remove_if(floats.begin(), floats.end(), [](const FloatText& f) { return f.t >= f.life; }), floats.end());
    for (auto& w : waterFx) { w.angle -= dt; w.pos.y += 60 * dt; }
    waterFx.erase(std::remove_if(waterFx.begin(), waterFx.end(), [](const Sprite& s) { return s.angle <= 0; }), waterFx.end());
    if (tipT > 0) tipT -= dt;

    if (finished) return;
    flyTip.update(dt);
    updateHud(dt, input);
    tipTriggers();
    checkFinish();
}

void Game::updateHud(float dt, bool input) {
    Vec2 m = Input::mouse();
    bool consumed = false;
    for (auto k : g_plat.keysPressed)
        if (k == SDLK_ESCAPE || k == SDLK_P) { togglePause(); return; }

    // HUD buttons
    std::string c = hud.update(dt, input);
    if (c == "MenuButton") { togglePause(); return; }
    if (c == "AimButton") { showGameAim(*this); return; }
    if (!c.empty()) consumed = true;
    for (auto& b : buy) {
        b.b.enabled = money >= b.cost;
        b.coin.update(dt);
        if (b.b.update(dt, input && !consumed)) {
            consumed = true;
            if (spend(b.cost)) {
                spawnPet(b.pet, true);
                if (prof) prof->stat[ST_PETS_BOUGHT]++;
                checkCounterAwards();
            }
        }
    }
    // upgrade buttons
    auto upd = [&](Building& bd, int item) {
        bd.upgrade.show = bd.level < bd.maxLevel;
        long long cost = upgradeCost(item, bd.level);
        bd.upgrade.cost.set(std::to_string(cost));
        bd.upgrade.btn.enabled = money >= cost;
        if (bd.upgrade.show && bd.upgrade.btn.update(dt, input && !consumed)) {
            consumed = true;
            if (spend(cost)) bd.level++;
        }
    };
    upd(well, 1);
    upd(depot, 2);
    upd(truck, 3);
    for (auto& h : houses) upd(*h, 4 + h->type);
    // building places
    for (auto it = housePlaces.begin(); it != housePlaces.end(); ++it) {
        it->btn.enabled = it->allowed && money >= it->price;
        if (it->btn.update(dt, input && !consumed)) {
            consumed = true;
            if (!it->allowed) { showTip(tr("Купите это здание в магазине"), 2); break; }
            if (spend(it->price)) {
                auto ph = std::make_unique<ProdHouse>();
                ph->load(it->type, it->posIndex);
                static const int houseShop[HOUSE_COUNT] = {SHOP_POWDER, SHOP_CAKE, SHOP_SPINNERY, SHOP_TEXTILE, SHOP_BUTTER, SHOP_CHEESE};
                ph->maxLevel = std::min(4, shopLevel(houseShop[it->type]) - 1);
                static const int houseTip[HOUSE_COUNT] = {TIP_POWDER, TIP_CAKE, TIP_SPINNERY, TIP_TEXTILE, TIP_BUTTER, TIP_CHEESE};
                Vec2 hp = ph->pos;
                HouseType ht = it->type;
                houses.push_back(std::move(ph));
                housePlaces.erase(it);
                if (ht == H_POWDER) startFlyTip(*this, flyTip, houseTip[ht], hp);
                else showHelpTip(*this, houseTip[ht], hp);
            }
            break;
        }
    }
    if (consumed || !input) return;

    // hover states
    well.hovered = well.hitTest(m);
    depot.hovered = depot.hitTest(m);
    truck.hovered = !truck.away && truck.hitTest(m);
    for (auto& h : houses) h->hovered = h->hitTest(m);

    if (!Input::pressed()) return;
    if (uiBlocks(m)) return;

    // field entities: topmost first (products, bears, pets)
    Entity* best = nullptr;
    int bestDepth = -1e9;
    for (auto& e : ents) {
        if (e->dead || !e->hit(m)) continue;
        int pri = dynamic_cast<Product*>(e.get()) ? 2000 : dynamic_cast<Bear*>(e.get()) ? 1000 : 0;
        if (dynamic_cast<Pet*>(e.get()) && !dynamic_cast<Bear*>(e.get())) continue;  // pets don't block clicks
        int d = e->depth() + pri;
        if (d > bestDepth) { bestDepth = d; best = e.get(); }
    }
    if (best) {
        if (dynamic_cast<Product*>(best)) manualClicks++;
        best->click(*this);
        return;
    }

    if (well.hovered) {
        if (well.level == 4) { well.autoOn = !well.autoOn; g_audio.playSound(well.clickSound); return; }
        if (well.canRefill()) {
            long long c = well.ups[std::min(well.level, (int)well.ups.size() - 1)].fillCost;
            if (spend(c)) {
                g_audio.playSound(well.clickSound);
                well.refilling = true;
                well.refillT = 0;
            }
        }
        return;
    }
    if (depot.hovered) {
        if (!truck.away) openMarket();
        else showTip(tr("Машина ещё не вернулась"), 2);
        return;
    }
    if (truck.hovered) { openMarket(); return; }
    for (auto& h : houses)
        if (h->hovered) { h->click(*this); return; }

    onFieldClick(m);
}

void Game::openMarket() { showMarket(*this); }

// ---------------- draw ----------------
static std::string fmtTime(float t) {
    int s = (int)t;
    char b[16];
    snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
    return b;
}

void Game::draw() {
    for (auto& s : li.back) s.draw();
    for (auto& c : clouds) c.spr.draw();
    for (auto& p : placePics) p.draw();
    for (auto& col : grass)
        for (auto& g : col)
            if (g) g->draw(*this);
    for (auto& w : waterFx) {
        Sprite s = w;
        s.angle = 0;
        s.draw();
    }
    well.draw(*this);
    depot.draw(*this);
    truck.draw(*this);
    for (auto& h : houses) h->draw(*this);
    for (auto& hp : housePlaces) {
        hp.btn.draw();
        hp.cost.draw();
        hp.coin.drawAt(hp.coin.pos, hp.btn.enabled ? 0 : 1);
    }
    for (auto& e : ents) e->draw(*this);
    for (auto& f : flies) {
        float k = std::min(1.f, f.t / f.dur);
        Vec2 p = f.from + (f.to - f.from) * k;
        p.y -= std::sin(k * kPi) * f.arc;
        DrawCmd c;
        c.tex = f.tex;
        if (!c.tex) continue;
        c.x = p.x; c.y = p.y;
        c.w = f.size.x + (f.sizeTo.x - f.size.x) * k;
        c.h = f.size.y + (f.sizeTo.y - f.size.y) * k;
        c.deep = f.deep;
        g_plat.draw(c);
    }
    for (auto& f : floats) f.text.drawAt(f.pos, 1 - f.t / f.life);
    drawHud();
    flyTip.draw();
}

void Game::drawHud() {
    hud.draw();
    for (auto& b : buy) {
        b.b.draw();
        b.price.draw();
        b.coin.drawAt(b.coin.pos, money >= b.cost ? 0 : 1);
        if (b.b.hovered) {
            for (auto& t : b.tipTexts) t.draw();
            for (auto& p : b.tipPics) p.draw();
        }
    }
    Text mt = moneyText;
    mt.set(std::to_string(money));
    mt.draw();
    Text tt = timeText;
    tt.set(fmtTime(time));
    if (time <= li.goldTime) tt.color = xColor(hud.root(), "GoldTimeColor");
    else if (time <= li.silverTime) tt.color = xColor(hud.root(), "SilverTimeColor");
    else tt.color = xColor(hud.root(), "TimeColor");
    tt.draw();

    // goals summary inside the aim frame: cycle through unfinished goals
    if (!li.goals.empty()) {
        Button& ab = hud.btn("AimButton");
        XNode an = hud.node("AimButton");
        int idx = 0;
        std::vector<int> open;
        for (size_t i = 0; i < li.goals.size(); ++i)
            if (goalCurrent(li.goals[i].first) < li.goals[i].second) open.push_back((int)i);
        if (open.empty()) for (size_t i = 0; i < li.goals.size(); ++i) open.push_back((int)i);
        idx = open[(int)(SDL_GetTicks() / 3000) % open.size()];
        auto& g = li.goals[idx];
        TexPtr icon = goalIcon(g.first, true);
        if (icon) {
            DrawCmd c;
            c.tex = icon;
            Vec2 ip = ab.spr.pos + xVec(an.child("IconProto"), "position");
            Vec2 is = xVec(an.child("IconProto"), "size", {26, 26});
            c.x = ip.x; c.y = ip.y; c.w = is.x; c.h = is.y;
            c.deep = 1201;
            g_plat.draw(c);
        }
        Text nt;
        nt.load(an.child("NumberText"));
        nt.pos = ab.spr.pos + xVec(an.child("NumberText"), "position");
        long long cur = std::min(goalCurrent(g.first), g.second);
        nt.set(std::to_string(cur) + "/" + std::to_string(g.second));
        nt.draw();
        Progress pr;
        pr.load(an.child("Progress"), ab.spr.pos);
        Progress sub;
        sub.load(an.child("Substrate"), ab.spr.pos);
        sub.side = 2;
        sub.value = sub.maxValue = 1;
        long long done = 0, total = 0;
        for (auto& gg : li.goals) { total += 1; done += goalCurrent(gg.first) >= gg.second ? 1 : 0; }
        pr.maxValue = 1;
        pr.value = (float)done / total;
        sub.draw();
        pr.draw();
    }

    // truck on the road
    if (truck.away) {
        minimap.draw();
        float k = truck.tripT / truck.tripTime();
        Vec2 p;
        Sprite h = k < 0.5f ? horseTo : horseFrom;
        float kk = k < 0.5f ? k * 2 : (1 - k) * 2;
        p = farmPos + (marketPos - farmPos) * kk;
        if (!horseFrames.empty()) h.anim.tex = horseFrames[std::min((int)horseFrames.size() - 1, truck.level)];
        h.drawAt(p);
        if (!truck.paid) {
            Text t = minimapCost;
            t.set(std::to_string(truck.cargoValue));
            t.drawAt(p + Vec2(-12, 19));
        }
    }
    if (tipT > 0 && !tip.empty()) {
        Text t = tipLine;
        t.set(tip);
        t.draw(std::min(1.f, tipT));
    }
}

// ---------------- clouds ----------------
void Game::loadClouds() {
    auto x = loadXml("Clouds.xml");
    XNode r = x->root();
    if (!r) return;
    cloudPeriod[0] = xVec(r, "NearPeriod", {20, 40});
    cloudPeriod[1] = xVec(r, "FarPeriod", {200, 400});
    cloudY = xVec(r, "YOutputInterval", {20, 60});
    cloudsLeftToRight = irand(2) == 0;
    for (XNode c : r.children()) {
        if (c.type() != pugi::node_element) continue;
        Sprite s;
        s.load(c);
        s.angle = xFloat(c, "Speed", 6);  // keep speed alongside the sprite
        cloudProtos[xInt(c, "type") ? 1 : 0].push_back(s);
    }
    int n0 = xInt(r, "MaxStartNearClouds", 2), n1 = xInt(r, "MaxStartFarClouds", 1);
    for (int i = 0; i < n0; ++i) spawnCloud(0, true);
    for (int i = 0; i < n1; ++i) spawnCloud(1, true);
    for (int t = 0; t < 2; ++t) cloudNext[t] = frand(cloudPeriod[t].x, cloudPeriod[t].y);
}

void Game::spawnCloud(int type, bool anywhere) {
    auto& v = cloudProtos[type];
    if (v.empty()) return;
    Cloud c;
    c.spr = v[irand((int)v.size())];
    c.speed = c.spr.angle;
    c.spr.angle = 0;
    float y = frand(cloudY.x, cloudY.y);
    float x = anywhere ? frand(0, 800) : (cloudsLeftToRight ? -c.spr.size.x * 0.5f + 1 : 800 + c.spr.size.x * 0.5f - 1);
    c.spr.pos = {x, y};
    clouds.push_back(c);
}

void Game::updateClouds(float dt) {
    for (int t = 0; t < 2; ++t) {
        cloudTimer[t] += dt;
        if (cloudTimer[t] >= cloudNext[t]) {
            cloudTimer[t] = 0;
            cloudNext[t] = frand(cloudPeriod[t].x, cloudPeriod[t].y);
            spawnCloud(t, false);
        }
    }
    float dir = cloudsLeftToRight ? 1.f : -1.f;
    for (auto& c : clouds) c.spr.pos.x += dir * c.speed * dt;
    clouds.erase(std::remove_if(clouds.begin(), clouds.end(), [](const Cloud& c) {
        return c.spr.pos.x < -c.spr.size.x || c.spr.pos.x > 800 + c.spr.size.x;
    }), clouds.end());
}

// ---------------- tips that depend on the field state ----------------
void Game::tipTriggers() {
    if (tipBusy()) return;
    if (!introShown) {
        introShown = true;
        if (showHelpTip(*this, TIP_AIM, hud.btn("AimButton").spr.pos)) return;
    }
    if (tipAvailable(TIP_WATER)) { showWaterTip(*this); return; }
    // buy goose hint when a goose is affordable and part of the goals
    for (auto& g : li.goals)
        if (g.first == G_GOOSE && money >= buy[0].cost && startFlyTip(*this, flyTip, TIP_BUY_GOOSE, buy[0].b.spr.pos)) return;
    for (auto& hp : housePlaces)
        if (hp.allowed && startFlyTip(*this, flyTip, TIP_HOUSEPLACE, hp.btn.spr.pos)) return;
    auto upTip = [&](Building& b) {
        return b.upgrade.show && b.upgrade.btn.enabled && showHelpTip(*this, TIP_UPGRADE, b.upgrade.btn.spr.pos);
    };
    if (upTip(well) || upTip(depot) || upTip(truck)) return;
    for (auto& h : houses) if (upTip(*h)) return;
    if (depot.used() > 0 && !truck.away && showHelpTip(*this, TIP_TRUCK, truck.pos)) return;
    if (well.level == 4 && startFlyTip(*this, flyTip, TIP_WELL_AUTO, well.pos)) return;
    // awards that depend on the field
    int bears = 0;
    for (auto& e : ents)
        if (auto* b = dynamic_cast<Bear*>(e.get())) if (!b->dead && !b->leaving) ++bears;
    if (bears >= awardData(A_MANY_BEARS)) giveAward(A_MANY_BEARS);
    // "all shelves filled": the biggest depot holding every product, from eggs to a caged bear
    if (depot.level >= awardData(A_DEPOT_FULL)) {
        bool all = true;
        for (int c : depot.count) if (c <= 0) all = false;
        if (all) giveAward(A_DEPOT_FULL);
    }
    // "unhurried": granted on the spot at the 30th minute of a level
    if (time >= awardData(A_LONG_LEVEL)) giveAward(A_LONG_LEVEL);
}

void Game::onLevelComplete() {
    if (!prof) return;
    auto& st = prof->stat;
    st[ST_NO_BEAR_SELL_STREAK] = bearsSold == 0 ? st[ST_NO_BEAR_SELL_STREAK] + 1 : 0;
    st[ST_PET_SAFE_STREAK] = petsLost == 0 ? st[ST_PET_SAFE_STREAK] + 1 : 0;
    st[ST_NO_TIPS_STREAK] = warnings == 0 ? st[ST_NO_TIPS_STREAK] + 1 : 0;
    if (st[ST_NO_BEAR_SELL_STREAK] >= awardData(A_NO_BEAR_SELL)) giveAward(A_NO_BEAR_SELL);
    if (st[ST_PET_SAFE_STREAK] >= awardData(A_PET_SAFE)) giveAward(A_PET_SAFE);
    if (st[ST_NO_TIPS_STREAK] >= 3) giveAward(A_NO_TIPS);
    int harvestedAny = 0;
    for (int h : harvested) harvestedAny += h;
    if (manualClicks == 0 && harvestedAny > 0) giveAward(A_CATS_ONLY);
    if (bearsAppeared > 0 && bearsCaught == 0) giveAward(A_NO_CATCH);
    removeFile(g_profiles.savePath(*prof));
}
