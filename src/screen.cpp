#include "screen.h"
#include <algorithm>

ScreenManager g_screens;

void ScreenManager::update(float dt) {
    if (clearAll_) { stack_.clear(); clearAll_ = false; }
    for (auto& p : pending_) stack_.push_back(p);
    pending_.clear();
    bool input = true;
    // top-down so the top modal screen consumes input
    std::vector<ScreenPtr> snapshot = stack_;
    for (int i = (int)snapshot.size() - 1; i >= 0; --i) {
        auto& s = snapshot[i];
        if (s->closed) continue;
        s->update(dt, input);
        if (s->modal()) input = false;
    }
    stack_.erase(std::remove_if(stack_.begin(), stack_.end(), [](const ScreenPtr& s) { return s->closed; }),
                 stack_.end());
}

void ScreenManager::draw() {
    int first = 0;
    for (int i = (int)stack_.size() - 1; i >= 0; --i)
        if (stack_[i]->opaque()) { first = i; break; }
    for (int i = first; i < (int)stack_.size(); ++i) {
        g_plat.deepBias = (i - first) * 100000;
        stack_[i]->draw();
    }
    g_plat.deepBias = 0;
}

void drawDim(int deep, float a) {
    static TexPtr white;
    if (!white) {
        white = std::make_shared<Texture>();
        uint32_t px = 0xFFFFFFFF;
        white->tex = SDL_CreateTexture(g_plat.renderer(), SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, 1, 1);
        SDL_UpdateTexture(white->tex, nullptr, &px, 4);
        white->w = white->h = 1;
    }
    DrawCmd c;
    c.tex = white;
    c.x = 400; c.y = 300; c.w = 800; c.h = 600;
    c.color = {0, 0, 0, (uint8_t)(a * 255)};
    c.deep = deep;
    g_plat.draw(c);
}
