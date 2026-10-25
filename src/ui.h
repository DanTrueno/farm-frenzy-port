// Generic loader for the game's UI description files (UI/*.xml).
// Top-level children are sorted into buttons, texts, sprites and progress bars by their attributes.
#pragma once
#include "engine.h"
#include <map>

struct UiDoc {
    std::shared_ptr<XmlDoc> xml;
    std::vector<Sprite> background;
    std::map<std::string, Button> buttons;
    std::map<std::string, Text> texts;
    std::map<std::string, Sprite> sprites;
    std::map<std::string, Progress> progress;
    std::vector<Text> extraTexts;  // children of <Texts>/<Title>
    bool load(const std::string& path);
    XNode root() const { return xml ? xml->root() : XNode(); }
    XNode node(const char* name) const { return root().child(name); }
    // Update all buttons; returns name of clicked button or "".
    std::string update(float dt, bool input);
    void draw(float alpha = 1);
    Button& btn(const std::string& n) { return buttons[n]; }
    Text& txt(const std::string& n) { return texts[n]; }
};

bool isButtonNode(XNode n);
