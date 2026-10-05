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
#include <unistd.h>

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
    card.dir = dir;
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

namespace
{
/* UTF-16 big-endian, up to n characters, to UTF-8. */
std::string utf16be(const std::uint8_t *p, std::size_t n)
{
    std::string out;
    for (std::size_t i = 0; i < n; ++i)
    {
        std::uint32_t c = std::uint32_t(p[i * 2]) << 8 | p[i * 2 + 1];
        if (c == 0)
            break;
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n)
        {
            const std::uint32_t lo = std::uint32_t(p[i * 2 + 2]) << 8 | p[i * 2 + 3];
            c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
            ++i;
        }
        if (c < 0x80)
            out += char(c);
        else if (c < 0x800)
        {
            out += char(0xC0 | (c >> 6));
            out += char(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            out += char(0xE0 | (c >> 12));
            out += char(0x80 | ((c >> 6) & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
        else
        {
            out += char(0xF0 | (c >> 18));
            out += char(0x80 | ((c >> 12) & 0x3F));
            out += char(0x80 | ((c >> 6) & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '\n'))
        out.pop_back();
    return out;
}

/* Every file under dir: how many, how big, the newest time. */
void tally(const std::string &dir, int &files, long long &bytes, long long &newest, int depth = 0)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return;
    while (dirent *e = readdir(d))
    {
        const std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        const std::string p = dir + "/" + name;
        struct stat st;
        if (stat(p.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            if (depth < 6)
                tally(p, files, bytes, newest, depth + 1);
            continue;
        }
        ++files;
        bytes += st.st_size;
        newest = std::max(newest, (long long)st.st_mtime);
    }
    closedir(d);
}

bool copy_tree_to(const std::string &from, const std::string &to, int depth = 0)
{
    mkdir(to.c_str(), 0777);
    DIR *d = opendir(from.c_str());
    if (!d)
        return false;
    bool ok = true;
    while (dirent *e = readdir(d))
    {
        const std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        const std::string a = from + "/" + name, b = to + "/" + name;
        struct stat st;
        if (stat(a.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            ok = depth < 6 && copy_tree_to(a, b, depth + 1) && ok;
            continue;
        }
        std::vector<std::uint8_t> bytes;
        if (!read_file(a, bytes) && st.st_size > 0)
        {
            ok = false;
            continue;
        }
        std::FILE *f = std::fopen(b.c_str(), "wb");
        if (!f)
        {
            ok = false;
            continue;
        }
        ok = (bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size()) && ok;
        std::fclose(f);
    }
    closedir(d);
    return ok;
}

bool remove_tree(const std::string &dir, int depth = 0)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return false;
    bool ok = true;
    while (dirent *e = readdir(d))
    {
        const std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        const std::string p = dir + "/" + name;
        struct stat st;
        if (stat(p.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
            ok = depth < 6 && remove_tree(p, depth + 1) && ok;
        else
            ok = std::remove(p.c_str()) == 0 && ok;
    }
    closedir(d);
    return rmdir(dir.c_str()) == 0 && ok;
}
} // namespace

bool parse_wii_banner(const std::string &path, WiiSave &out)
{
    std::vector<std::uint8_t> b;
    constexpr std::size_t kHeader = 0xA0, kBanner = 192 * 64 * 2, kIcon = 48 * 48 * 2;
    if (!read_file(path, b) || b.size() < kHeader || std::memcmp(b.data(), "WIBN", 4) != 0)
        return false;
    out.title = utf16be(b.data() + 0x20, 32);
    out.detail = utf16be(b.data() + 0x60, 32);
    if (b.size() >= kHeader + kBanner)
        decode_rgb5a3(b.data() + kHeader, b.size() - kHeader, 192, 64, out.banner);
    if (b.size() >= kHeader + kBanner + kIcon)
        decode_rgb5a3(b.data() + kHeader + kBanner, b.size() - kHeader - kBanner, 48, 48, out.icon);
    return true;
}

void load_wii_saves(const std::string &saves_dir, std::vector<WiiSave> &out)
{
    out.clear();
    /* Disc games (00010000) and their channels' kin (00010004: some discs). */
    for (const char *kind : {"00010000", "00010004"})
    {
        const std::string root = saves_dir + "/User/Wii/title/" + kind;
        DIR *d = opendir(root.c_str());
        if (!d)
            continue;
        while (dirent *e = readdir(d))
        {
            const std::string id = e->d_name;
            if (id.size() != 8)
                continue;
            WiiSave s;
            s.data_dir = root + "/" + id + "/data";
            s.title_id = id;
            for (int i = 0; i < 4; ++i)
            {
                const int v = std::stoi(id.substr(std::size_t(i) * 2, 2), nullptr, 16);
                s.game_code += v >= 0x20 && v < 0x7F ? char(v) : '?';
            }
            struct stat st;
            if (stat(s.data_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
                continue;
            tally(s.data_dir, s.files, s.bytes, s.modified);
            if (s.files == 0)
                continue;
            if (!parse_wii_banner(s.data_dir + "/banner.bin", s))
                s.title = s.game_code;
            out.push_back(std::move(s));
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end(), [](const WiiSave &x, const WiiSave &y) { return x.title < y.title; });
}

bool backup_wii_save(const std::string &saves_dir, const WiiSave &save, std::string &where)
{
    const std::string root = saves_dir + "/User/Wii/backups";
    mkdir(root.c_str(), 0777);
    char stamp[32] = "copy";
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    if (localtime_r(&now, &tm))
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    where = root + "/" + save.game_code + "-" + stamp;
    return copy_tree_to(save.data_dir, where);
}

bool delete_wii_save(const WiiSave &save)
{
    return remove_tree(save.data_dir);
}

namespace
{
bool copy_one(const std::string &from, const std::string &to)
{
    std::FILE *in = std::fopen(from.c_str(), "rb");
    if (!in)
        return false;
    const std::string part = to + ".part";
    std::FILE *out = std::fopen(part.c_str(), "wb");
    bool ok = out != nullptr;
    char buf[65536];
    std::size_t n;
    while (ok && (n = std::fread(buf, 1, sizeof buf, in)) > 0)
        ok = std::fwrite(buf, 1, n, out) == n;
    std::fclose(in);
    if (out)
        ok = std::fclose(out) == 0 && ok;
    if (!ok || std::rename(part.c_str(), to.c_str()) != 0)
    {
        std::remove(part.c_str());
        return false;
    }
    return true;
}

bool is_dir(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string usb_saves(bool make)
{
    const std::string usb = usb_root();
    if (usb.empty())
        return "";
    const std::string dir = usb + "/Porpoise Saves";
    if (make)
    {
        mkdir(dir.c_str(), 0777);
        mkdir((dir + "/GameCube").c_str(), 0777);
        mkdir((dir + "/Wii").c_str(), 0777);
    }
    return dir;
}
} // namespace

std::string usb_root()
{
    for (int i = 0; i < 8; ++i)
    {
        const std::string root = "/mnt/usb" + std::to_string(i);
        if (is_dir(root))
            return root;
    }
    return "";
}

bool export_gc_save(const Save &save, std::string &where)
{
    const std::string dir = usb_saves(true);
    if (dir.empty() || save.path.size() < 4 || save.path.compare(save.path.size() - 4, 4, ".gci") != 0)
        return false;
    where = dir + "/GameCube/" + save.path.substr(save.path.rfind('/') + 1);
    return copy_one(save.path, where);
}

bool export_wii_save(const std::string &saves_dir, const WiiSave &save, std::string &where)
{
    const std::string dir = usb_saves(true);
    const std::string wii_root = saves_dir + "/User/Wii/";
    if (dir.empty() || save.data_dir.compare(0, wii_root.size(), wii_root) != 0)
        return false;
    where = dir + "/Wii/" + save.game_code + " " + save.title_id;
    if (!copy_tree_to(save.data_dir, where))
        return false;
    if (std::FILE *f = std::fopen((where + "/porpoise-wii-path.txt").c_str(), "w"))
    {
        std::fprintf(f, "%s\n", save.data_dir.substr(wii_root.size()).c_str());
        std::fclose(f);
    }
    return true;
}

bool import_usb_saves(const std::string &saves_dir, const Card &card, int &gc, int &wii, int &skipped)
{
    gc = wii = skipped = 0;
    const std::string dir = usb_saves(false);
    if (dir.empty())
        return false;
    if (card.folder && !card.dir.empty())
        if (DIR *d = opendir((dir + "/GameCube").c_str()))
        {
            while (dirent *e = readdir(d))
            {
                const std::string name = e->d_name;
                if (name.size() < 5 || name.compare(name.size() - 4, 4, ".gci") != 0)
                    continue;
                const std::string to = card.dir + "/" + name;
                struct stat st;
                if (stat(to.c_str(), &st) == 0)
                    ++skipped;
                else if (copy_one(dir + "/GameCube/" + name, to))
                    ++gc;
            }
            closedir(d);
        }
    if (DIR *d = opendir((dir + "/Wii").c_str()))
    {
        while (dirent *e = readdir(d))
        {
            const std::string name = e->d_name;
            if (name.empty() || name[0] == '.')
                continue;
            const std::string from = dir + "/Wii/" + name;
            char rel[256] = {0};
            if (std::FILE *f = std::fopen((from + "/porpoise-wii-path.txt").c_str(), "r"))
            {
                if (!std::fgets(rel, sizeof rel, f))
                    rel[0] = 0;
                std::fclose(f);
            }
            std::string path = rel;
            while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
                path.pop_back();
            if (path.empty() || path.find("..") != std::string::npos || path.rfind("title/", 0) != 0)
                continue;
            const std::string to = saves_dir + "/User/Wii/" + path;
            if (is_dir(to))
            {
                ++skipped;
                continue;
            }
            /* The folders above it, then the save. */
            for (std::size_t at = path.find('/'); at != std::string::npos; at = path.find('/', at + 1))
                mkdir((saves_dir + "/User/Wii/" + path.substr(0, at)).c_str(), 0777);
            if (copy_tree_to(from, to))
            {
                std::remove((to + "/porpoise-wii-path.txt").c_str());
                ++wii;
            }
        }
        closedir(d);
    }
    return true;
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
