// Logo, main menu, profile selection, options, exit dialog.
#include "game_flow.h"
#include "profile.h"
#include "vfs.h"
#include "game.h"
#include "awards.h"
#include <cmath>
#include <cstdlib>
#include <sstream>

// ---------------- helpers ----------------
void playMenuMusic() { g_audio.playMusic("Sounds/Menu_music.ogg"); }

void EditField::load(XNode n) {
    text.load(n);
    value.clear();
}

void EditField::update(float dt) {
    blink += dt;
    for (uint32_t c : g_plat.textInput)
        if (value.size() < maxChars && c >= 32 && text.font && (c == ' ' || text.font->glyph(c))) value += (char32_t)c;
    for (auto k : g_plat.keysPressed)
        if (k == SDLK_BACKSPACE && !value.empty()) value.pop_back();
}

void EditField::draw() const {
    Text t = text;
    t.str = value;
    if (std::fmod(blink, 0.6f) < 0.3f) t.str += U'_';
    else t.str += U' ';
    t.draw();
}

// Fixed background used behind menus
struct FixedBack {
    std::vector<Sprite> pics;
    void load() {
        UiDoc d;
        d.load("Ui/FixedBack.xml");
        pics = d.background;
    }
    void draw() { for (auto& p : pics) p.draw(); }
};

// ---------------- Yes/No dialog ----------------
class YesNoScreen : public Screen {
public:
    UiDoc ui;
    Text msg;
    std::function<void(bool)> cb;
    YesNoScreen(const std::string& file, const std::string& text, std::function<void(bool)> c) : cb(std::move(c)) {
        ui.load(file);
        if (XNode t = ui.node("Text")) {
            msg.load(t);
            std::string s = text;
            if (s.empty()) {
                // inner <Text> element holds the default message
                s = t.child("Text").text().get();
                if (xBool(t, "Localized")) s = tr(s);
            }
            msg.set(s);
        }
        ui.texts.erase("Text");
    }
    bool opaque() const override { return false; }
    void update(float dt, bool input) override {
        std::string c = ui.update(dt, input);
        if (c == "YesButton" || c == "NoButton") {
            closed = true;
            if (cb) cb(c == "YesButton");
        }
        for (auto k : g_plat.keysPressed)
            if (input && k == SDLK_ESCAPE) { closed = true; if (cb) cb(false); }
    }
    void draw() override {
        drawDim(ui.btn("YesButton").spr.deep - 600, 0.35f);
        ui.draw();
        msg.draw();
    }
};

void showYesNo(const std::string& text, std::function<void(bool)> cb) {
    g_screens.push(std::make_shared<YesNoScreen>("UI/YesNo.xml", text, cb));
}

// ---------------- Options ----------------
class OptionsScreen : public Screen {
public:
    UiDoc ui;
    struct Slider {
        Sprite track, knob, knobHover, knobPressed;
        float value = 1;
        bool dragging = false;
        void load(XNode n) {
            track.load(n);
            XNode s = n.child("Slider");
            knob.load(s);
            knobHover = knob; knobPressed = knob;
            if (xHas(s, "texture_hover")) knobHover.anim.tex = g_plat.texture(xStr(s, "texture_hover"));
            if (xHas(s, "texture_pressed")) knobPressed.anim.tex = g_plat.texture(xStr(s, "texture_pressed"));
        }
        float x0() const { return track.pos.x - track.size.x / 2; }
        float x1() const { return track.pos.x + track.size.x / 2; }
        Vec2 knobPos() const { return {x0() + (x1() - x0()) * value, track.pos.y}; }
        bool update(bool input) {
            Vec2 m = Input::mouse();
            Vec2 kp = knobPos();
            bool over = std::fabs(m.x - kp.x) < 18 && std::fabs(m.y - kp.y) < 18;
            bool onTrack = m.x >= x0() - 10 && m.x <= x1() + 10 && std::fabs(m.y - track.pos.y) < 16;
            if (input && Input::pressed() && (over || onTrack)) dragging = true;
            if (!Input::down()) dragging = false;
            if (dragging) {
                value = std::max(0.f, std::min(1.f, (m.x - x0()) / (x1() - x0())));
                return true;
            }
            hover = over && input;
            return false;
        }
        bool hover = false;
        void draw() const {
            const Sprite& k = dragging ? knobPressed : hover ? knobHover : knob;
            k.drawAt(knobPos());
        }
    } sound, music;
    Button fullscreen;
    Sprite tick;
    std::vector<Sprite> flowers;
    FixedBack back;
    bool inGame;

    explicit OptionsScreen(bool overGame) : inGame(overGame) {
        ui.load("UI/Options.xml");
        sound.load(ui.node("SoundVolume"));
        music.load(ui.node("MusicVolume"));
        ui.buttons.erase("SoundVolume");
        ui.buttons.erase("MusicVolume");
        ui.sprites.erase("SoundVolume");
        ui.sprites.erase("MusicVolume");
        XNode fs = ui.node("FullScreen");
        fullscreen.load(fs);
        tick.load(fs.child("Tick"));
        ui.buttons.erase("FullScreen");
        ui.sprites.erase("FullScreen");
        sound.value = g_profiles.settings.sound;
        music.value = g_profiles.settings.music;
        if (!inGame) {
            back.load();
            for (XNode f : ui.node("Flowers").children("Flower")) {
                Sprite s;
                s.load(f);
                s.anim.play = true;
                s.anim.loop = false;
                int last = xInt(f, "LastFrame", s.anim.frames);
                s.anim.length = 0.08f * last;
                s.anim.frames = last;
                flowers.push_back(s);
            }
        }
    }
    bool opaque() const override { return !inGame; }
    void update(float dt, bool input) override {
        for (auto& f : flowers) f.update(dt);
        if (sound.update(input)) {
            g_profiles.settings.sound = sound.value;
            g_audio.setSoundVolume(sound.value);
        }
        if (music.update(input)) {
            g_profiles.settings.music = music.value;
            g_audio.setMusicVolume(music.value);
        }
        if (fullscreen.update(dt, input)) {
            g_plat.toggleFullscreen();
            g_profiles.settings.fullscreen = g_plat.fullscreen();
        }
        std::string c = ui.update(dt, input && !sound.dragging && !music.dragging);
        bool esc = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_ESCAPE) esc = true;
        if (c == "OkButton" || (input && esc)) {
            g_profiles.save();
            closed = true;
        }
    }
    void draw() override {
        if (inGame) drawDim(ui.background.empty() ? 500 : ui.background[0].deep - 1, 0.35f);
        back.draw();
        for (auto& f : flowers) f.draw();
        ui.draw();
        sound.draw();
        music.draw();
        fullscreen.draw();
        if (g_plat.fullscreen()) tick.draw();
    }
};

void showOptions() { g_screens.push(std::make_shared<OptionsScreen>(true)); }

// ---------------- Profile selection ----------------
class SelectUserScreen : public Screen {
public:
    UiDoc ui;
    EditField edit;
    Button up, down, rowProto;
    std::vector<Button> rows;
    int scroll = 0, selected = -1;
    int maxRows = 9;
    float interlinear = 25;
    Vec2 topLeft;
    std::function<void()> onDone;
    bool firstUser;

    SelectUserScreen(bool first, std::function<void()> done) : onDone(std::move(done)), firstUser(first) {
        ui.load(first ? "UI/NewUser.xml" : "UI/SelectUser.xml");
        edit.load(ui.node("NameEdit"));
        ui.texts.erase("NameEdit");
        if (!first) {
            XNode ul = ui.node("UserList");
            up.load(ul.child("UpButton"));
            down.load(ul.child("DownButton"));
            rowProto.load(ul.child("Button"));
            maxRows = xInt(ul, "MaxRows", 9);
            interlinear = xFloat(ul, "Interlinear", 25);
            topLeft = xVec(ul, "TopLeft");
            ui.sprites.erase("UserList");
            ui.buttons.erase("UserList");
            selected = g_profiles.currentIndex;
            if (auto* p = g_profiles.current()) edit.value = utf8to32(p->name);
        }
    }
    bool opaque() const override { return false; }

    void choose(int i) {
        selected = i;
        edit.value = utf8to32(g_profiles.list[i].name);
    }

    void accept() {
        std::string name = utf32to8(edit.value);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (name.empty()) return;
        int found = -1;
        for (size_t i = 0; i < g_profiles.list.size(); ++i)
            if (g_profiles.list[i].name == name) found = (int)i;
        if (found < 0) {
            Profile p;
            p.name = name;
            g_profiles.list.push_back(p);
            found = (int)g_profiles.list.size() - 1;
        }
        g_profiles.currentIndex = found;
        g_profiles.save();
        closed = true;
        if (onDone) onDone();
    }

    void update(float dt, bool input) override {
        if (input) edit.update(dt);
        std::string c = ui.update(dt, input);
        for (auto k : g_plat.keysPressed)
            if (input && (k == SDLK_RETURN || k == SDLK_KP_ENTER)) c = "OkButton";
        if (c == "OkButton") accept();
        else if (c == "ExitButton" && g_profiles.current()) closed = true;
        else if (c == "DeleteButton" && selected >= 0 && selected < (int)g_profiles.list.size()) {
            int idx = selected;
            std::string q = xStr(ui.root(), "StringConfirm");
            showYesNo(tr(q), [this, idx](bool yes) {
                if (!yes) return;
                g_profiles.list.erase(g_profiles.list.begin() + idx);
                if (g_profiles.currentIndex == idx) g_profiles.currentIndex = g_profiles.list.empty() ? -1 : 0;
                else if (g_profiles.currentIndex > idx) g_profiles.currentIndex--;
                selected = -1;
                edit.value.clear();
                g_profiles.save();
            });
        }
        if (!firstUser) {
            if (up.update(dt, input) && scroll > 0) scroll--;
            if (down.update(dt, input) && scroll + maxRows < (int)g_profiles.list.size()) scroll++;
            for (int r = 0; r < maxRows && scroll + r < (int)g_profiles.list.size(); ++r) {
                Button b = rowProto;
                b.spr.pos = {rowProto.spr.size.x * 0.5f + topLeft.x, topLeft.y + interlinear * (r + 0.5f)};
                b.text.pos = b.spr.pos;
                if (b.update(dt, input)) choose(scroll + r);
            }
        }
    }

    void draw() override {
        drawDim(ui.background.empty() ? 900 : ui.background[0].deep - 1, 0.3f);
        ui.draw();
        edit.draw();
        if (!firstUser) {
            up.draw();
            down.draw();
            for (int r = 0; r < maxRows && scroll + r < (int)g_profiles.list.size(); ++r) {
                Text t = rowProto.text;
                t.pos = {topLeft.x + rowProto.spr.size.x * 0.5f, topLeft.y + interlinear * (r + 0.5f)};
                t.set(g_profiles.list[scroll + r].name);
                if (scroll + r == selected) t.color = {255, 230, 0, 255};
                t.deep = 1001;
                t.draw();
            }
        }
    }
};

// ---------------- Main menu ----------------
class MainMenuScreen : public Screen {
public:
    UiDoc ui;
    FixedBack back;
    bool submenu = false;
    Sprite substrate;
    Text userName, userText;

    MainMenuScreen() {
        back.load();
        ui.load("UI/MainMenu.xml");
        substrate.load(ui.node("SubmenuSubstrate"));
        substrate.visible = true;
        ui.sprites.erase("SubmenuSubstrate");
        userName = ui.txt("UserName");
        userText = ui.txt("UserText");
        ui.texts.erase("UserName");
        ui.texts.erase("UserText");
        ui.texts.erase("SubmenuUserName");
        playMenuMusic();
        refresh();
    }

    void refresh() {
        if (auto* p = g_profiles.current()) userName.set(p->name);
        else userName.set("");
        ui.btn("ContinueButton").enabled = hasSavedGame();
    }

    static const char* submenuButtons[];

    void update(float dt, bool input) override {
        if (input && !g_profiles.current()) {
            g_screens.push(std::make_shared<SelectUserScreen>(true, [this] { refresh(); }));
            return;
        }
        // split buttons into main and sub-menu sets
        std::string clicked;
        for (auto& kv : ui.buttons) {
            bool isSub = kv.first.rfind("Submenu", 0) == 0 || kv.first == "ContinueButton";
            bool active = isSub == submenu;
            if (kv.second.update(dt, input && active && clicked.empty())) clicked = kv.first;
        }
        for (auto& s : ui.background) s.update(dt);
        bool esc = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_ESCAPE) esc = true;
        if (input && esc) clicked = submenu ? "SubmenuCancel" : "ExitButton";

        if (clicked == "StartButton") {
            // an unfinished level offers "continue / new game", otherwise straight to the map
            if (hasSavedGame()) submenu = true;
            else goMap();
            return;
        } else if (clicked == "SubmenuCancel") submenu = false;
        else if (clicked == "ContinueButton") { continueSavedGame(); return; }
        else if (clicked == "SubmenuStart") { goMap(); return; }
        else if (clicked == "Options") g_screens.push(std::make_shared<OptionsScreen>(false));
        else if (clicked == "ExitButton") {
            g_screens.push(std::make_shared<YesNoScreen>("UI/YesNoExit.xml", "", [](bool yes) {
                if (yes) g_plat.quitRequested = true;
            }));
        } else if (clicked == "ChangeUserButton") {
            g_screens.push(std::make_shared<SelectUserScreen>(false, [this] { refresh(); }));
        } else if (clicked == "CreditsButton") {
            extern void showCredits();
            showCredits();
        } else if (clicked == "AwardsButton") {
            extern void showAwards();
            showAwards();
        }
        if (!clicked.empty()) refresh();
    }

    void draw() override {
        back.draw();
        for (auto& s : ui.background) s.draw();
        for (auto& kv : ui.buttons) {
            bool isSub = kv.first.rfind("Submenu", 0) == 0 || kv.first == "ContinueButton";
            if (isSub == submenu) kv.second.draw();
        }
        if (submenu) substrate.draw();
        userName.draw();
        userText.draw();
    }
};

void goMainMenu() { g_screens.replaceAll(std::make_shared<MainMenuScreen>()); }

// ---------------- Credits ----------------
class CreditsScreen : public Screen {
public:
    UiDoc ui;
    FixedBack back;
    std::vector<Text> lines;
    float scrollY = 0, speed = 30;
    float top = 0, bottom = 600;
    CreditsScreen() {
        back.load();
        ui.load("UI/Credits.xml");
        XNode r = ui.root();
        speed = xFloat(r, "Speed", 30);
        float y = 620;
        for (XNode n : r.children()) {
            if (xHas(n, "Font") && !isButtonNode(n)) {
                Text t;
                t.load(n);
                if (!t.hasPos) { t.pos = {400, y}; y += 40; }
                lines.push_back(t);
                ui.texts.erase(n.name());
            }
            for (XNode c : n.children())
                if (xHas(c, "Font") && !isButtonNode(c)) {
                    Text t;
                    t.load(c);
                    if (!t.hasPos) { t.pos = {400, y}; y += 40; }
                    lines.push_back(t);
                }
        }
        ui.extraTexts.clear();
    }
    void update(float dt, bool input) override {
        scrollY += speed * dt;
        std::string c = ui.update(dt, input);
        bool esc = false;
        for (auto k : g_plat.keysPressed) if (k == SDLK_ESCAPE) esc = true;
        if (!c.empty() || (input && (esc || (Input::released() && c.empty() && ui.buttons.empty())))) closed = true;
        float maxY = -1e9f;
        for (auto& t : lines) maxY = std::max(maxY, t.pos.y - scrollY);
        if (maxY < -40) scrollY = 0;
    }
    void draw() override {
        back.draw();
        ui.draw();
        for (auto& t : lines) {
            Vec2 p = t.pos - Vec2(0, scrollY);
            if (p.y > -30 && p.y < 630) t.drawAt(p);
        }
    }
};

void showCredits() { g_screens.push(std::make_shared<CreditsScreen>()); }

// ---------------- Logo ----------------
class LogoScreen : public Screen {
public:
    std::vector<std::pair<TexPtr, float>> logos;
    float t = 0;
    size_t idx = 0;
    LogoScreen() {
        if (g_vfs.exists("Logo/Me_logo.dds")) logos.push_back({g_plat.texture("Logo/Me_logo.dds"), 2.5f});
        if (g_vfs.exists("Logo/alawar_en.dds")) logos.push_back({g_plat.texture("Logo/alawar_en.dds"), 2.5f});
    }
    void update(float dt, bool input) override {
        t += dt;
        if (idx < logos.size() && (t > logos[idx].second || (input && Input::released()))) { t = 0; ++idx; }
        if (idx >= logos.size()) goMainMenu();
    }
    void draw() override {
        if (idx >= logos.size()) return;
        auto& tex = logos[idx].first;
        if (!tex) return;
        float d = logos[idx].second;
        float a = std::min(1.f, std::min(t / 0.5f, (d - t) / 0.5f));
        DrawCmd c;
        c.tex = tex;
        c.x = 400; c.y = 300; c.w = (float)tex->w; c.h = (float)tex->h;
        c.color.a = (uint8_t)(std::max(0.f, a) * 255);
        g_plat.draw(c);
    }
};

// ---------------- flow ----------------
static std::vector<std::pair<int, std::string>> g_script;

void gameFlowInit() {
    g_profiles.load();
    g_audio.setMusicVolume(g_profiles.settings.music);
    g_audio.setSoundVolume(g_profiles.settings.sound);
    if (g_profiles.settings.fullscreen) g_plat.setFullscreen(true);
    if (const char* s = getenv("FF_SCRIPT")) {
        // "frame:cmd;frame:cmd" where cmd = click x y | key name | level n | screen name
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, ';')) {
            auto c = item.find(':');
            if (c != std::string::npos) g_script.push_back({atoi(item.substr(0, c).c_str()), item.substr(c + 1)});
        }
    }
    if (getenv("FF_SKIP_LOGO")) goMainMenu();
    else g_screens.push(std::make_shared<LogoScreen>());
}

void gameFlowShutdown() {
    saveRunningGame();
    g_profiles.save();
}

void scriptedInput(int frame) {
    for (auto& e : g_script) {
        std::stringstream ss(e.second);
        std::string cmd;
        ss >> cmd;
        if (cmd == "move" && e.first == frame) { ss >> g_plat.mouse.x >> g_plat.mouse.y; }
        if (cmd == "click") {
            float x, y;
            ss >> x >> y;
            if (e.first == frame) { g_plat.mouse = {x, y}; g_plat.mouseDown = true; g_plat.mousePressed = true; }
            if (e.first + 2 == frame) { g_plat.mouse = {x, y}; g_plat.mouseDown = false; g_plat.mouseReleased = true; }
            if (e.first + 1 == frame) g_plat.mouse = {x, y};
        }
        if (e.first != frame) continue;
        if (cmd == "text") {
            std::string t;
            ss >> t;
            for (char32_t c : utf8to32(t)) g_plat.textInput.push_back(c);
        } else if (cmd == "key") {
            std::string k;
            ss >> k;
            if (k == "esc") g_plat.keysPressed.push_back(SDLK_ESCAPE);
            if (k == "enter") g_plat.keysPressed.push_back(SDLK_RETURN);
        } else if (cmd == "profile") {
            std::string n;
            ss >> n;
            if (!g_profiles.current()) {
                Profile p; p.name = n; g_profiles.list.push_back(p); g_profiles.currentIndex = (int)g_profiles.list.size() - 1;
            }
        } else if (cmd == "level") {
            int n; ss >> n; startLevel(n);
        } else if (cmd == "map") goMap();
        else if (cmd == "shop") goShop();
        else if (cmd == "collect") {
            // debug: click every product on the field
            if (auto g = g_screens.find<Game>()) {
                std::vector<Entity*> ps;
                for (auto& en : g->ents) if (dynamic_cast<Product*>(en.get()) && !en->dead) ps.push_back(en.get());
                if (getenv("FF_DEBUG")) fprintf(stderr, "[frame %d] collect %zu products, ents=%zu\n", frame, ps.size(), g->ents.size());
                for (auto* p : ps) p->click(*g);
            }
        } else if (cmd == "money") {
            long long n; ss >> n;
            if (auto g = g_screens.find<Game>()) g->money = n;
        } else if (cmd == "shopset") {
            int i, n; ss >> i >> n;
            if (auto* p = g_profiles.current()) p->shop[i] = n;
        } else if (cmd == "hitbear") {
            int n = 1; ss >> n;
            if (auto g = g_screens.find<Game>())
                for (auto& en : g->ents)
                    if (auto* b = dynamic_cast<Bear*>(en.get())) { for (int i = 0; i < n && !b->dead; ++i) b->click(*g); break; }
        } else if (cmd == "award") {
            int a; ss >> a; giveAward(a);
        } else if (cmd == "awards") {
            showAwards();
        } else if (cmd == "savegame") {
            saveRunningGame();
        } else if (cmd == "continue") {
            continueSavedGame();
        } else if (cmd == "bear") {
            if (auto g = g_screens.find<Game>()) g->spawnBear();
        } else if (cmd == "stars") { long long n; ss >> n; if (auto* p = g_profiles.current()) p->stars = n; }
    }
}
