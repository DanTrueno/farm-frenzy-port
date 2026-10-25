// Screen stack: each screen updates/draws; top-most modal gets input.
#pragma once
#include "engine.h"
#include <memory>
#include <vector>

class Screen {
public:
    virtual ~Screen() = default;
    virtual void update(float dt, bool input) = 0;
    virtual void draw() = 0;
    virtual bool modal() const { return true; }  // blocks input to screens below
    virtual bool opaque() const { return true; } // hides screens below
    bool closed = false;
    float alpha = 1;
};
using ScreenPtr = std::shared_ptr<Screen>;

class ScreenManager {
public:
    void push(ScreenPtr s) { pending_.push_back(s); }
    void replaceAll(ScreenPtr s) { clearAll_ = true; pending_.push_back(s); }
    void update(float dt);
    void draw();
    bool empty() const { return stack_.empty() && pending_.empty(); }
    template <class T> std::shared_ptr<T> find() {
        for (auto& s : stack_) if (auto p = std::dynamic_pointer_cast<T>(s)) return p;
        return nullptr;
    }
private:
    std::vector<ScreenPtr> stack_, pending_;
    bool clearAll_ = false;
};

extern ScreenManager g_screens;

// Draw a full-screen dark overlay at given depth
void drawDim(int deep, float a);
