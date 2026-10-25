// Awards (cAwardManager): 16 trophies described in UI/Awards.xml, thresholds in AwardData.
#pragma once
#include "engine.h"

enum AwardType {
    A_GAME_COMPLETE, A_GOLD_COMPLETE, A_LONG_LEVEL, A_BEARS, A_PRODUCTS, A_PETS, A_SHOP_FULL,
    A_NO_BEAR_SELL, A_PET_SAFE, A_DEPOT_FULL, A_MANY_BEARS, A_CATS_ONLY, A_NO_CATCH, A_ALL_AWARDS,
    A_MILLION, A_NO_TIPS, AWARD_COUNT
};

// profile stat slots
enum StatSlot {
    ST_BEARS = 0, ST_PRODUCTS = 1, ST_PETS_BOUGHT = 2, ST_NO_BEAR_SELL_STREAK = 3, ST_PET_SAFE_STREAK = 4,
    ST_NO_TIPS_STREAK = 5
};

long long awardData(int a);
// grants the award if not owned yet and queues its window; returns true when newly granted
bool giveAward(int a);
// checks counters that can complete at any time
void checkCounterAwards();
void showAwards();
