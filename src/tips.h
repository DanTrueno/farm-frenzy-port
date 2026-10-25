// Tutorial tips: modal help windows (UI/Tips.xml), flying hints next to objects,
// the watering tip (UI/WaterTips.xml) and the two-page help (UI/RoundHelpWindow.xml).
// Tip source strings are extracted at build time from the original farm.wrp.exe into
// Data/tips.txt and translated through sys/strings.xml.
#pragma once
#include "engine.h"

class Game;

enum TipId {
    TIP_GOOSE = 0, TIP_DOG = 1, TIP_CAT = 2, TIP_COW = 3, TIP_SHEEP = 4, TIP_BEAR = 5, TIP_PRODUCT = 6,
    TIP_HOUSEPLACE = 7, TIP_POWDER = 8, TIP_SPINNERY = 9, TIP_CAKE = 10, TIP_TEXTILE = 11, TIP_BUTTER = 12,
    TIP_CHEESE = 13, TIP_TRUCK = 16, TIP_WELL_EMPTY = 17, TIP_WELL_AUTO = 18, TIP_DEPOT_FULL = 19,
    TIP_UPGRADE = 22, TIP_AIM = 25, TIP_HUNGRY = 28, TIP_BEAR_CLICK = 29, TIP_BUY_GOOSE = 32, TIP_NO_MONEY = 34,
    TIP_WATER = 40,  // WaterTips.xml (own text)
    TIP_COUNT = 64
};

void tipsLoad(const std::string& dataDir);
bool tipAvailable(int id);   // text known and not yet seen by this profile
void tipMarkSeen(int id);

// modal tip pointing at target; returns true if shown
bool showHelpTip(Game& g, int id, Vec2 target);
void showWaterTip(Game& g);
void showRoundHelp();

// non-modal hint bubble, owned by the game
struct FlyTip {
    int id = -1;
    Vec2 target;
    float t = 0, life = 6;
    std::u32string text;
    bool active() const { return id >= 0; }
    void update(float dt);
    void draw() const;
};
bool startFlyTip(Game& g, FlyTip& ft, int id, Vec2 target, bool once = true);
