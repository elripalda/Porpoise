/* Porpoise - the player's own GameCube BIOS, found by what is in it.
 * Porpoise ships no BIOS and none of its data.
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
} // namespace porpoise::bios
