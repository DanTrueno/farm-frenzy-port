// Well, depot, truck, production houses and building places.
#include "game.h"
#include "vfs.h"
#include <algorithm>
#include <cmath>

// ---------------- shared ----------------
void UpgradeButton::load(XNode n, Vec2 origin) {
    btn.load(n, origin);
    if (XNode t = n.child("Text")) {
        cost.load(t);
        // the text position in these files is relative to the building
        cost.pos = cost.pos + origin;
        if (XNode c = t.child("Coin")) {
            coin.load(c);
            coin.pos = cost.pos + coin.pos;
        }
    }
    btn.hasText = false;
}

void UpgradeButton::draw() {
    if (!show) return;
    btn.draw();
    cost.draw();
    coin.drawAt(coin.pos, btn.enabled ? 0 : 1);
}

void Building::loadUpgrades(XNode ups) {
    for (XNode u : ups.children()) {
        if (u.type() != pugi::node_element) continue;
        Anim a;
        a.load(u, false);
        upAnims.push_back(a);
        Anim h;
        if (XNode hv = u.child("Hovered")) h.load(hv, false);
        upHover.push_back(h);
        upSizes.push_back(xVec(u, "size", spr.size));
    }
}

bool Building::hitTest(Vec2 p) const {
    float hw = spr.size.x * 0.5f, hh = spr.size.y * 0.5f;
    if (std::fabs(p.x - pos.x) > hw || std::fabs(p.y - pos.y) > hh) return false;
    float u = (p.x - pos.x) / spr.size.x + 0.5f, v = (p.y - pos.y) / spr.size.y + 0.5f;
    if (spr.flipX) u = 1 - u;
    if (spr.anim.mask) return spr.anim.mask->opaqueAt(u, v);
    if (level < (int)upAnims.size() && upAnims[level].tex) {
        const Anim& a = upAnims[level];
        float u0, v0, u1, v1;
        a.uv(-1, u0, v0, u1, v1);
        return a.tex->opaqueAt(u0 + (u1 - u0) * u, v0 + (v1 - v0) * v);
    }
    return true;
}

void Building::drawBody(int deep, float alpha) {
    if (level >= (int)upAnims.size()) return;
    Sprite s = spr;
    const Anim& h = upHover[level];
    s.anim = (hovered && h.tex) ? h : upAnims[level];
    if (hovered && h.tex && h.frames == upAnims[level].frames) s.anim.time = upAnims[level].time, s.anim.frame = upAnims[level].frame;
    s.deep = deep;
    s.drawAt(pos, -1, alpha);
}

// ---------------- Well ----------------
void Well::load() {
    auto x = loadXml("Houses/Well.xml");
    XNode r = x->root();
    spr.load(r);
    pos = spr.pos;
    clickSound = xStr(r.child("Sounds"), "Click");
    water.load(r.child("Progress"), pos);
    substrate.load(r.child("SubstrateProgress"), pos);
    substrate.value = substrate.maxValue;
    waterMax = water.maxValue;
    water.value = waterMax;
    upgrade.load(r.child("UpgradeButton"), pos);
    loadUpgrades(r.child("Upgrades"));
    for (XNode u : r.child("Upgrades").children()) {
        if (u.type() != pugi::node_element) continue;
        Up up;
        up.reload = xFloat(u, "ReloadTime", 3);
        up.delta = xFloat(u, "DeltaWater", 20);
        up.fillCost = xInt(u, "FillWellCost", 19);
        ups.push_back(up);
    }
    if (XNode c = r.child("CostText")) {
        costText.load(c, pos);
        if (XNode cn = c.child("Coin")) { costCoin.load(cn); costCoin.pos = costText.pos + costCoin.pos; }
    }
    if (XNode a = r.child("ArrowPointer")) arrow.load(a, pos);
}

void Well::update(Game& g, float dt) {
    for (auto& a : upAnims) a.update(dt);
    for (auto& a : upHover) a.update(dt);
    arrow.update(dt);
    if (refilling) {
        if (level < (int)upAnims.size()) upAnims[level].play = true;
        refillT += dt;
        float dur = ups[std::min(level, (int)ups.size() - 1)].reload;
        water.value = std::min(waterMax, refillT / dur * waterMax);
        if (refillT >= dur) { refilling = false; water.value = waterMax; }
    } else if (level < (int)upAnims.size()) {
        upAnims[level].play = false;
        upAnims[level].frame = 0;
    }
    // top level: automatic sprinkler while switched on
    if (level == 4 && autoOn) {
        autoTimer += dt;
        if (autoTimer > 2.0f) {
            autoTimer = 0;
            long long c = ups[4].fillCost;
            if (g.money >= c) {
                g.money -= c;
                g.water({frand(g_data.zoneX0, g_data.zoneX1), frand(g_data.zoneY0, g_data.zoneY1)});
            }
        }
    }
    costText.set(std::to_string(ups[std::min(level, (int)ups.size() - 1)].fillCost));
}

void Well::draw(Game& g) {
    (void)g;
    drawBody((int)pos.y + 40);
    substrate.drawAt(substrate.pos);
    water.drawAt(water.pos);
    if (canRefill()) {
        costText.draw();
        costCoin.drawAt(costCoin.pos, g.money >= ups[std::min(level, (int)ups.size() - 1)].fillCost ? 0 : 1);
        arrow.draw();
    }
    upgrade.draw();
}

// ---------------- Depot ----------------
void Depot::load() {
    auto x = loadXml("Houses/depot.xml");
    XNode r = x->root();
    spr.load(r);
    pos = spr.pos;
    for (XNode p : r.child("ProductPlaces").children()) {
        if (p.type() != pugi::node_element) continue;
        Vec2 v;
        sscanf(p.text().get(), "%f %f", &v.x, &v.y);
        places.push_back(v);
    }
    int i = 0;
    for (XNode p : r.child("Productions").children()) {
        if (p.type() != pugi::node_element || i >= PROD_COUNT) continue;
        mini[i].load(p);
        mini[i].anim.play = false;
        mini[i].anim.frames = 1;
        mini[i].anim.cols = mini[i].anim.rows = 1;
        ++i;
    }
    fullness.load(r.child("Fullness"), pos);
    substrate.load(r.child("SubstrateProgress"), pos);
    substrate.value = substrate.maxValue;
    upgrade.load(r.child("UpgradeButton"), pos);
    loadUpgrades(r.child("Upgrades"));
    for (XNode u : r.child("Upgrades").children())
        if (u.type() == pugi::node_element) capacity.push_back(xInt(u, "Capacity", 60));
    if (XNode a = r.child("ArrowPointer")) arrow.load(a, pos);
}

float Depot::used() const {
    float u = 0;
    for (int i = 0; i < PROD_COUNT; ++i) u += count[i] * g_data.prod[i].depotSize;
    return u;
}

bool Depot::canAdd(ProdType t) const { return used() + g_data.prod[t].depotSize <= cap() + 1e-3f; }

void Depot::add(ProdType t, int n) {
    count[t] += n;
    showFull = 3;
}

int Depot::remove(ProdType t, int n) {
    int r = std::min(n, count[t]);
    count[t] -= r;
    if (r) showFull = 3;
    return r;
}

void Depot::update(Game& g, float dt) {
    (void)g;
    showFull = std::max(0.f, showFull - dt);
    arrow.update(dt);
}

void Depot::draw(Game& g) {
    (void)g;
    drawBody((int)pos.y + 30);
    // little products stacked inside, proportional to fullness
    float c = cap();
    int slotsUsed = (int)std::ceil(used() / c * places.size() - 1e-3f);
    int slot = 0;
    for (int t = 0; t < PROD_COUNT && slot < slotsUsed; ++t) {
        if (!count[t]) continue;
        int n = std::max(1, (int)std::round(count[t] * g_data.prod[t].depotSize / c * places.size()));
        for (int k = 0; k < n && slot < slotsUsed && slot < (int)places.size(); ++k, ++slot) {
            Sprite s = mini[t];
            s.deep = (int)pos.y + 31 + slot;
            s.drawAt(pos + places[places.size() - 1 - slot]);
        }
    }
    if (showFull > 0 || hovered) {
        Progress f = fullness;
        f.maxValue = c;
        f.value = used();
        substrate.drawAt(substrate.pos);
        f.drawAt(f.pos);
    }
    upgrade.draw();
}

// ---------------- Truck ----------------
void Truck::load() {
    auto x = loadXml("Houses/telega.xml");
    XNode r = x->root();
    spr.load(r);
    pos = spr.pos;
    upgrade.load(r.child("UpgradeButton"), pos);
    loadUpgrades(r.child("Upgrades"));
    for (XNode u : r.child("Upgrades").children())
        if (u.type() == pugi::node_element) period.push_back(xFloat(u, "HorsePeriod", 20));
    auto t = loadXml("UI/ToMarket/telegaUI.xml");
    for (XNode u : t->root().child("Upgrades").children()) {
        if (u.type() != pugi::node_element) continue;
        int n = 0;
        for (XNode p : u.child("ProductPlaces").children()) if (p.type() == pugi::node_element) ++n;
        slotsPerLevel.push_back(std::max(1, n));
    }
}

void Truck::update(Game& g, float dt) {
    if (!away) return;
    tripT += dt;
    float T = tripTime();
    if (!paid && tripT >= T * 0.5f) {
        paid = true;
        g.addMoney(cargoValue, {400, 560}, false);
        g_audio.playSound("Sounds/money.ogg");
    }
    if (tripT >= T) {
        away = false;
        tripT = 0;
        boxes.clear();
    }
}

void Truck::draw(Game& g) {
    (void)g;
    if (away) return;
    drawBody((int)pos.y + 20);
    upgrade.draw();
}

// ---------------- Production houses ----------------
static const char* kHouseXml[HOUSE_COUNT] = {"Houses/DriedEggsHouse.xml", "Houses/CakeHouse.xml",
                                             "Houses/SpinneryHouse.xml",  "Houses/TextileHouse.xml",
                                             "Houses/ButterHouse.xml",    "Houses/CheeseHouse.xml"};
static const char* kPlaceXml[HOUSE_COUNT] = {"Houses/Driedeggs_houseplace.xml", "Houses/cake_houseplace.xml",
                                             "Houses/spinnery_houseplace.xml",  "Houses/textile_houseplace.xml",
                                             "Houses/butter_houseplace.xml",    "Houses/cheese_houseplace.xml"};

void ProdHouse::load(HouseType t, int p) {
    type = t;
    posIndex = p;
    static const ProdType ins[HOUSE_COUNT] = {P_EGG, P_DRIED, P_WOOL, P_STRINGS, P_MILK, P_BUTTER};
    static const ProdType outs[HOUSE_COUNT] = {P_DRIED, P_CAKE, P_STRINGS, P_TEXTILE, P_BUTTER, P_CHEESE};
    in = ins[t];
    out = outs[t];
    auto x = loadXml(kHouseXml[t]);
    XNode r = x->root();
    spr.load(r);
    spr.deep = 0;
    pos = p < (int)g_data.places.size() ? g_data.places[p].building : Vec2(400, 300);
    flip = false;
    clickSound = xStr(r.child("Sounds"), "Click");
    progress.load(r.child("Progress"), pos);
    substrate.load(r.child("SubstrateProgress"), pos);
    substrate.value = substrate.maxValue;
    upgrade.load(r.child("UpgradeButton"), pos);
    loadUpgrades(r.child("Upgrades"));
    for (XNode u : r.child("Upgrades").children()) {
        if (u.type() != pugi::node_element) continue;
        prodNum.push_back(xInt(u, "ProductionNum", 1));
        prodTime.push_back(xFloat(u, "MaxProductionTime", 15));
    }
}

void ProdHouse::click(Game& g) {
    if (busy) return;
    if (g.depot.count[in] <= 0) {
        g.showTip(tr("Нет продуктов для переработки"), 2);
        g_audio.playSound(g_data.foulSound);
        return;
    }
    g_audio.playSound(clickSound);
    int n = std::min(prodNum[std::min(level, (int)prodNum.size() - 1)], g.depot.count[in]);
    g.depot.remove(in, n);
    batch = n;
    busy = true;
    waitingInput = true;
    t = 0;
    const ProductDef& d = g_data.prod[in];
    for (int i = 0; i < n; ++i)
        g.fly(d.tex, g.depot.pos + Vec2(0, -20), pos, d.size, 0.5f + 0.1f * i,
              i == n - 1 ? std::function<void()>([this] { waitingInput = false; }) : nullptr);
}

void ProdHouse::update(Game& g, float dt) {
    for (auto& a : upAnims) a.update(dt);
    for (auto& a : upHover) a.update(dt);
    if (level < (int)upAnims.size()) upAnims[level].play = busy && !waitingInput;
    if (!busy || waitingInput) return;
    t += dt;
    float T = prodTime[std::min(level, (int)prodTime.size() - 1)];
    progress.value = t / T * progress.maxValue;
    if (t >= T) {
        busy = false;
        progress.value = 0;
        for (int i = 0; i < batch; ++i) {
            Vec2 p;
            if (pos.x >= g_data.zoneX0) p = {pos.x - frand(1.f, 1.5f) * 80, pos.y + 50 + frand(0, 25)};
            else p = {pos.x + frand(1.f, 1.5f) * 80, pos.y + 50 + frand(0, 25)};
            g.spawnProduct(out, p);
        }
        batch = 0;
    }
}

void ProdHouse::draw(Game& g) {
    (void)g;
    drawBody((int)pos.y + 40);
    if (busy) {
        substrate.drawAt(substrate.pos);
        progress.drawAt(progress.pos);
    }
    upgrade.draw();
}

void HousePlace::load(HouseType t, int p) {
    type = t;
    posIndex = p;
    auto x = loadXml(kPlaceXml[t]);
    XNode r = x->root();
    Vec2 at = p < (int)g_data.places.size() ? g_data.places[p].building : Vec2(400, 300);
    btn.load(r, {});
    btn.spr.pos = at;
    btn.hasText = false;
    if (XNode tx = r.child("Text")) {
        cost.load(tx);
        cost.pos = at + xVec(tx, "position");
        if (XNode c = tx.child("Coin")) { coin.load(c); coin.pos = cost.pos + coin.pos; }
    }
}
