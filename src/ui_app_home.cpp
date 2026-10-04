/* Porpoise UI - the Revolution look's home screen: a bright grid of game
 * tiles, twelve to a page, pointed at by moving the controller. The first
 * tile goes back to the game played last; the bar below keeps the time and
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
constexpr float kHTileW = 384, kHTileH = 216, kGapX = 32, kGapY = 30;
constexpr float kGridX = (1920 - (kCols * kHTileW + (kCols - 1) * kGapX)) * 0.5f, kGridY = 118;
/* Focus targets besides the tiles (0..11). */
constexpr int kHomeSettings = 12, kHomeCards = 13, kHomePrev = 14, kHomeNext = 15;
constexpr float kBtnSize = 132, kBtnY = 1000, kBtnX[2] = {156, 1764};
constexpr float kArrowSize = 84, kArrowY = kGridY + (kRows * kHTileH + (kRows - 1) * kGapY) * 0.5f;
constexpr float kArrowX[2] = {62, 1858};

/* The palette: white glass on a pale room. */
const Color kRoom = rgba(0xF5F7FA);
const Color kTile = rgba(0xFFFFFF);
const Color kRim = rgba(0xC3CAD3);
const Color kHover = rgba(0x35B2E8);
const Color kInk = rgba(0x3B434D);
const Color kInkSoft = rgba(0x7A838E);
const Color kBar = rgba(0xE6EAEF);
const Color kBarEdge = rgba(0xB9C1CB);
const Color kAccent = rgba(0x2FA6DE);
const Color kGameCube = rgba(0x6E63D9);
const Color kWii = rgba(0x39A9DF);

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
    const auto &games = lib_->games();
    const int shown = lib_->shown();
    /* The first tile: the game played most recently, when there is one. */
    int last = -1;
    for (int i = 0; i < shown; ++i)
        if (games[std::size_t(i)].last_played > 0 &&
            (last < 0 || games[std::size_t(i)].last_played > games[std::size_t(last)].last_played))
            last = i;
    if (last >= 0)
        items.push_back({last, true});
    for (int i = 0; i < shown; ++i)
        items.push_back({i, false});
    return items;
}

/* The home screen points with the controller: the pad keeps the pointer while
 * it shows (and lets go of it otherwise, unless the Wii Remote setup has it). */
void App::home_pad_sync()
{
    const bool want = home_showing() && !dialog_.open && !updating();
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
    return -1;
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
        sfx(Sound::HomePage);
        return true;
    };

    /* First time here: the selected game's tile (the library's selection). */
    if (!home_synced_)
    {
        home_synced_ = true;
        for (int i = 0; i < n; ++i)
            if (items[std::size_t(i)].game == selected_ && !items[std::size_t(i)].resume)
            {
                home_page_ = i / kPer;
                home_focus_ = i % kPer;
                home_scroll_ = float(home_page_);
            }
    }

    /* The pointer, when the controller has a middle. */
    const porpoise::pad::Motion m = porpoise::pad::snapshot(0).motion;
    bool aimed = m.valid && m.centred;
    float px = 960 + std::clamp(m.aim_x, -1.1f, 1.1f) * 960;
    float py = 540 + std::clamp(m.aim_y, -1.1f, 1.1f) * 540;
#ifdef PORPOISE_HOST_PREVIEW
    if (preview_px_ >= 0)
    {
        aimed = true;
        px = preview_px_;
        py = preview_py_;
    }
#endif
    if (aimed)
    {
        /* After the D-pad, the pointer leads again once it really moves. */
        if (!home_pointing_ && std::hypot(px - home_hide_x_, py - home_hide_y_) > 70)
            home_pointing_ = true;
        home_px_ = px;
        home_py_ = py;
        home_roll_ = std::clamp(m.roll, -1.2f, 1.2f);
    }
    else
        home_pointing_ = false;

    const int before = home_focus_, page_before = home_page_;
    if (left || right || up || down)
    {
        if (home_pointing_)
        {
            home_pointing_ = false;
            home_hide_x_ = px;
            home_hide_y_ = py;
        }
        auto clamp_slot = [&](int slot) {
            const int k = on_page(home_page_);
            return k == 0 ? kHomeSettings : std::min(slot, k - 1);
        };
        int f = home_focus_;
        if (f < 0 || f == kHomePrev || f == kHomeNext)
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
        flash_note(tr("Pointer centred: it points where the controller points now."));
        sfx(Sound::MenuScroll);
    }

    if ((home_focus_ != before || home_page_ != page_before) && home_focus_ >= 0)
    {
        sfx(Sound::HomeHover);
        if (settings_->rumble && home_pointing_)
        {
            /* A tiny tap in the hand as the pointer lands on something. */
            porpoise::pad::set_rumble(0, false, 0x2600);
            home_buzz_until_ = time_ + 0.035;
        }
    }

    /* The focused tile is the library's selection, for Details and the rest. */
    const int item = home_focus_ >= 0 && home_focus_ < kPer ? home_page_ * kPer + home_focus_ : -1;
    if (item >= 0 && item < n)
    {
        selected_ = items[std::size_t(item)].game;
        play = pressed(BtnCross) != 0;
        details = pressed(BtnSquare) != 0;
        fav = pressed(BtnOptions) != 0;
    }
    else if (pressed(BtnCross))
    {
        if (home_focus_ == kHomeSettings)
            set_tab(int(Tab::Settings), +1);
        else if (home_focus_ == kHomeCards)
            set_tab(int(Tab::MemoryCards), +1);
        else if (home_focus_ == kHomePrev)
            turn_page(home_page_ - 1);
        else if (home_focus_ == kHomeNext)
            turn_page(home_page_ + 1);
    }

    const bool calm = settings_->reduced_motion;
    for (int i = 0; i < 16; ++i)
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
    (void)time;
    auto &games = lib_->games();
    const std::vector<HomeItem> items = home_items();
    const int n = int(items.size());
    const int pages = std::max(1, (n + kPer - 1) / kPer);

    /* The room: pale, a touch darker toward the floor, with two soft lights. */
    g.panel(0, 0, 1920, 1080, kRoom, 0.92f, 0);
    g.blob(360, 140, 1100, 520, rgba(0xFFFFFF, 0.55f));
    g.blob(1600, 760, 1200, 600, rgba(0xDCE9F5, 0.45f));

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
                /* An empty place: a faint outline of a tile. */
                g.panel(x, y, w, h, rgba(0xFFFFFF, 0.38f), 1, 28, rgba(0xD3D9E0, 0.9f), 2);
                continue;
            }
            const float grow = here ? home_grow_[slot] : 0.0f;
            const HomeItem &it = items[std::size_t(i)];
            Game &game = games[std::size_t(it.game)];
            const bool wii = game.platform == "Wii";
            /* A soft shadow, then the white glass. */
            g.blob(x + w * 0.5f, y + h + 4, w * 0.92f, 34 + grow * 14, rgba(0x2A3442, 0.10f + grow * 0.06f));
            g.panel(x, y, w, h, kTile, 0.95f, 28, grow > 0.02f ? with_alpha(kHover, 0.4f + 0.6f * grow) : kRim,
                    3.0f + grow * 2.5f, grow * 14.0f, 0.35f);
            /* The cover, upright at the left. */
            const float pad = 14 * (w / kHTileW);
            const float ch = h - pad * 2;
            float cw = ch * 5.0f / 7.0f;
            Texture *cover = cover_of(game);
            if (cover && cover->height > 0)
                cw = std::min(ch * float(cover->width) / float(cover->height), w * 0.42f);
            if (cover)
                g.image(cover, x + pad, y + pad, cw, ch, {}, 14);
            else
            {
                g.panel(x + pad, y + pad, cw, ch, with_alpha(wii ? kWii : kGameCube, 0.18f), 1, 14,
                        with_alpha(wii ? kWii : kGameCube, 0.5f), 2);
                g.text_mid(Font::Bold, ts(20), x + pad + cw * 0.5f, y + pad + ch * 0.5f,
                           wii ? kWii : kGameCube, Align::Center, wii ? "Wii" : "GC");
            }
            const float tx = x + pad * 2 + cw, tw = x + w - pad - tx;
            float ty = y + pad + 6;
            if (it.resume)
            {
                g.text(Font::Bold, ts(22), tx, ty, kAccent, Align::Left, tr("Continue"), 1.0f);
                ty += 36;
            }
            for (const std::string &l : wrap(g, Font::Bold, ts(25), game.title, tw, it.resume ? 2 : 3))
            {
                g.text(Font::Bold, ts(25), tx, ty, kInk, Align::Left, l);
                ty += 33;
            }
            /* Underneath: when it was played (the first tile), or which console. */
            const float by = y + h - pad - 30;
            if (it.resume)
            {
                g.text_mid(Font::Regular, ts(19), tx, by + 15, kInkSoft, Align::Left,
                           fit(g, Font::Regular, ts(19), relative_time(game.last_played, (long long)std::time(nullptr)),
                               tw - 48));
                /* A round play mark. */
                g.panel(x + w - pad - 40, by - 6, 40, 40, kAccent, 0.85f, 20);
                g.glyph(Glyph::Arrow, x + w - pad - 18, by + 14, 20, kTile, kPi * 0.5f);
            }
            else
            {
                const std::string label = wii ? "Wii" : "GameCube";
                const float lw = g.measure(Font::SemiBold, ts(17), label) + 26;
                g.panel(tx, by, lw, 30, wii ? kWii : kGameCube, 0.9f, 15);
                g.text_mid(Font::SemiBold, ts(17), tx + lw * 0.5f, by + 15, kTile, Align::Center, label);
            }
            if (game.favourite)
                g.glyph(Glyph::Star, x + w - pad - 12, y + pad + 12, 24, rgba(0xF5B82E));
        }
    }

    /* Covers away from the pages in view give their memory back. */
    for (int i = 0; i < n; ++i)
    {
        Game &game = games[std::size_t(items[std::size_t(i)].game)];
        if (std::abs(i / kPer - home_page_) > 1 && game.cover && !items[std::size_t(i)].resume)
        {
            g.free_texture(game.cover);
            game.cover = nullptr;
            game.cover_tried = false;
        }
    }

    if (n == 0)
    {
        const bool none = games.empty();
        g.panel(560, 380, 800, 190, rgba(0xFFFFFF, 0.92f), 0.96f, 28, kRim, 3);
        g.text_mid(Font::Bold, ts(34), 960, 440, kInk, Align::Center,
                   none ? tr("No games yet")
                        : lib_->show() == Library::Show::Wii ? tr("No Wii games here") : tr("No GameCube games here"));
        for (const std::string &l :
             wrap(g, Font::Regular, ts(23),
                  none ? tr("Put them in /data/porpoise/games, or add a folder in Settings, under Games.")
                       : tr("Sort & filter (Triangle) can show all of your games."),
                  720, 2))
        {
            g.text_mid(Font::Regular, ts(23), 960, 500 + 0.0f, kInkSoft, Align::Center, l);
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
                grow > 0.02f ? kHover : kRim, 3.0f, grow * 10.0f, 0.3f);
        g.glyph(Glyph::Arrow, kArrowX[side] + (side == 0 ? -3.0f : 3.0f), kArrowY, 30, grow > 0.5f ? kHover : kInkSoft,
                side == 0 ? -kPi * 0.5f : kPi * 0.5f);
    }
    if (pages > 1)
    {
        const float dy = 862, gap = 26, x0 = 960 - (pages - 1) * gap * 0.5f;
        for (int p = 0; p < pages; ++p)
        {
            const bool on = p == home_page_;
            const float r = on ? 7.0f : 5.0f;
            g.panel(x0 + p * gap - r, dy - r, r * 2, r * 2, on ? kAccent : rgba(0xB4BCC6), 1, r);
        }
    }

    /* The bar: flat along the floor, rising into a gentle dome for the clock.
     * Edge colour first, a little larger, then the fill over it. */
    constexpr float kDomeR = 1821, kDomeTop = 884, kBarTop = 952;
    g.panel(960 - kDomeR - 3, kDomeTop - 3, (kDomeR + 3) * 2, (kDomeR + 3) * 2, kBarEdge, 1, kDomeR + 3);
    g.panel(0, kBarTop - 3, 1920, 1080 - kBarTop + 3, kBarEdge, 1, 0);
    g.panel(960 - kDomeR, kDomeTop, kDomeR * 2, kDomeR * 2, kBar, 1, kDomeR);
    g.panel(0, kBarTop, 1920, 1080 - kBarTop, kBar, 1, 0);
    g.blob(960, kDomeTop + 24, 900, 70, rgba(0xFFFFFF, 0.55f)); /* a sheen along the dome */

    /* The time and the date, in the dome. */
    {
        const std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);
        char hm[16];
        const int h12 = tm.tm_hour % 12 == 0 ? 12 : tm.tm_hour % 12;
        std::snprintf(hm, sizeof hm, "%d:%02d", h12, tm.tm_min);
        const char *ampm = tm.tm_hour < 12 ? "AM" : "PM";
        static const char *const kDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                                             "Saturday"};
        static const char *const kMonths[12] = {"January", "February", "March",     "April",   "May",      "June",
                                                "July",    "August",   "September", "October", "November", "December"};
        const float tw = g.measure(Font::Bold, ts(60), hm), aw = g.measure(Font::SemiBold, ts(26), ampm);
        const float x0 = 960 - (tw + 10 + aw) * 0.5f;
        g.text_mid(Font::Bold, ts(60), x0, 948, rgba(0x5E6772), Align::Left, hm);
        g.text_mid(Font::SemiBold, ts(26), x0 + tw + 10, 960, kInkSoft, Align::Left, ampm);
        const std::string date = tr(kDays[std::clamp(tm.tm_wday, 0, 6)]) + "  " +
                                 tr(kMonths[std::clamp(tm.tm_mon, 0, 11)]) + " " + std::to_string(tm.tm_mday);
        g.text_mid(Font::SemiBold, ts(26), 960, 1018, kInkSoft, Align::Center, date);
    }

    /* Round buttons: Settings (sliders) and the memory cards (a card). */
    for (int b = 0; b < 2; ++b)
    {
        const float grow = home_grow_[b == 0 ? kHomeSettings : kHomeCards];
        const float s = kBtnSize * (1.0f + 0.10f * grow), cx = kBtnX[b];
        g.blob(cx, kBtnY + s * 0.42f, s * 0.9f, 26, rgba(0x2A3442, 0.12f));
        g.panel(cx - s * 0.5f, kBtnY - s * 0.5f, s, s, kTile, 0.93f, s * 0.5f, grow > 0.02f ? kHover : kRim, 3.0f,
                grow * 12.0f, 0.4f);
        const Color ink = grow > 0.5f ? kHover : rgba(0x6B7480);
        const float k = s / kBtnSize;
        if (b == 0)
        {
            /* Three sliders. */
            for (int i = 0; i < 3; ++i)
            {
                const float ly = kBtnY - 20 * k + i * 20 * k, kx = cx + (i == 1 ? 10.0f : i == 0 ? -12.0f : 4.0f) * k;
                g.panel(cx - 26 * k, ly - 2.5f * k, 52 * k, 5 * k, ink, 1, 2.5f * k);
                g.panel(kx - 7 * k, ly - 7 * k, 14 * k, 14 * k, kTile, 1, 7 * k, ink, 3.0f * k);
            }
        }
        else
        {
            /* A memory card: a rounded card with a label and contacts. */
            g.panel(cx - 22 * k, kBtnY - 28 * k, 44 * k, 56 * k, kTile, 1, 7 * k, ink, 3.5f * k);
            g.panel(cx - 13 * k, kBtnY - 18 * k, 26 * k, 18 * k, with_alpha(ink, 0.25f), 1, 3 * k);
            for (int i = 0; i < 4; ++i)
                g.panel(cx - 14 * k + i * 8 * k, kBtnY + 12 * k, 4 * k, 9 * k, ink, 1, 1.5f * k);
        }
        if (grow > 0.3f && home_pointing_)
        {
            /* What it is, in a bubble above. */
            const std::string label = b == 0 ? tr("Settings") : tr("Memory Cards");
            const float lw = g.measure(Font::SemiBold, ts(24), label) + 40;
            const float bx = std::clamp(cx - lw * 0.5f, 16.0f, 1904.0f - lw);
            g.panel(bx, kBtnY - s * 0.5f - 66, lw, 48, rgba(0xFFFFFF, grow), 0.95f, 24, with_alpha(kHover, grow), 2.5f);
            g.text_mid(Font::SemiBold, ts(24), bx + lw * 0.5f, kBtnY - s * 0.5f - 42, with_alpha(kInk, grow),
                       Align::Center, label);
        }
    }

    /* What the buttons do, small, along the bar. */
    auto hint = [&](Glyph gl, const std::string &label, float x, bool right_aligned) {
        const float w = 30 + 8 + g.measure(Font::SemiBold, ts(20), label);
        const float x0 = right_aligned ? x - w : x;
        g.glyph(gl, x0 + 15, 1046, 30, rgba(0x7A838E));
        g.text_mid(Font::SemiBold, ts(20), x0 + 38, 1046, kInkSoft, Align::Left, label);
        return w;
    };
    if (screen_ == Screen::Main)
    {
        float hx = 262;
        hx += hint(Glyph::Cross, tr("Play"), hx, false) + 26;
        hint(Glyph::Square, tr("Details"), hx, false);
        float rx = 1658;
        rx -= hint(Glyph::R3, tr("Centre"), rx, true) + 26;
        hint(Glyph::Triangle, tr("Sort & filter"), rx, true);
        const std::string note = library_note();
        if (!note.empty())
        {
            const float nw = g.measure(Font::SemiBold, ts(22), note) + 44;
            g.panel(960 - nw * 0.5f, 34, nw, 46, rgba(0xFFFFFF, 0.92f), 0.96f, 23, kRim, 2);
            g.text_mid(Font::SemiBold, ts(22), 960, 57, kInk, Align::Center, note);
        }
    }
}

/* An original pointer: a white disc with a dark rim and a blue centre (the
 * spot it points at), and a notch on its rim that turns with the
 * controller's roll. */
void App::draw_home_pointer()
{
    if (!home_pointing_ || screen_ != Screen::Main || dialog_.open)
        return;
    Gfx &g = *g_;
    const float x = home_px_, y = home_py_, r = 23;
    g.blob(x + 6, y + 10, 84, 84, rgba(0x1A2230, 0.22f));
    g.panel(x - r - 3, y - r - 3, (r + 3) * 2, (r + 3) * 2, rgba(0x26303D, 0.92f), 1, r + 3);
    g.panel(x - r, y - r, r * 2, r * 2, rgba(0xFFFFFF), 0.9f, r, {}, 0, 0, 0.5f);
    g.panel(x - 7, y - 7, 14, 14, kAccent, 1, 7);
    const float a = home_roll_, nx = x + std::sin(a) * (r - 6), ny = y - std::cos(a) * (r - 6);
    g.panel(nx - 4, ny - 4, 8, 8, rgba(0x26303D), 1, 4);
}
} // namespace porpoise::ui
