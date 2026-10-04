/* Porpoise UI - the button-mapping screen: the DualSense, drawn, with the
 * GameCube button each of its controls is, and a list to change them.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings > Controls > Customize buttons (and the same in a game's own
 * settings). Up and down pick a GameCube button; Cross waits for the
 * DualSense control to give it, and the control's old button moves to where
 * the new one was, so every button stays reachable. The last two rows go
 * back to a ready-made layout. Changing a button makes the layout Custom. */
#include <algorithm>
#include <cmath>
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
constexpr int kPresetRows = 2; /* "GameCube layout", "PlayStation layout" after the buttons */
constexpr double kCaptureSeconds = 6.0;

/* The controller art (assets/ui/dualsense.png, tools/make-controller-art.py)
 * is 1400x900; these are its controls' centres there, in Control order, and
 * where each one's label sits (an offset from the centre). */
constexpr float kArtW = 1400, kArtH = 900;
struct Spot
{
    float x, y, lx, ly;
};
constexpr Spot kSpots[CtlCount] = {
    {1010, 406, 0, 64},    /* Cross */
    {1086, 330, 70, 0},    /* Circle */
    {934, 330, -70, 0},    /* Square */
    {1010, 254, 60, -48},  /* Triangle */
    {410, 143, -185, 20},  /* L1 */
    {990, 143, 185, 20},   /* R1 */
    {410, 88, -165, -24},  /* L2 */
    {990, 88, 165, -24},   /* R2 */
    {545, 480, 0, 96},     /* L3 */
    {855, 480, 0, 96},     /* R3 */
    {930, 205, -16, -6},   /* Options */
    {700, 279, 0, 0},      /* Touch pad */
    {390, 264, 0, 0},      /* Up */
    {390, 396, 0, 0},      /* Down */
    {324, 330, 0, 0},      /* Left */
    {456, 330, 0, 0},      /* Right */
};

/* The GameCube buttons: a short label for the chips, a name for the list, and
 * a colour in the spirit of the controller's own. */
struct GcInfo
{
    const char *chip;
    const char *name;
    std::uint32_t colour;
};
constexpr GcInfo kGc[GcCount] = {
    {"A", "A button", 0x34C77B},     {"B", "B button", 0xF2536B},    {"X", "X button", 0xC9D3E6},
    {"Y", "Y button", 0xC9D3E6},     {"Z", "Z button", 0x8E6CFF},    {"L", "L trigger", 0x9AA8C7},
    {"R", "R trigger", 0x9AA8C7},    {"START", "Start", 0x9AA8C7},   {"", "D-pad up", 0x9AA8C7},
    {"", "D-pad down", 0x9AA8C7},    {"", "D-pad left", 0x9AA8C7},   {"", "D-pad right", 0x9AA8C7},
};
/* The arrow's rotation for the D-pad inputs (Glyph::Arrow points up at 0). */
float arrow_rotation(int gc)
{
    switch (gc)
    {
    case GcDown: return kPi;
    case GcLeft: return -kPi * 0.5f;
    case GcRight: return kPi * 0.5f;
    default: return 0;
    }
}

/* How a DualSense control is written on a keycap ("" for the face buttons,
 * which are drawn as their symbols). */
std::string control_label(int c)
{
    switch (c)
    {
    case CtlL1: return "L1";
    case CtlR1: return "R1";
    case CtlL2: return "L2";
    case CtlR2: return "R2";
    case CtlL3: return "L3";
    case CtlR3: return "R3";
    case CtlOptions: return tr("OPTIONS");
    case CtlTouch: return tr("TOUCH PAD");
    case CtlUp: return tr("D-pad up");
    case CtlDown: return tr("D-pad down");
    case CtlLeft: return tr("D-pad left");
    case CtlRight: return tr("D-pad right");
    default: return "";
    }
}

Glyph face_glyph(int c)
{
    switch (c)
    {
    case CtlCircle: return Glyph::Circle;
    case CtlSquare: return Glyph::Square;
    case CtlTriangle: return Glyph::Triangle;
    default: return Glyph::Cross;
    }
}

bool is_face(int c)
{
    return c == CtlCross || c == CtlCircle || c == CtlSquare || c == CtlTriangle;
}

const char *layout_name(int layout)
{
    return layout == LayoutPlayStation ? "PlayStation" : layout == LayoutCustom ? "Custom" : "GameCube";
}
} // namespace

/* ---- opening and saving ------------------------------------------------------------------- */

void App::open_mapping()
{
    const bool game = screen_ == Screen::GameSettings && game_for_;
    map_target_ = game ? &game_ : settings_;
    map_return_ = game ? Screen::GameSettings : Screen::Main;
    map_row_ = 0;
    map_capture_ = false;
    map_note_.clear();
    open_screen(Screen::Mapping);
    sfx(Sound::DetailsFlip);
}

void App::close_mapping()
{
    map_capture_ = false;
    if (map_return_ == Screen::GameSettings)
    {
        build_game_settings();
        open_screen(Screen::GameSettings);
    }
    else
    {
        build_settings();
        open_screen(Screen::Main);
    }
    sfx(Sound::DetailsFlip);
}

void App::save_mapping()
{
    if (map_target_ == &game_ && game_for_)
    {
        for (const std::string &k : Settings::mapping_keys())
            if (std::find(game_keys_.begin(), game_keys_.end(), k) == game_keys_.end())
                game_keys_.push_back(k);
        mkdir((data_dir_ + "/game-settings").c_str(), 0777);
        game_.save_keys(game_settings_path(*game_for_), game_keys_);
        return;
    }
    settings_->save(settings_path_);
}

void App::use_layout(int layout)
{
    Settings &s = *map_target_;
    s.button_layout = layout;
    const Mapping m = preset(layout);
    for (int gc = 0; gc < GcCount; ++gc)
        *s.map_slot(gc) = m.control[gc];
    save_mapping();
    map_note_ = trf("The {layout} layout is on.", {{"layout", tr(layout_name(layout))}});
    map_note_time_ = time_;
}

void App::assign_control(int gc, int control)
{
    Settings &s = *map_target_;
    if (s.button_layout != LayoutCustom)
    {
        /* The first change starts the custom layout from the one in use. */
        const Mapping m = s.mapping();
        for (int i = 0; i < GcCount; ++i)
            *s.map_slot(i) = m.control[i];
        s.button_layout = LayoutCustom;
    }
    const int old = *s.map_slot(gc);
    for (int i = 0; i < GcCount; ++i)
        if (i != gc && *s.map_slot(i) == control)
            *s.map_slot(i) = old; /* the control's old button takes this one's place */
    *s.map_slot(gc) = control;
    save_mapping();
    map_note_ = trf("{button} is now on {control}.",
                    {{"button", tr(kGc[gc].name)},
                     {"control", is_face(control) ? tr(control == CtlCross     ? "Cross"
                                                       : control == CtlCircle ? "Circle"
                                                       : control == CtlSquare ? "Square"
                                                                              : "Triangle")
                                                  : control_label(control)}});
    map_note_time_ = time_;
}

/* ---- input --------------------------------------------------------------------------------- */

App::Action App::update_mapping(bool up, bool down)
{
    const bool global = map_target_ == settings_;
    if (map_capture_)
    {
        /* The press that started the capture doesn't count: wait for every
         * button to be let go, then take the first one pressed. */
        const std::uint32_t mask = 0xFFFFu;
        if (!map_armed_)
        {
            map_armed_ = (raw_held_ & mask) == 0;
            return Action::None;
        }
        const std::uint32_t fresh = (raw_held_ & ~raw_prev_) & mask;
        if (fresh)
            for (int c = 0; c < CtlCount; ++c)
                if (fresh & control_bit(c))
                {
                    assign_control(map_row_, c);
                    map_capture_ = false;
                    sfx(Sound::LaunchGame);
                    return global ? Action::SettingsChanged : Action::None;
                }
        if (time_ - map_capture_start_ > kCaptureSeconds)
        {
            map_capture_ = false;
            map_note_ = tr("Nothing pressed; the button is unchanged.");
            map_note_time_ = time_;
        }
        return Action::None;
    }

    const int rows = GcCount + kPresetRows;
    if (up && map_row_ > 0)
    {
        --map_row_;
        sfx(Sound::MenuScroll);
    }
    if (down && map_row_ + 1 < rows)
    {
        ++map_row_;
        sfx(Sound::MenuScroll);
    }
    if (pressed(BtnCircle))
    {
        close_mapping();
        return Action::None;
    }
    if (pressed(BtnCross))
    {
        if (map_row_ < GcCount)
        {
            map_capture_ = true;
            map_armed_ = false;
            map_capture_start_ = time_;
            sfx(Sound::DetailsFlip);
        }
        else
        {
            use_layout(map_row_ == GcCount ? LayoutGameCube : LayoutPlayStation);
            sfx(Sound::LaunchGame);
            return global ? Action::SettingsChanged : Action::None;
        }
    }
    return Action::None;
}

/* ---- drawing ------------------------------------------------------------------------------- */

void App::draw_keycap(float right, float cy, const std::string &label, bool on, float height)
{
    Gfx &g = *g_;
    const float size = height * 0.52f;
    const float w = std::max(height * 1.5f, g.measure(Font::Bold, size, label) + height * 0.8f);
    g.panel(right - w, cy - height * 0.5f, w, height, rgba(0x07102E, on ? 0.65f : 0.45f), 1, kR * 0.8f,
            on ? rgba(0xE8F2FF) : rgba(0xA9B8E8, 0.85f), 1.8f);
    g.text_mid(Font::Bold, size, right - w * 0.5f, cy, kWhite, Align::Center, label);
}

void App::draw_mapping(double time)
{
    Gfx &g = *g_;
    if (!map_target_)
        return;
    const Settings &s = *map_target_;
    const Mapping m = s.mapping();
    const bool game = map_target_ == &game_;

    /* Left: the controller. */
    const float lx = 90, ly = 136, lw = 830, lh = 800;
    g.panel(lx, ly, lw, lh, rgba(0x0A1236, 0.55f), 0.85f, kR, rgba(0x3D4F9E, 0.9f), 1.6f, 0, 0.10f);
    g.text_mid(Font::Bold, ts(46), lx + 46, ly + 64, kWhite, Align::Left, tr("Buttons"));
    g.text_mid(Font::Regular, ts(26), lx + 46, ly + 112, kLavender, Align::Left,
               game && game_for_ ? fit(g, Font::Regular, ts(26), trf("For {game} only", {{"game", game_for_->title}}),
                                       lw - 92)
                                 : trf("Layout: {layout}", {{"layout", tr(layout_name(s.button_layout))}}));

    if (!pad_art_tried_)
    {
        pad_art_tried_ = true;
        pad_art_ = g.texture_file(g.asset_dir() + "/ui/dualsense.png");
    }
    const float aw = 760, ah = aw * kArtH / kArtW, ax = lx + (lw - aw) * 0.5f, ay = ly + 170;
    const float k = aw / kArtW;
    auto at = [&](float x, float y) { return std::pair<float, float>{ax + x * k, ay + y * k}; };
    if (pad_art_)
        g.image(pad_art_, ax, ay, aw, ah, kWhite);

    /* Which GameCube input is on each control. */
    int on_control[CtlCount];
    std::fill(std::begin(on_control), std::end(on_control), -1);
    for (int gc = 0; gc < GcCount; ++gc)
        if (m.control[gc] >= 0 && m.control[gc] < CtlCount)
            on_control[int(m.control[gc])] = gc;
    const int focus_gc = map_row_ < GcCount ? map_row_ : -1;
    const int focus_ctl = focus_gc >= 0 ? m.control[focus_gc] : -1;
    const float pulse = settings_->reduced_motion ? 1.0f : 0.75f + 0.25f * std::sin(float(time) * 5.0f);

    /* The focused control glows; while waiting for a press, every control does. */
    for (int c = 0; c < CtlCount; ++c)
    {
        const bool lit = map_capture_ ? true : c == focus_ctl;
        if (!lit)
            continue;
        const auto [x, y] = at(kSpots[c].x, kSpots[c].y);
        const float r = map_capture_ ? 34 : 54;
        g.blob(x, y, r * 2, r * 2, rgba(0x5CD3FF, (map_capture_ ? 0.35f : 0.75f) * pulse));
    }
    /* A chip on each control with the GameCube button it is. */
    for (int c = 0; c < CtlCount; ++c)
    {
        const int gc = on_control[c];
        if (gc < 0)
            continue;
        const bool dpad_home = gc >= GcUp && int(m.control[gc]) == CtlUp + (gc - GcUp);
        if (dpad_home && gc != focus_gc)
            continue; /* the D-pad as the D-pad needs no labels */
        const auto [x, y] = at(kSpots[c].x + kSpots[c].lx, kSpots[c].y + kSpots[c].ly);
        const bool on = gc == focus_gc;
        const float h = on ? 40 : 34;
        const std::string label = kGc[gc].chip;
        const float w = label.empty() ? h : std::max(h, g.measure(Font::Bold, h * 0.55f, label) + h * 0.7f);
        g.panel(x - w * 0.5f, y - h * 0.5f, w, h, with_alpha(rgba(kGc[gc].colour), on ? 1.0f : 0.92f), 0.8f, h * 0.5f,
                on ? kWhite : rgba(0xFFFFFF, 0.7f), on ? 2.4f : 1.4f, on ? 10 : 0, 0.3f);
        if (label.empty())
            g.glyph(Glyph::Arrow, x, y, h * 0.5f, rgba(0x0A1236), arrow_rotation(gc));
        else
            g.text_mid(Font::Bold, h * 0.55f, x, y, rgba(0x0A1236), Align::Center, label);
    }
    g.text_mid(Font::Regular, ts(24), lx + lw * 0.5f, ly + lh - 56, kLavender, Align::Center,
               tr("Left stick: control stick   \xE2\x80\xA2   Right stick: C-stick"));

    /* Right: the list. */
    const float px = 950, py = 136, pw = 880, ph = 800;
    g.panel(px, py, pw, ph, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    const float row_h = 46, row_x = px + 22, row_w = pw - 44;
    float y = py + 26;
    for (int i = 0; i < GcCount + kPresetRows; ++i)
    {
        if (i == GcCount)
        {
            g.panel(row_x + 20, y + 8, row_w - 40, 1.5f, rgba(0x3D4F9E, 0.8f), 1, 0);
            y += 18;
        }
        const bool on = i == map_row_;
        const float cy = y + row_h * 0.5f;
        if (on)
            g.panel(row_x, y + 2, row_w, row_h - 4, rgba(0x1D45B8, 0.88f), 0.7f, kR, kIcy, 2.2f, 8, 0.18f);
        if (i < GcCount)
        {
            /* The GameCube button's chip, its name, and its control. */
            const float h = 30, cx = row_x + 40;
            const std::string label = kGc[i].chip;
            const float w = label.empty() ? h : std::max(h, g.measure(Font::Bold, h * 0.55f, label) + h * 0.7f);
            g.panel(cx - w * 0.5f, cy - h * 0.5f, w, h, rgba(kGc[i].colour), 0.8f, h * 0.5f, rgba(0xFFFFFF, 0.6f), 1.2f);
            if (label.empty())
                g.glyph(Glyph::Arrow, cx, cy, h * 0.5f, rgba(0x0A1236), arrow_rotation(i));
            else
                g.text_mid(Font::Bold, h * 0.55f, cx, cy, rgba(0x0A1236), Align::Center, label);
            g.text_mid(Font::SemiBold, ts(27), row_x + 92, cy, on ? kWhite : kSoft, Align::Left, tr(kGc[i].name));
            const int c = m.control[i];
            const float right = row_x + row_w - 22;
            if (on && map_capture_)
                g.text_mid(Font::Bold, ts(25), right, cy, kCyan, Align::Right, tr("Press a button\xE2\x80\xA6"));
            else if (is_face(c))
                g.glyph(face_glyph(c), right - 18, cy, 38, kWhite);
            else
                draw_keycap(right, cy, control_label(c), on, 34);
        }
        else
        {
            const bool gc_preset = i == GcCount;
            const int layout = gc_preset ? LayoutGameCube : LayoutPlayStation;
            g.text_mid(Font::SemiBold, ts(27), row_x + 34, cy, on ? kWhite : kSoft, Align::Left,
                       tr(gc_preset ? "Use the GameCube layout" : "Use the PlayStation layout"));
            g.text_mid(Font::Regular, ts(22), row_x + row_w - 22, cy, with_alpha(kLavender, 0.9f), Align::Right,
                       s.button_layout == layout ? tr("In use")
                                                 : tr(gc_preset ? "Cross A \xE2\x80\xA2 Square B" : "Cross A \xE2\x80\xA2 Circle B"));
        }
        y += row_h;
    }
    /* A line after a change, fading out. */
    const double since = time_ - map_note_time_;
    if (!map_note_.empty() && since < 4.0)
    {
        const float a = since < 3.0 ? 1.0f : float(4.0 - since);
        g.text_mid(Font::SemiBold, ts(24), px + pw * 0.5f, py + ph - 34, with_alpha(kCyan, a), Align::Center,
                   fit(g, Font::SemiBold, ts(24), map_note_, pw - 60));
    }

    if (map_capture_)
    {
        const int left = int(std::ceil(kCaptureSeconds - (time_ - map_capture_start_)));
        draw_prompts({}, {}, trf("Press the DualSense button for {button} ({n})",
                                 {{"button", tr(kGc[map_row_].name)}, {"n", std::to_string(std::max(1, left))}}));
    }
    else if (map_row_ < GcCount)
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Cross, "Change"}, {Glyph::Circle, "Back"}}, {}, "");
    else
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Cross, "Use"}, {Glyph::Circle, "Back"}}, {}, "");
}
} // namespace porpoise::ui
