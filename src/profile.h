// Player profiles: progress, shop upgrades, settings. Stored as a small text file
// in ~/Library/Application Support/Melesta/FarmFrenzy/.
#pragma once
#include <string>
#include <vector>
#include <array>

enum ShopItem { SHOP_CAGE = 0, SHOP_WELL, SHOP_DEPOT, SHOP_TRUCK, SHOP_POWDER, SHOP_CAKE, SHOP_SPINNERY,
                SHOP_TEXTILE, SHOP_BUTTER, SHOP_CHEESE, SHOP_COUNT };

constexpr int MAX_LEVELS = 48;

struct Profile {
    std::string name;
    long long stars = 0;                       // shop currency
    std::array<int, MAX_LEVELS> medal{};       // 0 = not passed, 1 = passed, 2 = silver, 3 = gold
    std::array<float, MAX_LEVELS> bestTime{};  // seconds, 0 = none
    std::array<int, SHOP_COUNT> shop{};        // bought levels
    int lastLevel = 1;
    // award counters
    std::array<long long, 32> stat{};
    std::array<int, 32> awards{};
    bool tutorialSeen = false;
    std::array<int, 64> tipSeen{};             // tutorial tips already shown
    // in-progress level save
    bool hasSave = false;
};

struct Settings {
    float music = 1, sound = 1;
    bool fullscreen = false;
    bool cursor = true;
};

class Profiles {
public:
    void load();
    void save();
    Profile* current();
    std::vector<Profile> list;
    int currentIndex = -1;
    Settings settings;
    std::string dir;
    std::string savePath(const Profile& p) const;  // mid-level save file
};

extern Profiles g_profiles;
