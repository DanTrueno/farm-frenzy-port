#include "profile.h"
#include "portable.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

Profiles g_profiles;

Profile* Profiles::current() {
    if (currentIndex < 0 || currentIndex >= (int)list.size()) return nullptr;
    return &list[currentIndex];
}

void Profiles::load() {
    if (const char* p = SDL_GetPrefPath("Melesta", "FarmFrenzy")) dir = p;
    std::ifstream f(u8path(dir + "profiles.txt"));
    if (!f) return;
    std::string line;
    Profile* cur = nullptr;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string key;
        ss >> key;
        std::string rest;
        std::getline(ss, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
        std::istringstream rs(rest);
        if (key == "music") rs >> settings.music;
        else if (key == "sound") rs >> settings.sound;
        else if (key == "fullscreen") rs >> settings.fullscreen;
        else if (key == "current") rs >> currentIndex;
        else if (key == "profile") { list.emplace_back(); cur = &list.back(); cur->name = rest; }
        else if (!cur) continue;
        else if (key == "stars") rs >> cur->stars;
        else if (key == "last") rs >> cur->lastLevel;
        else if (key == "tutorial") rs >> cur->tutorialSeen;
        else if (key == "medal") for (auto& v : cur->medal) rs >> v;
        else if (key == "time") for (auto& v : cur->bestTime) rs >> v;
        else if (key == "shop") for (auto& v : cur->shop) rs >> v;
        else if (key == "stat") for (auto& v : cur->stat) rs >> v;
        else if (key == "awards") for (auto& v : cur->awards) rs >> v;
        else if (key == "tips") for (auto& v : cur->tipSeen) rs >> v;
    }
    if (currentIndex >= (int)list.size()) currentIndex = list.empty() ? -1 : 0;
}

void Profiles::save() {
    std::ofstream f(u8path(dir + "profiles.txt"));
    f << "music " << settings.music << "\n";
    f << "sound " << settings.sound << "\n";
    f << "fullscreen " << settings.fullscreen << "\n";
    f << "current " << currentIndex << "\n";
    for (auto& p : list) {
        f << "profile " << p.name << "\n";
        f << "stars " << p.stars << "\n";
        f << "last " << p.lastLevel << "\n";
        f << "tutorial " << p.tutorialSeen << "\n";
        f << "medal"; for (auto v : p.medal) f << " " << v; f << "\n";
        f << "time"; for (auto v : p.bestTime) f << " " << v; f << "\n";
        f << "shop"; for (auto v : p.shop) f << " " << v; f << "\n";
        f << "stat"; for (auto v : p.stat) f << " " << v; f << "\n";
        f << "awards"; for (auto v : p.awards) f << " " << v; f << "\n";
        f << "tips"; for (auto v : p.tipSeen) f << " " << v; f << "\n";
    }
}

std::string Profiles::savePath(const Profile& p) const {
    // file name from the profile name bytes (hex) so any characters are safe
    static const char* hex = "0123456789abcdef";
    std::string n;
    for (unsigned char c : p.name) { n += hex[c >> 4]; n += hex[c & 15]; }
    return dir + "save_" + n + ".txt";
}
