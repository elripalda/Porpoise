/* Porpoise UI - the in-game menu: Options + touch pad pauses the game and
 * slides this in from the left, in the launcher's own glass.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <sys/stat.h>

#include "porpoise_pad.hpp"
#include "ui_app.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

namespace
{
enum Row
{
    kResume,
    kResolution,
    kWidescreen,
    kFps,
    kUpscale,
    kVolume,
    kLibrary,
    kHome,
    kRowCount,
};

const char *const kResolutions[] = {"1x (480p)",           "2x (720p)", "3x (1080p)", "4x (1440p) \xE2\x80\xA2 exp.",
                                    "5x (1800p) \xE2\x80\xA2 exp.", "6x (4K) \xE2\x80\xA2 exp."};
} // namespace

void App::open_game_menu(Game *game, Settings *play)
{
    menu_game_ = game;
    menu_play_ = play;
    menu_row_ = kResume;
    menu_anim_ = 0;
    menu_closing_ = false;
    menu_answer_ = 0;
    prev_ = held_ = 0xFFFFFFFFu; /* buttons still down from the shortcut don't count */
    sfx(Sound::DetailsFlip);
}

int App::update_game_menu(const Input &in, double dt)
{
    prev_ = held_;
    held_ = in.held;
    if (in.stick_y < -0.55f) held_ |= BtnUp;
    if (in.stick_y > 0.55f) held_ |= BtnDown;
    if (in.stick_x < -0.55f) held_ |= BtnLeft;
    if (in.stick_x > 0.55f) held_ |= BtnRight;
    const bool calm = settings_->reduced_motion;
    if (menu_closing_)
    {
        menu_anim_ = calm ? 0.0f : std::max(0.0f, menu_anim_ - float(dt) * 6.0f);
        return menu_anim_ <= 0.0f ? menu_answer_ : 0;
    }
    menu_anim_ = calm ? 1.0f : std::min(1.0f, menu_anim_ + float(dt) * 5.0f);
    if (!menu_play_)
        return 1;

    const bool up = nav(BtnUp, rep_up_, dt), down = nav(BtnDown, rep_down_, dt);
    const bool left = nav(BtnLeft, rep_left_, dt), right = nav(BtnRight, rep_right_, dt);
    if (up)
    {
        menu_row_ = (menu_row_ + kRowCount - 1) % kRowCount;
        sfx(Sound::MenuScroll);
    }
    if (down)
    {
        menu_row_ = (menu_row_ + 1) % kRowCount;
        sfx(Sound::MenuScroll);
    }

    auto close = [&](int answer) {
        sfx(answer == 1 ? Sound::DetailsFlip : Sound::MovingTab);
        menu_closing_ = true;
        menu_answer_ = answer;
        if (calm)
            menu_anim_ = 0;
        return calm ? answer : 0;
    };
    const bool combo = (held_ & (BtnOptions | BtnTouch)) == (BtnOptions | BtnTouch) &&
                       (prev_ & (BtnOptions | BtnTouch)) != (BtnOptions | BtnTouch);
    if (pressed(BtnCircle) || combo)
        return close(1);

    Settings &p = *menu_play_;
    std::string key;
    const int dir = left ? -1 : (right || pressed(BtnCross)) ? 1 : 0;
    switch (menu_row_)
    {
    case kResume:
        if (pressed(BtnCross))
            return close(1);
        break;
    case kLibrary:
        if (pressed(BtnCross))
            return close(2);
        break;
    case kHome:
        if (pressed(BtnCross))
            return close(3);
        break;
    case kResolution:
        if (dir)
        {
            const int v = std::clamp(p.resolution + dir, 1, 6);
            if (v != p.resolution)
            {
                p.resolution = v;
                key = "resolution";
            }
        }
        break;
    case kWidescreen:
        if (dir)
        {
            p.widescreen = !p.widescreen;
            key = "widescreen";
        }
        break;
    case kFps:
        if (dir)
        {
            p.fps_overlay = !p.fps_overlay;
            key = "fps_overlay";
        }
        break;
    case kUpscale:
        if (dir)
        {
            p.sharp = !p.sharp;
            key = "sharp";
        }
        break;
    case kVolume:
        if (dir)
        {
            const int v = std::clamp(p.volume + dir, 0, 10);
            if (v != p.volume)
            {
                p.volume = v;
                key = "volume";
            }
        }
        break;
    default:
        break;
    }
    if (!key.empty() && menu_game_)
    {
        /* Saved as this game's own setting, so it sticks next time. */
        std::vector<std::string> keys = Settings::keys_in(game_settings_path(*menu_game_));
        if (std::find(keys.begin(), keys.end(), key) == keys.end())
            keys.push_back(key);
        mkdir((data_dir_ + "/game-settings").c_str(), 0777);
        p.save_keys(game_settings_path(*menu_game_), keys);
        menu_change_ = key;
        sfx(Sound::MenuScroll);
    }
    return 0;
}

void App::draw_game_menu(double time)
{
    if (!menu_play_)
        return;
    Gfx &g = *g_;
    const float t = ease_out(menu_anim_);
    const Settings &p = *menu_play_;
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.55f * t), 1, 0);

    /* The panel slides in from the left. */
    const float w = 660, x = 40, y = 40, h = 1000;
    g.set_layer(-(w + 80) * (1.0f - t), 0, 0.3f + 0.7f * t);
    Glass face;
    face.tint = rgba(0x13308A, 0.88f);
    face.rim = rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.2f;
    face.glow = 12;
    face.phase = 0.2f;
    glass_block(g, x + w * 0.5f, y + h * 0.5f, w, h, 22, 0, 0, 36, face);

    /* Header: the game's cover and name. */
    float hy = y + 40;
    if (menu_game_)
    {
        Texture *cover = cover_of(*menu_game_);
        const float cw = 100, ch = 140, cx = x + 40, cy = hy;
        if (cover)
        {
            float uv[4];
            cover_uv(cover, cw, ch, uv);
            Corner c[4] = {{cx, cy, 1}, {cx + cw, cy, 1}, {cx + cw, cy + ch, 1}, {cx, cy + ch, 1}};
            g.quad3d(cover, c, cw, ch, kWhite, kR * 0.6f, false, false, uv);
            gloss_over(g, c, cw, ch, kR * 0.6f, 0.2f);
        }
        else
            g.panel(cx, cy, cw, ch, kTileFill, 0.6f, kR * 0.6f, kEdge, 1.6f);
        g.text_mid(Font::SemiBold, ts(22), cx + cw + 28, cy + 28, kCyan, Align::Left, tr("PAUSED"), 3.0f);
        const auto lines = wrap(g, Font::Bold, ts(34), menu_game_->title, w - cw - 110, 2);
        float ly = cy + 72;
        for (const std::string &l : lines)
        {
            g.text_mid(Font::Bold, ts(34), cx + cw + 28, ly, kWhite, Align::Left, l);
            ly += 42;
        }
        hy = cy + ch + 34;
    }
    g.panel(x + 36, hy, w - 72, 1.5f, rgba(0x6F8FE0, 0.5f), 1, 0);
    hy += 18;

    /* Rows. */
    static const char *const labels[kRowCount] = {"Resume", "Internal resolution", "Widescreen hack", "FPS counter",
                                                  "Upscaling", "Volume", "Quit to library", "Close Porpoise"};
    const float row_h = 70, rx = x + 24, rw = w - 48;
    for (int i = 0; i < kRowCount; ++i)
    {
        const float ry = hy + float(i) * row_h + (i >= kLibrary ? 18.0f : 0.0f);
        const float cy = ry + row_h * 0.5f;
        const bool on = i == menu_row_;
        if (i == kLibrary)
            g.panel(x + 36, ry - 12, w - 72, 1.5f, rgba(0x6F8FE0, 0.5f), 1, 0);
        if (on)
            g.panel(rx, ry + 5, rw, row_h - 10, rgba(0x1D45B8, 0.9f), 0.7f, kR, kIcy, 2.2f, 8, 0.18f);
        g.text_mid(on ? Font::Bold : Font::SemiBold, ts(29), rx + 28, cy, on ? kWhite : kSoft, Align::Left, tr(labels[i]));
        std::string value;
        switch (i)
        {
        case kResolution: value = tr(kResolutions[std::clamp(p.resolution, 1, 6) - 1]); break;
        case kWidescreen: value = tr(p.widescreen ? "On" : "Off"); break;
        case kFps: value = tr(p.fps_overlay ? "On" : "Off"); break;
        case kUpscale: value = tr(p.sharp ? "Sharp" : "Smooth"); break;
        case kVolume: value = std::to_string(p.volume * 10) + "%"; break;
        default: break;
        }
        if (!value.empty())
        {
            const float right = rx + rw - 24;
            g.text_mid(Font::Bold, ts(26), right - (on ? 30 : 0), cy, on ? kWhite : kSoft, Align::Right, value);
            if (on)
            {
                const float vw = g.measure(Font::Bold, ts(26), value);
                g.glyph(Glyph::Arrow, right - 30 - vw - 22, cy, 16, kCyan, -kPi * 0.5f);
                g.glyph(Glyph::Arrow, right - 8, cy, 16, kCyan, kPi * 0.5f);
            }
        }
        else if (on)
            g.glyph(Glyph::Arrow, rx + rw - 30, cy, 20, kWhite, kPi * 0.5f);
    }
    const float note_y = hy + float(kRowCount) * row_h + 18 + 34;
    const std::string note = menu_row_ == kResolution && p.resolution >= 4
                                 ? tr("Experimental: may slow some games down.")
                                 : tr("Changes here are saved for this game.");
    for (const std::string &l : wrap(g, Font::Regular, ts(23), note, w - 96, 2))
    {
        g.text_mid(Font::Regular, ts(23), x + 48, std::min(note_y, y + h - 128.0f), kLavender, Align::Left, l);
        break;
    }
    (void)time;
    /* The prompts, smaller, inside the panel. */
    {
        float px = x + 52;
        const float py = y + h - 58, size = ts(23);
        const std::pair<Glyph, const char *> prompts[2] = {{Glyph::Cross, "Select"}, {Glyph::Circle, "Resume"}};
        for (int i = 0; i < 2; ++i)
        {
            if (i > 0)
            {
                g.panel(px - 2, py - 16, 1.5f, 32, rgba(0x6F8FE0, 0.6f), 1, 0);
                px += 26;
            }
            g.glyph(prompts[i].first, px + 15, py, 32, kWhite);
            px += 42;
            px += g.text_mid(Font::SemiBold, size, px, py, kWhite, Align::Left, tr(prompts[i].second)) + 30;
        }
    }
    g.set_layer();
}

void App::return_from_game()
{
    menu_game_ = nullptr;
    menu_play_ = nullptr;
    launch_ = nullptr;
    screen_ = Screen::Main;
    tab_ = Tab::Library;
    screen_anim_ = 1.0f;
    held_ = prev_ = 0xFFFFFFFFu; /* the button that quit doesn't press anything here */
    cards_scanned_ = false;
    lift_ = 0.4f;
}
} // namespace porpoise::ui
