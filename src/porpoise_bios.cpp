/* Porpoise - the player's own GameCube BIOS: found by what is in it, and its
 * sounds read out of it. Porpoise ships no BIOS and none of its data.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_bios.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <dirent.h>
#include <sys/stat.h>

namespace porpoise::bios
{
namespace
{
constexpr long kIplSize = 2 * 1024 * 1024;

bool contains(const unsigned char *data, std::size_t size, const char *text)
{
    const std::size_t n = std::strlen(text);
    for (std::size_t i = 0; i + n <= size; ++i)
        if (std::memcmp(data + i, text, n) == 0)
            return true;
    return false;
}

void look_in(const std::string &dir, int depth, std::vector<GameCubeBios> &out)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return;
    std::vector<std::string> names;
    while (dirent *e = readdir(d))
        if (e->d_name[0] != '.')
            names.push_back(e->d_name);
    closedir(d);
    std::sort(names.begin(), names.end());
    for (const std::string &name : names)
    {
        const std::string path = dir + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            if (depth > 0)
                look_in(path, depth - 1, out);
            continue;
        }
        if (!S_ISREG(st.st_mode) || st.st_size != kIplSize)
            continue;
        unsigned char head[256];
        std::FILE *f = std::fopen(path.c_str(), "rb");
        if (!f)
            continue;
        const bool got = std::fread(head, 1, sizeof head, f) == sizeof head;
        std::fclose(f);
        GameCubeBios b;
        b.path = path;
        if (got && read_header(head, sizeof head, b.pal, b.revision))
            out.push_back(b);
    }
}

/* The IPL's code and data (0x100..0x1AFF00) are scrambled; this is the
 * descrambler Dolphin uses (CEXIIPL::Descrambler). */
void descramble(unsigned char *data, std::size_t size)
{
    std::uint8_t acc = 0, nacc = 0;
    std::uint16_t t = 0x2953, u = 0xd9c2, v = 0x3ff1;
    std::uint8_t x = 1;
    for (std::size_t it = 0; it < size;)
    {
        const int t0 = t & 1, t1 = (t >> 1) & 1, u0 = u & 1, u1 = (u >> 1) & 1, v0 = v & 1;
        x ^= t1 ^ v0;
        x ^= (u0 | u1);
        x ^= (t0 ^ u1 ^ v0) & (t0 ^ u0);
        if (t0 == u0)
        {
            v >>= 1;
            if (v0)
                v ^= 0xb3d0;
        }
        if (t0 == 0)
        {
            u >>= 1;
            if (u0)
                u ^= 0xfb10;
        }
        t >>= 1;
        if (t0)
            t ^= 0xa740;
        ++nacc;
        acc = std::uint8_t(2 * acc + x);
        if (nacc == 8)
        {
            data[it++] ^= acc;
            nacc = 0;
        }
    }
}

std::uint32_t be32(const unsigned char *p)
{
    return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 | std::uint32_t(p[2]) << 8 | p[3];
}

float be_float(const unsigned char *p)
{
    const std::uint32_t bits = be32(p);
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

/* Nintendo's 4-bit ADPCM (AFC): nine bytes for sixteen samples. */
void decode_afc(const unsigned char *data, std::size_t size, std::size_t samples, std::vector<std::int16_t> &out)
{
    static const int kCoef[16][2] = {{0, 0},       {2048, 0},     {0, 2048},     {1024, 1024},
                                     {4096, -2048}, {3584, -1536}, {3072, -1024}, {4608, -2560},
                                     {4200, -2248}, {4800, -2300}, {5120, -3072}, {2048, -2048},
                                     {1024, -1024}, {-1024, 1024}, {-1024, 0},    {-2048, 0}};
    out.clear();
    out.reserve(samples);
    int h1 = 0, h2 = 0;
    for (std::size_t p = 0; out.size() < samples && p + 9 <= size; p += 9)
    {
        const int delta = 1 << (data[p] >> 4);
        const int c1 = kCoef[data[p] & 15][0], c2 = kCoef[data[p] & 15][1];
        for (int i = 0; i < 16 && out.size() < samples; ++i)
        {
            const unsigned char b = data[p + 1 + i / 2];
            int n = i % 2 == 0 ? b >> 4 : b & 15;
            if (n >= 8)
                n -= 16;
            int s = ((n * delta) << 11) + c1 * h1 + c2 * h2;
            s >>= 11;
            s = std::clamp(s, -32768, 32767);
            out.push_back(std::int16_t(s));
            h2 = h1;
            h1 = s;
        }
    }
}
} // namespace

bool read_header(const unsigned char *head, std::size_t size, bool &pal, std::string &revision)
{
    if (!contains(head, size, "(C) 1999") || !contains(head, size, "Nintendo"))
        return false;
    pal = contains(head, size, "PAL ") && !contains(head, size, "MPAL");
    revision.clear();
    for (std::size_t i = 0; i + 12 <= size; ++i)
        if (std::memcmp(head + i, "Revision ", 9) == 0 && head[i + 9] >= '0' && head[i + 9] <= '9')
        {
            std::size_t j = i + 9;
            while (j < size && ((head[j] >= '0' && head[j] <= '9') || head[j] == '.'))
                revision += char(head[j++]);
            break;
        }
    if (revision.empty())
        revision = "1.0"; /* the first NTSC BIOS names no revision */
    return true;
}

std::vector<GameCubeBios> find_gamecube(const std::string &dir)
{
    std::vector<GameCubeBios> out;
    look_in(dir, 1, out);
    return out;
}

const GameCubeBios *for_region(const std::vector<GameCubeBios> &found, bool pal)
{
    const GameCubeBios *best = nullptr;
    for (const GameCubeBios &b : found)
        if (b.pal == pal && (!best || b.revision > best->revision))
            best = &b; /* the newest of a region */
    return best;
}

std::vector<Sound> read_sounds(const std::string &path)
{
    std::vector<Sound> sounds;
    std::vector<unsigned char> rom;
    if (std::FILE *f = std::fopen(path.c_str(), "rb"))
    {
        rom.resize(std::size_t(kIplSize));
        const std::size_t got = std::fread(rom.data(), 1, rom.size(), f);
        std::fclose(f);
        rom.resize(got);
    }
    if (rom.size() < 0x1AFF00)
        return sounds;
    descramble(rom.data() + 0x100, 0x1AFE00);
    const std::size_t end = 0x1AFF00;

    /* The wave bank: "WSYS", then its wave info (WINF) and the waves, packed
     * right after it. */
    std::size_t w = 0;
    for (std::size_t i = 0x100; i + 0x40 < end; i += 4)
        if (std::memcmp(rom.data() + i, "WSYS", 4) == 0)
        {
            w = i;
            break;
        }
    if (!w)
        return sounds;
    const std::uint32_t wsys_size = be32(&rom[w + 4]), winf_at = be32(&rom[w + 0x10]);
    if (wsys_size < 0x40 || winf_at + 16 > wsys_size || w + wsys_size > end ||
        std::memcmp(&rom[w + winf_at], "WINF", 4) != 0 || be32(&rom[w + winf_at + 4]) < 1)
        return sounds;
    const std::size_t group = w + be32(&rom[w + winf_at + 8]);
    const std::size_t aw = (w + wsys_size + 31) & ~std::size_t(31);
    if (group + 0x80 > end)
        return sounds;
    /* The group: its archive's name, then the count and the waves' offsets
     * (where the name ends differs between BIOS versions). */
    std::size_t count_at = 0, count = 0;
    for (std::size_t k = 0x20; k < 0x80 && !count_at; k += 4)
    {
        const std::uint32_t c = be32(&rom[group + k]);
        if (c < 1 || c > 64 || group + k + 4 + c * 4 > end)
            continue;
        bool rising = true;
        for (std::uint32_t j = 0; j + 1 < c && rising; ++j)
        {
            const std::uint32_t a = be32(&rom[group + k + 4 + j * 4]), b = be32(&rom[group + k + 8 + j * 4]);
            rising = a > 0 && a < b && b < wsys_size;
        }
        if (rising)
        {
            count_at = k;
            count = c;
        }
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::size_t e = w + be32(&rom[group + count_at + 4 + i * 4]);
        if (e + 0x20 > end)
            break;
        Sound s;
        const int format = rom[e + 1];
        s.rate = int(be_float(&rom[e + 4]) + 0.5f);
        const std::uint32_t off = be32(&rom[e + 8]), size = be32(&rom[e + 12]);
        s.loops = be32(&rom[e + 16]) != 0;
        s.loop_start = be32(&rom[e + 20]);
        const std::uint32_t count_samples = be32(&rom[e + 28]);
        if (format != 0 || s.rate < 4000 || s.rate > 96000 || aw + off + size > end || count_samples > 48000 * 30)
        {
            sounds.push_back(Sound{}); /* kept in its place, empty */
            continue;
        }
        decode_afc(&rom[aw + off], size, count_samples, s.samples);
        if (s.loop_start >= s.samples.size())
            s.loops = false;
        sounds.push_back(std::move(s));
    }
    return sounds;
}
} // namespace porpoise::bios
