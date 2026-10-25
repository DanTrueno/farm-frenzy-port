#include "vfs.h"
#include "portable.h"
#include "stb_image.h"  // declarations only: its inflater replaces zlib
#include <cstdio>
#include <cstring>
#include <algorithm>

Vfs g_vfs;

static uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

std::string Vfs::normalize(const std::string& path) {
    std::string s;
    s.reserve(path.size());
    for (char c : path) {
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if (c == '/' && !s.empty() && s.back() == '/') continue;
        s += c;
    }
    while (s.size() > 2 && s[0] == '.' && s[1] == '/') s.erase(0, 2);
    if (!s.empty() && s[0] == '/') s.erase(0, 1);
    return s;
}

bool Vfs::mount(const std::string& packPath) {
    std::ifstream f(u8path(packPath), std::ios::binary | std::ios::ate);
    if (!f) return false;
    long sz = (long)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(sz);
    if (!f.read((char*)data.data(), sz)) return false;

    // Find end of central directory record.
    long eocd = -1;
    for (long i = sz - 22; i >= 0 && i >= sz - 65557; --i) {
        if (rd32(&data[i]) == 0x06054b50) { eocd = i; break; }
    }
    if (eocd < 0) return false;
    uint16_t count = rd16(&data[eocd + 10]);
    uint32_t cdOff = rd32(&data[eocd + 16]);
    int packIdx = (int)packs_.size();
    const uint8_t* p = &data[cdOff];
    for (int i = 0; i < count; ++i) {
        if (rd32(p) != 0x02014b50) break;
        Entry e;
        e.pack = packIdx;
        e.method = rd16(p + 10);
        e.compSize = rd32(p + 20);
        e.size = rd32(p + 24);
        uint16_t nlen = rd16(p + 28), xlen = rd16(p + 30), clen = rd16(p + 32);
        e.localOffset = rd32(p + 42);
        std::string name((const char*)p + 46, nlen);
        if (!name.empty() && name.back() != '/') entries_[normalize(name)] = e;
        p += 46 + nlen + xlen + clen;
    }
    packs_.push_back(std::move(data));
    return true;
}

bool Vfs::exists(const std::string& path) const {
    return entries_.count(normalize(path)) != 0;
}

bool Vfs::read(const std::string& path, std::vector<uint8_t>& out) const {
    auto it = entries_.find(normalize(path));
    if (it == entries_.end()) return false;
    const Entry& e = it->second;
    const std::vector<uint8_t>& pk = packs_[e.pack];
    const uint8_t* lh = &pk[e.localOffset];
    const uint8_t* src = lh + 30 + rd16(lh + 26) + rd16(lh + 28);
    out.resize(e.size);
    if (e.method == 0) {
        memcpy(out.data(), src, e.size);
        return true;
    }
    if (e.method != 8) return false;
    // raw deflate (zip method 8)
    int n = stbi_zlib_decode_noheader_buffer((char*)out.data(), (int)e.size, (const char*)src, (int)e.compSize);
    return n == (int)e.size;
}

std::vector<std::string> Vfs::list(const std::string& prefix) const {
    std::string np = normalize(prefix);
    std::vector<std::string> res;
    for (auto& kv : entries_)
        if (kv.first.compare(0, np.size(), np) == 0) res.push_back(kv.first);
    std::sort(res.begin(), res.end());
    return res;
}
