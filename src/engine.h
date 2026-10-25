// Engine layer mirroring the original Melesta engine concepts:
// XML-described sprites, animated frames, buttons, progress bars, bitmap fonts.
#pragma once
#include "platform.h"
#include "pugixml.hpp"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// ---------- XML ----------
using XNode = pugi::xml_node;
struct XmlDoc {
    pugi::xml_document doc;
    XNode root() const { return doc.document_element(); }
    bool ok = false;
};
std::shared_ptr<XmlDoc> loadXml(const std::string& path);
float xFloat(XNode n, const char* a, float def = 0);
int xInt(XNode n, const char* a, int def = 0);
bool xBool(XNode n, const char* a, bool def = false);
std::string xStr(XNode n, const char* a, const char* def = "");
Vec2 xVec(XNode n, const char* a, Vec2 def = {});
Color xColor(XNode n, const char* a, Color def = {});
bool xHas(XNode n, const char* a);

// ---------- Strings / localisation ----------
std::string tr(const std::string& s);
void loadStrings();
std::u32string utf8to32(const std::string& s);
std::string utf32to8(const std::u32string& s);

// ---------- Font ----------
struct Glyph { float u0, v0, u1, v1, adv; };
class Font {
public:
    bool load(const std::string& fnt, const std::string& tex);
    const Glyph* glyph(char32_t c) const;
    float width(const std::u32string& s, float mul, float spaceWidth) const;
    float lineHeight(float mul) const;
    // align: 0 = centre, 1 = left, 2 = right. pos = anchor (vertical centre).
    void draw(const std::u32string& s, Vec2 pos, float mul, float spaceWidth, Color col, int deep, int align,
              float angle = 0) const;
    TexPtr tex;
private:
    struct Range { char32_t first, last; int start; };
    std::vector<Range> ranges_;
    std::vector<Glyph> glyphs_;
};
std::shared_ptr<Font> getFont(const std::string& fnt, const std::string& tex);

// ---------- Animated sprite ----------
struct Anim {
    TexPtr tex, mask;
    int frames = 1, cols = 1, rows = 1;
    float length = 1;
    bool loop = true;
    bool play = false;
    float time = 0;
    int frame = 0;
    void load(XNode n, bool defaultPlay = false);
    void update(float dt);
    void uv(int f, float& u0, float& v0, float& u1, float& v1) const;
    bool finished() const { return !loop && time >= length; }
    void reset() { time = 0; frame = 0; }
};

struct Sprite {
    Anim anim;
    Vec2 pos, size;
    int deep = 0;
    float angle = 0;
    Color color;
    bool visible = true;
    bool flipX = false;
    Blend blend = Blend::Alpha;
    void load(XNode n, Vec2 origin = {}, bool defaultPlay = false);
    void update(float dt) { anim.update(dt); }
    void draw() const;
    void drawAt(Vec2 p, int frameOverride = -1, float alpha = 1) const;
    bool hit(Vec2 p) const;
    bool valid() const { return anim.tex != nullptr; }
};

// ---------- Text ----------
struct Text {
    std::shared_ptr<Font> font;
    std::u32string str;
    Vec2 pos;
    float mul = 1, spaceWidth = 10;
    Color color;
    int deep = 0;
    int align = 0;
    bool visible = true;
    bool hasPos = false;
    void load(XNode n, Vec2 origin = {});
    void set(const std::string& s) { str = utf8to32(s); }
    void draw(float alpha = 1) const;
    void drawAt(Vec2 p, float alpha = 1) const;
    float width() const { return font ? font->width(str, mul, spaceWidth) : 0; }
};

// ---------- Progress bar ----------
struct Progress {
    TexPtr tex;
    Vec2 pos, size;
    int deep = 0;
    int side = 0; // 0 left,1 right,2 up,3 down
    float angle = 0;
    float maxValue = 100, value = 0;
    bool visible = true;
    Color color;
    void load(XNode n, Vec2 origin = {});
    void draw() const;
    void drawAt(Vec2 p) const;
};

// ---------- Button ----------
struct Button {
    Sprite spr;
    TexPtr texHover, texPressed;  // alternative textures
    int fNormal = 0, fHover = 0, fPressed = 0, fDisabled = 0;
    bool hasFrames = false;
    bool enabled = true;
    bool visible = true;
    Text text;
    bool hasText = false;
    std::string sndHover, sndClick;
    bool hovered = false, pressed = false;
    std::function<void()> onClick;
    // "States" button variant (cap textures by width)
    struct Part { TexPtr n, h, p; float len; };
    std::vector<Part> parts;
    void load(XNode n, Vec2 origin = {});
    // returns true if clicked this frame
    bool update(float dt, bool inputAllowed = true);
    void draw(float alpha = 1) const;
    bool contains(Vec2 p) const;
};

// Global input gate: modal windows can capture input
struct Input {
    static Vec2 mouse() { return g_plat.mouse; }
    static bool pressed() { return g_plat.mousePressed; }
    static bool released() { return g_plat.mouseReleased; }
    static bool down() { return g_plat.mouseDown; }
};

float frand(float a, float b);
int irand(int n);
std::string fmtInt(long long v);
