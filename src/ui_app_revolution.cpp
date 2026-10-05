/* Porpoise UI - the Revolution look's shared pieces: the lined room, the top
 * bar with Porpoise's mark and the tabs, the segmented clock, the hand that
 * points, and a game's "opened tile" screen (its Details): a big picture,
 * Start and the controls below it.
 *
 * Original drawings for Porpoise, in the spirit of a living-room console's
 * menus of the mid-2000s; none of another product's art, names or sounds.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <unistd.h>

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
bool inside(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

bool in_circle(float px, float py, float cx, float cy, float r)
{
    return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r;
}

/* The opened tile: its frame, the arrows beside it, the buttons below. */
constexpr float kFrameX = 118, kFrameY = 122, kFrameW = 1684, kFrameH = 688;
constexpr float kBigW = 500, kBigH = 104, kBigY = 878, kBigGap = 64;
constexpr float kChipY = 1022, kChipH = 46;
constexpr float kSideArrow = 84, kSideY = kFrameY + kFrameH * 0.5f, kSideX[2] = {58, 1862};
/* Focus targets on the opened tile. */
constexpr int kRdStart = 0, kRdSecond = 1, kRdChip = 10, kRdPrev = 20, kRdNext = 21;
} // namespace

/* ---- the room, the bar, the clock, the hand ---------------------------------------------- */

void App::draw_room()
{
    Gfx &g = *g_;
    g.panel(0, 0, 1920, 1080, rev::kRoom, 0.93f, 0);
    g.blob(420, 160, 1300, 560, rgba(0xFFFFFF, 0.65f));
    g.blob(1560, 820, 1300, 640, rgba(0xDDE8F3, 0.45f));
    /* Fine lines across it, like a screen's. */
    for (float y = 2; y < 1080; y += 6)
        g.panel(0, y, 1920, 2, with_alpha(rev::kLine, 0.32f), 1, 0);
}

/* Faint lines inside a shape (an empty tile). */
void App::draw_lines_in(float x, float y, float w, float h, float inset)
{
    Gfx &g = *g_;
    for (float ly = y + inset; ly < y + h - inset; ly += 5)
        g.panel(x + inset, ly, w - inset * 2, 1.6f, with_alpha(rev::kLine, 0.55f), 1, 0);
}

void App::draw_rev_top_bar(bool clock)
{
    Gfx &g = *g_;
    /* Porpoise's mark, the wordmark in slate. */
    {
        const bool was = g.toned();
        g.set_tone(true);
        draw_brand(kBarCy);
        g.set_tone(was);
    }
    const std::string names[3] = {tr("Library"), tr("Memory Cards"), tr("Settings")};
    float widths[3], bar_w = 16;
    for (int i = 0; i < 3; ++i)
    {
        widths[i] = std::max(176.0f, g.measure(Font::Bold, ts(27), names[i]) + 76);
        bar_w += widths[i];
    }
    const float bar_x = 960 - bar_w * 0.5f;
    g.panel(bar_x, kBarY, bar_w, kBarH, rgba(0xFFFFFF, 0.92f), 0.95f, kBarH * 0.5f, rev::kRim, 2.5f, 0, 0.3f);
    float tx = bar_x + 8;
    for (int i = 0; i < 3; ++i)
    {
        rev_tab_x_[i] = tx;
        rev_tab_w_[i] = widths[i];
        const bool on = int(tab_) == i;
        const float hover = home_grow_[kRevTab0 + i];
        if (on || hover > 0.02f)
            g.panel(tx, kBarY + 6, widths[i], kBarH - 12, on ? rgba(0xEAF7FD) : rgba(0xF4F9FC, hover), 0.95f,
                    (kBarH - 12) * 0.5f, with_alpha(rev::kBlue, on ? 1.0f : hover), on ? 3.0f : 2.0f,
                    on ? 6.0f : 4.0f * hover, 0.4f);
        g.text_mid(on ? Font::Bold : Font::SemiBold, ts(27), tx + widths[i] * 0.5f, kBarCy,
                   on ? rgba(0x1E8CC4) : rev::kInkSoft, Align::Center, names[i]);
        tx += widths[i];
    }
    g.glyph(Glyph::L1, bar_x - 62, kBarCy, 44, rgba(0x8A939D));
    g.glyph(Glyph::R1, bar_x + bar_w + 62, kBarCy, 44, rgba(0x8A939D));
    if (clock)
        g.text_mid(Font::SemiBold, ts(27), 1866, kBarCy, rev::kInkSoft, Align::Right, clock_text());
}

/* Digits drawn in segments, like a clock's display; the colon blinks. */
void App::draw_seg_clock(float cx, float cy, float h)
{
    Gfx &g = *g_;
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    const int h12 = tm.tm_hour % 12 == 0 ? 12 : tm.tm_hour % 12;
    const int digits[4] = {h12 / 10, h12 % 10, tm.tm_min / 10, tm.tm_min % 10};
    /* a b c d e f g, for 0..9 */
    static const unsigned char kSeg[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    const float w = h * 0.52f, t = h * 0.11f, gap = h * 0.30f, colon = h * 0.34f;
    const float total = w * 4 + gap * 2 + colon;
    float x = cx - total * 0.5f;
    const float top = cy - h * 0.5f;
    const Color on = rev::kDigits;
    auto seg = [&](float sx, float sy, float sw, float sh) { g.panel(sx, sy, sw, sh, on, 0.95f, t * 0.5f); };
    for (int i = 0; i < 4; ++i)
    {
        if (i == 0 && digits[0] == 0)
        {
            x += w + gap; /* "9:41", not "09:41" */
            continue;
        }
        const unsigned char m = kSeg[digits[i]];
        const float hw = w - t, hh = (h - t * 3) * 0.5f;
        if (m & 0x01) seg(x + t * 0.6f, top, hw - t * 0.2f, t);                         /* a */
        if (m & 0x02) seg(x + w - t, top + t * 0.6f, t, hh + t * 0.4f);                 /* b */
        if (m & 0x04) seg(x + w - t, top + t * 2 + hh - t * 0.2f, t, hh + t * 0.4f);    /* c */
        if (m & 0x08) seg(x + t * 0.6f, top + h - t, hw - t * 0.2f, t);                 /* d */
        if (m & 0x10) seg(x, top + t * 2 + hh - t * 0.2f, t, hh + t * 0.4f);            /* e */
        if (m & 0x20) seg(x, top + t * 0.6f, t, hh + t * 0.4f);                         /* f */
        if (m & 0x40) seg(x + t * 0.6f, top + t + hh, hw - t * 0.2f, t);                /* g */
        x += w;
        if (i == 1)
        {
            const bool blink = settings_->reduced_motion || (now % 2) == 0;
            const float d = t * 1.15f, ccx = x + gap * 0.5f + colon * 0.5f - d * 0.5f;
            if (blink)
            {
                g.panel(ccx, top + h * 0.28f, d, d, on, 1, d * 0.5f);
                g.panel(ccx, top + h * 0.66f, d, d, on, 1, d * 0.5f);
            }
            x += colon + gap;
        }
        else if (i < 3)
            x += gap * 0.45f;
    }
    g.text_mid(Font::Bold, ts(h * 0.36f), x + gap * 0.6f, cy + h * 0.30f, on, Align::Left,
               tm.tm_hour < 12 ? "AM" : "PM");
}

/* The date under the clock: "Sun 10/4". */
std::string App::short_date() const
{
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    static const char *const kDays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const std::string day = tr(kDays[std::clamp(tm.tm_wday, 0, 6)]);
    const std::string m = std::to_string(tm.tm_mon + 1), d = std::to_string(tm.tm_mday);
    /* Month first in English, Japanese, Korean and Chinese, the day first
     * elsewhere. */
    switch (language())
    {
    case Language::English: return day + " " + m + "/" + d;
    case Language::Japanese:
    case Language::Korean: return m + "/" + d + " (" + day + ")";
    case Language::ChineseSimplified:
    case Language::ChineseTraditional: return m + "/" + d + " " + day;
    case Language::Turkish: return day + " " + d + "." + m;
    case Language::German:
    case Language::Polish:
    case Language::Russian: return day + " " + d + "." + m + ".";
    default: return day + " " + d + "/" + m;
    }
}

/* The pointer: Ruben's hands (tools/pointer-art), an open hand over nothing
 * and a pointing hand over something it can choose (assets/ui/pointer-open.png,
 * pointer-hand.png), leaning with the controller's roll, the player's number
 * on the palm. The fractions come from tools/make-pointer-hand.py. */
namespace
{
struct HandArt
{
    const char *file;
    float tip_x, tip_y;   /* the spot it points at, as a fraction of its size */
    float num_x, num_y;   /* where the player's number sits */
};
constexpr HandArt kHands[2] = {
    {"pointer-open.png", 0.338f, 0.048f, 0.523f, 0.704f},
    {"pointer-hand.png", 0.309f, 0.008f, 0.513f, 0.719f},
};
} // namespace

void App::draw_hand(float x, float y, float roll, int player, bool pointing)
{
    Gfx &g = *g_;
    if (!hand_tried_)
    {
        hand_tried_ = true;
        for (int i = 0; i < 2; ++i)
        {
            const std::string path = g.asset_dir() + "/ui/" + kHands[i].file;
            if (access(path.c_str(), R_OK) == 0)
                hands_[i] = g.texture_file(path, 512);
        }
    }
    const int which = pointing || !hands_[0] ? 1 : 0;
    Texture *tex = hands_[which];
    if (!tex)
    {
        /* No picture: a ringed dot. */
        g.panel(x - 14, y - 14, 28, 28, rgba(0xFFFFFF), 1, 14, rgba(0x26303D), 3);
        return;
    }
    const HandArt &art = kHands[which];
    /* Both pictures are at one scale, so the hand keeps its size as it changes. */
    constexpr float kHandScale = 0.42f;
    const float h = float(tex->height) * kHandScale, w = float(tex->width) * kHandScale;
    const float hx = art.tip_x * w, hy = art.tip_y * h;
    const float a = std::clamp(roll, -1.0f, 1.0f) * 0.55f, c = std::cos(a), s = std::sin(a);
    g.blob(x + 10, y + h * 0.55f, w * 1.1f, h * 0.85f, rgba(0x1A2230, 0.16f));
    auto corner = [&](float lx, float ly) {
        const float dx = lx - hx, dy = ly - hy;
        return Corner{x + dx * c - dy * s, y + dx * s + dy * c, 1.0f};
    };
    const Corner q[4] = {corner(0, 0), corner(w, 0), corner(w, h), corner(0, h)};
    g.quad3d(tex, q, w, h, rgba(0xFFFFFF), 0, false, false);
    const Corner num = corner(w * art.num_x, h * art.num_y);
    g.text_mid(Font::ExtraBold, 40, num.x, num.y, rgba(0x1E9BE0), Align::Center, std::to_string(player));
}

/* ---- pointing, shared by the home screen and the opened tile ----------------------------- */

bool App::pointer_screen() const
{
    if (!revolution() || tab_ != Tab::Library || dialog_.open || updating())
        return false;
    if (screen_ == Screen::Details)
        return true;
    return settings_->ui_layout == 0 && (screen_ == Screen::Main || screen_ == Screen::Sort);
}

/* Reads where the controller points; the touch pad turns pointing on and off. */
void App::rev_pointer_step()
{
    if (pressed(BtnTouch))
    {
        settings_->ui_pointer = !settings_->ui_pointer;
        settings_->save(settings_path_);
        flash_note(settings_->ui_pointer ? tr("Pointer on: point at the screen.")
                                         : tr("Pointer off: the touch pad turns it back on."));
        sfx(Sound::MovingTab);
    }
    const porpoise::pad::Motion m = porpoise::pad::snapshot(0).motion;
    bool aimed = settings_->ui_pointer && m.valid && m.centred;
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
}

/* The D-pad takes over from the pointer until the controller moves again. */
void App::rev_pointer_rest()
{
    if (home_pointing_)
    {
        home_pointing_ = false;
        home_hide_x_ = home_px_;
        home_hide_y_ = home_py_;
    }
}

/* A tap in the hand and a tick as the pointer (or the D-pad) lands on something. */
void App::rev_landed()
{
    sfx(Sound::GameRow);
    if (settings_->rumble && home_pointing_)
    {
        porpoise::pad::set_rumble(0, false, 0x2600);
        home_buzz_until_ = time_ + 0.035;
    }
}

int App::rev_tab_hit(float px, float py) const
{
    for (int i = 0; i < 3; ++i)
        if (rev_tab_w_[i] > 0 && inside(px, py, rev_tab_x_[i], kBarY, rev_tab_w_[i], kBarH))
            return kRevTab0 + i;
    return -1;
}

void App::draw_rev_pointer()
{
    if (!home_pointing_ || !pointer_screen() || screen_ == Screen::Sort)
        return;
    const bool over = screen_ == Screen::Details ? rd_focus_ >= 0 : home_focus_ >= 0;
    draw_hand(home_px_, home_py_, home_roll_, 1, over);
}

/* ---- the opened tile (Details) --------------------------------------------------------------- */

namespace
{
enum ChipId
{
    kChipStates,
    kChipSettings,
    kChipSave,
    kChipFavourite,
};
} // namespace

std::vector<std::pair<int, std::string>> App::rev_chips(const Game &game) const
{
    std::vector<std::pair<int, std::string>> chips;
    chips.push_back({kChipStates, details_states_ > 0 ? trf("Save states ({n})", {{"n", std::to_string(details_states_)}})
                                                      : tr("Save states")});
    if (game.platform == "Wii")
        chips.push_back({kChipSettings, tr("Game settings")});
    chips.push_back({kChipSave, tr("Save data")});
    chips.push_back({kChipFavourite, game.favourite ? tr("Favourite") : tr("Add to favourites")});
    for (auto &c : chips)
        c.second = title_case(c.second);
    return chips;
}

/* Where each focus target sits on the opened tile. */
void App::rev_details_rects(const Game &game, std::vector<std::pair<int, std::array<float, 4>>> &out)
{
    Gfx &g = *g_;
    out.clear();
    /* The controls on the left, Start on the right. */
    out.push_back({kRdSecond, {960 - kBigGap * 0.5f - kBigW, kBigY, kBigW, kBigH}});
    out.push_back({kRdStart, {960 + kBigGap * 0.5f, kBigY, kBigW, kBigH}});
    const auto chips = rev_chips(game);
    float total = 0;
    std::vector<float> widths;
    for (const auto &c : chips)
    {
        widths.push_back(g.measure(Font::SemiBold, ts(22), c.second) + 56);
        total += widths.back();
    }
    total += 18.0f * float(chips.size() - 1);
    float x = 960 - total * 0.5f;
    for (std::size_t i = 0; i < chips.size(); ++i)
    {
        out.push_back({kRdChip + int(i), {x, kChipY, widths[i], kChipH}});
        x += widths[i] + 18;
    }
    out.push_back({kRdPrev, {kSideX[0] - kSideArrow * 0.5f, kSideY - kSideArrow * 0.5f, kSideArrow, kSideArrow}});
    out.push_back({kRdNext, {kSideX[1] - kSideArrow * 0.5f, kSideY - kSideArrow * 0.5f, kSideArrow, kSideArrow}});
}

App::Action App::update_rev_details(bool left, bool right, bool up, bool down, double dt)
{
    auto &games = lib_->games();
    Action action = Action::None;
    const int shown = lib_->shown();
    if (shown == 0)
    {
        open_screen(Screen::Main);
        return action;
    }
    selected_ = std::clamp(selected_, 0, shown - 1);
    Game &game = games[std::size_t(selected_)];
    const auto chips = rev_chips(game);
    const int nchips = int(chips.size());

    auto go = [&](int dir) {
        const int to = selected_ + dir;
        if (to < 0 || to >= shown)
            return;
        swipe_from_ = selected_;
        selected_ = to;
        swipe_dir_ = dir;
        swipe_anim_ = settings_->reduced_motion ? 0.0f : 1.0f;
        rd_info_ = false;
        count_states(&games[std::size_t(selected_)]);
        lib_->set_selected(Library::key_of(games[std::size_t(selected_)]));
        home_synced_ = false;
        sfx(Sound::GameRow);
        opened_tile(games[std::size_t(selected_)]);
    };

    rev_pointer_step();
    const int before = rd_focus_;
    if (left || right || up || down)
    {
        rev_pointer_rest();
        int f = rd_focus_;
        if (f == kRdPrev || f == kRdNext || f < 0)
            f = kRdStart;
        else if (f < kRdChip)
        {
            if (left && f == kRdStart)
                f = kRdSecond;
            else if (right && f == kRdSecond)
                f = kRdStart;
            else if (left && f == kRdSecond)
                go(-1);
            else if (right && f == kRdStart)
                go(+1);
            else if (down)
                f = kRdChip + std::min(nchips - 1, f == kRdSecond ? 0 : nchips / 2);
        }
        else
        {
            const int c = f - kRdChip;
            if (left && c > 0)
                f = f - 1;
            else if (right && c + 1 < nchips)
                f = f + 1;
            else if (up)
                f = c < nchips / 2 ? kRdSecond : kRdStart;
        }
        rd_focus_ = f;
    }
    else if (home_pointing_)
    {
        std::vector<std::pair<int, std::array<float, 4>>> rects;
        rev_details_rects(game, rects);
        int hit = rev_tab_hit(home_px_, home_py_);
        for (const auto &r : rects)
        {
            if ((r.first == kRdPrev && selected_ == 0) || (r.first == kRdNext && selected_ + 1 >= shown))
                continue;
            const auto &b = r.second;
            const bool round = r.first == kRdPrev || r.first == kRdNext;
            if (round ? in_circle(home_px_, home_py_, b[0] + b[2] * 0.5f, b[1] + b[3] * 0.5f, b[2] * 0.5f + 10)
                      : inside(home_px_, home_py_, b[0], b[1], b[2], b[3]))
                hit = r.first;
        }
        rd_focus_ = hit;
    }
    if (rd_focus_ != before && rd_focus_ >= 0)
        rev_landed();

    if (pressed(BtnL2))
        go(-1);
    if (pressed(BtnR2))
        go(+1);
    if (pressed(BtnR3))
    {
        porpoise::pad::centre_now(0);
        flash_note(tr("Pointer centred: it points where the controller points now."));
    }
    if (pressed(BtnTriangle))
    {
        rd_info_ = !rd_info_;
        sfx(Sound::DetailsFlip);
    }
    if (pressed(BtnOptions))
    {
        lib_->toggle_favourite(game);
        sfx(game.favourite ? Sound::LaunchGame : Sound::MovingTab);
    }
    if (pressed(BtnCircle))
    {
        open_screen(Screen::Main);
        start_zoom(-1);
        sfx(Sound::DetailsFlip);
        if (jingle_)
            jingle_(nullptr, 0);
        jingle_for_.clear();
        return action;
    }
    if (pressed(BtnCross))
    {
        const int f = rd_focus_;
        if (f >= kRevTab0 && f < kRevTab0 + 3)
        {
            open_screen(Screen::Main);
            set_tab(f - kRevTab0, +1);
        }
        else if (f == kRdStart)
        {
            if (jingle_)
                jingle_(nullptr, 0);
            action = start_game(&game, "");
            sfx(Sound::LaunchGame);
        }
        else if (f == kRdSecond)
        {
            sfx(Sound::MenuScroll);
            if (game.platform == "Wii")
                open_wii_setup(&game, false, "");
            else
                open_game_settings(game);
        }
        else if (f == kRdPrev)
            go(-1);
        else if (f == kRdNext)
            go(+1);
        else if (f >= kRdChip && f < kRdChip + nchips)
        {
            switch (chips[std::size_t(f - kRdChip)].first)
            {
            case kChipStates:
                open_states(&game);
                break;
            case kChipSettings:
                open_game_settings(game);
                sfx(Sound::MenuScroll);
                break;
            case kChipSave:
                open_screen(Screen::Main);
                if (game.platform == "Wii")
                {
                    mc_wii_ = true;
                    wii_focus_code_ = game.id;
                }
                else
                {
                    mc_wii_ = false;
                    mc_focus_code_ = game.id;
                }
                set_tab(int(Tab::MemoryCards), +1);
                break;
            case kChipFavourite:
                lib_->toggle_favourite(game);
                sfx(game.favourite ? Sound::LaunchGame : Sound::MovingTab);
                break;
            default:
                break;
            }
        }
    }

    const bool calm = settings_->reduced_motion;
    for (int i = 0; i < kRdGrow; ++i)
    {
        const float target = rd_focus_ == i ? 1.0f : 0.0f;
        rd_grow_[i] = calm ? target : smooth(rd_grow_[i], target, dt, 14.0f);
    }
    for (int i = 0; i < 3; ++i)
    {
        const float target = rd_focus_ == kRevTab0 + i ? 1.0f : 0.0f;
        home_grow_[kRevTab0 + i] = calm ? target : smooth(home_grow_[kRevTab0 + i], target, dt, 14.0f);
    }
    return action;
}

/* What fills the frame when there is no banner: the cover large, the facts
 * beside it, all on a wash of the cover's own colours. */
void App::draw_rev_picture(Game &game, float x, float y, float w, float h, float fade)
{
    Gfx &g = *g_;
    const bool wii = game.platform == "Wii";
    Texture *cover = cover_of(game);
    if (cover)
    {
        float uv[4];
        cover_uv(cover, w, h, uv);
        g.image_part(cover, x, y, w, h, uv, rgba(0xFFFFFF, 0.16f * fade));
    }
    g.panel(x, y, w, h, rgba(0xFFFFFF, 0.55f * fade), 1.0f, rev::kTileR - 8);
    const float ch = h - 96, cw = ch * 5.0f / 7.0f, cx = x + 56, cy = y + 48;
    g.blob(cx + cw * 0.5f + 10, cy + ch + 6, cw * 1.05f, 50, rgba(0x1A2230, 0.16f * fade));
    if (cover)
        g.image(cover, cx, cy, cw, ch, rgba(0xFFFFFF, fade), 18);
    else
    {
        g.panel(cx, cy, cw, ch, with_alpha(wii ? rev::kWii : rev::kGameCube, 0.16f * fade), 1, 18,
                with_alpha(wii ? rev::kWii : rev::kGameCube, 0.5f * fade), 2);
        if (!cover_loading(game))
            draw_mark(cx + cw * 0.5f, cy + ch * 0.5f, cw * 0.5f, with_alpha(kWhite, fade));
    }
    const float tx = cx + cw + 64, tw = x + w - 56 - tx;
    float ty = cy + 6;
    for (const std::string &l : wrap(g, Font::Bold, ts(50), game.title, tw, 2))
    {
        g.text(Font::Bold, ts(50), tx, ty, with_alpha(rev::kInk, fade), Align::Left, l);
        ty += 62;
    }
    ty += 8;
    /* A pill for the console, then what is known about it. */
    {
        const std::string label = wii ? "Wii" : "GameCube";
        const float lw = g.measure(Font::SemiBold, ts(21), label) + 30;
        g.panel(tx, ty, lw, 36, with_alpha(wii ? rev::kWii : rev::kGameCube, fade), 0.9f, 18);
        g.text_mid(Font::SemiBold, ts(21), tx + lw * 0.5f, ty + 18, with_alpha(rev::kTile, fade), Align::Center, label);
        std::string meta;
        auto add = [&](const std::string &s) {
            if (!s.empty())
                meta += (meta.empty() ? "" : "   \xE2\x80\xA2   ") + s;
        };
        add(game.region);
        add(game.released.size() >= 4 ? game.released.substr(game.released.size() - 4) : game.released);
        if (game.players > 0)
            add(plural(game.players, "1 player", "{n} players"));
        add(relative_time(game.last_played, (long long)std::time(nullptr)));
        g.text_mid(Font::SemiBold, ts(24), tx + lw + 22, ty + 18, with_alpha(rev::kInkSoft, fade), Align::Left,
                   fit(g, Font::SemiBold, ts(24), meta, tw - lw - 22));
        ty += 66;
    }
    const std::string who = !game.developer.empty() && !game.publisher.empty() && game.developer != game.publisher
                                ? game.developer + "  /  " + game.publisher
                                : (!game.developer.empty() ? game.developer : game.publisher);
    if (!who.empty())
    {
        g.text(Font::SemiBold, ts(26), tx, ty, with_alpha(rev::kInk, fade), Align::Left, fit(g, Font::SemiBold, ts(26), who, tw));
        ty += 40;
    }
    if (!game.genre.empty())
    {
        g.text(Font::Regular, ts(24), tx, ty, with_alpha(rev::kInkSoft, fade), Align::Left, fit(g, Font::Regular, ts(24), game.genre, tw));
        ty += 44;
    }
    ty += 10;
    const std::string about = !game.synopsis.empty()
                                  ? game.synopsis
                                  : tr("No description yet: Sort & filter (Triangle, on the home screen) can get the "
                                       "game info.");
    const int lines = std::max(2, int((y + h - 40 - ty) / 38));
    for (const std::string &l : wrap(g, Font::Regular, ts(25), about, tw, std::size_t(lines)))
    {
        g.text(Font::Regular, ts(25), tx, ty, with_alpha(rev::kInkSoft, fade), Align::Left, l);
        ty += 38;
    }
}

void App::draw_rev_details(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    if (lib_->shown() == 0)
        return;
    Game &game = games[std::size_t(selected_)];
    const bool wii = game.platform == "Wii";

    /* The frame, and what is in it: the game's own banner when Porpoise has
     * read it from the disc, else the cover and its facts. Changing game
     * slides the old one out. */
    const float p = swipe_anim_ > 0 ? ease_out(1.0f - swipe_anim_) : 1.0f;
    g.blob(960, kFrameY + kFrameH + 8, kFrameW * 0.94f, 60, rgba(0x1A2230, 0.10f));
    g.panel(kFrameX, kFrameY, kFrameW, kFrameH, rev::kTile, 0.97f, 34, rev::kRim, 3.0f, 0, 0.25f);
    const float ix = kFrameX + 14, iy = kFrameY + 14, iw = kFrameW - 28, ih = kFrameH - 28;
    auto content = [&](Game &which, float dx, float fade) {
        const float clip_l = std::max(ix, ix + dx), clip_r = std::min(ix + iw, ix + iw + dx);
        if (clip_r - clip_l < 40)
            return;
        if (!rd_info_ && draw_banner(which, ix + dx, iy, iw, ih, time, fade, true))
            return;
        draw_rev_picture(which, ix + dx, iy, iw, ih, fade);
    };
    if (swipe_anim_ > 0 && swipe_from_ >= 0 && swipe_from_ < int(games.size()) && swipe_from_ != selected_)
    {
        content(games[std::size_t(swipe_from_)], -float(swipe_dir_) * 420.0f * p, 1.0f - p);
        content(game, float(swipe_dir_) * 420.0f * (1.0f - p), p);
    }
    else
        content(game, 0, 1);
    if (has_banner(game))
    {
        /* Triangle turns between the disc's own banner and the facts. */
        const std::string hint = rd_info_ ? tr("The game's banner") : tr("About this game");
        const float hw = g.measure(Font::SemiBold, ts(20), hint) + 36 + 34;
        const float hx = kFrameX + kFrameW - 26 - hw, hy = kFrameY + kFrameH - 26 - 40;
        g.panel(hx, hy, hw, 40, rgba(0xFFFFFF, 0.92f), 0.96f, 20, rev::kRim, 2);
        g.glyph(Glyph::Triangle, hx + 24, hy + 20, 24, rev::kInkSoft);
        g.text_mid(Font::SemiBold, ts(20), hx + 44, hy + 20, rev::kInk, Align::Left, hint);
    }

    /* Arrows to the games beside it. */
    for (int side = 0; side < 2; ++side)
    {
        const bool show = side == 0 ? selected_ > 0 : selected_ + 1 < lib_->shown();
        if (!show)
            continue;
        const float grow = rd_grow_[side == 0 ? kRdPrev : kRdNext];
        const float s = kSideArrow * (1.0f + 0.12f * grow);
        g.panel(kSideX[side] - s * 0.5f, kSideY - s * 0.5f, s, s, rev::kTile, 0.94f, s * 0.5f,
                grow > 0.02f ? rev::kBlue : rev::kRim, 3.0f, grow * 10.0f, 0.3f);
        g.glyph(Glyph::Arrow, kSideX[side] + (side == 0 ? -3.0f : 3.0f), kSideY, 30,
                grow > 0.5f ? rev::kBlue : rev::kInkSoft, side == 0 ? -kPi * 0.5f : kPi * 0.5f);
    }

    /* The floor: Start and the controls, the smaller choices under them. */
    std::vector<std::pair<int, std::array<float, 4>>> rects;
    rev_details_rects(game, rects);
    const auto chips = rev_chips(game);
    for (const auto &r : rects)
    {
        const int id = r.first;
        if (id == kRdPrev || id == kRdNext)
            continue;
        const auto &b = r.second;
        const float grow = id < kRdGrow ? rd_grow_[id] : 0.0f;
        const bool big = id < kRdChip;
        const float k = 1.0f + (big ? 0.05f : 0.06f) * grow;
        const float w = b[2] * k, h = b[3] * k, x = b[0] + b[2] * 0.5f - w * 0.5f, y = b[1] + b[3] * 0.5f - h * 0.5f;
        g.blob(x + w * 0.5f, y + h + 4, w * 0.9f, 24, rgba(0x1A2230, 0.10f));
        g.panel(x, y, w, h, rev::kTile, 0.94f, h * 0.5f, grow > 0.02f ? rev::kBlue : rev::kBlueSoft,
                big ? 3.5f + grow * 1.5f : 2.5f, grow * (big ? 14.0f : 8.0f), 0.45f);
        std::string label;
        if (id == kRdStart)
            label = title_case(tr("Start"));
        else if (id == kRdSecond)
            label = title_case(wii ? tr("Wii controls") : tr("Game settings"));
        else
            label = chips[std::size_t(id - kRdChip)].second;
        const Color ink = grow > 0.5f ? rgba(0x1E8CC4) : rev::kInk;
        if (big)
            g.text_mid(Font::Bold, ts(38) * k, x + w * 0.5f, y + h * 0.5f, ink, Align::Center, label);
        else
        {
            const bool star = chips[std::size_t(id - kRdChip)].first == kChipFavourite;
            if (star)
                g.glyph(game.favourite ? Glyph::Star : Glyph::StarOutline, x + 30, y + h * 0.5f, 22,
                        rgba(0xF5B82E));
            g.text_mid(Font::SemiBold, ts(22), x + w * 0.5f + (star ? 12.0f : 0.0f), y + h * 0.5f, ink,
                       Align::Center, label);
        }
    }
    const std::string note = library_note();
    if (!note.empty())
    {
        const float nw = g.measure(Font::SemiBold, ts(22), note) + 44;
        g.panel(960 - nw * 0.5f, kFrameY + 18, nw, 46, rgba(0xFFFFFF, 0.95f), 0.96f, 23, rev::kRim, 2);
        g.text_mid(Font::SemiBold, ts(22), 960, kFrameY + 41, rev::kInk, Align::Center, note);
    }
}
} // namespace porpoise::ui
