// Field simulation. Rules follow the decompiled cGame / cProductionPet / cGrass / cWell / cDepot /
// cProductionHouse / cBear / cDog / cCat logic; numbers come from the original XML files.
#include "game.h"
#include "portable.h"
#include "game_flow.h"
#include "vfs.h"
#include "awards.h"
#include <algorithm>
#include <cmath>

GameData g_data;

static const char* kProductXml[PROD_COUNT] = {"Products/egg.xml",    "Products/DriedEggs.xml", "Products/Cake.xml",
                                              "Products/wool.xml",   "Products/strings.xml",   "Products/textile.xml",
                                              "Products/milk.xml",   "Products/butter.xml",    "Products/cheese.xml",
                                              "Products/bearPrisoner.xml"};
static const char* kIcon[PROD_COUNT] = {"egg", "driedEggs", "cake", "wool", "Strings", "textile", "milk", "butter", "cheese", "bear"};
static const char* kPetXml[PET_COUNT] = {"Pets/Goose.xml", "Pets/Sheep.xml", "Pets/Cow.xml",
                                         "Pets/dog.xml",   "Pets/cat.xml",   "Pets/bear.xml"};
static const char* kPetIcon[PET_COUNT] = {"goose", "sheep", "cow", "", "", "bear"};

void GameData::load() {
    if (loaded) return;
    loaded = true;
    game = loadXml("Game.xml");
    XNode g = game->root();
    Vec2 a = xVec(g, "MovingZone"), b;
    {
        float v[4] = {150, 180, 650, 440};
        sscanf(xStr(g, "MovingZone", "150 180 650 440").c_str(), "%f %f %f %f", &v[0], &v[1], &v[2], &v[3]);
        zoneX0 = v[0]; zoneY0 = v[1]; zoneX1 = v[2]; zoneY1 = v[3];
    }
    (void)a; (void)b;
    productLifespan = xFloat(g, "ProductLifespan", 10);
    bearShadowTime = xFloat(g, "BearShadowChangeTime", 5);
    grassSound = xStr(g, "GrassSound");
    music = xStr(g, "Music");
    foulSound = xStr(g, "FoulSound");
    petsBoomSound = xStr(g, "PetsBoomSound");
    bearBoomSound = xStr(g, "BearBoomSound");
    XNode gr = g.child("Grass");
    Vec2 cell = xVec(gr, "NearestGrassTilesDistance", {24, 16});
    cellW = cell.x; cellH = cell.y;
    grassA = xFloat(gr, "a", 50);
    grassB = xFloat(gr, "b", 35);
    for (XNode p : g.child("BuildingPlaces").children()) {
        if (p.type() != pugi::node_element) continue;
        Place pl;
        pl.building = xVec(p, "BuildingPos");
        pl.pos = xVec(p, "position");
        pl.size = xVec(p, "size");
        pl.deep = xInt(p, "deep", -99);
        pl.pic = g_plat.texture(xStr(p, "Picture"));
        places.push_back(pl);
    }
    for (XNode it : g.child("Costs").children()) {
        if (it.type() != pugi::node_element) continue;
        std::vector<long long> v;
        for (XNode u : it.children())
            if (u.type() == pugi::node_element) v.push_back(atoll(u.text().get()));
        costs.push_back(v);
    }
    for (XNode p : g.child("BearFallPositions").children("p")) bearFall.push_back(xVec(p, "position"));
    grassXml = loadXml("grass.xml");

    auto tele = loadXml("UI/ToMarket/telegaUI.xml");
    XNode depotX = loadXml("Houses/depot.xml")->root();
    for (int i = 0; i < PROD_COUNT; ++i) {
        ProductDef& d = prod[i];
        d.xml = kProductXml[i];
        auto x = loadXml(d.xml);
        XNode r = x->root();
        if (xHas(r, "Picture")) d.tex = g_plat.texture(xStr(r, "Picture"));
        if (xHas(r, "texture_mask")) d.mask = g_plat.texture(xStr(r, "texture_mask"));
        if (xHas(r, "texture_hover")) d.hover = g_plat.texture(xStr(r, "texture_hover"));
        d.size = xVec(r, "size", {32, 32});
        d.heightOffset = xFloat(r, "HeightOffset");
        d.cost = xInt(r, "Cost");
        d.depotSize = xFloat(r, "DepotSize", 1);
        d.moveSpeed = xFloat(r, "MoveEffectSpeed", 1000);
        d.warningAlpha = xFloat(r, "WarningAlpha", 0.4f);
        d.clickSound = xStr(r.child("Sounds"), "Click");
        d.deathSound = xStr(r, "DethSound");
        std::string pn = "p" + std::to_string(i);
        XNode dp = depotX.child("Productions").child(pn.c_str());
        if (xHas(dp, "Picture")) d.mini = g_plat.texture(xStr(dp, "Picture"));
        d.icon = g_plat.texture(std::string("UI/Icons/") + kIcon[i] + ".dds");
        d.icon26 = g_plat.texture(std::string("UI/Icons/") + kIcon[i] + "26.dds");
        XNode bx = tele->root().child("ProductBoxes").child(pn.c_str());
        if (xHas(bx, "Picture")) d.box = g_plat.texture(xStr(bx, "Picture"));
        d.boxCapacity = xInt(bx, "Capacity", 1);
    }
    auto gui = loadXml("UI/GameUI.xml");
    const char* buyNames[5] = {"BuyGooseButton", "BuySheepButton", "BuyCowButton", "BuyDogButton", "BuyCatButton"};
    for (int i = 0; i < PET_COUNT; ++i) {
        pets[i].xml = loadXml(kPetXml[i]);
        if (kPetIcon[i][0]) {
            pets[i].icon = g_plat.texture(std::string("UI/Icons/") + kPetIcon[i] + ".dds");
            pets[i].icon26 = g_plat.texture(std::string("UI/Icons/") + kPetIcon[i] + "26.dds");
        }
        if (i < 3) {
            XNode bx = tele->root().child("PetBoxes").child(("p" + std::to_string(i)).c_str());
            if (xHas(bx, "Picture")) pets[i].box = g_plat.texture(xStr(bx, "Picture"));
            pets[i].boxCapacity = xInt(bx, "Capacity", 1);
        }
        if (i < 5) pets[i].cost = xInt(gui->root().child(buyNames[i]), "Cost");
    }
}

// ---------------- helpers ----------------
static float clampf(float v, float a, float b) { return std::max(a, std::min(b, v)); }

void Directional::load(XNode st) {
    const char* names[6] = {"s1IDLE", "s2MOVE_UP", "s3MOVE_UP_LEFT", "s4MOVE_LEFT", "s5MOVE_DOWN_LEFT", "s6MOVE_DOWN"};
    for (int i = 0; i < 6; ++i) {
        XNode n = st.child(names[i]);
        if (n) { states[i].load(n, true); states[i].play = true; }
    }
}

void Directional::setDir(Vec2 d) {
    float l = d.len();
    if (l < 1e-4f) return;
    float c = -d.y / l;
    float ang = std::acos(clampf(c, -1, 1));
    const float pi = kPi;
    int s;
    if (ang < pi * 0.125f) s = 1;
    else if (ang < pi * 0.375f) s = 2;
    else if (ang < pi * 0.625f) s = 3;
    else if (ang < pi * 0.875f) s = 4;
    else s = 5;
    flip = d.x > 0;
    if (s != cur) { states[s].time = states[cur].time; cur = s; }
}

bool Faller::update(float dt) {
    if (!falling) return false;
    t += dt;
    if (t >= fallTime) { falling = false; return true; }
    return false;
}
float Faller::current() const {
    if (!falling) return finalHeight;
    float k = clampf(t / fallTime, 0, 1);
    return height + (finalHeight - height) * k * k;
}

// ---------------- Grass ----------------
void Grass::update(Game& g, float dt) {
    (void)g;
    float growRate = maxValue / 0.5f;          // FloweringTime
    float decayRate = maxValue / 1200.f;       // Lifespan
    if (flowering && value < target) {
        value = std::min(target, value + growRate * dt);
    } else {
        flowering = false;
        value -= decayRate * dt;
        if (target > value) target = value;
    }
    if (value <= 0) dead = true;
    anim.update(dt);
}

void Grass::draw(Game& g) {
    (void)g;
    Sprite s;
    s.anim = anim;
    s.size = size;
    s.deep = -60;
    int f = (int)(value / maxValue * (anim.frames - 1) + 0.5f);
    s.drawAt(pos, std::max(0, std::min(anim.frames - 1, f)));
}

// ---------------- Pet base ----------------
void Pet::baseLoad(XNode r) {
    size = xVec(r, "size", {64, 64});
    speed = xFloat(r, "Speed", 50);
    dir.load(r.child("States"));
    if (XNode s = r.child("Shadow")) {
        shadow.load(s);
        shadowOff = xVec(s, "position");
        shadow.deep = -40;
    }
    fall.start(-xFloat(r, "HeightOffset", 600), -xFloat(r, "FinalHeightOffset", 30), xFloat(r, "FallingTime", 0.4f));
}

Vec2 Pet::randomPoint() const {
    return {frand(g_data.zoneX0, g_data.zoneX1), frand(g_data.zoneY0, g_data.zoneY1)};
}

bool Pet::moveTo(Vec2 t, float spd, float dt) {
    Vec2 d = t - pos;
    float l = d.len();
    if (l < 1e-3f) return true;
    dir.setDir(d);
    float step = spd * dt;
    if (step >= l) { pos = t; return true; }
    pos += d * (step / l);
    return false;
}

void Pet::drawSprite(Game& g, const Anim& a, float alpha) {
    (void)g;
    Sprite s;
    s.anim = a;
    s.size = size;
    s.deep = depth();
    s.flipX = dir.flip;
    s.drawAt({pos.x, pos.y + fall.current()}, -1, alpha);
}

void Pet::drawShadow() {
    if (!shadow.valid()) return;
    float k = 1;
    if (fall.falling) k = clampf(fall.t / fall.fallTime, 0.2f, 1);
    Sprite s = shadow;
    s.size = shadow.size * k;
    s.flipX = dir.flip;
    Vec2 off = shadowOff;
    if (dir.flip) off.x = -off.x;
    s.drawAt(pos + off);
}

bool Pet::hit(Vec2 p) const {
    Vec2 c(pos.x, pos.y + fall.current());
    return std::fabs(p.x - c.x) < size.x * 0.35f && std::fabs(p.y - c.y) < size.y * 0.35f;
}

// ---------------- ProductionPet ----------------
void ProductionPet::load(PetType t) {
    type = t;
    produces = t == PET_GOOSE ? P_EGG : t == PET_SHEEP ? P_WOOL : P_MILK;
    XNode r = g_data.pets[t].xml->root();
    baseLoad(r);
    hungryValue = xFloat(r, "HungryValue", 50);
    hungrySpeed = xFloat(r, "HungrySpeed", 3);
    hungryPart = xFloat(r, "HungryPart", 0.4f);
    prodPeriod = xFloat(r, "ProductionPeriod", 20);
    oneTimeBite = xFloat(r, "OneTimeBite", 10);
    waitTime = xFloat(r, "WaitTime", 2);
    hungryMovingSpeed = xFloat(r, "HungryMovingSpeed", 110);
    Vec2 wa = xVec(r, "WaitAppearanceTime", {10, 20});
    idleAppear = frand(wa.x, wa.y);
    idleSound = xStr(r, "IdleSound");
    deathSound = xStr(r, "DethSound");
    XNode as = r.child("AnimationStates");
    eat.load(as.child("s1EAT"), true);
    death.load(as.child("s2Death"), true);
    death.loop = false;
    lifeBar.load(r.child("Life"));
    life = hungryValue;
    prodRand = frand(0.75f, 1.25f);
    target = randomPoint();
}

static int petTip(PetType t) {
    static const int tips[PET_COUNT] = {TIP_GOOSE, TIP_SHEEP, TIP_COW, TIP_DOG, TIP_CAT, TIP_BEAR};
    return tips[t];
}

void ProductionPet::update(Game& g, float dt) {
    if (fall.update(dt)) {
        g_audio.playSound(g_data.petsBoomSound, 0.6f);
        showHelpTip(g, petTip(type), {pos.x, pos.y - size.y * 0.5f});
    }
    if (fall.falling) return;
    if (state == DYING) {
        death.update(dt);
        stateTimer += dt;
        if (stateTimer > death.length + 0.6f) dead = true;
        return;
    }
    // hunger
    if (state != EAT) life -= dt * hungrySpeed;
    if (life < 0) {
        state = DYING;
        stateTimer = 0;
        g.petsLost++;
        death.reset();
        g_audio.playSound(deathSound);
        return;
    }
    // production
    prodTimer += dt;
    if (prodTimer >= prodPeriod * prodRand) {
        prodTimer = 0;
        prodRand = frand(0.75f, 1.25f);
        g.spawnProduct(produces, pos);
    }
    bool hungry = life < hungryValue * hungryPart;
    if (hungry) showHelpTip(g, TIP_HUNGRY, {pos.x, pos.y - size.y * 0.5f});
    switch (state) {
        case WALK:
            if (hungry) { state = TO_GRASS; break; }
            idleTimer += dt;
            if (idleTimer > idleAppear) {
                idleTimer = 0;
                state = IDLE;
                stateTimer = 0;
                eat.reset();
                if (irand(3) == 0) g_audio.playSound(idleSound, 0.6f);
                break;
            }
            dir.states[dir.cur].update(dt);
            if (moveTo(target, speed, dt)) target = randomPoint();
            break;
        case IDLE:
            eat.update(dt);
            stateTimer += dt;
            if (hungry) state = TO_GRASS;
            else if (stateTimer > waitTime) { state = WALK; target = randomPoint(); }
            break;
        case TO_GRASS: {
            Grass* gr = g.closestGrass(pos);
            dir.states[dir.cur].update(dt);
            if (!gr) {
                if (moveTo(target, hungryMovingSpeed, dt)) target = randomPoint();
                break;
            }
            if (moveTo(gr->pos, hungryMovingSpeed, dt)) { state = EAT; biteTimer = 0; eat.reset(); }
            break;
        }
        case EAT: {
            eat.update(dt);
            Grass* gr = g.closestGrass(pos);
            if (!gr || (gr->pos - pos).len() > 6) {
                // grass moved away/eaten: continue searching or stop when full
                if (life >= hungryValue) { life = hungryValue; state = WALK; target = randomPoint(); }
                else state = TO_GRASS;
                break;
            }
            biteTimer += dt;
            if (biteTimer >= eat.length * 0.5f) {
                biteTimer = 0;
                float remaining = std::max(0.f, gr->value - oneTimeBite);
                float eaten = gr->value - remaining;
                gr->value = remaining;
                gr->target = std::min(gr->target, remaining);
                life += eaten;
                if (remaining <= 0) {
                    gr->dead = true;
                    if (life < hungryValue) state = TO_GRASS;
                    else { life = hungryValue; state = WALK; target = randomPoint(); }
                } else if (life >= hungryValue) {
                    life = hungryValue;
                    state = WALK;
                    target = randomPoint();
                }
            }
            break;
        }
        default: break;
    }
}

void ProductionPet::draw(Game& g) {
    drawShadow();
    const Anim* a = &dir.states[dir.cur];
    if (state == IDLE || state == EAT) a = &eat;
    if (state == DYING) a = &death;
    if (fall.falling) a = &dir.states[5];
    float alpha = 1;
    if (state == DYING && stateTimer > death.length) alpha = 1 - (stateTimer - death.length) / 0.6f;
    drawSprite(g, *a, alpha);
    if (state != DYING && !fall.falling) {
        Progress p = lifeBar;
        p.maxValue = hungryValue;
        p.value = life;
        p.deep = 600 + (int)pos.y;
        // colour: green -> red when hungry
        float k = life / hungryValue;
        p.color = k < hungryPart ? Color{255, 80, 60, 255} : Color{255, 255, 255, 255};
        p.drawAt({pos.x + lifeBar.pos.x, pos.y + fall.current() - size.y * 0.5f + lifeBar.pos.y});
    }
}

void ProductionPet::click(Game& g) {
    (void)g;
    g_audio.playSound(idleSound, 0.8f);
}

// ---------------- Dog ----------------
void Dog::load() {
    type = PET_DOG;
    XNode r = g_data.pets[PET_DOG].xml->root();
    baseLoad(r);
    collisionRadius = xFloat(r, "CollisionRadius", 55);
    sleepPeriod = xFloat(r, "SleepingTimePeriod", 10);
    sleepTime = xFloat(r, "SleepTime", 10);
    woof.load(r.child("Woof"), true);
    gafSound = xStr(r, "GafSound");
    target = randomPoint();
}

void Dog::update(Game& g, float dt) {
    if (fall.update(dt)) {
        g_audio.playSound(g_data.petsBoomSound, 0.6f);
        showHelpTip(g, TIP_DOG, {pos.x, pos.y - size.y * 0.5f});
    }
    if (fall.falling) return;
    // the target must still be a live bear on the field
    if (targetBear) {
        bool alive = false;
        for (auto& e : g.ents)
            if (e.get() == targetBear && !e->dead && !targetBear->leaving && !targetBear->fall.falling) alive = true;
        if (!alive) { targetBear = nullptr; barking = false; target = randomPoint(); }
    }
    // pick a bear nobody barks at yet, otherwise any bear (cDog::findRandomBear)
    if (!targetBear) {
        Bear* any = nullptr;
        for (auto& e : g.ents)
            if (auto* b = dynamic_cast<Bear*>(e.get()))
                if (!b->dead && !b->leaving && !b->fall.falling && b->shadowT < 0) {
                    if (!any) any = b;
                    if (b->barkers == 0) { targetBear = b; break; }
                }
        if (!targetBear) targetBear = any;
    }
    if (barking) {
        // keeps barking next to the bear until it is caught or escapes
        woof.update(dt);
        gafTimer += dt;
        if (gafTimer > 1.5f) { gafTimer = 0; g_audio.playSound(gafSound, 0.7f); }
        if (targetBear) dir.flip = targetBear->pos.x > pos.x;
        return;
    }
    if (targetBear) {
        // runs to the bear at double speed (dog chase task)
        dir.states[dir.cur].update(dt);
        moveTo(targetBear->pos, speed * 2.0f, dt);
        if ((targetBear->pos - pos).len() < collisionRadius) {
            barking = true;
            gafTimer = 0;
            woof.reset();
            g_audio.playSound(gafSound);
            targetBear->stopped = true;
            targetBear->barkers++;
        }
        return;
    }
    dir.states[dir.cur].update(dt);
    if (moveTo(target, speed * 0.6f, dt)) target = randomPoint();
}

void Dog::draw(Game& g) {
    drawShadow();
    drawSprite(g, barking ? woof : dir.states[dir.cur]);
}

// ---------------- Cat ----------------
void Cat::load() {
    type = PET_CAT;
    XNode r = g_data.pets[PET_CAT].xml->root();
    baseLoad(r);
    sleepPeriod = xFloat(r, "SleepingTimePeriod", 10);
    sleepTime = xFloat(r, "SleepTime", 10);
    target = randomPoint();
}

void Cat::update(Game& g, float dt) {
    if (fall.update(dt)) {
        g_audio.playSound(g_data.petsBoomSound, 0.6f);
        showHelpTip(g, TIP_CAT, {pos.x, pos.y - size.y * 0.5f});
    }
    if (fall.falling) return;
    if (targetProd) {
        bool ok = false;
        for (auto& e : g.ents) if (e.get() == targetProd && !e->dead) ok = true;
        if (!ok) targetProd = nullptr;
    }
    if (!targetProd) {
        float best = 1e9f;
        for (auto& e : g.ents)
            if (auto* p = dynamic_cast<Product*>(e.get()))
                if (!p->dead && !p->claimed && p->type != P_BEAR && g.depot.canAdd(p->type)) {
                    float d = (p->pos - pos).len();
                    if (d < best) { best = d; targetProd = p; }
                }
        if (targetProd) targetProd->claimed = true;
    }
    dir.states[dir.cur].update(dt);
    if (targetProd) {
        if (moveTo(targetProd->pos, speed * 2.0f, dt)) {
            targetProd->claimed = false;
            targetProd->click(g);
            targetProd = nullptr;
        }
        return;
    }
    if (moveTo(target, speed, dt)) target = randomPoint();
}

void Cat::draw(Game& g) {
    drawShadow();
    drawSprite(g, dir.states[dir.cur]);
}

// ---------------- Bear ----------------
void Bear::load(Game& g) {
    type = PET_BEAR;
    XNode r = g_data.pets[PET_BEAR].xml->root();
    baseLoad(r);
    baseSpeed = speed = xFloat(r, "Speed", 75);
    buildSpeed = xFloat(r, "BuildSpeed", 10);
    reloadTime = xFloat(r, "ReloadTime", 0.5f);
    collisionRadius = xFloat(r, "CollisionRadius", 30);
    screamSound = xStr(r, "BearScreamSound");
    clickSound = xStr(r.child("Sounds"), "Click");
    XNode c = r.child("Cage");
    cageSpr.load(c);
    int lvl = std::min(3, g.shopLevel(SHOP_CAGE));
    int i = 0;
    for (XNode u : c.child("Upgrades").children()) {
        if (u.type() != pugi::node_element) continue;
        Anim a;
        a.load(u);
        a.play = false;
        cageAnims.push_back(a);
        if (i == lvl) cageMax = xFloat(u, "ProgressMaxValue", 100);
        ++i;
    }
    if (lvl < (int)cageAnims.size()) cageSpr.anim = cageAnims[lvl];
    fall.start(-xFloat(r, "HeightOffset", 600), -xFloat(r, "FinalHeightOffset", 51), xFloat(r, "FallingTime", 0.4f));
    finalShadow = xVec(r, "FinalShadowSize", {72, 44});
    // DefferedFallStart: the shadow grows on the ground first (cBear::taskChangeShadowSize)
    if (xBool(r, "DefferedFallStart", false)) shadowT = 0;
    target = randomPoint();
}

void Bear::update(Game& g, float dt) {
    if (shadowT >= 0) {
        shadowT += dt;
        if (shadowT < g_data.bearShadowTime) return;
        shadowT = -1;
    }
    if (fall.update(dt)) {
        g_audio.playSound(g_data.bearBoomSound);
        g_audio.playSound(screamSound, 0.7f);
        g.bearsAppeared++;
        if (!showHelpTip(g, TIP_BEAR, {pos.x, pos.y - size.y * 0.5f}))
            startFlyTip(g, g.flyTip, TIP_BEAR_CLICK, {pos.x, pos.y - size.y * 0.5f});
    }
    if (fall.falling) return;
    dir.states[dir.cur].update(dt);
    if (leaving) {
        cage = 0;
        if (moveTo(target, baseSpeed * 2.5f, dt) || pos.x < -100 || pos.x > 900) dead = true;
        return;
    }
    // cage decays
    cage = std::max(0.f, cage - dt * cageMax * 0.1f);
    speed = (1 - cage / cageMax) * baseSpeed;
    if (!stopped && moveTo(target, speed, dt)) target = randomPoint();
    // attack
    attackTimer += dt;
    if (kills < g.li.bearKillingNum && attackTimer > reloadTime) {
        attackTimer = 0;
        for (auto& e : g.ents) {
            if (e->dead || e.get() == this) continue;
            if ((e->pos - pos).len() > collisionRadius + 15) continue;
            if (auto* pp = dynamic_cast<ProductionPet*>(e.get())) {
                if (pp->fall.falling || pp->state == ProductionPet::DYING) continue;
                XNode r = g_data.pets[pp->type].xml->root();
                g_audio.playSound(xStr(r, "BearCrackDethSound"));
                pp->dead = true;
                g.petsLost++;
                ++kills;
            } else if (auto* pr = dynamic_cast<Product*>(e.get())) {
                if (pr->type == P_BEAR) continue;
                g_audio.playSound(g_data.prod[pr->type].deathSound);
                pr->dead = true;
                ++kills;
            } else if (auto* c = dynamic_cast<Cat*>(e.get())) {
                if (c->fall.falling) continue;
                g_audio.playSound(xStr(g_data.pets[PET_CAT].xml->root(), "BearCrackDethSound"));
                c->dead = true;
                g.petsLost++;
                ++kills;
            }
            if (kills >= g.li.bearKillingNum) break;
        }
    }
}

bool Bear::hit(Vec2 p) const {
    if (leaving || shadowT >= 0) return false;
    Vec2 c(pos.x, pos.y + fall.current());
    return std::fabs(p.x - c.x) < size.x * 0.4f && std::fabs(p.y - c.y) < size.y * 0.45f;
}

void Bear::click(Game& g) {
    if (fall.falling || leaving) return;
    g_audio.playSound(clickSound);
    cage += buildSpeed;
    if (cage >= cageMax) {
        dead = true;
        g.bearsCaught++;
        if (g.prof) g.prof->stat[ST_BEARS]++;
        checkCounterAwards();
        auto p = std::make_unique<Product>();
        p->type = P_BEAR;
        p->pos = pos;
        p->size = g_data.prod[P_BEAR].size;
        auto x = loadXml("Products/bearPrisoner.xml");
        int lvl = std::min(3, g.shopLevel(SHOP_CAGE));
        XNode up = x->root().child(("Up" + std::to_string(lvl)).c_str());
        p->cageAnim.load(up.child("Anim"), true);
        p->cageAnim.play = true;
        g.ents.push_back(std::move(p));
    }
}

void Bear::draw(Game& g) {
    if (shadowT >= 0) {
        // only the shadow, growing to its final size
        Sprite s = shadow;
        s.size = finalShadow * std::min(1.f, shadowT / std::max(0.01f, g_data.bearShadowTime));
        s.drawAt(pos + shadowOff);
        return;
    }
    drawShadow();
    drawSprite(g, dir.states[dir.cur]);
    if (cage > 0 && !cageAnims.empty()) {
        Sprite s = cageSpr;
        float k = cage / cageMax;
        int f = (int)(k * (s.anim.frames - 1));
        s.deep = depth() + 1;
        s.drawAt({pos.x + cageSpr.pos.x, pos.y + cageSpr.pos.y}, f);
    }
}

// ---------------- Product ----------------
void Product::update(Game& g, float dt) {
    life += dt;
    if (height < 0 || heightVel != 0) {
        heightVel += 900 * dt;
        height += heightVel * dt;
        if (height >= 0) {
            height = 0;
            heightVel = heightVel > 120 ? -heightVel * 0.3f : 0;
        }
    }
    cageAnim.update(dt);
    float span = g_data.productLifespan;
    alpha = 1;
    if (life > span * 0.66f) alpha = (std::fmod(life * 6, 1.f) < 0.5f) ? 1.f : g_data.prod[type].warningAlpha;
    if (life >= span) {
        dead = true;
        if (type == P_BEAR) {
            // the bear breaks the cage and runs away
            auto b = std::make_unique<Bear>();
            b->load(g);
            b->pos = pos;
            b->fall.falling = false;
            b->shadowT = -1;
            b->leaving = true;
            b->target = {pos.x < 400 ? -150.f : 950.f, pos.y};
            auto x = loadXml("Products/bearPrisoner.xml");
            g_audio.playSound(xStr(x->root(), "BrokeCageSound"));
            g.ents.push_back(std::move(b));
        }
    }
    hovered = hit(Input::mouse());
}

bool Product::hit(Vec2 p) const {
    const ProductDef& d = g_data.prod[type];
    Vec2 c(pos.x, pos.y - d.heightOffset + height);
    float hw = size.x * 0.5f, hh = size.y * 0.5f;
    if (std::fabs(p.x - c.x) > hw || std::fabs(p.y - c.y) > hh) return false;
    if (type == P_BEAR) return true;
    TexPtr m = d.mask ? d.mask : d.tex;
    if (!m) return true;
    float u = (p.x - c.x) / size.x + 0.5f, v = (p.y - c.y) / size.y + 0.5f;
    return m->opaqueAt(u, v) || (std::fabs(p.x - c.x) < hw * 0.6f && std::fabs(p.y - c.y) < hh * 0.6f);
}

void Product::draw(Game& g) {
    (void)g;
    const ProductDef& d = g_data.prod[type];
    Vec2 c(pos.x, pos.y - d.heightOffset + height);
    if (type == P_BEAR) {
        Sprite s;
        s.anim = cageAnim;
        s.size = size;
        s.deep = depth();
        s.drawAt(c, -1, alpha);
        return;
    }
    DrawCmd dc;
    dc.tex = hovered && d.hover ? d.hover : d.tex;
    if (!dc.tex) return;
    dc.x = c.x; dc.y = c.y;
    dc.w = size.x; dc.h = size.y;
    dc.deep = depth();
    dc.color.a = (uint8_t)(alpha * 255);
    g_plat.draw(dc);
}

void Product::click(Game& g) {
    if (dead) return;
    const ProductDef& d = g_data.prod[type];
    if (!g.depot.canAdd(type)) {
        g.showTip(tr("Склад заполнен"), 2);
        g.warnings++;
        g_audio.playSound(g_data.foulSound);
        g.depot.showFull = 3;
        showHelpTip(g, TIP_DEPOT_FULL, g.depot.pos - Vec2(0, 40));
        return;
    }
    dead = true;
    g_audio.playSound(d.clickSound);
    g.harvested[type]++;
    Vec2 from(pos.x, pos.y - d.heightOffset + height);
    ProdType t = type;
    TexPtr tex = type == P_BEAR ? d.mini : d.tex;
    Vec2 sz = type == P_BEAR ? Vec2(50, 50) : size;
    g.depot.add(t, 1);
    g.fly(tex, from, g.depot.pos + Vec2(0, -10), sz, 0.45f, nullptr, Vec2(14, 14));
    if (g.prof) g.prof->stat[ST_PRODUCTS]++;
    checkCounterAwards();
}
