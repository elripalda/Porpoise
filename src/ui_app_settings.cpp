/* Porpoise UI - Settings, a game's own settings, and the game folder browser.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings is a rail of sections on the left and the rows of one section on
 * the right. Focus starts on the rail: up and down pick a section, Right or
 * Cross go into it, Circle comes back out. A game's own settings use the same
 * screen with the global values underneath and its changes on top. */
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#include "porpoise_pad.hpp"
#include "ui_app.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

#if defined(__has_include)
#if __has_include("title_build_identity.h")
#include "title_build_identity.h"
#endif
#endif

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

std::string App::build_label() const
{
    std::string label = kVersion;
#ifdef PS5_RETROARCH_BUILD_ID
    const std::string id = PS5_RETROARCH_BUILD_ID;
    const auto colon = id.rfind(": ");
    if (colon != std::string::npos && id.size() >= colon + 10)
        label += "  (" + id.substr(colon + 2, 8) + ")";
#endif
    return label;
}

std::string App::game_settings_path(const Game &g) const
{
    return data_dir_ + "/game-settings/" + Library::key_of(g) + ".ini";
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
        r.label = tr(label);
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
        r.label = tr(label);
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
    toggle("sharp", "Upscaling to the TV", "How Porpoise fits the picture to your TV.", &t.sharp, "Smooth", "Sharp");
    toggle("fps_overlay", "FPS overlay", "Shows the frame rate in the corner while you play.", &t.fps_overlay);

    header("Graphics");
    choice("shader_mode", "Shader compilation",
           "Ubershaders hide the stutter when a game draws something new, at a GPU cost.", &t.shader_mode, 0,
           {"Synchronous", "Ubershaders", "Async ubershaders", "Async, skip drawing"});
    choice("texture_cache", "Texture cache accuracy", "Safe fixes some games' text and effects; Fast is quickest.",
           &t.texture_cache, 0, {"Fast", "Middle", "Safe"});
    toggle("pixel_lighting", "Per-pixel lighting", "Smoother lighting on surfaces. A little heavier.",
           &t.pixel_lighting);
    toggle("disable_fog", "Disable fog", "Removes distance fog. Some games use fog for their look.", &t.disable_fog);
    toggle("crop_overscan", "Crop overscan", "Hides the black borders some games draw at the edges.",
           &t.crop_overscan);
    toggle("custom_textures", "Custom textures",
           "Loads texture packs from /data/porpoise/saves/User/Load/Textures/<game ID>.", &t.custom_textures);
    toggle("skip_dupes", "Skip duplicate frames", "Saves work when a game shows the same frame twice.",
           &t.skip_dupes);

    header("Audio");
    choice("volume", "Game volume", "Volume of the game's sound.", &t.volume, 0,
           {"0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"});
    toggle("muted", "Mute game", "Silences the game.", &t.muted);
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
    toggle("gamecube_layout", "Button layout",
           "GameCube: Cross is A, like confirming on PlayStation. PlayStation: by position.", &t.gamecube_layout,
           "PlayStation", "GameCube");
    toggle("rumble", "Vibration", "Controller rumble.", &t.rumble);

    header("System");
    choice("cpu_clock", "CPU clock", "Overclocking can smooth a game that slows down. 100% is the real console.",
           &t.cpu_clock, 0, {"50%", "60%", "70%", "80%", "90%", "100%", "150%", "200%", "250%", "300%"});
    toggle("dual_core", "Dual core", "Faster. Turn it off for a game that freezes or glitches.", &t.dual_core);
    toggle("fast_disc", "Fast disc loading", "Shorter loading screens. A few games need real disc speed.",
           &t.fast_disc);
    toggle("cheats", "Cheats", "Dolphin's cheat codes for games that have them.", &t.cheats);
    choice("language", "System language", "The console's language. European games show their text in it.",
           &t.language, 0,
           {"English", "Japanese", "German", "French", "Spanish", "Italian", "Dutch", "Chinese (simplified)",
            "Chinese (traditional)", "Korean"});
    toggle("progressive", "Progressive scan", "480p output, as on a component cable.", &t.progressive);
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
        r.label = tr(label);
        r.help = tr(help);
        r.bool_value = value;
        r.values = {tr("Off"), tr("On")};
        rows_.push_back(r);
    };
    auto action = [&](const char *label, const char *help, const std::string &value, int act, int folder = -1) {
        SettingRow r;
        r.section = section;
        r.label = tr(label);
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

    add_game_rows(*settings_, false);

    header("Interface");
    {
        SettingRow r;
        r.section = section;
        r.key = "ui_language";
        r.label = tr("Language");
        r.help = tr("The language of Porpoise's menus. System follows your PS5.");
        r.int_value = &settings_->ui_language;
        for (int i = 0; i < 5; ++i)
            r.values.push_back(language_choice(i));
        rows_.push_back(r);
    }
    toggle("reduced_motion", "Reduced motion", "Stops the moving lights and shortens animations.",
           &settings_->reduced_motion);
    toggle("large_text", "Larger text", "Bigger labels across Porpoise.", &settings_->large_text);
    action("Reset all settings", "Every setting back to how Porpoise ships. Games, folders and saves stay.",
           "Reset\xE2\x80\xA6", kRowResetAll);

    header("About");
    info("Porpoise", build_label(), "A GameCube and Wii player for PS5, in the spirit of the GameCube's own menus.");
    info("Created by", "@elripalda", "Ruben - www.elripalda.com");
    info("Website", "www.elripalda.com", "Updates, news and more from the creator of Porpoise.");
    info("Music and sounds", "@elripalda", "The menu music and sound effects, made for Porpoise by Ruben.");
    info("Emulation", "Dolphin", "Dolphin, by the Dolphin Team - dolphin-emu.org. Free software, GPL v2 or later.");
    info("Dolphin core", "libretro", "Dolphin's libretro core, maintained by the libretro team (GPL v2 or later).");
    info("Core API", "libretro / RetroArch", "Porpoise hosts the core through the libretro API that RetroArch made.");
    info("PS5 graphics", "Mesa RADV", "Vulkan on PS5 through Mesa's RADV driver (MIT), PS5_Mesa and PS5_Vulkan ports.");
    info("PS5 toolchain", "ps5-payload-sdk", "John T\xC3\xB6rnblom's ps5-payload-sdk (GPL v3) and its PS5 ports.");
    info("PS5 port", "Mihawk", "The PS5 Dolphin and RetroArch port work Porpoise is built on.");
    info("Inspired by", "PS5SX2, ProsperoEden", "PS5 homebrew front ends that showed the way.");
    info("Box art and info", "GameTDB.com", "Covers, disc art and game details from GameTDB.com and its contributors.");
    info("Font", "Nunito", "Nunito by Vernon Adams and contributors, SIL Open Font License.");
    info("Images and audio", "stb", "stb_image, stb_truetype and stb_vorbis by Sean Barrett (public domain / MIT).");
    info("Thanks", "PS5 scene", "etaHEN, kstuff and ShadowMountPlus make homebrew like this possible.");
    info("Trademarks", "Nintendo", "GameCube and Wii are trademarks of Nintendo. Porpoise is not affiliated with Nintendo.");

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
    r.values = {game_keys_.empty() ? tr("None yet") : plural((long long)game_keys_.size(), "1 change", "{n} changes")};
    rows_.push_back(r);
    SettingRow reset;
    reset.section = "This game";
    reset.label = tr("Reset to default");
    reset.help = tr("Forgets this game's own settings; it follows your settings again.");
    reset.values = {tr("Reset\xE2\x80\xA6")};
    reset.action = kRowResetGame;
    rows_.push_back(reset);
    add_game_rows(game_, true);
    if (settings_row_ < 0 || settings_row_ >= int(rows_.size()) || rows_[std::size_t(settings_row_)].header)
        settings_row_ = 1;
    rail_ = std::clamp(rail_, 0, std::max(0, section_count() - 1));
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
        *r.int_value = std::clamp(*r.int_value + dir, r.min, r.min + n - 1);
    }
    if (screen_ == Screen::GameSettings && game_for_)
    {
        if (!r.key.empty() && std::find(game_keys_.begin(), game_keys_.end(), r.key) == game_keys_.end())
            game_keys_.push_back(r.key);
        mkdir((data_dir_ + "/game-settings").c_str(), 0777);
        game_.save_keys(game_settings_path(*game_for_), game_keys_);
        rows_[1].values = {plural((long long)game_keys_.size(), "1 change", "{n} changes")};
        return;
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
        open_browser("");
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
    if (row.action)
        return pressed(BtnCross) ? activate_row(row) : Action::None;
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

    for (const SettingRow &r : rows_)
    {
        if (!r.header)
            continue;
        const bool here = r.section == current;
        const float ih = 60;
        if (here && on_rail_)
            g.panel(rx + 14, sy, rw - 28, ih, rgba(0x1F63F0), 0.62f, kR, rgba(0x7FD9FF), 1.6f, 6, 0.35f);
        else if (here)
            g.panel(rx + 14, sy, rw - 28, ih, rgba(0x1F63F0, 0.25f), 0.8f, kR, rgba(0x7FD9FF, 0.55f), 1.4f);
        g.text_mid(here ? Font::Bold : Font::SemiBold, ts(29), rx + 46, sy + ih * 0.5f, here ? kWhite : kSoft,
                   Align::Left, tr(r.section));
        if (here && on_rail_)
            g.glyph(Glyph::Arrow, rx + rw - 46, sy + ih * 0.5f, 20, kWhite, kPi * 0.5f);
        sy += ih + 8;

    }

    /* The section's rows. */
    const float px = 500, py = 136, pw = 1330, ph = 800;
    g.panel(px, py, pw, ph, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    g.text_mid(Font::Bold, ts(46), px + 50, py + 64, kWhite, Align::Left, tr(current));
    std::string subtitle = tr("Changes apply the next time a game starts");
    if (current == "About")
        subtitle = tr("Porpoise for PS5") + " \xE2\x80\xA2 " + tr("created by @elripalda") + " \xE2\x80\xA2 www.elripalda.com";
    else if (current == "Games")
        subtitle = tr("Where Porpoise looks for games, and what it downloads for them");
    else if (current == "This game")
        subtitle = tr("Values in blue are this game's own");
    else if (current == "Interface")
        subtitle = tr("How Porpoise looks and reads");
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
    for (std::size_t k = first; k < section_rows.size() && k < first + kVisible; ++k)
    {
        const int i = section_rows[k];
        const SettingRow &r = rows_[std::size_t(i)];
        const bool on = !on_rail_ && i == settings_row_;
        const bool own = game && !r.key.empty() &&
                         std::find(game_keys_.begin(), game_keys_.end(), r.key) != game_keys_.end();
        const float cy = y + row_h * 0.5f;
        if (on)
            g.panel(row_x, y + 4, row_w, row_h - 8, rgba(0x1D45B8, 0.88f), 0.7f, kR, kIcy, 2.4f, 10, 0.18f);
        else if (k + 1 < section_rows.size() && k + 1 < first + kVisible && (on_rail_ || section_rows[k + 1] != settings_row_))
            g.panel(row_x + 24, y + row_h - 1, row_w - 48, 1.5f, rgba(0x3D4F9E, 0.55f), 1, 0);
        g.text_mid(Font::SemiBold, ts(30), row_x + 28, cy, on ? kWhite : (on_rail_ ? with_alpha(kSoft, 0.8f) : kSoft),
                   Align::Left, fit(g, Font::SemiBold, ts(30), r.label, row_w - 420));

        std::string value;
        int vi = 0;
        const int count = int(r.values.size());
        if (r.bool_value)
            vi = *r.bool_value ? 1 : 0;
        else if (r.int_value)
            vi = *r.int_value - r.min;
        if (vi >= 0 && vi < count)
            value = r.values[std::size_t(vi)];
        const float right = row_x + row_w - 24;
        const Color value_c = own ? kCyan : (on ? kWhite : kSoft);
        if (r.action)
        {
            const bool danger = r.action == kRowResetAll || r.action == kRowResetGame;
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
        else if (!r.bool_value && !r.int_value)
            g.text_mid(Font::SemiBold, ts(28), right, cy, on ? kWhite : kSoft, Align::Right, value);
        else
        {
            const float vw = g.measure(Font::Bold, ts(28), value);
            const float cw = std::max(270.0f, vw + 110), ch = 50, cx = right - cw;
            g.panel(cx, cy - ch * 0.5f, cw, ch, rgba(0x07102E, on ? 0.55f : 0.40f), 1, kR,
                    own ? with_alpha(kCyan, 0.9f) : (on ? rgba(0x8BD9FF) : rgba(0x3D5AB0, 0.75f)), on ? 1.8f : 1.4f);
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
    g.panel(px + 50, py + ph - 100, pw - 100, 1.5f, rgba(0x3D4F9E, 0.7f), 1, 0);
    g.text_mid(Font::Regular, ts(26), px + 50, py + ph - 50, kLavender, Align::Left,
               fit(g, Font::Regular, ts(26), help, pw - 100));

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
    else if (focus.action)
        draw_prompts({{Glyph::Cross, "Reset"}, {Glyph::Circle, "Sections"}}, {}, "");
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

/* The drives, as the browser's top level shows them. */
std::vector<std::pair<std::string, std::string>> places()
{
    std::vector<std::pair<std::string, std::string>> out;
    if (is_dir("/data"))
        out.push_back({tr("Console storage"), "/data"});
    for (int i = 0; i < 8; ++i)
    {
        const std::string p = "/mnt/usb" + std::to_string(i);
        if (is_dir(p))
            out.push_back({trf("USB drive {n}", {{"n", std::to_string(i + 1)}}), p});
    }
    for (int i = 0; i < 2; ++i)
    {
        const std::string p = "/mnt/ext" + std::to_string(i);
        if (is_dir(p))
            out.push_back({tr("Extended storage") + std::string(i ? " 2" : ""), p});
    }
    out.push_back({tr("Whole system"), "/"});
    return out;
}
} // namespace

void App::open_browser(const std::string &path)
{
    if (screen_ != Screen::Browse)
        open_screen(Screen::Browse);
    browse_path_ = path;
    browse_entries_.clear();
    browse_row_ = 0;
    browse_first_ = 0;
    browse_games_ = 0;
    if (path.empty())
    {
        browse_entries_ = places();
        return;
    }
    if (DIR *d = opendir(path.c_str()))
    {
        while (dirent *e = readdir(d))
        {
            const std::string name = e->d_name;
            if (name.empty() || name[0] == '.')
                continue;
            const std::string full = path == "/" ? "/" + name : path + "/" + name;
            if (is_dir(full))
                browse_entries_.push_back({name, full});
        }
        closedir(d);
    }
    std::sort(browse_entries_.begin(), browse_entries_.end(),
              [](const auto &a, const auto &b) { return lower_copy(a.first) < lower_copy(b.first); });
    browse_games_ = path == "/" ? 0 : count_games(path, 0);
}

App::Action App::update_browser(bool up, bool down)
{
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
        open_browser(browse_entries_[std::size_t(browse_row_)].second);
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
        if (browse_path_.empty())
        {
            open_screen(Screen::Main);
            return Action::None;
        }
        /* Up a level; from a drive's top, back to the drives. */
        const std::string from = browse_path_;
        bool is_place = false;
        for (const auto &p : places())
            is_place |= p.second == from;
        std::string parent;
        if (!is_place)
        {
            parent = from.substr(0, from.rfind('/'));
            if (parent.empty())
                parent = "/";
        }
        open_browser(parent);
        for (int i = 0; i < int(browse_entries_.size()); ++i)
            if (browse_entries_[std::size_t(i)].second == from)
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
    const std::string where = browse_path_.empty() ? tr("Pick a drive") : browse_path_;
    g.text_mid(Font::SemiBold, ts(26), x + 50, y + 110, kIcy, Align::Left, fit(g, Font::SemiBold, ts(26), where, w - 420));
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
                   tr("No folders inside this one"));
    for (int i = browse_first_; i < n && i < browse_first_ + kVisible; ++i)
    {
        const bool on = i == browse_row_;
        const float cy = ry + row_h * 0.5f;
        if (on)
            g.panel(rx, ry + 4, rw, row_h - 8, rgba(0x1D45B8, 0.88f), 0.7f, kR, kIcy, 2.4f, 10, 0.18f);
        /* A little folder. */
        const Color fc = on ? kIcy : with_alpha(kCyan, 0.75f);
        g.panel(rx + 30, cy - 15, 18, 8, fc, 1, 3);
        g.panel(rx + 30, cy - 10, 40, 26, fc, 0.8f, 4);
        g.text_mid(Font::SemiBold, ts(30), rx + 92, cy, on ? kWhite : kSoft, Align::Left,
                   fit(g, Font::SemiBold, ts(30), browse_entries_[std::size_t(i)].first, rw - 200));
        if (on)
            g.glyph(Glyph::Arrow, rx + rw - 36, cy, 22, kCyan, kPi * 0.5f);
        ry += row_h;
    }
    if (browse_first_ > 0)
        g.glyph(Glyph::Arrow, x + w - 30, y + 180, 18, rgba(0x58B8FF), 0);
    if (browse_first_ + kVisible < n)
        g.glyph(Glyph::Arrow, x + w - 30, y + h - 30, 18, rgba(0x58B8FF), kPi);

    std::vector<std::pair<Glyph, std::string>> right;
    if (!browse_path_.empty() && browse_path_ != "/")
        right.push_back({Glyph::Square, "Use this folder"});
    draw_prompts({{Glyph::Cross, "Open"}, {Glyph::Circle, browse_path_.empty() ? "Cancel" : "Up"}}, right, "");
}

} // namespace porpoise::ui
