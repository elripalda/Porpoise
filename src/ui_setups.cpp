/* Porpoise UI - setups: a game's video and graphics settings, saved to use
 * again on other games (or on every game).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_setups.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace porpoise::ui::setups
{
namespace
{
std::string g_dir = "/data/porpoise/setups";
constexpr char kFrom[] = "# from: ";

std::string path_of(int index)
{
    return g_dir + "/setup" + std::to_string(index + 1) + ".ini";
}
} // namespace

void set_dir(const std::string &data_dir)
{
    g_dir = data_dir + "/setups";
}

const std::vector<std::string> &keys()
{
    static const std::vector<std::string> k = {
        "resolution",   "widescreen", "wide", "aspect",      "antialiasing",   "anisotropy",  "texture_filter",
        "resampling",   "screen_filter",  "filter_strength", "border",     "fps_overlay", "shader_mode",
        "texture_cache", "pixel_lighting", "disable_fog", "crop_overscan",  "skip_dupes",  "custom_textures",
        "bloom",        "color_saturation", "color_contrast", "color_warmth",
    };
    return k;
}

Setup get(int index)
{
    Setup s;
    if (index < 0 || index >= kCount)
        return s;
    s.path = path_of(index);
    std::FILE *f = std::fopen(s.path.c_str(), "r");
    if (!f)
        return s;
    s.exists = true;
    char line[512];
    while (std::fgets(line, sizeof line, f))
        if (std::strncmp(line, kFrom, sizeof kFrom - 1) == 0)
        {
            s.from = line + sizeof kFrom - 1;
            while (!s.from.empty() && (s.from.back() == '\n' || s.from.back() == '\r'))
                s.from.pop_back();
            break;
        }
    std::fclose(f);
    return s;
}

bool save(int index, const porpoise::Settings &settings, const std::string &from)
{
    if (index < 0 || index >= kCount)
        return false;
    mkdir(g_dir.c_str(), 0777);
    const std::string path = path_of(index);
    if (!settings.save_keys(path, keys()))
        return false;
    /* The game's name on the first line, the settings after it. */
    std::string body;
    if (std::FILE *f = std::fopen(path.c_str(), "r"))
    {
        char buf[512];
        while (std::fgets(buf, sizeof buf, f))
            if (buf[0] != '#')
                body += buf;
        std::fclose(f);
    }
    std::FILE *f = std::fopen(path.c_str(), "w");
    if (!f)
        return false;
    std::fprintf(f, "%s%s\n%s", kFrom, from.c_str(), body.c_str());
    std::fclose(f);
    return true;
}

bool apply(int index, porpoise::Settings &settings, std::vector<std::string> *game_keys)
{
    const Setup s = get(index);
    if (!s.exists || !settings.load(s.path, true))
        return false;
    if (game_keys)
        for (const std::string &k : porpoise::Settings::keys_in(s.path))
            if (std::find(game_keys->begin(), game_keys->end(), k) == game_keys->end())
                game_keys->push_back(k);
    return true;
}
} // namespace porpoise::ui::setups
