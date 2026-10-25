#include "level.h"
#include "profile.h"
#include <cstdlib>

static std::shared_ptr<XmlDoc> records() {
    static std::shared_ptr<XmlDoc> d = loadXml("LevelsRecords.xml");
    return d;
}

int levelCount() {
    int n = 0;
    for (XNode r : records()->root().children("record")) { (void)r; ++n; }
    return std::min(n, MAX_LEVELS);
}

static XNode recordNode(int n) {
    int i = 1;
    for (XNode r : records()->root().children("record")) {
        if (i == n) return r;
        ++i;
    }
    return XNode();
}

bool loadLevelInfo(int n, LevelInfo& li) {
    li = LevelInfo();
    li.number = n;
    XNode r = recordNode(n);
    if (!r) return false;
    li.startMoney = atoll(xStr(r, "StartMoney", "0").c_str());
    li.goldTime = xFloat(r, "GoldTime", 300);
    li.silverTime = xFloat(r, "SilverTime", 600);
    li.prize = xInt(r, "Prize");
    li.silverPrize = xInt(r, "SilverPrize");
    li.goldPrize = xInt(r, "GoldPrize");
    li.bearKillingNum = xInt(r, "BearKillingNum", 3);
    for (XNode p : r.child("StartPets").children("Pet")) li.pets.push_back(xInt(p, "Type"));
    for (XNode g : r.child("Goals").children("goal")) li.goals.push_back({xInt(g, "Type"), atoll(xStr(g, "Val").c_str())});
    for (XNode h : r.child("Houses").children("h"))
        li.houses.push_back({xInt(h, "Type"), xInt(h, "PosIndex"), xInt(h, "Upgrade")});
    for (XNode c : r.child("Accessibility").children("c")) li.access.push_back({xInt(c, "CritID"), xInt(c, "Value")});

    auto pack = loadXml("Packs/" + std::to_string(n) + ".xml");
    if (pack->ok) {
        XNode root = pack->root();
        for (XNode t : root.child("BearTimes").children("t")) li.bearTimes.push_back((float)atof(t.text().get()));
        for (XNode o : root.child("Back").child("Background").children()) {
            if (o.type() != pugi::node_element) continue;
            Sprite s;
            s.load(o);
            li.back.push_back(s);
        }
    }
    return true;
}

int critToShop(int crit) {
    switch (crit) {
        case 1: return SHOP_CAKE;
        case 2: return SHOP_POWDER;
        case 3: return SHOP_SPINNERY;
        case 4: return SHOP_TEXTILE;
        case 5: return SHOP_BUTTER;
        case 6: return SHOP_CHEESE;
        default: return -1;
    }
}

bool levelAccessible(const LevelInfo& li) {
    Profile* p = g_profiles.current();
    if (!p) return false;
    for (auto& c : li.access) {
        int s = critToShop(c.first);
        if (s < 0) continue;
        if (p->shop[s] < c.second) return false;
    }
    return true;
}
