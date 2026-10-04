/* Porpoise - settings.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * One table names every setting: its key in settings.ini (and in a game's own
 * file), its field, and its range. Loading, saving, a game's overrides and
 * resetting all go through it. */
#include "ui_settings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace porpoise
{
namespace
{
std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
        s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
    return s.substr(i);
}

bool as_bool(const std::string &v)
{
    return v == "1" || v == "true" || v == "on" || v == "yes" || v == "enabled";
}

struct Field
{
    const char *key;
    int Settings::*i;
    bool Settings::*b;
    int min, max;
};

const Field kFields[] = {
    {"auto_search", nullptr, &Settings::auto_search, 0, 1},
    {"download_covers", nullptr, &Settings::download_covers, 0, 1},
    {"download_info", nullptr, &Settings::download_info, 0, 1},
    {"resolution", &Settings::resolution, nullptr, 1, 6},
    {"widescreen", nullptr, &Settings::widescreen, 0, 1},
    {"aspect", &Settings::aspect, nullptr, 0, 3},
    {"anisotropy", &Settings::anisotropy, nullptr, 0, 4},
    {"texture_filter", &Settings::texture_filter, nullptr, 0, 2},
    {"antialiasing", &Settings::antialiasing, nullptr, 0, 6},
    {"resampling", &Settings::resampling, nullptr, 0, 6},
    {"sharp", nullptr, &Settings::sharp, 0, 1},
    {"fps_overlay", nullptr, &Settings::fps_overlay, 0, 1},
    {"shader_mode", &Settings::shader_mode, nullptr, 0, 3},
    {"texture_cache", &Settings::texture_cache, nullptr, 0, 2},
    {"pixel_lighting", nullptr, &Settings::pixel_lighting, 0, 1},
    {"disable_fog", nullptr, &Settings::disable_fog, 0, 1},
    {"crop_overscan", nullptr, &Settings::crop_overscan, 0, 1},
    {"custom_textures", nullptr, &Settings::custom_textures, 0, 1},
    {"skip_dupes", nullptr, &Settings::skip_dupes, 0, 1},
    {"volume", &Settings::volume, nullptr, 0, 10},
    {"muted", nullptr, &Settings::muted, 0, 1},
    {"menu_music", nullptr, &Settings::menu_music, 0, 1},
    {"music_volume", &Settings::music_volume, nullptr, 0, 10},
    {"menu_sounds", nullptr, &Settings::menu_sounds, 0, 1},
    {"sounds_volume", &Settings::sounds_volume, nullptr, 0, 10},
    {"gamecube_layout", nullptr, &Settings::gamecube_layout, 0, 1},
    {"rumble", nullptr, &Settings::rumble, 0, 1},
    {"cpu_clock", &Settings::cpu_clock, nullptr, 0, 9},
    {"dual_core", nullptr, &Settings::dual_core, 0, 1},
    {"fast_disc", nullptr, &Settings::fast_disc, 0, 1},
    {"cheats", nullptr, &Settings::cheats, 0, 1},
    {"language", &Settings::language, nullptr, 0, 9},
    {"progressive", nullptr, &Settings::progressive, 0, 1},
    {"reduced_motion", nullptr, &Settings::reduced_motion, 0, 1},
    {"large_text", nullptr, &Settings::large_text, 0, 1},
    {"ui_language", &Settings::ui_language, nullptr, 0, 4},
};

const Field *field(const std::string &key)
{
    for (const Field &f : kFields)
        if (key == f.key)
            return &f;
    return nullptr;
}

void write_field(std::FILE *f, const Settings &s, const Field &fd)
{
    if (fd.i)
        std::fprintf(f, "%s = %d\n", fd.key, s.*fd.i);
    else
        std::fprintf(f, "%s = %d\n", fd.key, (s.*fd.b) ? 1 : 0);
}

const char *on_off(bool v)
{
    return v ? "enabled" : "disabled";
}

/* Values the Dolphin core expects, in the order Porpoise lists them. */
const char *const kCpuClocks[] = {"0.50", "0.60", "0.70", "0.80", "0.90", "1.00", "1.50", "2.00", "2.50", "3.00"};
const char *const kTextureCache[] = {"128", "512", "0"};
/* English first; the core numbers Japanese 0. */
const int kLanguages[] = {1, 0, 2, 3, 4, 5, 6, 7, 8, 9};

const char *const kManaged[] = {
    "dolphin_efb_scale", "dolphin_widescreen_hack", "dolphin_aspect_ratio", "dolphin_max_anisotropy",
    "dolphin_force_texture_filtering_mode", "dolphin_anti_aliasing", "dolphin_enhance_output_resampling",
    "dolphin_shader_compilation_mode", "dolphin_texture_cache_accuracy", "dolphin_pixel_lighting",
    "dolphin_disable_fog", "dolphin_crop_overscan", "dolphin_load_custom_textures",
    "dolphin_cache_custom_textures", "dolphin_skip_dupe_frames", "dolphin_enable_rumble", "dolphin_cpu_clock_rate",
    "dolphin_main_cpu_thread", "dolphin_fast_disc_speed", "dolphin_cheats_enabled", "dolphin_language",
    "dolphin_progressive_scan",
};
} // namespace

bool Settings::load(const std::string &path, bool overlay)
{
    std::FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
        return false;
    if (!overlay)
        folders.clear();
    bool versioned = false;
    char line[1024];
    while (std::fgets(line, sizeof line, f))
    {
        std::string s = trim(line);
        if (s.empty() || s[0] == '#')
            continue;
        const auto eq = s.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string k = trim(s.substr(0, eq)), v = trim(s.substr(eq + 1));
        if (k == "settings_version")
        {
            versioned = true;
            continue;
        }
        if (k == "folder")
        {
            if (!overlay && !v.empty() && std::find(folders.begin(), folders.end(), v) == folders.end())
                folders.push_back(v);
            continue;
        }
        if (const Field *fd = field(k))
        {
            if (fd->i)
                this->*fd->i = std::clamp(std::atoi(v.c_str()), fd->min, fd->max);
            else
                this->*fd->b = as_bool(v);
        }
    }
    std::fclose(f);
    /* Builds before 10 saved 4K as the default; 1080p is the default now, and
     * 4K stays only for those who choose it again. */
    if (!overlay && !versioned && resolution == 6)
        resolution = 3;
    return true;
}

bool Settings::save(const std::string &path) const
{
    const std::string tmp = path + ".part";
    std::FILE *f = std::fopen(tmp.c_str(), "w");
    if (!f)
        return false;
    std::fprintf(f, "# Porpoise settings (written by the Settings screen)\nsettings_version = 10\n");
    for (const Field &fd : kFields)
        write_field(f, *this, fd);
    for (const std::string &folder : folders)
        std::fprintf(f, "folder = %s\n", folder.c_str());
    std::fclose(f);
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool Settings::save_keys(const std::string &path, const std::vector<std::string> &keys) const
{
    if (keys.empty())
    {
        std::remove(path.c_str());
        return true;
    }
    std::FILE *f = std::fopen(path.c_str(), "w");
    if (!f)
        return false;
    std::fprintf(f, "# This game's own settings (Porpoise > Details > Game settings)\n");
    for (const std::string &k : keys)
        if (const Field *fd = field(k))
            write_field(f, *this, *fd);
    std::fclose(f);
    return true;
}

std::vector<std::string> Settings::keys_in(const std::string &path)
{
    std::vector<std::string> keys;
    std::FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
        return keys;
    char line[512];
    while (std::fgets(line, sizeof line, f))
    {
        const std::string s = trim(line);
        const auto eq = s.find('=');
        if (s.empty() || s[0] == '#' || eq == std::string::npos)
            continue;
        const std::string k = trim(s.substr(0, eq));
        if (field(k) && std::find(keys.begin(), keys.end(), k) == keys.end())
            keys.push_back(k);
    }
    std::fclose(f);
    return keys;
}

void Settings::reset()
{
    const std::vector<std::string> keep = folders;
    *this = Settings{};
    folders = keep;
}

bool Settings::write_core_options(const std::string &options_ini) const
{
    std::vector<std::string> keep;
    if (std::FILE *f = std::fopen(options_ini.c_str(), "r"))
    {
        char line[512];
        while (std::fgets(line, sizeof line, f))
        {
            std::string s = line;
            const std::string key = trim(s.substr(0, s.find('=')));
            bool is_managed = false;
            for (const char *m : kManaged)
                is_managed |= key == m;
            if (!is_managed && s.rfind("# Set by Porpoise", 0) != 0)
                keep.push_back(s);
        }
        std::fclose(f);
    }
    std::FILE *f = std::fopen(options_ini.c_str(), "w");
    if (!f)
        return false;
    for (const std::string &s : keep)
        std::fputs(s.c_str(), f);
    std::fprintf(f, "# Set by Porpoise's Settings screen:\n");
    std::fprintf(f, "dolphin_efb_scale = %d\n", resolution);
    std::fprintf(f, "dolphin_widescreen_hack = %s\n", on_off(widescreen));
    std::fprintf(f, "dolphin_aspect_ratio = %d\n", aspect);
    std::fprintf(f, "dolphin_max_anisotropy = %d\n", anisotropy);
    std::fprintf(f, "dolphin_force_texture_filtering_mode = %d\n", texture_filter);
    std::fprintf(f, "dolphin_anti_aliasing = %d\n", antialiasing);
    std::fprintf(f, "dolphin_enhance_output_resampling = %d\n", resampling);
    std::fprintf(f, "dolphin_shader_compilation_mode = %d\n", shader_mode);
    std::fprintf(f, "dolphin_texture_cache_accuracy = %s\n", kTextureCache[std::clamp(texture_cache, 0, 2)]);
    std::fprintf(f, "dolphin_pixel_lighting = %s\n", on_off(pixel_lighting));
    std::fprintf(f, "dolphin_disable_fog = %s\n", on_off(disable_fog));
    std::fprintf(f, "dolphin_crop_overscan = %s\n", on_off(crop_overscan));
    std::fprintf(f, "dolphin_load_custom_textures = %s\n", on_off(custom_textures));
    std::fprintf(f, "dolphin_cache_custom_textures = %s\n", on_off(custom_textures));
    std::fprintf(f, "dolphin_skip_dupe_frames = %s\n", on_off(skip_dupes));
    std::fprintf(f, "dolphin_enable_rumble = %s\n", on_off(rumble));
    std::fprintf(f, "dolphin_cpu_clock_rate = %s\n", kCpuClocks[std::clamp(cpu_clock, 0, 9)]);
    std::fprintf(f, "dolphin_main_cpu_thread = %s\n", on_off(dual_core));
    std::fprintf(f, "dolphin_fast_disc_speed = %s\n", on_off(fast_disc));
    std::fprintf(f, "dolphin_cheats_enabled = %s\n", on_off(cheats));
    std::fprintf(f, "dolphin_language = %d\n", kLanguages[std::clamp(language, 0, 9)]);
    std::fprintf(f, "dolphin_progressive_scan = %s\n", on_off(progressive));
    std::fclose(f);
    return true;
}
} // namespace porpoise
