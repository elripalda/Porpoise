/* Porpoise UI - the launcher screens.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The library, a game's details, memory cards, dialogs and the launch screen.
 * Settings and the folder browser are in ui_app_settings.cpp; the look they
 * share is in ui_app_common.hpp. */
#include "porpoise_states.hpp"
#include "ui_app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "porpoise_pad.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

void App::init(Gfx *gfx, Library *library, Settings *settings, const std::string &settings_path,
               const std::string &options_path, const std::string &saves_dir)
{
    g_ = gfx;
    lib_ = library;
    settings_ = settings;
    settings_path_ = settings_path;
    options_path_ = options_path;
    saves_dir_ = saves_dir;
    data_dir_ = settings_path.substr(0, settings_path.rfind('/'));
    build_settings();
    /* Return to the game played last. */
    const std::string key = lib_->selected_id();
    for (std::size_t i = 0; i < lib_->games().size(); ++i)
        if (Library::key_of(lib_->games()[i]) == key)
            selected_ = int(i);
    scroll_ = float(selected_);
}

void App::forget_textures()
{
    /* The device is gone and its textures with it: only drop the pointers. */
    for (Game &g : lib_->games())
    {
        g.cover = nullptr;
        g.cover_tried = false;
        g.disc = nullptr;
        g.disc_tried = false;
        g.back = nullptr;
        g.back_tried = false;
    }
    for (Card *c : {&card_a_, &card_b_})
        for (Save &s : c->saves)
        {
            s.icon_tex = nullptr;
            s.banner_tex = nullptr;
        }
    logo_ = nullptr;
    logo_tried_ = false;
    pad_art_ = nullptr;
    pad_art_tried_ = false;
    lines_art_ = nullptr;
    lines_art_tried_ = false;
    for (Texture *&t : menu_slot_tex_)
        t = nullptr;
}

void App::free_card_textures()
{
    for (Card *c : {&card_a_, &card_b_})
        for (Save &s : c->saves)
        {
            if (s.icon_tex)
                g_->free_texture(s.icon_tex);
            if (s.banner_tex)
                g_->free_texture(s.banner_tex);
            s.icon_tex = nullptr;
            s.banner_tex = nullptr;
        }
}

Texture *App::cover_of(Game &g)
{
    if (!g.cover_tried)
    {
        g.cover_tried = true;
        const std::string path = lib_->cover_path(g);
        if (!path.empty())
            g.cover = g_->texture_file(path);
    }
    return g.cover;
}

Texture *App::disc_of(Game &g)
{
    if (!g.disc_tried)
    {
        g.disc_tried = true;
        const std::string path = lib_->disc_path(g);
        if (!path.empty())
            g.disc = g_->texture_file(path);
    }
    return g.disc;
}

Texture *App::back_of(Game &g)
{
    if (!g.back_tried)
    {
        g.back_tried = true;
        const std::string path = lib_->back_path(g);
        if (!path.empty())
            g.back = g_->texture_file(path);
    }
    return g.back;
}

Texture *App::save_icon(Save &s)
{
    if (!s.icon_tex && s.icon.size() == 32u * 32u * 4u)
    {
        const auto big = enlarge(s.icon, 32, 32, 3);
        s.icon_tex = g_->texture_rgba(big.data(), 96, 96);
    }
    return s.icon_tex;
}

Texture *App::save_banner(Save &s)
{
    if (!s.banner_tex && s.banner.size() == 96u * 32u * 4u)
    {
        const auto big = enlarge(s.banner, 96, 32, 3);
        s.banner_tex = g_->texture_rgba(big.data(), 288, 96);
    }
    return s.banner_tex;
}

/* ---- memory cards model ------------------------------------------------------------------ */

void App::scan_memory_cards()
{
    free_card_textures();
    load_cards(saves_dir_, card_a_, card_b_);
    cards_scanned_ = true;
    Card *cards[2] = {&card_a_, &card_b_};
    for (int i = 0; i < 2; ++i)
        mc_sel_[i] = std::clamp(mc_sel_[i], 0, std::max(0, int(cards[i]->saves.size()) - 1));
    if (!mc_focus_code_.empty())
    {
        bool found = false;
        for (int i = 0; i < 2 && !found; ++i)
            for (std::size_t j = 0; j < cards[i]->saves.size(); ++j)
                if (cards[i]->saves[j].game_code.compare(0, 4, mc_focus_code_, 0, 4) == 0)
                {
                    mc_card_ = i;
                    mc_sel_[i] = int(j);
                    found = true;
                    break;
                }
        mc_focus_code_.clear();
    }
    mc_lift_ = 0;
}

void App::move_memcard(int dx, int dy)
{
    Card *cards[2] = {&card_a_, &card_b_};
    const int cur = mc_card_;
    const int n = int(cards[cur]->saves.size());
    int &sel = mc_sel_[cur];
    const int col = sel % kMcCols, row = sel / kMcCols;
    const int moved_from = sel;
    if (dx != 0)
    {
        const int other = 1 - cur;
        const bool cross = dx > 0 ? (col == kMcCols - 1 || sel + 1 >= n) && cur == 0
                                  : col == 0 && cur == 1;
        if (cross)
        {
            /* Into the other card, on the same visible row. */
            const int vis_row = row - int(mc_scroll_[cur]);
            const int n_other = int(cards[other]->saves.size());
            const int target_row = int(mc_scroll_[other]) + vis_row;
            const int idx = target_row * kMcCols + (dx > 0 ? 0 : kMcCols - 1);
            mc_sel_[other] = std::clamp(idx, 0, std::max(0, n_other - 1));
            mc_card_ = other;
            mc_lift_ = 0.4f;
            sfx(Sound::MenuScroll);
            return;
        }
        if (dx > 0 && col < kMcCols - 1 && sel + 1 < n)
            ++sel;
        if (dx < 0 && col > 0)
            --sel;
    }
    if (dy != 0)
    {
        const int next = sel + dy * kMcCols;
        if (next >= 0 && next < n)
            sel = next;
        else if (dy > 0 && n > 0 && row < (n - 1) / kMcCols)
            sel = n - 1; /* the last, shorter row */
    }
    if (sel != moved_from)
    {
        mc_lift_ = 0.4f;
        sfx(Sound::MenuScroll);
    }
}

/* ---- input ------------------------------------------------------------------------------- */

bool App::nav(std::uint32_t bit, float &timer, double dt)
{
    if (!(held_ & bit))
    {
        timer = 0;
        return false;
    }
    if (!(prev_ & bit))
    {
        timer = 0.38f;
        return true;
    }
    timer -= float(dt);
    if (timer <= 0)
    {
        timer = 0.085f;
        return true;
    }
    return false;
}

App::Action App::update(const Input &in, double dt)
{
    time_ += dt;
    prev_ = held_;
    held_ = in.held;
    raw_prev_ = raw_held_;
    raw_held_ = in.held;
    if (in.stick_x < -0.55f) held_ |= BtnLeft;
    if (in.stick_x > 0.55f) held_ |= BtnRight;
    if (in.stick_y < -0.55f) held_ |= BtnUp;
    if (in.stick_y > 0.55f) held_ |= BtnDown;

    const bool left = nav(BtnLeft, rep_left_, dt), right = nav(BtnRight, rep_right_, dt);
    const bool up = nav(BtnUp, rep_up_, dt), down = nav(BtnDown, rep_down_, dt);
    auto &games = lib_->games();
    Action action = Action::None;

    const bool calm = settings_->reduced_motion;
    const float rate = calm ? 40.0f : 14.0f;
    scroll_ = smooth(scroll_, float(selected_), dt, rate);
    lift_ = smooth(lift_, 1.0f, dt, 10.0f);
    mc_lift_ = smooth(mc_lift_, 1.0f, dt, 10.0f);
    tab_anim_ = calm ? 0.0f : std::max(0.0f, tab_anim_ - float(dt) * 3.6f);
    screen_anim_ = calm ? 0.0f : std::max(0.0f, screen_anim_ - float(dt) * 4.5f);
    dialog_.anim = calm ? 1.0f : std::min(1.0f, dialog_.anim + float(dt) * 6.0f);
    flip_anim_ = calm ? 0.0f : std::max(0.0f, flip_anim_ - float(dt) * 2.4f);
    swipe_anim_ = calm ? 0.0f : std::max(0.0f, swipe_anim_ - float(dt) * 3.4f);
    right_x_ = std::fabs(in.right_x) > 0.15f ? in.right_x : 0.0f;
    {
        const float target = (box_back_ ? kPi : 0.0f) + right_x_ * 0.9f;
        box_yaw_ = calm ? target : smooth(box_yaw_, target, dt, 7.0f);
    }
    if (pill_x_ >= 0 && tab_w_[0] > 0)
    {
        const float target = tab_x_[int(tab_)], width = tab_w_[int(tab_)];
        pill_x_ = calm ? target : smooth(pill_x_, target, dt, 16.0f);
        pill_w_ = calm ? width : smooth(pill_w_, width, dt, 16.0f);
    }

    if (dialog_.open)
        return update_dialog(left, right);
    if (screen_ == Screen::Browse)
        return update_browser(up, down);
    if (screen_ == Screen::GameSettings)
        return update_settings(up, down, left, right);
    if (screen_ == Screen::Mapping)
        return update_mapping(up, down, left, right);
    if (screen_ == Screen::States)
        return update_states(left, right);
    if (screen_ == Screen::Sort)
    {
        if (up || down)
        {
            sort_row_ = 1 - sort_row_;
            sfx(Sound::MenuScroll);
        }
        if (pressed(BtnCross))
        {
            const std::string key = games.empty() ? "" : Library::key_of(games[selected_]);
            lib_->sort(sort_row_ == 0 ? Library::Sort::Title : Library::Sort::Recent);
            for (std::size_t i = 0; i < games.size(); ++i)
                if (Library::key_of(games[i]) == key)
                    selected_ = int(i);
            scroll_ = float(selected_);
            lib_->save();
            screen_ = Screen::Main;
        }
        if (pressed(BtnCircle | BtnTriangle))
            screen_ = Screen::Main;
        return action;
    }
    if (screen_ == Screen::Details)
    {
        /* L2 / R2: the previous / next game, without leaving Details. Held,
         * they keep going, a little slower than the D-pad so each swipe shows. */
        auto trigger = [&](std::uint32_t bit, float &timer) {
            if (!(held_ & bit))
            {
                timer = 0;
                return false;
            }
            if (!(prev_ & bit))
            {
                timer = 0.42f;
                return true;
            }
            timer -= float(dt);
            if (timer <= 0)
            {
                timer = 0.26f;
                return true;
            }
            return false;
        };
        const bool prev_game = trigger(BtnL2, rep_l2_), next_game = trigger(BtnR2, rep_r2_);
        if ((prev_game || next_game) && !games.empty())
        {
            const int dir = next_game ? +1 : -1;
            const int to = selected_ + dir;
            if (to >= 0 && to < int(games.size()))
            {
                swipe_from_ = selected_;
                selected_ = to;
                swipe_dir_ = dir;
                swipe_anim_ = calm ? 0.0f : 1.0f;
                flip_anim_ = 0;
                box_back_ = false;
                box_yaw_ = calm ? 0.0f : float(dir) * 1.25f; /* it swings in and settles */
                details_custom_ = !Settings::keys_in(game_settings_path(games[std::size_t(selected_)])).empty();
                count_states(&games[std::size_t(selected_)]);
                lib_->set_selected(Library::key_of(games[std::size_t(selected_)]));
                sfx(Sound::GameRow);
            }
        }
        if (up && details_row_ > 0)
        {
            --details_row_;
            sfx(Sound::MenuScroll);
        }
        if (down && details_row_ < 3)
        {
            ++details_row_;
            sfx(Sound::MenuScroll);
        }
        if (pressed(BtnCircle))
        {
            open_screen(Screen::Main);
            sfx(Sound::DetailsFlip);
        }
        if (pressed(BtnTriangle))
        {
            box_back_ = !box_back_; /* the back of the box */
            sfx(Sound::DetailsFlip);
        }
        if (pressed(BtnCross) && !games.empty())
        {
            if (details_row_ == 0)
            {
                launch_ = &games[selected_];
                launch_state_.clear();
                action = Action::Launch;
                sfx(Sound::LaunchGame);
            }
            else if (details_row_ == 1)
                open_states(&games[std::size_t(selected_)]);
            else if (details_row_ == 2)
            {
                open_game_settings(games[std::size_t(selected_)]);
                sfx(Sound::MenuScroll);
            }
            else
            {
                /* Straight to this game's save, when there is one. */
                open_screen(Screen::Main);
                mc_focus_code_ = games[selected_].id;
                set_tab(int(Tab::MemoryCards), +1);
            }
        }
        return action;
    }

    if (pressed(BtnL1))
        set_tab((int(tab_) + 2) % 3, -1);
    if (pressed(BtnR1))
        set_tab((int(tab_) + 1) % 3, +1);

    if (tab_ == Tab::Library && !games.empty())
    {
        if (left && selected_ > 0)
        {
            --selected_;
            lift_ = 0.4f;
            sfx(Sound::GameRow);
        }
        if (right && selected_ + 1 < int(games.size()))
        {
            ++selected_;
            lift_ = 0.4f;
            sfx(Sound::GameRow);
        }
        if (pressed(BtnCross))
        {
            launch_ = &games[selected_];
            launch_state_.clear();
            action = Action::Launch;
            sfx(Sound::LaunchGame);
        }
        if (pressed(BtnSquare))
        {
            open_screen(Screen::Details);
            sfx(Sound::DetailsFlip);
            flip_anim_ = 1.0f;
            box_back_ = false;
            box_yaw_ = calm ? 0.0f : -2.0f * kPi; /* one full turn on the way in */
            details_row_ = 0;
            details_custom_ = !Settings::keys_in(game_settings_path(games[std::size_t(selected_)])).empty();
            count_states(&games[std::size_t(selected_)]);
        }
        if (pressed(BtnTriangle))
        {
            sfx(Sound::MenuScroll);
            screen_ = Screen::Sort;
            sort_row_ = lib_->sort_order() == Library::Sort::Title ? 0 : 1;
        }
        lib_->set_selected(Library::key_of(games[selected_]));
    }
    else if (tab_ == Tab::Settings)
        action = update_settings(up, down, left, right);
    else if (tab_ == Tab::MemoryCards)
    {
        if (cards_scanned_)
        {
            if (left)
                move_memcard(-1, 0);
            if (right)
                move_memcard(+1, 0);
            if (up)
                move_memcard(0, -1);
            if (down)
                move_memcard(0, +1);
            if (pressed(BtnTriangle))
                ask_delete_save();
            if (pressed(BtnSquare))
                ask_copy_save();
        }
        if (pressed(BtnCircle))
            set_tab(int(Tab::Library), -1);
    }
    return action;
}

void App::set_tab(int tab, int dir)
{
    if (tab == int(tab_))
        return;
    tab_ = Tab(tab);
    sfx(Sound::MovingTab);
    tab_dir_ = dir;
    tab_anim_ = 1.0f;
    if (tab_ == Tab::MemoryCards)
        cards_scanned_ = false; /* fresh from disk each visit */
    if (tab_ == Tab::Settings)
    {
        on_rail_ = true;
        build_settings(); /* game counts and folders may have changed */
    }
}

void App::open_screen(Screen s)
{
    screen_ = s;
    screen_anim_ = 1.0f;
}

/* ---- dialogs ------------------------------------------------------------------------------ */

void App::open_dialog(DialogKind kind, const std::string &title, const std::string &message, const std::string &yes,
                      bool danger)
{
    dialog_.open = true;
    dialog_.kind = kind;
    dialog_.title = title;
    dialog_.message = message;
    dialog_.yes = yes;
    dialog_.danger = danger;
    dialog_.choice = 0; /* Cancel first: nothing happens by accident */
    dialog_.anim = 0;
    sfx(Sound::DetailsFlip);
}

App::Action App::update_dialog(bool left, bool right)
{
    if (!dialog_.yes.empty())
    {
        if (left && dialog_.choice != 0)
        {
            dialog_.choice = 0;
            sfx(Sound::MenuScroll);
        }
        if (right && dialog_.choice != 1)
        {
            dialog_.choice = 1;
            sfx(Sound::MenuScroll);
        }
    }
    if (pressed(BtnCircle))
    {
        dialog_.open = false;
        return Action::None;
    }
    if (pressed(BtnCross))
    {
        dialog_.open = false;
        if (!dialog_.yes.empty() && dialog_.choice == 1)
            return confirm_dialog(dialog_.kind);
    }
    return Action::None;
}

App::Action App::confirm_dialog(DialogKind kind)
{
    switch (kind)
    {
    case DialogKind::ResetAll:
        settings_->reset();
        settings_->save(settings_path_);
        settings_->write_core_options(options_path_);
        apply_language(settings_->ui_language, data_dir_ + "/lang");
        build_settings();
        return Action::SettingsChanged;
    case DialogKind::ResetGame:
        if (game_for_)
        {
            game_keys_.clear();
            std::remove(game_settings_path(*game_for_).c_str());
            game_ = *settings_;
            build_game_settings();
        }
        return Action::None;
    case DialogKind::DeleteState:
        if (states_game_)
        {
            porpoise::states::remove(Library::key_of(*states_game_), states_sel_);
            load_slots(states_game_);
            count_states(states_game_);
        }
        return Action::None;
    case DialogKind::DeleteSave:
        if (Save *s = focused_save())
        {
            const std::string path = s->path;
            if (std::remove(path.c_str()) != 0)
            {
                open_dialog(DialogKind::Info, tr("Could not delete the save"),
                            trf("The file could not be removed: {path}", {{"path", path}}), "");
                return Action::None;
            }
            cards_scanned_ = false;
        }
        return Action::None;
    case DialogKind::CopySave:
        if (Save *s = focused_save())
        {
            const std::string target = copy_target(*s);
            const std::string dir = target.substr(0, target.rfind('/'));
            /* The other card's folder, and its parents. */
            for (std::size_t i = 1; i <= dir.size(); ++i)
                if (i == dir.size() || dir[i] == '/')
                    mkdir(dir.substr(0, i).c_str(), 0777);
            bool ok = false;
            if (std::FILE *in = std::fopen(s->path.c_str(), "rb"))
            {
                const std::string part = target + ".part";
                if (std::FILE *out = std::fopen(part.c_str(), "wb"))
                {
                    char buf[16384];
                    std::size_t n;
                    ok = true;
                    while ((n = std::fread(buf, 1, sizeof buf, in)) > 0)
                        ok &= std::fwrite(buf, 1, n, out) == n;
                    std::fclose(out);
                    ok = ok && std::rename(part.c_str(), target.c_str()) == 0;
                }
                std::fclose(in);
            }
            if (!ok)
                open_dialog(DialogKind::Info, tr("Could not copy the save"),
                            trf("Writing {path} failed.", {{"path", target}}), "");
            cards_scanned_ = false;
        }
        return Action::None;
    default:
        return Action::None;
    }
}

void App::draw_dialog()
{
    if (!dialog_.open)
        return;
    Gfx &g = *g_;
    const float t = ease_out(dialog_.anim);
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.62f * t), 1, 0);
    g.set_layer(0, 24.0f * (1.0f - t), t);
    const float w = 820;
    const auto lines = wrap(g, Font::Regular, ts(28), dialog_.message, w - 112, 4);
    const float h = 248 + float(lines.size()) * 38;
    const float x = 960 - w * 0.5f, y = 540 - h * 0.5f - 20;
    Glass face;
    face.tint = rgba(0x16348F, 0.92f);
    face.rim = dialog_.danger ? kDanger : rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.4f;
    face.glow = 14;
    face.phase = 0.4f;
    glass_block(g, 960, y + h * 0.5f, w, h, 18, 0, 0, 42, face);
    g.text_mid(Font::Bold, ts(38), x + 56, y + 64, kWhite, Align::Left, fit(g, Font::Bold, ts(38), dialog_.title, w - 112));
    float ly = y + 118;
    for (const std::string &l : lines)
    {
        g.text_mid(Font::Regular, ts(28), x + 56, ly, kSoft, Align::Left, l);
        ly += 38;
    }
    /* Buttons: Cancel and the action, or just OK. */
    const float bh = 60, by = y + h - 56 - bh * 0.5f;
    auto button = [&](float bx, float bw, const std::string &label, bool on, bool danger) {
        g.panel(bx, by, bw, bh, on ? (danger ? rgba(0xB0305A, 0.92f) : rgba(0x1F63F0, 0.92f)) : rgba(0x07102E, 0.5f),
                0.7f, kR, on ? (danger ? kDanger : kIcy) : rgba(0x3D5AB0, 0.8f), on ? 2.2f : 1.4f, on ? 8 : 0,
                on ? 0.25f : 0.0f);
        g.text_mid(Font::Bold, ts(28), bx + bw * 0.5f, by + bh * 0.5f, on ? kWhite : kSoft, Align::Center, label);
    };
    if (dialog_.yes.empty())
        button(x + w - 56 - 220, 220, tr("OK"), true, false);
    else
    {
        button(x + w - 56 - 220, 220, dialog_.yes, dialog_.choice == 1, dialog_.danger);
        button(x + w - 56 - 220 - 24 - 220, 220, tr("Cancel"), dialog_.choice == 0, false);
    }
    g.set_layer();
    drawing_dialog_ = true;
    if (dialog_.yes.empty())
        draw_prompts({{Glyph::Cross, "OK"}}, {}, "");
    else
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Confirm"}, {Glyph::Circle, "Cancel"}}, {}, "");
    drawing_dialog_ = false;
}

/* ---- memory card actions ------------------------------------------------------------------ */

Save *App::focused_save()
{
    Card &card = mc_card_ == 0 ? card_a_ : card_b_;
    const int sel = mc_sel_[mc_card_];
    return sel >= 0 && sel < int(card.saves.size()) ? &card.saves[std::size_t(sel)] : nullptr;
}

static bool is_gci(const std::string &path)
{
    return path.size() > 4 && (path.compare(path.size() - 4, 4, ".gci") == 0 || path.compare(path.size() - 4, 4, ".GCI") == 0);
}

/* The same file name in the other slot's folder (Card A <-> Card B). */
std::string App::copy_target(const Save &s) const
{
    const std::size_t slash = s.path.rfind('/');
    if (slash == std::string::npos)
        return "";
    const std::string dir = s.path.substr(0, slash), name = s.path.substr(slash + 1);
    const std::size_t up = dir.rfind('/');
    const std::string parent = up == std::string::npos ? "" : dir.substr(0, up);
    const std::string card = dir.substr(up == std::string::npos ? 0 : up + 1);
    std::string other;
    if (card == "Card A")
        other = "Card B";
    else if (card == "Card B")
        other = "Card A";
    else
        return "";
    return parent + "/" + other + "/" + name;
}

void App::ask_delete_save()
{
    Save *s = focused_save();
    if (!s)
        return;
    const std::string slot = mc_card_ == 0 ? "A" : "B";
    if (!is_gci(s->path))
    {
        open_dialog(DialogKind::Info, tr("This save is inside a card file"),
                    tr("Porpoise can delete saves that Dolphin keeps as files (its GCI folders), not ones inside a "
                       ".raw memory card image."),
                    "");
        return;
    }
    const std::string what = s->title.empty() ? s->file_name : s->title;
    open_dialog(DialogKind::DeleteSave, tr("Delete this save?"),
                trf("{save}. It is removed from Slot {slot} for good.",
                    {{"save", what + (s->detail.empty() ? "" : " - " + s->detail)}, {"slot", slot}}),
                tr("Delete"), true);
}

void App::ask_copy_save()
{
    Save *s = focused_save();
    if (!s)
        return;
    const std::string to = mc_card_ == 0 ? "B" : "A";
    const std::string target = is_gci(s->path) ? copy_target(*s) : "";
    if (target.empty())
    {
        open_dialog(DialogKind::Info, tr("This save can't be copied"),
                    tr("Porpoise copies saves that Dolphin keeps as files (its GCI folders)."), "");
        return;
    }
    const Card &other = mc_card_ == 0 ? card_b_ : card_a_;
    struct stat st;
    const bool exists = stat(target.c_str(), &st) == 0;
    if (!exists && other.present && s->blocks > other.free_blocks)
    {
        open_dialog(DialogKind::Info, tr("Not enough room"),
                    trf("Slot {slot} has {free} blocks open; this save needs {need}.",
                        {{"slot", to}, {"free", std::to_string(other.free_blocks)}, {"need", std::to_string(s->blocks)}}),
                    "");
        return;
    }
    const std::string what = s->title.empty() ? s->file_name : s->title;
    if (exists)
        open_dialog(DialogKind::CopySave, trf("Replace the save on Slot {slot}?", {{"slot", to}}),
                    trf("Slot {slot} already has {save}. Copying replaces it with this one.", {{"slot", to}, {"save", what}}),
                    tr("Replace"), true);
    else
        open_dialog(DialogKind::CopySave, trf("Copy to Slot {slot}?", {{"slot", to}}),
                    trf("{save} ({blocks}) is copied to Slot {slot}.",
                        {{"save", what}, {"blocks", plural(s->blocks, "1 block", "{n} blocks")}, {"slot", to}}),
                    tr("Copy"));
}

/* ---- drawing: shared ---------------------------------------------------------------------- */

std::string App::clock_text() const
{
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    int hour = tm.tm_hour % 12;
    if (hour == 0)
        hour = 12;
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d:%02d %s", hour, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
    return buf;
}

/* The Porpoise mark and name, centred on cy. Ruben's textured logo
 * (assets/brand/logo.png) replaces the tinted silhouette when it is there. */
void App::draw_brand(float cy)
{
    Gfx &g = *g_;
    if (!logo_tried_)
    {
        logo_tried_ = true;
        const std::string path = g.asset_dir() + "/brand/logo.png";
        if (access(path.c_str(), R_OK) == 0)
            logo_ = g.texture_file(path);
    }
    float x = 48;
    if (logo_ && logo_->height > 0)
    {
        const float h = 72, w = std::min(160.0f, h * float(logo_->width) / float(logo_->height));
        g.image(logo_, x, cy - h * 0.5f, w, h, kWhite);
        x += w + 16;
    }
    else
    {
        if (g.brand_mask())
            g.image(g.brand_mask(), x, cy - 32, 64, 64, kCyan);
        x += 82;
    }
    g.text_mid(Font::SemiBold, 30, x, cy, rgba(0xBFE9FF), Align::Left, "PORPOISE", 7.0f);
}

bool App::draw_mark(float cx, float cy, float width, Color tint)
{
    Gfx &g = *g_;
    if (logo_ && logo_->width > 0)
    {
        /* Ruben's logo keeps its own colours; only the tint's fade applies. */
        const float h = width * float(logo_->height) / float(logo_->width);
        g.image(logo_, cx - width * 0.5f, cy - h * 0.5f, width, h, with_alpha(kWhite, tint.a));
        return true;
    }
    if (!g.brand_mask())
        return false;
    g.image(g.brand_mask(), cx - width * 0.5f, cy - width * 0.5f, width, width, tint);
    return true;
}

void App::draw_top_bar()
{
    Gfx &g = *g_;
    draw_brand(kBarCy);

    /* Section tabs, as wide as their names; the highlight glides to the chosen one. */
    const std::string names[3] = {tr("Library"), tr("Memory Cards"), tr("Settings")};
    float widths[3], bar_w = 12;
    for (int i = 0; i < 3; ++i)
    {
        widths[i] = std::max(170.0f, g.measure(Font::Bold, ts(27), names[i]) + 74);
        bar_w += widths[i];
    }
    const float bar_x = 960 - bar_w * 0.5f;
    g.panel(bar_x, kBarY, bar_w, kBarH, rgba(0x0A1236, 0.70f), 0.9f, kR, rgba(0x3D4F9E, 0.9f), 1.6f);
    {
        float tx = bar_x + 6;
        for (int i = 0; i < 3; ++i)
        {
            tab_x_[i] = tx;
            tab_w_[i] = widths[i];
            tx += widths[i];
        }
    }
    if (pill_x_ < 0)
    {
        pill_x_ = tab_x_[int(tab_)];
        pill_w_ = tab_w_[int(tab_)];
    }
    g.panel(pill_x_, kBarY + 6, pill_w_, kBarH - 12, rgba(0x1F63F0), 0.62f, kR, rgba(0x7FD9FF), 1.6f, 6, 0.35f);
    for (int i = 0; i < 3; ++i)
    {
        const bool on = int(tab_) == i;
        g.text_mid(on ? Font::Bold : Font::SemiBold, ts(27), tab_x_[i] + widths[i] * 0.5f, kBarCy, on ? kWhite : kSoft,
                   Align::Center, names[i]);
    }
    /* L1 / R1 keycaps */
    auto keycap = [&](float cx, Glyph glyph, const char *label) {
        if (g.has_icons())
        {
            g.glyph(glyph, cx, kBarCy, 46, rgba(0xE8F0FF));
            return;
        }
        g.panel(cx - 30, kBarCy - 19, 60, 38, kClear, 1, kR, rgba(0xDDE6FF, 0.85f), 2.0f);
        g.text_mid(Font::Bold, 21, cx, kBarCy, kWhite, Align::Center, label);
    };
    keycap(bar_x - 66, Glyph::L1, "L1");
    keycap(bar_x + bar_w + 66, Glyph::R1, "R1");

    g.text_mid(Font::SemiBold, ts(27), 1866, kBarCy, kWhite, Align::Right, clock_text());
}

/* Two keycaps side by side (L2 R2, L1 R1) where a prompt's glyph would be;
 * returns their width. measure_only draws nothing. */
float App::draw_key_pair(float x, float cy, Glyph which, bool measure_only)
{
    Gfx &g = *g_;
    const char *a = which == kKeyL1R1 ? "L1" : "L2";
    const char *b = which == kKeyL1R1 ? "R1" : "R2";
    if (g.has_icons())
    {
        /* The two buttons' own icons, side by side. */
        const Glyph ga = which == kKeyL1R1 ? Glyph::L1 : Glyph::L2;
        const Glyph gb = which == kKeyL1R1 ? Glyph::R1 : Glyph::R2;
        const float cell = 44, gap = 4;
        if (!measure_only)
        {
            g.glyph(ga, x + cell * 0.5f, cy, cell, kWhite);
            g.glyph(gb, x + cell * 1.5f + gap, cy, cell, kWhite);
        }
        return cell * 2 + gap;
    }
    const float kw = 50, kh = 34, gap = 6;
    if (!measure_only)
        for (int i = 0; i < 2; ++i)
        {
            const float kx = x + float(i) * (kw + gap);
            g.panel(kx, cy - kh * 0.5f, kw, kh, kClear, 1, kR * 0.8f, rgba(0xDDE6FF, 0.9f), 2.0f);
            g.text_mid(Font::Bold, 19, kx + kw * 0.5f, cy, kWhite, Align::Center, i == 0 ? a : b);
        }
    return kw * 2 + gap;
}

void App::draw_prompts(const std::vector<std::pair<Glyph, std::string>> &left_in,
                       const std::vector<std::pair<Glyph, std::string>> &right_in, const std::string &center)
{
    Gfx &g = *g_;
    if (dialog_.open && !drawing_dialog_)
        return; /* the dialog brings its own */
    /* Every prompt is translated here, so callers write plain English. */
    std::vector<std::pair<Glyph, std::string>> left_tr, right_tr;
    for (const auto &p : left_in)
        left_tr.push_back({p.first, tr(p.second)});
    for (const auto &p : right_in)
        right_tr.push_back({p.first, tr(p.second)});
    const auto &left = left_tr;
    const auto &right = right_tr;
    const float y = kPromptY, size = ts(28);
    float x = 58;
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        if (i > 0)
        {
            g.panel(x - 2, y - 22, 1.6f, 44, rgba(0x5A68A8, 0.8f), 1, 0);
            x += 38;
        }
        if (int(left[i].first) >= int(kKeyL2R2))
            x += draw_key_pair(x, y, left[i].first) - 54 + 14;
        else
            g.glyph(left[i].first, x + 20, y, 44, kWhite);
        x += 54;
        x += g.text_mid(Font::SemiBold, size, x, y, kWhite, Align::Left, left[i].second) + 40;
    }
    float rx = 1862;
    for (std::size_t i = right.size(); i-- > 0;)
    {
        const float w = g.measure(Font::SemiBold, size, right[i].second);
        g.text_mid(Font::SemiBold, size, rx, y, kWhite, Align::Right, right[i].second);
        float glyph_x = rx - w - 36;
        if (int(right[i].first) >= int(kKeyL2R2))
        {
            const float kw = draw_key_pair(0, y, right[i].first, true);
            glyph_x = rx - w - 14 - kw;
            draw_key_pair(glyph_x, y, right[i].first);
            glyph_x += 22; /* as if a glyph centred here */
        }
        else
            g.glyph(right[i].first, glyph_x, y, 44, kWhite);
        rx = glyph_x - 22 - 38;
        if (i > 0)
        {
            g.panel(rx, y - 22, 1.6f, 44, rgba(0x5A68A8, 0.8f), 1, 0);
            rx -= 38;
        }
    }
    if (!center.empty())
        g.text_mid(Font::SemiBold, ts(26), 960, y, kLavender, Align::Center, center, 2.0f);
}

/* One glass tile with its cover, in perspective, and its reflection. */
void App::draw_tile(Game *game, float cx, float cy, float w, float h, float yaw, float alpha, bool focused,
                    bool reflection)
{
    Gfx &g = *g_;
    const float m = 13.0f; /* glass frame margin */
    const float glow = focused ? 20.0f : 0.0f;
    const float ex = w * 0.5f + m + glow * 3, ey = h * 0.5f + m + glow * 3;
    const float inner_r = kR * 0.6f; /* the cover sits inside the frame */
    Texture *cover = game ? cover_of(*game) : nullptr;

    if (reflection)
    {
        /* The tile mirrored below its bottom edge, fading out. */
        const float base = cy + h * 0.5f + m;
        const Corner r[4] = {project(cx, base, -w * 0.5f, 0, yaw), project(cx, base, w * 0.5f, 0, yaw),
                             project(cx, base, w * 0.5f, h * 0.85f, yaw), project(cx, base, -w * 0.5f, h * 0.85f, yaw)};
        if (cover)
        {
            float uv[4] = {0, 0, 1, 1};
            const float ta = float(cover->width) / float(cover->height), aa = w / h;
            if (ta > aa)
            {
                const float k = aa / ta;
                uv[0] = 0.5f - k * 0.5f;
                uv[2] = 0.5f + k * 0.5f;
            }
            uv[1] = 0.15f;
            g.quad3d(cover, r, w, h * 0.85f, with_alpha(rgba(0xB8C8FF), 0.30f * alpha), inner_r, true, true, uv);
        }
        else
            g.quad3d(nullptr, r, w, h * 0.85f, with_alpha(rgba(0x2A4FD0), 0.25f * alpha), inner_r, true, true);
        return;
    }

    /* The block: a slab of blue glass with real thickness, the cover set into its face. */
    Glass face;
    face.tint = kGlassBody;
    face.rim = focused ? kIcy : kEdge;
    face.fade = alpha;
    face.radius = kR;
    face.rim_w = focused ? 3.2f : 2.2f;
    face.glow = glow;
    face.phase = cx / 1920.0f;
    const float depth = 34.0f * (w / kTileW);
    if (std::cos(yaw) < 0.0f && game)
    {
        /* Turned around: the back of the box shows, set into the far face. */
        glass_block(g, cx, cy, w + m * 2, h + m * 2, depth, yaw, 0, glow * 3, face, true);
        Corner b[4];
        rect_at(cx, cy, w * 0.5f, h * 0.5f, yaw, depth, 0, b);
        if (Texture *back = back_of(*game))
        {
            float uv[4];
            cover_uv(back, w, h, uv);
            std::swap(uv[0], uv[2]); /* seen from behind, the box's left is on the right */
            g.quad3d(back, b, w, h, with_alpha(kWhite, alpha), inner_r, false, false, uv);
        }
        else
        {
            g.quad3d(nullptr, b, w, h, with_alpha(rgba(0x13286F), alpha), inner_r, false, false);
            Texture *mark = logo_ ? logo_ : g.brand_mask();
            if (mark)
            {
                Corner lc[4];
                const float lw = w * 0.42f, lh = lw * float(mark->height) / float(std::max(1, mark->width));
                rect_at(cx, cy, lw * 0.5f, lh * 0.5f, yaw, depth, 0, lc);
                std::swap(lc[0], lc[1]);
                std::swap(lc[2], lc[3]);
                g.quad3d(mark, lc, lw, lh, with_alpha(logo_ ? kWhite : kCyan, 0.85f * alpha), 0, false, false);
            }
        }
        gloss_over(g, b, w, h, inner_r, cx / 1920.0f, alpha);
        (void)ex;
        (void)ey;
        return;
    }
    glass_block(g, cx, cy, w + m * 2, h + m * 2, depth, yaw, 0, glow * 3, face);
    (void)ex;
    (void)ey;

    const Corner c[4] = {project(cx, cy, -w * 0.5f, -h * 0.5f, yaw), project(cx, cy, w * 0.5f, -h * 0.5f, yaw),
                         project(cx, cy, w * 0.5f, h * 0.5f, yaw), project(cx, cy, -w * 0.5f, h * 0.5f, yaw)};
    if (cover)
    {
        float uv[4] = {0, 0, 1, 1};
        const float ta = float(cover->width) / float(cover->height), aa = w / h;
        if (ta > aa)
        {
            const float k = aa / ta;
            uv[0] = 0.5f - k * 0.5f;
            uv[2] = 0.5f + k * 0.5f;
        }
        else
        {
            const float k = ta / aa;
            uv[1] = 0.5f - k * 0.5f;
            uv[3] = 0.5f + k * 0.5f;
        }
        g.quad3d(cover, c, w, h, with_alpha(kWhite, alpha), inner_r, false, false, uv);
    }
    else
    {
        /* No cover yet: a Porpoise tile with the game's title. */
        g.quad3d(nullptr, c, w, h, with_alpha(rgba(0x16328F), alpha), inner_r, false, false);
        if (std::fabs(yaw) < 0.08f && game)
        {
            const float s = w / kTileW;
            draw_mark(cx, cy - h * 0.30f + 70 * s, 150 * s, with_alpha(rgba(0x6FD8FF), 0.9f * alpha));
            /* Title, wrapped to two lines. */
            const std::string &t = game->title;
            const float size = 34 * s, max_w = w - 40 * s;
            std::vector<std::string> lines;
            std::string line, word;
            for (std::size_t i = 0; i <= t.size(); ++i)
            {
                if (i == t.size() || t[i] == ' ')
                {
                    const std::string trial = line.empty() ? word : line + " " + word;
                    if (!line.empty() && g.measure(Font::ExtraBold, size, trial) > max_w)
                    {
                        lines.push_back(line);
                        line = word;
                    }
                    else
                        line = trial;
                    word.clear();
                }
                else
                    word += t[i];
            }
            if (!line.empty())
                lines.push_back(line);
            if (lines.size() > 3)
                lines.resize(3);
            float ty = cy + h * 0.06f;
            for (const std::string &l : lines)
            {
                g.text(Font::ExtraBold, size, cx, ty, with_alpha(kWhite, alpha), Align::Center, l);
                ty += size * 1.15f;
            }
        }
    }

    /* The glass over the art: gloss and the room's reflection. */
    Glass gloss;
    gloss.face = 2;
    gloss.radius = inner_r;
    gloss.rim_w = 0;
    gloss.tint = kWhite;
    gloss.rim = kClear;
    gloss.fade = alpha;
    gloss.phase = cx / 1920.0f;
    g.glass(c, w, h, 0, gloss);
}

/* ---- library ------------------------------------------------------------------------------ */

void App::draw_empty()
{
    Gfx &g = *g_;
    g.text(Font::Bold, ts(54), 90, 132, kWhite, Align::Left, tr("Your games"));
    g.text(Font::SemiBold, ts(32), 92, 200, kLavender, Align::Left, tr("No games yet"));
    g.panel(560, 330, 800, 380, rgba(0x13256F, 0.55f), 0.6f, kR, rgba(0x5A8CFF, 0.9f), 2.0f, 0, 0.2f);
    draw_mark(960, 420, 150, kCyan);
    g.text(Font::Bold, ts(40), 960, 500, kWhite, Align::Center, tr("Add games"));
    g.text(Font::Regular, ts(26), 960, 560, kSoft, Align::Center, tr("Copy .iso, .rvz or .ciso files with PS5 Upload into"));
    g.text(Font::SemiBold, ts(26), 960, 600, kIcy, Align::Center, "/data/porpoise/games");
    g.text(Font::Regular, ts(26), 960, 640, kSoft, Align::Center, tr("or add your own folder in Settings, under Games."));
}

void App::draw_library(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    if (games.empty())
    {
        draw_empty();
        draw_prompts({}, {}, "");
        return;
    }
    const std::string count = plural((long long)games.size(), "1 game", "{n} games");
    g.text(Font::Bold, ts(54), 90, 132, kWhite, Align::Left, tr("Your games"));
    const float cw = g.text(Font::SemiBold, ts(32), 92, 200, kLavender, Align::Left, count);
    if (!note_.empty())
        g.text(Font::Regular, ts(26), 92 + cw + 24, 205, with_alpha(kIcy, 0.85f), Align::Left,
               "\xE2\x80\xA2  " + note_);

    /* Layout of a tile at offset k from the selection (k may be fractional
     * while sliding): x, scale and yaw. */
    auto layout = [](float k, float &x, float &scale, float &yaw) {
        const float a = std::fabs(k), s = k < 0 ? -1.0f : 1.0f;
        if (a <= 1.0f)
        {
            x = kCx + s * a * 372.0f;
            scale = 1.0f - 0.14f * a;
            yaw = s * a * 0.22f;
        }
        else
        {
            x = kCx + s * (372.0f + (a - 1.0f) * 284.0f);
            scale = std::max(0.5f, 0.86f - 0.10f * (a - 1.0f));
            yaw = s * 0.22f;
        }
    };

    /* Back to front: far tiles first, the selection last. */
    std::vector<int> order;
    for (int i = 0; i < int(games.size()); ++i)
        if (std::fabs(float(i) - scroll_) < 4.2f)
            order.push_back(i);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return std::fabs(float(a) - scroll_) > std::fabs(float(b) - scroll_);
    });
    for (int pass = 0; pass < 2; ++pass) /* reflections, then tiles */
        for (int i : order)
        {
            const float k = float(i) - scroll_;
            float x, s, yaw;
            layout(k, x, s, yaw);
            const float a = std::fabs(k);
            const float alpha = std::clamp(1.0f - (a - 3.0f), 0.0f, 1.0f);
            const bool focused = i == selected_ && a < 0.5f;
            const float lift = focused ? (lift_ * 6.0f) : 0.0f;
            /* The focused block turns gently, showing its glass edge. */
            const float sway = settings_->reduced_motion ? 0.0f : std::sin(float(time) * 0.9f) * 0.06f * (1.0f - std::min(a * 2.0f, 1.0f));
            draw_tile(&games[std::size_t(i)], x, kCy - lift, kTileW * s, kTileH * s, yaw + sway, alpha, focused,
                      pass == 0);
        }

    /* Covers far from the selection give their memory back; they reload
     * when they come into view again. */
    for (int i = 0; i < int(games.size()); ++i)
        if (std::abs(i - selected_) > 9 && games[std::size_t(i)].cover)
        {
            g.free_texture(games[std::size_t(i)].cover);
            games[std::size_t(i)].cover = nullptr;
            games[std::size_t(i)].cover_tried = false;
        }

    /* Focus pointer, side arrows. */
    const float bob = settings_->reduced_motion ? 0.0f : std::sin(float(time) * 2.4f) * 3.0f;
    g.glyph(Glyph::Pointer, kCx, kCy - kTileH * 0.5f - 52 + bob, 52, kWhite);
    if (selected_ > 0)
        g.glyph(Glyph::Arrow, 34, kCy, 46, rgba(0x58B8FF), -kPi * 0.5f);
    if (selected_ + 1 < int(games.size()))
        g.glyph(Glyph::Arrow, 1886, kCy, 46, rgba(0x58B8FF), kPi * 0.5f);

    /* Title, details, play. */
    Game &sel = games[std::size_t(selected_)];
    g.text(Font::Bold, ts(46), kCx, 748, kWhite, Align::Center, sel.title);
    const std::string meta =
        sel.platform + "   \xE2\x80\xA2   " + relative_time(sel.last_played, (long long)std::time(nullptr));
    g.text(Font::SemiBold, ts(28), kCx, 812, kLavender, Align::Center, meta);

    /* Play: the Cross and the word, centred together in the button. */
    const float ph = 64, py = 864, gsize = 42, gap = 14;
    const float label_w = g.measure(Font::Bold, ts(32), tr("Play"));
    const float group = gsize + gap + label_w, pw = std::max(214.0f, group + 84);
    g.panel(kCx - pw * 0.5f, py, pw, ph, rgba(0x0B1640, 0.85f), 0.8f, kR, rgba(0x8BD9FF), 2.0f, 8, 0.15f);
    const float gx = kCx - group * 0.5f;
    g.glyph(Glyph::Cross, gx + gsize * 0.5f, py + ph * 0.5f, gsize, kWhite);
    g.text_mid(Font::Bold, ts(32), gx + gsize + gap, py + ph * 0.5f, kWhite, Align::Left, tr("Play"));

    char pos[32];
    std::snprintf(pos, sizeof pos, "%02d / %02zu", selected_ + 1, games.size());
    draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Cross, "Play"}},
                 {{Glyph::Square, "Details"}, {Glyph::Triangle, "Sort"}}, pos);
}

void App::draw_sort()
{
    Gfx &g = *g_;
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.55f), 1, 0);
    const float w = 560, h = 300, x = 960 - w * 0.5f, y = 390;
    g.panel(x, y, w, h, rgba(0x13256F, 0.92f), 0.65f, kR, rgba(0x6FAEFF), 2.0f, 10, 0.25f);
    g.text_mid(Font::Bold, ts(36), x + 40, y + 62, kWhite, Align::Left, tr("Sort games"));
    const std::string labels[2] = {tr("Title A-Z"), tr("Recently played")};
    for (int i = 0; i < 2; ++i)
    {
        const float ry = y + 112 + i * 80, rh = 62, cy = ry + rh * 0.5f;
        const bool on = sort_row_ == i;
        g.panel(x + 40, ry, w - 80, rh, on ? rgba(0x1D3FA8, 0.9f) : rgba(0x0E1C55, 0.6f), 0.8f, kR,
                on ? kIcy : rgba(0x3D5AB0, 0.8f), on ? 2.4f : 1.4f, on ? 10 : 0);
        g.text_mid(Font::SemiBold, ts(30), x + 72, cy, on ? kWhite : kSoft, Align::Left, labels[i]);
        const bool current = (lib_->sort_order() == Library::Sort::Title) == (i == 0);
        if (current)
            g.text_mid(Font::SemiBold, ts(24), x + w - 72, cy, kCyan, Align::Right, tr("Current"));
    }
    draw_prompts({{Glyph::Cross, "Choose"}, {Glyph::Circle, "Back"}}, {}, "");
}

/* ---- details ------------------------------------------------------------------------------ */

void App::draw_details(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    if (games.empty())
        return;
    Game &game = games[std::size_t(selected_)];

    /* The box, large, in glass: 5 wide for 7 high. It flies in from the
     * library turning over once, and turns with the right stick. */
    const float cw = 486, ch = 680, ccx = 120 + cw * 0.5f, ccy = 150 + ch * 0.5f + 10;
    /* L2 / R2: the old box slides off one side as the new one swings in from
     * the other, and the panel follows a little behind. */
    const bool swiping = swipe_anim_ > 0 && swipe_from_ >= 0 && swipe_from_ < int(games.size());
    const float st = swiping ? ease_out(1.0f - swipe_anim_) : 1.0f;
    {
        const float p = ease_out(1.0f - flip_anim_);
        const float bx = kCx + (ccx - kCx) * p, by = kCy + (ccy - kCy) * p;
        const float bw = kTileW + (cw - kTileW) * p, bh = kTileH + (ch - kTileH) * p;
        g.set_layer(); /* the box is not part of the panel's fade */
        if (swiping)
        {
            Game &old = games[std::size_t(swipe_from_)];
            draw_tile(&old, ccx - float(swipe_dir_) * 760.0f * st, ccy, cw, ch, -float(swipe_dir_) * 0.9f * st,
                      1.0f - st, false, false);
        }
        draw_tile(&game, bx + (swiping ? float(swipe_dir_) * 760.0f * (1.0f - st) : 0.0f), by, bw, bh, box_yaw_,
                  swiping ? st : 1.0f, false, false);
        g.set_layer(layer_dx_ + (swiping ? float(swipe_dir_) * 120.0f * (1.0f - st) : 0.0f), layer_dy_,
                    layer_fade_ * (swiping ? 0.15f + 0.85f * st : 1.0f));
    }

    /* Information panel. */
    const float px = 690, py = 150, pw = 1140, ph = 790;
    g.panel(px, py, pw, ph, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);

    /* The disc, turning slowly, at the panel's top right. */
    float text_w = pw - 104;
    if (Texture *disc = disc_of(game))
    {
        const float d = 176, dcx = px + pw - 52 - d * 0.5f, dcy = py + 36 + d * 0.5f;
        const float angle = settings_->reduced_motion ? 0.0f : float(time) * 0.6f;
        Corner c[4];
        const float ca = std::cos(angle), sa = std::sin(angle), r = d * 0.5f;
        const float pts[4][2] = {{-r, -r}, {r, -r}, {r, r}, {-r, r}};
        for (int i = 0; i < 4; ++i)
            c[i] = Corner{dcx + pts[i][0] * ca - pts[i][1] * sa, dcy + pts[i][0] * sa + pts[i][1] * ca, 1};
        g.blob(dcx, dcy + 8, d * 1.2f, d * 1.2f, rgba(0x000000, 0.45f));
        g.quad3d(disc, c, d, d, kWhite, r, false, false);
        Corner gc[4];
        rect_at(dcx, dcy, r, r, 0, 0, 0, gc);
        gloss_over(g, gc, d, d, r, 0.7f);
        text_w = pw - 104 - d - 30;
    }

    const std::string title = game.db_title.empty() ? game.title : game.db_title;
    const auto title_lines = wrap(g, Font::Bold, ts(48), title, text_w, 2);
    float ty = py + 70;
    for (const std::string &l : title_lines)
    {
        g.text_mid(Font::Bold, ts(48), px + 52, ty, kWhite, Align::Left, l);
        ty += 56;
    }
    std::string sub = game.platform;
    if (!game.region.empty())
        sub += "   \xE2\x80\xA2   " + tr(game.region);
    if (!game.id.empty())
        sub += "   \xE2\x80\xA2   " + game.id;
    g.text_mid(Font::SemiBold, ts(26), px + 52, ty - 6, kCyan, Align::Left, sub);

    /* Facts: two columns of label and value. */
    std::vector<std::pair<std::string, std::string>> facts;
    if (!game.developer.empty()) facts.push_back({tr("Developer"), game.developer});
    if (!game.publisher.empty()) facts.push_back({tr("Publisher"), game.publisher});
    if (!game.released.empty()) facts.push_back({tr("Released"), game.released});
    if (!game.genre.empty()) facts.push_back({tr("Genre"), game.genre});
    if (game.players > 0) facts.push_back({tr("Players"), std::to_string(game.players)});
    if (!game.rating.empty()) facts.push_back({tr("Rating"), game.rating});
    facts.push_back({tr("Last played"), relative_time(game.last_played, (long long)std::time(nullptr))});
    facts.push_back({tr("File"), game.format + "  \xE2\x80\xA2  " + human_size(game.bytes)});
    if (facts.size() > 9)
        facts.resize(9);
    const float fy0 = std::max(ty + 34, py + 236), col_w = (pw - 104) / 3.0f;
    for (std::size_t i = 0; i < facts.size(); ++i)
    {
        const float fx = px + 52 + float(i % 3) * col_w, fy = fy0 + float(i / 3) * 62;
        g.text_mid(Font::Regular, ts(22), fx, fy, kLavender, Align::Left, facts[i].first);
        g.text_mid(Font::SemiBold, ts(27), fx, fy + 28, kWhite, Align::Left,
                   fit(g, Font::SemiBold, ts(27), facts[i].second, col_w - 30));
    }
    float dy = fy0 + float((facts.size() + 2) / 3) * 62 + 6;

    /* What the game is about. */
    const float actions_y = py + ph - 4 * 66 - 24;
    const std::size_t room = std::size_t(std::max(0.0f, (actions_y - 16 - dy) / 34.0f));
    if (!game.synopsis.empty() && room > 0)
    {
        for (const std::string &l : wrap(g, Font::Regular, ts(25), game.synopsis, pw - 104, room))
        {
            g.text_mid(Font::Regular, ts(25), px + 52, dy + 12, kSoft, Align::Left, l);
            dy += 34;
        }
    }
    else if (game.synopsis.empty() && room > 0)
        g.text_mid(Font::Regular, ts(24), px + 52, dy + 12, with_alpha(kLavender, 0.8f), Align::Left,
                   settings_->download_info ? tr("No description yet. It arrives with the game info from GameTDB.com.")
                                            : tr("Turn on Settings > Games > Download game info for a description."));

    const std::string actions[4] = {tr("Play"), tr("Save states"), tr("Game settings"), tr("Save data")};
    for (int i = 0; i < 4; ++i)
    {
        const float ry = actions_y + i * 66, rx = px + 52 + 40, rw = pw - 104 - 40, rh = 58;
        const bool on = details_row_ == i;
        g.panel(rx, ry, rw, rh, on ? rgba(0x153A9E, 0.9f) : rgba(0x0E1C55, 0.55f), 0.8f, kR,
                on ? kIcy : rgba(0x3D5AB0, 0.8f), on ? 2.6f : 1.4f, on ? 12 : 0, on ? 0.15f : 0.0f);
        g.text_mid(Font::SemiBold, ts(29), rx + 36, ry + rh * 0.5f, on ? kWhite : kSoft, Align::Left, actions[i]);
        if (i == 1 && details_states_ > 0)
            g.text_mid(Font::SemiBold, ts(22), rx + rw - 30, ry + rh * 0.5f, kCyan, Align::Right,
                       plural(details_states_, "1 saved", "{n} saved"));
        if (i == 2 && details_custom_)
            g.text_mid(Font::SemiBold, ts(22), rx + rw - 30, ry + rh * 0.5f, kCyan, Align::Right, tr("Custom"));
        if (on)
            g.glyph(Glyph::Arrow, rx - 26, ry + rh * 0.5f, 32, kWhite, kPi * 0.5f);
    }
    g.set_layer(layer_dx_, layer_dy_, layer_fade_);
    if (screen_ != Screen::States) /* the save states bring their own */
        draw_prompts({{Glyph::Cross, "Confirm"}, {Glyph::Circle, "Back"}},
                     {{kKeyL2R2, "Other games"}, {Glyph::Triangle, box_back_ ? "Front of box" : "Back of box"}},
                     games.size() > 1 ? "" : tr("Right stick: turn the box"));
}

/* ---- memory cards ------------------------------------------------------------------------- */

/* One card, the way the GameCube shows it: the slot letter, the free blocks,
 * and a grid of save icons on glass. */
void App::draw_card(Card &card, int which, float x, float y, double time)
{
    Gfx &g = *g_;
    const bool active = mc_card_ == which;
    g.panel(x, y, kMcW, kMcH, rgba(0x0F1F63, 0.62f), 0.75f, kR,
            active ? rgba(0x6FAEFF, 0.95f) : rgba(0x3D4F9E, 0.8f), active ? 2.2f : 1.6f, 0, 0.12f);

    /* Header: the slot letter, the card, its free blocks. */
    const float head_cy = y + 66;
    const float bx = x + 34, bs = 72;
    g.panel(bx, head_cy - bs * 0.5f, bs, bs, rgba(0x1F63F0), 0.62f, kR, rgba(0x7FD9FF), 1.8f, active ? 8 : 0,
            0.35f);
    g.text_mid(Font::ExtraBold, 44, bx + bs * 0.5f, head_cy, kWhite, Align::Center, card.slot);
    const int n = int(card.saves.size());
    std::string sub;
    if (!card.present)
        sub = tr("No saves yet");
    else
        sub = plural(n, "1 save", "{n} saves");
    g.text_mid(Font::Bold, ts(32), bx + bs + 24, head_cy - 17, kWhite, Align::Left, trf("Slot {slot}", {{"slot", card.slot}}));
    g.text_mid(Font::Regular, ts(24), bx + bs + 24, head_cy + 21, kLavender, Align::Left, sub);

    const std::string free = card.present ? std::to_string(card.free_blocks) : std::to_string(card.total_blocks);
    const float num_w = g.measure(Font::Bold, ts(32), free);
    const float box_w = std::max(116.0f, num_w + 44), box_h = 54, box_x = x + kMcW - 34 - box_w;
    g.panel(box_x, head_cy - box_h * 0.5f, box_w, box_h, rgba(0x07102E, 0.6f), 1, kR, rgba(0x8BD9FF, 0.9f), 1.8f);
    g.text_mid(Font::Bold, ts(32), box_x + box_w * 0.5f, head_cy, kWhite, Align::Center, free);
    g.text_mid(Font::SemiBold, ts(26), box_x - 16, head_cy, kSoft, Align::Right, trc("memcard", "Open"));

    /* Grid, scrolled so the focused row shows. */
    const int rows_total = std::max(kMcRows, (n + kMcCols - 1) / kMcCols);
    const int sel = mc_sel_[which];
    int first = int(mc_scroll_[which]);
    if (sel / kMcCols < first)
        first = sel / kMcCols;
    if (sel / kMcCols > first + kMcRows - 1)
        first = sel / kMcCols - (kMcRows - 1);
    first = std::clamp(first, 0, rows_total - kMcRows);
    mc_scroll_[which] = float(first);

    const float grid_w = kMcCols * kMcTile + (kMcCols - 1) * kMcGap;
    const float gx = x + (kMcW - grid_w) * 0.5f, gy = y + 146;
    float focus_cx = -1, focus_top = 0;
    for (int pass = 0; pass < 2; ++pass) /* the focused tile last, above its neighbours */
        for (int r = 0; r < kMcRows; ++r)
            for (int c = 0; c < kMcCols; ++c)
            {
                const int idx = (first + r) * kMcCols + c;
                const bool focused = active && idx == sel;
                if ((pass == 1) != focused)
                    continue;
                const float tx = gx + c * (kMcTile + kMcGap), ty = gy + r * (kMcTile + kMcGap);
                if (idx >= n)
                {
                    /* An empty slot, pressed into the card. */
                    g.panel(tx, ty, kMcTile, kMcTile, rgba(0x050C26, 0.55f), 1.0f, kR,
                            focused ? kIcy : rgba(0x2A3C80, 0.65f), focused ? 2.4f : 1.4f);
                    if (focused)
                    {
                        focus_cx = tx + kMcTile * 0.5f;
                        focus_top = ty;
                    }
                    continue;
                }
                Save &s = card.saves[std::size_t(idx)];
                /* A small glass block; the focused one rises out of the card toward you. */
                const float bcx = tx + kMcTile * 0.5f, bcy = ty + kMcTile * 0.5f;
                const float z0 = focused ? -90.0f * mc_lift_ : 0.0f;
                Glass face;
                face.tint = kGlassBody;
                face.rim = focused ? kIcy : kEdge;
                face.radius = kR;
                face.rim_w = focused ? 3.0f : 2.0f;
                face.glow = focused ? 16.0f : 0.0f;
                face.phase = bcx / 1920.0f + bcy / 3000.0f;
                glass_block(g, bcx, bcy, kMcTile, kMcTile, 20.0f, 0, z0, face.glow * 3, face);
                Corner ic[4];
                rect_at(bcx, bcy, 48, 48, 0, 0, z0, ic);
                if (Texture *icon = save_icon(s))
                    g.quad3d(icon, ic, 96, 96, kWhite, 0, false, false);
                else if (g.brand_mask())
                {
                    rect_at(bcx, bcy, 36, 36, 0, 0, z0, ic);
                    g.quad3d(g.brand_mask(), ic, 72, 72, with_alpha(kCyan, 0.85f), 0, false, false);
                }
                Corner tc[4];
                rect_at(bcx, bcy, kMcTile * 0.5f, kMcTile * 0.5f, 0, 0, z0, tc);
                Glass gloss;
                gloss.face = 2;
                gloss.radius = kR;
                gloss.rim_w = 0;
                gloss.tint = kWhite;
                gloss.rim = kClear;
                gloss.phase = face.phase;
                g.glass(tc, kMcTile, kMcTile, 0, gloss);
                if (focused)
                {
                    focus_cx = (tc[0].x + tc[1].x) * 0.5f;
                    focus_top = tc[0].y;
                }
            }

    /* More saves above or below the visible rows. */
    if (first > 0)
        g.glyph(Glyph::Arrow, x + kMcW - 22, gy + 16, 18, rgba(0x58B8FF), 0);
    if (first + kMcRows < rows_total && (first + kMcRows) * kMcCols < n)
        g.glyph(Glyph::Arrow, x + kMcW - 22, gy + kMcRows * (kMcTile + kMcGap) - kMcGap - 16, 18,
                rgba(0x58B8FF), kPi);

    if (focus_cx >= 0)
    {
        const float bob = settings_->reduced_motion ? 0.0f : std::sin(float(time) * 2.4f) * 3.0f;
        g.glyph(Glyph::Pointer, focus_cx, focus_top - 24 + bob, 34, kWhite);
    }
}

void App::draw_memory_cards(double time)
{
    Gfx &g = *g_;
    if (!cards_scanned_)
        scan_memory_cards();
    draw_card(card_a_, 0, kMcX[0], kMcY, time);
    draw_card(card_b_, 1, kMcX[1], kMcY, time);

    /* Information bar: the focused save's banner and what it is. */
    const float ix = kMcX[0], iy = 812, iw = kMcX[1] + kMcW - kMcX[0], ih = 128, icy = iy + ih * 0.5f;
    g.panel(ix, iy, iw, ih, rgba(0x0A1236, 0.72f), 0.85f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    Card &card = mc_card_ == 0 ? card_a_ : card_b_;
    const int n = int(card.saves.size()), sel = mc_sel_[mc_card_];
    const float pic_x = ix + 18, pic_w = 288, pic_h = 96;
    const float text_x = pic_x + pic_w + 30;
    if (sel < n)
    {
        Save &s = card.saves[std::size_t(sel)];
        if (Texture *banner = save_banner(s))
        {
            g.image(banner, pic_x, icy - pic_h * 0.5f, pic_w, pic_h, kWhite, kR);
            g.panel(pic_x, icy - pic_h * 0.5f, pic_w, pic_h, kClear, 1, kR, rgba(0x7FB6FF, 0.8f), 1.6f);
        }
        else
        {
            g.panel(pic_x, icy - pic_h * 0.5f, pic_w, pic_h, kTileFill, 0.55f, kR, kEdge, 1.6f);
            if (Texture *icon = save_icon(s))
                g.image(icon, pic_x + pic_w * 0.5f - 40, icy - 40, 80, 80, kWhite);
        }
        const std::string blocks = plural(s.blocks, "1 block", "{n} blocks");
        const float right_x = ix + iw - 32;
        const float bw = g.measure(Font::Bold, ts(32), blocks);
        const float tw = right_x - std::max(bw, 260.0f) - 40 - text_x;
        const std::string title = s.title.empty() ? s.file_name : s.title;
        g.text_mid(Font::Bold, ts(36), text_x, icy - 19, kWhite, Align::Left, fit(g, Font::Bold, ts(36), title, tw));
        g.text_mid(Font::Regular, ts(26), text_x, icy + 23, kLavender, Align::Left,
                   fit(g, Font::Regular, ts(26), s.detail.empty() ? s.game_code : s.detail, tw));
        g.text_mid(Font::Bold, ts(32), right_x, icy - 19, kWhite, Align::Right, blocks);
        g.text_mid(Font::Regular, ts(26), right_x, icy + 23, kLavender, Align::Right, format_date(s.modified));
    }
    else
    {
        g.panel(pic_x, icy - pic_h * 0.5f, pic_w, pic_h, rgba(0x050C26, 0.55f), 1.0f, kR, rgba(0x2A3C80, 0.65f),
                1.4f);
        draw_mark(pic_x + pic_w * 0.5f, icy, 90, with_alpha(kCyan, 0.6f));
        g.text_mid(Font::Bold, ts(36), text_x, icy - 19, kWhite, Align::Left,
                   trf("Nothing saved in Slot {slot} yet", {{"slot", card.slot}}));
        g.text_mid(Font::Regular, ts(26), text_x, icy + 23, kLavender, Align::Left,
                   tr("Your saves appear here after you save in a game."));
    }

    char pos[48];
    if (n > 0)
        std::snprintf(pos, sizeof pos, "%s   %02d / %02d", trf("SLOT {slot}", {{"slot", card.slot}}).c_str(), sel + 1, n);
    else
        std::snprintf(pos, sizeof pos, "%s", trf("SLOT {slot}", {{"slot", card.slot}}).c_str());
    if (sel < n)
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Circle, "Back"}},
                     {{Glyph::Square, trf("Copy to {slot}", {{"slot", mc_card_ == 0 ? "B" : "A"}})}, {Glyph::Triangle, "Delete"}},
                     pos);
    else
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Circle, "Back"}}, {}, pos);
}

void App::release_covers()
{
    for (Game &game : lib_->games())
    {
        if (game.cover)
            g_->free_texture(game.cover);
        if (game.disc)
            g_->free_texture(game.disc);
        if (game.back)
            g_->free_texture(game.back);
        game.cover = game.disc = game.back = nullptr;
        game.cover_tried = game.disc_tried = game.back_tried = false;
    }
}

void App::library_changed()
{
    auto &games = lib_->games();
    const std::string key = lib_->selected_id();
    selected_ = 0;
    for (std::size_t i = 0; i < games.size(); ++i)
        if (Library::key_of(games[i]) == key)
            selected_ = int(i);
    selected_ = std::clamp(selected_, 0, std::max(0, int(games.size()) - 1));
    scroll_ = float(selected_);
    build_settings();
}

void App::cover_arrived(const std::string &id)
{
    for (Game &game : lib_->games())
        if (game.id == id)
        {
            if (game.cover)
                g_->free_texture(game.cover);
            if (game.disc)
                g_->free_texture(game.disc);
            if (game.back)
                g_->free_texture(game.back);
            game.cover = game.disc = game.back = nullptr;
            game.cover_tried = game.disc_tried = game.back_tried = false;
        }
}

/* ---- frame ------------------------------------------------------------------------------- */

void App::draw(double time)
{
    Gfx &g = *g_;
    g.background();
    draw_top_bar();

    /* Motion: a new tab slides in from the side it came from; a screen that
     * opens rises and fades in. */
    float dx = 0, dy = 0, fade = 1;
    if (tab_anim_ > 0)
    {
        const float p = ease_out(1.0f - tab_anim_);
        dx = float(tab_dir_) * 150.0f * (1.0f - p);
        fade = 0.2f + 0.8f * p;
    }
    if (screen_anim_ > 0)
    {
        const float p = ease_out(1.0f - screen_anim_);
        dy = 22.0f * (1.0f - p);
        fade *= 0.25f + 0.75f * p;
    }
    g.set_layer(dx, dy, fade);
    layer_dx_ = dx;
    layer_dy_ = dy;
    layer_fade_ = fade;
    if (screen_ == Screen::Details)
        draw_details(time);
    else if (screen_ == Screen::States)
    {
        draw_details(time);
        draw_states();
    }
    else if (screen_ == Screen::Browse)
        draw_browser();
    else if (screen_ == Screen::GameSettings)
        draw_settings();
    else if (screen_ == Screen::Mapping)
        draw_mapping(time);
    else
        switch (tab_)
        {
        case Tab::Library:
            draw_library(time);
            if (screen_ == Screen::Sort)
                draw_sort();
            break;
        case Tab::MemoryCards:
            draw_memory_cards(time);
            break;
        case Tab::Settings:
            draw_settings();
            break;
        }
    g.set_layer();
    draw_dialog();
}

/* ---- launch ------------------------------------------------------------------------------- */

void App::begin_launch(Game *game)
{
    launch_ = game;
    launch_status_ = "Loading Dolphin";
    launch_progress_ = -1;
    launch_start_ = time_;
    if (game)
        lib_->mark_played(*game);
}

void App::set_launch_status(const std::string &status, float progress)
{
    launch_status_ = status;
    launch_progress_ = progress;
}

void App::draw_launch(double time)
{
    Gfx &g = *g_;
    g.background();
    draw_brand(kBarCy);
    g.text_mid(Font::SemiBold, ts(27), 1866, kBarCy, rgba(0x9FD8FF), Align::Right, tr("Launching"));

    /* The tile: the game's cover when there is one, else the Porpoise mark. */
    const float size = 330, cx = 960, cy = 420;
    Texture *cover = launch_ ? cover_of(*launch_) : nullptr;
    Glass face;
    face.tint = rgba(0x1E50D8, 0.80f);
    face.rim = rgba(0x9FDFFF);
    face.radius = kR;
    face.rim_w = 3.0f;
    face.glow = 22;
    face.phase = 0.5f;
    glass_block(g, cx, cy, size, size, 40.0f, 0, 0, 66, face);
    if (cover)
    {
        const float ch = size - 36, cw = ch * float(cover->width) / float(cover->height);
        g.image(cover, cx - cw * 0.5f, cy - ch * 0.5f, cw, ch, kWhite, kR * 0.6f);
        Corner gc[4];
        rect_at(cx, cy, cw * 0.5f, ch * 0.5f, 0, 0, 0, gc);
        Glass gloss;
        gloss.face = 2;
        gloss.radius = kR * 0.6f;
        gloss.rim_w = 0;
        gloss.tint = kWhite;
        gloss.rim = kClear;
        gloss.phase = 0.5f;
        g.glass(gc, cw, ch, 0, gloss);
    }
    else
        draw_mark(cx, cy, 250, rgba(0x6EDCFF));
    /* Its reflection on the floor. */
    g.blob(cx, cy + size * 0.5f + 40, size * 1.4f, 120, rgba(0x2F7BFF, 0.35f));

    g.text(Font::SemiBold, ts(52), 960, 690, kWhite, Align::Center,
           launch_ ? trf("Starting {game}\xE2\x80\xA6", {{"game", launch_->title}}) : tr("Starting game\xE2\x80\xA6"));

    /* Progress: measured when known, otherwise a moving segment. */
    const float bx = 591, by = 800, bw = 738, bh = 24;
    g.panel(bx, by, bw, bh, rgba(0x08113A, 0.9f), 1, kR, rgba(0x3B5BB5, 0.95f), 2.0f);
    if (launch_progress_ >= 0)
        g.panel(bx + 3, by + 3, std::max(18.0f, (bw - 6) * std::min(launch_progress_, 1.0f)), bh - 6,
                rgba(0x5AD8FF), 0.85f, kR, rgba(0xBDF1FF, 0.6f), 0, 10);
    else
    {
        const float t = float(std::fmod(time - launch_start_, 1.6) / 1.6);
        const float seg = 220, sx = bx + 3 + (bw - 6 - seg) * (0.5f - 0.5f * std::cos(t * 2 * kPi));
        g.panel(sx, by + 3, seg, bh - 6, rgba(0x5AD8FF), 0.85f, kR, rgba(0xBDF1FF, 0.6f), 0, 10);
    }
    g.text(Font::SemiBold, ts(28), 960, 852, rgba(0xB6BDE8), Align::Center, tr(launch_status_));
}
} // namespace porpoise::ui
