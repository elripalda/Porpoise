/* Porpoise UI - the Revolution look's home screen: a bright grid of game
 * tiles, twelve to a page, pointed at by moving the controller (arrows at
 * the sides turn the page). The bar below keeps the time and
 * the date, with round buttons for Settings and the memory cards. The D-pad
 * works as well as pointing does.
 *
 * An original design for Porpoise, in the spirit of a living-room console's
 * home screen of the mid-2000s: no other product's art, names or sounds.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cmath>
#include <ctime>

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
constexpr int kCols = 4, kRows = 3, kPer = kCols * kRows;
constexpr float kHTileW = 384, kHTileH = 202, kGapX = 32, kGapY = 24;
constexpr float kGridX = (1920 - (kCols * kHTileW + (kCols - 1) * kGapX)) * 0.5f, kGridY = 126;
/* Focus targets besides the tiles (0..11); the tabs are kRevTab0.. (ui_app.hpp). */
constexpr int kHomeSettings = 12, kHomeCards = 13, kHomePrev = 14, kHomeNext = 15;
constexpr float kBtnSize = 140, kBtnY = 950, kBtnX[2] = {160, 1760};
constexpr float kArrowSize = 84, kArrowY = kGridY + (kRows * kHTileH + (kRows - 1) * kGapY) * 0.5f;
constexpr float kArrowX[2] = {62, 1858};

/* The palette (look::rev, ui_app_common.hpp). */
const Color kTile = rev::kTile;
const Color kRim = rev::kRim;
const Color kHover = rev::kBlue;
const Color kInk = rev::kInk;
const Color kInkSoft = rev::kInkSoft;
const Color kBar = rev::kBar;
const Color kAccent = rev::kBlue;
const Color kGameCube = rev::kGameCube;
const Color kWii = rev::kWii;

bool inside(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

bool in_circle(float px, float py, float cx, float cy, float r)
{
    return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r;
}
} // namespace

std::vector<App::HomeItem> App::home_items() const
{
    std::vector<HomeItem> items;
    const int shown = lib_->shown();
    for (int i = 0; i < shown; ++i)
        items.push_back({i, false});
    return items;
}

/* The home screen points with the controller: the pad keeps the pointer while
 * it shows (and lets go of it otherwise, unless the Wii Remote setup has it). */
void App::home_pad_sync()
{
    const bool want = pointer_screen() && settings_->ui_pointer;
    const bool has = porpoise::pad::wii().menu;
    if (want && !has)
    {
        porpoise::pad::WiiConfig c = settings_->wii_config(false);
        c.active = false;
        c.controller = porpoise::pad::WiiRemote;
        c.pointer = porpoise::pad::PointerGyro;
        c.motion = c.shake = false;
        c.menu = true;
        porpoise::pad::set_wii(c);
    }
    else if (!want && has && screen_ != Screen::WiiSetup)
        porpoise::pad::set_wii(porpoise::pad::WiiConfig{});
    if (home_buzz_until_ > 0 && time_ > home_buzz_until_)
    {
        porpoise::pad::set_rumble(0, false, 0);
        home_buzz_until_ = -1;
    }
}

void App::home_tile_rect(int slot, float scroll_dx, float &x, float &y, float &w, float &h) const
{
    const int c = slot % kCols, r = slot / kCols;
    const float grow = 1.0f + 0.07f * home_grow_[slot];
    w = kHTileW * grow;
    h = kHTileH * grow;
    x = kGridX + c * (kHTileW + kGapX) + kHTileW * 0.5f - w * 0.5f + scroll_dx;
    y = kGridY + r * (kHTileH + kGapY) + kHTileH * 0.5f - h * 0.5f;
}

/* What the pointer is over: a tile (only ones with a game), a button, -1. */
int App::home_hit(float px, float py, int items, int pages) const
{
    for (int slot = 0; slot < kPer; ++slot)
    {
        if (home_page_ * kPer + slot >= items)
            break;
        float x, y, w, h;
        home_tile_rect(slot, 0, x, y, w, h);
        if (inside(px, py, x, y, w, h))
            return slot;
    }
    if (in_circle(px, py, kBtnX[0], kBtnY, kBtnSize * 0.5f + 6))
        return kHomeSettings;
    if (in_circle(px, py, kBtnX[1], kBtnY, kBtnSize * 0.5f + 6))
        return kHomeCards;
    if (home_page_ > 0 && in_circle(px, py, kArrowX[0], kArrowY, kArrowSize * 0.5f + 10))
        return kHomePrev;
    if (home_page_ + 1 < pages && in_circle(px, py, kArrowX[1], kArrowY, kArrowSize * 0.5f + 10))
        return kHomeNext;
    return rev_tab_hit(px, py);
}

void App::update_home(bool left, bool right, bool up, bool down, bool &play, bool &details, bool &fav, double dt)
{
    const std::vector<HomeItem> items = home_items();
    const int n = int(items.size());
    const int pages = std::max(1, (n + kPer - 1) / kPer);
    home_page_ = std::clamp(home_page_, 0, pages - 1);
    auto on_page = [&](int page) { return std::clamp(n - page * kPer, 0, kPer); };
    auto turn_page = [&](int to) {
        if (to < 0 || to >= pages || to == home_page_)
            return false;
        home_page_ = to;
        sfx(Sound::MovingTab);
        return true;
    };

    /* First time here: the selected game's tile (the library's selection). */
    if (!home_synced_)
    {
        home_synced_ = true;
        for (int i = 0; i < n; ++i)
            if (items[std::size_t(i)].game == selected_)
            {
                home_page_ = i / kPer;
                home_focus_ = i % kPer;
                home_scroll_ = float(home_page_);
            }
    }

    rev_pointer_step();

    const int before = home_focus_, page_before = home_page_;
    if (left || right || up || down)
    {
        rev_pointer_rest();
        auto clamp_slot = [&](int slot) {
            const int k = on_page(home_page_);
            return k == 0 ? kHomeSettings : std::min(slot, k - 1);
        };
        int f = home_focus_;
        if (f < 0 || f == kHomePrev || f == kHomeNext || f >= kRevTab0)
            f = clamp_slot(0);
        else if (f < kPer)
        {
            const int c = f % kCols, r = f / kCols;
            if (left)
            {
                if (c > 0)
                    f = f - 1;
                else if (turn_page(home_page_ - 1))
                    f = clamp_slot(r * kCols + kCols - 1);
            }
            else if (right)
            {
                if (c + 1 < kCols && home_page_ * kPer + f + 1 < n)
                    f = f + 1;
                else if (turn_page(home_page_ + 1))
                    f = clamp_slot(r * kCols);
            }
            else if (up && r > 0)
                f -= kCols;
            else if (down)
                f = r + 1 < kRows && home_page_ * kPer + f + kCols < n ? f + kCols
                                                                       : (c < kCols / 2 ? kHomeSettings : kHomeCards);
        }
        else if (f == kHomeSettings)
        {
            if (right)
                f = kHomeCards;
            else if (up)
                f = on_page(home_page_) ? clamp_slot((kRows - 1) * kCols) : f;
        }
        else if (f == kHomeCards)
        {
            if (left)
                f = kHomeSettings;
            else if (up)
                f = on_page(home_page_) ? clamp_slot(kRows * kCols - 1) : f;
        }
        home_focus_ = f;
    }
    else if (home_pointing_ && screen_ == Screen::Main)
        home_focus_ = home_hit(home_px_, home_py_, n, pages);

    /* L2 / R2 turn the page too. */
    if (pressed(BtnL2) && turn_page(home_page_ - 1) && !home_pointing_)
        home_focus_ = std::min(home_focus_, on_page(home_page_) - 1);
    if (pressed(BtnR2) && turn_page(home_page_ + 1) && !home_pointing_ && home_focus_ < kPer)
        home_focus_ = std::min(std::max(home_focus_, 0), on_page(home_page_) - 1);
    if (pressed(BtnR3))
    {
        porpoise::pad::centre_now(0);
        flash_note(tr("Pointer centered: it points where the controller points now."));
        sfx(Sound::MenuScroll);
    }

    if ((home_focus_ != before || home_page_ != page_before) && home_focus_ >= 0)
        rev_landed();

    /* The focused tile is the library's selection, for Details and the rest. */
    const int item = home_focus_ >= 0 && home_focus_ < kPer ? home_page_ * kPer + home_focus_ : -1;
    if (item >= 0 && item < n)
    {
        selected_ = items[std::size_t(item)].game;
        /* Cross opens the tile (Start is there); Square plays straight away. */
        details = pressed(BtnCross) != 0;
        play = pressed(BtnSquare) != 0;
        fav = pressed(BtnOptions) != 0;
    }
    else if (pressed(BtnCross))
    {
        if (home_focus_ >= kRevTab0 && home_focus_ < kRevTab0 + 3)
            set_tab(home_focus_ - kRevTab0, +1);
        else if (home_focus_ == kHomeSettings)
            set_tab(int(Tab::Settings), +1);
        else if (home_focus_ == kHomeCards)
            set_tab(int(Tab::MemoryCards), +1);
        else if (home_focus_ == kHomePrev)
            turn_page(home_page_ - 1);
        else if (home_focus_ == kHomeNext)
            turn_page(home_page_ + 1);
    }

    const bool calm = settings_->reduced_motion;
    for (int i = 0; i < kHomeGrow; ++i)
    {
        const float target = home_focus_ == i ? 1.0f : 0.0f;
        home_grow_[i] = calm ? target : smooth(home_grow_[i], target, dt, 14.0f);
    }
    home_scroll_ = calm ? float(home_page_) : smooth(home_scroll_, float(home_page_), dt, 9.0f);
    if (std::fabs(home_scroll_ - float(home_page_)) < 0.001f)
        home_scroll_ = float(home_page_);
}

void App::draw_home(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    const std::vector<HomeItem> items = home_items();
    const int n = int(items.size());
    const int pages = std::max(1, (n + kPer - 1) / kPer);
    const bool calm = settings_->reduced_motion;

    /* Tiles: the page in view, and its neighbour while a page turns. */
    const int p0 = int(std::floor(home_scroll_)), p1 = int(std::ceil(home_scroll_));
    for (int page = p0; page <= p1; ++page)
    {
        if (page < 0 || page >= pages)
            continue;
        const float dx = (float(page) - home_scroll_) * 1920.0f;
        for (int slot = 0; slot < kPer; ++slot)
        {
            const bool here = page == home_page_;
            const int i = page * kPer + slot;
            float x, y, w, h;
            if (here)
                home_tile_rect(slot, dx, x, y, w, h);
            else
            {
                w = kHTileW;
                h = kHTileH;
                x = kGridX + (slot % kCols) * (kHTileW + kGapX) + dx;
                y = kGridY + (slot / kCols) * (kHTileH + kGapY);
            }
            if (x + w < -40 || x > 1960)
                continue;
            if (i >= n)
            {
                /* An empty place: a pale tile with fine lines in it. */
                g.panel(x, y, w, h, rgba(0xF7F8FA, 0.85f), 1, rev::kTileR, rgba(0xD3D9E0), 2.5f);
                draw_lines_in(x, y, w, h, 12);
                continue;
            }
            const float grow = here ? home_grow_[slot] : 0.0f;
            const HomeItem &it = items[std::size_t(i)];
            Game &game = games[std::size_t(it.game)];
            const bool wii = game.platform == "Wii";
            g.blob(x + w * 0.5f, y + h + 4, w * 0.92f, 34 + grow * 14, rgba(0x2A3442, 0.10f + grow * 0.06f));
            g.panel(x, y, w, h, kTile, 0.95f, rev::kTileR, grow > 0.02f ? with_alpha(kHover, 0.4f + 0.6f * grow) : kRim,
                    3.0f + grow * 2.0f, grow * 14.0f, 0.35f);
            const float pad = 12 * (w / kHTileW);
            /* The disc's own tile, when Porpoise has read it: it fills the tile. */
            if (!draw_banner(game, x + pad * 0.5f, y + pad * 0.5f, w - pad, h - pad, time, 1.0f, false))
            {
                /* Else the cover, upright at the left, and its name. */
                const float ch = h - pad * 2;
                float cw = ch * 5.0f / 7.0f;
                Texture *cover = cover_of(game);
                if (cover && cover->height > 0)
                    cw = std::min(ch * float(cover->width) / float(cover->height), w * 0.42f);
                if (cover)
                    g.image(cover, x + pad, y + pad, cw, ch, {}, 14);
                else if (cover_loading(game))
                    g.panel(x + pad, y + pad, cw, ch, with_alpha(wii ? kWii : kGameCube, 0.10f), 1, 14);
                else
                {
                    g.panel(x + pad, y + pad, cw, ch, with_alpha(wii ? kWii : kGameCube, 0.16f), 1, 14,
                            with_alpha(wii ? kWii : kGameCube, 0.5f), 2);
                    g.text_mid(Font::Bold, ts(20), x + pad + cw * 0.5f, y + pad + ch * 0.5f, wii ? kWii : kGameCube,
                               Align::Center, wii ? "Wii" : "GC");
                }
                const float tx = x + pad * 2 + cw, tw = x + w - pad - tx;
                float ty = y + pad + 8;
                for (const std::string &l : wrap(g, Font::Bold, ts(25), game.title, tw, 3))
                {
                    g.text(Font::Bold, ts(25), tx, ty, kInk, Align::Left, l);
                    ty += 33;
                }
                const float by = y + h - pad - 32;
                const std::string label = wii ? "Wii" : "GameCube";
                const float lw = g.measure(Font::SemiBold, ts(17), label) + 26;
                g.panel(tx, by + 2, lw, 30, wii ? kWii : kGameCube, 0.9f, 15);
                g.text_mid(Font::SemiBold, ts(17), tx + lw * 0.5f, by + 17, kTile, Align::Center, label);
            }
            if (game.favourite)
                g.glyph(Glyph::Star, x + w - pad - 14, y + pad + 14, 24, rgba(0xF5B82E));
            /* A sheen that crosses the tile under the pointer now and then. */
            if (grow > 0.5f && !calm)
            {
                const float t = std::fmod(float(time) * 0.55f, 2.4f);
                if (t < 1.0f)
                {
                    const float bw = w * 0.16f, bx = x + 10 + (w - 20 - bw) * t;
                    g.panel(bx, y + 8, bw, h - 16, rgba(0xFFFFFF, 0.22f * std::sin(t * kPi)), 1, bw * 0.5f);
                }
            }
        }
    }

    /* Covers away from the pages in view give their memory back. */
    for (int i = 0; i < n; ++i)
    {
        Game &game = games[std::size_t(items[std::size_t(i)].game)];
        if (std::abs(i / kPer - home_page_) > 1 && game.cover)
        {
            g.free_texture(game.cover);
            game.cover = nullptr;
            game.cover_tried = false;
        }
    }

    if (n == 0)
    {
        const bool none = games.empty();
        g.panel(560, 380, 800, 190, rgba(0xFFFFFF, 0.95f), 0.96f, 28, kRim, 3);
        g.text_mid(Font::Bold, ts(34), 960, 440, kInk, Align::Center,
                   none ? tr("No games yet")
                        : lib_->show() == Library::Show::Wii        ? tr("No Wii games here")
                        : lib_->show() == Library::Show::Channels ? tr("No channels here")
                                                                  : tr("No GameCube games here"));
        for (const std::string &l :
             wrap(g, Font::Regular, ts(23),
                  none ? tr("Put them in /data/porpoise/games, or add a folder in Settings, under Games.")
                       : tr("Sort & filter (Triangle) can show all of your games."),
                  720, 2))
        {
            g.text_mid(Font::Regular, ts(23), 960, 500, kInkSoft, Align::Center, l);
            break;
        }
    }

    /* Page arrows, and a dot per page. */
    for (int side = 0; side < 2; ++side)
    {
        const bool show = side == 0 ? home_page_ > 0 : home_page_ + 1 < pages;
        if (!show)
            continue;
        const float grow = home_grow_[side == 0 ? kHomePrev : kHomeNext];
        const float s = kArrowSize * (1.0f + 0.12f * grow);
        g.panel(kArrowX[side] - s * 0.5f, kArrowY - s * 0.5f, s, s, kTile, 0.94f, s * 0.5f,
                grow > 0.02f ? kHover : rev::kBlueSoft, 3.0f, grow * 10.0f, 0.3f);
        g.glyph(Glyph::Arrow, kArrowX[side] + (side == 0 ? -3.0f : 3.0f), kArrowY, 30, kHover,
                side == 0 ? -kPi * 0.5f : kPi * 0.5f);
    }
    if (pages > 1)
    {
        const float dy = 796, gap = 26, x0 = 960 - (pages - 1) * gap * 0.5f;
        for (int p = 0; p < pages; ++p)
        {
            const bool on = p == home_page_;
            const float r = on ? 7.0f : 5.0f;
            g.panel(x0 + p * gap - r, dy - r, r * 2, r * 2, on ? kAccent : rgba(0xB4BCC6), 1, r);
        }
    }

    /* The floor: flat along the bottom, dipping into a wide bowl for the
     * clock, edged in blue. The edge colour goes first, a little larger,
     * then the fill over it. */
    constexpr float kShoulder = 812, kFloor = 904;
    {
        /* The floor's top edge: high at the sides, easing down into a dip
         * for the clock and back up. Drawn as thin slanted strips, filled,
         * with the blue edge along the top. */
        auto edge_y = [&](float x) {
            auto ease = [](float t) {
                t = std::clamp(t, 0.0f, 1.0f);
                return t * t * (3.0f - 2.0f * t);
            };
            if (x < 960)
                return kShoulder + (kFloor - kShoulder) * ease((x - 470.0f) / 200.0f);
            return kShoulder + (kFloor - kShoulder) * ease((1450.0f - x) / 200.0f);
        };
        auto quad = [&](float x0, float y0, float x1, float y1, float b0, float b1, Color c) {
            const Corner q[4] = {{x0, y0, 1}, {x1, y1, 1}, {x1, b1, 1}, {x0, b0, 1}};
            g.quad3d(nullptr, q, x1 - x0, std::max(b0 - y0, b1 - y1), c, 0, false, false);
        };
        const float e = 4;
        float x = 0;
        while (x < 1920)
        {
            const bool sloped = (x >= 460 && x < 680) || (x >= 1240 && x < 1460);
            const float step = sloped ? 5.0f : (x < 460 ? 460.0f - x : x < 1240 ? (x < 680 ? 680.0f - x : 1240.0f - x)
                                                                                 : 1920.0f - x);
            const float x1 = std::min(1920.0f, x + std::max(step, 1.0f));
            const float y0 = edge_y(x), y1 = edge_y(x1);
            quad(x, y0, x1, y1, 1080, 1080, kBar);
            quad(x, y0 - e, x1, y1 - e, y0, y1, rev::kBlue);
            x = x1;
        }
        g.blob(960, kFloor + 46, 1000, 70, rgba(0xFFFFFF, 0.45f));
    }

    /* The time in segments, in the dip; the date under the line. */
    draw_seg_clock(960, 852, 60);
    g.text_mid(Font::Bold, ts(42), 960, 972, rev::kDigits, Align::Center, short_date());

    /* Round buttons: Porpoise (Settings) and the memory cards. */
    for (int b = 0; b < 2; ++b)
    {
        const float grow = home_grow_[b == 0 ? kHomeSettings : kHomeCards];
        const float s = kBtnSize * (1.0f + 0.08f * grow), cx = kBtnX[b];
        g.panel(cx - s * 0.5f - 10, kBtnY - s * 0.5f - 10, s + 20, s + 20, rgba(0xDDE2E8), 1, s * 0.5f + 10);
        g.panel(cx - s * 0.5f, kBtnY - s * 0.5f, s, s, rgba(0xF7F8FA), 0.92f, s * 0.5f, kHover, 4.0f + grow * 1.5f,
                grow * 14.0f, 0.5f);
        const float k = s / kBtnSize;
        if (b == 0)
        {
            if (!draw_mark(cx, kBtnY, 92 * k, rgba(0xFFFFFF)))
                g.text_mid(Font::Bold, ts(26), cx, kBtnY, kInkSoft, Align::Center, "P");
        }
        else
        {
            const Color ink = grow > 0.5f ? kHover : rgba(0x8A939D);
            g.panel(cx - 24 * k, kBtnY - 30 * k, 48 * k, 60 * k, kTile, 1, 8 * k, ink, 3.5f * k);
            g.panel(cx - 14 * k, kBtnY - 19 * k, 28 * k, 19 * k, with_alpha(ink, 0.25f), 1, 3 * k);
            for (int i = 0; i < 4; ++i)
                g.panel(cx - 15 * k + i * 8.6f * k, kBtnY + 13 * k, 4.4f * k, 10 * k, ink, 1, 1.5f * k);
        }
        if (grow > 0.3f && home_pointing_)
        {
            const std::string label = b == 0 ? tr("Settings") : tr("Memory Cards");
            const float lw = g.measure(Font::SemiBold, ts(24), label) + 40;
            const float bx = std::clamp(cx - lw * 0.5f, 16.0f, 1904.0f - lw);
            g.panel(bx, kBtnY - s * 0.5f - 70, lw, 48, rgba(0xFFFFFF, grow), 0.95f, 24, with_alpha(kHover, grow), 2.5f);
            g.text_mid(Font::SemiBold, ts(24), bx + lw * 0.5f, kBtnY - s * 0.5f - 46, with_alpha(kInk, grow),
                       Align::Center, label);
        }
    }

    /* What the buttons do, small, along the floor. */
    auto hint = [&](Glyph gl, const std::string &label_in, float x, bool right_aligned) {
        const std::string label = title_case(label_in);
        const float w = 30 + 8 + g.measure(Font::SemiBold, ts(20), label);
        const float x0 = right_aligned ? x - w : x;
        g.glyph(gl, x0 + 15, 1052, 28, rgba(0x8A939D));
        g.text_mid(Font::SemiBold, ts(20), x0 + 38, 1052, kInkSoft, Align::Left, label);
        return w;
    };
    if (screen_ == Screen::Main)
    {
        float hx = 262;
        hx += hint(Glyph::Cross, tr("Open"), hx, false) + 24;
        hint(Glyph::Square, tr("Play now"), hx, false);
        float rx = 1658;
        rx -= hint(Glyph::TouchPad, settings_->ui_pointer ? tr("Pointer off") : tr("Pointer on"), rx, true) + 24;
        hint(Glyph::Triangle, tr("Sort & filter"), rx, true);
        const std::string note = library_note();
        if (!note.empty())
        {
            const float nw = g.measure(Font::SemiBold, ts(22), note) + 44;
            g.panel(960 - nw * 0.5f, 1004, nw, 46, rgba(0xFFFFFF, 0.95f), 0.96f, 23, kRim, 2);
            g.text_mid(Font::SemiBold, ts(22), 960, 1027, kInk, Align::Center, note);
        }
    }
}
/* ---- a tile opening: it grows to fill the screen ---------------------------------------- */

namespace
{
constexpr float kZoomGrow = 0.55f;  /* opening: the tile grows, then the opened tile shows through */
constexpr float kZoomShrink = 0.35f; /* closing: the opened tile goes white, then shrinks back */

float ease_in_out(float u)
{
    u = std::clamp(u, 0.0f, 1.0f);
    return u < 0.5f ? 4 * u * u * u : 1 - (-2 * u + 2) * (-2 * u + 2) * (-2 * u + 2) * 0.5f;
}
} // namespace

/* Where a game's tile sits on the home screen (its page brought into view). */
bool App::home_rect_of(int game, float rect[4])
{
    const std::vector<HomeItem> items = home_items();
    for (int i = 0; i < int(items.size()); ++i)
        if (items[std::size_t(i)].game == game)
        {
            home_page_ = i / kPer;
            home_scroll_ = float(home_page_);
            home_focus_ = i % kPer;
            home_synced_ = true;
            home_tile_rect(home_focus_, 0, rect[0], rect[1], rect[2], rect[3]);
            return true;
        }
    return false;
}

void App::start_zoom(int dir)
{
    zoom_anim_ = 0;
    if (settings_->reduced_motion || settings_->ui_layout != 0 || !home_rect_of(selected_, zoom_from_))
        return;
    zoom_game_ = selected_;
    zoom_dir_ = dir;
    zoom_anim_ = 1;
}

bool App::zoom_shows_home() const
{
    const float t = 1.0f - zoom_anim_;
    return zoom_dir_ > 0 ? t < kZoomGrow : t >= kZoomShrink;
}

void App::draw_zoom(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    if (zoom_anim_ <= 0 || tab_ != Tab::Library || zoom_game_ < 0 || zoom_game_ >= int(games.size()))
        return;
    const float t = 1.0f - zoom_anim_;
    float u, fade = 1;
    if (zoom_dir_ > 0)
    {
        u = ease_in_out(t / kZoomGrow);
        if (t >= kZoomGrow)
            fade = 1.0f - (t - kZoomGrow) / (1.0f - kZoomGrow);
    }
    else
    {
        u = t < kZoomShrink ? 1.0f : 1.0f - ease_in_out((t - kZoomShrink) / (1.0f - kZoomShrink));
        if (t < kZoomShrink)
            fade = t / kZoomShrink;
    }
    const float to[4] = {-6, -6, 1932, 1092};
    float r[4];
    for (int i = 0; i < 4; ++i)
        r[i] = zoom_from_[i] + (to[i] - zoom_from_[i]) * u;
    const float radius = rev::kTileR * (1.0f - u);
    Game &game = games[std::size_t(zoom_game_)];
    if (u < 0.98f)
        g.blob(r[0] + r[2] * 0.5f, r[1] + r[3] + 6, r[2] * 0.92f, 40, rgba(0x2A3442, 0.14f * fade));
    g.panel(r[0], r[1], r[2], r[3], with_alpha(rev::kTile, fade), 1, radius, with_alpha(rev::kBlue, fade * (1.0f - u)),
            4.0f);
    /* What the tile showed, growing with it and fading as it fills the screen. */
    const float k = fade * (1.0f - 0.85f * u);
    const float pad = 6 + 40 * u;
    const float cx = r[0] + pad, cy = r[1] + pad, cw = r[2] - pad * 2, ch = r[3] - pad * 2;
    if (k > 0.01f && !draw_banner(game, cx, cy, cw, ch, time, k, false))
    {
        Texture *cover = cover_of(game);
        if (cover && cover->height > 0)
        {
            const float h = ch * 0.8f, w = h * float(cover->width) / float(cover->height);
            g.image(cover, r[0] + r[2] * 0.5f - w * 0.5f, cy + ch * 0.1f, w, h, rgba(0xFFFFFF, k), 14 + 10 * u);
        }
        else if (!cover_loading(game))
            g.text_mid(Font::Bold, ts(30 + 30 * u), r[0] + r[2] * 0.5f, r[1] + r[3] * 0.5f, with_alpha(rev::kInk, k),
                       Align::Center, game.title);
    }
}
} // namespace porpoise::ui
