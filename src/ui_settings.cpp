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
    {"screen_filter", &Settings::screen_filter, nullptr, 0, 8},
    {"filter_strength", &Settings::filter_strength, nullptr, 1, 10},
    {"fps_overlay", nullptr, &Settings::fps_overlay, 0, 1},
    {"shader_mode", &Settings::shader_mode, nullptr, 0, 3},
    {"texture_cache", &Settings::texture_cache, nullptr, 0, 2},
    {"pixel_lighting", nullptr, &Settings::pixel_lighting, 0, 1},
    {"disable_fog", nullptr, &Settings::disable_fog, 0, 1},
    {"crop_overscan", nullptr, &Settings::crop_overscan, 0, 1},
    {"custom_textures", nullptr, &Settings::custom_textures, 0, 1},
    {"skip_dupes", nullptr, &Settings::skip_dupes, 0, 1},
    {"fast_states", nullptr, &Settings::fast_states, 0, 1},
    {"volume", &Settings::volume, nullptr, 0, 10},
    {"muted", nullptr, &Settings::muted, 0, 1},
    {"menu_music", nullptr, &Settings::menu_music, 0, 1},
    {"music_volume", &Settings::music_volume, nullptr, 0, 10},
    {"menu_sounds", nullptr, &Settings::menu_sounds, 0, 1},
    {"sounds_volume", &Settings::sounds_volume, nullptr, 0, 10},
    {"button_layout", &Settings::button_layout, nullptr, 0, 5},
    {"rumble", nullptr, &Settings::rumble, 0, 1},
    {"cpu_clock", &Settings::cpu_clock, nullptr, 0, 9},
    {"dual_core", nullptr, &Settings::dual_core, 0, 1},
    {"fast_disc", nullptr, &Settings::fast_disc, 0, 1},
    {"cheats", nullptr, &Settings::cheats, 0, 1},
    {"language", &Settings::language, nullptr, 0, 9},
    {"progressive", nullptr, &Settings::progressive, 0, 1},
    {"reduced_motion", nullptr, &Settings::reduced_motion, 0, 1},
    {"large_text", nullptr, &Settings::large_text, 0, 1},
    {"ui_language", &Settings::ui_language, nullptr, 0, 6},
};

/* Settings kept as text rather than numbers. */
bool is_dolphin_key(const std::string &key)
{
    /* dolphin.<Section>.<Key>: one of Dolphin's own per-game settings. */
    return key.rfind("dolphin.", 0) == 0 && key.find('.', 8) != std::string::npos;
}

bool is_text_key(const std::string &key)
{
    return key == "border" || is_dolphin_key(key);
}

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
    int version = 0;
    int legacy_sharp = -1;   /* 1.0's Smooth / Sharp switch */
    bool saw_filter = false;
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
            version = std::atoi(v.c_str());
            continue;
        }
        if (k == "folder")
        {
            if (!overlay && !v.empty() && std::find(folders.begin(), folders.end(), v) == folders.end())
                folders.push_back(v);
            continue;
        }
        if (k == "border")
        {
            border = v;
            continue;
        }
        if (is_dolphin_key(k))
        {
            set(k, v);
            continue;
        }
        if (k.size() == 7 && k.rfind("layout", 0) == 0 && k[6] >= '1' && k[6] <= '4')
        {
            /* One of the player's own layouts: twelve controls. */
            if (overlay)
                continue;
            int *row = presets[k[6] - '1'];
            const char *c = v.c_str();
            for (int i = 0; i < porpoise::pad::GcCount && *c; ++i)
            {
                char *end = nullptr;
                const long n = std::strtol(c, &end, 10);
                if (end == c)
                    break;
                row[i] = std::clamp(int(n), 0, porpoise::pad::CtlCount - 1);
                c = end;
            }
            continue;
        }
        if (k.rfind("map_", 0) == 0)
        {
            /* 1.0 kept a single custom layout in map_* keys: it becomes the
             * player's layout 1. */
            static const char *const kNames[porpoise::pad::GcCount] = {
                "map_a", "map_b", "map_x", "map_y", "map_z", "map_l", "map_r", "map_start", "map_up",
                "map_down", "map_left", "map_right"};
            if (!overlay)
                for (int i = 0; i < porpoise::pad::GcCount; ++i)
                    if (k == kNames[i])
                        presets[0][i] = std::clamp(std::atoi(v.c_str()), 0, porpoise::pad::CtlCount - 1);
            continue;
        }
        if (k == "sharp")
        {
            legacy_sharp = as_bool(v) ? 1 : 0;
            continue;
        }
        if (k == "screen_filter")
            saw_filter = true;
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
    /* Before 11: shaders compiled synchronously by default, which stalls a
     * game each time it draws something new; asynchronous ubershaders are the
     * default now. And the old two-way layout switch is gone: everyone starts
     * on the GameCube layout, PlayStation and Custom are a choice away. */
    if (!overlay && version < 11 && shader_mode == 0)
        shader_mode = 2;
    /* 1.1: Smooth / Sharp became the first two screen filters. */
    if (legacy_sharp >= 0 && !saw_filter)
        screen_filter = legacy_sharp;
    return true;
}

bool Settings::save(const std::string &path) const
{
    const std::string tmp = path + ".part";
    std::FILE *f = std::fopen(tmp.c_str(), "w");
    if (!f)
        return false;
    std::fprintf(f, "# Porpoise settings (written by the Settings screen)\nsettings_version = 12\n");
    for (const Field &fd : kFields)
        write_field(f, *this, fd);
    std::fprintf(f, "border = %s\n", border.c_str());
    for (int p = 0; p < kPresets; ++p)
    {
        std::fprintf(f, "layout%d =", p + 1);
        for (int i = 0; i < porpoise::pad::GcCount; ++i)
            std::fprintf(f, " %d", presets[p][i]);
        std::fprintf(f, "\n");
    }
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
    {
        if (const Field *fd = field(k))
            write_field(f, *this, *fd);
        else if (k == "border")
            std::fprintf(f, "border = %s\n", border.c_str());
        else if (is_dolphin_key(k) && !get(k).empty())
            std::fprintf(f, "%s = %s\n", k.c_str(), get(k).c_str());
    }
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
        if ((field(k) || is_text_key(k)) && std::find(keys.begin(), keys.end(), k) == keys.end())
            keys.push_back(k);
    }
    std::fclose(f);
    return keys;
}

void Settings::forget(const std::string &key)
{
    dolphin.erase(std::remove_if(dolphin.begin(), dolphin.end(),
                                 [&](const std::pair<std::string, std::string> &kv) { return kv.first == key; }),
                  dolphin.end());
}

bool Settings::write_dolphin_game_ini(const std::string &path) const
{
    /* Dolphin's own per-game file in its user folder (its LocalGame layer),
     * over the shipped Sys/GameSettings: Porpoise writes it before each game
     * from that game's dolphin.* settings, and removes it when there are none,
     * as long as it is Porpoise's. */
    static const char kMark[] = "# Written by Porpoise";
    bool ours = true;
    if (std::FILE *f = std::fopen(path.c_str(), "r"))
    {
        char first[64] = {0};
        ours = std::fgets(first, sizeof first, f) && std::strncmp(first, kMark, sizeof kMark - 1) == 0;
        std::fclose(f);
    }
    else if (dolphin.empty())
        return true;
    if (!ours)
        return false; /* the player's own file: left alone */
    if (dolphin.empty())
        return std::remove(path.c_str()) == 0;
    std::vector<std::string> sections;
    for (const auto &kv : dolphin)
    {
        const std::string sec = kv.first.substr(8, kv.first.find('.', 8) - 8);
        if (std::find(sections.begin(), sections.end(), sec) == sections.end())
            sections.push_back(sec);
    }
    std::FILE *f = std::fopen(path.c_str(), "w");
    if (!f)
        return false;
    std::fprintf(f, "%s from this game's settings; changes here are replaced.\n", kMark);
    for (const std::string &sec : sections)
    {
        std::fprintf(f, "\n[%s]\n", sec.c_str());
        for (const auto &kv : dolphin)
            if (kv.first.compare(8, sec.size() + 1, sec + ".") == 0)
                std::fprintf(f, "%s = %s\n", kv.first.substr(9 + sec.size()).c_str(), kv.second.c_str());
    }
    std::fclose(f);
    return true;
}

bool Settings::migrate_game_file(const std::string &path, Settings &global)
{
    /* 1.0 kept a game's own buttons as map_* lines with button_layout = 2
     * (Custom), and its Smooth / Sharp choice as sharp. 1.1 has the player's
     * four layouts in the global settings, and screen_filter. */
    std::FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
        return false;
    static const char *const kNames[porpoise::pad::GcCount] = {
        "map_a", "map_b", "map_x", "map_y", "map_z", "map_l", "map_r", "map_start", "map_up",
        "map_down", "map_left", "map_right"};
    std::vector<std::string> keep;
    int map[porpoise::pad::GcCount];
    const porpoise::pad::Mapping base = porpoise::pad::preset(porpoise::pad::LayoutGameCube);
    for (int i = 0; i < porpoise::pad::GcCount; ++i)
        map[i] = base.control[i];
    bool old = false, saw_map = false, custom = false, saw_filter = false;
    int sharp = -1;
    char line[1024];
    while (std::fgets(line, sizeof line, f))
    {
        const std::string t = trim(line);
        const auto eq = t.find('=');
        const std::string k = eq == std::string::npos ? "" : trim(t.substr(0, eq));
        const std::string v = eq == std::string::npos ? "" : trim(t.substr(eq + 1));
        bool drop = false;
        for (int i = 0; i < porpoise::pad::GcCount; ++i)
            if (k == kNames[i])
            {
                map[i] = std::clamp(std::atoi(v.c_str()), 0, porpoise::pad::CtlCount - 1);
                saw_map = drop = old = true;
            }
        if (k == "sharp")
        {
            sharp = as_bool(v) ? 1 : 0;
            drop = old = true;
        }
        if (k == "screen_filter")
            saw_filter = true;
        if (k == "button_layout" && std::atoi(v.c_str()) == 2)
        {
            custom = true;
            drop = true; /* written again below */
        }
        if (!drop)
            keep.push_back(t);
    }
    std::fclose(f);
    if (!old)
        return false;
    bool global_changed = false;
    if (custom && saw_map)
    {
        /* The same layout among the player's, or a free one (still the
         * GameCube layout it started as), else the last. */
        int use = -1;
        for (int p = 0; p < kPresets && use < 0; ++p)
            if (std::equal(map, map + porpoise::pad::GcCount, global.presets[p]))
                use = p;
        for (int p = 1; p < kPresets && use < 0; ++p)
        {
            bool fresh = true;
            for (int i = 0; i < porpoise::pad::GcCount; ++i)
                fresh &= global.presets[p][i] == base.control[i];
            if (fresh)
                use = p;
        }
        if (use < 0)
            use = kPresets - 1;
        if (!std::equal(map, map + porpoise::pad::GcCount, global.presets[use]))
        {
            std::copy(map, map + porpoise::pad::GcCount, global.presets[use]);
            global_changed = true;
        }
        keep.push_back("button_layout = " + std::to_string(2 + use));
    }
    else if (custom)
        keep.push_back("button_layout = 2");
    if (sharp >= 0 && !saw_filter)
        keep.push_back(std::string("screen_filter = ") + (sharp ? "1" : "0"));
    const std::string tmp = path + ".part";
    if (std::FILE *o = std::fopen(tmp.c_str(), "w"))
    {
        for (const std::string &l : keep)
            if (!l.empty())
                std::fprintf(o, "%s\n", l.c_str());
        std::fclose(o);
        std::rename(tmp.c_str(), path.c_str());
    }
    return global_changed;
}

bool Settings::set(const std::string &key, const std::string &value)
{
    if (key == "border")
    {
        border = value;
        return true;
    }
    if (is_dolphin_key(key))
    {
        for (auto &kv : dolphin)
            if (kv.first == key)
            {
                kv.second = value;
                return true;
            }
        dolphin.push_back({key, value});
        return true;
    }
    const Field *fd = field(key);
    if (!fd)
        return false;
    if (fd->i)
        this->*fd->i = std::clamp(std::atoi(value.c_str()), fd->min, fd->max);
    else
        this->*fd->b = as_bool(value);
    return true;
}

std::string Settings::get(const std::string &key) const
{
    if (key == "border")
        return border;
    if (is_dolphin_key(key))
    {
        for (const auto &kv : dolphin)
            if (kv.first == key)
                return kv.second;
        return "";
    }
    const Field *fd = field(key);
    if (!fd)
        return "";
    return fd->i ? std::to_string(this->*fd->i) : (this->*fd->b ? "1" : "0");
}

porpoise::pad::Mapping Settings::mapping() const
{
    const int own = preset_in_use();
    if (own < 0)
        return porpoise::pad::preset(button_layout);
    porpoise::pad::Mapping m{};
    for (int gc = 0; gc < porpoise::pad::GcCount; ++gc)
        m.control[gc] = static_cast<std::int8_t>(std::clamp(presets[own][gc], 0, porpoise::pad::CtlCount - 1));
    return m;
}

void Settings::reset()
{
    /* The game folders and the player's own button layouts stay. */
    const std::vector<std::string> keep = folders;
    int own[kPresets][porpoise::pad::GcCount];
    std::memcpy(own, presets, sizeof own);
    *this = Settings{};
    folders = keep;
    std::memcpy(presets, own, sizeof own);
}

std::vector<std::pair<std::string, std::string>> Settings::core_options() const
{
    return {
        {"dolphin_efb_scale", std::to_string(resolution)},
        {"dolphin_widescreen_hack", on_off(widescreen)},
        {"dolphin_aspect_ratio", std::to_string(aspect)},
        {"dolphin_max_anisotropy", std::to_string(anisotropy)},
        {"dolphin_force_texture_filtering_mode", std::to_string(texture_filter)},
        {"dolphin_anti_aliasing", std::to_string(antialiasing)},
        {"dolphin_enhance_output_resampling", std::to_string(resampling)},
        {"dolphin_shader_compilation_mode", std::to_string(shader_mode)},
        {"dolphin_texture_cache_accuracy", kTextureCache[std::clamp(texture_cache, 0, 2)]},
        {"dolphin_pixel_lighting", on_off(pixel_lighting)},
        {"dolphin_disable_fog", on_off(disable_fog)},
        {"dolphin_crop_overscan", on_off(crop_overscan)},
        {"dolphin_load_custom_textures", on_off(custom_textures)},
        {"dolphin_cache_custom_textures", on_off(custom_textures)},
        {"dolphin_skip_dupe_frames", on_off(skip_dupes)},
        {"dolphin_enable_rumble", on_off(rumble)},
        {"dolphin_cpu_clock_rate", kCpuClocks[std::clamp(cpu_clock, 0, 9)]},
        {"dolphin_main_cpu_thread", on_off(dual_core)},
        {"dolphin_fast_disc_speed", on_off(fast_disc)},
        {"dolphin_cheats_enabled", on_off(cheats)},
        {"dolphin_language", std::to_string(kLanguages[std::clamp(language, 0, 9)])},
        {"dolphin_progressive_scan", on_off(progressive)},
    };
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
    for (const auto &[key, value] : core_options())
        std::fprintf(f, "%s = %s\n", key.c_str(), value.c_str());
    std::fclose(f);
    return true;
}
} // namespace porpoise
