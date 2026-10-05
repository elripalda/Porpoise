/* Porpoise UI - Settings, a game's own settings, and the game folder browser.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings is a rail of sections on the left and the rows of one section on
 * the right. Focus starts on the rail: up and down pick a section, Right or
 * Cross go into it, Circle comes back out. A game's own settings use the same
 * screen with the global values underneath and its changes on top. */
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>

#include "porpoise_pad.hpp"
#include "porpoise_update.hpp"
#include "ui_app.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"
#include "ui_recommend.hpp"
#include "ui_setups.hpp"
#include "title_threads.hpp"

#if defined(__has_include)
#if __has_include("title_build_identity.h")
#include "title_build_identity.h"
#endif
#endif

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

/* "2.0 beta 1", "1.5.1", "1.1 (build 15)": how a release is named to the player. */
std::string App::version_label(const std::string &tag, int build)
{
    int major = 0, minor = 0, patch = 0, beta = 0;
    if (!porpoise::update::tag_version(tag, major, minor, patch, beta))
        return tag;
    std::string shown = std::to_string(major) + "." + std::to_string(minor);
    if (patch)
        shown += "." + std::to_string(patch);
    if (beta)
        shown += " Beta " + std::to_string(beta);
    if (build > 0)
        shown += " (build " + std::to_string(build) + ")";
    return shown;
}

namespace
{
/* "Quick resume (beta)": the label without its marker, which is drawn as a
 * BETA badge beside it instead. */
std::string beta_label(const char *label, bool &beta)
{
    std::string text = label;
    const std::string mark = " (beta)";
    beta = text.size() > mark.size() && text.compare(text.size() - mark.size(), mark.size(), mark) == 0;
    if (beta)
        text.resize(text.size() - mark.size());
    return tr(text);
}

bool copy_file(const std::string &from, const std::string &to, std::size_t keep_tail = 0)
{
    std::FILE *in = std::fopen(from.c_str(), "rb");
    if (!in)
        return false;
    /* A long log keeps its end, where the problem is. */
    if (keep_tail && std::fseek(in, 0, SEEK_END) == 0)
    {
        const long size = std::ftell(in);
        std::fseek(in, size > long(keep_tail) ? size - long(keep_tail) : 0, SEEK_SET);
    }
    std::FILE *out = std::fopen(to.c_str(), "wb");
    bool ok = out != nullptr;
    char buf[65536];
    std::size_t n;
    while (ok && (n = std::fread(buf, 1, sizeof buf, in)) > 0)
        ok = std::fwrite(buf, 1, n, out) == n;
    std::fclose(in);
    if (out)
        ok = std::fclose(out) == 0 && ok;
    return ok;
}
} // namespace

void App::show_setup_check(bool first_start)
{
    std::vector<std::pair<bool, std::string>> checks;
    auto line = [&](bool ok, const std::string &what) { checks.emplace_back(ok, what); };
    if (sandboxed_)
        line(false, tr("Porpoise is inside the app sandbox: it can't see /data or USB drives. Turn on Legacy Command "
                       "Server in etaHEN, add PPSA99764 to OnionHEN's exact_title_ids, or run a Lapy daemon, then "
                       "open Porpoise again."));
    else
        line(true, tr("Porpoise can see /data and USB drives."));
    std::vector<std::string> drives;
    for (int i = 0; i < 8; ++i)
    {
        struct stat st;
        const std::string root = "/mnt/usb" + std::to_string(i);
        if (stat(root.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            drives.push_back("USB " + std::to_string(i + 1));
    }
    for (int i = 0; i < 2; ++i)
    {
        struct stat st;
        if (stat(("/mnt/ext" + std::to_string(i)).c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            drives.push_back(tr("extended storage"));
    }
    if (!sandboxed_)
    {
        std::string list;
        for (const std::string &d : drives)
            list += (list.empty() ? "" : ", ") + d;
        line(true, drives.empty() ? tr("No USB drive found (games on one: format it exFAT).")
                                  : trf("Drives: {list}.", {{"list", list}}));
    }
    const int games = int(lib_->games().size());
    if (games > 0)
        line(true, plural(games, "1 game found.", "{n} games found."));
    else
        line(false, tr("No games yet. Put them in /data/porpoise/games or on a USB drive, or add a folder in "
                       "Settings > Games."));
    line(settings_->download_covers, settings_->download_covers ? tr("Covers download while the console is online.")
                                                                 : tr("Cover downloads are off (Settings > Games)."));
    open_dialog(DialogKind::Info, first_start ? tr("Welcome to Porpoise") : tr("Your setup"), "", "");
    dialog_.checks = std::move(checks);
}

std::string App::save_report(std::string &usb)
{
    usb.clear();
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tm);
    const std::string name = std::string("report-") + stamp;
    const std::string dir = data_dir_ + "/reports/" + name;
    mkdir((data_dir_ + "/reports").c_str(), 0777);
    if (mkdir(dir.c_str(), 0777) != 0)
        return "";
    /* What happened (the newest 2 MB of each log), the settings, and a note
     * of what this is. */
    copy_file("/app0/trace.txt", dir + "/trace.txt", 2u << 20);
    copy_file("/app0/porpoise/core.log", dir + "/core.log", 2u << 20);
    copy_file(settings_path_, dir + "/settings.ini");
    if (std::FILE *f = std::fopen((dir + "/about.txt").c_str(), "w"))
    {
        std::fprintf(f, "Porpoise %s\n", build_label().c_str());
        std::fprintf(f, "Saved %s\n", stamp);
        std::fprintf(f, "Games in the library: %d\n", int(lib_->games().size()));
        if (selected_ >= 0 && selected_ < int(lib_->games().size()))
        {
            const Game &g = lib_->games()[std::size_t(selected_)];
            std::fprintf(f, "Selected game: %s (%s, %s)\n", g.title.c_str(), g.id.c_str(), g.platform.c_str());
        }
        std::fprintf(f, "Player data: %s\n", data_dir_.c_str());
        std::fclose(f);
    }
    /* A copy on a USB drive, for a computer. */
    for (int i = 0; i < 8; ++i)
    {
        const std::string root = "/mnt/usb" + std::to_string(i);
        struct stat st;
        if (stat(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
            continue;
        const std::string to = root + "/porpoise-" + name;
        if (mkdir(to.c_str(), 0777) != 0)
            continue;
        bool any = false;
        for (const char *file : {"trace.txt", "core.log", "settings.ini", "about.txt"})
            any |= copy_file(dir + "/" + file, to + "/" + file);
        if (any)
        {
            usb = "porpoise-" + name;
            break;
        }
    }
    return dir;
}

void App::set_versions(const std::vector<std::string> &tags, const std::vector<int> &builds,
                       const std::vector<bool> &betas, const std::vector<std::size_t> &sizes)
{
    const std::string was = version_pick_ >= 0 && version_pick_ < int(version_tags_.size())
                                ? version_tags_[std::size_t(version_pick_)]
                                : "";
    version_tags_ = tags;
    version_builds_ = builds;
    version_sizes_ = sizes;
    version_labels_.clear();
    version_pick_ = 0;
    bool latest_marked = false;
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
        std::string label = version_label(tags[i], builds[i]);
        int major = 0, minor = 0, patch = 0, beta = 0;
        porpoise::update::tag_version(tags[i], major, minor, patch, beta);
        if (porpoise::update::compare_versions(major, minor, patch, beta, builds[i], kVersionMajor, kVersionMinor,
                                               kVersionPatch, kVersionBeta, kBuild) == 0)
            label += "  \xE2\x80\xA2  " + tr("installed");
        else if (betas[i])
            label += "  \xE2\x80\xA2  " + tr("beta");
        else if (!latest_marked)
            label += "  \xE2\x80\xA2  " + tr("latest");
        if (!betas[i])
            latest_marked = true;
        version_labels_.push_back(label);
        if (tags[i] == was)
            version_pick_ = int(i);
    }
    if (screen_ != Screen::GameSettings && screen_ != Screen::Mapping && !map_in_game_)
        build_settings();
}

void App::set_latest_release(const std::string &tag, const std::string &url, std::size_t zip_size, int build)
{
    latest_size_ = zip_size;
    /* "v1.2", "1.2.1" or "v2.0-beta.2": newer than this build? */
    int major = 0, minor = 0, patch = 0, beta = 0;
    if (!porpoise::update::tag_version(tag, major, minor, patch, beta))
        return;
    const bool newer = porpoise::update::compare_versions(major, minor, patch, beta, build, kVersionMajor,
                                                          kVersionMinor, kVersionPatch, kVersionBeta, kBuild) > 0;
    const std::string shown = version_label(tag, build);
    latest_version_ = newer ? shown : "";
    latest_url_ = newer ? url : "";
    /* Only while the global settings are what rows_ holds: a game's settings
     * or the mapping screen keep theirs (About is built afresh later). */
    if (screen_ != Screen::GameSettings && screen_ != Screen::Mapping && !map_in_game_)
        build_settings();
}

std::string App::update_row_value() const
{
    switch (update_phase_)
    {
    case 1: return tr("Checking\xE2\x80\xA6");
    case 2:
    case 3:
    case 4:
    case 7: return tr("Updating\xE2\x80\xA6");
    default: break;
    }
    if (update_available())
        return trf("Install {version}", {{"version", latest_version_}});
    return tr("Check now");
}

void App::set_update_progress(int phase, std::size_t done, std::size_t total, const std::string &error)
{
    if (phase == update_phase_ && done == update_done_ && total == update_total_)
        return;
    const int was = update_phase_;
    update_phase_ = phase;
    update_done_ = done;
    update_total_ = total;
    update_error_ = error;
    if ((was == 2 || was == 3) && phase == 5)
        update_failed_ = true;
    if (phase == 4 && was != 4)
        update_done_time_ = time_;
    if (was == 1 && phase == 5)
    {
        update_note_ = tr(error);
        update_note_time_ = time_;
    }
    if (was == 1 && phase == 6)
    {
        update_note_ = update_available() ? trf("Porpoise {version} is out.", {{"version", latest_version_}})
                                          : tr("This is the newest Porpoise.");
        update_note_time_ = time_;
    }
}

std::string App::build_label() const
{
    std::string label = kVersion;
    if (kBuild > 0)
        label += " (build " + std::to_string(kBuild) + ")";
#ifdef PS5_RETROARCH_BUILD_ID
    const std::string id = PS5_RETROARCH_BUILD_ID;
    const auto colon = id.rfind(": ");
    if (colon != std::string::npos && id.size() >= colon + 10)
        label += "  (" + id.substr(colon + 2, 8) + ")";
#endif
    return label;
}

/* A game's own settings, counted as the player sees them: its custom buttons
 * are one change, not one per button. */
long long App::change_count() const
{
    long long n = 0;
    bool buttons = false;
    for (const std::string &k : game_keys_)
    {
        if (k.rfind("map_", 0) == 0 || k == "button_layout")
            buttons = true;
        else
            ++n;
    }
    return n + (buttons ? 1 : 0);
}

std::string App::game_settings_path(const Game &g) const
{
    return data_dir_ + "/game-settings/" + Library::settings_key_of(g) + ".ini";
}

/* ---- the rows ------------------------------------------------------------------------------ */

/* Video, Graphics, Audio, Controls and System: the rows a game can change too. */
void App::add_game_rows(Settings &t, bool per_game)
{
    std::string section;
    auto header = [&](const char *name) {
        SettingRow r;
        r.section = section = name;
        r.header = true;
        rows_.push_back(r);
    };
    auto choice = [&](const char *key, const char *label, const char *help, int *value, int min,
                      std::vector<std::string> values) {
        SettingRow r;
        r.section = section;
        r.key = key;
        r.label = beta_label(label, r.beta); /* "... (beta)": a BETA badge */
        r.help = tr(help);
        r.int_value = value;
        r.min = min;
        for (const std::string &v : values)
            r.values.push_back(tr(v));
        rows_.push_back(r);
    };
    auto toggle = [&](const char *key, const char *label, const char *help, bool *value, const char *off = "Off",
                      const char *on = "On") {
        SettingRow r;
        r.section = section;
        r.key = key;
        r.label = beta_label(label, r.beta);
        r.help = tr(help);
        r.bool_value = value;
        r.values = {tr(off), tr(on)};
        rows_.push_back(r);
    };

    header("Video");
    choice("resolution", "Internal resolution", "How sharp games render. 1080p is the tested default; above it is experimental and can slow games.",
           &t.resolution, 1, {"1x (480p)", "2x (720p)", "3x (1080p)", "4x (1440p) \xE2\x80\xA2 experimental",
            "5x (1800p) \xE2\x80\xA2 experimental", "6x (4K) \xE2\x80\xA2 experimental"});
    toggle("widescreen", "Widescreen hack", "Draws games in 16:9. Some games show glitches at the screen edges.",
           &t.widescreen);
    choice("aspect", "Aspect ratio", "The picture's shape. Auto follows the game; Stretch fills the screen.",
           &t.aspect, 0, {"Auto", "Force 16:9", "Force 4:3", "Stretch to fill"});
    choice("antialiasing", "Anti-aliasing", "Smooths jagged edges. SSAA is the sharpest and the heaviest.",
           &t.antialiasing, 0, {"Off", "2x MSAA", "4x MSAA", "8x MSAA", "2x SSAA", "4x SSAA", "8x SSAA"});
    choice("anisotropy", "Anisotropic filtering", "Sharper textures on floors and walls seen at an angle.",
           &t.anisotropy, 0, {"1x", "2x", "4x", "8x", "16x"});
    choice("texture_filter", "Texture filtering", "Force sharp or smooth textures, or leave it to the game.",
           &t.texture_filter, 0, {"Game's own", "Nearest (sharp)", "Linear (smooth)"});
    choice("resampling", "Output resampling", "How Dolphin scales its picture. Sharp bilinear keeps pixels crisp.",
           &t.resampling, 0,
           {"Default", "Bilinear", "B-Spline", "Mitchell-Netravali", "Catmull-Rom", "Sharp bilinear", "Area sampling"});
    choice("screen_filter", "Screen filter",
           "Porpoise's own filter on the way to the TV: smooth or sharp scaling, sharpening, a CRT, an arcade monitor, a worn or a soft VHS tape, 8-bit pixels, or a green handheld screen.",
           &t.screen_filter, 0, {"Smooth", "Sharp", "Sharpen", "CRT", "Arcade CRT", "VHS", "Soft VHS", "8-bit", "Pocket"});
    choice("filter_strength", "Filter strength", "How strong the screen filter is.", &t.filter_strength, 1,
           {"10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"});
    {
        /* Borders: the built-in ones and the player's own PNGs. */
        SettingRow r;
        r.section = section;
        r.key = "border";
        r.label = tr("Border");
        r.help = tr("Fills the bars beside a 4:3 picture (widescreen off). Add your own 1920x1080 PNGs to "
                    "/data/porpoise/borders.");
        border_names_.clear();
        border_choice_ = 0;
        for (const porpoise::borders::Border &b : porpoise::borders::list())
        {
            if (b.name == t.border)
                border_choice_ = int(border_names_.size());
            border_names_.push_back(b.name);
            r.values.push_back(b.built_in ? tr(b.label) : b.label);
        }
        r.int_value = &border_choice_;
        r.text_value = &t.border;
        rows_.push_back(r);
    }
    toggle("fps_overlay", "FPS overlay", "Shows the frame rate in the corner while you play.", &t.fps_overlay);
    toggle("vsync", "V-Sync",
           "Shows every frame on the TV's own refresh, for the smoothest motion. Off times frames with Porpoise's "
           "own clock instead.",
           &t.vsync);

    if (!per_game)
        choice("output_res", "Output resolution",
               "The picture Porpoise sends to the TV, menus and games alike. 1080p is the quickest; 1440p and 4K are "
               "sharper on a 4K TV with a high internal resolution, and a little slower. Takes effect the next time "
               "Porpoise starts.",
               &t.output_res, 0, {"1080p", "1440p", "4K"});
    if (!per_game)
        add_setup_rows(false);

    header("Graphics");
    choice("shader_mode", "Shader compilation",
           "Ubershaders hide the stutter when a game draws something new, at a GPU cost.", &t.shader_mode, 0,
           {"Synchronous", "Ubershaders", "Async ubershaders", "Async, skip drawing"});
    toggle("threaded_gpu", "Threaded GPU recording (beta)",
           "The graphics driver records Dolphin's drawing on a thread of its own, so Dolphin's video thread spends "
           "less time in the driver. Can speed up demanding games. Turn it off if a game crashes or looks wrong. "
           "Applies the next time a game starts.",
           &t.threaded_gpu);
    choice("texture_cache", "Texture cache accuracy", "Safe fixes some games' text and effects; Fast is quickest.",
           &t.texture_cache, 0, {"Fast", "Middle", "Safe"});
    toggle("pixel_lighting", "Per-pixel lighting", "Smoother lighting on surfaces. A little heavier.",
           &t.pixel_lighting);
    toggle("disable_fog", "Disable fog", "Removes distance fog. Some games use fog for their look.", &t.disable_fog);
    toggle("crop_overscan", "Crop overscan", "Hides the black borders some games draw at the edges.",
           &t.crop_overscan);
    toggle("custom_textures", "Custom textures",
           "Loads HD texture packs. Put each pack's folder, named with the game's ID (like GALE01), in "
           "/data/porpoise/saves/User/Load/Textures. A game's Details say when its pack is found.",
           &t.custom_textures);
    toggle("skip_dupes", "Skip duplicate frames", "Saves work when a game shows the same frame twice.",
           &t.skip_dupes);
    toggle("quick_resume", "Quick resume (beta)",
           "Leaving a game from the in-game menu keeps where you were, and the game picks up right there the next "
           "time you start it. Start over (in the in-game menu) boots it fresh.",
           &t.quick_resume);
    toggle("fast_states", "Fast save states",
           "Leaves the GPU's texture cache out of save states: much quicker to save, and smaller. Turn it off if a "
           "game looks wrong for a moment after loading a state.",
           &t.fast_states);

    header("Audio");
    choice("volume", "Game volume", "Volume of the game's sound.", &t.volume, 0,
           {"0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"});
    toggle("muted", "Mute game", "Silences the game.", &t.muted);
    toggle("dsp_accurate", "Accurate audio (beta)",
           "Dolphin's exact sound chip (LLE) instead of its fast one: fixes missing or wrong sound in a few games, but "
           "needs much more of the processor. Best turned on for one game.",
           &t.dsp_accurate);
    choice("wiimote_speaker", "Wii Remote speaker (beta)",
           "Sounds Wii games play from the Remote's own speaker (a bow, an item box): in the TV's sound, or from "
           "each player's controller, as on a Wii.",
           &t.wiimote_speaker, 0, {"Off", "TV", "Controller"});
    choice("audio_buffer", "Audio buffer (beta)",
           "How much sound is kept ready. Safe holds more, against crackling in demanding games, for a little delay.",
           &t.audio_buffer, 0, {"Low", "Normal", "Safe"});
    toggle("audio_stretch", "Audio stretching (beta)",
           "When a game slows down, its sound slows with it, slightly lower, instead of crackling.",
           &t.audio_stretch);
    toggle("microphone", "Microphone (beta)",
           "The DualSense's microphone as the GameCube Microphone (Mario Party 6 and 7; R3 is its button) and the "
           "Wii Speak.",
           &t.microphone);
    if (!per_game)
    {
        /* Porpoise's own sound: the menus, not the games. */
        toggle("menu_music", "Menu music", "The music that plays in Porpoise's menus.", &settings_->menu_music);
        choice("music_volume", "Music volume", "How loud the menu music plays.", &settings_->music_volume, 0,
               {"0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"});
        toggle("menu_sounds", "Menu sounds", "The sounds of moving through the menus.", &settings_->menu_sounds);
        choice("sounds_volume", "Sounds volume", "How loud the menu sounds play.", &settings_->sounds_volume, 0,
               {"0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"});
    }

    header("Controls");
    {
        std::vector<std::string> layouts = {"GameCube", "PlayStation"};
        for (int i = 1; i <= Settings::kPresets; ++i)
            layouts.push_back(trf("My layout {n}", {{"n", std::to_string(i)}}));
        choice("button_layout", "Button layout",
               "PlayStation: Cross is A, Circle is B. GameCube: Circle is A, Cross is B. My layouts: your own, made "
               "in Customize buttons.",
               &t.button_layout, 0, layouts);
    }
    {
        SettingRow r;
        r.section = section;
        r.label = tr("Customize buttons");
        r.help = tr("Make up to four layouts of your own, on a picture of the controller. Any game can use any of them.");
        r.values = {tr("Edit\xE2\x80\xA6")};
        r.action = kRowMapping;
        rows_.push_back(r);
    }
    toggle("rumble", "Vibration", "Controller rumble.", &t.rumble);
    if (per_game)
        choice("console", "Console",
               "Which console's controls the game gets: as Porpoise detected it, or GameCube or Wii, for a game it "
               "got wrong.",
               &t.console, 0, {"Auto", "GameCube", "Wii"});
    toggle("ff_buttons", "Fast forward buttons",
           "In a game, touch pad + R1 steps fast forward (off, 2x, 4x) and touch pad + R2 fast-forwards while held. "
           "On a Wii Remote, the touch pad's Minus then goes when you let go of it.",
           &t.ff_buttons);
    if (!per_game)
    {
        SettingRow r;
        r.section = section;
        r.key = "players"; /* its value is counted when it is drawn */
        r.label = tr("Controllers");
        r.help = tr("Up to four players. To join, turn on another controller and choose a user for it.");
        r.values = {""};
        rows_.push_back(r);
    }

    header("Wii Remote");
    {
        SettingRow r;
        r.section = section;
        r.label = tr("Wii Remote setup");
        r.help = tr("Beta. Pick the Wii controller, then point at the middle and two corners of your screen: the "
                    "pointer then matches your screen and how far you sit from it.");
        r.values = {t.wii_screen_x > 0 ? tr("Measured") : tr("Start\xE2\x80\xA6")};
        r.action = kRowWiiSetup;
        rows_.push_back(r);
    }
    if (!per_game)
    {
        std::vector<std::string> presets = {"None"};
        const auto &names = Settings::wii_preset_names();
        for (int i = 0; i < Settings::kWiiPresets; ++i)
        {
            const Settings::WiiPreset &w = settings_->wii_presets[i];
            presets.push_back(std::to_string(i + 1) + ": " + (w.used ? names[std::size_t(w.name)] : "empty"));
        }
        choice("wii_preset", "Wii preset",
               "A whole Wii Remote set-up kept under a name: made in the setup's Fine-tune page (Advanced).",
               &t.wii_preset, 0, presets);
    }
    if (per_game)
        toggle("wii_setup_ask", "Setup before this game",
               "Shows the Wii Remote setup when this game starts; Triangle there plays straight away.",
               &t.wii_setup_ask, "Don't show", "Show");
    else
        toggle("wii_setup_ask", "Setup before each Wii game",
               "Shows the Wii Remote setup when a Wii game starts; Triangle there plays straight away. A game "
               "can have its own choice in its settings.",
               &t.wii_setup_ask, "Don't show", "Show");
    {
        SettingRow r;
        r.section = section;
        r.label = tr("How to hold it");
        r.help = tr("Beta: a picture of the DualSense as each Wii controller, how to hold it and what every button "
                    "does. Wii motion controls are still being tuned.");
        r.values = {tr("Show\xE2\x80\xA6")};
        r.action = kRowWiiGuide;
        rows_.push_back(r);
    }
    choice("wii_controller", "Wii controller",
           "How a Wii game sees your DualSense. Remote + Nunchuk: the Nunchuk on the left stick and L1 / L2. Remote: "
           "held pointing at the TV, with its motion. Sideways: held like an NES pad, tilt to steer. Two "
           "controllers (beta): the second DualSense is the Nunchuk. Otherwise every other controller is another "
           "player's own Wii Remote, with its own pointer and motion.",
           &t.wii_controller, 0, {"Remote + Nunchuk", "Remote", "Remote sideways", "Classic Controller",
                                  "Two controllers (alpha)"});
    choice("wii_pointer", "Pointer", "What moves the Remote's pointer. Gyro: point the controller at the screen; hold R1 a moment to center it.",
           &t.wii_pointer, 0, {"Gyro", "Touch pad", "Right stick"});
    choice("wii_speed", "Pointer speed",
           "How far you turn the controller to reach the screen's edge. Your screen: as measured by the Wii Remote "
           "setup, so the pointer is where you point.",
           &t.wii_speed, 0, {"Your screen", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"});
    choice("wii_grip", "Grip", "How you hold the DualSense. Auto reads it: flat in both hands, or stood on end in either hand.", &t.wii_grip, 0,
           {"Auto", "Both hands", "Upright, trigger to the TV", "Upright, facing you"});
    toggle("wii_motion", "Motion", "The DualSense's motion is the Remote's: tilt, swing and point.", &t.wii_motion);
    toggle("wii_shake", "Flick to shake", "A quick flick of the controller shakes the Remote.", &t.wii_shake);

    header("System");
    choice("cpu_clock", "CPU clock", "Overclocking can smooth a game that slows down. 100% is the real console.",
           &t.cpu_clock, 0, {"50%", "60%", "70%", "80%", "90%", "100%", "150%", "200%", "250%", "300%"});
    toggle("dual_core", "Dual core", "Faster. Turn it off for a game that freezes or glitches.", &t.dual_core);
    toggle("own_cores", "Emulator on its own cores",
           "Gives the emulated console's processor and its graphics a core of the PS5 each, away from "
           "Porpoise's other work. Usually faster; turn it off if a game runs worse.",
           &t.own_cores);
    toggle("accurate_fma", "Exact multiply-add",
           "Rounds the console's floating-point multiply-adds exactly, as Dolphin does on a PC. Much slower in "
           "games heavy on 3D math, and very few games need it.",
           &t.accurate_fma);
    toggle("fast_disc", "Fast disc loading", "Shorter loading screens. A few games need real disc speed.",
           &t.fast_disc);
    toggle("cheats", "Cheats", "Dolphin's cheat codes for games that have them.", &t.cheats);
    choice("language", "System language", "The console's language. European games show their text in it.",
           &t.language, 0,
           {"English", "Japanese", "German", "French", "Spanish", "Italian", "Dutch", "Chinese (simplified)",
            "Chinese (traditional)", "Korean"});
    toggle("progressive", "Progressive scan", "480p output, as on a component cable.", &t.progressive);
    toggle("wii_widescreen", "Wii widescreen (beta)",
           "The Wii's own 16:9 setting, which Wii games follow. Off: 4:3.", &t.wii_widescreen);
    toggle("pal60", "PAL games at 60 Hz (beta)",
           "European Wii games run at 60 Hz, as a Wii set to EURGB60 does. Off: 50 Hz.", &t.pal60);
    choice("sensor_bar", "Sensor bar (beta)", "Where the Wii is told its sensor bar sits.", &t.sensor_bar, 0,
           {"Below the TV", "Above the TV"});
    toggle("wii_menu_boot", "Start Wii discs in the Wii Menu (beta)",
           "Wii discs start from the Wii Menu, as on a Wii. Needs your own Wii Menu, installed from your own console "
           "(its WAD in your games). Porpoise includes none.",
           &t.wii_menu_boot);
    toggle("gc_bios", "GameCube boot animation (beta)",
           "GameCube games start with the console's own start-up, from your own console's BIOS: put its IPL.bin in "
           "/data/porpoise/bios/USA, EUR or JAP. Porpoise includes none.",
           &t.gc_bios);
    if (!per_game)
        toggle("debug_logs", "Debug logs",
               "For testing: Porpoise keeps notes on what it did in /data/porpoise/debug, for bug reports.",
               &t.debug_logs);
    if (!per_game && settings_->developer)
    {
        header("Developer");
        toggle("motion_readout", "Motion readout", "The controller's motion and the pointer, over the game.",
               &t.motion_readout);
        toggle("wii_invert_x", "Invert pointer left / right", "If the gyro pointer moves the wrong way.",
               &t.wii_invert_x);
        toggle("wii_invert_y", "Invert pointer up / down", "If the gyro pointer moves the wrong way.",
               &t.wii_invert_y);
        toggle("motion_logs", "Motion logs",
               "Each Wii game writes the controller's motion to /data/porpoise/debug/motion-<date>.csv.",
               &t.motion_logs);
        SettingRow r;
        r.section = section;
        r.label = tr("Turn off developer options");
        r.help = tr("Hides this section again. Its settings go back to off.");
        r.values = {tr("Turn off")};
        r.action = kRowDeveloperOff;
        rows_.push_back(r);
    }
}

void App::build_settings()
{
    rows_.clear();
    std::string section;
    auto header = [&](const char *name) {
        SettingRow r;
        r.section = section = name;
        r.header = true;
        rows_.push_back(r);
    };
    auto toggle = [&](const char *key, const char *label, const char *help, bool *value) {
        SettingRow r;
        r.section = section;
        r.key = key;
        r.label = beta_label(label, r.beta);
        r.help = tr(help);
        r.bool_value = value;
        r.values = {tr("Off"), tr("On")};
        rows_.push_back(r);
    };
    auto action = [&](const char *label, const char *help, const std::string &value, int act, int folder = -1) {
        SettingRow r;
        r.section = section;
        r.label = beta_label(label, r.beta);
        r.help = tr(help);
        r.values = {tr(value)};
        r.action = act;
        r.folder = folder;
        rows_.push_back(r);
    };
    auto info = [&](const char *label, const std::string &value, const char *help) {
        SettingRow r;
        r.section = section;
        r.label = tr(label);
        r.help = tr(help);
        r.values = {value};
        rows_.push_back(r);
    };
    auto choice = [&](const char *key, const char *label, const char *help, int *value,
                      std::initializer_list<const char *> values) {
        SettingRow r;
        r.section = section;
        r.key = key;
        r.label = tr(label);
        r.help = tr(help);
        r.int_value = value;
        for (const char *v : values)
            r.values.push_back(tr(v));
        rows_.push_back(r);
    };

    header("Games");
    toggle("auto_search", "Find games automatically",
           "Looks in /data/porpoise/games, /data/games, /data/roms, /data/iso and on USB drives.",
           &settings_->auto_search);
    rows_.back().rescan = true;
    toggle("download_covers", "Download covers",
           "Box art from GameTDB.com, saved in /data/porpoise/covers. Needs the console online.",
           &settings_->download_covers);
    toggle("download_info", "Download game info",
           "Descriptions, developers, release dates and disc art from GameTDB.com, for Details.",
           &settings_->download_info);
    for (std::size_t i = 0; i < settings_->folders.size(); ++i)
    {
        SettingRow r;
        r.section = section;
        r.label = settings_->folders[i];
        r.help = tr("Porpoise looks in this folder and four levels below it. Cross stops looking here; files stay.");
        r.values = {tr("Remove")};
        r.action = kRowRemoveFolder;
        r.folder = int(i);
        rows_.push_back(r);
    }
    action("Add a game folder", "Pick any folder on the console or a USB drive to search for games.",
           "Choose\xE2\x80\xA6", kRowAddFolder);
    const std::size_t n = lib_ ? lib_->games().size() : 0;
    action("Search for games now", "Looks through every folder again, for games you have just copied over.",
           plural((long long)n, "1 game", "{n} games"), kRowRescan);
    action("Check my setup", "What Porpoise can see on this console - /data, USB drives, games - and what to do "
           "about anything missing.",
           "Check\xE2\x80\xA6", kRowSetupCheck);
    action("Saves from a USB drive (beta)",
           "Copies saves from the USB drive's Porpoise Saves folder in: GameCube saves onto Slot A, Wii saves to "
           "their games. A save that's already here is left as it is. Options in Memory Cards copies a save out.",
           "Copy in\xE2\x80\xA6", kRowImportSaves);

    add_game_rows(*settings_, false);

    header("Interface");
    {
        SettingRow r;
        r.section = section;
        r.key = "ui_language";
        r.label = tr("Language");
        r.help = tr("The language of Porpoise's menus. System follows your PS5.");
        r.int_value = &settings_->ui_language;
        r.order = language_order();
        for (int i : r.order)
            r.values.push_back(language_choice(i));
        rows_.push_back(r);
    }
    {
        SettingRow r;
        r.section = section;
        r.key = "ui_theme";
        r.label = tr("Theme");
        r.help = tr(th().about);
        r.int_value = &settings_->ui_theme;
        for (int i = 0; i < kThemes; ++i)
            r.values.push_back(tr(theme(i).name));
        rows_.push_back(r);
    }
    if (th().palettes)
    {
        SettingRow r;
        r.section = section;
        r.key = "ui_palette";
        r.label = tr("Colors");
        r.help = tr("The theme's colors: its glass, its light and what is chosen.");
        r.int_value = &settings_->ui_palette;
        r.values.push_back(tr("Theme default"));
        for (int i = 0; i < kPalettes; ++i)
            r.values.push_back(tr(palette(i).name));
        rows_.push_back(r);
    }
    {
        SettingRow r;
        r.section = section;
        r.key = "ui_font";
        r.label = tr("Font");
        r.help = tr("The letters across Porpoise. Each theme starts with its own; any font works with any theme.");
        r.int_value = &settings_->ui_font;
        r.values.push_back(tr("Theme default"));
        for (int i = 0; i < kFontSets; ++i)
            r.values.push_back(font_set(i).name);
        rows_.push_back(r);
    }
    if (settings_->ui_theme == 1)
    {
        SettingRow r;
        r.section = section;
        r.key = "ui_layout";
        r.label = tr("Home screen");
        r.help = tr("Tiles: a grid you point at, twelve to a page. Library view: your games in the Library view below.");
        r.int_value = &settings_->ui_layout;
        r.values = {tr("Tiles"), tr("Library view")};
        rows_.push_back(r);
        toggle("ui_pointer", "Point with the controller",
               "Move the controller to point at tiles and buttons. The touch pad turns it on and off too.",
               &settings_->ui_pointer);
    }
    if (settings_->ui_theme == int(ThemeId::StarCube))
        choice("sc_games", "Games page", "Star Cube's Games page: your games as spinning discs, or as covers.",
               &settings_->sc_games, {"Discs", "Covers"});
    else
    {
        static const char *const kHelp[8] = {
            "Cover flow: the boxes in a row, the chosen one in front.",
            "Wheel: the boxes around a turning wheel; the one at the front is chosen.",
            "Disc flow: your discs, spinning into place as you go.",
            "Shelf: rows of boxes, many at once. Up and down change row.",
            "Box: one box at a time, its whole cover wrapped round it. The right stick turns it.",
            "List: your games by name, the chosen one's box beside them. Up and down go through them.",
            "Stack: a deck of boxes; the front one flips away as you go.",
            "Helix: the boxes climbing round a turning column."};
        choice("lib_view", "Library view", kHelp[std::clamp(settings_->lib_view, 0, 7)], &settings_->lib_view,
               {"Cover flow", "Wheel", "Disc flow", "Shelf", "Box", "List", "Stack", "Helix"});
    }
    if (settings_->ui_theme != int(ThemeId::StarCube))
    {
        static const char *const kHelp[4] = {
            "Cards: both memory cards side by side, their saves as icons.",
            "Blocks: one card at a time, drawn as the card itself with each save's blocks.",
            "By game: every save on both cards and the Wii, grouped by game.",
            "Cubes: both cards side by side, each save a little glass cube on a grid."};
        choice("mc_view", "Memory Cards view", kHelp[std::clamp(settings_->mc_view, 0, 3)], &settings_->mc_view,
               {"Cards", "Blocks", "By game", "Cubes"});
    }
    action("Reset all settings", "Every setting back to how Porpoise ships. Games, folders and saves stay.",
           "Reset\xE2\x80\xA6", kRowResetAll);
    action("Reinitialize Porpoise",
           "A fresh start: Porpoise opens as it did the first time and you choose a theme again. Games, folders, "
           "memory cards, saves and save states stay.",
           "Reinitialize\xE2\x80\xA6", kRowReinitialize);

    header("Accessibility");
    choice("text_size", "Text size", "Bigger labels across Porpoise.", &settings_->text_size,
           {"Normal", "Large", "Larger"});
    choice("colour_filter", "Color filter",
           "For color blindness: moves the colors you may not tell apart to ones you can. Red-weak and "
           "green-weak help with reds and greens, blue-weak with blues and yellows.",
           &settings_->colour_filter, {"Off", "Red-weak", "Green-weak", "Blue-weak", "Grayscale"});
    toggle("colour_filter_games", "Color filter in games", "The same color filter on the game's picture too.",
           &settings_->colour_filter_games);
    toggle("high_contrast", "High contrast", "Solid panels, clearer edges and brighter text.",
           &settings_->high_contrast);
    toggle("reduced_motion", "Reduced motion", "Stops the moving lights and shortens animations.",
           &settings_->reduced_motion);
    toggle("still_background", "Still background", "The background holds still; everything else moves as usual.",
           &settings_->still_background);
    toggle("big_prompts", "Larger button hints", "The button hints along the bottom of the screen, larger.",
           &settings_->big_prompts);

    header("About");
    info("Porpoise", build_label(), "A GameCube and Wii emulator for PS5, powered by Dolphin.");
    {
        SettingRow r;
        r.section = section;
        r.label = update_available() ? trf("Porpoise {version} is out", {{"version", latest_version_}})
                                     : tr("Updates");
        r.help = update_available()
                     ? trf("Downloads Porpoise {version} from GitHub, checks it, and puts it in place of this one. "
                           "Your games, saves and settings stay. Porpoise closes when it's done.",
                           {{"version", latest_version_}})
                     : tr("Looks on GitHub for a newer Porpoise. Porpoise also looks once a day by itself.");
        r.values = {""};
        r.action = kRowUpdate;
        rows_.push_back(r);
    }
    toggle("beta_updates", "Beta updates",
           "Offer test versions (pre-releases) too when they come out: newer, but less tested. A beta of Porpoise "
           "always offers the next beta.",
           &settings_->beta_updates);
    if (!version_labels_.empty())
    {
        SettingRow r;
        r.section = section;
        r.key = "version_pick";
        r.label = tr("Choose a version");
        r.help = tr("Any Porpoise release, newer or older, betas included: left and right to pick, Cross to install. "
                    "Going back keeps your games, saves and settings; save states made by a newer Porpoise may not "
                    "load in an older one.");
        r.values = version_labels_;
        r.int_value = &version_pick_;
        r.action = kRowPickVersion;
        rows_.push_back(r);
    }
    /* Help and the community: the RIPALDA Discord (bug reports go there now),
     * with a code to scan beside it. */
    info("Discord", "discord.gg/GgDE5Vynyu",
         "Help, bug reports, news and the community, on the RIPALDA Discord. Scan the code with your phone to join.");
    rows_.back().key = "discord";
    {
        SettingRow r;
        r.section = section;
        r.label = tr("Report a bug");
        r.help = tr("Cross gathers Porpoise's logs into /data/porpoise/reports (and onto a USB drive, when one is "
                    "in). Share them in the Discord's bug reports with the game and what happened.");
        r.values = {tr("Save a report\xE2\x80\xA6")};
        r.action = kRowSendReport;
        rows_.push_back(r);
    }
    toggle("perf_profile", "Performance report",
           "For a game that runs slowly: from the next time Porpoise starts, it records where the emulator spends "
           "its time, for a bug report. Slows games a little; turn it off again afterwards.",
           &settings_->perf_profile);

    /* Credits: Ruben first, his mark beside his name. */
    info("Created by", "Ruben (@elripalda)", "Porpoise, its menus, music and sounds: Ruben. @elripalda - ripalda.dev");
    rows_.back().key = "creator"; /* three presses: developer options (not advertised) */
    info("Website", "ripalda.dev", "Ruben's projects, news and downloads, with links to the Discord and more.");
    info("Music and sounds", "@elripalda", "The menu music and sound effects, made for Porpoise by Ruben.");
    info("Dolphin on PS5", "Mihawk (mihawk-99)",
         "Mihawk (mihawk-99) brought the Dolphin core to the PS5. Porpoise is built on his port of Dolphin and "
         "RetroArch.");
    info("Controller art", "Zacksly",
         "PS5 Button Icons and Controls by Zacksly - zacksly.itch.io, @_Zacksly on Twitter. CC BY 3.0, adapted for "
         "Porpoise.");
    info("Emulation", "Dolphin", "Dolphin, by the Dolphin Team - dolphin-emu.org. Free software, GPL v2 or later.");
    info("Dolphin core", "libretro", "Dolphin's libretro core, maintained by the libretro team (GPL v2 or later).");
    info("Core API", "libretro / RetroArch", "Porpoise hosts the core through the libretro API that RetroArch made.");
    info("PS5 graphics", "Mesa RADV", "Vulkan on PS5 through Mesa's RADV driver (MIT), PS5_Mesa and PS5_Vulkan ports.");
    info("PS5 toolchain", "ps5-payload-sdk", "John T\xC3\xB6rnblom's ps5-payload-sdk (GPL v3) and its PS5 ports.");
    info("Inspired by", "PS5SX2, ProsperoEden", "PS5 homebrew front ends that showed the way.");
    info("Box art and info", "GameTDB.com", "Covers, disc art and game details from GameTDB.com and its contributors.");
    info("Fonts", "Nunito, Noto Sans",
         "Nunito; Noto Sans JP, SC, TC and KR; M PLUS 1; JetBrains Mono; VT323; Doto; Exo 2; and Lora (as Porpoise "
         "Serif). All under the SIL Open Font License.");
    info("Images and audio", "stb", "stb_image, stb_truetype and stb_vorbis by Sean Barrett (public domain / MIT).");
    info("Wii banners", "Wii Banner Player",
         "How a Wii disc's own tile and banner play: after the Wii Banner Player Project (zlib license), rewritten "
         "for Porpoise. RVZ discs are read with Zstandard's decoder (BSD).");
    info("Thanks", "PS5 scene", "etaHEN, kstuff and ShadowMountPlus make homebrew like this possible.");
    info("Trademarks", "Nintendo", "GameCube and Wii are trademarks of Nintendo. Porpoise is not affiliated with Nintendo.");
    info("Trademarks", "Sony",
         "PlayStation, PS5 and DualSense are trademarks of Sony Interactive Entertainment. Porpoise is not affiliated "
         "with Sony.");

    if (settings_row_ < 0 || settings_row_ >= int(rows_.size()) || rows_[std::size_t(settings_row_)].header)
        settings_row_ = 1;
    rail_ = std::clamp(rail_, 0, std::max(0, section_count() - 1));
}

void App::build_game_settings()
{
    rows_.clear();
    SettingRow h;
    h.section = "This game";
    h.header = true;
    rows_.push_back(h);
    SettingRow r;
    r.section = "This game";
    r.label = tr("Own settings");
    r.help = tr("Changes here apply to this game only. Everything else follows your settings.");
    r.values = {game_keys_.empty() ? tr("None yet") : plural(change_count(), "1 change", "{n} changes")};
    rows_.push_back(r);
    SettingRow reset;
    reset.section = "This game";
    reset.label = tr("Reset to default");
    reset.help = tr("Forgets this game's own settings; it follows your settings again.");
    reset.values = {tr("Reset\xE2\x80\xA6")};
    reset.action = kRowResetGame;
    rows_.push_back(reset);
    add_setup_rows(true);
    add_game_rows(game_, true);
    add_recommended_rows();
    add_cheat_rows();
    if (settings_row_ < 0 || settings_row_ >= int(rows_.size()) || rows_[std::size_t(settings_row_)].header)
        settings_row_ = 1;
    rail_ = std::clamp(rail_, 0, std::max(0, section_count() - 1));
}

/* One row for each saved setup: Cross puts its Video and Graphics settings on
 * this game (per_game) or on every game. */
void App::add_setup_rows(bool per_game)
{
    const std::string section = per_game ? "This game" : "Video";
    for (int i = 0; i < setups::kCount; ++i)
    {
        const setups::Setup su = setups::get(i);
        if (!su.exists)
            continue;
        SettingRow r;
        r.section = section;
        r.label = trf("Use setup {n}", {{"n", std::to_string(i + 1)}});
        r.help = per_game ? trf("Puts setup {n}'s Video and Graphics settings on this game. It was saved from {game}.",
                                {{"n", std::to_string(i + 1)}, {"game", su.from}})
                          : trf("Puts setup {n}'s Video and Graphics settings on every game. It was saved from {game}.",
                                {{"n", std::to_string(i + 1)}, {"game", su.from}});
        r.values = {su.from.empty() ? tr("Use") : su.from};
        r.action = kRowUseSetup;
        r.setup = i;
        rows_.push_back(r);
    }
}

/* The codes Dolphin lists for this game (ui_cheats.hpp), one switch each. */
void App::add_cheat_rows()
{
    cheats_.clear();
    cheat_on_.clear();
    if (!game_for_ || game_for_->id.size() != 6 || sys_dir_.empty())
        return;
    cheats_ = cheats_for(sys_dir_, game_for_->id);
    if (cheats_.empty())
        return;
    SettingRow h;
    h.section = "Cheats and patches (beta)";
    h.header = true;
    rows_.push_back(h);
    for (std::size_t i = 0; i < cheats_.size() && i < 80; ++i)
    {
        const Cheat &c = cheats_[i];
        const bool on = c.default_on ? game_.get(cheat_key(c, false)) != "1" : game_.get(cheat_key(c, true)) == "1";
        cheat_on_.push_back(on);
    }
    for (std::size_t i = 0; i < cheat_on_.size(); ++i)
    {
        const Cheat &c = cheats_[i];
        SettingRow r;
        r.section = "Cheats and patches (beta)";
        r.key = "cheat";
        r.folder = int(i);
        r.label = c.name.substr(1);
        r.help = cheat_help(c);
        r.bool_value = &cheat_on_[i];
        r.values = {tr("Off"), tr("On")};
        rows_.push_back(r);
    }
}

/* What's recommended for this game, as switches: Porpoise's picks (all at
 * once, or one by one) and Dolphin's own fixes, which are on unless turned
 * off here. Built after the game's other rows, whose names it borrows. */
void App::add_recommended_rows()
{
    rec_rows_.clear();
    if (!game_for_)
        return;
    std::vector<SettingRow> out;
    SettingRow h;
    h.section = "Recommended";
    h.header = true;
    out.push_back(h);
    /* A Dolphin settings file of the player's own for this game: Porpoise
     * leaves it as it is, so its switches would change nothing. */
    if (game_for_->id.size() == 6)
    {
        const std::string own = "User/GameSettings/" + game_for_->id + ".ini";
        if (std::FILE *f = std::fopen((saves_dir_ + "/" + own).c_str(), "r"))
        {
            char first[64] = {0};
            const bool ours = std::fgets(first, sizeof first, f) && std::strncmp(first, "# Written by Porpoise", 21) == 0;
            std::fclose(f);
            if (!ours)
            {
                SettingRow r;
                r.section = "Recommended";
                r.label = tr("Your own Dolphin file");
                r.help = trf("saves/{file} is yours, so Porpoise leaves it as it is and the switches below change "
                             "nothing. Remove it to use them.",
                             {{"file", own}});
                r.values = {""};
                out.push_back(r);
            }
        }
    }
    auto add = [&](const std::string &label, const std::string &help, RecRow rec) {
        SettingRow r;
        r.section = "Recommended";
        r.label = label;
        r.tag = rec.kind == RecRow::DolphinFix ? tr("Dolphin's fix")
                : rec.kind == RecRow::Pick     ? tr("Porpoise's pick")
                                               : "";
        r.help = help;
        r.values = {""};
        r.rec = int(rec_rows_.size());
        r.toggle = rec_on(rec) ? 1 : 0;
        rec_rows_.push_back(rec);
        out.push_back(r);
    };
    /* How a setting and its value read, from the game's own rows. */
    auto describe = [&](const std::string &key, const std::string &value, std::string &label, std::string &text) {
        if (key.rfind("dolphin.", 0) == 0)
        {
            recommend::describe_dolphin(key, value, label, text);
            label = tr(label);
            text = tr(text);
            return;
        }
        label = key;
        text = value;
        for (const SettingRow &r : rows_)
            if (r.key == key && (r.int_value || r.bool_value))
            {
                label = r.label;
                const int v = std::atoi(value.c_str());
                const int i = r.bool_value ? (v ? 1 : 0) : v - r.min;
                if (i >= 0 && i < int(r.values.size()))
                    text = r.values[std::size_t(i)];
            }
    };

    recommend::Pick pick;
    if (recommend::pick_for(game_for_->id, pick))
    {
        RecRow all;
        all.kind = RecRow::AllPicks;
        add(tr("Porpoise's picks"),
            (pick.note.empty() ? tr("Settings that run this game best on PS5, as tested.") : pick.note) + "  " +
                tr("Cross turns them all on or off."),
            all);
        for (const auto &[k, v] : pick.values)
        {
            std::string label, text;
            describe(k, v, label, text);
            RecRow one;
            one.kind = RecRow::Pick;
            one.key = k;
            one.value = v;
            add(label + ": " + text, trf("One of Porpoise's picks for this game: {setting}.", {{"setting", label + " " + text}}),
                one);
        }
    }
    for (const recommend::Fix &fix : recommend::dolphin_fixes(game_for_->id))
    {
        const std::string label = tr(fix.label) + ": " + tr(fix.value);
        const std::string why = fix.why.empty() ? "" : trf("Dolphin's note: {why}", {{"why", fix.why}}) + "  ";
        if (fix.override_key.empty())
        {
            /* A value, not a switch: shown as it is. */
            SettingRow r;
            r.section = "Recommended";
            r.label = label;
            r.help = why + tr("One of Dolphin's own fixes for this game. Dolphin applies it by itself.");
            r.values = {tr("Applied")};
            out.push_back(r);
            continue;
        }
        RecRow rec;
        rec.kind = RecRow::DolphinFix;
        rec.key = fix.override_key;
        rec.value = fix.raw;
        rec.off_value = fix.off_value;
        /* Dual core turned back off by the player's own choice, not forced on. */
        if (fix.key == "CPUThread")
            rec.off_value = game_.dual_core ? "True" : "False";
        add(label, why + tr("Dolphin's own fix for this game, on by itself. Turning it off may help speed but can bring "
                            "back the problem it fixes."),
            rec);
    }
    if (out.size() == 1)
    {
        SettingRow r;
        r.section = "Recommended";
        r.label = tr("Nothing needed");
        r.help = tr("Dolphin has no fixes listed for this game, and Porpoise has no picks for it yet. The list grows "
                    "as games are tested.");
        r.values = {""};
        out.push_back(r);
    }
    /* Right after This game. */
    std::size_t at = 0;
    while (at < rows_.size() && (rows_[at].header ? rows_[at].section == "This game" : rows_[at].section == "This game"))
        ++at;
    rows_.insert(rows_.begin() + std::ptrdiff_t(at), out.begin(), out.end());
}

bool App::rec_on(const RecRow &rec) const
{
    auto has = [&](const std::string &k, const std::string &v) {
        if (std::find(game_keys_.begin(), game_keys_.end(), k) == game_keys_.end())
            return false;
        porpoise::Settings probe = game_;
        probe.set(k, v);
        return probe.get(k) == game_.get(k);
    };
    switch (rec.kind)
    {
    case RecRow::DolphinFix:
        /* On unless this game turns it off. */
        return game_.get(rec.key).empty() || game_.get(rec.key) == rec.value;
    case RecRow::Pick:
        return has(rec.key, rec.value);
    case RecRow::AllPicks:
    {
        recommend::Pick pick;
        if (!recommend::pick_for(game_for_->id, pick))
            return false;
        for (const auto &[k, v] : pick.values)
            if (!has(k, v))
                return false;
        return true;
    }
    }
    return false;
}

void App::set_game_key(const std::string &key, const std::string *value)
{
    const auto it = std::find(game_keys_.begin(), game_keys_.end(), key);
    if (value)
    {
        game_.set(key, *value);
        if (it == game_keys_.end())
            game_keys_.push_back(key);
        return;
    }
    /* Back to what every game uses. */
    if (key.rfind("dolphin.", 0) == 0)
        game_.forget(key);
    else
        game_.set(key, settings_->get(key));
    if (it != game_keys_.end())
        game_keys_.erase(it);
}

void App::toggle_recommended(int index)
{
    if (!game_for_ || index < 0 || index >= int(rec_rows_.size()))
        return;
    const RecRow rec = rec_rows_[std::size_t(index)];
    const bool on = rec_on(rec);
    switch (rec.kind)
    {
    case RecRow::DolphinFix:
        if (on)
            set_game_key(rec.key, &rec.off_value);
        else
            set_game_key(rec.key, nullptr);
        break;
    case RecRow::Pick:
        set_game_key(rec.key, on ? nullptr : &rec.value);
        break;
    case RecRow::AllPicks:
    {
        recommend::Pick pick;
        if (recommend::pick_for(game_for_->id, pick))
            for (const auto &[k, v] : pick.values)
                set_game_key(k, on ? nullptr : &v);
        break;
    }
    }
    mkdir((data_dir_ + "/game-settings").c_str(), 0777);
    game_.save_keys(game_settings_path(*game_for_), game_keys_);
    const int row = settings_row_;
    build_game_settings();
    settings_row_ = std::clamp(row, 0, int(rows_.size()) - 1);
    sfx(on ? Sound::MovingTab : Sound::LaunchGame);
}

int App::section_count() const
{
    int n = 0;
    for (const SettingRow &r : rows_)
        n += r.header ? 1 : 0;
    return n;
}

std::string App::section_name(int index) const
{
    int n = 0;
    for (const SettingRow &r : rows_)
        if (r.header && n++ == index)
            return r.section;
    return "";
}

int App::first_row_of(const std::string &section) const
{
    for (int i = 0; i < int(rows_.size()); ++i)
        if (!rows_[std::size_t(i)].header && rows_[std::size_t(i)].section == section)
            return i;
    return -1;
}

void App::open_game_settings(Game &g)
{
    game_for_ = &g;
    game_ = *settings_;
    const std::string path = game_settings_path(g);
    game_.load(path, true);
    game_keys_ = Settings::keys_in(path);
    on_rail_ = true;
    rail_ = 0;
    settings_row_ = 1;
    build_game_settings();
    open_screen(Screen::GameSettings);
}

void App::close_game_settings()
{
    details_custom_ = !game_keys_.empty();
    game_for_ = nullptr;
    build_settings();
    on_rail_ = true;
    rail_ = 0;
    open_screen(Screen::Details);
}

/* ---- changing ------------------------------------------------------------------------------ */

void App::change_setting(int dir)
{
    SettingRow &r = rows_[std::size_t(settings_row_)];
    if (r.header || r.action || (!r.bool_value && !r.int_value))
        return;
    if (r.bool_value)
        *r.bool_value = !*r.bool_value;
    else
    {
        const int n = int(r.values.size());
        if (!r.order.empty())
        {
            const auto at = std::find(r.order.begin(), r.order.end(), *r.int_value);
            const int i = at == r.order.end() ? 0 : int(at - r.order.begin());
            *r.int_value = r.order[std::size_t(std::clamp(i + dir, 0, n - 1))];
        }
        else
            *r.int_value = std::clamp(*r.int_value + dir, r.min, r.min + n - 1);
        if (r.text_value && r.int_value == &border_choice_)
            *r.text_value = border_names_[std::size_t(std::clamp(border_choice_, 0, int(border_names_.size()) - 1))];
    }
    if (r.key == "cheat" && screen_ == Screen::GameSettings && game_for_ && r.folder >= 0 &&
        r.folder < int(cheats_.size()))
    {
        /* A code's switch: named in [<kind>_Enabled] when it is on and
         * Dolphin doesn't turn it on by itself, in [<kind>_Disabled] when it
         * is off and Dolphin would. Cheats (not patches) need Dolphin's
         * cheats on for the game. */
        const Cheat &c = cheats_[std::size_t(r.folder)];
        const bool on = cheat_on_[std::size_t(r.folder)];
        for (const std::string &k : {cheat_key(c, true), cheat_key(c, false)})
        {
            game_.forget(k);
            game_keys_.erase(std::remove(game_keys_.begin(), game_keys_.end(), k), game_keys_.end());
        }
        const std::string k = on && !c.default_on ? cheat_key(c, true) : !on && c.default_on ? cheat_key(c, false) : "";
        if (!k.empty())
        {
            game_.set(k, "1");
            game_keys_.push_back(k);
        }
        if (on && c.kind != "OnFrame" && !game_.cheats)
        {
            game_.cheats = true;
            if (std::find(game_keys_.begin(), game_keys_.end(), "cheats") == game_keys_.end())
                game_keys_.push_back("cheats");
        }
        mkdir((data_dir_ + "/game-settings").c_str(), 0777);
        game_.save_keys(game_settings_path(*game_for_), game_keys_);
        rows_[1].values = {plural(change_count(), "1 change", "{n} changes")};
        return;
    }
    if (screen_ == Screen::GameSettings && game_for_)
    {
        if (!r.key.empty() && std::find(game_keys_.begin(), game_keys_.end(), r.key) == game_keys_.end())
            game_keys_.push_back(r.key);
        mkdir((data_dir_ + "/game-settings").c_str(), 0777);
        game_.save_keys(game_settings_path(*game_for_), game_keys_);
        rows_[1].values = {plural(change_count(), "1 change", "{n} changes")};
        return;
    }
    if (r.key == "ui_theme")
        look_changed(theme_seen_);
    if (r.key == "ui_theme" || r.key == "ui_layout" || r.key == "lib_view" || r.key == "mc_view" || r.key == "sc_games" ||
        r.key == "ui_palette" || r.key == "ui_font")
        build_settings(); /* rows and help that follow the look */
    if (r.key == "wii_preset" && settings_->wii_preset > 0)
    {
        /* A preset brings its whole set-up. */
        settings_->use_wii_preset(settings_->wii_preset - 1);
        build_settings();
    }
    settings_->save(settings_path_);
    settings_->write_core_options(options_path_);
    if (r.key == "ui_language")
    {
        /* The menus change language at once. */
        apply_language(settings_->ui_language, data_dir_ + "/lang");
        build_settings();
    }
}

App::Action App::activate_row(const SettingRow &row)
{
    switch (row.action)
    {
    case kRowAddFolder:
        open_browser("/");
        /* Start on /data, where most players keep their games. */
        for (int i = 0; i < int(browse_entries_.size()); ++i)
            if (browse_entries_[std::size_t(i)].path == "/data")
                browse_row_ = i;
        return Action::None;
    case kRowUpdate:
        if (updating() || update_phase_ == 1)
            return Action::None;
        if (update_available())
        {
            open_dialog(DialogKind::InstallUpdate, trf("Install Porpoise {version}?", {{"version", latest_version_}}),
                        latest_size_ ? trf("{mb} MB from GitHub. Porpoise checks the download, puts the new files in "
                                           "place, and then closes so you can open the new one. Your games, saves "
                                           "and settings stay.",
                                           {{"mb", std::to_string((latest_size_ + (1 << 20) - 1) >> 20)}})
                                     : tr("Porpoise checks the download, puts the new files in place, and then closes "
                                          "so you can open the new one. Your games, saves and settings stay."),
                        tr("Install"));
            return Action::None;
        }
        sfx(Sound::MenuScroll);
        update_phase_ = 1; /* until the updater says otherwise: a quick failure still shows */
        return Action::CheckUpdate;
    case kRowSetupCheck:
        sfx(Sound::MenuScroll);
        show_setup_check(false);
        return Action::None;
    case kRowImportSaves:
    {
        sfx(Sound::MenuScroll);
        if (usb_root().empty())
        {
            open_dialog(DialogKind::Info, tr("No USB drive"),
                        tr("Plug in the USB drive with a Porpoise Saves folder (made by Options in Memory Cards)."),
                        "");
            return Action::None;
        }
        Card a, b;
        load_cards(saves_dir_, a, b);
        int gc = 0, wii = 0, skipped = 0;
        import_usb_saves(saves_dir_, a, gc, wii, skipped);
        cards_scanned_ = wii_scanned_ = false;
        save_scan_.reset(); /* a scan already running read the old state */
        std::string text = trf("GameCube saves copied onto Slot A: {gc}. Wii saves copied: {wii}.",
                               {{"gc", std::to_string(gc)}, {"wii", std::to_string(wii)}});
        if (skipped)
            text += " " + plural(skipped, "1 was already here and was left as it is.",
                                 "{n} were already here and were left as they are.");
        if (!a.folder && gc == 0)
            text += " " + tr("Slot A is one memory card file, so GameCube saves can't be added to it one by one.");
        open_dialog(DialogKind::Info, tr("Saves from the USB drive"), text, "");
        return Action::None;
    }
    case kRowSendReport:
    {
        sfx(Sound::MenuScroll);
        std::string usb;
        const std::string where = save_report(usb);
        if (where.empty())
            open_dialog(DialogKind::Info, tr("No report saved"),
                        tr("Porpoise couldn't write the report folder. Copy trace.txt and porpoise/core.log from "
                           "Porpoise's app folder by FTP instead."),
                        "");
        else
            open_dialog(DialogKind::Info, tr("Report saved"),
                        usb.empty() ? trf("It's in {path}. Scan the code beside Report a bug to join the Discord, "
                                          "and share its files in the bug reports.",
                                          {{"path", where}})
                                    : trf("It's in {path}, and on your USB drive as {usb}. Scan the code beside "
                                          "Report a bug to join the Discord, and share its files in the bug reports.",
                                          {{"path", where}, {"usb", usb}}),
                        "");
        return Action::None;
    }
    case kRowPickVersion:
    {
        if (updating() || update_phase_ == 1 || version_pick_ < 0 || version_pick_ >= int(version_tags_.size()))
            return Action::None;
        const std::string shown = version_label(version_tags_[std::size_t(version_pick_)],
                                                version_builds_[std::size_t(version_pick_)]);
        int major = 0, minor = 0, patch = 0, beta = 0;
        porpoise::update::tag_version(version_tags_[std::size_t(version_pick_)], major, minor, patch, beta);
        const int order = porpoise::update::compare_versions(major, minor, patch, beta,
                                                             version_builds_[std::size_t(version_pick_)], kVersionMajor,
                                                             kVersionMinor, kVersionPatch, kVersionBeta, kBuild);
        const std::string mb = std::to_string((version_sizes_[std::size_t(version_pick_)] + (1 << 20) - 1) >> 20);
        open_dialog(DialogKind::InstallVersion, trf("Install Porpoise {version}?", {{"version", shown}}),
                    order < 0 ? trf("{mb} MB from GitHub. This goes back to an older Porpoise: your games, saves and "
                                    "settings stay, but save states made by this version may not load in it. "
                                    "Porpoise closes when it's done.",
                                    {{"mb", mb}})
                    : order == 0
                        ? trf("{mb} MB from GitHub. This is the version you have: it is put in place again, which "
                              "repairs a damaged install. Porpoise closes when it's done.",
                              {{"mb", mb}})
                        : trf("{mb} MB from GitHub. Porpoise checks the download, puts the new files in place, and "
                              "then closes so you can open the new one. Your games, saves and settings stay.",
                              {{"mb", mb}}),
                    tr("Install"));
        return Action::None;
    }
    case kRowUseSetup:
        if (screen_ == Screen::GameSettings && game_for_)
        {
            if (setups::apply(row.setup, game_, &game_keys_))
            {
                mkdir((data_dir_ + "/game-settings").c_str(), 0777);
                game_.save_keys(game_settings_path(*game_for_), game_keys_);
                build_game_settings();
                sfx(Sound::LaunchGame);
            }
            return Action::None;
        }
        if (setups::apply(row.setup, *settings_))
        {
            settings_->save(settings_path_);
            settings_->write_core_options(options_path_);
            build_settings();
            sfx(Sound::LaunchGame);
            return Action::SettingsChanged;
        }
        return Action::None;

    case kRowRemoveFolder:
        if (row.folder >= 0 && row.folder < int(settings_->folders.size()))
        {
            settings_->folders.erase(settings_->folders.begin() + row.folder);
            settings_->save(settings_path_);
            build_settings();
            return Action::Rescan;
        }
        return Action::None;
    case kRowRescan:
        return Action::Rescan;
    case kRowResetAll:
        open_dialog(DialogKind::ResetAll, tr("Reset all settings?"),
                    tr("Every setting goes back to how Porpoise ships. Your games, folders, covers and saves stay."),
                    tr("Reset"), true);
        return Action::None;
    case kRowReinitialize:
        open_dialog(DialogKind::Reinitialize, tr("Reinitialize Porpoise?"),
                    tr("Every setting, and each game's own settings, go back to how Porpoise ships, and it starts again "
                       "as it did the first time. Your games, folders, memory cards, saves and save states stay."),
                    tr("Reinitialize"), true);
        return Action::None;
    case kRowMapping:
        open_mapping();
        return Action::None;
    case kRowWiiGuide:
        open_wii_guide();
        return Action::None;
    case kRowWiiSetup:
        open_wii_setup(screen_ == Screen::GameSettings ? game_for_ : nullptr, false, "");
        return Action::None;
    case kRowDeveloperOff:
        settings_->developer = false;
        settings_->motion_readout = settings_->wii_invert_x = settings_->wii_invert_y = false;
        settings_->save(settings_path_);
        build_settings();
        on_rail_ = true;
        rail_ = std::min(rail_, section_count() - 1);
        sfx(Sound::MovingTab);
        return Action::SettingsChanged;
    case kRowResetGame:
        open_dialog(DialogKind::ResetGame, tr("Reset this game's settings?"),
                    tr("It forgets its own settings and follows your settings again."), tr("Reset"), true);
        return Action::None;
    default:
        return Action::None;
    }
}

App::Action App::update_settings(bool up, bool down, bool left, bool right)
{
    const bool game = screen_ == Screen::GameSettings;
    const int sections = section_count();
    if (on_rail_)
    {
        if (up && rail_ > 0)
        {
            --rail_;
            sfx(Sound::MenuScroll);
        }
        if (down && rail_ + 1 < sections)
        {
            ++rail_;
            sfx(Sound::MenuScroll);
        }
        if (right || pressed(BtnCross))
        {
            const int first = first_row_of(section_name(rail_));
            if (first >= 0)
            {
                settings_row_ = first;
                on_rail_ = false;
                sfx(Sound::MenuScroll);
            }
        }
        if (pressed(BtnCircle))
        {
            if (game)
                close_game_settings();
            else
                set_tab(int(Tab::Library), -1);
        }
        return Action::None;
    }

    /* Inside a section: up and down stay in it. */
    const std::string section = rows_[std::size_t(settings_row_)].section;
    auto step = [&](int dir) {
        int r = settings_row_ + dir;
        if (r >= 0 && r < int(rows_.size()) && !rows_[std::size_t(r)].header && rows_[std::size_t(r)].section == section)
        {
            settings_row_ = r;
            sfx(Sound::MenuScroll);
        }
    };
    if (up)
        step(-1);
    if (down)
        step(+1);
    if (pressed(BtnCircle))
    {
        on_rail_ = true;
        sfx(Sound::MenuScroll);
        return Action::None;
    }
    const SettingRow &row = rows_[std::size_t(settings_row_)];
    if (row.toggle >= 0)
    {
        /* A recommendation's switch: Cross, or left / right toward off / on. */
        if (pressed(BtnCross) || (left && row.toggle) || (right && !row.toggle))
            toggle_recommended(row.rec);
        return Action::None;
    }
    if (row.action == kRowPickVersion && (left || right) && !row.values.empty())
    {
        version_pick_ = std::clamp(version_pick_ + (right ? 1 : -1), 0, int(row.values.size()) - 1);
        sfx(Sound::MenuScroll);
        return Action::None;
    }
    if (row.action)
        return pressed(BtnCross) ? activate_row(row) : Action::None;
    if (row.key == "creator" && pressed(BtnCross) && !game)
    {
        /* Cross three times on the creator's name: developer options. */
        creator_presses_ = time_ - creator_time_ < 1.5 ? creator_presses_ + 1 : 1;
        creator_time_ = time_;
        if (creator_presses_ >= 3)
        {
            creator_presses_ = 0;
            update_note_ = settings_->developer ? tr("Developer options are already on.")
                                                : tr("Developer options are on: Settings now has a Developer section.");
            update_note_time_ = time_;
            if (!settings_->developer)
            {
                settings_->developer = true;
                settings_->save(settings_path_);
                build_settings();
                for (int i = 0; i < int(rows_.size()); ++i)
                    if (rows_[std::size_t(i)].key == "creator")
                        settings_row_ = i;
                sfx(Sound::LaunchGame);
                return Action::SettingsChanged;
            }
        }
        sfx(Sound::MenuScroll);
        return Action::None;
    }
    if (!row.bool_value && !row.int_value)
        return Action::None;
    Action action = Action::None;
    const Action changed = (row.rescan && !game) ? Action::Rescan : Action::SettingsChanged;
    if (left)
    {
        change_setting(-1);
        action = changed;
        sfx(Sound::MenuScroll);
    }
    if (right || pressed(BtnCross))
    {
        change_setting(+1);
        action = changed;
        sfx(Sound::MenuScroll);
    }
    return game ? Action::None : action;
}

/* ---- drawing ------------------------------------------------------------------------------- */

void App::draw_settings()
{
    Gfx &g = *g_;
    const bool game = screen_ == Screen::GameSettings;
    const std::string current = on_rail_ ? section_name(rail_) : rows_[std::size_t(settings_row_)].section;

    /* Section rail. */
    const float rx = 90, ry = 136, rw = 380, rh = 800;
    g.panel(rx, ry, rw, rh, rgba(0x0A1236, 0.62f), 0.85f, kR, rgba(0x3D4F9E, 0.9f), 1.6f, 0, 0.10f);
    float sy = ry + 18;
    if (game && game_for_)
    {
        g.text_mid(Font::SemiBold, ts(22), rx + 30, sy + 18, kLavender, Align::Left, tr("GAME SETTINGS"), 2.0f);
        g.text_mid(Font::Bold, ts(28), rx + 30, sy + 56, kWhite, Align::Left,
                   fit(g, Font::Bold, ts(28), game_for_->title, rw - 60));
        sy += 92;
    }

    /* The highlights glide from where they were to where they go. */
    const float glide_dt = float(std::clamp(time_ - glide_time_, 0.0, 0.1));
    glide_time_ = time_;
    auto glide = [&](float &at, float target, float jump) {
        if (at < 0 || settings_->reduced_motion || std::fabs(target - at) > jump)
            at = target;
        else
            at += (target - at) * std::min(1.0f, glide_dt * 16.0f);
        return at;
    };
    {
        float hy = sy;
        for (const SettingRow &r : rows_)
            if (r.header)
            {
                if (r.section == current)
                    break;
                hy += 68;
            }
        const float y = glide(rail_glide_, hy, 2000);
        if (on_rail_)
            g.panel(rx + 14, y, rw - 28, 60, rgba(0x1F63F0), 0.62f, kR, rgba(0x7FD9FF), 1.6f, 6, 0.35f);
        else
            g.panel(rx + 14, y, rw - 28, 60, rgba(0x1F63F0, 0.25f), 0.8f, kR, rgba(0x7FD9FF, 0.55f), 1.4f);
    }
    for (const SettingRow &r : rows_)
    {
        if (!r.header)
            continue;
        const bool here = r.section == current;
        const float ih = 60;
        g.text_mid(here ? Font::Bold : Font::SemiBold, ts(29), rx + 46, sy + ih * 0.5f, here ? kWhite : kSoft,
                   Align::Left, tr(r.section));
        if (here && on_rail_)
            g.glyph(Glyph::Arrow, rx + rw - 46, sy + ih * 0.5f, 20, kWhite, kPi * 0.5f);
        sy += ih + 8;

    }

    /* The section's rows. */
    const float px = 500, py = 136, pw = 1330, ph = 800;
    g.panel(px, py, pw, ph, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    const float title_w = g.text_mid(starcube() ? Font::ExtraBold : Font::Bold, ts(46), px + 50, py + 64, kWhite,
                                     Align::Left, tr(current));
    if (current == "Recommended")
    {
        /* A beta: the list grows as games are tested. */
        const std::string beta = tr("Beta");
        const float bw = g.measure(Font::Bold, ts(20), beta) + 28;
        g.panel(px + 50 + title_w + 18, py + 64 - 17, bw, 34, rgba(0x6A3FD8, 0.35f), 1, 17, rgba(0xB89BFF), 1.6f);
        g.text_mid(Font::Bold, ts(20), px + 50 + title_w + 18 + bw * 0.5f, py + 64, rgba(0xE4DAFF), Align::Center, beta);
    }
    std::string subtitle = tr("Changes apply the next time a game starts");
    if (current == "About")
        subtitle = tr("Porpoise for PS5") + " \xE2\x80\xA2 " + tr("created by @elripalda") + " \xE2\x80\xA2 ripalda.dev";
    else if (current == "Games")
        subtitle = tr("Where Porpoise looks for games, and what it downloads for them");
    else if (current == "This game")
        subtitle = tr("Values in blue are this game's own");
    else if (current == "Interface")
        subtitle = tr("How Porpoise looks and reads");
    else if (current == "Accessibility")
        subtitle = tr("Easier to see, read and follow");
    else if (current == "Developer")
        subtitle = tr("For tuning the Wii Remote; nothing here is needed to play");
    else if (current == "Recommended")
        subtitle = tr("Green is on for this game \xE2\x80\xA2 changes apply the next time it starts");
    else if (game)
        subtitle = tr("For this game only \xE2\x80\xA2 values in blue are its own");
    g.text_mid(Font::Regular, ts(26), px + 50, py + 112, kLavender, Align::Left, subtitle);

    const float row_h = 80, row_x = px + 26, row_w = pw - 52;
    float y = py + 158;
    std::vector<int> section_rows;
    for (int i = 0; i < int(rows_.size()); ++i)
        if (!rows_[std::size_t(i)].header && rows_[std::size_t(i)].section == current)
            section_rows.push_back(i);
    /* Six rows fit; a longer section scrolls with the focus. */
    constexpr std::size_t kVisible = 6;
    std::size_t first = 0;
    for (std::size_t k = 0; k < section_rows.size(); ++k)
        if (!on_rail_ && section_rows[k] == settings_row_ && k >= kVisible)
            first = k - kVisible + 1;
    if (on_rail_)
        row_glide_ = -1;
    else
        for (std::size_t k = first; k < section_rows.size() && k < first + kVisible; ++k)
            if (section_rows[k] == settings_row_)
            {
                const float hy = glide(row_glide_, y + float(k - first) * row_h, row_h * 3.5f);
                g.panel(row_x, hy + 4, row_w, row_h - 8, rgba(0x1D45B8, 0.88f), 0.7f, kR, kIcy, 2.4f, 10, 0.18f);
            }
    for (std::size_t k = first; k < section_rows.size() && k < first + kVisible; ++k)
    {
        const int i = section_rows[k];
        const SettingRow &r = rows_[std::size_t(i)];
        const bool on = !on_rail_ && i == settings_row_;
        const bool own = game && !r.key.empty() &&
                         std::find(game_keys_.begin(), game_keys_.end(), r.key) != game_keys_.end();
        const float cy = y + row_h * 0.5f;
        if (on)
            ; /* the gliding highlight, above */
        else if (k + 1 < section_rows.size() && k + 1 < first + kVisible && (on_rail_ || section_rows[k + 1] != settings_row_))
            g.panel(row_x + 24, y + row_h - 1, row_w - 48, 1.5f, rgba(0x3D4F9E, 0.55f), 1, 0);
        {
            const std::string shown = fit(g, Font::SemiBold, ts(30), r.label, row_w - (r.beta ? 540 : 420));
            g.text_mid(Font::SemiBold, ts(30), row_x + 28, cy, on ? kWhite : (on_rail_ ? with_alpha(kSoft, 0.8f) : kSoft),
                       Align::Left, shown);
            if (r.beta)
            {
                /* The BETA badge: new, and still being tuned. */
                const float bx = row_x + 28 + g.measure(Font::SemiBold, ts(30), shown) + 14;
                const float bw = g.measure(Font::Bold, ts(18), "BETA") + 22, bh = 30;
                g.panel(bx, cy - bh * 0.5f, bw, bh, rgba(0xFFB347, on ? 0.95f : 0.8f), 1.0f, bh * 0.5f);
                g.text_mid(Font::Bold, ts(18), bx + bw * 0.5f, cy, rgba(0x2A1600), Align::Center, "BETA");
            }
        }

        std::string value;
        int vi = 0;
        const int count = int(r.values.size());
        if (r.bool_value)
            vi = *r.bool_value ? 1 : 0;
        else if (r.int_value && !r.order.empty())
            vi = int(std::find(r.order.begin(), r.order.end(), *r.int_value) - r.order.begin());
        else if (r.int_value)
            vi = *r.int_value - r.min;
        if (vi >= 0 && vi < count)
            value = r.values[std::size_t(vi)];
        if (r.key == "players")
        {
            const int n = porpoise::pad::connected_count();
            value = n <= 1 ? tr("1 player") : trf("{n} players", {{"n", std::to_string(n)}});
        }
        const float right = row_x + row_w - 24;
        const Color value_c = own ? kCyan : (on ? kWhite : kSoft);
        if (r.action == kRowUpdate)
            value = update_row_value();
        if (r.action)
        {
            const bool danger = r.action == kRowResetAll || r.action == kRowResetGame || r.action == kRowReinitialize;
            const float vw = g.measure(Font::Bold, ts(28), value);
            const float cw = std::max(250.0f, vw + 110), ch = 50, cx = right - cw;
            g.panel(cx, cy - ch * 0.5f, cw, ch, on ? (danger ? rgba(0xB0305A, 0.85f) : rgba(0x1F63F0, 0.85f))
                                                  : rgba(0x07102E, 0.40f),
                    0.7f, kR, on ? (danger ? kDanger : rgba(0x8BD9FF)) : rgba(0x3D5AB0, 0.75f), on ? 1.8f : 1.4f, 0,
                    on ? 0.25f : 0.0f);
            if (on)
            {
                const float group = 30 + 10 + vw;
                g.glyph(Glyph::Cross, cx + cw * 0.5f - group * 0.5f + 15, cy, 30, kWhite);
                g.text_mid(Font::Bold, ts(28), cx + cw * 0.5f - group * 0.5f + 40, cy, kWhite, Align::Left, value);
            }
            else
                g.text_mid(Font::Bold, ts(28), cx + cw * 0.5f, cy, kSoft, Align::Center, value);
        }
        else if (r.toggle >= 0)
        {
            /* A switch: a green track with the knob right when on. */
            const bool lit = r.toggle == 1;
            const float tw = 92, th = 44, tx = right - tw;
            g.panel(tx, cy - th * 0.5f, tw, th, lit ? rgba(0x2FB574, 0.95f) : rgba(0x07102E, 0.6f), 0.85f, th * 0.5f,
                    lit ? rgba(0xBDF5D8) : (on ? rgba(0x8BD9FF) : rgba(0x3D5AB0, 0.85f)), on ? 2.0f : 1.4f,
                    lit ? 8 : 0);
            const float kx = lit ? tx + tw - th * 0.5f : tx + th * 0.5f;
            g.blob(kx, cy, th * 0.9f, th * 0.9f, rgba(0xFFFFFF, lit ? 0.35f : 0.15f));
            g.panel(kx - th * 0.38f, cy - th * 0.38f, th * 0.76f, th * 0.76f, lit ? kWhite : rgba(0xA9B8E8), 1,
                    th * 0.38f);
            const float ow = g.text_mid(Font::Bold, ts(26), tx - 18, cy, lit ? rgba(0x7CF0B4) : kLavender,
                                        Align::Right, lit ? tr("On") : tr("Off"));
            if (!r.tag.empty())
            {
                /* Where it comes from, in a small pill. */
                const bool dolphin = r.rec >= 0 && rec_rows_[std::size_t(r.rec)].kind == RecRow::DolphinFix;
                const float pw2 = g.measure(Font::SemiBold, ts(19), r.tag) + 26, px2 = tx - 18 - ow - 22 - pw2;
                g.panel(px2, cy - 15, pw2, 30, dolphin ? rgba(0x2A2F6E, 0.8f) : rgba(0x0E3A6E, 0.8f), 1, 15,
                        dolphin ? rgba(0xA9A0FF, 0.8f) : with_alpha(kCyan, 0.8f), 1.2f);
                g.text_mid(Font::SemiBold, ts(19), px2 + pw2 * 0.5f, cy, dolphin ? rgba(0xCFC8FF) : kCyan,
                           Align::Center, r.tag);
            }
        }
        else if (!r.bool_value && !r.int_value)
        {
            const float vw = g.text_mid(Font::SemiBold, ts(28), right, cy, on ? kWhite : kSoft, Align::Right, value);
            if (r.key == "creator")
            {
                /* Ruben's mark beside his name. */
                if (!mark_tried_)
                {
                    mark_tried_ = true;
                    ripalda_ = g.texture_file(g.asset_dir() + "/brand/ripalda.png");
                }
                if (ripalda_)
                    g.image(ripalda_, right - vw - 58, cy - 22, 44, 44, on ? kWhite : kSoft);
            }
        }
        else
        {
            const float vw = g.measure(Font::Bold, ts(28), value);
            /* Room for the arrows, and for the flag beside a language's name. */
            const float flag_w = r.key == "ui_language" ? 54.0f : 0.0f;
            const float cw = std::max(270.0f, vw + 110 + flag_w), ch = 50, cx = right - cw;
            g.panel(cx, cy - ch * 0.5f, cw, ch, rgba(0x07102E, on ? 0.55f : 0.40f), 1, kR,
                    own ? with_alpha(kCyan, 0.9f) : (on ? rgba(0x8BD9FF) : rgba(0x3D5AB0, 0.75f)), on ? 1.8f : 1.4f);
            if (r.key == "ui_language" && r.int_value)
            {
                /* The language's flag beside its name; System shows the one it follows. */
                if (!flags_tried_)
                {
                    flags_tried_ = true;
                    flags_ = g.texture_file(g.asset_dir() + "/ui/flags.png", 2048);
                }
                const int lang = *r.int_value > 0 ? *r.int_value : int(language()) + 1;
                const float vw = g.measure(Font::Bold, ts(28), value), fw = 42, fh = 28, gap = 12;
                const float x0 = cx + (cw - (fw + gap + vw)) * 0.5f;
                if (flags_ && lang >= 1 && lang <= kLanguages)
                {
                    const float uv[4] = {float(lang - 1) / float(kLanguages), 0, float(lang) / float(kLanguages), 1};
                    g.image_part(flags_, x0, cy - fh * 0.5f, fw, fh, uv);
                }
                g.text_mid(Font::Bold, ts(28), x0 + fw + gap, cy, value_c, Align::Left, value);
            }
            else
                g.text_mid(Font::Bold, ts(28), cx + cw * 0.5f, cy, value_c, Align::Center, value);
            if (on)
            {
                const bool at_min = r.int_value && vi <= 0;
                const bool at_max = r.int_value && vi >= count - 1;
                g.glyph(Glyph::Arrow, cx + 26, cy, 18, with_alpha(kCyan, at_min ? 0.3f : 1.0f), -kPi * 0.5f);
                g.glyph(Glyph::Arrow, cx + cw - 26, cy, 18, with_alpha(kCyan, at_max ? 0.3f : 1.0f), kPi * 0.5f);
            }
        }
        y += row_h;
    }
    if (first > 0)
        g.glyph(Glyph::Arrow, px + pw - 30, py + 170, 18, rgba(0x58B8FF), 0);
    if (first + kVisible < section_rows.size())
        g.glyph(Glyph::Arrow, px + pw - 30, py + ph - 120, 18, rgba(0x58B8FF), kPi);

    /* What the focused row does. */
    std::string help;
    if (on_rail_)
        help = tr("Choose a section with up and down, then press Right or Cross to go into it.");
    else
        help = rows_[std::size_t(settings_row_)].help;
    if (!on_rail_ &&
        (rows_[std::size_t(settings_row_)].action == kRowUpdate || rows_[std::size_t(settings_row_)].key == "creator") &&
        !update_note_.empty() &&
        time_ - update_note_time_ < 8.0)
        help = update_note_;
    g.panel(px + 50, py + ph - 100, pw - 100, 1.5f, rgba(0x3D4F9E, 0.7f), 1, 0);
    {
        /* One line, or two smaller ones when it is long. */
        const auto one = wrap(g, Font::Regular, ts(26), help, pw - 100, 2);
        if (one.size() <= 1)
            g.text_mid(Font::Regular, ts(26), px + 50, py + ph - 50, kLavender, Align::Left, help);
        else
        {
            const auto two = wrap(g, Font::Regular, ts(23), help, pw - 100, 2);
            for (std::size_t i = 0; i < two.size(); ++i)
                g.text_mid(Font::Regular, ts(23), px + 50, py + ph - 66 + float(i) * 32, kLavender, Align::Left,
                           two[i]);
        }
    }
    /* About > Discord and Report a bug: a code to the Discord, for a phone. */
    if (!on_rail_ && settings_row_ >= 0 && settings_row_ < int(rows_.size()) &&
        (rows_[std::size_t(settings_row_)].label == tr("Report a bug") ||
         rows_[std::size_t(settings_row_)].key == "discord"))
    {
        if (!qr_tried_)
        {
            qr_tried_ = true;
            qr_ = g.texture_file(g.asset_dir() + "/ui/report-qr.png");
        }
        if (qr_)
        {
            const float qs = 118, qx = px + pw - qs - 70, qy = py + 22; /* above the first row */
            g.panel(qx - 8, qy - 8, qs + 16, qs + 16, rgba(0x07102E, 0.5f), 1, kR, with_alpha(kCyan, 0.9f), 1.6f, 8);
            g.image(qr_, qx, qy, qs, qs, kWhite, 10);
        }
    }
    /* About > Created by: Ruben's mark, large, where the code shows. */
    if (!on_rail_ && settings_row_ >= 0 && settings_row_ < int(rows_.size()) &&
        rows_[std::size_t(settings_row_)].key == "creator" && ripalda_)
    {
        const float qs = 118, qx = px + pw - qs - 70, qy = py + 22;
        g.panel(qx - 8, qy - 8, qs + 16, qs + 16, rgba(0x07102E, 0.7f), 1, kR, with_alpha(kCyan, 0.9f), 1.6f, 8);
        g.image(ripalda_, qx + 12, qy + 12, qs - 24, qs - 24, kWhite);
    }

    if (on_rail_)
    {
        draw_prompts({{Glyph::DPad, "Sections"}, {Glyph::Cross, "Open"}, {Glyph::Circle, game ? "Details" : "Back"}},
                     {}, "");
        return;
    }
    const SettingRow &focus = rows_[std::size_t(settings_row_)];
    const bool info = !focus.bool_value && !focus.int_value;
    if (focus.action == kRowAddFolder)
        draw_prompts({{Glyph::Cross, "Choose a folder"}, {Glyph::Circle, "Sections"}}, {}, "");
    else if (focus.action == kRowRemoveFolder)
        draw_prompts({{Glyph::Cross, "Remove"}, {Glyph::Circle, "Sections"}}, {}, "");
    else if (focus.action == kRowRescan)
        draw_prompts({{Glyph::Cross, "Search"}, {Glyph::Circle, "Sections"}}, {}, "");
    else if (focus.action == kRowMapping)
        draw_prompts({{Glyph::Cross, "Customize"}, {Glyph::Circle, "Sections"}}, {}, "");
    else if (focus.action == kRowWiiGuide)
        draw_prompts({{Glyph::Cross, "Show"}, {Glyph::Circle, "Sections"}}, {}, "");
    else if (focus.action == kRowWiiSetup)
        draw_prompts({{Glyph::Cross, "Start"}, {Glyph::Circle, "Sections"}}, {}, "");
    else if (focus.action == kRowUpdate)
        draw_prompts({{Glyph::Cross, update_available() ? "Install" : "Check now"}, {Glyph::Circle, "Sections"}}, {},
                     "");
    else if (focus.action == kRowUseSetup)
        draw_prompts({{Glyph::Cross, "Use"}, {Glyph::Circle, "Sections"}}, {}, "");
    else if (focus.toggle >= 0)
        draw_prompts({{Glyph::Cross, focus.toggle ? "Turn off" : "Turn on"}, {Glyph::Circle, "Sections"}}, {}, "");
    else if (focus.action)
    {
        /* The button says what the row's own button says ("Check…" → Check). */
        std::string verb = focus.values.empty() ? std::string("Select") : focus.values.front();
        const std::string dots = "\xE2\x80\xA6";
        if (verb.size() >= dots.size() && verb.compare(verb.size() - dots.size(), dots.size(), dots) == 0)
            verb.resize(verb.size() - dots.size());
        if (verb.empty())
            verb = "Select";
        draw_prompts({{Glyph::Cross, verb}, {Glyph::Circle, "Sections"}}, {}, "");
    }
    else if (info)
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Circle, "Sections"}}, {}, "");
    else
        draw_prompts({{Glyph::DPad, "Change"}, {Glyph::Circle, "Sections"}}, {}, "");
}

/* ---- game folders ------------------------------------------------------------------------ */

namespace
{
bool is_dir(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string lower_copy(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}

/* Whether a folder has anything in it (an empty USB port's mount point
 * doesn't). */
bool has_entries(const std::string &path)
{
    bool any = false;
    if (DIR *d = opendir(path.c_str()))
    {
        while (dirent *e = readdir(d))
            if (e->d_name[0] != '.')
            {
                any = true;
                break;
            }
        closedir(d);
    }
    return any;
}

std::string size_text(long long bytes)
{
    char buf[32];
    if (bytes >= (1LL << 30))
        std::snprintf(buf, sizeof buf, "%.1f GB", double(bytes) / double(1LL << 30));
    else
        std::snprintf(buf, sizeof buf, "%lld MB", (bytes + (1LL << 20) - 1) >> 20);
    return buf;
}

/* ---- copying a game to the console ---- */

struct GameCopy
{
    std::string from, to;
    std::atomic<long long> done{0}, total{0};
    std::atomic<int> state{0}; /* 0 idle, 1 copying, 2 done, 3 failed, 4 stopped */
    std::atomic<bool> stop{false};
    std::string error;
    pthread_t thread{};
    bool joinable = false;
};
GameCopy g_copy;

void *copy_worker(void *)
{
    const std::string part = g_copy.to + ".part";
    std::FILE *in = std::fopen(g_copy.from.c_str(), "rb");
    std::FILE *out = in ? std::fopen(part.c_str(), "wb") : nullptr;
    bool ok = in && out;
    std::vector<char> buf(std::size_t(4) << 20);
    while (ok && !g_copy.stop.load())
    {
        const std::size_t n = std::fread(buf.data(), 1, buf.size(), in);
        if (n == 0)
            break;
        ok = std::fwrite(buf.data(), 1, n, out) == n;
        g_copy.done += (long long)n;
    }
    if (in)
    {
        ok = ok && !std::ferror(in);
        std::fclose(in);
    }
    if (out)
        ok = std::fclose(out) == 0 && ok;
    if (g_copy.stop.load())
    {
        std::remove(part.c_str());
        g_copy.state = 4;
        return nullptr;
    }
    if (!ok || std::rename(part.c_str(), g_copy.to.c_str()) != 0)
    {
        std::remove(part.c_str());
        g_copy.error = "The copy didn't finish (the console may be full). Nothing was left behind.";
        g_copy.state = 3;
        return nullptr;
    }
    g_copy.state = 2;
    return nullptr;
}

void finish_copy_thread()
{
    if (g_copy.joinable)
    {
        pthread_join(g_copy.thread, nullptr);
        g_copy.joinable = false;
    }
}
} // namespace

/* Where to start, and the shortcuts Triangle shows: the console's storage,
 * Porpoise's own games folder, and the drives that have something on them. */
std::vector<App::BrowseEntry> App::browse_places() const
{
    std::vector<BrowseEntry> out;
    auto add = [&](const std::string &label, const std::string &path) {
        BrowseEntry e;
        e.kind = BrowseEntry::Place;
        e.label = label;
        e.path = path;
        out.push_back(e);
    };
    if (is_dir(data_dir_ + "/games") && data_dir_ != "/app0/porpoise")
        add(tr("Porpoise's games folder"), data_dir_ + "/games");
    if (is_dir("/data"))
        add(tr("Console storage"), "/data");
    for (int i = 0; i < 8; ++i)
    {
        const std::string p = "/mnt/usb" + std::to_string(i);
        if (is_dir(p) && has_entries(p))
            add(trf("USB drive {n}", {{"n", std::to_string(i + 1)}}), p);
    }
    for (int i = 0; i < 2; ++i)
    {
        const std::string p = "/mnt/ext" + std::to_string(i);
        if (is_dir(p) && has_entries(p))
            add(tr("Extended storage") + std::string(i ? " 2" : ""), p);
    }
    add(tr("Whole system"), "/");
    return out;
}

void App::open_browser(std::string path)
{
    if (screen_ != Screen::Browse)
        open_screen(Screen::Browse);
    browse_path_ = path;
    browse_entries_.clear();
    browse_row_ = 0;
    browse_first_ = 0;
    browse_games_ = 0;
    browse_unreadable_ = false;
    if (path.empty())
    {
        browse_entries_ = browse_places();
        return;
    }
    std::vector<BrowseEntry> folders, games;
    if (DIR *d = opendir(path.c_str()))
    {
        while (dirent *e = readdir(d))
        {
            const std::string name = e->d_name;
            if (name.empty() || name[0] == '.')
                continue;
            const std::string full = path == "/" ? "/" + name : path + "/" + name;
            /* The entry's own type when the file system gives it; stat
             * otherwise (links, and file systems that don't say). A folder
             * stat can't look into still shows. */
            struct stat st;
            const bool stat_ok = stat(full.c_str(), &st) == 0;
            const bool folder = e->d_type == DT_DIR || (stat_ok && S_ISDIR(st.st_mode));
            BrowseEntry b;
            b.label = name;
            b.path = full;
            if (folder)
            {
                b.kind = BrowseEntry::Folder;
                folders.push_back(b);
            }
            else if (is_game_name(name))
            {
                b.kind = BrowseEntry::Game;
                b.size = stat_ok ? (long long)st.st_size : 0;
                games.push_back(b);
            }
        }
        closedir(d);
    }
    else
        browse_unreadable_ = true;
    auto by_name = [](const BrowseEntry &a, const BrowseEntry &b) { return lower_copy(a.label) < lower_copy(b.label); };
    std::sort(folders.begin(), folders.end(), by_name);
    std::sort(games.begin(), games.end(), by_name);
    /* How many games each folder holds right inside it (not on "/", where
     * the folders are the system's own). */
    if (path != "/")
        for (BrowseEntry &f : folders)
            f.games = count_games(f.path, 0);
    browse_games_ = int(games.size());
    browse_entries_ = std::move(folders);
    browse_entries_.insert(browse_entries_.end(), games.begin(), games.end());
}

void App::start_game_copy(const std::string &from)
{
    finish_copy_thread();
    const std::string dir = data_dir_ + "/games";
    mkdir(dir.c_str(), 0777);
    g_copy.from = from;
    g_copy.to = dir + from.substr(from.rfind('/'));
    g_copy.done = 0;
    struct stat st;
    g_copy.total = stat(from.c_str(), &st) == 0 ? (long long)st.st_size : 0;
    g_copy.stop = false;
    g_copy.error.clear();
    g_copy.state = 1;
    browse_copy_name_ = from.substr(from.rfind('/') + 1);
    if (create_title_thread(&g_copy.thread, copy_worker, nullptr) == 0)
        g_copy.joinable = true;
    else
        copy_worker(nullptr);
}

App::Action App::update_browser(bool up, bool down)
{
    /* A game being copied holds the screen; Circle stops it. */
    const int copy_state = g_copy.state.load();
    if (copy_state == 1)
    {
        if (pressed(BtnCircle))
            g_copy.stop = true;
        return Action::None;
    }
    if (copy_state >= 2)
    {
        finish_copy_thread();
        g_copy.state = 0;
        if (copy_state == 2)
        {
            open_dialog(DialogKind::Info, tr("Copied to the console"),
                        trf("{game} is in Porpoise's games folder now, and in your library.",
                            {{"game", browse_copy_name_}}),
                        "");
            open_browser(browse_path_);
            return Action::Rescan;
        }
        if (copy_state == 3)
            open_dialog(DialogKind::Info, tr("The copy didn't finish"), tr(g_copy.error), "");
        return Action::None;
    }

    const int n = int(browse_entries_.size());
    if (up && browse_row_ > 0)
    {
        --browse_row_;
        sfx(Sound::MenuScroll);
    }
    if (down && browse_row_ + 1 < n)
    {
        ++browse_row_;
        sfx(Sound::MenuScroll);
    }
    if (pressed(BtnCross) && browse_row_ < n)
    {
        const BrowseEntry &e = browse_entries_[std::size_t(browse_row_)];
        if (e.kind != BrowseEntry::Game)
        {
            open_browser(e.path);
            return Action::None;
        }
        /* A game: offer to copy it to the console's own storage, unless it's
         * there already. */
        const std::string home = data_dir_ + "/games";
        if (data_dir_ == "/app0/porpoise" || browse_path_ == home)
        {
            sfx(Sound::MovingTab);
            return Action::None;
        }
        browse_copy_name_ = e.path;
        open_dialog(DialogKind::CopyGame, tr("Copy this game to the console?"),
                    trf("{game} ({size}) is copied to Porpoise's games folder on the console, where Porpoise always "
                        "finds it. The original stays where it is.",
                        {{"game", e.label}, {"size", size_text(e.size)}}),
                    tr("Copy"));
        return Action::None;
    }
    if (pressed(BtnTriangle))
    {
        sfx(Sound::MovingTab);
        open_browser(browse_path_.empty() ? "/" : "");
        return Action::None;
    }
    if (pressed(BtnSquare) && !browse_path_.empty() && browse_path_ != "/")
    {
        std::string folder = browse_path_;
        while (folder.size() > 1 && folder.back() == '/')
            folder.pop_back();
        if (std::find(settings_->folders.begin(), settings_->folders.end(), folder) == settings_->folders.end())
            settings_->folders.push_back(folder);
        settings_->save(settings_path_);
        build_settings();
        for (int i = 0; i < int(rows_.size()); ++i)
            if (rows_[std::size_t(i)].action == kRowRemoveFolder && rows_[std::size_t(i)].label == folder)
                settings_row_ = i;
        open_screen(Screen::Main);
        tab_ = Tab::Settings;
        on_rail_ = false;
        return Action::Rescan;
    }
    if (pressed(BtnCircle))
    {
        /* Up a level; from "/" or the shortcuts, out. */
        if (browse_path_.empty() || browse_path_ == "/")
        {
            open_screen(Screen::Main);
            return Action::None;
        }
        const std::string from = browse_path_;
        std::string parent = from.substr(0, from.rfind('/'));
        if (parent.empty())
            parent = "/";
        open_browser(parent);
        for (int i = 0; i < int(browse_entries_.size()); ++i)
            if (browse_entries_[std::size_t(i)].path == from)
                browse_row_ = i;
    }
    return Action::None;
}

void App::draw_browser()
{
    Gfx &g = *g_;
    const float x = 190, y = 136, w = 1540, h = 800;
    g.panel(x, y, w, h, rgba(0x0F1F63, 0.66f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    g.text_mid(Font::Bold, ts(44), x + 50, y + 62, kWhite, Align::Left, tr("Choose a game folder"));
    const std::string where = browse_path_.empty() ? tr("Drives and shortcuts") : browse_path_;
    g.text_mid(Font::SemiBold, ts(26), x + 50, y + 110, kIcy, Align::Left, fit(g, Font::SemiBold, ts(26), where, w - 520));
    if (!browse_path_.empty() && browse_path_ != "/")
    {
        const std::string count = browse_games_ == 0 ? tr("No games right here")
                                  : plural(browse_games_, "1 game right here", "{n} games right here");
        g.text_mid(Font::SemiBold, ts(26), x + w - 50, y + 110, browse_games_ ? kCyan : kLavender, Align::Right, count);
    }
    g.panel(x + 50, y + 146, w - 100, 1.5f, rgba(0x3D4F9E, 0.7f), 1, 0);

    const int n = int(browse_entries_.size());
    constexpr int kVisible = 8;
    if (browse_row_ < browse_first_)
        browse_first_ = browse_row_;
    if (browse_row_ >= browse_first_ + kVisible)
        browse_first_ = browse_row_ - kVisible + 1;
    const float row_h = 72, rx = x + 26, rw = w - 52;
    float ry = y + 166;
    if (n == 0)
        g.text_mid(Font::SemiBold, ts(30), x + w * 0.5f, y + 380, kSoft, Align::Center,
                   browse_unreadable_ ? tr("Porpoise can't open this folder") : tr("This folder is empty"));
    for (int i = browse_first_; i < n && i < browse_first_ + kVisible; ++i)
    {
        const BrowseEntry &e = browse_entries_[std::size_t(i)];
        const bool on = i == browse_row_;
        const bool game = e.kind == BrowseEntry::Game;
        const float cy = ry + row_h * 0.5f;
        if (on)
            g.panel(rx, ry + 4, rw, row_h - 8, rgba(game ? 0x0E5A8A : 0x1D45B8, 0.88f), 0.7f, kR, kIcy, 2.4f, 10, 0.18f);
        if (game)
        {
            /* A little disc. */
            const Color dc = on ? kWhite : kCyan;
            g.panel(rx + 34, cy - 16, 32, 32, with_alpha(dc, 0.9f), 1, 16);
            g.panel(rx + 45, cy - 5, 10, 10, rgba(0x0F1F63), 1, 5);
        }
        else
        {
            /* A little folder (a drive is a folder too). */
            const Color fc = on ? kIcy : with_alpha(kCyan, 0.75f);
            g.panel(rx + 30, cy - 15, 18, 8, fc, 1, 3);
            g.panel(rx + 30, cy - 10, 40, 26, fc, 0.8f, 4);
        }
        const float right_w = game ? 170.0f : (e.games > 0 ? 260.0f : 80.0f);
        g.text_mid(Font::SemiBold, ts(30), rx + 92, cy, game ? (on ? kWhite : kCyan) : (on ? kWhite : kSoft),
                   Align::Left, fit(g, Font::SemiBold, ts(30), e.label, rw - 120 - right_w));
        if (game)
            g.text_mid(Font::Regular, ts(24), rx + rw - 30, cy, on ? kWhite : kLavender, Align::Right,
                       size_text(e.size));
        else
        {
            if (e.games > 0)
                g.text_mid(Font::SemiBold, ts(22), rx + rw - 70, cy, kCyan, Align::Right,
                           plural(e.games, "1 game", "{n} games"));
            if (on)
                g.glyph(Glyph::Arrow, rx + rw - 36, cy, 22, kCyan, kPi * 0.5f);
        }
        ry += row_h;
    }
    if (browse_first_ > 0)
        g.glyph(Glyph::Arrow, x + w - 30, y + 180, 18, rgba(0x58B8FF), 0);
    if (browse_first_ + kVisible < n)
        g.glyph(Glyph::Arrow, x + w - 30, y + h - 30, 18, rgba(0x58B8FF), kPi);

    /* Copying a game: a box over the list. */
    if (g_copy.state.load() == 1)
    {
        g.panel(x, y, w, h, rgba(0x02040C, 0.7f), 1, kR);
        const float bw = 900, bh = 300, bx = 960 - bw * 0.5f, by = 400;
        g.panel(bx, by, bw, bh, rgba(0x13308A, 0.95f), 1, kR, rgba(0x8BD9FF), 2.2f, 12);
        g.text_mid(Font::Bold, ts(36), 960, by + 64, kWhite, Align::Center, tr("Copying to the console"));
        g.text_mid(Font::Regular, ts(24), 960, by + 118, kSoft, Align::Center,
                   fit(g, Font::Regular, ts(24), browse_copy_name_, bw - 100));
        const long long done = g_copy.done.load(), total = std::max(1LL, g_copy.total.load());
        const float frac = std::clamp(float(double(done) / double(total)), 0.0f, 1.0f);
        g.panel(bx + 60, by + 160, bw - 120, 26, rgba(0x07102E, 0.7f), 1, 13, rgba(0x3D5AB0, 0.9f), 1.4f);
        g.panel(bx + 63, by + 163, std::max(20.0f, (bw - 126) * frac), 20, rgba(0x5CD3FF), 0.8f, 10);
        g.text_mid(Font::Regular, ts(22), 960, by + 230, kLavender, Align::Center,
                   size_text(done) + " / " + size_text(total));
        draw_prompts({{Glyph::Circle, "Stop"}}, {}, "");
        return;
    }

    std::vector<std::pair<Glyph, std::string>> right;
    if (!browse_path_.empty() && browse_path_ != "/")
        right.push_back({Glyph::Square, "Use this folder"});
    right.push_back({Glyph::Triangle, browse_path_.empty() ? "Whole system" : "Drives"});
    const bool on_game = browse_row_ < n && browse_entries_[std::size_t(browse_row_)].kind == BrowseEntry::Game;
    const bool can_copy = on_game && data_dir_ != "/app0/porpoise" && browse_path_ != data_dir_ + "/games";
    std::vector<std::pair<Glyph, std::string>> left;
    if (!on_game && n > 0)
        left.push_back({Glyph::Cross, "Open"});
    if (can_copy)
        left.push_back({Glyph::Cross, "Copy to console"});
    left.push_back({Glyph::Circle, browse_path_.empty() || browse_path_ == "/" ? "Cancel" : "Up"});
    draw_prompts(left, right, "");
}

} // namespace porpoise::ui
