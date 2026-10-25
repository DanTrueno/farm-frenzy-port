#include "platform.h"
#include "engine.h"
#include "screen.h"
#include "vfs.h"
#include "game_flow.h"
#include "tips.h"
#include "portable.h"
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

std::string g_dataDir;


// Locate the original game's Data folder (data.pack + ru.pack).
static std::string findDataDir() {
    std::vector<std::string> cands;
    if (const char* e = getenv("FF_DATA")) cands.push_back(e);
    if (const char* b = SDL_GetBasePath()) {
        std::string base = b;
        cands.push_back(base + "Data");                       // Contents/Resources/Data in the .app
        cands.push_back(base + "../Resources/Data");
        cands.push_back(base + "../../../Data");             // next to the .app
        cands.push_back(base + "../../../Веселая ферма/Data");
    }
    if (const char* pref = SDL_GetPrefPath("Melesta", "FarmFrenzy")) {
        cands.push_back(std::string(pref) + "Data");
    }
    cands.push_back("Data");
    for (auto& c : cands)
        if (fileExists(c + "/data.pack")) return c;
    return "";
}

static void saveScreenshot(const char* path) {
    SDL_Surface* s = SDL_RenderReadPixels(g_plat.renderer(), nullptr);
    if (!s) return;
    SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ABGR8888);
    stbi_write_png(path, c->w, c->h, 4, c->pixels, c->pitch);
    SDL_DestroySurface(c);
    SDL_DestroySurface(s);
}

int main(int argc, char** argv) {
    std::string data = findDataDir();
    if (data.empty()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Farm Frenzy",
                                 "Не найдены файлы игры (Data/data.pack).\n"
                                 "Положите папку Data рядом с приложением или задайте FF_DATA.",
                                 nullptr);
        return 1;
    }
    g_dataDir = data;
    g_vfs.mount(data + "/data.pack");
    g_vfs.mount(data + "/ru.pack");  // locale overrides

    if (!g_plat.init("Веселая ферма")) return 1;
    if (!getenv("FF_NO_HD") && g_plat.mountHd(data + "/hd.pack")) fprintf(stderr, "HD textures: on\n");
    g_audio.init();
    loadStrings();
    tipsLoad(g_dataDir);
    gameFlowInit();

    const char* shot = getenv("FF_SHOT");
    int shotFrame = getenv("FF_SHOT_FRAME") ? atoi(getenv("FF_SHOT_FRAME")) : 60;
    int frame = 0;

    uint64_t last = SDL_GetTicksNS();
    while (!g_plat.quitRequested && !g_screens.empty()) {
        g_plat.pollEvents();
        scriptedInput(frame);
        uint64_t now = SDL_GetTicksNS();
        float dt = (float)((now - last) / 1e9);
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (getenv("FF_FIXED_DT")) dt = 1.0f / 60.0f;
        g_screens.update(dt);
        g_audio.update();
        g_screens.draw();
        g_plat.render();
        if (shot && frame == shotFrame) {
            saveScreenshot(shot);
            break;
        }
        g_plat.swap();
        ++frame;
    }
    gameFlowShutdown();
    g_audio.shutdown();
    g_plat.shutdown();
    return 0;
}
