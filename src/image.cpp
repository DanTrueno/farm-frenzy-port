#include "image.h"
#include <cstring>
#include <cstdlib>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"

static uint32_t be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

// zlib stream (JNG alpha IDAT) via stb_image's built-in inflater - no zlib dependency
static bool inflateAll(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, size_t expected) {
    out.resize(expected);
    int n = stbi_zlib_decode_buffer((char*)out.data(), (int)expected, (const char*)in.data(), (int)in.size());
    return n >= 0;
}

static uint8_t paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    if (pa <= pb && pa <= pc) return (uint8_t)a;
    if (pb <= pc) return (uint8_t)b;
    return (uint8_t)c;
}

// PNG-style unfiltering of an 8-bit single channel image.
static bool unfilterGray8(const std::vector<uint8_t>& raw, int w, int h, std::vector<uint8_t>& out) {
    out.assign((size_t)w * h, 255);
    size_t stride = (size_t)w + 1;
    if (raw.size() < stride * h) return false;
    std::vector<uint8_t> prev(w, 0);
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = &raw[stride * y];
        uint8_t ft = src[0];
        uint8_t* dst = &out[(size_t)w * y];
        for (int x = 0; x < w; ++x) {
            int a = x > 0 ? dst[x - 1] : 0, b = prev[x], c = x > 0 ? prev[x - 1] : 0;
            int v = src[x + 1];
            switch (ft) {
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += paeth(a, b, c); break;
            }
            dst[x] = (uint8_t)v;
        }
        memcpy(prev.data(), dst, w);
    }
    return true;
}

static bool decodeJng(const uint8_t* d, size_t size, Image& out) {
    size_t p = 8;
    uint32_t w = 0, h = 0;
    uint8_t alphaDepth = 0, alphaComp = 0;
    std::vector<uint8_t> jdat, jdaa, idat;
    while (p + 12 <= size) {
        uint32_t len = be32(d + p);
        const uint8_t* t = d + p + 4;
        const uint8_t* c = d + p + 8;
        if (p + 12 + len > size) break;
        if (!memcmp(t, "JHDR", 4)) {
            w = be32(c); h = be32(c + 4);
            alphaDepth = c[12]; alphaComp = c[13];
        } else if (!memcmp(t, "JDAT", 4)) jdat.insert(jdat.end(), c, c + len);
        else if (!memcmp(t, "JDAA", 4)) jdaa.insert(jdaa.end(), c, c + len);
        else if (!memcmp(t, "IDAT", 4)) idat.insert(idat.end(), c, c + len);
        else if (!memcmp(t, "IEND", 4)) break;
        p += 12 + len;
    }
    int jw, jh, jc;
    uint8_t* rgb = stbi_load_from_memory(jdat.data(), (int)jdat.size(), &jw, &jh, &jc, 4);
    if (!rgb) return false;
    out.w = jw; out.h = jh;
    out.rgba.assign(rgb, rgb + (size_t)jw * jh * 4);
    stbi_image_free(rgb);
    if (alphaDepth == 0) return true;
    std::vector<uint8_t> alpha;
    if (alphaComp == 8 && !jdaa.empty()) {
        int aw, ah, ac;
        uint8_t* a = stbi_load_from_memory(jdaa.data(), (int)jdaa.size(), &aw, &ah, &ac, 1);
        if (a && aw == jw && ah == jh) alpha.assign(a, a + (size_t)aw * ah);
        if (a) stbi_image_free(a);
    } else if (!idat.empty() && alphaDepth == 8) {
        std::vector<uint8_t> raw;
        if (inflateAll(idat, raw, (size_t)(w + 1) * h)) unfilterGray8(raw, w, h, alpha);
    }
    if (alpha.size() == (size_t)jw * jh)
        for (size_t i = 0; i < alpha.size(); ++i) out.rgba[i * 4 + 3] = alpha[i];
    return true;
}

static void decodeDxt5Block(const uint8_t* b, uint8_t px[16][4]) {
    uint8_t a[8];
    a[0] = b[0]; a[1] = b[1];
    if (a[0] > a[1]) for (int i = 1; i < 7; ++i) a[i + 1] = (uint8_t)(((7 - i) * a[0] + i * a[1]) / 7);
    else {
        for (int i = 1; i < 5; ++i) a[i + 1] = (uint8_t)(((5 - i) * a[0] + i * a[1]) / 5);
        a[6] = 0; a[7] = 255;
    }
    uint64_t abits = 0;
    for (int i = 0; i < 6; ++i) abits |= (uint64_t)b[2 + i] << (8 * i);
    const uint8_t* cb = b + 8;
    uint16_t c0 = cb[0] | (cb[1] << 8), c1 = cb[2] | (cb[3] << 8);
    uint8_t col[4][3];
    auto expand = [](uint16_t c, uint8_t* o) {
        o[0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
        o[1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
        o[2] = (uint8_t)((c & 31) * 255 / 31);
    };
    expand(c0, col[0]); expand(c1, col[1]);
    for (int k = 0; k < 3; ++k) {
        col[2][k] = (uint8_t)((2 * col[0][k] + col[1][k]) / 3);
        col[3][k] = (uint8_t)((col[0][k] + 2 * col[1][k]) / 3);
    }
    uint32_t idx = le32(cb + 4);
    for (int i = 0; i < 16; ++i) {
        int ci = (idx >> (2 * i)) & 3;
        px[i][0] = col[ci][0]; px[i][1] = col[ci][1]; px[i][2] = col[ci][2];
        px[i][3] = a[(abits >> (3 * i)) & 7];
    }
}

static bool decodeDds(const uint8_t* d, size_t size, Image& out) {
    if (size < 128) return false;
    uint32_t h = le32(d + 12), w = le32(d + 16);
    uint32_t pfFlags = le32(d + 80);
    const uint8_t* four = d + 84;
    uint32_t bits = le32(d + 88);
    const uint8_t* px = d + 128;
    size_t avail = size - 128;
    out.w = w; out.h = h;
    out.rgba.assign((size_t)w * h * 4, 0);
    if ((pfFlags & 4) && !memcmp(four, "DXT5", 4)) {
        size_t bw = (w + 3) / 4, bh = (h + 3) / 4;
        if (avail < bw * bh * 16) return false;
        for (size_t by = 0; by < bh; ++by)
            for (size_t bx = 0; bx < bw; ++bx) {
                uint8_t blk[16][4];
                decodeDxt5Block(px + (by * bw + bx) * 16, blk);
                for (int i = 0; i < 16; ++i) {
                    size_t x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                    if (x < w && y < h) memcpy(&out.rgba[(y * w + x) * 4], blk[i], 4);
                }
            }
        return true;
    }
    if (bits == 32) {
        if (avail < (size_t)w * h * 4) return false;
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            out.rgba[i * 4 + 0] = px[i * 4 + 2];
            out.rgba[i * 4 + 1] = px[i * 4 + 1];
            out.rgba[i * 4 + 2] = px[i * 4 + 0];
            out.rgba[i * 4 + 3] = (pfFlags & 1) ? px[i * 4 + 3] : 255;
        }
        return true;
    }
    if (bits == 16) {
        if (avail < (size_t)w * h * 2) return false;
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            uint16_t v = px[i * 2] | (px[i * 2 + 1] << 8);
            out.rgba[i * 4 + 0] = ((v >> 8) & 15) * 17;
            out.rgba[i * 4 + 1] = ((v >> 4) & 15) * 17;
            out.rgba[i * 4 + 2] = (v & 15) * 17;
            out.rgba[i * 4 + 3] = ((v >> 12) & 15) * 17;
        }
        return true;
    }
    return false;
}

bool decodeImage(const uint8_t* data, size_t size, Image& out) {
    if (size >= 8 && data[0] == 0x8b && !memcmp(data + 1, "JNG", 3)) return decodeJng(data, size, out);
    if (size >= 4 && !memcmp(data, "DDS ", 4)) return decodeDds(data, size, out);
    int w, h, c;
    uint8_t* p = stbi_load_from_memory(data, (int)size, &w, &h, &c, 4);
    if (!p) return false;
    out.w = w; out.h = h;
    out.rgba.assign(p, p + (size_t)w * h * 4);
    stbi_image_free(p);
    return true;
}
