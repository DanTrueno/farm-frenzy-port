// Mid-level save (counterpart of cGame::saveLevelState / loadLevelState).
// Plain text, one object per line.
#include "game.h"
#include "portable.h"
#include <algorithm>
#include <fstream>
#include <sstream>

int Game::savedLevel(const std::string& path) {
    std::ifstream f(u8path(path));
    std::string k;
    int lvl = 0;
    if (f >> k >> lvl && k == "level") return lvl;
    return 0;
}

bool Game::saveState(const std::string& path) const {
    if (finished) return false;
    std::ofstream f(u8path(path));
    if (!f) return false;
    f << "level " << li.number << "\n";
    f << "game " << time << " " << money << " " << nextBear << " " << bearsCaught << " " << bearsAppeared << " "
      << bearsSold << " " << petsLost << " " << manualClicks << " " << tipsThisLevel << " " << cloudsLeftToRight << " " << warnings << "\n";
    f << "harvested";
    for (int h : harvested) f << " " << h;
    f << "\n";
    f << "well " << well.level << " " << well.water.value << " " << well.refilling << " " << well.refillT << " "
      << well.autoOn << "\n";
    f << "depot " << depot.level;
    for (int c : depot.count) f << " " << c;
    f << "\n";
    f << "truck " << truck.level << " " << truck.away << " " << truck.tripT << " " << truck.paid << " "
      << truck.cargoValue << " " << truck.boxes.size();
    for (auto& b : truck.boxes) f << " " << b.pet << " " << b.type << " " << b.count;
    f << "\n";
    for (auto& h : houses)
        f << "house " << h->type << " " << h->posIndex << " " << h->level << " " << h->maxLevel << " " << h->busy
          << " " << h->t << " " << h->batch << "\n";
    for (int c = 0; c < cols; ++c)
        for (int r = 0; r < rows; ++r)
            if (auto& g = grass[c][r]) f << "grass " << c << " " << r << " " << g->value << " " << g->target << "\n";
    for (auto& e : ents) {
        if (e->dead) continue;
        if (auto* p = dynamic_cast<ProductionPet*>(e.get())) {
            if (p->state == ProductionPet::DYING) continue;
            f << "pet " << p->type << " " << p->pos.x << " " << p->pos.y << " " << p->life << " " << p->prodTimer << "\n";
        } else if (auto* d = dynamic_cast<Dog*>(e.get())) {
            f << "dog " << d->pos.x << " " << d->pos.y << "\n";
        } else if (auto* c = dynamic_cast<Cat*>(e.get())) {
            f << "cat " << c->pos.x << " " << c->pos.y << "\n";
        } else if (auto* b = dynamic_cast<Bear*>(e.get())) {
            if (b->leaving) continue;
            f << "bear " << b->pos.x << " " << b->pos.y << " " << b->cage << " " << b->kills << " " << b->stopped << "\n";
        } else if (auto* pr = dynamic_cast<Product*>(e.get())) {
            f << "product " << pr->type << " " << pr->pos.x << " " << pr->pos.y << " " << pr->life << "\n";
        }
    }
    return true;
}

bool Game::loadState(const std::string& path) {
    std::ifstream f(u8path(path));
    if (!f) return false;
    std::string line;
    std::vector<std::pair<int, int>> built;  // type, place of houses in the save
    ents.clear();
    houses.clear();
    for (auto& col : grass) for (auto& g : col) g.reset();
    XNode gx = g_data.grassXml->root();
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string k;
        ss >> k;
        if (k == "game") {
            ss >> time >> money >> nextBear >> bearsCaught >> bearsAppeared >> bearsSold >> petsLost >> manualClicks >>
                tipsThisLevel >> cloudsLeftToRight >> warnings;
        } else if (k == "harvested") {
            for (int& h : harvested) ss >> h;
        } else if (k == "well") {
            ss >> well.level >> well.water.value >> well.refilling >> well.refillT >> well.autoOn;
        } else if (k == "depot") {
            ss >> depot.level;
            for (int& c : depot.count) ss >> c;
        } else if (k == "truck") {
            size_t n = 0;
            ss >> truck.level >> truck.away >> truck.tripT >> truck.paid >> truck.cargoValue >> n;
            truck.boxes.resize(n);
            for (auto& b : truck.boxes) ss >> b.pet >> b.type >> b.count;
        } else if (k == "house") {
            int t, p;
            auto h = std::make_unique<ProdHouse>();
            ss >> t >> p;
            h->load((HouseType)t, p);
            ss >> h->level >> h->maxLevel >> h->busy >> h->t >> h->batch;
            built.push_back({t, p});
            houses.push_back(std::move(h));
        } else if (k == "grass") {
            int c, r;
            float v, tg;
            ss >> c >> r >> v >> tg;
            if (c < 0 || c >= cols || r < 0 || r >= rows) continue;
            auto g = std::make_unique<Grass>();
            g->pos = cellPos(c, r);
            g->anim.load(gx, false);
            g->anim.play = false;
            g->size = xVec(gx, "size", {48, 48});
            g->maxValue = xFloat(gx, "ProgressMaxValue", 30);
            g->value = v;
            g->target = tg;
            grass[c][r] = std::move(g);
        } else if (k == "pet") {
            int t;
            Vec2 p;
            ss >> t >> p.x >> p.y;
            spawnPet((PetType)t, false, p);
            if (auto* pp = dynamic_cast<ProductionPet*>(ents.back().get())) ss >> pp->life >> pp->prodTimer;
            petLanded[t] = true;
        } else if (k == "dog" || k == "cat") {
            Vec2 p;
            ss >> p.x >> p.y;
            spawnPet(k == "dog" ? PET_DOG : PET_CAT, false, p);
        } else if (k == "bear") {
            auto b = std::make_unique<Bear>();
            b->load(*this);
            ss >> b->pos.x >> b->pos.y >> b->cage >> b->kills >> b->stopped;
            b->fall.falling = false;
            b->shadowT = -1;
            ents.push_back(std::move(b));
        } else if (k == "product") {
            int t;
            Vec2 p;
            float life;
            ss >> t >> p.x >> p.y >> life;
            spawnProduct((ProdType)t, p, false);
            auto* pr = static_cast<Product*>(ents.back().get());
            pr->pos = p;
            pr->life = life;
            if (t == P_BEAR) {
                auto x = loadXml("Products/bearPrisoner.xml");
                int lvl = std::min(3, shopLevel(SHOP_CAGE));
                pr->cageAnim.load(x->root().child(("Up" + std::to_string(lvl)).c_str()).child("Anim"), true);
                pr->cageAnim.play = true;
            }
        }
    }
    // building places that were used in the saved game
    housePlaces.erase(std::remove_if(housePlaces.begin(), housePlaces.end(), [&](const HousePlace& hp) {
        for (auto& b : built) if (b.second == hp.posIndex) return true;
        return false;
    }), housePlaces.end());
    introShown = true;
    return true;
}
