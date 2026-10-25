#include "engine.h"
#include "portable.h"
#include "vfs.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <unordered_map>

// ---------------- random ----------------
static std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}
float frand(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng()); }
int irand(int n) { return n <= 0 ? 0 : (int)(rng()() % (unsigned)n); }

std::string fmtInt(long long v) { return std::to_string(v); }

// ---------------- UTF ----------------
std::u32string utf8to32(const std::string& s) {
    std::u32string r;
    const unsigned char* p = (const unsigned char*)s.c_str();
    while (*p) {
        char32_t cp;
        if (*p < 0x80) cp = *p++;
        else if ((*p & 0xE0) == 0xC0) { cp = (*p++ & 0x1F) << 6; if (*p) cp |= (*p++ & 0x3F); }
        else if ((*p & 0xF0) == 0xE0) {
            cp = (*p++ & 0x0F) << 12;
            if (*p) cp |= (*p++ & 0x3F) << 6;
            if (*p) cp |= (*p++ & 0x3F);
        } else {
            cp = (*p++ & 0x07) << 18;
            for (int i = 2; i >= 0 && *p; --i) cp |= (*p++ & 0x3F) << (6 * i);
        }
        r += cp;
    }
    return r;
}

std::string utf32to8(const std::u32string& s) {
    std::string r;
    for (char32_t c : s) {
        if (c < 0x80) r += (char)c;
        else if (c < 0x800) { r += (char)(0xC0 | (c >> 6)); r += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) {
            r += (char)(0xE0 | (c >> 12)); r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F));
        } else {
            r += (char)(0xF0 | (c >> 18)); r += (char)(0x80 | ((c >> 12) & 0x3F));
            r += (char)(0x80 | ((c >> 6) & 0x3F)); r += (char)(0x80 | (c & 0x3F));
        }
    }
    return r;
}

// ---------------- XML ----------------
std::shared_ptr<XmlDoc> loadXml(const std::string& path) {
    auto d = std::make_shared<XmlDoc>();
    std::vector<uint8_t> data;
    if (!g_vfs.read(path, data)) {
        fprintf(stderr, "xml missing: %s\n", path.c_str());
        return d;
    }
    auto r = d->doc.load_buffer(data.data(), data.size(), pugi::parse_default, pugi::encoding_auto);
    d->ok = (bool)r;
    if (!d->ok) fprintf(stderr, "xml parse error %s: %s\n", path.c_str(), r.description());
    return d;
}

bool xHas(XNode n, const char* a) { return n && n.attribute(a); }
float xFloat(XNode n, const char* a, float def) {
    auto at = n.attribute(a);
    return at ? (float)atof(at.value()) : def;
}
int xInt(XNode n, const char* a, int def) {
    auto at = n.attribute(a);
    return at ? atoi(at.value()) : def;
}
bool xBool(XNode n, const char* a, bool def) {
    auto at = n.attribute(a);
    return at ? atoi(at.value()) != 0 : def;
}
std::string xStr(XNode n, const char* a, const char* def) {
    auto at = n.attribute(a);
    return at ? std::string(at.value()) : std::string(def);
}
Vec2 xVec(XNode n, const char* a, Vec2 def) {
    auto at = n.attribute(a);
    if (!at) return def;
    Vec2 v;
    if (sscanf(at.value(), "%f %f", &v.x, &v.y) < 2) v.y = v.x;
    return v;
}
Color xColor(XNode n, const char* a, Color def) {
    auto at = n.attribute(a);
    if (!at) return def;
    const char* s = at.value();
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint32_t v = (uint32_t)strtoul(s, nullptr, 16);
    if (strlen(s) <= 6) v |= 0xFF000000u;
    return Color::fromArgb(v);
}

// ---------------- localisation ----------------
static std::unordered_map<std::string, std::string> g_strings;

static std::string trimNl(std::string s) {
    // normalise CRLF -> LF
    std::string r;
    for (char c : s) if (c != '\r') r += c;
    return r;
}

void loadStrings() {
    auto d = loadXml("sys/strings.xml");
    if (!d->ok) return;
    for (XNode s : d->root().children("s")) {
        std::string t = trimNl(s.child("text").text().get());
        std::string v = trimNl(s.child("translation").text().get());
        g_strings[t] = v;
    }
}

std::string tr(const std::string& s) {
    auto it = g_strings.find(trimNl(s));
    return it != g_strings.end() ? it->second : s;
}

// ---------------- Font ----------------
bool Font::load(const std::string& fnt, const std::string& texPath) {
    std::vector<uint8_t> d;
    if (!g_vfs.read(fnt, d) || d.size() < 12 || memcmp(d.data(), "tnof", 4) != 0) {
        fprintf(stderr, "font missing: %s\n", fnt.c_str());
        return false;
    }
    auto u32 = [&](size_t o) { uint32_t v; memcpy(&v, &d[o], 4); return v; };
    auto u16 = [&](size_t o) { uint16_t v; memcpy(&v, &d[o], 2); return v; };
    uint32_t nr = u32(4);
    size_t p = 8;
    for (uint32_t i = 0; i < nr; ++i, p += 6) ranges_.push_back({u16(p), u16(p + 2), u16(p + 4)});
    uint32_t ng = u32(p);
    p += 4;
    for (uint32_t i = 0; i < ng && p + 20 <= d.size(); ++i, p += 20) {
        Glyph g;
        memcpy(&g, &d[p], 20);
        glyphs_.push_back(g);
    }
    tex = g_plat.texture(texPath);
    return true;
}

const Glyph* Font::glyph(char32_t c) const {
    for (auto& r : ranges_)
        if (c >= r.first && c <= r.last) {
            size_t i = r.start + (c - r.first);
            return i < glyphs_.size() ? &glyphs_[i] : nullptr;
        }
    return nullptr;
}

float Font::lineHeight(float mul) const {
    if (!tex || glyphs_.empty()) return 20 * mul;
    const Glyph& g = glyphs_[0];
    return std::fabs(g.v1 - g.v0) * tex->h * mul;
}

float Font::width(const std::u32string& s, float mul, float spaceWidth) const {
    if (!tex) return 0;
    float w = 0;
    for (char32_t c : s) {
        if (c == U' ') { w += spaceWidth * mul; continue; }
        const Glyph* g = glyph(c);
        if (!g) { w += spaceWidth * mul; continue; }
        w += g->adv * tex->w * mul;
    }
    return w;
}

void Font::draw(const std::u32string& s, Vec2 pos, float mul, float spaceWidth, Color col, int deep, int align,
                float angle) const {
    if (!tex) return;
    float w = width(s, mul, spaceWidth);
    float x = pos.x;
    if (align == 0) x -= w * 0.5f;
    else if (align == 2) x -= w;
    for (char32_t c : s) {
        const Glyph* g = c == U' ' ? nullptr : glyph(c);
        if (!g) { x += spaceWidth * mul; continue; }
        float gw = (g->u1 - g->u0) * tex->w * mul;
        float gh = (g->v1 - g->v0) * tex->h * mul;
        float adv = g->adv * tex->w * mul;
        DrawCmd dc;
        dc.tex = tex;
        dc.u0 = g->u0; dc.u1 = g->u1;
        dc.v0 = 1 - g->v1; dc.v1 = 1 - g->v0;
        dc.w = gw; dc.h = gh;
        dc.x = x + adv * 0.5f;
        dc.y = pos.y;
        dc.color = col;
        dc.deep = deep;
        dc.angle = angle;
        g_plat.draw(dc);
        x += adv;
    }
}

std::shared_ptr<Font> getFont(const std::string& fnt, const std::string& tex) {
    static std::map<std::string, std::shared_ptr<Font>> cache;
    std::string key = Vfs::normalize(fnt) + "|" + Vfs::normalize(tex);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    auto f = std::make_shared<Font>();
    std::string t = tex;
    if (!g_vfs.exists(t)) {
        // some XMLs reference non-existing textures (honey1_font.dds); fall back to the font's own
        t = fnt.substr(0, fnt.rfind('.')) + ".dds";
    }
    if (!f->load(fnt, t)) f = nullptr;
    cache[key] = f;
    return f;
}

// ---------------- Anim ----------------
void Anim::load(XNode n, bool defaultPlay) {
    std::string pic = xStr(n, "Picture");
    if (!pic.empty()) tex = g_plat.texture(pic);
    std::string m = xStr(n, "texture_mask");
    if (!m.empty()) mask = g_plat.texture(m);
    frames = std::max(1, xInt(n, "frames_count", 1));
    cols = std::max(1, xInt(n, "anim_columns", 1));
    rows = std::max(1, xInt(n, "anim_rows", 1));
    // some XMLs claim more frames than the sheet holds (grass: 32 vs 4x4)
    if (cols * rows > 1) frames = std::min(frames, cols * rows);
    else if (frames > 1 && tex) {
        // frame count without a grid: pick the grid whose cells are closest to square
        int bc = 0, br = 0;
        float best = 1e9f;
        for (int c = 1; c <= frames; ++c) {
            int r = (frames + c - 1) / c;
            float cw = (float)tex->w / c, ch = (float)tex->h / r;
            float err = std::fabs(cw - ch) / std::max(cw, ch);
            if (err < best) { best = err; bc = c; br = r; }
        }
        if (best < 0.15f) { cols = bc; rows = br; }
        else frames = 1;
    }
    length = xFloat(n, "anim_length", 1);
    loop = xBool(n, "anim_loop", true);
    // only PlayAnimation=1 runs a sheet by itself; otherwise frames are picked by the code
    // (coin icons: frame 0 gold = affordable, frame 1 grey)
    play = xBool(n, "PlayAnimation", defaultPlay);
    time = 0;
    frame = 0;
}

void Anim::update(float dt) {
    if (!play || frames <= 1) return;
    time += dt;
    if (loop) {
        if (length > 0) time = std::fmod(time, length);
    } else if (time > length) time = length;
    frame = length > 0 ? (int)(time / length * frames) : 0;
    if (frame >= frames) frame = loop ? 0 : frames - 1;
}

void Anim::uv(int f, float& u0, float& v0, float& u1, float& v1) const {
    if (f < 0) f = frame;
    f = std::max(0, std::min(f, frames - 1));
    int cx = f % cols, cy = f / cols;
    u0 = (float)cx / cols; u1 = (float)(cx + 1) / cols;
    v0 = (float)cy / rows; v1 = (float)(cy + 1) / rows;
}

// ---------------- Sprite ----------------
void Sprite::load(XNode n, Vec2 origin, bool defaultPlay) {
    anim.load(n, defaultPlay);
    pos = xVec(n, "position") + origin;
    // some XMLs declare a frame grid for a texture that is a single image of exactly the
    // sprite's size (the bank icon: 4x4 frames on a 36x32 picture) - treat it as one frame
    if (anim.tex && anim.cols * anim.rows > 1 && xHas(n, "size")) {
        Vec2 sz = xVec(n, "size");
        if (std::fabs(anim.tex->w - sz.x) <= 2 && std::fabs(anim.tex->h - sz.y) <= 2) {
            anim.cols = anim.rows = anim.frames = 1;
            anim.play = false;
        }
    }
    size = xVec(n, "size", anim.tex ? Vec2((float)anim.tex->w / anim.cols, (float)anim.tex->h / anim.rows) : Vec2());
    deep = xInt(n, "deep", 0);
    angle = xFloat(n, "angle", 0);
    visible = xBool(n, "Visible", true);
    flipX = xBool(n, "InverceX", false);
    if (xHas(n, "alpha")) color.a = (uint8_t)(xFloat(n, "alpha") * 255);
    if (xHas(n, "Color")) color = xColor(n, "Color");
}

void Sprite::draw() const {
    if (visible) drawAt(pos);
}

void Sprite::drawAt(Vec2 p, int frameOverride, float alpha) const {
    if (!anim.tex) return;
    DrawCmd c;
    c.tex = anim.tex;
    anim.uv(frameOverride, c.u0, c.v0, c.u1, c.v1);
    c.x = p.x; c.y = p.y;
    c.w = size.x; c.h = size.y;
    c.angle = angle;
    c.color = color;
    c.color.a = (uint8_t)(color.a * std::max(0.f, std::min(1.f, alpha)));
    c.deep = deep;
    c.flipX = flipX;
    c.blend = blend;
    g_plat.draw(c);
}

bool Sprite::hit(Vec2 p) const {
    float lx = p.x - pos.x, ly = p.y - pos.y;
    if (std::fabs(lx) > size.x * 0.5f || std::fabs(ly) > size.y * 0.5f) return false;
    float u = lx / size.x + 0.5f, v = ly / size.y + 0.5f;
    if (flipX) u = 1 - u;
    if (anim.mask) {
        // masks cover a single frame
        return anim.mask->opaqueAt(u, v);
    }
    if (!anim.tex) return true;
    float u0, v0, u1, v1;
    anim.uv(-1, u0, v0, u1, v1);
    return anim.tex->opaqueAt(u0 + (u1 - u0) * u, v0 + (v1 - v0) * v);
}

// ---------------- Text ----------------
void Text::load(XNode n, Vec2 origin) {
    font = getFont(xStr(n, "Font"), xStr(n, "Texture"));
    std::string s = xStr(n, "String");
    if (xBool(n, "Localized")) s = tr(s);
    str = utf8to32(s);
    hasPos = xHas(n, "position");
    pos = xVec(n, "position") + origin;
    mul = xFloat(n, "Multiplier", 1);
    spaceWidth = xFloat(n, "SpaceWidth", 10);
    color = xColor(n, "Color");
    deep = xInt(n, "deep", 0);
    align = xInt(n, "Align", 0);
    visible = xBool(n, "Visible", true);
}

void Text::draw(float alpha) const {
    if (visible) drawAt(pos, alpha);
}

void Text::drawAt(Vec2 p, float alpha) const {
    if (!font || str.empty()) return;
    Color c = color;
    c.a = (uint8_t)(c.a * std::max(0.f, std::min(1.f, alpha)));
    // multi-line support
    size_t start = 0;
    float lh = font->lineHeight(mul) * 0.85f;
    std::vector<std::u32string> lines;
    for (size_t i = 0; i <= str.size(); ++i)
        if (i == str.size() || str[i] == U'\n') { lines.push_back(str.substr(start, i - start)); start = i + 1; }
    float y = p.y - lh * (lines.size() - 1) * 0.5f;
    for (auto& l : lines) {
        font->draw(l, {p.x, y}, mul, spaceWidth, c, deep, align);
        y += lh;
    }
}

// ---------------- Progress ----------------
void Progress::load(XNode n, Vec2 origin) {
    std::string pic = xStr(n, "Picture");
    if (!pic.empty()) tex = g_plat.texture(pic);
    pos = xVec(n, "position") + origin;
    size = xVec(n, "size");
    deep = xInt(n, "deep");
    side = xInt(n, "Side");
    angle = xFloat(n, "angle");
    maxValue = xFloat(n, "ProgressMaxValue", 100);
    value = xFloat(n, "ProgressCurrentValue", 0);
    visible = xBool(n, "Visible", true);
}

void Progress::draw() const {
    if (visible) drawAt(pos);
}

void Progress::drawAt(Vec2 p) const {
    if (!tex || maxValue <= 0) return;
    float f = std::max(0.f, std::min(1.f, value / maxValue));
    if (f <= 0) return;
    DrawCmd c;
    c.tex = tex;
    c.deep = deep;
    c.color = color;
    c.angle = angle;
    float w = size.x, h = size.y;
    // local offset of the filled part's centre, before rotation
    float ox = 0, oy = 0;
    switch (side) {
        case 0: // fills from right to left (anchored right)
            c.u0 = 1 - f; c.w = w * f; c.h = h; ox = w * (1 - f) * 0.5f; break;
        case 1: // anchored left
            c.u1 = f; c.w = w * f; c.h = h; ox = -w * (1 - f) * 0.5f; break;
        case 2: // grows upwards (anchored bottom)
            c.v0 = 1 - f; c.w = w; c.h = h * f; oy = h * (1 - f) * 0.5f; break;
        default: // anchored top
            c.v1 = f; c.w = w; c.h = h * f; oy = -h * (1 - f) * 0.5f; break;
    }
    float r = angle * kPi / 180.f;
    c.x = p.x + ox * std::cos(r) - oy * std::sin(r);
    c.y = p.y + ox * std::sin(r) + oy * std::cos(r);
    g_plat.draw(c);
}

// ---------------- Button ----------------
void Button::load(XNode n, Vec2 origin) {
    spr.load(n, origin);
    spr.anim.play = false;
    hasFrames = xHas(n, "frame_normal");
    fNormal = xInt(n, "frame_normal");
    fHover = xInt(n, "frame_hover", fNormal);
    fPressed = xInt(n, "frame_pressed", fNormal);
    fDisabled = xInt(n, "frame_disabled", fNormal);
    std::string th = xStr(n, "texture_hover"), tp = xStr(n, "texture_pressed");
    if (!th.empty()) texHover = g_plat.texture(th);
    if (!tp.empty()) texPressed = g_plat.texture(tp);
    visible = xBool(n, "Visible", true);
    if (XNode t = n.child("Text")) {
        hasText = true;
        bool useTextPos = xBool(n, "UseTextPosition", false);
        text.load(t, {});
        // UseTextPosition=1: the text keeps its own absolute position; otherwise it is centred on the button
        if (!text.hasPos || !useTextPos) text.pos = spr.pos;
        if (!xHas(t, "deep")) text.deep = spr.deep + 1;
    }
    if (XNode s = n.child("Sounds")) {
        sndHover = xStr(s, "Hover");
        sndClick = xStr(s, "Click");
    }
    if (XNode st = n.child("States")) {
        for (XNode s : st.children("State")) {
            XNode in = s.child("State");
            Part p;
            p.len = xFloat(s, "Length");
            p.n = g_vfs.exists(xStr(in, "Picture")) ? g_plat.texture(xStr(in, "Picture")) : nullptr;
            p.h = g_vfs.exists(xStr(in, "texture_hover")) ? g_plat.texture(xStr(in, "texture_hover")) : nullptr;
            p.p = g_vfs.exists(xStr(in, "texture_pressed")) ? g_plat.texture(xStr(in, "texture_pressed")) : nullptr;
            parts.push_back(p);
        }
    }
}

bool Button::contains(Vec2 p) const {
    // a mask defines the clickable shape; otherwise the whole rectangle is clickable
    // (some buttons, like the fullscreen checkbox, are frames with a transparent middle)
    if (spr.anim.mask) return spr.hit(p);
    return std::fabs(p.x - spr.pos.x) <= spr.size.x * 0.5f && std::fabs(p.y - spr.pos.y) <= spr.size.y * 0.5f;
}

bool Button::update(float dt, bool inputAllowed) {
    spr.update(dt);
    if (!visible || !enabled || !inputAllowed) {
        hovered = pressed = false;
        return false;
    }
    bool in = contains(Input::mouse());
    if (in && !hovered && !sndHover.empty()) g_audio.playSound(sndHover);
    hovered = in;
    if (in && Input::pressed()) pressed = true;
    bool clicked = false;
    if (Input::released()) {
        if (pressed && in) clicked = true;
        pressed = false;
    }
    if (!Input::down()) pressed = false;
    if (clicked) {
        if (!sndClick.empty()) g_audio.playSound(sndClick);
        if (onClick) onClick();
    }
    return clicked;
}

void Button::draw(float alpha) const {
    if (!visible) return;
    int f = !enabled ? fDisabled : (pressed && hovered) ? fPressed : hovered ? fHover : fNormal;
    if (spr.anim.tex) {
        if (hasFrames) spr.drawAt(spr.pos, f, alpha);
        else {
            Sprite s = spr;
            if (enabled && pressed && hovered && texPressed) s.anim.tex = texPressed;
            else if (enabled && hovered && texHover) s.anim.tex = texHover;
            s.drawAt(s.pos, -1, alpha);
        }
    }
    if (hasText) {
        Text t = text;
        if (pressed && hovered) t.pos = t.pos + Vec2(1, 1);
        t.drawAt(t.pos, alpha * (enabled ? 1.f : 0.6f));
    }
}
