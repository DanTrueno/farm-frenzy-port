#include "ui.h"
#include <cstring>

bool isButtonNode(XNode n) {
    std::string name = n.name();
    return xHas(n, "frame_normal") || n.child("Sounds") || n.child("States") ||
           name.find("Button") != std::string::npos;
}

bool UiDoc::load(const std::string& path) {
    xml = loadXml(path);
    if (!xml->ok) return false;
    for (XNode n : root().children()) {
        if (n.type() != pugi::node_element) continue;
        std::string name = n.name();
        if (name == "Background") {
            for (XNode p : n.children()) {
                if (p.type() != pugi::node_element) continue;
                Sprite s;
                s.load(p);
                background.push_back(s);
            }
        } else if (name == "Texts" || name == "Title") {
            for (XNode t : n.children()) {
                if (t.type() != pugi::node_element) continue;
                Text tx;
                tx.load(t);
                extraTexts.push_back(tx);
            }
        } else if (isButtonNode(n) && (xHas(n, "position") || xHas(n, "size"))) {
            buttons[name].load(n);
        } else if (xHas(n, "Font")) {
            texts[name].load(n);
        } else if (xHas(n, "Side")) {
            progress[name].load(n);
        } else if (xHas(n, "Picture") || xHas(n, "position")) {
            sprites[name].load(n);
        }
    }
    return true;
}

std::string UiDoc::update(float dt, bool input) {
    std::string clicked;
    for (auto& s : background) s.update(dt);
    for (auto& kv : sprites) kv.second.update(dt);
    for (auto& kv : buttons)
        if (kv.second.update(dt, input && clicked.empty())) clicked = kv.first;
    return clicked;
}

void UiDoc::draw(float alpha) {
    for (auto& s : background) if (s.visible) s.drawAt(s.pos, -1, alpha);
    for (auto& kv : sprites) if (kv.second.visible) kv.second.drawAt(kv.second.pos, -1, alpha);
    for (auto& t : extraTexts) t.draw(alpha);
    for (auto& kv : texts) kv.second.draw(alpha);
    for (auto& kv : progress) kv.second.draw();
    for (auto& kv : buttons) kv.second.draw(alpha);
}
