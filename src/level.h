// Level parameters: LevelsRecords.xml (rules) + Packs/N.xml (background, bear schedule).
#pragma once
#include "engine.h"
#include <vector>

enum ProdType { P_EGG, P_DRIED, P_CAKE, P_WOOL, P_STRINGS, P_TEXTILE, P_MILK, P_BUTTER, P_CHEESE, P_BEAR, PROD_COUNT };
enum PetType { PET_GOOSE, PET_SHEEP, PET_COW, PET_DOG, PET_CAT, PET_BEAR, PET_COUNT };
enum GoalType { G_MONEY = 0, G_GOOSE = 10, G_SHEEP = 11, G_COW = 12 };
enum HouseType { H_POWDER, H_CAKE, H_SPINNERY, H_TEXTILE, H_BUTTER, H_CHEESE, HOUSE_COUNT };

struct LevelInfo {
    int number = 1;
    long long startMoney = 0;
    float goldTime = 300, silverTime = 600;
    int prize = 0, silverPrize = 0, goldPrize = 0;
    int bearKillingNum = 3;
    std::vector<int> pets;
    std::vector<std::pair<int, long long>> goals;  // type, value
    struct House { int type, pos, upgrade; };
    std::vector<House> houses;
    std::vector<std::pair<int, int>> access;       // crit id, value
    std::vector<float> bearTimes;
    std::vector<Sprite> back;
};

bool loadLevelInfo(int n, LevelInfo& out);
int levelCount();

// crit id -> shop item
int critToShop(int crit);
// is the level accessible with the current profile purchases
bool levelAccessible(const LevelInfo& li);
