// Platform layer: window, renderer, textures, audio, input. Built on SDL3.
#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float k) const { return {x * k, y * k}; }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    float len() const;
};

struct Texture {
    SDL_Texture* tex = nullptr;
    SDL_Texture* hd = nullptr;  // AI-upscaled version (hd.pack), same logical size
    int w = 0, h = 0;           // logical size = original texture size
    std::vector<uint8_t> alpha; // per-pixel alpha for hit testing
    bool opaqueAt(float u, float v) const; // u,v in [0,1]
    ~Texture();
};
using TexPtr = std::shared_ptr<Texture>;

// Colour as 0xAARRGGBB like the original engine.
struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    static Color fromArgb(uint32_t v) { return {uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), uint8_t(v >> 24)}; }
};

enum class Blend { Alpha, Add };

struct DrawCmd {
    TexPtr tex;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    float x = 0, y = 0, w = 0, h = 0; // centre + size in virtual 800x600 space
    float angle = 0;                   // degrees
    Color color;
    Blend blend = Blend::Alpha;
    int deep = 0;
    int seq = 0;
    bool flipX = false;
    // optional clip rect (in virtual coords); w<=0 means none
    SDL_FRect clip{0, 0, 0, 0};
};

class Platform {
public:
    static constexpr int W = 800, H = 600;
    bool init(const char* title);
    void shutdown();
    TexPtr texture(const std::string& path);   // cached, from VFS
    void draw(const DrawCmd& c);              // queued, sorted by deep
    void present();
    void render();   // draw queued commands without presenting
    void swap() { SDL_RenderPresent(ren_); }
    void toggleFullscreen();
    bool fullscreen() const { return fullscreen_; }
    void setFullscreen(bool fs);
    bool mountHd(const std::string& packPath);  // optional upscaled textures
    bool hdEnabled = true;

    // Input state (virtual coordinates)
    Vec2 mouse;
    bool mouseDown = false, mousePressed = false, mouseReleased = false;
    bool quitRequested = false;
    int deepBias = 0;  // added to every draw (screen layering)
    std::vector<uint32_t> textInput;  // unicode code points typed this frame
    std::vector<SDL_Keycode> keysPressed;
    void pollEvents();

    SDL_Renderer* renderer() { return ren_; }
    SDL_Window* window() { return win_; }

private:
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    bool fullscreen_ = false;
    std::unordered_map<std::string, TexPtr> cache_;
    bool useHdNow_ = false;
    std::vector<DrawCmd> queue_;
    int seq_ = 0;
};

extern Platform g_plat;

// ---------------- Audio ----------------
class Audio {
public:
    bool init();
    void shutdown();
    void playSound(const std::string& path, float volume = 1.0f);
    void playMusic(const std::string& path);
    void stopMusic();
    void setMusicVolume(float v);  // 0..1
    void setSoundVolume(float v);
    float musicVolume() const { return musicVol_; }
    float soundVolume() const { return soundVol_; }
    void update();

    struct Pcm { std::vector<float> samples; int channels = 2; int rate = 44100; };
private:
    std::shared_ptr<Pcm> load(const std::string& path);
    std::unordered_map<std::string, std::shared_ptr<Pcm>> cache_;
    SDL_AudioDeviceID dev_ = 0;
    struct Voice { std::shared_ptr<Pcm> pcm; SDL_AudioStream* stream; };
    std::vector<Voice> voices_;
    SDL_AudioStream* music_ = nullptr;
    std::shared_ptr<Pcm> musicPcm_;
    std::string musicPath_;
    float musicVol_ = 1, soundVol_ = 1;
};

extern Audio g_audio;
