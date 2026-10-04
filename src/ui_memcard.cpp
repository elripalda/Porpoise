/* Porpoise UI - GameCube memory cards.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A save on a GameCube card is a 64-byte directory entry and its data. The
 * entry says where in the data the banner (96x32) and up to eight icon frames
 * (32x32) are, in which format (RGB5A3 or 8-bit palette, in 4x4 / 8x4 tiles),
 * and where the two 32-character comment lines are. Dolphin keeps a card
 * either as a .raw image of the whole card or as a folder of .gci files (entry
 * followed by data). Layout as documented by Dolphin's GCMemcard and YAGCD. */
#include "ui_memcard.hpp"
#include "ui_i18n.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>

namespace porpoise::ui
{
namespace
{
constexpr std::size_t kBlock = 0x2000;
constexpr long long kEpoch2000 = 946684800; /* 2000-01-01 in unix time */

std::uint16_t be16(const std::uint8_t *p)
{
    return std::uint16_t((p[0] << 8) | p[1]);
}

std::uint32_t be32(const std::uint8_t *p)
{
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}

void rgb5a3(std::uint16_t v, std::uint8_t *out)
{
    if (v & 0x8000) /* RGB555, opaque */
    {
        out[0] = std::uint8_t(((v >> 10) & 0x1f) * 255 / 31);
        out[1] = std::uint8_t(((v >> 5) & 0x1f) * 255 / 31);
        out[2] = std::uint8_t((v & 0x1f) * 255 / 31);
        out[3] = 255;
    }
    else /* A3 RGB444 */
    {
        out[0] = std::uint8_t(((v >> 8) & 0xf) * 17);
        out[1] = std::uint8_t(((v >> 4) & 0xf) * 17);
        out[2] = std::uint8_t((v & 0xf) * 17);
        out[3] = std::uint8_t(((v >> 12) & 0x7) * 255 / 7);
    }
}

/* RGB5A3 in 4x4 tiles. */
bool decode_rgb5a3(const std::uint8_t *src, std::size_t avail, int w, int h, std::vector<std::uint8_t> &out)
{
    if (avail < std::size_t(w) * h * 2)
        return false;
    out.assign(std::size_t(w) * h * 4, 0);
    std::size_t i = 0;
    for (int ty = 0; ty < h; ty += 4)
        for (int tx = 0; tx < w; tx += 4)
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 4; ++x, i += 2)
                    rgb5a3(be16(src + i), &out[(std::size_t(ty + y) * w + tx + x) * 4]);
    return true;
}

/* 8-bit palette indices in 8x4 tiles, with a 256-entry RGB5A3 palette. */
bool decode_ci8(const std::uint8_t *src, std::size_t avail, const std::uint8_t *palette, int w, int h,
                std::vector<std::uint8_t> &out)
{
    if (avail < std::size_t(w) * h || !palette)
        return false;
    out.assign(std::size_t(w) * h * 4, 0);
    std::size_t i = 0;
    for (int ty = 0; ty < h; ty += 4)
        for (int tx = 0; tx < w; tx += 8)
            for (int y = 0; y < 4; ++y)
                for (int x = 0; x < 8; ++x, ++i)
                    rgb5a3(be16(palette + src[i] * 2), &out[(std::size_t(ty + y) * w + tx + x) * 4]);
    return true;
}

std::string text_field(const std::uint8_t *p, std::size_t n)
{
    std::string s;
    for (std::size_t i = 0; i < n && p[i]; ++i)
        s += (p[i] >= 0x20 && p[i] < 0x7f) ? char(p[i]) : ' ';
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
    std::size_t start = 0;
    while (start < s.size() && s[start] == ' ')
        ++start;
    return s.substr(start);
}

/* Fills a Save from its directory entry and its data. */
bool parse_entry(const std::uint8_t *entry, const std::uint8_t *data, std::size_t size, Save &out)
{
    if (be32(entry) == 0xFFFFFFFF)
        return false;
    out.game_code = text_field(entry, 4) + text_field(entry + 4, 2);
    out.file_name = text_field(entry + 0x08, 32);
    out.modified = be32(entry + 0x28) + kEpoch2000;
    out.blocks = be16(entry + 0x38);

    const std::uint32_t comment = be32(entry + 0x3C);
    if (comment != 0xFFFFFFFF && comment + 64 <= size)
    {
        out.title = text_field(data + comment, 32);
        out.detail = text_field(data + comment + 32, 32);
    }
    if (out.title.empty())
        out.title = out.file_name;

    std::uint32_t off = be32(entry + 0x2C);
    if (off == 0xFFFFFFFF || off >= size)
        return true;
    const int banner_fmt = entry[0x07] & 3;
    if (banner_fmt == 2) /* RGB5A3 */
    {
        decode_rgb5a3(data + off, size - off, 96, 32, out.banner);
        off += 96 * 32 * 2;
    }
    else if (banner_fmt == 1) /* CI8 + its own palette */
    {
        if (off + 96 * 32 + 512 <= size)
            decode_ci8(data + off, size - off, data + off + 96 * 32, 96, 32, out.banner);
        off += 96 * 32 + 512;
    }

    /* Icon frames: two bits of format and two of speed each; a speed of 0
     * ends the animation. Frames in CI8 "shared" format use one palette that
     * follows the last frame. */
    const std::uint16_t icon_fmt = be16(entry + 0x30), speeds = be16(entry + 0x32);
    std::uint32_t first = 0;
    int first_fmt = 0;
    std::uint32_t pos = off;
    for (int i = 0; i < 8; ++i)
    {
        const int fmt = (icon_fmt >> (2 * i)) & 3, speed = (speeds >> (2 * i)) & 3;
        if (speed == 0)
            break;
        if (fmt && !first_fmt)
        {
            first = pos;
            first_fmt = fmt;
        }
        if (fmt == 2)
            pos += 32 * 32 * 2;
        else if (fmt == 1)
            pos += 32 * 32;
        else if (fmt == 3)
            pos += 32 * 32 + 512;
    }
    const std::uint32_t shared_palette = pos;
    if (first_fmt == 2 && first < size)
        decode_rgb5a3(data + first, size - first, 32, 32, out.icon);
    else if (first_fmt == 1 && shared_palette + 512 <= size)
        decode_ci8(data + first, size - first, data + shared_palette, 32, 32, out.icon);
    else if (first_fmt == 3 && first + 32 * 32 + 512 <= size)
        decode_ci8(data + first, size - first, data + first + 32 * 32, 32, 32, out.icon);
    return true;
}

bool read_file(const std::string &path, std::vector<std::uint8_t> &out)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0 || n > 64L * 1024 * 1024)
    {
        std::fclose(f);
        return false;
    }
    out.resize(std::size_t(n));
    const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

void load_folder(const std::string &dir, Card &card)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return;
    card.present = true;
    card.folder = true;
    int used = 0;
    while (dirent *e = readdir(d))
    {
        const std::string name = e->d_name;
        if (name.size() < 5 || name.substr(name.size() - 4) != ".gci")
            continue;
        Save s;
        if (parse_gci(dir + "/" + name, s))
        {
            used += s.blocks;
            card.saves.push_back(std::move(s));
        }
    }
    closedir(d);
    card.free_blocks = std::max(0, card.total_blocks - used);
}

/* A whole-card image: header, two directories, two allocation tables. */
void load_raw(const std::string &path, Card &card)
{
    std::vector<std::uint8_t> raw;
    if (!read_file(path, raw) || raw.size() < kBlock * 5)
        return;
    card.present = true;
    card.folder = false;
    const int mbits = be16(&raw[0x22]);
    card.total_blocks = std::max(0, mbits * 16 - 5);
    /* The directory and allocation table with the higher update counter are current. */
    const std::uint8_t *dir = be16(&raw[kBlock * 2 + 0x1FFA]) > be16(&raw[kBlock * 1 + 0x1FFA]) ? &raw[kBlock * 2]
                                                                                                    : &raw[kBlock * 1];
    const std::uint8_t *bat = be16(&raw[kBlock * 4 + 4]) > be16(&raw[kBlock * 3 + 4]) ? &raw[kBlock * 4]
                                                                                        : &raw[kBlock * 3];
    card.free_blocks = be16(bat + 6);
    const std::size_t blocks_in_file = raw.size() / kBlock;
    for (int i = 0; i < 127; ++i)
    {
        const std::uint8_t *entry = dir + i * 64;
        if (be32(entry) == 0xFFFFFFFF)
            continue;
        /* Gather the save's blocks by following the allocation chain. */
        std::vector<std::uint8_t> data;
        std::uint16_t block = be16(entry + 0x36);
        const int count = be16(entry + 0x38);
        for (int n = 0; n < count && block >= 5 && block < blocks_in_file; ++n)
        {
            data.insert(data.end(), raw.begin() + std::ptrdiff_t(block * kBlock),
                        raw.begin() + std::ptrdiff_t((block + 1) * kBlock));
            const std::size_t at = 0x0A + std::size_t(block - 5) * 2;
            if (at + 2 > kBlock)
                break;
            block = be16(bat + at);
            if (block == 0xFFFF)
                break;
        }
        Save s;
        s.path = path;
        if (parse_entry(entry, data.data(), data.size(), s))
            card.saves.push_back(std::move(s));
    }
}

void scan(const std::string &dir, int depth, std::vector<std::string> &dirs, std::vector<std::string> &raws)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return;
    std::vector<std::string> sub;
    while (dirent *e = readdir(d))
    {
        const std::string name = e->d_name;
        if (name.empty() || name[0] == '.')
            continue;
        const std::string path = dir + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            if (name == "Card A" || name == "Card B")
                dirs.push_back(path);
            else if (depth > 0)
                sub.push_back(path);
        }
        else if (name.size() > 4 && name.substr(name.size() - 4) == ".raw" && name.rfind("MemoryCard", 0) == 0)
            raws.push_back(path);
    }
    closedir(d);
    for (const std::string &s : sub)
        scan(s, depth - 1, dirs, raws);
}
} // namespace

bool parse_gci(const std::string &path, Save &out)
{
    std::vector<std::uint8_t> file;
    if (!read_file(path, file) || file.size() < 64)
        return false;
    out.path = path;
    return parse_entry(file.data(), file.data() + 64, file.size() - 64, out);
}

std::string format_date(long long unix_time)
{
    if (unix_time <= 0)
        return "";
    const std::time_t t = static_cast<std::time_t>(unix_time);
    std::tm tm{};
    gmtime_r(&t, &tm); /* the card stores local time already */
    char buf[32];
    if (language() == Language::English) /* month first, as in the US */
        std::snprintf(buf, sizeof buf, "%d/%d/%04d  %02d:%02d", tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900,
                      tm.tm_hour, tm.tm_min);
    else /* day first */
        std::snprintf(buf, sizeof buf, "%d/%d/%04d  %02d:%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                      tm.tm_hour, tm.tm_min);
    return buf;
}

void load_cards(const std::string &saves_dir, Card &a, Card &b)
{
    a = Card{};
    b = Card{};
    a.slot = "A";
    b.slot = "B";
    std::vector<std::string> dirs, raws;
    scan(saves_dir, 4, dirs, raws);
    std::sort(dirs.begin(), dirs.end());
    std::sort(raws.begin(), raws.end());
    for (const std::string &d : dirs)
    {
        Card &c = d.substr(d.size() - 1) == "A" ? a : b;
        if (!c.present)
            load_folder(d, c);
    }
    for (const std::string &r : raws)
    {
        const std::string name = r.substr(r.rfind('/') + 1);
        Card &c = name.size() > 10 && name[10] == 'B' ? b : a;
        if (!c.present)
            load_raw(r, c);
    }
    auto by_title = [](const Save &x, const Save &y) { return x.title < y.title; };
    std::sort(a.saves.begin(), a.saves.end(), by_title);
    std::sort(b.saves.begin(), b.saves.end(), by_title);
}
} // namespace porpoise::ui
