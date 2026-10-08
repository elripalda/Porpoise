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
#include <dirent.h>
#include <sys/stat.h>
#include <vector>

#include "porpoise_atomic.hpp"
#include "porpoise_image.hpp"
#include "porpoise_paths.hpp"
#include "porpoise_tile_art.hpp"
#include "trace.hpp"

namespace porpoise::forwarders
{
namespace
{
constexpr const char *kHomebrew = "/data/homebrew";
constexpr int kFirst = 98001, kLast = 98999;

using porpoise::tileart::Picture;

bool exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::uint8_t *at(Picture &p, int x, int y)
{
    return &p.px[(std::size_t(y) * p.w + x) * 4];
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
                    std::memcpy(block[y * 4 + x], at(const_cast<Picture &>(pic), bx * 4 + x, by * 4 + y), 4);
            bc7_block(block, &file[148 + (std::size_t(by) * bw + bx) * 16]);
        }
    return porpoise::write_whole(path, file.data(), file.size());
}

/* The console starts an app only if its files can be read and run by everyone,
 * as an app copied to the console arrives (folders and files 0777; the mode
 * is what decides, not the owner).
 * Written 0644 it answers "Can't start the game or app" (CE-107750-0): making
 * the process fails with EACCES (found by ProsperoStore on a console). */
void open_to_all(const std::string &path, int depth = 0)
{
    struct stat st;
    if (depth > 8 || stat(path.c_str(), &st) != 0 || !(S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)))
        return;
    chmod(path.c_str(), 0777);
    if (!S_ISDIR(st.st_mode))
        return;
    std::vector<std::string> names;
    if (DIR *d = opendir(path.c_str()))
    {
        while (const dirent *e = readdir(d))
            if (std::strcmp(e->d_name, ".") != 0 && std::strcmp(e->d_name, "..") != 0)
                names.emplace_back(e->d_name);
        closedir(d);
    }
    for (const auto &name : names)
        open_to_all(path + "/" + name, depth + 1);
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

std::string existing(const std::string &data_dir, const std::string &game_path)
{
    for (const auto &[id, path] : read_list(data_dir))
        if (path == game_path && exists(std::string(kHomebrew) + "/" + id))
            return id;
    return "";
}

void repair(const std::string &data_dir)
{
    /* Tiles keep this Porpoise's copy of the tile program and the launcher,
     * so an update's fixes reach the tiles made before it. */
    std::vector<unsigned char> current[2];
    const char *names[2] = {"eboot.bin", "home-launcher.elf"};
    for (int i = 0; i < 2; ++i)
        porpoise::read_whole(std::string(PORPOISE_APP "/assets/forwarder/") + names[i], current[i]);
    for (const auto &e : read_list(data_dir))
    {
        const std::string folder = std::string(kHomebrew) + "/" + e.first;
        struct stat st;
        if (stat((folder + "/eboot.bin").c_str(), &st) != 0)
            continue;
        bool changed = (st.st_mode & 0777) != 0777;
        for (int i = 0; i < 2; ++i)
        {
            std::vector<unsigned char> have;
            if (!current[i].empty() && (!porpoise::read_whole(folder + "/" + names[i], have) || have != current[i]))
                changed |= porpoise::write_whole(folder + "/" + names[i], current[i].data(), current[i].size());
        }
        if (changed)
        {
            open_to_all(folder);
            ps5::debug::mark(("forwarders: brought up to date " + folder).c_str());
        }
    }
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
    /* The tile: its icon and the background behind it, as the player set
     * them in the tile art editor (porpoise_tile_art). */
    namespace art = porpoise::tileart;
    Picture src, made;
    if (art::load_picture(art::picture_path(o.art.icon, data_dir, o.game_id, o.cover_path), src, 2048) &&
        art::compose(o.art.icon, src, 512, 512, false, made))
        porpoise::image::write_png(r.folder + "/sce_sys/icon0.png", made.px, 512, 512);
    else
        copy_file(PORPOISE_APP "/sce_sys/icon0.png", r.folder + "/sce_sys/icon0.png");
    bool background = false;
    if (o.art.bg.source != art::Porpoise &&
        art::load_picture(art::picture_path(o.art.bg, data_dir, o.game_id, o.cover_path), src, 4096) &&
        art::compose(o.art.bg, src, 3840, 2160, true, made))
        background = write_dds_bc7(r.folder + "/sce_sys/pic0.dds", made);
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
    open_to_all(r.folder);
    ps5::debug::mark(("forwarders: made " + r.folder + " for " + o.game_path).c_str());
    r.ok = true;
    return r;
}
} // namespace porpoise::forwarders
