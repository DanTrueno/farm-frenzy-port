// Modal windows shown during a level.
#pragma once
#include "game.h"

void showMarket(Game& g);
void showGameMenu(Game& g);
void showGameAim(Game& g);
void showLevelResults(Game& g);
void showPreGameAim(Game& g);
TexPtr goalIcon(int goalType, bool small);
std::string goalName(int goalType);
