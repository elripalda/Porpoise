/* Porpoise - a Wii disc's own tile and banner, played in software (see
 * porpoise_banner.hpp).
 *
 * Formats and behaviour after the Wii Banner Player Project's player
 * (Copyright (c) 2010 - Wii Banner Player Project, zlib licence; altered:
 * rewritten for Porpoise, with a software TEV, the GX alpha test and blend,
 * texture pattern keys and konst colours); see third_party/wii-banner-player.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_banner.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <sys/stat.h>

#include "porpoise_disc.hpp"

namespace porpoise::banner
{
namespace
{
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using s16 = std::int16_t;

/* ---- reading ------------------------------------------------------------------------------------ */

struct Reader
{
    const u8 *data = nullptr;
    std::size_t size = 0;
    bool ok(std::size_t at, std::size_t n) const { return at <= size && n <= size - at; }
    u8 u8_(std::size_t at) const { return ok(at, 1) ? data[at] : 0; }
    u16 u16_(std::size_t at) const { return ok(at, 2) ? u16(data[at] << 8 | data[at + 1]) : 0; }
    u32 u32_(std::size_t at) const
    {
        return ok(at, 4) ? u32(data[at]) << 24 | u32(data[at + 1]) << 16 | u32(data[at + 2]) << 8 | data[at + 3] : 0;
    }
    float f32(std::size_t at) const
    {
        const u32 v = u32_(at);
        float f;
        std::memcpy(&f, &v, 4);
        return std::isfinite(f) ? f : 0.0f;
    }
    std::string str(std::size_t at, std::size_t max) const
    {
        std::string s;
        for (std::size_t i = 0; i < max && ok(at + i, 1) && data[at + i]; ++i)
            s += char(data[at + i]);
        return s;
    }
};

std::string lower(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

/* A U8 archive: path -> (offset, size) of each file. */
bool read_u8(const std::vector<u8> &bytes, std::size_t base, std::map<std::string, std::pair<std::size_t, std::size_t>> &files)
{
    Reader r{bytes.data(), bytes.size()};
    if (r.u32_(base) != 0x55AA382D)
        return false;
    const std::size_t root = base + r.u32_(base + 4);
    const u32 count = r.u32_(root + 8);
    if (count == 0 || count > 100000 || !r.ok(root, std::size_t(count) * 12))
        return false;
    const std::size_t names = root + std::size_t(count) * 12;
    /* Walk the nodes keeping the folder path; a folder's size is the index
     * after its last child. */
    std::vector<std::pair<u32, std::string>> dirs = {{count, ""}};
    for (u32 i = 1; i < count; ++i)
    {
        while (dirs.size() > 1 && i >= dirs.back().first)
            dirs.pop_back();
        const std::size_t n = root + std::size_t(i) * 12;
        const bool dir = r.u8_(n) == 1;
        const std::string name = r.str(names + (r.u32_(n) & 0xFFFFFF), 256);
        const std::string path = dirs.back().second.empty() ? name : dirs.back().second + "/" + name;
        if (dir)
            dirs.push_back({r.u32_(n + 8), path});
        else
            files[lower(path)] = {base + r.u32_(n + 4), r.u32_(n + 8)};
    }
    return true;
}

/* Nintendo's LZ77 (type 1, "LZ77" header) and LZ10/LZ11; anything else is
 * returned as it is. */
std::vector<u8> unlz(const u8 *p, std::size_t n)
{
    std::size_t at = 0;
    if (n >= 4 && std::memcmp(p, "LZ77", 4) == 0)
        at = 4;
    if (n < at + 4 || (p[at] != 0x10 && p[at] != 0x11))
        return std::vector<u8>(p, p + n);
    const u8 type = p[at];
    std::size_t out_size = std::size_t(p[at + 1]) | std::size_t(p[at + 2]) << 8 | std::size_t(p[at + 3]) << 16;
    at += 4;
    if (out_size == 0 && n >= at + 4)
    {
        out_size = std::size_t(p[at]) | std::size_t(p[at + 1]) << 8 | std::size_t(p[at + 2]) << 16 |
                   std::size_t(p[at + 3]) << 24;
        at += 4;
    }
    if (out_size > (64u << 20))
        return {};
    std::vector<u8> out;
    out.reserve(out_size);
    while (out.size() < out_size && at < n)
    {
        const u8 flags = p[at++];
        for (int b = 0; b < 8 && out.size() < out_size; ++b)
        {
            if (!(flags & (0x80 >> b)))
            {
                if (at >= n)
                    break;
                out.push_back(p[at++]);
                continue;
            }
            std::size_t len = 0, disp = 0;
            if (type == 0x10)
            {
                if (at + 2 > n)
                    break;
                len = (p[at] >> 4) + 3;
                disp = ((std::size_t(p[at]) & 0xF) << 8 | p[at + 1]) + 1;
                at += 2;
            }
            else
            {
                if (at + 2 > n)
                    break;
                const u8 ind = p[at] >> 4;
                if (ind == 0)
                {
                    if (at + 3 > n)
                        break;
                    len = ((std::size_t(p[at]) & 0xF) << 4 | p[at + 1] >> 4) + 0x11;
                    disp = ((std::size_t(p[at + 1]) & 0xF) << 8 | p[at + 2]) + 1;
                    at += 3;
                }
                else if (ind == 1)
                {
                    if (at + 4 > n)
                        break;
                    len = ((std::size_t(p[at]) & 0xF) << 12 | std::size_t(p[at + 1]) << 4 | p[at + 2] >> 4) + 0x111;
                    disp = ((std::size_t(p[at + 2]) & 0xF) << 8 | p[at + 3]) + 1;
                    at += 4;
                }
                else
                {
                    len = ind + 1;
                    disp = ((std::size_t(p[at]) & 0xF) << 8 | p[at + 1]) + 1;
                    at += 2;
                }
            }
            if (disp > out.size())
                return {};
            for (std::size_t k = 0; k < len && out.size() < out_size; ++k)
                out.push_back(out[out.size() - disp]);
        }
    }
    return out;
}

/* One of opening.bnr's inner files (meta/icon.bin ...), its IMD5 header
 * skipped and decompressed. */
bool inner_file(const std::vector<u8> &bnr, const std::string &name, std::vector<u8> &out)
{
    /* The U8 archive follows the IMET header (0x600 on a disc, 0x640 in a
     * channel's 00000000.app). */
    std::map<std::string, std::pair<std::size_t, std::size_t>> files;
    bool found = false;
    for (std::size_t base : {std::size_t(0x600), std::size_t(0x640), std::size_t(0)})
        if (bnr.size() > base + 4 && read_u8(bnr, base, files))
        {
            found = true;
            break;
        }
    if (!found)
        return false;
    const auto it = files.find(name);
    if (it == files.end() || it->second.first + it->second.second > bnr.size())
        return false;
    std::size_t at = it->second.first, n = it->second.second;
    if (n >= 32 && std::memcmp(&bnr[at], "IMD5", 4) == 0)
    {
        at += 32;
        n -= 32;
    }
    out = unlz(&bnr[at], n);
    return !out.empty();
}

/* ---- textures ------------------------------------------------------------------------------------ */

struct Tex
{
    int w = 0, h = 0;
    std::vector<float> rgba; /* 0..1 */
};

void rgb5a3(u16 v, float *o)
{
    if (v & 0x8000)
    {
        o[0] = float((v >> 10) & 31) / 31.0f;
        o[1] = float((v >> 5) & 31) / 31.0f;
        o[2] = float(v & 31) / 31.0f;
        o[3] = 1;
    }
    else
    {
        o[0] = float((v >> 8) & 15) / 15.0f;
        o[1] = float((v >> 4) & 15) / 15.0f;
        o[2] = float(v & 15) / 15.0f;
        o[3] = float((v >> 12) & 7) / 7.0f;
    }
}

void rgb565(u16 v, float *o)
{
    o[0] = float((v >> 11) & 31) / 31.0f;
    o[1] = float((v >> 5) & 63) / 63.0f;
    o[2] = float(v & 31) / 31.0f;
    o[3] = 1;
}

void palette_entry(u16 v, u32 fmt, float *o)
{
    if (fmt == 0) /* IA8 */
    {
        o[0] = o[1] = o[2] = float(v & 0xFF) / 255.0f;
        o[3] = float(v >> 8) / 255.0f;
    }
    else if (fmt == 1)
        rgb565(v, o);
    else
        rgb5a3(v, o);
}

/* A TPL's first image, decoded. */
bool decode_tpl(const u8 *p, std::size_t n, Tex &t)
{
    Reader r{p, n};
    if (r.u32_(0) != 0x0020AF30 || r.u32_(4) == 0)
        return false;
    const std::size_t table = r.u32_(8);
    const std::size_t img = r.u32_(table), pal = r.u32_(table + 4);
    const int h = r.u16_(img), w = r.u16_(img + 2);
    const u32 fmt = r.u32_(img + 4);
    const std::size_t data = r.u32_(img + 8);
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024)
        return false;
    std::vector<u16> palette;
    u32 pal_fmt = 2;
    if (pal)
    {
        const u16 count = r.u16_(pal);
        pal_fmt = r.u32_(pal + 4);
        const std::size_t pd = r.u32_(pal + 8);
        for (u16 i = 0; i < count; ++i)
            palette.push_back(r.u16_(pd + std::size_t(i) * 2));
    }
    t.w = w;
    t.h = h;
    t.rgba.assign(std::size_t(w) * h * 4, 0.0f);
    int bw = 4, bh = 4;
    switch (fmt)
    {
    case 0: case 8: case 14: bw = 8; bh = 8; break; /* I4, C4, CMPR */
    case 1: case 2: case 9: bw = 8; bh = 4; break;  /* I8, IA4, C8 */
    default: break;
    }
    const int bx_n = (w + bw - 1) / bw, by_n = (h + bh - 1) / bh;
    std::size_t at = data;
    auto put = [&](int x, int y, const float *c) {
        if (x < w && y < h)
            std::memcpy(&t.rgba[(std::size_t(y) * w + x) * 4], c, sizeof(float) * 4);
    };
    for (int by = 0; by < by_n; ++by)
        for (int bx = 0; bx < bx_n; ++bx)
        {
            const int x0 = bx * bw, y0 = by * bh;
            float c[4];
            switch (fmt)
            {
            case 0: /* I4 */
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; x += 2)
                    {
                        const u8 b = r.u8_(at++);
                        c[0] = c[1] = c[2] = c[3] = float(b >> 4) / 15.0f;
                        put(x0 + x, y0 + y, c);
                        c[0] = c[1] = c[2] = c[3] = float(b & 15) / 15.0f;
                        put(x0 + x + 1, y0 + y, c);
                    }
                break;
            case 1: /* I8 */
                for (int y = 0; y < 4; ++y)
                    for (int x = 0; x < 8; ++x)
                    {
                        c[0] = c[1] = c[2] = c[3] = float(r.u8_(at++)) / 255.0f;
                        put(x0 + x, y0 + y, c);
                    }
                break;
            case 2: /* IA4 */
                for (int y = 0; y < 4; ++y)
                    for (int x = 0; x < 8; ++x)
                    {
                        const u8 b = r.u8_(at++);
                        c[0] = c[1] = c[2] = float(b & 15) / 15.0f;
                        c[3] = float(b >> 4) / 15.0f;
                        put(x0 + x, y0 + y, c);
                    }
                break;
            case 3: /* IA8 */
                for (int y = 0; y < 4; ++y)
                    for (int x = 0; x < 4; ++x)
                    {
                        c[3] = float(r.u8_(at)) / 255.0f;
                        c[0] = c[1] = c[2] = float(r.u8_(at + 1)) / 255.0f;
                        at += 2;
                        put(x0 + x, y0 + y, c);
                    }
                break;
            case 4: /* RGB565 */
            case 5: /* RGB5A3 */
                for (int y = 0; y < 4; ++y)
                    for (int x = 0; x < 4; ++x)
                    {
                        const u16 v = r.u16_(at);
                        at += 2;
                        if (fmt == 4)
                            rgb565(v, c);
                        else
                            rgb5a3(v, c);
                        put(x0 + x, y0 + y, c);
                    }
                break;
            case 6: /* RGBA8: AR then GB */
                for (int i = 0; i < 16; ++i)
                {
                    c[3] = float(r.u8_(at + i * 2)) / 255.0f;
                    c[0] = float(r.u8_(at + i * 2 + 1)) / 255.0f;
                    c[1] = float(r.u8_(at + 32 + i * 2)) / 255.0f;
                    c[2] = float(r.u8_(at + 32 + i * 2 + 1)) / 255.0f;
                    put(x0 + i % 4, y0 + i / 4, c);
                }
                at += 64;
                break;
            case 8: /* C4 */
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; x += 2)
                    {
                        const u8 b = r.u8_(at++);
                        for (int k = 0; k < 2; ++k)
                        {
                            const u8 idx = k ? b & 15 : b >> 4;
                            palette_entry(idx < palette.size() ? palette[idx] : 0, pal_fmt, c);
                            put(x0 + x + k, y0 + y, c);
                        }
                    }
                break;
            case 9: /* C8 */
                for (int y = 0; y < 4; ++y)
                    for (int x = 0; x < 8; ++x)
                    {
                        const u8 idx = r.u8_(at++);
                        palette_entry(idx < palette.size() ? palette[idx] : 0, pal_fmt, c);
                        put(x0 + x, y0 + y, c);
                    }
                break;
            case 10: /* C14X2 */
                for (int y = 0; y < 4; ++y)
                    for (int x = 0; x < 4; ++x)
                    {
                        const u16 idx = r.u16_(at) & 0x3FFF;
                        at += 2;
                        palette_entry(idx < palette.size() ? palette[idx] : 0, pal_fmt, c);
                        put(x0 + x, y0 + y, c);
                    }
                break;
            case 14: /* CMPR: four DXT1-like 4x4 blocks */
                for (int sub = 0; sub < 4; ++sub)
                {
                    const u16 c0 = r.u16_(at), c1 = r.u16_(at + 2);
                    float pal4[4][4];
                    rgb565(c0, pal4[0]);
                    rgb565(c1, pal4[1]);
                    if (c0 > c1)
                        for (int k = 0; k < 4; ++k)
                        {
                            pal4[2][k] = (2 * pal4[0][k] + pal4[1][k]) / 3;
                            pal4[3][k] = (pal4[0][k] + 2 * pal4[1][k]) / 3;
                        }
                    else
                        for (int k = 0; k < 4; ++k)
                        {
                            pal4[2][k] = (pal4[0][k] + pal4[1][k]) / 2;
                            pal4[3][k] = 0;
                        }
                    for (int y = 0; y < 4; ++y)
                    {
                        const u8 bits = r.u8_(at + 4 + y);
                        for (int x = 0; x < 4; ++x)
                            put(x0 + (sub & 1) * 4 + x, y0 + (sub >> 1) * 4 + y, pal4[(bits >> (6 - x * 2)) & 3]);
                    }
                    at += 8;
                }
                break;
            default:
                return false;
            }
        }
    return true;
}

/* ---- the layout ---------------------------------------------------------------------------------- */

struct HKey
{
    float frame, value, slope;
};
struct SKey
{
    float frame;
    u8 d1, d2;
};
struct Track
{
    u32 type;
    u8 index, target;
    bool step;
    std::vector<HKey> h;
    std::vector<SKey> s;
};
struct Anim
{
    std::vector<Track> tracks[2];
};

constexpr u32 fourcc(const char *s)
{
    return u32(u8(s[0])) << 24 | u32(u8(s[1])) << 16 | u32(u8(s[2])) << 8 | u8(s[3]);
}
constexpr u32 kRLPA = fourcc("RLPA"), kRLTS = fourcc("RLTS"), kRLVI = fourcc("RLVI"), kRLVC = fourcc("RLVC"),
              kRLMC = fourcc("RLMC"), kRLTP = fourcc("RLTP");

float hermite_at(const std::vector<HKey> &keys, float f)
{
    if (keys.empty())
        return 0;
    if (f <= keys.front().frame)
        return keys.front().value;
    if (f >= keys.back().frame)
        return keys.back().value;
    std::size_t i = 1;
    while (i < keys.size() && keys[i].frame < f)
        ++i;
    const HKey &a = keys[i - 1], &b = keys[i];
    const float nf = b.frame - a.frame;
    if (std::fabs(nf) < 0.01f)
        return a.value;
    const float t = (f - a.frame) / nf, t2 = t * t, t3 = t2 * t;
    return a.slope * nf * (t + t3 - 2 * t2) + b.slope * nf * (t3 - t2) + a.value * (1 + (2 * t3 - 3 * t2)) +
           b.value * (-2 * t3 + 3 * t2);
}

const SKey *step_at(const std::vector<SKey> &keys, float f)
{
    if (keys.empty())
        return nullptr;
    const SKey *k = &keys.front();
    for (const SKey &s : keys)
        if (s.frame <= f)
            k = &s;
    return k;
}

struct Material
{
    std::string name;
    float regs[3][4] = {};   /* TEV registers 0..2, 0..1 (may go past) */
    float konst[4][4] = {};  /* TEV konst colours */
    float color[4] = {1, 1, 1, 1};
    struct Map
    {
        int tex;
        u8 ws, wt;
    };
    std::vector<Map> maps;
    struct Srt
    {
        float tx = 0, ty = 0, rot = 0, sx = 1, sy = 1;
    };
    std::vector<Srt> srts;
    struct Gen
    {
        u8 type, src, mtx;
    };
    std::vector<Gen> gens;
    bool chan = false;
    u8 chan_color = 1, chan_alpha = 1;
    struct Stage
    {
        u8 coord, map;
        u8 c[4], cop, cbias, cscale, cclamp, creg, ksel;
        u8 a[4], aop, abias, ascale, aclamp, areg, kasel;
    };
    std::vector<Stage> stages;
    u8 ac_func = 0x66, ac_op = 0, ac_ref0 = 0, ac_ref1 = 0;
    u8 bl_type = 1, bl_src = 4, bl_dst = 5;
    Anim anim;
};

struct Pane
{
    enum Kind
    {
        Plain,
        Picture,
        Window,
        Text,
    } kind = Plain;
    std::string name;
    u8 flags = 1, origin = 4, alpha = 255;
    bool hide = false;
    float t[3] = {}, r[3] = {}, s[2] = {1, 1}, w = 0, h = 0;
    float vc[4][4] = {};
    int mat = -1;
    std::vector<std::array<float, 8>> tc;
    std::vector<Pane> children;
    Anim anim;
    bool visible() const { return (flags & 1) != 0 && !hide; }
    bool influenced() const { return (flags & 2) == 0; }
    bool position_adjust() const { return (flags & 4) == 0; }
};

struct Layout
{
    float width = 0, height = 0;
    std::vector<std::string> tex_names;
    std::vector<Tex> textures;
    std::vector<Material> mats;
    std::vector<Pane> panes;
    std::map<std::string, std::vector<std::string>> lang_groups; /* RootGroup's children */
    float start_len = 0, loop_len = 0;
    std::vector<std::string> pattern_names[2]; /* RLTP's texture names, per key set */
};

void read_pane(const Reader &r, std::size_t at, Pane &p)
{
    p.flags = r.u8_(at);
    p.origin = r.u8_(at + 1);
    p.alpha = r.u8_(at + 2);
    p.name = r.str(at + 4, 16);
    for (int i = 0; i < 3; ++i)
        p.t[i] = r.f32(at + 0x1C + i * 4);
    for (int i = 0; i < 3; ++i)
        p.r[i] = r.f32(at + 0x28 + i * 4);
    p.s[0] = r.f32(at + 0x34);
    p.s[1] = r.f32(at + 0x38);
    p.w = r.f32(at + 0x3C);
    p.h = r.f32(at + 0x40);
}

void read_quad(const Reader &r, std::size_t at, Pane &p)
{
    for (int v = 0; v < 4; ++v)
        for (int k = 0; k < 4; ++k)
            p.vc[v][k] = float(r.u8_(at + v * 4 + k));
    p.mat = r.u16_(at + 16);
    const u8 n = r.u8_(at + 18);
    for (u8 i = 0; i < n && i < 8; ++i)
    {
        std::array<float, 8> c;
        for (int k = 0; k < 8; ++k)
            c[std::size_t(k)] = r.f32(at + 20 + std::size_t(i) * 32 + std::size_t(k) * 4);
        p.tc.push_back(c);
    }
}

void read_material(const Reader &r, std::size_t at, Material &m)
{
    m.name = r.str(at, 20);
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 4; ++k)
            m.regs[i][k] = float(s16(r.u16_(at + 20 + std::size_t(i * 4 + k) * 2))) / 255.0f;
    for (int i = 0; i < 4; ++i)
        for (int k = 0; k < 4; ++k)
            m.konst[i][k] = float(r.u8_(at + 44 + std::size_t(i * 4 + k))) / 255.0f;
    const u32 f = r.u32_(at + 60);
    const u32 n_map = f & 15, n_srt = (f >> 4) & 15, n_gen = (f >> 8) & 15, swap = (f >> 12) & 1,
              n_isrt = (f >> 13) & 3, n_istage = (f >> 15) & 7, n_stage = (f >> 18) & 31, acmp = (f >> 23) & 1,
              blend = (f >> 24) & 1, chan = (f >> 25) & 1, mcol = (f >> 27) & 1;
    std::size_t p = at + 64;
    for (u32 i = 0; i < n_map; ++i, p += 4)
        m.maps.push_back({r.u16_(p), r.u8_(p + 2), r.u8_(p + 3)});
    for (u32 i = 0; i < n_srt; ++i, p += 20)
    {
        Material::Srt s;
        s.tx = r.f32(p);
        s.ty = r.f32(p + 4);
        s.rot = r.f32(p + 8);
        s.sx = r.f32(p + 12);
        s.sy = r.f32(p + 16);
        m.srts.push_back(s);
    }
    for (u32 i = 0; i < n_gen; ++i, p += 4)
        m.gens.push_back({r.u8_(p), r.u8_(p + 1), r.u8_(p + 2)});
    if (chan)
    {
        m.chan = true;
        m.chan_color = r.u8_(p);
        m.chan_alpha = r.u8_(p + 1);
        p += 4;
    }
    if (mcol)
    {
        for (int k = 0; k < 4; ++k)
            m.color[k] = float(r.u8_(p + std::size_t(k))) / 255.0f;
        p += 4;
    }
    if (swap)
        p += 4;
    p += std::size_t(n_isrt) * 20 + std::size_t(n_istage) * 4;
    for (u32 i = 0; i < n_stage; ++i, p += 16)
    {
        Material::Stage s{};
        s.coord = r.u8_(p);
        const u8 b2 = r.u8_(p + 2), b3 = r.u8_(p + 3);
        const u16 map = u16(b2 | (b3 & 1) << 8);
        s.map = map >= 0xFF ? 0xFF : u8(map);
        const u8 c0 = r.u8_(p + 4), c1 = r.u8_(p + 5), c2 = r.u8_(p + 6), c3 = r.u8_(p + 7);
        s.c[0] = c0 & 15;
        s.c[1] = c0 >> 4;
        s.c[2] = c1 & 15;
        s.c[3] = c1 >> 4;
        s.cop = c2 & 15;
        s.cbias = (c2 >> 4) & 3;
        s.cscale = c2 >> 6;
        s.cclamp = c3 & 1;
        s.creg = (c3 >> 1) & 3;
        s.ksel = c3 >> 3;
        const u8 a0 = r.u8_(p + 8), a1 = r.u8_(p + 9), a2 = r.u8_(p + 10), a3 = r.u8_(p + 11);
        s.a[0] = a0 & 15;
        s.a[1] = a0 >> 4;
        s.a[2] = a1 & 15;
        s.a[3] = a1 >> 4;
        s.aop = a2 & 15;
        s.abias = (a2 >> 4) & 3;
        s.ascale = a2 >> 6;
        s.aclamp = a3 & 1;
        s.areg = (a3 >> 1) & 3;
        s.kasel = a3 >> 3;
        m.stages.push_back(s);
    }
    if (m.stages.empty())
    {
        /* No stages: the texture between the two registers, then the
         * vertex colour (the Wii Banner Player's default). */
        Material::Stage a{};
        a.map = 0;
        a.c[0] = 2; a.c[1] = 4; a.c[2] = 8; a.c[3] = 15;
        a.a[0] = 1; a.a[1] = 2; a.a[2] = 4; a.a[3] = 7;
        a.cclamp = a.aclamp = 1;
        Material::Stage b{};
        b.map = 0xFF;
        b.c[0] = 15; b.c[1] = 0; b.c[2] = 10; b.c[3] = 15;
        b.a[0] = 7; b.a[1] = 0; b.a[2] = 5; b.a[3] = 7;
        b.cclamp = b.aclamp = 1;
        m.stages = {a, b};
    }
    if (acmp)
    {
        m.ac_func = r.u8_(p);
        m.ac_op = r.u8_(p + 1);
        m.ac_ref0 = r.u8_(p + 2);
        m.ac_ref1 = r.u8_(p + 3);
        p += 4;
    }
    if (blend)
    {
        m.bl_type = r.u8_(p);
        m.bl_src = r.u8_(p + 1);
        m.bl_dst = r.u8_(p + 2);
    }
}

bool read_layout(const std::vector<u8> &arc_bytes, const std::map<std::string, std::pair<std::size_t, std::size_t>> &files,
                 const std::string &brlyt, Layout &lay)
{
    const auto it = files.find(brlyt);
    if (it == files.end())
        return false;
    Reader r{arc_bytes.data() + it->second.first, std::min(it->second.second, arc_bytes.size() - it->second.first)};
    if (r.u32_(0) != fourcc("RLYT"))
        return false;
    const std::size_t first = r.u16_(12);
    const u16 sections = r.u16_(14);
    std::vector<std::vector<Pane> *> stack = {&lay.panes};
    Pane *last = nullptr;
    int group_depth = 0;
    std::string group_parent, last_group;
    std::size_t at = first;
    for (u16 s = 0; s < sections && r.ok(at, 8); ++s)
    {
        const u32 magic = r.u32_(at), size = r.u32_(at + 4);
        const std::size_t body = at + 8;
        if (magic == fourcc("lyt1"))
        {
            lay.width = r.f32(body + 4);
            lay.height = r.f32(body + 8);
        }
        else if (magic == fourcc("txl1"))
        {
            const u16 n = r.u16_(body);
            const std::size_t table = body + 4;
            for (u16 i = 0; i < n; ++i)
                lay.tex_names.push_back(r.str(table + r.u32_(table + std::size_t(i) * 8), 128));
        }
        else if (magic == fourcc("mat1"))
        {
            const u16 n = r.u16_(body);
            for (u16 i = 0; i < n; ++i)
            {
                Material m;
                read_material(r, at + r.u32_(body + 4 + std::size_t(i) * 4), m);
                lay.mats.push_back(std::move(m));
            }
        }
        else if (magic == fourcc("pan1") || magic == fourcc("bnd1") || magic == fourcc("pic1") ||
                 magic == fourcc("wnd1") || magic == fourcc("txt1"))
        {
            Pane p;
            read_pane(r, body, p);
            if (magic == fourcc("pic1"))
            {
                p.kind = Pane::Picture;
                read_quad(r, body + 0x44, p);
            }
            else if (magic == fourcc("wnd1"))
            {
                /* A window: its content quad (frames left out). */
                p.kind = Pane::Window;
                const u32 content = r.u32_(body + 0x44 + 20);
                read_quad(r, at + content, p);
            }
            else if (magic == fourcc("txt1"))
                p.kind = Pane::Text; /* text needs the console's fonts: not drawn */
            stack.back()->push_back(std::move(p));
            last = &stack.back()->back();
        }
        else if (magic == fourcc("pas1"))
        {
            if (last)
                stack.push_back(&last->children);
        }
        else if (magic == fourcc("pae1"))
        {
            if (stack.size() > 1)
                stack.pop_back();
            last = nullptr;
        }
        else if (magic == fourcc("grp1"))
        {
            const std::string name = r.str(body, 16);
            last_group = name;
            if (group_depth == 1 && group_parent == "RootGroup")
            {
                const u16 n = r.u16_(body + 16);
                auto &list = lay.lang_groups[name];
                for (u16 i = 0; i < n; ++i)
                    list.push_back(r.str(body + 20 + std::size_t(i) * 16, 16));
            }
        }
        else if (magic == fourcc("grs1"))
        {
            if (++group_depth == 1)
                group_parent = last_group;
        }
        else if (magic == fourcc("gre1"))
            --group_depth;
        if (size < 8)
            break;
        at += size;
    }
    /* Textures from arc/timg. */
    for (const std::string &name : lay.tex_names)
    {
        Tex t;
        const auto ti = files.find("arc/timg/" + lower(name));
        if (ti != files.end() && ti->second.first + ti->second.second <= arc_bytes.size())
            decode_tpl(arc_bytes.data() + ti->second.first, ti->second.second, t);
        lay.textures.push_back(std::move(t));
    }
    return true;
}

Pane *find_pane(std::vector<Pane> &panes, const std::string &name)
{
    for (Pane &p : panes)
    {
        if (p.name == name)
            return &p;
        if (Pane *c = find_pane(p.children, name))
            return c;
    }
    return nullptr;
}

/* A brlan's keys into the layout's panes and materials; its length in frames. */
float read_anim(const std::vector<u8> &arc_bytes, const std::pair<std::size_t, std::size_t> &file, Layout &lay, int set)
{
    Reader r{arc_bytes.data() + file.first, std::min(file.second, arc_bytes.size() - file.first)};
    if (r.u32_(0) != fourcc("RLAN"))
        return 0;
    const std::size_t first = r.u16_(12);
    const u16 sections = r.u16_(14);
    float length = 0;
    std::size_t at = first;
    for (u16 s = 0; s < sections && r.ok(at, 8); ++s)
    {
        const u32 magic = r.u32_(at), size = r.u32_(at + 4);
        if (magic == fourcc("pai1"))
        {
            length = float(r.u16_(at + 8));
            const u16 n_files = r.u16_(at + 12), n_anims = r.u16_(at + 14);
            const std::size_t entries = at + r.u32_(at + 16);
            /* RLTP's texture names follow the header. */
            const std::size_t names = at + 20;
            for (u16 i = 0; i < n_files; ++i)
                lay.pattern_names[set].push_back(lower(r.str(names + r.u32_(names + std::size_t(i) * 4), 128)));
            for (u16 a = 0; a < n_anims; ++a)
            {
                const std::size_t ent = at + r.u32_(entries + std::size_t(a) * 4);
                const std::string name = r.str(ent, 20);
                const u8 tags = r.u8_(ent + 20), is_mat = r.u8_(ent + 21);
                Anim *anim = nullptr;
                if (is_mat)
                {
                    for (Material &m : lay.mats)
                        if (m.name == name)
                            anim = &m.anim;
                }
                else if (Pane *p = find_pane(lay.panes, name))
                    anim = &p->anim;
                if (!anim)
                    continue;
                for (u8 t = 0; t < tags; ++t)
                {
                    const std::size_t tag = ent + r.u32_(ent + 24 + std::size_t(t) * 4);
                    const u32 type = r.u32_(tag);
                    const u8 n = r.u8_(tag + 4);
                    for (u8 e = 0; e < n; ++e)
                    {
                        const std::size_t en = tag + r.u32_(tag + 8 + std::size_t(e) * 4);
                        Track tr;
                        tr.type = type;
                        tr.index = r.u8_(en);
                        tr.target = r.u8_(en + 1);
                        const u8 data_type = r.u8_(en + 2);
                        const u16 keys = r.u16_(en + 4);
                        const std::size_t k = en + 12;
                        tr.step = data_type == 1;
                        for (u16 i = 0; i < keys && keys < 4096; ++i)
                        {
                            if (tr.step)
                                tr.s.push_back({r.f32(k + std::size_t(i) * 8), r.u8_(k + std::size_t(i) * 8 + 4),
                                                r.u8_(k + std::size_t(i) * 8 + 5)});
                            else
                                tr.h.push_back({r.f32(k + std::size_t(i) * 12), r.f32(k + std::size_t(i) * 12 + 4),
                                                r.f32(k + std::size_t(i) * 12 + 8)});
                        }
                        std::stable_sort(tr.h.begin(), tr.h.end(), [](const HKey &x, const HKey &y) { return x.frame < y.frame; });
                        std::stable_sort(tr.s.begin(), tr.s.end(), [](const SKey &x, const SKey &y) { return x.frame < y.frame; });
                        if (data_type == 1 || data_type == 2)
                            anim->tracks[set].push_back(std::move(tr));
                    }
                }
            }
        }
        if (size < 8)
            break;
        at += size;
    }
    return length;
}

/* ---- animating ---------------------------------------------------------------------------------- */

void animate_material(Material &m, Layout &lay, float f, int set)
{
    for (const Track &t : m.anim.tracks[set])
    {
        if (t.step)
        {
            if (t.type == kRLTP)
            {
                const SKey *k = step_at(t.s, f);
                if (!k)
                    continue;
                const std::size_t map = t.index < m.maps.size() ? t.index : t.target;
                const auto &names = lay.pattern_names[set];
                const std::size_t pick = std::size_t(k->d1) << 8 | k->d2;
                if (map < m.maps.size() && pick < names.size())
                    for (std::size_t i = 0; i < lay.tex_names.size(); ++i)
                        if (lower(lay.tex_names[i]) == names[pick])
                            m.maps[map].tex = int(i);
            }
            continue;
        }
        const float v = hermite_at(t.h, f);
        if (t.type == kRLTS && t.index < m.srts.size())
        {
            Material::Srt &s = m.srts[t.index];
            float *fields[5] = {&s.tx, &s.ty, &s.rot, &s.sx, &s.sy};
            if (t.target < 5)
                *fields[t.target] = v;
        }
        else if (t.type == kRLMC)
        {
            if (t.target < 4)
                m.color[t.target] = std::clamp(v, 0.0f, 255.0f) / 255.0f;
            else if (t.target < 0x10)
                m.regs[(t.target - 4) / 4][(t.target - 4) % 4] = v / 255.0f;
            else if (t.target < 0x20)
                m.konst[(t.target - 0x10) / 4][(t.target - 0x10) % 4] = std::clamp(v, 0.0f, 255.0f) / 255.0f;
        }
    }
}

void animate_pane(Pane &p, float f, int set)
{
    for (const Track &t : p.anim.tracks[set])
    {
        if (t.step)
        {
            if (t.type == kRLVI)
                if (const SKey *k = step_at(t.s, f))
                    p.flags = u8(k->d2 ? (p.flags | 1) : (p.flags & ~1));
            continue;
        }
        const float v = hermite_at(t.h, f);
        if (t.type == kRLPA && t.target < 10)
        {
            float *fields[10] = {&p.t[0], &p.t[1], &p.t[2], &p.r[0], &p.r[1], &p.r[2], &p.s[0], &p.s[1], &p.w, &p.h};
            *fields[t.target] = v;
        }
        else if (t.type == kRLVC)
        {
            if (t.target == 0x10)
                p.alpha = u8(std::clamp(v, 0.0f, 255.0f));
            else if (t.target < 0x10)
                p.vc[t.target / 4][t.target % 4] = std::clamp(v, 0.0f, 255.0f);
        }
    }
    for (Pane &c : p.children)
        animate_pane(c, f, set);
}

void set_frame(Layout &lay, float frame)
{
    int set = 0;
    if (lay.start_len > 0 && frame >= lay.start_len)
    {
        set = 1;
        frame -= lay.start_len;
    }
    else if (lay.start_len <= 0)
        set = 1;
    for (Pane &p : lay.panes)
        animate_pane(p, frame, set);
    for (Material &m : lay.mats)
        animate_material(m, lay, frame, set);
}

void set_language(Layout &lay, const std::string &lang)
{
    if (lay.lang_groups.empty())
        return;
    const std::string use = lay.lang_groups.count(lang) ? lang : (lay.lang_groups.count("ENG") ? "ENG" : lang);
    for (const auto &g : lay.lang_groups)
        if (g.first.size() == 3 && g.first != use)
            for (const std::string &n : g.second)
                if (Pane *p = find_pane(lay.panes, n))
                    p->hide = true;
    const auto it = lay.lang_groups.find(use);
    if (it != lay.lang_groups.end())
        for (const std::string &n : it->second)
            if (Pane *p = find_pane(lay.panes, n))
                p->hide = false;
}

/* ---- drawing ----------------------------------------------------------------------------------- */

/* A 2D affine transform (an orthographic view of the 3D one). */
struct M
{
    float a[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    M operator*(const M &o) const
    {
        M r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 4; ++j)
            {
                float v = j == 3 ? a[i][3] : 0.0f;
                for (int k = 0; k < 3; ++k)
                    v += a[i][k] * o.a[k][j];
                r.a[i][j] = v;
            }
        return r;
    }
    static M translate(float x, float y, float z)
    {
        M m;
        m.a[0][3] = x;
        m.a[1][3] = y;
        m.a[2][3] = z;
        return m;
    }
    static M scale(float x, float y)
    {
        M m;
        m.a[0][0] = x;
        m.a[1][1] = y;
        return m;
    }
    static M rotate(int axis, float degrees)
    {
        M m;
        const float r = degrees * 3.14159265f / 180.0f, c = std::cos(r), s = std::sin(r);
        const int i = (axis + 1) % 3, j = (axis + 2) % 3;
        m.a[i][i] = c;
        m.a[i][j] = -s;
        m.a[j][i] = s;
        m.a[j][j] = c;
        return m;
    }
    void apply(float x, float y, float &ox, float &oy) const
    {
        ox = a[0][0] * x + a[0][1] * y + a[0][3];
        oy = a[1][0] * x + a[1][1] * y + a[1][3];
    }
};

struct Canvas
{
    int w, h;
    std::vector<float> px; /* RGBA */
};

void sample(const Tex &t, float u, float v, u8 ws, u8 wt, float *out)
{
    if (t.w == 0)
    {
        out[0] = out[1] = out[2] = out[3] = 1;
        return;
    }
    auto wrap = [](float c, int n, u8 mode) {
        if (mode == 1) /* repeat */
        {
            c = std::fmod(c, float(n));
            if (c < 0)
                c += float(n);
        }
        else if (mode == 2) /* mirror */
        {
            float p = std::fmod(c, float(n * 2));
            if (p < 0)
                p += float(n * 2);
            c = p < float(n) ? p : float(n * 2) - p - 0.001f;
        }
        if (!std::isfinite(c))
            c = 0; /* NaN slips through std::clamp; as an index it read far outside the texture */
        return std::clamp(c, 0.0f, float(n) - 1.0f);
    };
    const float x = wrap(u * float(t.w) - 0.5f, t.w, ws), y = wrap(v * float(t.h) - 0.5f, t.h, wt);
    const int x0 = int(x), y0 = int(y);
    const int x1 = std::min(t.w - 1, x0 + 1), y1 = std::min(t.h - 1, y0 + 1);
    const float fx = x - float(x0), fy = y - float(y0);
    const float *p00 = &t.rgba[(std::size_t(y0) * t.w + x0) * 4], *p10 = &t.rgba[(std::size_t(y0) * t.w + x1) * 4];
    const float *p01 = &t.rgba[(std::size_t(y1) * t.w + x0) * 4], *p11 = &t.rgba[(std::size_t(y1) * t.w + x1) * 4];
    for (int k = 0; k < 4; ++k)
        out[k] = (p00[k] * (1 - fx) + p10[k] * fx) * (1 - fy) + (p01[k] * (1 - fx) + p11[k] * fx) * fy;
}

float blend_factor(u8 f, const float *src, const float *dst, int k, bool for_alpha)
{
    switch (f & 7)
    {
    case 0: return 0;
    case 1: return 1;
    case 2: return for_alpha ? src[3] : src[k];        /* source colour */
    case 3: return 1 - (for_alpha ? src[3] : src[k]);  /* one minus it */
    case 4: return src[3];
    case 5: return 1 - src[3];
    case 6: return dst[3];
    default: return 1 - dst[3];
    }
}

bool alpha_test(u8 func, u8 ref, float a)
{
    const float r = float(ref) / 255.0f;
    switch (func & 7)
    {
    case 0: return false;
    case 1: return a < r;
    case 2: return std::fabs(a - r) < 0.5f / 255.0f;
    case 3: return a <= r;
    case 4: return a > r;
    case 5: return std::fabs(a - r) >= 0.5f / 255.0f;
    case 6: return a >= r;
    default: return true;
    }
}

/* One pixel through the material's TEV stages. */
void shade(const Material &m, const Layout &lay, const float *ras, const float uv[8][2], float *out)
{
    float prev[4] = {0, 0, 0, 0};
    float regs[3][4];
    std::memcpy(regs, m.regs, sizeof regs);
    for (const Material::Stage &s : m.stages)
    {
        float tex[4] = {1, 1, 1, 1};
        if (s.map != 0xFF && s.map < m.maps.size())
        {
            const Material::Map &mp = m.maps[s.map];
            const int coord = std::min<int>(s.coord, 7);
            if (mp.tex >= 0 && std::size_t(mp.tex) < lay.textures.size())
                sample(lay.textures[std::size_t(mp.tex)], uv[coord][0], uv[coord][1], mp.ws, mp.wt, tex);
        }
        auto konst = [&](u8 sel, int k, bool alpha) -> float {
            if (sel <= 7)
                return float(8 - sel) / 8.0f;
            if (!alpha && sel >= 12 && sel <= 15)
                return m.konst[sel - 12][k];
            if (sel >= 16 && sel <= 31)
                return m.konst[(sel - 16) % 4][(sel - 16) / 4];
            return 1;
        };
        auto cin = [&](u8 i, int k) -> float {
            switch (i)
            {
            case 0: return prev[k];
            case 1: return prev[3];
            case 2: return regs[0][k];
            case 3: return regs[0][3];
            case 4: return regs[1][k];
            case 5: return regs[1][3];
            case 6: return regs[2][k];
            case 7: return regs[2][3];
            case 8: return tex[k];
            case 9: return tex[3];
            case 10: return ras[k];
            case 11: return ras[3];
            case 12: return 1;
            case 13: return 0.5f;
            case 14: return konst(s.ksel, k, false);
            default: return 0;
            }
        };
        auto ain = [&](u8 i) -> float {
            switch (i)
            {
            case 0: return prev[3];
            case 1: return regs[0][3];
            case 2: return regs[1][3];
            case 3: return regs[2][3];
            case 4: return tex[3];
            case 5: return ras[3];
            case 6: return konst(s.kasel, 3, true);
            default: return 0;
            }
        };
        auto op = [](u8 o, float a, float b, float c, float d, u8 bias, u8 scale, u8 clamp) {
            c = std::clamp(c, 0.0f, 1.0f);
            float v;
            if (o <= 1)
            {
                const float l = a * (1 - c) + b * c;
                v = o == 0 ? d + l : d - l;
                v += bias == 1 ? 0.5f : bias == 2 ? -0.5f : 0.0f;
                v *= scale == 1 ? 2.0f : scale == 2 ? 4.0f : scale == 3 ? 0.5f : 1.0f;
            }
            else /* the comparisons: rarely used in banners */
                v = d + ((o & 1) ? (std::fabs(a - b) < 0.002f ? c : 0.0f) : (a > b ? c : 0.0f));
            return clamp ? std::clamp(v, 0.0f, 1.0f) : std::clamp(v, -4.0f, 4.0f);
        };
        float res[4];
        for (int k = 0; k < 3; ++k)
            res[k] = op(s.cop, cin(s.c[0], k), cin(s.c[1], k), cin(s.c[2], k), cin(s.c[3], k), s.cbias, s.cscale, s.cclamp);
        res[3] = op(s.aop, ain(s.a[0]), ain(s.a[1]), ain(s.a[2]), ain(s.a[3]), s.abias, s.ascale, s.aclamp);
        float *creg = s.creg == 0 ? prev : regs[s.creg - 1];
        float *areg = s.areg == 0 ? prev : regs[s.areg - 1];
        for (int k = 0; k < 3; ++k)
            creg[k] = res[k];
        areg[3] = res[3];
    }
    for (int k = 0; k < 4; ++k)
        out[k] = std::clamp(prev[k], 0.0f, 1.0f);
}

void draw_quad(Canvas &cv, const Layout &lay, const Pane &p, const M &mx, float alpha)
{
    if (p.mat < 0 || std::size_t(p.mat) >= lay.mats.size())
        return;
    const Material &m = lay.mats[std::size_t(p.mat)];
    /* Corners: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right (y up). */
    const float lx[4] = {0, 1, 0, 1}, ly[4] = {1, 1, 0, 0};
    float sx[4], sy[4];
    for (int v = 0; v < 4; ++v)
        mx.apply(lx[v], ly[v], sx[v], sy[v]);
    /* What each corner carries: its colour, then every texture coordinate. */
    float attr[4][4 + 16];
    for (int v = 0; v < 4; ++v)
    {
        float vc[4];
        for (int k = 0; k < 4; ++k)
            vc[k] = p.vc[v][k] / 255.0f;
        if (m.chan && m.chan_color == 0)
            for (int k = 0; k < 3; ++k)
                vc[k] = m.color[k];
        if (m.chan && m.chan_alpha == 0)
            vc[3] = m.color[3];
        vc[3] *= alpha;
        for (int k = 0; k < 4; ++k)
            attr[v][k] = vc[k];
        for (int c = 0; c < 8; ++c)
        {
            const std::array<float, 8> *set = p.tc.empty() ? nullptr : &p.tc[std::min<std::size_t>(c, p.tc.size() - 1)];
            attr[v][4 + c * 2] = set ? (*set)[std::size_t(v) * 2] : lx[v];
            attr[v][4 + c * 2 + 1] = set ? (*set)[std::size_t(v) * 2 + 1] : 1 - ly[v];
        }
    }
    /* The texture coordinate generators: which set, through which SRT. */
    struct Gen
    {
        int src;
        bool srt;
        float m00, m01, m02, m10, m11, m12;
    } gens[8];
    const std::size_t n_gens = std::max<std::size_t>(1, std::min<std::size_t>(8, m.gens.size()));
    for (std::size_t i = 0; i < n_gens; ++i)
    {
        Gen &g = gens[i];
        g.src = 0;
        g.srt = false;
        if (i < m.gens.size())
        {
            g.src = std::clamp(int(m.gens[i].src) - 4, 0, 7);
            const int which = (int(m.gens[i].mtx) - 30) / 3;
            if (m.gens[i].mtx >= 30 && which >= 0 && std::size_t(which) < m.srts.size())
            {
                /* uv' = R(rot) * (S * uv + t - S/2) + 1/2, about the texture's middle. */
                const Material::Srt &s = m.srts[std::size_t(which)];
                const float r = s.rot * 3.14159265f / 180.0f, c = std::cos(r), sn = std::sin(r);
                g.srt = true;
                g.m00 = c * s.sx;
                g.m01 = -sn * s.sy;
                g.m10 = sn * s.sx;
                g.m11 = c * s.sy;
                const float ox = s.tx - 0.5f * s.sx, oy = s.ty - 0.5f * s.sy;
                g.m02 = c * ox - sn * oy + 0.5f;
                g.m12 = sn * ox + c * oy + 0.5f;
            }
        }
    }
    auto triangle = [&](int a, int b, int c) {
        const float area = (sx[b] - sx[a]) * (sy[c] - sy[a]) - (sx[c] - sx[a]) * (sy[b] - sy[a]);
        if (!std::isfinite(area) || std::fabs(area) < 1e-6f)
            return; /* a corner off at infinity (a damaged animation): nothing to draw */
        for (int k : {a, b, c})
            if (!std::isfinite(sx[k]) || !std::isfinite(sy[k]) || std::fabs(sx[k]) > 1e6f || std::fabs(sy[k]) > 1e6f)
                return;
        const int x0 = std::max(0, int(std::floor(std::min({sx[a], sx[b], sx[c]}))));
        const int x1 = std::min(cv.w - 1, int(std::ceil(std::max({sx[a], sx[b], sx[c]}))));
        const int y0 = std::max(0, int(std::floor(std::min({sy[a], sy[b], sy[c]}))));
        const int y1 = std::min(cv.h - 1, int(std::ceil(std::max({sy[a], sy[b], sy[c]}))));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
            {
                const float px = float(x) + 0.5f, py = float(y) + 0.5f;
                const float w0 = ((sx[b] - px) * (sy[c] - py) - (sx[c] - px) * (sy[b] - py)) / area;
                const float w1 = ((sx[c] - px) * (sy[a] - py) - (sx[a] - px) * (sy[c] - py)) / area;
                const float w2 = 1 - w0 - w1;
                if (w0 < -1e-4f || w1 < -1e-4f || w2 < -1e-4f)
                    continue;
                float at[4 + 16];
                for (int k = 0; k < 20; ++k)
                    at[k] = attr[a][k] * w0 + attr[b][k] * w1 + attr[c][k] * w2;
                float uv[8][2];
                for (std::size_t i = 0; i < 8; ++i)
                {
                    const Gen &g = gens[std::min(i, n_gens - 1)];
                    const float u = at[4 + g.src * 2], v = at[4 + g.src * 2 + 1];
                    uv[i][0] = g.srt ? g.m00 * u + g.m01 * v + g.m02 : u;
                    uv[i][1] = g.srt ? g.m10 * u + g.m11 * v + g.m12 : v;
                }
                float src[4];
                shade(m, lay, at, uv, src);
                const bool t0 = alpha_test(m.ac_func & 15, m.ac_ref0, src[3]);
                const bool t1 = alpha_test(m.ac_func >> 4, m.ac_ref1, src[3]);
                const bool pass = m.ac_op == 0 ? t0 && t1 : m.ac_op == 1 ? t0 || t1 : m.ac_op == 2 ? t0 != t1 : t0 == t1;
                if (!pass)
                    continue;
                float *dst = &cv.px[(std::size_t(y) * cv.w + x) * 4];
                float out[4];
                if (m.bl_type == 0)
                    std::memcpy(out, src, sizeof out);
                else if (m.bl_type == 3)
                    for (int k = 0; k < 4; ++k)
                        out[k] = std::max(0.0f, dst[k] - src[k]);
                else
                {
                    for (int k = 0; k < 3; ++k)
                        out[k] = src[k] * blend_factor(m.bl_src, src, dst, k, false) +
                                 dst[k] * blend_factor(m.bl_dst, src, dst, k, false);
                    out[3] = src[3] + dst[3] * (1 - src[3]);
                }
                for (int k = 0; k < 4; ++k)
                    dst[k] = std::clamp(out[k], 0.0f, 1.0f);
            }
    };
    triangle(0, 1, 3);
    triangle(0, 3, 2);
}

void draw_pane(Canvas &cv, const Layout &lay, const Pane &p, const M &parent, float parent_alpha, float adj_x, float adj_y)
{
    if (!p.visible())
        return;
    const float own = float(p.alpha) / 255.0f;
    const float alpha = p.influenced() ? parent_alpha * own : own;
    M mx = parent * M::translate(p.t[0] * adj_x, p.t[1] * adj_y, p.t[2]);
    if (!p.position_adjust())
        adj_x = adj_y = 1;
    mx = mx * M::rotate(0, p.r[0]) * M::rotate(1, p.r[1]) * M::rotate(2, p.r[2]) * M::scale(p.s[0], p.s[1]);
    if (p.kind == Pane::Picture || p.kind == Pane::Window)
    {
        const float ox = float(p.origin % 3), oy = float(2 - p.origin / 3);
        const M q = mx * M::scale(p.w * adj_x, p.h * adj_y) * M::translate(-0.5f * ox, -0.5f * oy, 0);
        draw_quad(cv, lay, p, q, alpha);
    }
    for (const Pane &c : p.children)
        draw_pane(cv, lay, c, mx, alpha, adj_x, adj_y);
}

/* The layout as a 16:9 picture (a widescreen Wii stretches it so). */
void draw_layout(Canvas &cv, const Layout &lay)
{
    std::fill(cv.px.begin(), cv.px.end(), 0.0f);
    const float adj_x = 4.0f / 3.0f, adj_y = 1.0f;
    /* Layout units (centred, y up) to pixels. */
    M view = M::translate(float(cv.w) * 0.5f, float(cv.h) * 0.5f, 0) *
             M::scale(float(cv.w) / (lay.width * adj_x), -float(cv.h) / (lay.height * adj_y));
    for (const Pane &p : lay.panes)
        draw_pane(cv, lay, p, view, 1.0f, adj_x, adj_y);
}

bool load_layout(const std::vector<u8> &bnr, bool icon, Layout &lay, std::string &error)
{
    const std::string what = icon ? "icon" : "banner";
    std::vector<u8> arc;
    if (!inner_file(bnr, "meta/" + what + ".bin", arc))
    {
        error = "no meta/" + what + ".bin";
        return false;
    }
    std::map<std::string, std::pair<std::size_t, std::size_t>> files;
    if (!read_u8(arc, 0, files))
    {
        error = what + ".bin is not an archive";
        return false;
    }
    if (!read_layout(arc, files, "arc/blyt/" + what + ".brlyt", lay))
    {
        error = "no " + what + ".brlyt";
        return false;
    }
    lay.width = icon ? 128.0f : 608.0f; /* as the console shows them */
    lay.height = icon ? 96.0f : 456.0f;
    const auto start = files.find("arc/anim/" + what + "_start.brlan");
    if (start != files.end())
    {
        lay.start_len = read_anim(arc, start->second, lay, 0);
        const auto loop = files.find("arc/anim/" + what + "_loop.brlan");
        if (loop != files.end())
            lay.loop_len = read_anim(arc, loop->second, lay, 1);
    }
    else
    {
        const auto loop = files.find("arc/anim/" + what + ".brlan");
        if (loop != files.end())
            lay.loop_len = read_anim(arc, loop->second, lay, 1);
    }
    /* The textures a texture-pattern key swaps in (frames of a flipbook) need
     * not be in the layout's own list. */
    for (int set = 0; set < 2; ++set)
        for (const std::string &name : lay.pattern_names[set])
        {
            bool have = false;
            for (const std::string &n : lay.tex_names)
                have |= lower(n) == name;
            if (have)
                continue;
            Tex t;
            const auto ti = files.find("arc/timg/" + name);
            if (ti != files.end() && ti->second.first + ti->second.second <= arc.size())
                decode_tpl(arc.data() + ti->second.first, ti->second.second, t);
            lay.tex_names.push_back(name);
            lay.textures.push_back(std::move(t));
        }
    return true;
}
} // namespace

struct Player::Impl
{
    Layout lay;
    bool intro_applied = false;
    std::vector<float> px;
};

Player::Player() : impl_(new Impl) {}
Player::~Player() = default;

bool Player::open(const std::vector<std::uint8_t> &bnr, bool icon, const std::string &lang, std::string &error)
{
    impl_ = std::make_unique<Impl>();
    if (!load_layout(bnr, icon, impl_->lay, error))
        return false;
    set_language(impl_->lay, lang);
    return true;
}

float Player::intro_frames() const
{
    return impl_->lay.start_len;
}

float Player::loop_frames() const
{
    return impl_->lay.loop_len;
}

void Player::render(double frame, int w, int h, std::uint8_t *rgba)
{
    Layout &lay = impl_->lay;
    const double intro = lay.start_len, loop = lay.loop_len;
    double f = std::max(0.0, frame);
    if (f >= intro)
    {
        if (intro > 0 && !impl_->intro_applied)
        {
            /* What the intro leaves behind stays, under the loop's keys. */
            set_frame(lay, float(intro) - 0.001f);
            impl_->intro_applied = true;
        }
        f = loop > 0 ? intro + std::fmod(f - intro, loop) : intro;
    }
    set_frame(lay, float(f));
    Canvas cv{w, h, std::move(impl_->px)};
    cv.px.resize(std::size_t(w) * h * 4);
    draw_layout(cv, lay);
    for (std::size_t i = 0; i < cv.px.size(); ++i)
        rgba[i] = std::uint8_t(std::lround(cv.px[i] * 255.0f));
    impl_->px = std::move(cv.px);
}

/* ---- the jingle --------------------------------------------------------------------------------- */

namespace
{
std::vector<std::int16_t> to_48k_stereo(const std::vector<std::int16_t> &in, int channels, int rate)
{
    std::vector<std::int16_t> out;
    /* The rate comes from the file: a damaged one (1 Hz) would ask for a
     * buffer tens of thousands of times the jingle. Real ones are 8..48 kHz. */
    if (channels < 1 || rate < 4000 || rate > 96000 || in.empty())
        return out;
    const std::size_t frames = in.size() / std::size_t(channels);
    if (frames == 0)
        return out;
    const std::size_t n = std::min<std::size_t>(std::size_t(double(frames) * 48000.0 / double(rate)), 48000u * 120);
    out.resize(n * 2);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double pos = double(i) * double(rate) / 48000.0;
        const std::size_t a = std::min(frames - 1, std::size_t(pos)), b = std::min(frames - 1, a + 1);
        const double f = pos - double(a);
        for (int c = 0; c < 2; ++c)
        {
            const int ch = std::min(c, channels - 1);
            const double v = double(in[a * channels + ch]) * (1 - f) + double(in[b * channels + ch]) * f;
            out[i * 2 + c] = std::int16_t(std::clamp(v, -32768.0, 32767.0));
        }
    }
    return out;
}

void dsp_decode(const u8 *adpcm, std::size_t start_byte, std::size_t end_byte, const s16 coefs[16], std::int16_t *pcm,
                std::size_t stride, std::size_t max_samples)
{
    int yn1 = 0, yn2 = 0;
    std::size_t out = 0;
    for (std::size_t frame = start_byte; frame + 8 <= end_byte && out < max_samples; frame += 8)
    {
        const u8 ps = adpcm[frame];
        const int scale = 1 << (ps & 15), ci = (ps >> 4) & 7;
        const int c1 = coefs[ci * 2], c2 = coefs[ci * 2 + 1];
        for (int i = 0; i < 14 && out < max_samples; ++i)
        {
            const u8 b = adpcm[frame + 1 + i / 2];
            int nib = (i & 1) ? b & 15 : b >> 4;
            if (nib >= 8)
                nib -= 16;
            int s = scale * nib + ((0x400 + c1 * yn1 + c2 * yn2) >> 11);
            s = std::clamp(s, -32768, 32767);
            yn2 = yn1;
            yn1 = s;
            pcm[out++ * stride] = std::int16_t(s);
        }
    }
}
} // namespace

bool sound(const std::vector<std::uint8_t> &bnr, std::vector<std::int16_t> &pcm, std::string &error)
{
    std::vector<u8> bin;
    if (!inner_file(bnr, "meta/sound.bin", bin))
    {
        error = "no meta/sound.bin";
        return false;
    }
    Reader r{bin.data(), bin.size()};
    if (r.u32_(0) == fourcc("BNS "))
    {
        const std::size_t info = r.u32_(16), data = r.u32_(24);
        const u8 channels = r.u8_(info + 10);
        const int rate = r.u16_(info + 12);
        const u32 samples = r.u32_(info + 20);
        if (channels < 1 || channels > 2 || rate <= 0 || samples == 0 || samples > 48000u * 120)
        {
            error = "an unusual BNS";
            return false;
        }
        std::size_t p = info + 24 + 24;
        s16 coefs[2][16] = {};
        u32 right_start = 0;
        if (channels == 1)
            for (int i = 0; i < 16; ++i)
                coefs[0][i] = s16(r.u16_(p + std::size_t(i) * 2));
        else
        {
            p += 4;
            right_start = r.u32_(p);
            p += 4 + 8;
            for (int i = 0; i < 16; ++i)
                coefs[0][i] = s16(r.u16_(p + std::size_t(i) * 2));
            p += 32 + 16;
            for (int i = 0; i < 16; ++i)
                coefs[1][i] = s16(r.u16_(p + std::size_t(i) * 2));
        }
        const std::size_t adpcm = data + 8, size = r.u32_(data + 4) >= 8 ? r.u32_(data + 4) - 8 : 0;
        if (!r.ok(adpcm, size))
        {
            error = "a short BNS";
            return false;
        }
        std::vector<std::int16_t> raw(std::size_t(samples) * channels);
        if (channels == 1)
            dsp_decode(bin.data() + adpcm, 0, size, coefs[0], raw.data(), 1, samples);
        else
        {
            dsp_decode(bin.data() + adpcm, 0, std::min<std::size_t>(right_start, size), coefs[0], raw.data(), 2, samples);
            dsp_decode(bin.data() + adpcm, std::min<std::size_t>(right_start, size), size, coefs[1], raw.data() + 1, 2,
                       samples);
        }
        pcm = to_48k_stereo(raw, channels, rate);
        return !pcm.empty();
    }
    if (std::memcmp(bin.data(), "RIFF", 4) == 0 && bin.size() > 44)
    {
        /* A plain 16-bit WAV. */
        std::size_t at = 12;
        int channels = 0, rate = 0, bits = 0;
        while (at + 8 <= bin.size())
        {
            const u32 n = u32(bin[at + 4]) | u32(bin[at + 5]) << 8 | u32(bin[at + 6]) << 16 | u32(bin[at + 7]) << 24;
            if (std::memcmp(&bin[at], "fmt ", 4) == 0 && at + 24 <= bin.size())
            {
                channels = bin[at + 10] | bin[at + 11] << 8;
                rate = int(bin[at + 12] | bin[at + 13] << 8 | bin[at + 14] << 16 | u32(bin[at + 15]) << 24);
                bits = bin[at + 22] | bin[at + 23] << 8;
            }
            else if (std::memcmp(&bin[at], "data", 4) == 0 && bits == 16)
            {
                const std::size_t m = std::min<std::size_t>(n, bin.size() - at - 8) / 2;
                std::vector<std::int16_t> raw(m);
                for (std::size_t i = 0; i < m; ++i)
                    raw[i] = std::int16_t(bin[at + 8 + i * 2] | bin[at + 9 + i * 2] << 8);
                pcm = to_48k_stereo(raw, channels, rate);
                return !pcm.empty();
            }
            at += 8 + n + (n & 1);
        }
    }
    if (std::memcmp(bin.data(), "FORM", 4) == 0 && bin.size() > 12)
    {
        /* AIFF: big-endian 16-bit. */
        std::size_t at = 12;
        int channels = 0, bits = 0;
        double rate = 0;
        while (at + 8 <= bin.size())
        {
            const u32 n = r.u32_(at + 4);
            if (std::memcmp(&bin[at], "COMM", 4) == 0)
            {
                channels = r.u16_(at + 8);
                bits = r.u16_(at + 14);
                /* 80-bit float sample rate */
                const int exp = (r.u16_(at + 16) & 0x7FFF) - 16383;
                const double mant = double(r.u32_(at + 18)) * 4294967296.0 + double(r.u32_(at + 22));
                rate = std::ldexp(mant, exp - 63);
            }
            else if (std::memcmp(&bin[at], "SSND", 4) == 0 && bits == 16)
            {
                const std::size_t start = at + 16, m = (n >= 8 ? n - 8 : 0) / 2;
                std::vector<std::int16_t> raw(std::min<std::size_t>(m, (bin.size() - start) / 2));
                for (std::size_t i = 0; i < raw.size(); ++i)
                    raw[i] = std::int16_t(r.u16_(start + i * 2));
                pcm = to_48k_stereo(raw, channels, int(rate));
                return !pcm.empty();
            }
            at += 8 + n + (n & 1);
        }
    }
    error = "sound.bin in a format not read here";
    return false;
}

/* ---- the disc, and the cache ------------------------------------------------------------------- */

namespace
{
bool read_whole(const std::string &path, std::vector<u8> &out)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    bool ok = n > 0 && n < (64L << 20);
    if (ok)
    {
        out.resize(std::size_t(n));
        ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    }
    std::fclose(f);
    return ok;
}

bool write_whole(const std::string &path, const void *data, std::size_t n)
{
    const std::string part = path + ".part";
    std::FILE *f = std::fopen(part.c_str(), "wb");
    if (!f)
        return false;
    const bool ok = n == 0 || std::fwrite(data, 1, n, f) == n;
    std::fclose(f);
    return ok && std::rename(part.c_str(), path.c_str()) == 0;
}
} // namespace

bool load(const std::string &image, const std::string &id, const std::string &cache_dir, std::vector<std::uint8_t> &bnr,
          std::string &error)
{
    const std::string cached = cache_dir + "/" + id + ".bnr", none = cache_dir + "/" + id + ".none";
    if (read_whole(cached, bnr))
        return true;
    struct stat st;
    if (stat(none.c_str(), &st) == 0)
    {
        error = "no banner on this disc";
        return false;
    }
    if (!disc::read_file(image, "opening.bnr", bnr, error))
    {
        mkdir(cache_dir.c_str(), 0777);
        write_whole(none, error.data(), error.size());
        return false;
    }
    mkdir(cache_dir.c_str(), 0777);
    write_whole(cached, bnr.data(), bnr.size());
    return true;
}

/* ---- the workers ---------------------------------------------------------------------------------- */

namespace
{
using Clock = std::chrono::steady_clock;

struct Live
{
    std::string id, image;
    Kind kind = Kind::Icon;
    int w = 160, h = 90;
    double fps = 15;
    Clock::time_point born, wanted, next;
    std::unique_ptr<Player> player;
    std::vector<u8> bnr; /* a banner keeps it, to start again */
    bool busy = false, failed = false, restart = false;
    std::vector<u8> front;
    std::uint64_t serial = 0;
    std::vector<std::int16_t> pcm;
    bool pcm_ready = false;
};

struct Workers
{
    std::mutex lock;
    std::condition_variable wake;
    std::map<std::string, std::unique_ptr<Live>> lives;
    std::string cache_dir = "/data/porpoise/banners", lang = "ENG";
    bool paused = false;
    int threads = 0;
};
Workers &workers()
{
    static Workers *w = new Workers; /* lives as long as the title */
    return *w;
}

std::string key_of(const std::string &id, Kind kind)
{
    return id + (kind == Kind::Icon ? "/icon" : "/banner");
}

void *work(void *)
{
    Workers &w = workers();
    std::unique_lock<std::mutex> g(w.lock);
    std::vector<u8> pixels;
    while (true)
    {
        const auto now = Clock::now();
        /* What is no longer on screen stops. */
        for (auto it = w.lives.begin(); it != w.lives.end();)
            if (!it->second->busy && now - it->second->wanted > std::chrono::milliseconds(1500))
                it = w.lives.erase(it);
            else
                ++it;
        Live *pick = nullptr;
        if (!w.paused)
            for (auto &kv : w.lives)
            {
                Live *l = kv.second.get();
                if (l->busy || l->failed)
                    continue;
                if (!pick || l->next < pick->next)
                    pick = l;
            }
        if (!pick)
        {
            w.wake.wait_for(g, std::chrono::milliseconds(200));
            continue;
        }
        if (pick->next > now)
        {
            w.wake.wait_until(g, pick->next);
            continue;
        }
        pick->busy = true;
        const std::string cache_dir = w.cache_dir, lang = w.lang;
        g.unlock();

        bool failed = false;
        std::vector<std::int16_t> pcm;
        bool got_pcm = false;
        if (!pick->player || pick->restart)
        {
            std::string error;
            std::vector<u8> bnr = std::move(pick->bnr);
            if (bnr.empty() && !load(pick->image, pick->id, cache_dir, bnr, error))
                failed = true;
            else
            {
                auto player = std::make_unique<Player>();
                if (!player->open(bnr, pick->kind == Kind::Icon, lang, error))
                    failed = true;
                else
                {
                    if (pick->kind == Kind::Banner && !pick->pcm_ready && sound(bnr, pcm, error))
                        got_pcm = true;
                    pick->player = std::move(player);
                }
                if (pick->kind == Kind::Banner)
                    pick->bnr = std::move(bnr);
            }
        }
        if (!failed && pick->player)
        {
            const double frame = std::chrono::duration<double>(Clock::now() - pick->born).count() * 60.0;
            pixels.resize(std::size_t(pick->w) * pick->h * 4);
            pick->player->render(frame, pick->w, pick->h, pixels.data());
        }

        g.lock();
        pick->busy = false;
        pick->restart = false;
        pick->failed = failed;
        if (got_pcm)
        {
            pick->pcm = std::move(pcm);
            pick->pcm_ready = true;
        }
        if (!failed && pick->player)
        {
            pick->front.swap(pixels);
            ++pick->serial;
        }
        pick->next = Clock::now() + std::chrono::microseconds(std::int64_t(1e6 / pick->fps));
    }
}

void start_workers_locked(Workers &w)
{
    while (w.threads < 2)
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 1u << 20);
        pthread_t t;
        const bool ok = pthread_create(&t, &attr, work, nullptr) == 0;
        pthread_attr_destroy(&attr);
        if (!ok)
            break;
        pthread_detach(t);
        ++w.threads;
    }
}
} // namespace

void set_cache_dir(const std::string &dir)
{
    std::lock_guard<std::mutex> g(workers().lock);
    workers().cache_dir = dir;
}

void set_language(const std::string &lang)
{
    Workers &w = workers();
    std::lock_guard<std::mutex> g(w.lock);
    if (w.lang == lang)
        return;
    w.lang = lang;
    for (auto &kv : w.lives)
        kv.second->restart = true; /* its words in the new language */
}

void want(const std::string &id, const std::string &image, Kind kind, bool restart)
{
    Workers &w = workers();
    std::lock_guard<std::mutex> g(w.lock);
    auto &slot = w.lives[key_of(id, kind)];
    const auto now = Clock::now();
    if (!slot)
    {
        slot = std::make_unique<Live>();
        slot->id = id;
        slot->image = image;
        slot->kind = kind;
        if (kind == Kind::Banner)
        {
            slot->w = 448;
            slot->h = 252;
            slot->fps = 20;
        }
        slot->born = slot->next = now;
        w.wake.notify_one();
    }
    else if (restart)
    {
        slot->born = now;
        slot->restart = slot->player != nullptr;
    }
    slot->wanted = now;
    start_workers_locked(w);
}

bool frame(const std::string &id, Kind kind, std::vector<std::uint8_t> &rgba, int &w_out, int &h_out,
           std::uint64_t &serial)
{
    Workers &w = workers();
    std::lock_guard<std::mutex> g(w.lock);
    const auto it = w.lives.find(key_of(id, kind));
    if (it == w.lives.end() || it->second->serial == serial || it->second->front.empty())
        return false;
    rgba = it->second->front;
    w_out = it->second->w;
    h_out = it->second->h;
    serial = it->second->serial;
    return true;
}

bool failed(const std::string &id, Kind kind)
{
    Workers &w = workers();
    std::lock_guard<std::mutex> g(w.lock);
    const auto it = w.lives.find(key_of(id, kind));
    return it != w.lives.end() && it->second->failed;
}

bool take_jingle(const std::string &id, std::vector<std::int16_t> &pcm)
{
    Workers &w = workers();
    std::lock_guard<std::mutex> g(w.lock);
    const auto it = w.lives.find(key_of(id, Kind::Banner));
    if (it == w.lives.end() || !it->second->pcm_ready || it->second->pcm.empty())
        return false;
    pcm = std::move(it->second->pcm);
    it->second->pcm.clear();
    return true;
}

bool known_none(const std::string &id)
{
    std::string dir;
    {
        std::lock_guard<std::mutex> g(workers().lock);
        dir = workers().cache_dir;
    }
    struct stat st;
    return stat((dir + "/" + id + ".none").c_str(), &st) == 0;
}

void pause(bool paused)
{
    Workers &w = workers();
    std::lock_guard<std::mutex> g(w.lock);
    w.paused = paused;
    if (paused) /* nothing on screen while a game runs (one being drawn goes when it's done) */
    {
        for (auto it = w.lives.begin(); it != w.lives.end();)
        {
            if (it->second->busy)
            {
                it->second->wanted = Clock::time_point{};
                ++it;
            }
            else
                it = w.lives.erase(it);
        }
    }
    w.wake.notify_all();
}
} // namespace porpoise::banner
