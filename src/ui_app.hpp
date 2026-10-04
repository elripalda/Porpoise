/* Porpoise UI - the launcher: library, details, launch, memory cards, settings.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ui_app.cpp holds the library, details, memory cards, dialogs and launch;
 * ui_app_settings.cpp holds Settings, a game's own settings and the folder
 * browser. ui_app_common.hpp has what they share. */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ui_gfx.hpp"
#include "ui_library.hpp"
#include "ui_memcard.hpp"
#include "ui_settings.hpp"

namespace porpoise::ui
{
/* Physical buttons, as porpoise::pad::Button bits. */
struct Input
{
    std::uint32_t held = 0;
    float stick_x = 0, stick_y = 0; /* -1..1 */
    float right_x = 0, right_y = 0; /* the right stick, -1..1 */
};

/* The launcher's sounds; the host plays them (App only says when). */
enum class Sound
{
    GameRow,
    MenuScroll,
    MovingTab,
    DetailsFlip,
    LaunchGame,
};

class App
{
public:
    enum class Action
    {
        None,
        Launch,      /* launch_game() says which */
        SettingsChanged,
        Rescan,      /* game folders changed: search again */
    };

    void set_sound_hook(void (*play)(Sound)) { sound_ = play; }
    void init(Gfx *gfx, Library *library, Settings *settings, const std::string &settings_path,
              const std::string &options_path, const std::string &saves_dir);
    Action update(const Input &in, double dt);
    void draw(double time);

    Game *launch_game() { return launch_; }
    /* The launch screen, while the emulator starts. progress < 0 = unknown. */
    void begin_launch(Game *g);
    void set_launch_status(const std::string &status, float progress);
    void draw_launch(double time);
    /* Covers live on a device; after a device change they must be reloaded. */
    void forget_textures();
    /* Before the library is searched again: gives the covers' textures back. */
    void release_covers();
    /* After it was searched again. */
    void library_changed();
    /* Art (cover or disc) was downloaded for this disc ID. */
    void cover_arrived(const std::string &id);
    /* A short line under the game count, e.g. "Getting covers 3 of 9". */
    void set_note(const std::string &note) { note_ = note; }
    /* Where a game's own settings are kept (they may not exist). */
    std::string game_settings_path(const Game &g) const;

    /* The in-game menu (Options + touch pad), over the paused game.
     * play: the settings the game runs with; quick changes go there and into
     * the game's own settings file. */
    void open_game_menu(Game *game, Settings *play);
    /* Returns a porpoise::core::Menu answer (0 stay, 1 resume, 2 library, 3 home). */
    int update_game_menu(const Input &in, double dt);
    void draw_game_menu(double time);
    /* The key of a quick setting just changed in the menu ("" when none). */
    std::string take_menu_change()
    {
        std::string k;
        k.swap(menu_change_);
        return k;
    }
    /* Back from a game to the library. */
    void return_from_game();
    /* The menu language changed (rebuilds Settings' rows). */
    void language_changed() { build_settings(); }
    /* A message with an OK button. */
    void show_message(const std::string &title, const std::string &message)
    {
        open_dialog(DialogKind::Info, title, message, "");
    }

private:
    enum class Tab
    {
        Library,
        MemoryCards,
        Settings,
    };
    enum class Screen
    {
        Main,
        Details,
        Sort,
        Browse,       /* choosing a game folder */
        GameSettings, /* one game's own settings */
        Mapping,      /* the buttons: a GameCube input for each DualSense control */
    };
    struct SettingRow
    {
        std::string section, label, help, key;
        std::vector<std::string> values;
        int *int_value = nullptr;
        bool *bool_value = nullptr;
        int min = 0;
        bool header = false;
        int action = 0;      /* look::RowAction */
        int folder = -1;     /* the folder a Remove row is for */
        bool rescan = false; /* changing it searches the library again */
    };
    enum class DialogKind
    {
        Info,
        ResetAll,
        ResetGame,
        DeleteSave,
        CopySave,
    };
    struct Dialog
    {
        bool open = false;
        DialogKind kind = DialogKind::Info;
        std::string title, message, yes; /* yes empty: just OK */
        bool danger = false;
        int choice = 0; /* 0 = cancel, 1 = yes */
        float anim = 0;
    };

    std::uint32_t pressed(std::uint32_t bits) const { return (held_ & ~prev_) & bits; }
    void sfx(Sound s) const
    {
        if (sound_)
            sound_(s);
    }
    bool nav(std::uint32_t bit, float &timer, double dt);
    Texture *cover_of(Game &g);
    Texture *disc_of(Game &g);
    Texture *back_of(Game &g);
    void set_tab(int tab, int dir);
    void open_screen(Screen s);

    /* Settings (ui_app_settings.cpp) */
    void build_settings();
    void build_game_settings();
    void add_game_rows(Settings &target, bool per_game);
    void change_setting(int dir);
    Action update_settings(bool up, bool down, bool left, bool right);
    Action activate_row(const SettingRow &row);
    void draw_settings();
    void open_game_settings(Game &g);
    void close_game_settings();
    int section_count() const;
    std::string section_name(int index) const;
    int first_row_of(const std::string &section) const;
    void open_browser(const std::string &path);
    void draw_browser();
    Action update_browser(bool up, bool down);
    std::string build_label() const;
    long long change_count() const;

    /* Button mapping (ui_app_controls.cpp) */
    void open_mapping();
    void close_mapping();
    Action update_mapping(bool up, bool down);
    void draw_mapping(double time);
    void assign_control(int gc_input, int control);
    void use_layout(int layout);
    void save_mapping(const std::vector<std::string> &keys = {});
    void draw_keycap(float right, float cy, const std::string &label, bool on, float height = 40);

    /* Dialogs */
    void open_dialog(DialogKind kind, const std::string &title, const std::string &message,
                     const std::string &yes, bool danger = false);
    Action update_dialog(bool left, bool right);
    Action confirm_dialog(DialogKind kind);
    void draw_dialog();

    /* Memory cards */
    void scan_memory_cards();
    void move_memcard(int dx, int dy);
    void free_card_textures();
    Save *focused_save();
    void ask_delete_save();
    void ask_copy_save();
    std::string copy_target(const Save &s) const;

    void draw_top_bar();
    float draw_key_pair(float x, float cy, Glyph which, bool measure_only = false);
    void draw_prompts(const std::vector<std::pair<Glyph, std::string>> &left,
                      const std::vector<std::pair<Glyph, std::string>> &right, const std::string &center);
    void draw_library(double time);
    void draw_tile(Game *g, float cx, float cy, float w, float h, float yaw, float alpha, bool focused,
                   bool reflection);
    void draw_details(double time);
    void draw_sort();
    void draw_memory_cards(double time);
    void draw_card(Card &card, int which, float x, float y, double time);
    void draw_brand(float cy);
    /* The Porpoise mark, centred: the textured logo when it is there. */
    bool draw_mark(float cx, float cy, float width, Color tint);
    Texture *save_icon(Save &s);
    Texture *save_banner(Save &s);
    void draw_empty();
    std::string clock_text() const;
    float ts(float size) const { return settings_ && settings_->large_text ? size * 1.15f : size; }

    void (*sound_)(Sound) = nullptr;
    Gfx *g_ = nullptr;
    Library *lib_ = nullptr;
    Settings *settings_ = nullptr;
    std::string settings_path_, options_path_, saves_dir_, data_dir_;

    Tab tab_ = Tab::Library;
    Screen screen_ = Screen::Main;
    int selected_ = 0;
    float scroll_ = 0;     /* animated selection */
    float lift_ = 0;       /* focus lift, 0..1 */
    int details_row_ = 0;
    /* The box on the details page: it flips in from the library, turns with
     * the right stick, and Triangle shows its back. */
    float flip_anim_ = 0;  /* 1 -> 0 as it arrives */
    float box_yaw_ = 0;    /* animated */
    bool box_back_ = false;
    float right_x_ = 0;
    float layer_dx_ = 0, layer_dy_ = 0, layer_fade_ = 1; /* the screen's motion, this frame */
    bool details_custom_ = false; /* the game has its own settings */
    int sort_row_ = 0;

    /* Settings: the rail of sections, then the rows of one. */
    std::vector<SettingRow> rows_;
    bool on_rail_ = true;
    int rail_ = 0;         /* section under focus on the rail */
    int settings_row_ = 1; /* row under focus once inside a section */
    /* A game's own settings: the global ones with its changes on top. */
    Settings game_;
    std::vector<std::string> game_keys_;
    Game *game_for_ = nullptr;

    Card card_a_, card_b_;
    bool cards_scanned_ = false;
    int mc_card_ = 0;             /* 0 = A, 1 = B */
    int mc_sel_[2] = {0, 0};      /* save under focus on each card */
    float mc_scroll_[2] = {0, 0}; /* first visible row */
    std::string mc_focus_code_;   /* focus this game's save on the next scan */
    float mc_lift_ = 0;

    /* In-game menu */
    Game *menu_game_ = nullptr;
    Settings *menu_play_ = nullptr;
    int menu_row_ = 0;
    float menu_anim_ = 0;
    bool menu_closing_ = false;
    int menu_answer_ = 0;
    std::string menu_change_;

    Dialog dialog_;
    bool drawing_dialog_ = false;
    Texture *logo_ = nullptr;
    bool logo_tried_ = false;
    Texture *pad_art_ = nullptr; /* the controller on the mapping screen */
    bool pad_art_tried_ = false;

    /* Button mapping */
    Screen map_return_ = Screen::Main;
    Settings *map_target_ = nullptr; /* the global settings, or a game's */
    int map_row_ = 0;
    bool map_capture_ = false; /* waiting for a button press */
    bool map_armed_ = false;   /* every button was let go since the capture began */
    double map_capture_start_ = 0;
    std::string map_note_; /* a line under the list after a change */
    double map_note_time_ = -10;

    /* Details: L2 / R2 swipe to the previous / next game. */
    float swipe_anim_ = 0; /* 1 -> 0 */
    int swipe_dir_ = 0;
    int swipe_from_ = -1;
    float rep_l2_ = 0, rep_r2_ = 0;
    std::string note_;

    /* Motion: the content slides in when the tab changes; screens fade in. */
    float tab_anim_ = 0;
    int tab_dir_ = 0;
    float pill_x_ = -1, pill_w_ = 0;
    float tab_x_[3] = {0, 0, 0}, tab_w_[3] = {0, 0, 0}; /* laid out by draw_top_bar */
    float screen_anim_ = 0;

    /* The folder browser: "" lists the drives. */
    std::string browse_path_;
    std::vector<std::pair<std::string, std::string>> browse_entries_; /* label, path */
    int browse_row_ = 0;
    int browse_first_ = 0;
    int browse_games_ = 0;

    std::uint32_t held_ = 0, prev_ = 0;
    std::uint32_t raw_held_ = 0, raw_prev_ = 0; /* the buttons alone, without the stick as a D-pad */
    float rep_left_ = 0, rep_right_ = 0, rep_up_ = 0, rep_down_ = 0;
    double time_ = 0;

    Game *launch_ = nullptr;
    std::string launch_status_;
    float launch_progress_ = -1;
    double launch_start_ = 0;
};
} // namespace porpoise::ui
