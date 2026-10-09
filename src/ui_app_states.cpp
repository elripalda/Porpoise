/* Porpoise UI - a game's save states, from its Details page: the three slots
 * with what each one saw. Cross starts the game from a slot; Square deletes.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <ctime>

#include "porpoise_pad.hpp"
#include "porpoise_states.hpp"
#include "ui_app.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

namespace
{
std::string when(long long t)
{
    if (t <= 0)
        return "";
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    localtime_r(&tt, &tm);
    char buf[48];
    std::strftime(buf, sizeof buf, "%Y-%m-%d  %H:%M", &tm);
    return buf;
}
} // namespace

void App::count_states(const Game *game)
{
    details_states_ = 0;
    if (!game)
        return;
    const std::string key = Library::key_of(*game);
    for (int i = 0; i < porpoise::states::kSlots; ++i)
        if (porpoise::states::slot(key, i).exists)
            ++details_states_;
}

void App::open_states(Game *game)
{
    states_game_ = game;
    load_slots(game);
    /* Focus the newest slot. */
    states_sel_ = 0;
    long long newest = -1;
    for (int i = 0; i < porpoise::states::kSlots; ++i)
        if (menu_slot_used_[i] && menu_slot_time_[i] > newest)
        {
            newest = menu_slot_time_[i];
            states_sel_ = i;
        }
    screen_ = Screen::States;
    sfx(Sound::DetailsFlip);
}

App::Action App::update_states(bool left, bool right)
{
    if (left && states_sel_ > 0)
    {
        --states_sel_;
        sfx(Sound::MenuScroll);
    }
    if (right && states_sel_ + 1 < porpoise::states::kSlots)
    {
        ++states_sel_;
        sfx(Sound::MenuScroll);
    }
    if (pressed(BtnCircle))
    {
        menu_free_slots();
        count_states(states_game_);
        screen_ = Screen::Details;
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    if (!states_game_)
        return Action::None;
    const bool used = menu_slot_used_[states_sel_];
    if (pressed(BtnCross) && used)
    {
        const std::string state = porpoise::states::slot(Library::key_of(*states_game_), states_sel_).state_path;
        Game *game = states_game_;
        menu_free_slots();
        screen_ = Screen::Details;
        sfx(Sound::LaunchGame);
        return start_game(game, state);
    }
    if (pressed(BtnSquare) && used)
        open_dialog(DialogKind::DeleteState, tr("Delete this save state?"),
                    trf("Slot {n} of {game} will be deleted. Your memory card saves are not touched.",
                        {{"n", std::to_string(states_sel_ + 1)}, {"game", states_game_->title}}),
                    tr("Delete"), true);
    return Action::None;
}

void App::draw_states()
{
    Gfx &g = *g_;
    if (!states_game_)
        return;
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.78f), 1, 0);
    const float pw = 1640, ph = 620, px = (1920 - pw) * 0.5f, py = 210;
    Glass face;
    face.tint = rgba(0x13308A, 0.9f);
    face.rim = rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.2f;
    face.glow = 12;
    face.phase = 0.3f;
    glass_block(g, px + pw * 0.5f, py + ph * 0.5f, pw, ph, 22, 0, 0, 36, face);
    g.text_mid(Font::Bold, ts(44), px + 60, py + 66, kWhite, Align::Left, tr("Save States"));
    g.text_mid(Font::Regular, ts(26), px + 60, py + 112, kLavender, Align::Left,
               fit(g, Font::Regular, ts(26), states_game_->title, pw - 120));

    const float cw = 480, ch = 330, gap = 40, cx0 = px + (pw - (cw * 3 + gap * 2)) * 0.5f, cy = py + 160;
    for (int i = 0; i < porpoise::states::kSlots; ++i)
    {
        const float cx = cx0 + float(i) * (cw + gap);
        const bool on = i == states_sel_;
        g.panel(cx - 6, cy - 6, cw + 12, ch + 12, rgba(0x07102E, 0.65f), 1, kR,
                on ? kIcy : rgba(0x5A68A8, 0.8f), on ? 3.0f : 1.4f, on ? 14 : 0, on ? 0.12f : 0.0f);
        const float ih_max = ch - 70;
        if (menu_slot_tex_[i])
        {
            const float a = float(menu_slot_tex_[i]->width) / float(std::max(1, menu_slot_tex_[i]->height));
            float iw = cw, ih = cw / a;
            if (ih > ih_max)
            {
                ih = ih_max;
                iw = ih * a;
            }
            g.image(menu_slot_tex_[i], cx + (cw - iw) * 0.5f, cy + (ih_max - ih) * 0.5f, iw, ih,
                    on ? kWhite : rgba(0xFFFFFF, 0.8f), kR * 0.6f);
        }
        else
            g.text_mid(Font::SemiBold, ts(28), cx + cw * 0.5f, cy + ih_max * 0.5f, with_alpha(kLavender, 0.8f),
                       Align::Center, menu_slot_used_[i] ? tr("Saved") : tr("Empty"));
        g.text_mid(Font::Bold, ts(28), cx + 20, cy + ch - 34, on ? kWhite : kSoft, Align::Left,
                   trf("Slot {n}", {{"n", std::to_string(i + 1)}}));
        if (menu_slot_used_[i])
            g.text_mid(Font::Regular, ts(22), cx + cw - 16, cy + ch - 34, kLavender, Align::Right,
                       when(menu_slot_time_[i]));
    }
    g.text_mid(Font::Regular, ts(23), 960, py + ph - 40, kLavender, Align::Center,
               tr("Save a state from the in-game menu (OPTIONS + touch pad) while you play."));
    if (menu_slot_used_[states_sel_])
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Play from Here"}, {Glyph::Circle, "Back"}},
                     {{Glyph::Square, "Delete"}}, "");
    else
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Circle, "Back"}}, {}, "");
}
} // namespace porpoise::ui
