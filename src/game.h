// Gameplay: the farm field and everything on it.
#pragma once
#include "screen.h"
#include "ui.h"
#include "level.h"
#include "profile.h"
#include "tips.h"
#include <list>
#include <memory>

class Game;

// ---------- static data shared by all levels ----------
struct ProductDef {
    std::string xml;
    TexPtr tex, mask, hover, mini, icon, icon26, box;
    Vec2 size;
    float heightOffset = 0;
    int cost = 0;
    float depotSize = 1;
    float moveSpeed = 1000;
    float warningAlpha = 0.4f;
    int boxCapacity = 1;
    std::string clickSound, deathSound;
};

struct PetDef {
    std::shared_ptr<XmlDoc> xml;
    TexPtr icon, icon26, box;
    int boxCapacity = 1;
    int cost = 0;
};

struct GameData {
    bool loaded = false;
    std::shared_ptr<XmlDoc> game, grassXml;
    ProductDef prod[PROD_COUNT];
    PetDef pets[PET_COUNT];
    float zoneX0 = 150, zoneY0 = 180, zoneX1 = 650, zoneY1 = 440;
    float productLifespan = 10;
    float bearShadowTime = 5;  // BearShadowChangeTime: shadow shows before a bear drops
    std::string grassSound, music, foulSound, petsBoomSound, bearBoomSound;
    float grassA = 50, grassB = 35, cellW = 24, cellH = 16;
    struct Place { Vec2 building, pos, size; int deep; TexPtr pic; };
    std::vector<Place> places;
    std::vector<std::vector<long long>> costs; // [item][level]
    std::vector<Vec2> bearFall;
    void load();
};
extern GameData g_data;

// ---------- field objects ----------
struct Entity {
    Vec2 pos;          // ground position
    bool dead = false;
    virtual ~Entity() = default;
    virtual void update(Game& g, float dt) = 0;
    virtual void draw(Game& g) = 0;
    virtual bool hit(Vec2 p) const { (void)p; return false; }
    virtual void click(Game& g) { (void)g; }
    virtual int depth() const { return (int)pos.y; }
};

struct Directional {
    Anim states[6];  // idle, up, up_left, left, down_left, down
    int cur = 0;
    bool flip = false;
    void load(XNode statesNode);
    void setDir(Vec2 d);
};

struct Grass : Entity {
    float value = 0, target = 0, maxValue = 30;
    bool flowering = false;
    Anim anim;
    Vec2 size;
    void update(Game& g, float dt) override;
    void draw(Game& g) override;
};

struct Faller {
    // pets drop from the sky when bought
    float height = 0, finalHeight = 0, fallTime = 0.4f, t = 0;
    bool falling = true;
    void start(float from, float to, float time) { height = from; finalHeight = to; fallTime = time; t = 0; falling = true; }
    bool update(float dt);  // returns true when landing this frame
    float current() const;
};

struct Pet : Entity {
    PetType type;
    Directional dir;
    Sprite shadow;
    Vec2 shadowOff;
    Vec2 size;
    float speed = 50;
    Vec2 target;
    bool moving = false;
    Faller fall;
    void baseLoad(XNode root);
    bool moveTo(Vec2 t, float spd, float dt);  // true when arrived
    Vec2 randomPoint() const;
    void drawSprite(Game& g, const Anim& a, float alpha = 1);
    void drawShadow();
    bool hit(Vec2 p) const override;
};

struct ProductionPet : Pet {
    enum State { WALK, IDLE, TO_GRASS, EAT, DYING } state = WALK;
    float life = 0, hungryValue = 50, hungrySpeed = 3, hungryPart = 0.4f;
    float prodPeriod = 20, prodTimer = 0, prodRand = 1;
    float oneTimeBite = 10, waitTime = 2, idleTimer = 0, idleAppear = 15, stateTimer = 0, biteTimer = 0;
    float hungryMovingSpeed = 110;
    Anim eat, death;
    Progress lifeBar;
    std::string idleSound, deathSound;
    ProdType produces;
    void load(PetType t);
    void update(Game& g, float dt) override;
    void draw(Game& g) override;
    void click(Game& g) override;
};

struct Bear;

struct Dog : Pet {
    Anim woof;
    float collisionRadius = 55;
    float stateTimer = 0, sleepPeriod = 10, sleepTime = 10;
    bool sleeping = false, barking = false;
    float barkTimer = 0, gafTimer = 0;
    std::string gafSound;
    Bear* targetBear = nullptr;
    void load();
    void update(Game& g, float dt) override;
    void draw(Game& g) override;
};

struct Product;

struct Cat : Pet {
    Product* targetProd = nullptr;
    float sleepTimer = 0, sleepPeriod = 10, sleepTime = 10;
    bool sleeping = false;
    void load();
    void update(Game& g, float dt) override;
    void draw(Game& g) override;
};

struct Bear : Pet {
    float cage = 0, cageMax = 100, buildSpeed = 10;
    float reloadTime = 0.5f, attackTimer = 0, collisionRadius = 30;
    int kills = 0;
    float baseSpeed = 75;
    bool leaving = false;
    bool stopped = false;  // a dog barked at it: it stands still (cBear 0x3f1)
    float shadowT = -1;    // >= 0 while only the growing shadow is on the field
    Vec2 finalShadow{72, 44};
    int barkers = 0;       // dogs barking at this bear
    Sprite cageSpr;
    std::vector<Anim> cageAnims;
    std::string clickSound, screamSound;
    void load(Game& g);
    void update(Game& g, float dt) override;
    void draw(Game& g) override;
    void click(Game& g) override;
    bool hit(Vec2 p) const override;
    int depth() const override { return (int)pos.y + 5; }
};

struct Product : Entity {
    ProdType type;
    float life = 0;
    float height = 0, heightVel = 0;  // small bounce when dropped
    float alpha = 1;
    bool hovered = false;
    bool claimed = false;  // cat heading to it
    Anim cageAnim;          // for caged bear
    Vec2 size;
    void update(Game& g, float dt) override;
    void draw(Game& g) override;
    bool hit(Vec2 p) const override;
    void click(Game& g) override;
};

// flying icon effect (product -> depot etc.)
struct FlyEffect {
    TexPtr tex;
    Vec2 from, to, size, sizeTo;
    float t = 0, dur = 0.5f, arc = 0;
    int deep = 900;
    std::function<void()> done;
};

// floating text (money popups)
struct FloatText {
    Text text;
    Vec2 pos;
    float t = 0, life = 1.2f;
};

// ---------- buildings ----------
struct UpgradeButton {
    Button btn;
    Text cost;
    Sprite coin;
    bool show = false;
    void load(XNode n, Vec2 origin);
    void draw();
};

struct Building {
    Vec2 pos;
    int level = 0, maxLevel = 0;
    std::vector<Anim> upAnims, upHover;
    std::vector<Vec2> upSizes;
    Sprite spr;
    UpgradeButton upgrade;
    bool hovered = false;
    std::string clickSound;
    Text tipName;
    void loadUpgrades(XNode ups);
    bool hitTest(Vec2 p) const;
    void drawBody(int deep, float alpha = 1);
};

struct Well : Building {
    Progress water, substrate;
    float waterMax = 100;
    struct Up { float reload = 3, delta = 20; long long fillCost = 19; };
    std::vector<Up> ups;
    bool refilling = false, autoOn = true;
    float refillT = 0, autoTimer = 0;
    Text costText;
    Sprite costCoin;
    Sprite arrow;
    void load();
    void update(Game& g, float dt);
    void draw(Game& g);
    bool canRefill() const { return !refilling && water.value <= 0.001f; }
};

struct Depot : Building {
    std::vector<Vec2> places;
    Sprite mini[PROD_COUNT];
    Progress fullness, substrate;
    std::vector<int> capacity;
    int count[PROD_COUNT] = {};
    float showFull = 0;
    Sprite arrow;
    void load();
    float used() const;
    float cap() const { return capacity.empty() ? 60 : (float)capacity[std::min(level, (int)capacity.size() - 1)]; }
    bool canAdd(ProdType t) const;
    void add(ProdType t, int n = 1);
    int remove(ProdType t, int n);
    void update(Game& g, float dt);
    void draw(Game& g);
};

struct Truck : Building {
    std::vector<float> period;
    // trip state
    bool away = false;
    float tripT = 0;
    bool paid = false;
    long long cargoValue = 0;
    // loaded cargo (boxes)
    struct Box { bool pet = false; int type = 0; int count = 0; };
    std::vector<Box> boxes;
    std::vector<int> slotsPerLevel;
    void load();
    void update(Game& g, float dt);
    void draw(Game& g);
    int slots() const { return slotsPerLevel.empty() ? 2 : slotsPerLevel[std::min(level, (int)slotsPerLevel.size() - 1)]; }
    float tripTime() const { return period.empty() ? 20 : period[std::min(level, (int)period.size() - 1)]; }
};

struct ProdHouse : Building {
    HouseType type;
    ProdType in, out;
    int posIndex = 0;
    bool flip = false;
    std::vector<int> prodNum;
    std::vector<float> prodTime;
    Progress progress, substrate;
    bool busy = false;
    bool waitingInput = false;
    float t = 0;
    int batch = 0;
    void load(HouseType t, int pos);
    void update(Game& g, float dt);
    void draw(Game& g);
    void click(Game& g);
};

struct HousePlace {
    HouseType type;
    int posIndex;
    Button btn;
    Text cost;
    Sprite coin;
    long long price = 0;
    bool allowed = true;
    void load(HouseType t, int pos);
};

// ---------- the game screen ----------
class Game : public Screen {
public:
    explicit Game(int level);
    ~Game() override;
    void update(float dt, bool input) override;
    void draw() override;

    LevelInfo li;
    Profile* prof = nullptr;
    long long money = 0;
    float time = 0;
    bool finished = false, paused = false;
    int harvested[PROD_COUNT] = {};

    // field
    std::vector<std::vector<std::unique_ptr<Grass>>> grass;  // [col][row]
    int cols = 0, rows = 0;
    std::list<std::unique_ptr<Entity>> ents;
    std::vector<FlyEffect> flies;
    std::vector<FloatText> floats;
    std::vector<Sprite> waterFx;

    Well well;
    Depot depot;
    Truck truck;
    std::vector<std::unique_ptr<ProdHouse>> houses;
    std::vector<HousePlace> housePlaces;
    std::vector<Sprite> placePics;

    // bears
    size_t nextBear = 0;
    int bearsCaught = 0;

    // UI
    UiDoc hud;
    struct BuyBtn { Button b; PetType pet; long long cost; Text price; Sprite coin; std::vector<Text> tipTexts; std::vector<Sprite> tipPics; };
    std::vector<BuyBtn> buy;
    Text moneyText, timeText;
    Sprite minimap, horseTo, horseFrom;
    std::vector<TexPtr> horseFrames;
    Vec2 farmPos, marketPos;
    Text minimapCost;
    std::vector<std::pair<Sprite, Text>> aimLines;
    Text tipLine;
    std::string tip;
    float tipT = 0;

    // helpers
    Vec2 cellPos(int c, int r) const;
    Grass* closestGrass(Vec2 p);
    void water(Vec2 p);
    void spawnPet(PetType t, bool fallFromSky = true, Vec2 at = Vec2(-1, -1));
    void spawnProduct(ProdType t, Vec2 at, bool drop = true);
    void spawnBear();
    void addMoney(long long v, Vec2 at, bool popup = true);
    bool spend(long long v);
    long long goalCurrent(int type) const;
    bool goalsDone() const;
    int petCount(PetType t) const;
    void fly(TexPtr t, Vec2 from, Vec2 to, Vec2 size, float dur, std::function<void()> done = nullptr, Vec2 sizeTo = Vec2(-1, -1));
    void showTip(const std::string& s, float t = 3);
    void onFieldClick(Vec2 p);
    void checkFinish();
    void openMarket();
    void togglePause();
    long long upgradeCost(int item, int level) const;
    int shopLevel(int item) const;
    void updateHud(float dt, bool input);
    void drawHud();
    bool uiBlocks(Vec2 p) const;

    // clouds (cClouds)
    struct Cloud { Sprite spr; float speed; };
    std::vector<Sprite> cloudProtos[2];
    std::vector<Cloud> clouds;
    float cloudTimer[2] = {0, 0}, cloudNext[2] = {0, 0};
    Vec2 cloudPeriod[2], cloudY;
    bool cloudsLeftToRight = true;
    void loadClouds();
    void spawnCloud(int type, bool anywhere);
    void updateClouds(float dt);

    // tutorial tips
    FlyTip flyTip;
    int tipsThisLevel = 0;
    bool inputNow = false;
    bool tipBusy() const { return !inputNow || flyTip.active() || finished; }
    void tipTriggers();
    bool petLanded[PET_COUNT] = {};
    bool introShown = false;

    // award counters for this level
    int bearsAppeared = 0, bearsSold = 0, petsLost = 0, manualClicks = 0;
    int warnings = 0;  // "no water" / "depot full" / "no money" hints this level
    void onLevelComplete();

    // mid-level save (cGame::saveLevelState / loadLevelState)
    bool saveState(const std::string& path) const;
    bool loadState(const std::string& path);
    static int savedLevel(const std::string& path);
};

