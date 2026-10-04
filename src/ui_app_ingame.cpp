/* Porpoise UI - the in-game menu: Options + touch pad pauses the game and
 * slides this in from the left, in the launcher's own glass.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Four tabs, L1 / R1 between them:
 *   Game      resume, save states (three slots, with a picture each), fast
 *             forward, volume, quit
 *   Video     resolution, widescreen, aspect, the screen filter and border...
 *   Graphics  shader compilation and the other Dolphin graphics options
 *   Controls  the button layout, Customize buttons (the mapping screen, over
 *             the game), vibration, and the controller with a line out to
 *             every button, saying which GameCube button it is.
 * Changes are saved as the game's own settings and take effect at once
 * (main applies them as take_menu_change() reports them). */
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sys/stat.h>

#include "porpoise_borders.hpp"
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
enum TabId
{
    kTabGame,
    kTabVideo,
    kTabGraphics,
    kTabControls,
    kTabCount,
};
const char *const kTabNames[kTabCount] = {"Game", "Video", "Graphics", "Controls"};

/* What a row does. */
enum class Kind
{
    Resume,
    Save,
    Load,
    FastForward,
    Library,
    Home,
    Customize,
    Int,  /* a setting with a list of values */
    Bool, /* a setting that is on or off */
    Border,
};

struct Row
{
    Kind kind;
    const char *key; /* the setting's key, for Int / Bool / Border */
    const char *label;
    int *iv = nullptr;
    bool *bv = nullptr;
    int min = 0;
    std::vector<std::string> values = {}; /* English; translated when drawn */
    bool gap_before = false;
};

const std::vector<std::string> kPercent = {"0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"};

std::vector<Row> rows_for(int tab, Settings &p)
{
    std::vector<Row> r;
    switch (tab)
    {
    case kTabGame:
        r.push_back({Kind::Resume, "", "Resume"});
        r.push_back({Kind::Save, "", "Save state"});
        r.push_back({Kind::Load, "", "Load state"});
        r.push_back({Kind::FastForward, "", "Fast forward"});
        r.push_back({Kind::Int, "volume", "Volume", &p.volume, nullptr, 0, kPercent});
        r.push_back({Kind::Library, "", "Quit to library", nullptr, nullptr, 0, {}, true});
        r.push_back({Kind::Home, "", "Close Porpoise"});
        break;
    case kTabVideo:
        r.push_back({Kind::Int, "resolution", "Internal resolution", &p.resolution, nullptr, 1,
                     {"1x (480p)", "2x (720p)", "3x (1080p)", "4x (1440p) \xE2\x80\xA2 exp.", "5x (1800p) \xE2\x80\xA2 exp.",
                      "6x (4K) \xE2\x80\xA2 exp."}});
        r.push_back({Kind::Bool, "widescreen", "Widescreen hack", nullptr, &p.widescreen, 0, {"Off", "On"}});
        r.push_back({Kind::Int, "aspect", "Aspect ratio", &p.aspect, nullptr, 0,
                     {"Auto", "Force 16:9", "Force 4:3", "Stretch to fill"}});
        r.push_back({Kind::Int, "antialiasing", "Anti-aliasing", &p.antialiasing, nullptr, 0,
                     {"Off", "2x MSAA", "4x MSAA", "8x MSAA", "2x SSAA", "4x SSAA", "8x SSAA"}});
        r.push_back({Kind::Int, "anisotropy", "Anisotropic filtering", &p.anisotropy, nullptr, 0,
                     {"1x", "2x", "4x", "8x", "16x"}});
        r.push_back({Kind::Int, "screen_filter", "Screen filter", &p.screen_filter, nullptr, 0,
                     {"Smooth", "Sharp", "Sharpen", "CRT", "Arcade CRT", "VHS"}});
        r.push_back({Kind::Int, "filter_strength", "Filter strength", &p.filter_strength, nullptr, 1,
                     {"10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"}});
        r.push_back({Kind::Border, "border", "Border"});
        r.push_back({Kind::Bool, "fps_overlay", "FPS counter", nullptr, &p.fps_overlay, 0, {"Off", "On"}});
        break;
    case kTabGraphics:
        r.push_back({Kind::Int, "shader_mode", "Shader compilation", &p.shader_mode, nullptr, 0,
                     {"Synchronous", "Ubershaders", "Async ubershaders", "Async, skip drawing"}});
        r.push_back({Kind::Int, "texture_cache", "Texture cache accuracy", &p.texture_cache, nullptr, 0,
                     {"Fast", "Middle", "Safe"}});
        r.push_back({Kind::Int, "texture_filter", "Texture filtering", &p.texture_filter, nullptr, 0,
                     {"Game's own", "Nearest (sharp)", "Linear (smooth)"}});
        r.push_back({Kind::Int, "resampling", "Output resampling", &p.resampling, nullptr, 0,
                     {"Default", "Bilinear", "B-Spline", "Mitchell-Netravali", "Catmull-Rom", "Sharp bilinear",
                      "Area sampling"}});
        r.push_back({Kind::Bool, "pixel_lighting", "Per-pixel lighting", nullptr, &p.pixel_lighting, 0, {"Off", "On"}});
        r.push_back({Kind::Bool, "disable_fog", "Disable fog", nullptr, &p.disable_fog, 0, {"Off", "On"}});
        r.push_back({Kind::Bool, "crop_overscan", "Crop overscan", nullptr, &p.crop_overscan, 0, {"Off", "On"}});
        r.push_back({Kind::Bool, "skip_dupes", "Skip duplicate frames", nullptr, &p.skip_dupes, 0, {"Off", "On"}});
        break;
    case kTabControls:
        r.push_back({Kind::Int, "button_layout", "Button layout", &p.button_layout, nullptr, 0,
                     {"GameCube", "PlayStation", "My layout 1", "My layout 2", "My layout 3", "My layout 4"}});
        r.push_back({Kind::Customize, "", "Customize buttons"});
        r.push_back({Kind::Bool, "rumble", "Vibration", nullptr, &p.rumble, 0, {"Off", "On"}});
        break;
    default:
        break;
    }
    return r;
}

/* The help line under the rows. */
const char *help_for(const Row &row, const Settings &p)
{
    if (row.key == std::string("resolution") && p.resolution >= 4)
        return "Experimental: may slow some games down.";
    if (row.key == std::string("screen_filter"))
        return "Porpoise's own filter on the way to the TV. CRT and Arcade CRT look best at 1080p or above.";
    if (row.key == std::string("border"))
        return "Fills the bars beside a 4:3 picture (widescreen off). Add your own PNGs to /data/porpoise/borders.";
    if (row.key == std::string("shader_mode"))
        return "Async ubershaders hide the stutter when a game draws something new. Takes effect after a restart of the game.";
    if (row.key == std::string("texture_cache"))
        return "Safe fixes some games' text and effects; Fast is quickest.";
    if (row.kind == Kind::FastForward)
        return "Runs the game 2x or 4x faster until you turn it off. The game's sound pauses meanwhile.";
    if (row.kind == Kind::Save || row.kind == Kind::Load)
        return "Three slots for this game. Left and right choose the slot.";
    if (row.kind == Kind::Customize)
        return "Your own layouts: change any button on a picture of the DualSense.";
    return "Changes here are saved for this game.";
}

/* The schematic (assets/ui/controller-lines.png, tools/make-controller-art.py):
 * its crop of Zacksly's 4096x2160 drawing, and where each line ends there. */
constexpr float kLinesX0 = 780, kLinesY0 = 150, kLinesW = 2540, kLinesH = 1610;
struct LineEnd
{
    float x, y;
    bool left; /* the label goes to the left of the end */
};
enum EndId
{
    kEndL,
    kEndR,
    kEndTouch,
    kEndCreate,
    kEndOptions,
    kEndDPad,
    kEndFace,
    kEndLStick,
    kEndRStick,
    kEndCount,
};
constexpr LineEnd kEnds[kEndCount] = {
    {1023.36f, 229.18f, true},   {3072.64f, 230.91f, false}, {1885.13f, 237.25f, true},
    {969.74f, 544.5f, true},     {3126.26f, 544.5f, false},  {860.62f, 1015.03f, true},
    {3154.78f, 1015.03f, false}, {864.88f, 1430.69f, true},  {3231.12f, 1430.69f, false},
};

std::string slot_date(long long t)
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

/* ---- save-state slots ------------------------------------------------------------------- */

void App::menu_free_slots()
{
    for (Texture *&t : menu_slot_tex_)
        if (t)
        {
            g_->free_texture(t);
            t = nullptr;
        }
}

void App::load_slots(const Game *game)
{
    menu_free_slots();
    if (!game)
        return;
    const std::string key = Library::key_of(*game);
    for (int i = 0; i < porpoise::states::kSlots; ++i)
    {
        const porpoise::states::Slot s = porpoise::states::slot(key, i);
        menu_slot_used_[i] = s.exists;
        menu_slot_time_[i] = s.time;
        menu_slot_tex_[i] = s.exists ? g_->texture_file(s.picture_path) : nullptr;
    }
}

void App::menu_state_done(MenuRequest::Kind kind, int slot, bool ok)
{
    if (kind == MenuRequest::Save)
    {
        menu_note_ = ok ? trf("Saved to slot {n}.", {{"n", std::to_string(slot + 1)}})
                        : tr("The game's state couldn't be saved.");
        if (ok)
            load_slots(menu_game_);
        sfx(ok ? Sound::LaunchGame : Sound::MovingTab);
    }
    else
    {
        menu_note_ = ok ? trf("Slot {n} loaded.", {{"n", std::to_string(slot + 1)}})
                        : tr("That slot couldn't be loaded.");
        sfx(ok ? Sound::LaunchGame : Sound::MovingTab);
    }
    menu_note_time_ = time_;
}

/* ---- opening and input ------------------------------------------------------------------ */

void App::open_game_menu(Game *game, Settings *play)
{
    menu_game_ = game;
    menu_play_ = play;
    menu_row_ = 0;
    menu_tab_ = kTabGame;
    menu_anim_ = 0;
    menu_closing_ = false;
    menu_answer_ = 0;
    menu_note_.clear();
    map_in_game_ = false;
    menu_borders_ = porpoise::borders::list();
    load_slots(menu_game_);
    prev_ = held_ = raw_held_ = raw_prev_ = 0xFFFFFFFFu; /* buttons still down from the shortcut don't count */
    sfx(Sound::DetailsFlip);
}

int App::update_game_menu(const Input &in, double dt)
{
    time_ += dt;
    prev_ = held_;
    held_ = in.held;
    raw_prev_ = raw_held_;
    raw_held_ = in.held;
    if (in.stick_y < -0.55f) held_ |= BtnUp;
    if (in.stick_y > 0.55f) held_ |= BtnDown;
    if (in.stick_x < -0.55f) held_ |= BtnLeft;
    if (in.stick_x > 0.55f) held_ |= BtnRight;
    const bool calm = settings_->reduced_motion;
    if (menu_closing_)
    {
        menu_anim_ = calm ? 0.0f : std::max(0.0f, menu_anim_ - float(dt) * 6.0f);
        if (menu_anim_ <= 0.0f)
            menu_free_slots();
        return menu_anim_ <= 0.0f ? menu_answer_ : 0;
    }
    menu_anim_ = calm ? 1.0f : std::min(1.0f, menu_anim_ + float(dt) * 5.0f);
    if (!menu_play_)
        return 1;

    const bool up = nav(BtnUp, rep_up_, dt), down = nav(BtnDown, rep_down_, dt);
    const bool left = nav(BtnLeft, rep_left_, dt), right = nav(BtnRight, rep_right_, dt);

    /* The mapping screen, over the game. */
    if (map_in_game_)
    {
        update_mapping(up, down, left, right);
        return 0;
    }

    Settings &p = *menu_play_;
    std::vector<Row> rows = rows_for(menu_tab_, p);
    const int count = int(rows.size());

    /* L1 / R1: the tabs. */
    if (pressed(BtnL1) || pressed(BtnR1))
    {
        menu_tab_ = (menu_tab_ + (pressed(BtnL1) ? kTabCount - 1 : 1)) % kTabCount;
        menu_row_ = 0;
        sfx(Sound::MovingTab);
        return 0;
    }
    if (up)
    {
        menu_row_ = (menu_row_ + count - 1) % count;
        sfx(Sound::MenuScroll);
    }
    if (down)
    {
        menu_row_ = (menu_row_ + 1) % count;
        sfx(Sound::MenuScroll);
    }
    menu_row_ = std::clamp(menu_row_, 0, count - 1);

    auto close = [&](int answer) {
        sfx(answer == 1 ? Sound::DetailsFlip : Sound::MovingTab);
        menu_closing_ = true;
        menu_answer_ = answer;
        if (calm)
        {
            menu_anim_ = 0;
            menu_free_slots();
        }
        return calm ? answer : 0;
    };
    const bool combo = (held_ & (BtnOptions | BtnTouch)) == (BtnOptions | BtnTouch) &&
                       (prev_ & (BtnOptions | BtnTouch)) != (BtnOptions | BtnTouch);
    if (pressed(BtnCircle) || combo)
        return close(1);

    const Row &row = rows[std::size_t(menu_row_)];
    const bool cross = pressed(BtnCross);
    const int dir = left ? -1 : (right || cross) ? 1 : 0;
    std::string key;
    switch (row.kind)
    {
    case Kind::Resume:
        if (cross)
            return close(1);
        break;
    case Kind::Library:
        if (cross)
            return close(2);
        break;
    case Kind::Home:
        if (cross)
            return close(3);
        break;
    case Kind::Save:
    case Kind::Load:
        if (left || right)
        {
            menu_slot_ = (menu_slot_ + (left ? porpoise::states::kSlots - 1 : 1)) % porpoise::states::kSlots;
            sfx(Sound::MenuScroll);
        }
        else if (cross)
        {
            if (row.kind == Kind::Load && !menu_slot_used_[menu_slot_])
            {
                menu_note_ = tr("That slot is empty.");
                menu_note_time_ = time_;
                sfx(Sound::MovingTab);
            }
            else
            {
                menu_request_.kind = row.kind == Kind::Save ? MenuRequest::Save : MenuRequest::Load;
                menu_request_.slot = menu_slot_;
                if (row.kind == Kind::Load)
                    return close(1); /* back into the game at that moment */
            }
        }
        break;
    case Kind::FastForward:
        if (dir)
        {
            menu_ff_ = (menu_ff_ + (dir < 0 ? 2 : 1)) % 3;
            menu_change_ = "fast_forward";
            sfx(Sound::MenuScroll);
        }
        break;
    case Kind::Customize:
        if (cross)
            open_mapping_in_game();
        break;
    case Kind::Int:
        if (dir)
        {
            const int n = int(row.values.size());
            int v = *row.iv + dir;
            if (cross && !left && !right && v > row.min + n - 1)
                v = row.min; /* Cross goes round */
            v = std::clamp(v, row.min, row.min + n - 1);
            if (v != *row.iv)
            {
                *row.iv = v;
                key = row.key;
            }
        }
        break;
    case Kind::Bool:
        if (dir)
        {
            *row.bv = !*row.bv;
            key = row.key;
        }
        break;
    case Kind::Border:
        if (dir && !menu_borders_.empty())
        {
            int i = 0;
            for (std::size_t b = 0; b < menu_borders_.size(); ++b)
                if (menu_borders_[b].name == p.border)
                    i = int(b);
            const int n = int(menu_borders_.size());
            i = (i + (dir < 0 ? n - 1 : 1)) % n;
            p.border = menu_borders_[std::size_t(i)].name;
            key = row.key;
        }
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

/* ---- drawing ---------------------------------------------------------------------------- */

/* The controller with a line out to each button, and at each line's end the
 * GameCube button it is. w is the drawing's width; the labels go beside it. */
void App::draw_controller_lines(float x, float y, float w, const Mapping &m)
{
    Gfx &g = *g_;
    if (!lines_art_tried_)
    {
        lines_art_tried_ = true;
        lines_art_ = g.texture_file(g.asset_dir() + "/ui/controller-lines.png");
    }
    if (!lines_art_)
        return;
    const float k = w / kLinesW, h = kLinesH * k;
    g.image(lines_art_, x, y, w, h, rgba(0xBFD8FF, 0.95f));

    int on_control[CtlCount];
    std::fill(std::begin(on_control), std::end(on_control), -1);
    for (int gc = 0; gc < GcCount; ++gc)
        if (m.control[gc] >= 0 && m.control[gc] < CtlCount)
            on_control[int(m.control[gc])] = gc;

    /* One label line: a DualSense icon and what it is (a chip, or a word). */
    struct Item
    {
        Icon icon;
        int control; /* -1: no chip */
        std::string text;
    };
    const float ih = 30, chip_h = 24, gap = 6;
    auto item_w = [&](const Item &it) {
        float iw = ih + gap;
        if (!it.text.empty())
            iw += g.measure(Font::SemiBold, ts(18), it.text);
        else
            iw += gc_chip_width(it.control >= 0 ? on_control[it.control] : -1, chip_h);
        return iw;
    };
    auto draw_group = [&](EndId end, const std::vector<Item> &items) {
        const LineEnd &e = kEnds[end];
        const float ex = x + (e.x - kLinesX0) * k, ey = y + (e.y - kLinesY0) * k;
        float iy = ey - (float(items.size()) - 1) * ih * 0.5f;
        if (end == kEndTouch)
            iy = ey - 6; /* the touch pad's line ends going up: sit beside its end */
        for (const Item &it : items)
        {
            const float iw = item_w(it);
            float ix = e.left ? ex - 10 - iw : ex + 10;
            g.icon(it.icon, ix + ih * 0.5f, iy, ih + 6, rgba(0xDCE8FF));
            ix += ih + gap;
            if (!it.text.empty())
                g.text_mid(Font::SemiBold, ts(18), ix, iy, kLavender, Align::Left, it.text);
            else
            {
                const int gc = it.control >= 0 ? on_control[it.control] : -1;
                draw_gc_chip(gc, ix + gc_chip_width(gc, chip_h) * 0.5f, iy, chip_h);
            }
            iy += ih;
        }
    };
    draw_group(kEndL, {{Icon::L1, CtlL1, ""}, {Icon::L2, CtlL2, ""}});
    draw_group(kEndR, {{Icon::R1, CtlR1, ""}, {Icon::R2, CtlR2, ""}});
    draw_group(kEndTouch, {{Icon::TouchPad, CtlTouch, ""}});
    draw_group(kEndCreate, {{Icon::Create, -1, ""}});
    draw_group(kEndOptions, {{Icon::Options, CtlOptions, ""}});
    draw_group(kEndDPad, {{Icon::DPadUp, CtlUp, ""}, {Icon::DPadDown, CtlDown, ""}, {Icon::DPadLeft, CtlLeft, ""},
                          {Icon::DPadRight, CtlRight, ""}});
    draw_group(kEndFace, {{Icon::Triangle, CtlTriangle, ""}, {Icon::Square, CtlSquare, ""},
                          {Icon::Circle, CtlCircle, ""}, {Icon::Cross, CtlCross, ""}});
    draw_group(kEndLStick, {{Icon::LStick, -1, tr("Control stick")}, {Icon::L3, CtlL3, ""}});
    draw_group(kEndRStick, {{Icon::RStick, -1, tr("C-stick")}, {Icon::R3, CtlR3, ""}});
}

void App::draw_game_menu(double time)
{
    if (!menu_play_)
        return;
    Gfx &g = *g_;
    if (map_in_game_)
    {
        g.set_layer();
        draw_mapping(time);
        return;
    }
    const float t = ease_out(menu_anim_);
    Settings &p = *menu_play_;
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.55f * t), 1, 0);

    /* The panel slides in from the left. */
    const float w = 880, x = 40, y = 40, h = 1000;
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
    float hy = y + 34;
    if (menu_game_)
    {
        Texture *cover = cover_of(*menu_game_);
        const float cw = 84, ch = 118, cx = x + 40, cy = hy;
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
        g.text_mid(Font::SemiBold, ts(21), cx + cw + 28, cy + 22, kCyan, Align::Left, tr("PAUSED"), 3.0f);
        const auto lines = wrap(g, Font::Bold, ts(32), menu_game_->title, w - cw - 110, 2);
        float ly = cy + 62;
        for (const std::string &l : lines)
        {
            g.text_mid(Font::Bold, ts(32), cx + cw + 28, ly, kWhite, Align::Left, l);
            ly += 40;
        }
        hy = cy + ch + 24;
    }

    /* The tabs, L1 and R1 at their sides. */
    {
        const float ty = hy + 28, tab_h = 50;
        float widths[kTabCount], total = 0;
        for (int i = 0; i < kTabCount; ++i)
        {
            widths[i] = g.measure(Font::Bold, ts(25), tr(kTabNames[i])) + 44;
            total += widths[i];
        }
        float tx = x + (w - total) * 0.5f;
        g.glyph(Glyph::L1, tx - 44, ty, 40, rgba(0xE8F0FF));
        g.glyph(Glyph::R1, tx + total + 44, ty, 40, rgba(0xE8F0FF));
        for (int i = 0; i < kTabCount; ++i)
        {
            const bool on = i == menu_tab_;
            if (on)
                g.panel(tx, ty - tab_h * 0.5f, widths[i], tab_h, rgba(0x1F63F0), 0.62f, kR, rgba(0x7FD9FF), 1.6f, 6,
                        0.35f);
            g.text_mid(on ? Font::Bold : Font::SemiBold, ts(25), tx + widths[i] * 0.5f, ty, on ? kWhite : kSoft,
                       Align::Center, tr(kTabNames[i]));
            tx += widths[i];
        }
        hy = ty + tab_h * 0.5f + 16;
    }
    g.panel(x + 36, hy, w - 72, 1.5f, rgba(0x6F8FE0, 0.5f), 1, 0);
    hy += 10;

    /* Rows. */
    const std::vector<Row> rows = rows_for(menu_tab_, p);
    const float row_h = 56, rx = x + 24, rw = w - 48;
    float ry = hy;
    for (int i = 0; i < int(rows.size()); ++i)
    {
        const Row &row = rows[std::size_t(i)];
        if (row.gap_before)
        {
            g.panel(x + 36, ry + 6, w - 72, 1.5f, rgba(0x6F8FE0, 0.5f), 1, 0);
            ry += 14;
        }
        const float cy = ry + row_h * 0.5f;
        const bool on = i == menu_row_;
        if (on)
            g.panel(rx, ry + 4, rw, row_h - 8, rgba(0x1D45B8, 0.9f), 0.7f, kR, kIcy, 2.2f, 8, 0.18f);
        g.text_mid(on ? Font::Bold : Font::SemiBold, ts(27), rx + 28, cy, on ? kWhite : kSoft, Align::Left,
                   tr(row.label));
        std::string value;
        bool arrows = true;
        switch (row.kind)
        {
        case Kind::Save:
        case Kind::Load:
            value = trf("Slot {n}", {{"n", std::to_string(menu_slot_ + 1)}});
            break;
        case Kind::FastForward:
            value = menu_ff_ == 0 ? tr("Off") : menu_ff_ == 1 ? "2x" : "4x";
            break;
        case Kind::Int:
            value = tr(row.values[std::size_t(std::clamp(*row.iv - row.min, 0, int(row.values.size()) - 1))]);
            if (row.key == std::string("button_layout") && *row.iv >= LayoutOwn)
                value = trf("My layout {n}", {{"n", std::to_string(*row.iv - LayoutOwn + 1)}});
            break;
        case Kind::Bool:
            value = tr(*row.bv ? "On" : "Off");
            break;
        case Kind::Border:
            value = tr("None");
            for (const porpoise::borders::Border &b : menu_borders_)
                if (b.name == p.border)
                    value = b.built_in ? tr(b.label) : b.label;
            break;
        default:
            arrows = false;
            break;
        }
        if (!value.empty())
        {
            const float right = rx + rw - 24;
            value = fit(g, Font::Bold, ts(25), value, rw * 0.45f);
            g.text_mid(Font::Bold, ts(25), right - (on ? 30 : 0), cy, on ? kWhite : kSoft, Align::Right, value);
            if (on && arrows)
            {
                const float vw = g.measure(Font::Bold, ts(25), value);
                g.glyph(Glyph::Arrow, right - 30 - vw - 22, cy, 16, kCyan, -kPi * 0.5f);
                g.glyph(Glyph::Arrow, right - 8, cy, 16, kCyan, kPi * 0.5f);
            }
        }
        else if (on)
            g.glyph(Glyph::Arrow, rx + rw - 30, cy, 20, kWhite, kPi * 0.5f);
        ry += row_h;
    }

    const Row &focus = rows[std::size_t(std::clamp(menu_row_, 0, int(rows.size()) - 1))];
    float below = ry + 14;
    if (menu_tab_ == kTabGame)
    {
        /* The three slots, with what each one saw. */
        const bool slots_on = focus.kind == Kind::Save || focus.kind == Kind::Load;
        const float sw = 200, sh = 112, sgap = 24, sx0 = x + (w - (sw * 3 + sgap * 2)) * 0.5f;
        for (int i = 0; i < porpoise::states::kSlots; ++i)
        {
            const float sx = sx0 + float(i) * (sw + sgap), sy = below + 4;
            const bool on = slots_on && i == menu_slot_;
            g.panel(sx - 4, sy - 4, sw + 8, sh + 8, rgba(0x07102E, 0.6f), 1, kR * 0.7f,
                    on ? kIcy : rgba(0x5A68A8, 0.8f), on ? 2.6f : 1.4f, on ? 10 : 0);
            if (menu_slot_tex_[i])
            {
                /* Fit the picture inside the card, at its own shape. */
                const float a = float(menu_slot_tex_[i]->width) / float(std::max(1, menu_slot_tex_[i]->height));
                float iw = sw, ih = sw / a;
                if (ih > sh)
                {
                    ih = sh;
                    iw = sh * a;
                }
                g.image(menu_slot_tex_[i], sx + (sw - iw) * 0.5f, sy + (sh - ih) * 0.5f, iw, ih,
                        on ? kWhite : rgba(0xFFFFFF, 0.8f), kR * 0.5f);
            }
            else
                g.text_mid(Font::SemiBold, ts(21), sx + sw * 0.5f, sy + sh * 0.5f, with_alpha(kLavender, 0.8f),
                           Align::Center, menu_slot_used_[i] ? tr("Saved") : tr("Empty"));
            const float lw = g.text_mid(Font::Bold, ts(19), sx + 4, sy + sh + 22, on ? kWhite : kSoft, Align::Left,
                                        trf("Slot {n}", {{"n", std::to_string(i + 1)}}));
            if (menu_slot_used_[i])
            {
                /* The date, and the time when there is room. */
                std::string when = slot_date(menu_slot_time_[i]);
                if (g.measure(Font::Regular, ts(17), when) > sw - lw - 16)
                    when = when.substr(0, when.find("  "));
                g.text_mid(Font::Regular, ts(17), sx + sw - 2, sy + sh + 22, kLavender, Align::Right,
                           fit(g, Font::Regular, ts(17), when, sw - lw - 14));
            }
        }
        below += sh + 44;
    }
    else if (menu_tab_ == kTabControls)
    {
        /* The controller and its buttons, as this game has them. */
        const float aw = 560;
        draw_controller_lines(x + (w - aw) * 0.5f, below + 36, aw, p.mapping());
        below += 36 + kLinesH * aw / kLinesW;
    }

    /* A note after a save or load, else the row's help. */
    {
        const double since = time_ - menu_note_time_;
        const bool note = !menu_note_.empty() && since < 4.0;
        const std::string text = note ? menu_note_ : tr(help_for(focus, p));
        const float ny = std::min(below + 10, y + h - 112);
        const auto lines = wrap(g, Font::Regular, ts(22), text, w - 96, 2);
        float ly = ny;
        for (const std::string &l : lines)
        {
            g.text_mid(note ? Font::SemiBold : Font::Regular, ts(22), x + 48, ly,
                       note ? kCyan : kLavender, Align::Left, l);
            ly += 30;
        }
    }

    /* The prompts, smaller, inside the panel. */
    {
        float px = x + 52;
        const float py = y + h - 50, size = ts(22);
        std::vector<std::pair<Glyph, std::string>> prompts = {{Glyph::Cross, tr("Select")},
                                                              {Glyph::Circle, tr("Resume")}};
        if (focus.kind == Kind::Save)
            prompts[0].second = tr("Save");
        else if (focus.kind == Kind::Load)
            prompts[0].second = tr("Load");
        for (std::size_t i = 0; i < prompts.size(); ++i)
        {
            if (i > 0)
            {
                g.panel(px - 2, py - 16, 1.5f, 32, rgba(0x6F8FE0, 0.6f), 1, 0);
                px += 26;
            }
            g.glyph(prompts[i].first, px + 15, py, 30, kWhite);
            px += 42;
            px += g.text_mid(Font::SemiBold, size, px, py, kWhite, Align::Left, prompts[i].second) + 30;
        }
        /* L1 / R1 tabs, on the right. */
        const std::string tabs = tr("Tabs");
        const float tw = g.measure(Font::SemiBold, size, tabs);
        const float right = x + w - 48;
        g.text_mid(Font::SemiBold, size, right, py, kWhite, Align::Right, tabs);
        if (g.has_icons())
        {
            g.glyph(Glyph::R1, right - tw - 30, py, 30, kWhite);
            g.glyph(Glyph::L1, right - tw - 76, py, 30, kWhite);
        }
    }
    g.set_layer();
}

void App::return_from_game()
{
    menu_free_slots();
    menu_game_ = nullptr;
    menu_play_ = nullptr;
    map_in_game_ = false;
    menu_ff_ = 0;
    launch_ = nullptr;
    screen_ = Screen::Main;
    tab_ = Tab::Library;
    screen_anim_ = 1.0f;
    held_ = prev_ = 0xFFFFFFFFu; /* the button that quit doesn't press anything here */
    cards_scanned_ = false;
    lift_ = 0.4f;
}
} // namespace porpoise::ui
