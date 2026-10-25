// High level flow between screens (logo -> menu -> map -> level).
#pragma once
#include "screen.h"
#include "ui.h"
#include <functional>

void gameFlowInit();
void gameFlowShutdown();
void scriptedInput(int frame);

void goMainMenu();
void goMap();
void goShop();
void startLevel(int level);   // 1-based level / pack number
bool hasSavedGame();
void continueSavedGame();
void saveRunningGame();
void playMenuMusic();

// Simple modal dialogs
void showYesNo(const std::string& text, std::function<void(bool)> cb);
void showOptions();

// Text edit field used for profile names
struct EditField {
    Text text;
    std::u32string value;
    float blink = 0;
    size_t maxChars = 16;
    void load(XNode n);
    void update(float dt);
    void draw() const;
};
