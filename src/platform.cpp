#include "platform.h"
#include "portable.h"
#include "vfs.h"
#include "image.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

Platform g_plat;
Audio g_audio;

float Vec2::len() const { return std::sqrt(x * x + y * y); }

Texture::~Texture() {
    if (tex) SDL_DestroyTexture(tex);
    if (hd) SDL_DestroyTexture(hd);
}

static Vfs g_vfsHd;
static bool g_hdMounted = false;

bool Platform::mountHd(const std::string& packPath) {
    g_hdMounted = g_vfsHd.mount(packPath);
    return g_hdMounted;
}

bool Texture::opaqueAt(float u, float v) const {
    if (alpha.empty()) return true;
    int x = (int)(u * w), y = (int)(v * h);
    if (x < 0 || y < 0 || x >= w || y >= h) return false;
    return alpha[(size_t)y * w + x] > 16;
}

bool Platform::init(const char* title) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    win_ = SDL_CreateWindow(title, W, H, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win_) return false;
    ren_ = SDL_CreateRenderer(win_, nullptr);
    if (!ren_) return false;
    SDL_SetRenderVSync(ren_, 1);
    SDL_SetRenderLogicalPresentation(ren_, W, H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_StartTextInput(win_);
    return true;
}

void Platform::shutdown() {
    cache_.clear();
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
    SDL_Quit();
}

void Platform::setFullscreen(bool fs) {
    fullscreen_ = fs;
    bool ok = SDL_SetWindowFullscreen(win_, fs);
    if (getenv("FF_DEBUG")) fprintf(stderr, "setFullscreen(%d) -> %d %s\n", fs, ok, ok ? "" : SDL_GetError());
}

void Platform::toggleFullscreen() { setFullscreen(!fullscreen_); }

TexPtr Platform::texture(const std::string& path) {
    std::string key = Vfs::normalize(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    auto t = std::make_shared<Texture>();
    std::vector<uint8_t> data;
    Image img;
    std::string usedKey = key;
    bool ok = g_vfs.read(key, data) && decodeImage(data.data(), data.size(), img);
    if (!ok) {
        // Some XMLs reference .dds where the file is .jng or the other way round
        std::string alt = key;
        size_t dot = alt.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = alt.substr(dot);
            alt = alt.substr(0, dot) + (ext == ".dds" ? ".jng" : ".dds");
            ok = g_vfs.read(alt, data) && decodeImage(data.data(), data.size(), img);
            if (ok) usedKey = alt;
        }
    }
    if (!ok) {
        fprintf(stderr, "texture missing: %s\n", path.c_str());
        cache_[key] = nullptr;
        return nullptr;
    }
    t->w = img.w;
    t->h = img.h;
    t->alpha.resize((size_t)img.w * img.h);
    for (size_t i = 0; i < t->alpha.size(); ++i) t->alpha[i] = img.rgba[i * 4 + 3];
    t->tex = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, img.w, img.h);
    SDL_UpdateTexture(t->tex, nullptr, img.rgba.data(), img.w * 4);
    SDL_SetTextureScaleMode(t->tex, SDL_SCALEMODE_LINEAR);
    // upscaled copy: same path inside hd.pack (PNG), drawn at the original logical size
    if (g_hdMounted) {
        std::vector<uint8_t> hdData;
        Image hdImg;
        if (g_vfsHd.read(usedKey, hdData) && decodeImage(hdData.data(), hdData.size(), hdImg) && hdImg.w > img.w) {
            t->hd = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, hdImg.w, hdImg.h);
            SDL_UpdateTexture(t->hd, nullptr, hdImg.rgba.data(), hdImg.w * 4);
            SDL_SetTextureScaleMode(t->hd, SDL_SCALEMODE_LINEAR);
        }
    }
    cache_[key] = t;
    return t;
}

void Platform::draw(const DrawCmd& c) {
    if (!c.tex || c.color.a == 0) return;
    queue_.push_back(c);
    queue_.back().deep += deepBias;
    queue_.back().seq = seq_++;
}

void Platform::present() {
    render();
    SDL_RenderPresent(ren_);
}

void Platform::render() {
    // HD textures only pay off when the 800x600 scene is drawn noticeably larger;
    // without mipmaps they would shimmer in a small window
    int ow = W, oh = H;
    SDL_GetCurrentRenderOutputSize(ren_, &ow, &oh);
    useHdNow_ = hdEnabled && std::min(ow / (float)W, oh / (float)H) >= 1.5f;
    SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
    SDL_RenderClear(ren_);
    std::stable_sort(queue_.begin(), queue_.end(), [](const DrawCmd& a, const DrawCmd& b) {
        if (a.deep != b.deep) return a.deep < b.deep;
        return a.seq < b.seq;
    });
    for (auto& c : queue_) {
        SDL_Texture* st = (useHdNow_ && c.tex->hd) ? c.tex->hd : c.tex->tex;
        SDL_SetTextureBlendMode(st, c.blend == Blend::Add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
        if (c.clip.w > 0) {
            SDL_Rect r{(int)c.clip.x, (int)c.clip.y, (int)c.clip.w, (int)c.clip.h};
            SDL_SetRenderClipRect(ren_, &r);
        }
        float hw = c.w * 0.5f, hh = c.h * 0.5f;
        float ca = 1, sa = 0;
        if (c.angle != 0) {
            float r = c.angle * kPi / 180.0f;
            ca = std::cos(r);
            sa = std::sin(r);
        }
        float u0 = c.flipX ? c.u1 : c.u0, u1 = c.flipX ? c.u0 : c.u1;
        const float lx[4] = {-hw, hw, hw, -hw}, ly[4] = {-hh, -hh, hh, hh};
        const float uu[4] = {u0, u1, u1, u0}, vv[4] = {c.v0, c.v0, c.v1, c.v1};
        SDL_Vertex v[4];
        SDL_FColor col{c.color.r / 255.f, c.color.g / 255.f, c.color.b / 255.f, c.color.a / 255.f};
        for (int i = 0; i < 4; ++i) {
            v[i].position.x = c.x + lx[i] * ca - ly[i] * sa;
            v[i].position.y = c.y + lx[i] * sa + ly[i] * ca;
            v[i].color = col;
            v[i].tex_coord.x = uu[i];
            v[i].tex_coord.y = vv[i];
        }
        static const int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ren_, st, v, 4, idx, 6);
        if (c.clip.w > 0) SDL_SetRenderClipRect(ren_, nullptr);
    }
    queue_.clear();
    seq_ = 0;
}

void Platform::pollEvents() {
    mousePressed = mouseReleased = false;
    textInput.clear();
    keysPressed.clear();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        SDL_ConvertEventToRenderCoordinates(ren_, &e);
        switch (e.type) {
            case SDL_EVENT_QUIT: quitRequested = true; break;
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN: fullscreen_ = true; break;
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN: fullscreen_ = false; break;
            case SDL_EVENT_MOUSE_MOTION: mouse = {e.motion.x, e.motion.y}; break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouse = {e.button.x, e.button.y};
                    mouseDown = true;
                    mousePressed = true;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouse = {e.button.x, e.button.y};
                    mouseDown = false;
                    mouseReleased = true;
                }
                break;
            case SDL_EVENT_TEXT_INPUT: {
                // decode UTF-8
                const unsigned char* s = (const unsigned char*)e.text.text;
                while (*s) {
                    uint32_t cp;
                    if (*s < 0x80) cp = *s++;
                    else if ((*s & 0xE0) == 0xC0) { cp = (*s++ & 0x1F) << 6; cp |= (*s++ & 0x3F); }
                    else if ((*s & 0xF0) == 0xE0) { cp = (*s++ & 0x0F) << 12; cp |= (*s++ & 0x3F) << 6; cp |= (*s++ & 0x3F); }
                    else { cp = (*s++ & 0x07) << 18; cp |= (*s++ & 0x3F) << 12; cp |= (*s++ & 0x3F) << 6; cp |= (*s++ & 0x3F); }
                    textInput.push_back(cp);
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN:
                keysPressed.push_back(e.key.key);
                if (e.key.key == SDLK_RETURN && (e.key.mod & SDL_KMOD_ALT)) toggleFullscreen();
                if (e.key.key == SDLK_F && (e.key.mod & SDL_KMOD_GUI) && (e.key.mod & SDL_KMOD_CTRL)) toggleFullscreen();
                break;
        }
    }
}

// ---------------- Audio ----------------

bool Audio::init() {
    SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 44100};
    dev_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    return dev_ != 0;
}

void Audio::shutdown() {
    for (auto& v : voices_) SDL_DestroyAudioStream(v.stream);
    voices_.clear();
    if (music_) SDL_DestroyAudioStream(music_);
    music_ = nullptr;
    if (dev_) SDL_CloseAudioDevice(dev_);
}

std::shared_ptr<Audio::Pcm> Audio::load(const std::string& path) {
    std::string key = Vfs::normalize(path);
    auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    std::vector<uint8_t> data;
    std::shared_ptr<Pcm> pcm;
    if (g_vfs.read(key, data)) {
        int ch = 0, rate = 0;
        short* out = nullptr;
        int n = stb_vorbis_decode_memory(data.data(), (int)data.size(), &ch, &rate, &out);
        if (n > 0) {
            pcm = std::make_shared<Pcm>();
            pcm->channels = ch;
            pcm->rate = rate;
            pcm->samples.resize((size_t)n * ch);
            for (size_t i = 0; i < pcm->samples.size(); ++i) pcm->samples[i] = out[i] / 32768.0f;
            free(out);
        }
    }
    if (!pcm) fprintf(stderr, "sound missing: %s\n", path.c_str());
    cache_[key] = pcm;
    return pcm;
}

void Audio::playSound(const std::string& path, float volume) {
    if (!dev_ || path.empty()) return;
    auto pcm = load(path);
    if (!pcm) return;
    SDL_AudioSpec src{SDL_AUDIO_F32, pcm->channels, pcm->rate};
    SDL_AudioStream* s = SDL_CreateAudioStream(&src, nullptr);
    SDL_SetAudioStreamGain(s, volume * soundVol_);
    SDL_PutAudioStreamData(s, pcm->samples.data(), (int)(pcm->samples.size() * sizeof(float)));
    SDL_FlushAudioStream(s);
    SDL_BindAudioStream(dev_, s);
    voices_.push_back({pcm, s});
}

void Audio::playMusic(const std::string& path) {
    if (!dev_) return;
    if (path == musicPath_ && music_) return;
    stopMusic();
    musicPcm_ = load(path);
    if (!musicPcm_) return;
    musicPath_ = path;
    SDL_AudioSpec src{SDL_AUDIO_F32, musicPcm_->channels, musicPcm_->rate};
    music_ = SDL_CreateAudioStream(&src, nullptr);
    SDL_SetAudioStreamGain(music_, musicVol_);
    SDL_PutAudioStreamData(music_, musicPcm_->samples.data(), (int)(musicPcm_->samples.size() * sizeof(float)));
    SDL_BindAudioStream(dev_, music_);
}

void Audio::stopMusic() {
    if (music_) SDL_DestroyAudioStream(music_);
    music_ = nullptr;
    musicPath_.clear();
}

void Audio::setMusicVolume(float v) {
    musicVol_ = v;
    if (music_) SDL_SetAudioStreamGain(music_, v);
}

void Audio::setSoundVolume(float v) { soundVol_ = v; }

void Audio::update() {
    for (size_t i = 0; i < voices_.size();) {
        if (SDL_GetAudioStreamQueued(voices_[i].stream) == 0) {
            SDL_DestroyAudioStream(voices_[i].stream);
            voices_.erase(voices_.begin() + i);
        } else ++i;
    }
    if (music_ && musicPcm_ && SDL_GetAudioStreamQueued(music_) < 44100 * 8) {
        SDL_PutAudioStreamData(music_, musicPcm_->samples.data(), (int)(musicPcm_->samples.size() * sizeof(float)));
    }
}
