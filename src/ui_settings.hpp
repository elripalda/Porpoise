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
    int button_layout = 1; /* PlayStation by default (1.1 build 14) */
    /* The player's own layouts, kept with the global settings (a game only
     * picks one): a porpoise::pad::Control for each GameCube input, in
     * porpoise::pad::GcInput order. Each starts as the GameCube layout. */
    static constexpr int kPresets = 4;
    int presets[kPresets][porpoise::pad::GcCount] = {
        {0, 2, 1, 3, 5, 6, 7, 10, 12, 13, 14, 15}, {0, 2, 1, 3, 5, 6, 7, 10, 12, 13, 14, 15},
        {0, 2, 1, 3, 5, 6, 7, 10, 12, 13, 14, 15}, {0, 2, 1, 3, 5, 6, 7, 10, 12, 13, 14, 15}};
    bool rumble = true;
    /* Wii games: how the DualSense plays the Wii Remote (porpoise::pad::WiiConfig). */
    int wii_controller = 0; /* Remote + Nunchuk, Remote, sideways, Classic, two controllers */
    int wii_pointer = 0;    /* gyro, touch pad, right stick */
    int wii_speed = 5;      /* the gyro pointer's speed, 1..10; 0: as measured for the screen (wii_screen_x/y) */
    int wii_grip = 0;       /* auto; both hands; upright, trigger to the TV; upright, facing you (pad::WiiGrip) */
    bool wii_motion = true; /* the DualSense's motion is the Remote's */
    bool wii_shake = true;  /* a flick shakes the Remote */
    bool wii_invert_x = false, wii_invert_y = false;
    /* The Wii Remote setup: the screen's half-width and half-height as angles
     * from where the player sits, in tenths of a degree (0: not measured); and
     * whether to offer the setup before each Wii game. */
    int wii_screen_x = 0, wii_screen_y = 0;
    bool wii_setup_ask = true;
    bool wii_setup_advanced = false; /* the setup's extra pages: size and distance, fine-tuning, presets */
    int wii_smooth = 1;              /* pointer smoothing: off, light, medium, strong */
    int wii_reach = 100;             /* percent */
    int wii_size = 27, wii_distance = 30; /* the screen's diagonal (inches) and how far away (tenths of feet) */
    /* Wii presets: a whole Wii Remote set-up kept under a name (global only). */
    static constexpr int kWiiPresets = 4;
    struct WiiPreset
    {
        bool used = false;
        int name = 0; /* index into wii_preset_names() */
        int controller = 0, grip = 0, pointer = 0, speed = 5, screen_x = 0, screen_y = 0, smooth = 1, reach = 100;
    };
    WiiPreset wii_presets[kWiiPresets];
    int wii_preset = 0; /* the one in use: 0 none, 1..4 */
    void save_wii_preset(int slot, int name);
    bool use_wii_preset(int slot);
    bool motion_readout = false; /* in a game: the controller's motion on screen (developer options) */
    bool debug_logs = true;      /* test builds: /data/porpoise/debug */
    /* Developer options: unlocked in About, for tuning the Wii Remote. Without
     * them the readout, inverted pointer and motion logs are off. */
    bool developer = false;
    bool motion_logs = true; /* a CSV of the controller's motion per Wii game (developer options) */
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
    int ui_theme = 0; /* the look: 0 Porpoise, 1 Revolution (a bright grid of tiles and a pointer) */
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
    /* The Wii Remote's settings, for a Wii game (active) or not. */
    porpoise::pad::WiiConfig wii_config(bool active) const;
    static const std::vector<std::string> &wii_preset_names();
    /* The buttons in effect: a ready-made layout, or one of the player's own. */
    porpoise::pad::Mapping mapping() const;
    /* The player's own layout 0..3 being used, or -1 for a ready-made one. */
    int preset_in_use() const { return button_layout >= 2 ? button_layout - 2 : -1; }
    /* The Dolphin options these settings make, as key and value. */
    std::vector<std::pair<std::string, std::string>> core_options() const;
};
} // namespace porpoise
