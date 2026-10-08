/* Porpoise - home screen forwarders (porpoise_forwarders.hpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The folder, as the console wants a homebrew title:
 *   eboot.bin                the forwarder (assets/forwarder/eboot.bin)
 *   home-launcher.elf        what it hands the jailbreak's ELF loader to open
 *                            Porpoise (an app can't start another itself)
 *   sce_module/libc.prx      Porpoise's own runtime, which it needs
 *   sce_sys/param.json       its title ID (PPSA98001...), name and version
 *   sce_sys/icon0.png        512x512, the tile
 *   sce_sys/pic0.dds, pic1   3840x2160 BC7, the background behind it
 *   forward.txt              the game, and "exit-after-game"
 * Title IDs come from PPSA98001 up, one per game (a game made again keeps
 * its own), listed in <data>/home-art/forwarders.txt. */
#include "porpoise_forwarders.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <vector>

#include "porpoise_atomic.hpp"
#include "porpoise_image.hpp"
#include "porpoise_paths.hpp"
#include "trace.hpp"

namespace porpoise::forwarders
{
namespace
{
constexpr const char *kHomebrew = "/data/homebrew";
constexpr int kFirst = 98001, kLast = 98999;

struct Picture
{
    int w = 0, h = 0;
    std::vector<std::uint8_t> px; /* RGBA */
    std::uint8_t *at(int x, int y) { return &px[(std::size_t(y) * w + x) * 4]; }
    const std::uint8_t *at(int x, int y) const { return &px[(std::size_t(y) * w + x) * 4]; }
};

bool exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string first_of(const std::string &base)
{
    for (const char *ext : {".png", ".jpg", ".jpeg", ".PNG", ".JPG"})
        if (exists(base + ext))
            return base + ext;
    return "";
}

bool load(const std::string &path, Picture &p)
{
    return !path.empty() && porpoise::image::load_rgba(path, p.px, p.w, p.h) && p.w > 0 && p.h > 0;
}

/* Scaled to w x h: each output pixel the average of the source pixels under
 * it (smooth when shrinking), or the nearest four mixed (when growing). */
Picture scaled(const Picture &src, int w, int h)
{
    Picture out;
    out.w = w;
    out.h = h;
    out.px.assign(std::size_t(w) * h * 4, 0);
    const float sx = float(src.w) / float(w), sy = float(src.h) / float(h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            std::uint8_t *o = out.at(x, y);
            if (sx > 1.0f || sy > 1.0f)
            {
                const int x0 = int(x * sx), x1 = std::max(x0 + 1, std::min(src.w, int((x + 1) * sx)));
                const int y0 = int(y * sy), y1 = std::max(y0 + 1, std::min(src.h, int((y + 1) * sy)));
                const int step_x = std::max(1, (x1 - x0) / 4), step_y = std::max(1, (y1 - y0) / 4);
                unsigned sum[4] = {0, 0, 0, 0}, n = 0;
                for (int yy = y0; yy < y1; yy += step_y)
                    for (int xx = x0; xx < x1; xx += step_x)
                    {
                        const std::uint8_t *s = src.at(std::min(xx, src.w - 1), std::min(yy, src.h - 1));
                        for (int c = 0; c < 4; ++c)
                            sum[c] += s[c];
                        ++n;
                    }
                for (int c = 0; c < 4; ++c)
                    o[c] = std::uint8_t(sum[c] / n);
            }
            else
            {
                const float fx = std::max(0.0f, (x + 0.5f) * sx - 0.5f), fy = std::max(0.0f, (y + 0.5f) * sy - 0.5f);
                const int ix = std::min(int(fx), src.w - 1), iy = std::min(int(fy), src.h - 1);
                const int jx = std::min(ix + 1, src.w - 1), jy = std::min(iy + 1, src.h - 1);
                const float ax = fx - ix, ay = fy - iy;
                for (int c = 0; c < 4; ++c)
                {
                    const float top = src.at(ix, iy)[c] * (1 - ax) + src.at(jx, iy)[c] * ax;
                    const float bottom = src.at(ix, jy)[c] * (1 - ax) + src.at(jx, jy)[c] * ax;
                    o[c] = std::uint8_t(std::lround(top * (1 - ay) + bottom * ay));
                }
            }
        }
    return out;
}

/* The part of src with the w:h shape, from its middle (filling, not fitting). */
Picture filled(const Picture &src, int w, int h)
{
    const float want = float(w) / float(h), have = float(src.w) / float(src.h);
    Picture crop;
    int cw = src.w, ch = src.h;
    if (have > want)
        cw = std::max(1, int(src.h * want));
    else
        ch = std::max(1, int(src.w / want));
    crop.w = cw;
    crop.h = ch;
    crop.px.resize(std::size_t(cw) * ch * 4);
    const int ox = (src.w - cw) / 2, oy = (src.h - ch) / 2;
    for (int y = 0; y < ch; ++y)
        std::memcpy(crop.at(0, y), src.at(ox, oy + y), std::size_t(cw) * 4);
    return scaled(crop, w, h);
}

/* A soft, dark copy to sit behind: blurred at a small size, then grown. */
Picture backdrop(const Picture &src, int w, int h, float darken)
{
    Picture small = filled(src, std::max(8, w / 24), std::max(8, h / 24));
    for (int pass = 0; pass < 3; ++pass)
    {
        Picture blurred = small;
        for (int y = 0; y < small.h; ++y)
            for (int x = 0; x < small.w; ++x)
            {
                unsigned sum[3] = {0, 0, 0}, n = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int xx = std::clamp(x + dx, 0, small.w - 1), yy = std::clamp(y + dy, 0, small.h - 1);
                        for (int c = 0; c < 3; ++c)
                            sum[c] += small.at(xx, yy)[c];
                        ++n;
                    }
                for (int c = 0; c < 3; ++c)
                    blurred.at(x, y)[c] = std::uint8_t(sum[c] / n);
            }
        small = blurred;
    }
    Picture out = scaled(small, w, h);
    for (std::size_t i = 0; i < out.px.size(); i += 4)
    {
        for (int c = 0; c < 3; ++c)
            out.px[i + c] = std::uint8_t(out.px[i + c] * darken);
        out.px[i + 3] = 255;
    }
    return out;
}

/* src fitted inside w x h at (cx, cy), over dst, with a soft shadow. */
void place(Picture &dst, const Picture &src, int fit_w, int fit_h, int cx, int cy)
{
    const float k = std::min(float(fit_w) / src.w, float(fit_h) / src.h);
    const int w = std::max(1, int(src.w * k)), h = std::max(1, int(src.h * k));
    const Picture p = scaled(src, w, h);
    const int x0 = cx - w / 2, y0 = cy - h / 2;
    const int shadow = std::max(4, h / 30);
    for (int y = -shadow; y < h + shadow * 2; ++y)
        for (int x = -shadow; x < w + shadow; ++x)
        {
            const int dx = x0 + x, dy = y0 + y + shadow / 2;
            if (dx < 0 || dy < 0 || dx >= dst.w || dy >= dst.h)
                continue;
            const float ox = std::max({0.0f, float(-x), float(x - w + 1)}) / shadow;
            const float oy = std::max({0.0f, float(-y), float(y - h + 1)}) / shadow;
            const float a = 0.55f * std::max(0.0f, 1.0f - std::sqrt(ox * ox + oy * oy));
            std::uint8_t *d = dst.at(dx, dy);
            for (int c = 0; c < 3; ++c)
                d[c] = std::uint8_t(d[c] * (1.0f - a));
        }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const int dx = x0 + x, dy = y0 + y;
            if (dx < 0 || dy < 0 || dx >= dst.w || dy >= dst.h)
                continue;
            const std::uint8_t *s = p.at(x, y);
            std::uint8_t *d = dst.at(dx, dy);
            const unsigned a = s[3];
            for (int c = 0; c < 3; ++c)
                d[c] = std::uint8_t((s[c] * a + d[c] * (255 - a)) / 255);
        }
}

/* ---- BC7 (mode 6: one subset, RGBA, 4-bit indices) ---------------------- */

struct Bits
{
    std::uint64_t lo = 0, hi = 0;
    int at = 0;
    void put(std::uint32_t value, int count)
    {
        for (int i = 0; i < count; ++i, ++at)
        {
            const std::uint64_t bit = (value >> i) & 1u;
            if (at < 64)
                lo |= bit << at;
            else
                hi |= bit << (at - 64);
        }
    }
};

constexpr int kWeights[16] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};

void bc7_block(const std::uint8_t px[16][4], std::uint8_t out[16])
{
    /* The line through the block's colours: from its least to its greatest
     * along the colour's main direction (approximated by the box diagonal,
     * with each channel's sign from its covariance with the brightest). */
    float mean[4] = {0, 0, 0, 0};
    for (int i = 0; i < 16; ++i)
        for (int c = 0; c < 4; ++c)
            mean[c] += px[i][c] / 16.0f;
    float axis[4] = {0, 0, 0, 0};
    for (int i = 0; i < 16; ++i)
    {
        const float d[4] = {px[i][0] - mean[0], px[i][1] - mean[1], px[i][2] - mean[2], px[i][3] - mean[3]};
        const float s = d[0] + d[1] + d[2];
        for (int c = 0; c < 4; ++c)
            axis[c] += d[c] * (s >= 0 ? 1.0f : -1.0f);
    }
    float len = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2] + axis[3] * axis[3]);
    if (len < 1e-3f)
    {
        axis[0] = axis[1] = axis[2] = 0.57735f;
        axis[3] = 0;
        len = 1;
    }
    else
        for (float &a : axis)
            a /= len;
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 16; ++i)
    {
        const float t = (px[i][0] - mean[0]) * axis[0] + (px[i][1] - mean[1]) * axis[1] +
                        (px[i][2] - mean[2]) * axis[2] + (px[i][3] - mean[3]) * axis[3];
        lo = std::min(lo, t);
        hi = std::max(hi, t);
    }
    int e[2][4];
    int p[2];
    for (int k = 0; k < 2; ++k)
    {
        const float t = k == 0 ? lo : hi;
        float want[4];
        for (int c = 0; c < 4; ++c)
            want[c] = std::clamp(mean[c] + axis[c] * t, 0.0f, 255.0f);
        /* 7 bits a channel and one shared low bit: the closer of the two. */
        float best = 1e9f;
        for (int pb = 0; pb < 2; ++pb)
        {
            int q[4];
            float err = 0;
            for (int c = 0; c < 4; ++c)
            {
                q[c] = std::clamp(int(std::lround((want[c] - pb) / 2.0f)), 0, 127);
                const float v = float(q[c] * 2 + pb);
                err += (v - want[c]) * (v - want[c]);
            }
            if (err < best)
            {
                best = err;
                p[k] = pb;
                for (int c = 0; c < 4; ++c)
                    e[k][c] = q[c];
            }
        }
    }
    int full[2][4];
    for (int k = 0; k < 2; ++k)
        for (int c = 0; c < 4; ++c)
            full[k][c] = e[k][c] * 2 + p[k];
    int index[16];
    for (int i = 0; i < 16; ++i)
    {
        int best_i = 0;
        int best = 1 << 30;
        for (int w = 0; w < 16; ++w)
        {
            int err = 0;
            for (int c = 0; c < 4; ++c)
            {
                const int v = (full[0][c] * (64 - kWeights[w]) + full[1][c] * kWeights[w] + 32) >> 6;
                err += (v - px[i][c]) * (v - px[i][c]);
            }
            if (err < best)
            {
                best = err;
                best_i = w;
            }
        }
        index[i] = best_i;
    }
    if (index[0] & 8)
    {
        /* The first index has no top bit: swap the ends instead. */
        for (int c = 0; c < 4; ++c)
            std::swap(e[0][c], e[1][c]);
        std::swap(p[0], p[1]);
        for (int &i : index)
            i = 15 - i;
    }
    Bits b;
    b.put(1u << 6, 7); /* mode 6 */
    for (int c = 0; c < 4; ++c)
    {
        b.put(std::uint32_t(e[0][c]), 7);
        b.put(std::uint32_t(e[1][c]), 7);
    }
    b.put(std::uint32_t(p[0]), 1);
    b.put(std::uint32_t(p[1]), 1);
    b.put(std::uint32_t(index[0]), 3);
    for (int i = 1; i < 16; ++i)
        b.put(std::uint32_t(index[i]), 4);
    std::memcpy(out, &b.lo, 8);
    std::memcpy(out + 8, &b.hi, 8);
}

bool write_dds_bc7(const std::string &path, const Picture &pic)
{
    const int bw = pic.w / 4, bh = pic.h / 4;
    std::vector<std::uint8_t> file(148 + std::size_t(bw) * bh * 16, 0);
    auto u32 = [&](std::size_t at, std::uint32_t v) { std::memcpy(&file[at], &v, 4); };
    std::memcpy(&file[0], "DDS ", 4);
    u32(4, 124);
    u32(8, 0xA1007); /* caps, height, width, pixel format, mip count, linear size */
    u32(12, std::uint32_t(pic.h));
    u32(16, std::uint32_t(pic.w));
    u32(20, std::uint32_t(bw * bh * 16));
    u32(24, 1);
    u32(28, 1);
    u32(76, 32);
    u32(80, 4); /* a four-character code: */
    std::memcpy(&file[84], "DX10", 4);
    u32(108, 0x1000);
    u32(128, 98); /* BC7_UNORM */
    u32(132, 3);  /* a 2D texture */
    u32(136, 0);
    u32(140, 1);
    u32(144, 1);
    std::uint8_t block[16][4];
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx)
        {
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x)
                    std::memcpy(block[y * 4 + x], pic.at(bx * 4 + x, by * 4 + y), 4);
            bc7_block(block, &file[148 + (std::size_t(by) * bw + bx) * 16]);
        }
    return porpoise::write_whole(path, file.data(), file.size());
}

bool copy_file(const std::string &from, const std::string &to)
{
    std::vector<unsigned char> data;
    return porpoise::read_whole(from, data) && porpoise::write_whole(to, data.data(), data.size());
}

std::string json_escape(const std::string &s)
{
    std::string out;
    for (const char ch : s)
    {
        if (ch == '"' || ch == '\\')
            out += '\\';
        if (static_cast<unsigned char>(ch) < 0x20)
            continue;
        out += ch;
    }
    return out;
}

std::string list_path(const std::string &data_dir)
{
    return art_dir(data_dir) + "/forwarders.txt";
}

std::vector<std::pair<std::string, std::string>> read_list(const std::string &data_dir)
{
    std::vector<std::pair<std::string, std::string>> list;
    if (std::FILE *f = std::fopen(list_path(data_dir).c_str(), "r"))
    {
        char line[1200];
        while (std::fgets(line, sizeof line, f))
        {
            std::string s = line;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                s.pop_back();
            const auto tab = s.find('\t');
            if (tab != std::string::npos)
                list.emplace_back(s.substr(0, tab), s.substr(tab + 1));
        }
        std::fclose(f);
    }
    return list;
}
} // namespace

std::string art_dir(const std::string &data_dir)
{
    return data_dir + "/home-art";
}

std::string own_icon(const std::string &data_dir, const std::string &game_id)
{
    return first_of(art_dir(data_dir) + "/" + game_id + "-icon");
}

std::string own_background(const std::string &data_dir, const std::string &game_id)
{
    return first_of(art_dir(data_dir) + "/" + game_id + "-background");
}

std::string existing(const std::string &data_dir, const std::string &game_path)
{
    for (const auto &[id, path] : read_list(data_dir))
        if (path == game_path && exists(std::string(kHomebrew) + "/" + id))
            return id;
    return "";
}

Result make(const std::string &data_dir, const Options &o)
{
    Result r;
    mkdir(art_dir(data_dir).c_str(), 0777);
    struct stat st;
    if (stat(kHomebrew, &st) != 0 || !S_ISDIR(st.st_mode))
    {
        r.error = "no /data/homebrew";
        return r;
    }
    /* Its title ID: the one it had, else the first free. */
    auto list = read_list(data_dir);
    r.title_id = existing(data_dir, o.game_path);
    if (r.title_id.empty())
        for (int n = kFirst; n <= kLast && r.title_id.empty(); ++n)
        {
            const std::string id = "PPSA" + std::to_string(n);
            bool used = exists(std::string(kHomebrew) + "/" + id);
            for (const auto &e : list)
                used |= e.first == id;
            if (!used)
                r.title_id = id;
        }
    if (r.title_id.empty())
    {
        r.error = "no free title ID";
        return r;
    }
    r.folder = std::string(kHomebrew) + "/" + r.title_id;
    mkdir(r.folder.c_str(), 0777);
    mkdir((r.folder + "/sce_sys").c_str(), 0777);
    mkdir((r.folder + "/sce_module").c_str(), 0777);
    if (!copy_file(PORPOISE_APP "/assets/forwarder/eboot.bin", r.folder + "/eboot.bin") ||
        !copy_file(PORPOISE_APP "/assets/forwarder/home-launcher.elf", r.folder + "/home-launcher.elf") ||
        !copy_file(PORPOISE_APP "/sce_module/libc.prx", r.folder + "/sce_module/libc.prx"))
    {
        r.error = "couldn't copy the forwarder into " + r.folder;
        return r;
    }
    if (std::FILE *f = porpoise::open_atomic(r.folder + "/forward.txt"))
    {
        std::fprintf(f, "%s\n%s", o.game_path.c_str(), o.exit_after_game ? "exit-after-game\n" : "");
        porpoise::finish_atomic(f, r.folder + "/forward.txt");
    }
    const std::string number = r.title_id.substr(4);
    if (std::FILE *f = porpoise::open_atomic(r.folder + "/sce_sys/param.json"))
    {
        std::fprintf(f,
                     "{\n  \"ageLevel\": {\"default\": 0},\n  \"applicationCategoryType\": 0,\n"
                     "  \"applicationDrmType\": \"free\",\n  \"attribute\": 0,\n  \"attribute2\": 0,\n"
                     "  \"attribute3\": 524352,\n  \"conceptId\": \"%s\",\n  \"contentBadgeType\": 1,\n"
                     "  \"contentId\": \"UP9000-%s_00-PORPOISEFWD%s\",\n  \"contentVersion\": \"01.000.000\",\n"
                     "  \"downloadDataSize\": 0,\n"
                     "  \"gameIntent\": {\"permittedIntents\": [{\"intentType\": \"launchActivity\"}]},\n"
                     "  \"localizedParameters\": {\"defaultLanguage\": \"en-US\", \"en-US\": {\"titleName\": \"%s\"}},\n"
                     "  \"masterVersion\": \"01.00\",\n"
                     "  \"pubtools\": {\"creationDate\": \"2026-10-08 00:00:00\", \"loudnessSnd0\": \"-28.00\", "
                     "\"toolVersion\": \"2.00\"},\n"
                     "  \"requiredSystemSoftwareVersion\": \"0x0000000000000000\",\n"
                     "  \"sdkVersion\": \"0x0000000000000000\",\n  \"titleId\": \"%s\",\n  \"versionFileUri\": \"\"\n}\n",
                     number.c_str(), r.title_id.c_str(), number.c_str(), json_escape(o.title).c_str(),
                     r.title_id.c_str());
        porpoise::finish_atomic(f, r.folder + "/sce_sys/param.json");
    }
    /* The tile. */
    Picture cover, icon_src, bg_src;
    const bool have_cover = load(o.cover_path, cover);
    if (o.icon == 1 && load(own_icon(data_dir, o.game_id), icon_src))
    {
        const Picture icon = filled(icon_src, 512, 512);
        porpoise::image::write_png(r.folder + "/sce_sys/icon0.png", icon.px, 512, 512);
    }
    else if (have_cover)
    {
        Picture icon = backdrop(cover, 512, 512, 0.55f);
        place(icon, cover, 452, 452, 256, 256);
        porpoise::image::write_png(r.folder + "/sce_sys/icon0.png", icon.px, 512, 512);
    }
    else
        copy_file(PORPOISE_APP "/sce_sys/icon0.png", r.folder + "/sce_sys/icon0.png");
    /* The background behind it. */
    bool background = false;
    if (o.background == 2 && load(own_background(data_dir, o.game_id), bg_src))
        background = write_dds_bc7(r.folder + "/sce_sys/pic0.dds", filled(bg_src, 3840, 2160));
    else if (o.background == 1 && have_cover)
    {
        Picture bg = backdrop(cover, 3840, 2160, 0.45f);
        place(bg, cover, 1500, 1500, 2700, 1080);
        background = write_dds_bc7(r.folder + "/sce_sys/pic0.dds", bg);
    }
    if (background)
        copy_file(r.folder + "/sce_sys/pic0.dds", r.folder + "/sce_sys/pic1.dds");
    else
    {
        copy_file(PORPOISE_APP "/sce_sys/pic0.dds", r.folder + "/sce_sys/pic0.dds");
        copy_file(PORPOISE_APP "/sce_sys/pic1.dds", r.folder + "/sce_sys/pic1.dds");
    }
    /* Kept, so making it again keeps its title ID. */
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&](const auto &e) { return e.first == r.title_id || e.second == o.game_path; }),
               list.end());
    list.emplace_back(r.title_id, o.game_path);
    if (std::FILE *f = porpoise::open_atomic(list_path(data_dir)))
    {
        for (const auto &e : list)
            std::fprintf(f, "%s\t%s\n", e.first.c_str(), e.second.c_str());
        porpoise::finish_atomic(f, list_path(data_dir));
    }
    ps5::debug::mark(("forwarders: made " + r.folder + " for " + o.game_path).c_str());
    r.ok = true;
    return r;
}
} // namespace porpoise::forwarders
