/* Porpoise - settings: Porpoise's own, and the Dolphin options it manages.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "porpoise_pad.hpp"

namespace porpoise
{
struct Settings
{
    /* Games */
    bool auto_search = true;     /* look in the usual folders and on USB drives */
    bool download_covers = true; /* box art from GameTDB */
    bool download_info = true;   /* descriptions, developers, dates from GameTDB */
    std::vector<std::string> folders; /* folders the player added */

    /* Video */
    int resolution = 3;      /* dolphin_efb_scale 1..6: 1080p; 4x and up are experimental */
    bool widescreen = false; /* dolphin_widescreen_hack */
    int aspect = 3;          /* dolphin_aspect_ratio 0..3: Auto, 16:9, 4:3, Stretch */
    int anisotropy = 2;      /* dolphin_max_anisotropy 0..4 (1x..16x) */
    int texture_filter = 0;  /* dolphin_force_texture_filtering_mode 0..2 */
    int antialiasing = 0;    /* dolphin_anti_aliasing 0..6 */
    int resampling = 0;      /* dolphin_enhance_output_resampling 0..6 */
    /* Porpoise's own picture, drawn over the game's: how it is scaled and
     * filtered on the way to the TV, and what fills the bars beside a 4:3
     * picture. */
    int screen_filter = 0;   /* Smooth, Sharp, Sharpen, CRT, Arcade CRT, VHS, Soft VHS, 8-bit, Pocket */
    int filter_strength = 6; /* 1..10 */
    std::string border;      /* "" none, or a border's name (built in, or a PNG in /data/porpoise/borders) */
    bool fps_overlay = false;
    /* Graphics (advanced) */
    int shader_mode = 2;          /* dolphin_shader_compilation_mode 0..3: async ubershaders */
    int texture_cache = 0;        /* dolphin_texture_cache_accuracy: Fast, Middle, Safe */
    bool pixel_lighting = false;  /* dolphin_pixel_lighting */
    bool disable_fog = false;     /* dolphin_disable_fog */
    bool crop_overscan = false;   /* dolphin_crop_overscan */
    bool custom_textures = false; /* dolphin_load_custom_textures (+ prefetch) */
    bool skip_dupes = true;       /* dolphin_skip_dupe_frames */
    bool fast_states = true;      /* save states leave out Dolphin's GPU texture cache (GFX.ini) */
    /* Audio */
    int volume = 10; /* 0..10 */
    bool muted = false;
    /* Porpoise's own sound (the menus, not the games) */
    bool menu_music = true;
    int music_volume = 4; /* 0..10: a quiet bed by default */
    bool menu_sounds = true;
    int sounds_volume = 8;
    /* Controls */
    /* 0 GameCube, 1 PlayStation, 2..5 the player's own layouts 1..4. */
    int button_layout = 0;
    /* The player's own layouts, kept with the global settings (a game only
     * picks one): a porpoise::pad::Control for each GameCube input, in
     * porpoise::pad::GcInput order. Each starts as the GameCube layout. */
    static constexpr int kPresets = 4;
    int presets[kPresets][porpoise::pad::GcCount] = {
        {0, 2, 1, 3, 5, 6, 7, 10, 12, 13, 14, 15}, {0, 2, 1, 3, 5, 6, 7, 10, 12, 13, 14, 15},
        {0, 2, 1, 3, 5, 6, 7, 10, 12, 13, 14, 15}, {0, 2, 1, 3, 5, 6, 7, 10, 12, 13, 14, 15}};
    bool rumble = true;
    /* System */
    int cpu_clock = 5;       /* index into the clock list (50% .. 300%): 100% */
    bool dual_core = true;   /* dolphin_main_cpu_thread */
    bool fast_disc = false;  /* dolphin_fast_disc_speed */
    bool cheats = false;     /* dolphin_cheats_enabled */
    int language = 0;        /* 0..9: English first */
    bool progressive = true; /* dolphin_progressive_scan */
    /* Interface */
    bool reduced_motion = false;
    bool large_text = false;
    /* Dolphin's own per-game settings set by Porpoise, as "dolphin.<Section>.<Key>"
     * and its value (e.g. dolphin.Video_Hacks.EFBToTextureEnable = True). Kept
     * in a game's settings file only, and written for Dolphin before the game
     * starts (write_dolphin_game_ini). */
    std::vector<std::pair<std::string, std::string>> dolphin;
    int ui_language = 0; /* 0 follows the PS5, else porpoise::ui::Language + 1 (see ui_i18n.hpp) */

    /* Reads key = value lines. overlay: only the keys present change (a
     * game's own settings on top of the global ones). */
    bool load(const std::string &path, bool overlay = false);
    bool save(const std::string &path) const;
    /* Writes only the given keys (a game's own settings). */
    bool save_keys(const std::string &path, const std::vector<std::string> &keys) const;
    /* Keys present in a settings file. */
    static std::vector<std::string> keys_in(const std::string &path);
    /* Writes the Dolphin keys into options.ini, keeping any other lines. */
    bool write_core_options(const std::string &options_ini) const;
    /* Back to how Porpoise ships, keeping the game folders. */
    void reset();
    /* A game's settings file from 1.0, brought up to date: its own buttons
     * become one of the player's layouts in global (true when global changed
     * and must be saved), Smooth / Sharp becomes the screen filter. */
    static bool migrate_game_file(const std::string &path, Settings &global);
    /* Drops a dolphin.* setting (Dolphin's own value applies again). */
    void forget(const std::string &key);
    /* Writes Dolphin's per-game file (User/GameSettings/<ID>.ini) from the
     * dolphin.* settings; removes it when there are none. */
    bool write_dolphin_game_ini(const std::string &path) const;
    /* One setting by its key, as in the file (false: no such setting). */
    bool set(const std::string &key, const std::string &value);
    /* Its value as the file has it ("" for no such setting). */
    std::string get(const std::string &key) const;
    /* The buttons in effect: a ready-made layout, or one of the player's own. */
    porpoise::pad::Mapping mapping() const;
    /* The player's own layout 0..3 being used, or -1 for a ready-made one. */
    int preset_in_use() const { return button_layout >= 2 ? button_layout - 2 : -1; }
    /* The Dolphin options these settings make, as key and value. */
    std::vector<std::pair<std::string, std::string>> core_options() const;
};
} // namespace porpoise
