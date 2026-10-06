/* Porpoise UI - the launcher: library, details, launch, memory cards, settings.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ui_app.cpp holds the library, details, memory cards, dialogs and launch;
 * ui_app_settings.cpp holds Settings, a game's own settings and the folder
 * browser. ui_app_common.hpp has what they share. */
#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "porpoise_banner.hpp"
#include "porpoise_borders.hpp"
#include "ui_theme.hpp"
#include "ui_cheats.hpp"
#include "ui_gfx.hpp"
#include "ui_library.hpp"
#include "ui_memcard.hpp"
#include "ui_recommend.hpp"
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
    struct SaveScan; /* Memory Cards read on a worker thread (ui_app.cpp) */
    enum class Action
    {
        None,
        Launch,      /* launch_game() says which */
        SettingsChanged,
        Rescan,      /* game folders changed: search again */
        FetchCovers, /* covers and game info now (Sort & filter) */
        CheckUpdate, /* look for a newer Porpoise now */
        InstallUpdate,
        InstallVersion, /* the one picked in About > Choose a version (picked_version()) */
        Quit,        /* close Porpoise (after an update) */
        Reinitialize, /* settings wiped: Porpoise starts again as at its first start */
        MoveData,     /* Porpoise's folder moves to move_target() */
    };
    const std::string &move_target() const { return move_target_; }

    void set_sound_hook(void (*play)(Sound)) { sound_ = play; }
    /* Plays a Wii disc's banner jingle (48 kHz stereo frames); null stops it. */
    void set_jingle_hook(void (*play)(const std::int16_t *, std::size_t)) { jingle_ = play; }
    void init(Gfx *gfx, Library *library, Settings *settings, const std::string &settings_path,
              const std::string &options_path, const std::string &saves_dir);
    Action update(const Input &in, double dt);
    void draw(double time);

    Game *launch_game() { return launch_; }
#ifdef PORPOISE_HOST_PREVIEW
    void preview_setup_step(int step) { ws_step_ = step; } /* tools/ui-preview: show each step */
    void preview_mapping(int kind, int row) /* tools/ui-preview: Customize buttons on a controller's tab */
    {
        map_target_ = settings_;
        map_game_ = nullptr;
        map_in_game_ = false;
        map_return_ = Screen::Main;
        begin_mapping();
        map_kind_ = kind;
        map_row_ = row;
        open_screen(Screen::Mapping);
    }
    void preview_pointer(float x, float y) /* tools/ui-preview: the home screen's pointer there */
    {
        preview_px_ = x;
        preview_py_ = y;
    }
    void preview_home_page(int page) { home_page_ = page, home_scroll_ = float(page), home_synced_ = true; }
    void preview_starcube(int face, bool page);
#endif
    /* The save state to start it from ("" for none); asking clears it. */
    std::string take_launch_state()
    {
        std::string s;
        s.swap(launch_state_);
        return s;
    }
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
    /* A line by the game count for a few seconds ("Covers and game info are up to date."). */
    void flash_note(const std::string &note)
    {
        flash_note_ = note;
        flash_time_ = time_;
    }
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
    /* A save or load the menu asks of the host, which does it on the core's
     * thread and answers with menu_state_done(). */
    struct MenuRequest
    {
        enum Kind
        {
            None,
            Save,
            Load,
        } kind = None;
        int slot = 0;
    };
    MenuRequest take_menu_request();
    /* Saving or loading is under way (the menu shows it). */
    bool menu_busy() const { return menu_busy_.kind != MenuRequest::None; }
    void menu_state_done(MenuRequest::Kind kind, int slot, bool ok);
    /* Fast forward chosen in the menu: 1 (off), 2 or 4. Not saved. */
    int menu_fast_forward() const { return menu_ff_ == 2 ? 4 : menu_ff_ == 1 ? 2 : 1; }
    void step_fast_forward() { menu_ff_ = (menu_ff_ + 1) % 3; }
    /* The newest release on GitHub ("v1.2" and its page), for the update notice. */
    /* Every release GitHub lists (newest first), for About > Choose a version. */
    void set_versions(const std::vector<std::string> &tags, const std::vector<int> &builds,
                      const std::vector<bool> &betas, const std::vector<std::size_t> &sizes);
    int picked_version() const { return version_pick_; }
    static std::string version_label(const std::string &tag, int build);
    void set_latest_release(const std::string &tag, const std::string &url, std::size_t zip_size = 0,
                            int build = 0);
    /* The updater, as porpoise::update::Phase numbers: 0 idle, 1 checking,
     * 2 downloading, 3 installing, 4 done, 5 failed, 6 checked. */
    void set_update_progress(int phase, std::size_t done, std::size_t total, const std::string &error);
    bool updating() const { return update_phase_ == 2 || update_phase_ == 3 || update_phase_ == 4 || update_phase_ == 7; }
    /* Back from a game to the library. */
    void return_from_game();
    /* Black over everything at amount (0..1), with Porpoise's mark in the
     * middle: leaving a game, coming back, and the start. */
    void draw_curtain(float amount);
    /* The menus' entrance: everything but the background faded by `fade` and
     * lowered by `dy` design pixels (Porpoise starting, or back from a game). */
    void set_intro(float fade, float dy)
    {
        intro_fade_ = fade;
        intro_dy_ = dy;
    }
    /* Buttons already down count only once they've been let go. */
    void swallow_held() { held_ = prev_ = 0xFFFFFFFFu; }
    /* The menu language changed (rebuilds Settings' rows). */
    void language_changed() { build_settings(); }
    /* A message with an OK button. */
    void show_message(const std::string &title, const std::string &message)
    {
        open_dialog(DialogKind::Info, title, message, "");
    }
    /* The sandbox message at start, which the player can turn off. */
    void show_sandbox_notice(const std::string &title, const std::string &message);
    /* RetroAchievements: the account the panel shows, and signing in and out
     * (porpoise_ra, given by the host). Without them there is no panel. */
    struct RaState
    {
        bool signed_in = false, busy = false;
        std::string user, message;
        int points = 0;
    };
    void set_ra(std::function<RaState()> state, std::function<bool(const std::string &, const std::string &)> login,
                std::function<void()> logout)
    {
        ra_state_ = std::move(state);
        ra_login_ = std::move(login);
        ra_logout_ = std::move(logout);
    }
    /* The account panel (L1 + Square in the library, or Settings > Games). */
    void open_account();
    /* Signed in or out: Settings' row says so. */
    void account_changed() { build_settings(); }
#ifdef PORPOISE_HOST_PREVIEW
    void preview_account(bool typing, int field, int kr, int kc)
    {
        open_account();
        acct_.typing = typing;
        acct_.field = field;
        acct_.row = field;
        acct_.kr = kr;
        acct_.kc = kc;
        acct_.anim = 1;
        acct_.user = "Ripalda";
        acct_.pass = "hunter22";
    }
#endif

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
        States,       /* a game's save states, over its Details */
        WiiGuide,     /* how to hold the DualSense as each Wii controller */
        WiiSetup,     /* the Wii Remote setup: controller, hold, centre, the screen's corners */
        Welcome,      /* the first start: choose a theme */
    };
    struct SettingRow
    {
        std::string section, label, help, key;
        std::vector<std::string> values;
        int *int_value = nullptr;
        bool *bool_value = nullptr;
        std::string *text_value = nullptr; /* a text setting chosen from a list (the border) */
        std::vector<int> order;            /* when set: values[i] is the setting's order[i] */
        int toggle = -1;                   /* a switch: 1 on, 0 off; -1 not a switch */
        int rec = -1;                      /* its recommendation (rec_rows_) */
        std::string tag;                   /* a small label beside the switch: where it comes from */
        int setup = -1;                    /* kRowUseSetup: which setup */
        int min = 0;
        bool header = false;
        int action = 0;      /* look::RowAction */
        int folder = -1;     /* the folder a Remove row is for */
        bool rescan = false; /* changing it searches the library again */
        bool beta = false;   /* a BETA badge after the label */
    };
    enum class DialogKind
    {
        Info,
        ResetAll,
        ResetGame,
        DeleteSave,
        CopySave,
        DeleteState,
        InstallUpdate,
        InstallVersion,
        CopyGame,
        DeleteWiiSave,
        Resume, /* quick resume: Resume (yes) or Start Over (the other button) */
        Reinitialize,
        SandboxNotice, /* OK, or Don't show again (the other button) */
        MoveData,      /* Porpoise's folder to another drive */
    };
    struct Dialog
    {
        bool open = false;
        DialogKind kind = DialogKind::Info;
        std::string title, message, yes; /* yes empty: just OK */
        std::string no;                  /* the other button, when it isn't Cancel */
        Texture *picture = nullptr;      /* shown above the message (quick resume) */
        /* A checklist instead of a message (Check my setup): each line with a
         * tick, or a mark that something needs doing. */
        std::vector<std::pair<bool, std::string>> checks;
        bool danger = false;
        int choice = 0; /* 0 = cancel, 1 = yes */
        float anim = 0;
    };

    /* The RetroAchievements account panel and its keyboard. */
    struct AccountPanel
    {
        bool open = false;
        int row = 0;           /* signed out: 0 username, 1 password, 2 sign in */
        std::string user, pass;
        bool typing = false;   /* the keyboard is up */
        int field = 0;         /* what it types into: 0 username, 1 password */
        int kr = 1, kc = 0;    /* the key in focus */
        bool shift = false, symbols = false;
        bool was_busy = false;
        float anim = 0;
    };
    AccountPanel acct_;
    std::function<RaState()> ra_state_;
    std::function<bool(const std::string &, const std::string &)> ra_login_;
    std::function<void()> ra_logout_;
    /* L1 changes tab when it's let go, unless Square came while it was held
     * (L1 + Square: the account panel). */
    bool l1_armed_ = false, l1_chord_ = false, l1_tap_ = false;
    Action update_account(bool up, bool down, bool left, bool right);
    void draw_account();
    void account_key(int kr, int kc);

    std::uint32_t pressed(std::uint32_t bits) const { return (held_ & ~prev_) & bits; }
    void sfx(Sound s) const
    {
        if (sound_)
            sound_(s);
    }
    bool nav(std::uint32_t bit, float &timer, double dt);
    Texture *cover_of(Game &g);
    Texture *picture(Texture *&tex, bool &tried, std::string &wait,
                     std::string (Library::*where)(const Game &) const, const Game &g);
    static bool cover_loading(const Game &g) { return !g.cover && !g.cover_wait.empty(); }
    Texture *disc_of(Game &g);
    /* Memory Cards' other views (ui_app_cards.cpp). */
    struct SaveRef
    {
        int kind = 0; /* 0 Slot A, 1 Slot B, 2 the Wii */
        int index = 0;
    };
    struct SaveGroup
    {
        std::string key, title;
        Game *game = nullptr;
        std::vector<SaveRef> saves;
    };
    std::vector<SaveGroup> saves_by_game();
    void draw_card_blocks(double time);
    void draw_saves_by_game(double time);
    void draw_card_cubes(Card &card, int which, float x, float y, double time);
    void update_saves_by_game(bool up, bool down);
    int mc_game_sel_ = 0;
    float mc_game_scroll_ = 0;
    /* The library's views (ui_app_views.cpp). */
    void draw_cover_flow(double time);
    void draw_wheel(double time);
    void draw_disc_flow(double time);
    void draw_shelf(double time);
    void draw_box_view(double time);
    void draw_list_view(double time);
    void draw_stack(double time);
    void draw_helix(double time);
    void draw_glass_cube(float cx, float cy, float size, float yaw, float pitch, float roll, Color tint, Color rim,
                         Texture *front, float alpha, bool lit);
    void draw_disc(Game *game, float cx, float cy, float d, float spin, float yaw, float alpha, bool focused);
    void draw_box3d(Game &game, float cx, float cy, float w, float h, float depth, float yaw, float alpha);
    void release_far_art(int keep);
    std::string game_meta(const Game &g) const;
    bool update_view_nav(bool left, bool right, bool up, bool down, double dt);
    static constexpr int kShelfCols = 6;
    float row_scroll_ = 0;  /* the shelf's first row, sliding */
    float view_yaw_ = 0;    /* the box view's turn (the right stick) */
    double view_idle_ = 0;  /* how long the right stick has been still */
    Texture *spine_of(Game &g);
    Texture *back_of(Game &g);
    void set_tab(int tab, int dir);
    void open_screen(Screen s);

    /* Settings (ui_app_settings.cpp) */
    void build_settings();
    void build_game_settings();
    void add_recommended_rows();
    void add_cheat_rows();
    std::vector<Cheat> cheats_;
    std::deque<bool> cheat_on_; /* a deque: the rows point at its elements */
    std::string sys_dir_;
public:
    /* Dolphin's Sys folder, for the cheats it lists. */
    void set_sys_dir(const std::string &dir) { sys_dir_ = dir; }
private:
    void add_setup_rows(bool per_game);
    struct RecRow
    {
        enum Kind
        {
            AllPicks,
            Pick,
            DolphinFix,
        } kind = Pick;
        std::string key, value, off_value;
    };
    std::vector<RecRow> rec_rows_;
    bool rec_on(const RecRow &rec) const;
    void toggle_recommended(int index);
    /* A game's own value for a key, or (nullptr) back to the global one. */
    void set_game_key(const std::string &key, const std::string *value);
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
    void open_browser(std::string path); /* by value: callers pass entries it replaces */
    void draw_browser();
    Action update_browser(bool up, bool down);
    std::string build_label() const;
    /* About > Report a bug: Porpoise's logs into <data>/reports/<time>, and a
     * copy on the first USB drive (usb: its folder there, "" when none).
     * Returns the report's folder, "" when it couldn't be written. */
    std::string save_report(std::string &usb);
public:
    /* Whether Porpoise was left in the app sandbox (porpoise_main). */
    void set_sandboxed(bool sandboxed) { sandboxed_ = sandboxed; }
    /* The setup check: a title and what Porpoise can see, with what to do. */
    void show_setup_check(bool first_start);
    /* The first start (and after Reinitialize): a theme to begin with, then
     * the welcome and setup check (ui_app_welcome.cpp). */
    void start_welcome();
    /* After Reinitialize: the menus as at the first start. */
    void restart_fresh();
private:
    bool sandboxed_ = false;
    std::vector<std::string> version_tags_, version_labels_;
    std::vector<std::size_t> version_sizes_;
    std::vector<int> version_builds_;
    int version_pick_ = 0;
    long long change_count() const;

    /* Button mapping (ui_app_controls.cpp) */
    void open_mapping();
    void open_mapping_in_game();
    void begin_mapping();
    void use_preset(int preset);
    void start_preset_from(int layout);
    void draw_control(int control, float right, float cy, bool on, float size);
    void draw_gc_chip(int gc, float cx, float cy, float h, bool on = false);
    float gc_chip_width(int gc, float h);
    void close_mapping();
    Action update_mapping(bool up, bool down, bool left, bool right);
    void draw_mapping(double time);
    /* The same screen for a Wii controller's buttons (map_kind_ 1..4). */
    Action update_wii_mapping(bool up, bool down);
    void draw_wii_mapping(double time);
    void draw_map_tabs();
    bool switch_map_kind(); /* L1 / R1: GameCube, then each Wii controller */
    void assign_control(int gc_input, int control);
    void save_mapping(bool layout_changed);
    void draw_keycap(float right, float cy, const std::string &label, bool on, float height = 40);

    /* Dialogs */
    void open_dialog(DialogKind kind, const std::string &title, const std::string &message,
                     const std::string &yes, bool danger = false);
    Action update_dialog(bool left, bool right);
    Action confirm_dialog(DialogKind kind);
    void draw_dialog();

    /* Memory cards */
    void scan_memory_cards();
    void finish_card_scan();
    /* Entering Memory Cards reads both cards and the Wii saves on a worker
     * thread, so the tab slides in without a hitch (ui_app.cpp). */
    std::shared_ptr<SaveScan> save_scan_;
    void start_save_scan();
    void take_save_scan();
    void ensure_saves(bool wii);
    bool saves_loading() const { return save_scan_ != nullptr; }
    void move_memcard(int dx, int dy);
    void free_card_textures();
    Save *focused_save();
    void ask_delete_save();
    void ask_copy_save();
    void export_save_to_usb();
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

    /* The Revolution look's home screen (ui_app_home.cpp): a grid of tiles,
     * twelve to a page, pointed at with the controller's motion. */
    bool revolution() const { return settings_ && settings_->ui_theme == 1; }
    bool starcube() const { return settings_ && settings_->ui_theme == int(ThemeId::StarCube); }
    int mc_view() const { return starcube() ? 3 : settings_->mc_view; } /* Star Cube's saves are cubes */
    bool home_showing() const /* the grid of tiles (the Revolution look's home, laid out as a grid) */
    {
        return revolution() && settings_->ui_layout == 0 && tab_ == Tab::Library &&
               (screen_ == Screen::Main || screen_ == Screen::Sort);
    }
    static constexpr int kRevTab0 = 16;  /* focus targets: the top bar's tabs */
    static constexpr int kHomeGrow = 19; /* tiles, buttons, arrows, tabs */
    static constexpr int kRdGrow = 22;   /* the opened tile's targets */
    struct HomeItem
    {
        int game = -1;       /* index into the library */
        bool resume = false; /* the first tile: the game played last */
    };
    std::vector<HomeItem> home_items() const;
    void home_pad_sync();
    int home_hit(float x, float y, int items, int pages) const;
    void home_tile_rect(int slot, float scroll_dx, float &x, float &y, float &w, float &h) const;
    void update_home(bool left, bool right, bool up, bool down, bool &play, bool &details, bool &fav, double dt);
    void draw_home(double time);
    /* The Revolution look's shared pieces (ui_app_revolution.cpp). */
    void draw_room();
    void draw_lines_in(float x, float y, float w, float h, float inset);
    void draw_rev_top_bar(bool clock);
    void draw_seg_clock(float cx, float cy, float h);
    std::string short_date() const;
    void draw_hand(float x, float y, float roll, int player, bool pointing);
    /* Opening a tile: it grows to fill the screen, the opened tile comes up
     * out of it (and back down into it on the way out). */
    bool home_rect_of(int game, float rect[4]);
    void start_zoom(int dir);
    void draw_zoom(double time);
    bool zoom_shows_home() const;
    bool pointer_screen() const;
    void rev_pointer_step();
    void rev_pointer_rest();
    void rev_landed();
    int rev_tab_hit(float px, float py) const;
    void draw_rev_pointer();
    std::vector<std::pair<int, std::string>> rev_chips(const Game &game) const;
    void rev_details_rects(const Game &game, std::vector<std::pair<int, std::array<float, 4>>> &out);
    Action update_rev_details(bool left, bool right, bool up, bool down, double dt);
    void draw_rev_picture(Game &game, float x, float y, float w, float h, float fade);
    void draw_rev_details(double time);
    /* A Wii disc's own tile (small) or banner (big), read from the disc
     * (ui_app_banner.cpp); false when there is none (yet). */
    bool has_banner(const Game &game) const;
    bool draw_banner(Game &game, float x, float y, float w, float h, double time, float fade, bool big);
    std::string clock_text() const;
    float ts(float size) const { return settings_ ? size * (1.0f + 0.15f * float(settings_->text_size)) : size; }
    /* The theme in use, and the renderer set for it (each frame). */
    const Theme &th() const { return theme(settings_ ? settings_->ui_theme : 0); }
    void apply_look();
    void draw_theme_overlay();
    int theme_seen_ = -1;

    void (*sound_)(Sound) = nullptr;
    void (*jingle_)(const std::int16_t *, std::size_t) = nullptr;
    /* Wii discs' tiles and banners, played a frame at a time by
     * porpoise_banner into streamed textures (ui_app_banner.cpp). */
    struct BannerTex
    {
        Texture *tex = nullptr;
        int w = 0, h = 0;
        std::uint64_t serial = 0;
        double used = 0;
    };
    std::map<std::string, BannerTex> banner_tex_; /* by "<id>/icon" or "<id>/banner" */
    std::vector<std::uint8_t> banner_px_;
    std::set<std::string> no_banner_, banner_checked_;
    std::string jingle_for_;     /* the opened tile whose jingle is due */
    double rd_open_time_ = 0;    /* when the opened tile opened (its banner starts then) */
    std::string banner_lang_;
    void pump_banners();
    void opened_tile(const Game &game); /* the banner starts, its jingle plays */
    BannerTex *banner_texture(const Game &game, bool big);
    void draw_frames(const BannerTex &b, float x, float y, float w, float h, float fade, float radius, bool fill);
    float intro_fade_ = 1, intro_dy_ = 0;
    float rail_glide_ = -1, row_glide_ = -1; /* Settings' highlights, gliding */
    double glide_time_ = 0;
    /* The entrance in steps: what starts later (delay, of the whole) comes
     * in after the top bar. */
    void intro_stage(float delay)
    {
        const float f = std::clamp((intro_fade_ - delay) / (1.0f - delay), 0.0f, 1.0f);
        g_->set_intro(f, intro_dy_ > 0 ? 28.0f * (1.0f - f) : 0.0f);
    }
    Gfx *g_ = nullptr;
    Library *lib_ = nullptr;
    Settings *settings_ = nullptr;
    std::string settings_path_, options_path_, saves_dir_, data_dir_;
    std::string details_tex_for_, details_tex_; /* the Details game's texture pack folder, if any */

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
    int sort_row_ = 0;         /* Sort & filter: 0 sort, 1 show, 2 covers and info */
    void keep_selection(const std::string &key); /* after the order changed */
    void look_changed(int was);                 /* Settings > Interface > Look */
    void draw_revolution(double time);
    /* Star Cube (ui_app_starcube.cpp): its home cube and its pages. */
    void sc_tick(double dt);
    void sc_open(int face, bool zoom);
    void sc_go_home();
    void sc_open_details();
    int sc_face_of_tab() const;
    std::vector<int> sc_recent_games() const;
    Action update_starcube(bool left, bool right, bool up, bool down, bool &handled);
    void draw_starcube(double time);
    Action update_welcome(bool left, bool right, double dt);
    void draw_welcome(double time);
    void welcome_card(int which, float cx, float cy, float w, float h, float lift, float alpha, bool on, double time);
    int welcome_pick_ = 0;
    float welcome_time_ = 0, welcome_glide_ = 0;
    void draw_sc_home(double time, float zoom);
    void draw_sc_drift(double time, float alpha, float rush = 0);
    void draw_sc_header(const std::string &title, double time);
    void draw_sc_page(float x, float y, float w, float h);
    void draw_sc_games(double time);
    void draw_sc_details(double time);
    void draw_sc_calendar(double time);
    bool sc_home_ = true;      /* the cube shows (else one of its pages) */
    int sc_face_ = 0;          /* the edge it's turned to: Games, Calendar, Memory Cards, Settings */
    bool sc_calendar_ = false; /* the Calendar page is open */
    float sc_zoom_ = 0;        /* 0 the cube .. 1 into a page */
    float sc_yaw_ = 0, sc_pitch_ = 0.42f;
    float sc_glow_[4] = {1, 0, 0, 0};
    int sc_first_row_ = 0;     /* the games page's top row */
    float sc_rows_ = 0;
    int sc_recent_ = 0;        /* the calendar's chosen recent game */
    int sc_month_ = 0;         /* months from this one */
    float sc_vyaw_ = 0, sc_vpitch_ = 0; /* the cube's turn, on a spring */
    float sc_pulse_ = 0;       /* a new edge chosen: the labels and caption pop */
    float sc_page_time_ = 9;   /* since a page opened: its pieces arrive in turn */
    std::string library_count() const;          /* "12 games", "5 Wii games" */
    std::string library_note() const;           /* covers being fetched, or a flash */

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
    /* The Wii saves (ui_app_wiisaves.cpp). */
    std::vector<WiiSave> wii_saves_;
    bool wii_scanned_ = false;
    int wii_sel_ = 0;
    float wii_scroll_ = 0;
    void scan_wii_saves();
    void finish_wii_scan();
    void free_wii_textures();
    Texture *wii_save_banner(WiiSave &s);
    Texture *wii_save_icon(WiiSave &s);
    void update_wii_saves(bool left, bool right, bool up, bool down);
    void draw_wii_saves(double time);
    void forget_banners();
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
    bool menu_restart_armed_ = false; /* Start over asked once */
    std::string menu_change_;
    int menu_tab_ = 0;   /* Game, Video, Graphics, Controls */
    int menu_slot_ = 0;  /* the save-state slot under focus */
    int menu_ff_ = 0;    /* fast forward: off, 2x, 4x */
    int menu_setup_ = 0; /* the setup under focus in the Graphics tab */
    MenuRequest menu_busy_;           /* the save or load under way */
    bool menu_busy_handed_ = false;   /* given to the host */
    int menu_busy_frames_ = 0;        /* frames drawn showing it */
    double menu_busy_start_ = 0;
    int menu_slots_mode_ = 0;         /* 0 the list, 1 choosing where to save, 2 what to load */
    bool menu_confirm_ = false;       /* the next press replaces a used slot */
    std::string menu_note_;
    double menu_note_time_ = -10;
    Texture *menu_slot_tex_[3] = {nullptr, nullptr, nullptr};
    long long menu_slot_time_[3] = {0, 0, 0};
    bool menu_slot_used_[3] = {false, false, false};
    std::vector<porpoise::borders::Border> menu_borders_;
    Texture *lines_art_ = nullptr; /* the controller with lines, for the Controls tab */
    bool lines_art_tried_ = false;
    void load_slots(const Game *game); /* menu_slot_* for this game's save states */
    void menu_free_slots();
    /* Details > Save states (ui_app_states.cpp) */
    void open_states(Game *game);
    Action update_states(bool left, bool right);
    void draw_states();
    void count_states(const Game *game);
    Game *states_game_ = nullptr;
    int states_sel_ = 0;
    int details_states_ = 0; /* how many slots the game in Details has */
    std::string launch_state_; /* the state to start the launched game from */
    void draw_controller_lines(float x, float y, float w, const porpoise::pad::Mapping &m);
    /* A Wii game's controls: the DualSense as held, each Wii button on its control.
     * live_pose: how the first (second) controller is held right now, or -1. */
    void draw_wii_controls(float x, float y, float w, float h, const porpoise::pad::WiiConfig &wii, float alpha,
                           int live_pose = -1, int live_pose_second = -1);
    std::pair<float, float> draw_wii_pad(const porpoise::pad::WiiLayout &lay, int pose, float bx, float by,
                                         float art_h, float alpha);
    void wii_chip(int input, float cx, float cy, float ch, float alpha);
    float draw_wii_list(const porpoise::pad::WiiLayout &lay, float lx, float ly, float lw, int columns, float alpha);
    /* Settings > Wii Remote > How to hold it. */
    void open_wii_guide();
    void open_wii_setup_in_game(bool recalibrate_only);
    Action update_wii_guide(bool left, bool right);
    void draw_wii_guide(double time);
    /* The Wii Remote setup (before a Wii game, from Settings, or from the
     * pause menu). */
    Action start_game(Game *g, const std::string &state, bool resume_asked = false);
    Game *resume_game_ = nullptr; /* the game the quick-resume question is about */
    void close_dialog();
    void draw_check_mark(float cx, float cy, float size, bool ok);
    void open_wii_setup(Game *g, bool launch, const std::string &state);
    Action update_wii_setup(bool up, bool down, bool left, bool right);
    void draw_wii_setup(double time);
    void wii_setup_apply_pad();
    void close_wii_setup();
    Settings &ws_settings();
    void ws_store(const char *key, int value);
    enum WiiStep
    {
        kWsController,
        kWsHold,
        kWsScreen, /* advanced: corners, or size and distance */
        kWsCentre,
        kWsTopLeft,
        kWsBottomRight,
        kWsTry,
        kWsTune, /* advanced: smoothing, reach, grip, presets */
    };
    int ws_step_ = kWsController;
    int ws_controller_ = 0;
    Game *ws_game_ = nullptr;
    bool ws_launch_ = false;
    bool ws_in_game_ = false;    /* over the paused game */
    bool ws_recal_only_ = false; /* the pause menu's "recalibrate": centre, corners, try */
    bool ws_ask_ = true; /* the setup's game: offer the setup before it starts */
    int ws_row_ = 0;
    int ws_size_mode_ = 0;       /* 0 corners, 1 size and distance */
    int ws_preset_slot_ = 0, ws_preset_name_ = 0;
    std::string ws_state_;
    Screen ws_return_ = Screen::Main;
    double ws_hold_from_ = -1;     /* Cross held since, calmly (centring) */
    float ws_tl_[2] = {0, 0};      /* the top-left corner: yaw, pitch */
    std::string ws_note_;
    double ws_note_time_ = -100;
    Settings *guide_target_ = nullptr;
    Screen guide_return_ = Screen::Main;
    int guide_controller_ = 0;

    Dialog dialog_;
    bool drawing_dialog_ = false;
    Texture *logo_ = nullptr;
    bool logo_tried_ = false;
    Texture *pad_art_ = nullptr; /* the controller on the mapping screen */
    bool pad_art_tried_ = false;

    /* Button mapping */
    Screen map_return_ = Screen::Main;
    Settings *map_target_ = nullptr; /* the global settings, or a game's */
    std::string latest_version_, latest_url_; /* "1.2", from GitHub; "" when not newer */
    std::size_t latest_size_ = 0;
    int update_phase_ = 0;
    std::size_t update_done_ = 0, update_total_ = 0;
    std::string update_error_;
    std::string update_note_; /* after a check: up to date, or what went wrong */
    bool update_failed_ = false; /* an install failed: say so once */
    double update_done_time_ = 0; /* when the install finished */
    double update_note_time_ = -100;
    std::string update_row_value() const;
    void draw_update_overlay(double time);
    bool update_available() const { return !latest_version_.empty(); }
    Texture *flags_ = nullptr; /* Settings > Interface > Language */
    bool flags_tried_ = false;
    Texture *qr_ = nullptr; /* Settings > About: the Discord */
    Texture *ripalda_ = nullptr; /* Ruben's mark, beside his name in About */
    bool mark_tried_ = false;
    bool qr_tried_ = false;
    std::vector<std::string> border_names_; /* the Border row's choices */
    int border_choice_ = 0;
    Game *map_game_ = nullptr;       /* the game whose settings those are, if any */
    bool map_in_game_ = false;       /* opened from the in-game menu */
    int map_preset_ = 0;             /* the player's layout being edited, 0..3 */
    int map_row_ = 0;
    bool map_capture_ = false; /* waiting for a button press */
    bool map_armed_ = false;   /* every button was let go since the capture began */
    double map_capture_start_ = 0;
    std::string map_note_; /* a line under the list after a change */
    double map_note_time_ = -10;
    int map_kind_ = 0;
    /* Settings > Games > Move Porpoise's folder: the places offered (label,
     * path), the one picked, and the one confirmed. */
    std::vector<std::pair<std::string, std::string>> move_places_;
    int move_pick_ = -1;
    std::string move_target_; /* 0 GameCube; 1..4 a Wii controller's buttons (porpoise::pad::wii_button_set + 1) */

    /* Details: L2 / R2 swipe to the previous / next game. */
    float swipe_anim_ = 0; /* 1 -> 0 */
    int swipe_dir_ = 0;
    int swipe_from_ = -1;
    float rep_l2_ = 0, rep_r2_ = 0;
    std::string note_;
    std::string flash_note_;
    /* The home screen (Revolution look). Focus: a tile 0..11 on the page, or
     * one of the buttons (kHome* in ui_app_home.cpp), -1 none. */
    int home_page_ = 0;
    float home_scroll_ = 0;
    int home_focus_ = 0;
    bool home_synced_ = false; /* the focus has been put on the selected game */
    float home_grow_[kHomeGrow] = {};
    float rev_tab_x_[3] = {}, rev_tab_w_[3] = {};
    Texture *hands_[2] = {nullptr, nullptr}; /* open, pointing */
    float zoom_anim_ = 0;   /* 1 -> 0 as a tile opens or closes */
    int zoom_dir_ = 1;      /* +1 opening, -1 closing */
    int zoom_game_ = -1;
    float zoom_from_[4] = {0, 0, 0, 0};
    bool hand_tried_ = false;
    int rd_focus_ = 0;           /* the opened tile: Start, the controls, the chips, the arrows */
    float rd_grow_[kRdGrow] = {};
    bool rd_info_ = false;       /* Triangle: the facts over the banner */
    bool mc_wii_ = false;        /* Memory Cards shows the Wii saves */
    std::string wii_focus_code_; /* focus this game's Wii save on the next scan */
    bool home_pointing_ = false; /* the pointer leads (else the D-pad) */
    float home_px_ = 960, home_py_ = 540, home_roll_ = 0;
    float home_hide_x_ = 0, home_hide_y_ = 0;
    double home_buzz_until_ = -1;
#ifdef PORPOISE_HOST_PREVIEW
    float preview_px_ = -1, preview_py_ = -1;
#endif
    int creator_presses_ = 0; /* About: Cross on the creator's name (developer options) */
    double creator_time_ = -100;
    double flash_time_ = -100;

    /* Motion: the content slides in when the tab changes; screens fade in. */
    float tab_anim_ = 0;
    int tab_dir_ = 0;
    float pill_x_ = -1, pill_w_ = 0;
    float tab_x_[3] = {0, 0, 0}, tab_w_[3] = {0, 0, 0}; /* laid out by draw_top_bar */
    float screen_anim_ = 0;

    /* The folder browser: opens on "/", the whole console; "" is the drives
     * and shortcuts list (Triangle). Folders first, then the games in this
     * folder, so the player sees where their games are. */
    struct BrowseEntry
    {
        enum Kind
        {
            Folder,
            Game,
            Place,
        } kind = Folder;
        std::string label, path;
        long long size = 0; /* a game's, in bytes */
        int games = -1;     /* games right inside a folder; -1 not counted */
    };
    std::string browse_path_;
    std::vector<BrowseEntry> browse_entries_;
    int browse_row_ = 0;
    int browse_first_ = 0;
    int browse_games_ = 0;
    bool browse_unreadable_ = false;
    std::string browse_copy_name_; /* the game being copied to the console */
    void start_game_copy(const std::string &from);
    std::vector<BrowseEntry> browse_places() const;

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
