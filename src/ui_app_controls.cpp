/* Porpoise UI - the button-mapping screen: the DualSense, drawn, with the
 * GameCube button each of its controls is, and a list to change them.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Settings > Controls > Customize buttons, a game's own settings, and the
 * in-game menu's Controls tab all open it. The player keeps four layouts of
 * their own ("My layout 1".."4", with the global settings); the top row picks
 * which one to edit. Up and down pick a GameCube button; Cross waits for the
 * DualSense control to give it, and the control's old button moves to where
 * the new one was, so every button stays reachable. Changing a button puts
 * that layout to use for whatever opened the screen (every game, or one).
 * The last two rows start the layout over from a ready-made one.
 *
 * The controller is Zacksly's (PS5 Button Icons and Controls, CC BY 3.0). */
#include <algorithm>
#include <cmath>
#include <cstring>
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
/* Rows: the layout to edit, the twelve GameCube inputs, then "start from"
 * the two ready-made layouts. */
constexpr int kSelectRow = 0;
constexpr int kFirstButton = 1;
constexpr int kFromGameCube = kFirstButton + GcCount;
constexpr int kFromPlayStation = kFromGameCube + 1;
constexpr int kMapRows = kFromPlayStation + 1;
constexpr double kCaptureSeconds = 6.0;

/* The controller art (assets/ui/dualsense.png, from Zacksly's drawing by
 * tools/make-controller-art.py) is 1400x900 in these units whatever its
 * pixel size; these are its controls' centres there, in Control order, and
 * where each one's label sits (an offset from the centre). */
constexpr float kArtW = 1400, kArtH = 900;
struct Spot
{
    float x, y, lx, ly;
};
constexpr Spot kSpots[CtlCount] = {
    {1035, 399, 0, 66},     /* Cross */
    {1114, 319, 74, 0},     /* Circle */
    {955, 319, -74, 0},     /* Square */
    {1035, 240, 62, -50},   /* Triangle */
    {372, 140, -190, 22},   /* L1 */
    {1028, 140, 190, 22},   /* R1 */
    {366, 112, -170, -30},  /* L2 */
    {1034, 112, 170, -30},  /* R2 */
    {525, 468, 0, 100},     /* L3 */
    {873, 468, 0, 100},     /* R3 */
    {949, 198, 46, -72},    /* Options */
    {700, 233, 0, 0},       /* Touch pad */
    {363, 250, 0, 0},       /* Up */
    {363, 388, 0, 0},       /* Down */
    {299, 319, 0, 0},       /* Left */
    {428, 319, 0, 0},       /* Right */
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

std::string layout_name(int layout)
{
    if (layout >= LayoutOwn)
        return trf("My layout {n}", {{"n", std::to_string(layout - LayoutOwn + 1)}});
    return tr(layout == LayoutPlayStation ? "PlayStation" : "GameCube");
}

/* A DualSense control's own icon, or its name on a keycap without the atlas. */
Icon control_icon(int c)
{
    switch (c)
    {
    case CtlCross: return Icon::Cross;
    case CtlCircle: return Icon::Circle;
    case CtlSquare: return Icon::Square;
    case CtlTriangle: return Icon::Triangle;
    case CtlL1: return Icon::L1;
    case CtlR1: return Icon::R1;
    case CtlL2: return Icon::L2;
    case CtlR2: return Icon::R2;
    case CtlL3: return Icon::L3;
    case CtlR3: return Icon::R3;
    case CtlOptions: return Icon::Options;
    case CtlTouch: return Icon::TouchPad;
    case CtlUp: return Icon::DPadUp;
    case CtlDown: return Icon::DPadDown;
    case CtlLeft: return Icon::DPadLeft;
    case CtlRight: return Icon::DPadRight;
    default: return Icon::Count;
    }
}

std::string control_name(int c)
{
    if (is_face(c))
        return tr(c == CtlCross ? "Cross" : c == CtlCircle ? "Circle" : c == CtlSquare ? "Square" : "Triangle");
    return control_label(c);
}
} // namespace

/* A GameCube button's coloured chip (an arrow for the D-pad). */
void App::draw_gc_chip(int gc, float cx, float cy, float h, bool on)
{
    Gfx &g = *g_;
    if (gc < 0 || gc >= GcCount)
    {
        g.text_mid(Font::Bold, h * 0.6f, cx, cy, with_alpha(kLavender, 0.7f), Align::Center, "\xE2\x80\x93");
        return;
    }
    const std::string label = kGc[gc].chip;
    const float w = label.empty() ? h : std::max(h, g.measure(Font::Bold, h * 0.55f, label) + h * 0.7f);
    g.panel(cx - w * 0.5f, cy - h * 0.5f, w, h, rgba(kGc[gc].colour), 0.8f, h * 0.5f,
            on ? kWhite : rgba(0xFFFFFF, 0.6f), on ? 2.0f : 1.2f);
    if (label.empty())
        g.glyph(Glyph::Arrow, cx, cy, h * 0.5f, rgba(0x0A1236), arrow_rotation(gc));
    else
        g.text_mid(Font::Bold, h * 0.55f, cx, cy, rgba(0x0A1236), Align::Center, label);
}

float App::gc_chip_width(int gc, float h)
{
    if (gc < 0 || gc >= GcCount || !kGc[gc].chip[0])
        return h;
    return std::max(h, g_->measure(Font::Bold, h * 0.55f, kGc[gc].chip) + h * 0.7f);
}

/* A DualSense control at the right of a row: its icon (Zacksly's), or a keycap. */
void App::draw_control(int control, float right, float cy, bool on, float size)
{
    Gfx &g = *g_;
    const Icon ic = control_icon(control);
    if (g.has_icons() && ic != Icon::Count)
    {
        g.icon(ic, right - size * 0.5f, cy, size, on ? kWhite : rgba(0xDCE6FF));
        return;
    }
    if (is_face(control))
        g.glyph(face_glyph(control), right - 18, cy, size * 0.66f, kWhite);
    else
        draw_keycap(right, cy, control_label(control), on, size * 0.62f);
}

/* ---- opening and saving ------------------------------------------------------------------- */

void App::open_mapping()
{
    const bool game = screen_ == Screen::GameSettings && game_for_;
    map_target_ = game ? &game_ : settings_;
    map_game_ = game ? game_for_ : nullptr;
    map_in_game_ = false;
    map_return_ = game ? Screen::GameSettings : Screen::Main;
    begin_mapping();
    open_screen(Screen::Mapping);
}

void App::open_mapping_in_game()
{
    map_target_ = menu_play_;
    map_game_ = menu_game_;
    map_in_game_ = true;
    begin_mapping();
}

void App::begin_mapping()
{
    /* Edit the layout in use, or the first one. */
    const int own = map_target_ ? map_target_->preset_in_use() : -1;
    map_preset_ = own >= 0 ? own : 0;
    map_row_ = own >= 0 ? kFirstButton : kSelectRow;
    map_capture_ = false;
    map_note_.clear();
    sfx(Sound::DetailsFlip);
}

void App::close_mapping()
{
    map_capture_ = false;
    sfx(Sound::DetailsFlip);
    if (map_in_game_)
    {
        map_in_game_ = false;
        return;
    }
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
}

/* The layouts live with the global settings; which one is in use belongs to
 * whatever opened the screen. */
void App::save_mapping(bool layout_changed)
{
    for (Settings *other : {map_target_, &game_, menu_play_})
        if (other && other != settings_)
            std::memcpy(other->presets, settings_->presets, sizeof settings_->presets);
    if (map_game_ && map_target_ != settings_)
    {
        if (layout_changed)
        {
            const std::string path = game_settings_path(*map_game_);
            std::vector<std::string> keys = Settings::keys_in(path);
            if (std::find(keys.begin(), keys.end(), "button_layout") == keys.end())
                keys.push_back("button_layout");
            if (map_target_ == &game_ &&
                std::find(game_keys_.begin(), game_keys_.end(), "button_layout") == game_keys_.end())
                game_keys_.push_back("button_layout");
            mkdir((data_dir_ + "/game-settings").c_str(), 0777);
            map_target_->save_keys(path, keys);
        }
    }
    settings_->save(settings_path_);
    if (map_in_game_)
        menu_change_ = "button_layout";
}

void App::use_preset(int preset)
{
    const bool changed = map_target_->button_layout != LayoutOwn + preset;
    map_target_->button_layout = LayoutOwn + preset;
    save_mapping(changed);
    map_note_ = map_game_ && map_target_ != settings_
                    ? trf("{layout} is on for this game.", {{"layout", layout_name(LayoutOwn + preset)}})
                    : trf("{layout} is on.", {{"layout", layout_name(LayoutOwn + preset)}});
    map_note_time_ = time_;
}

void App::assign_control(int gc, int control)
{
    int *row = settings_->presets[map_preset_];
    const int old = row[gc];
    for (int i = 0; i < GcCount; ++i)
        if (i != gc && row[i] == control)
            row[i] = old; /* the control's old button takes this one's place */
    row[gc] = control;
    use_preset(map_preset_);
    map_note_ = trf("{button} is now on {control}.", {{"button", tr(kGc[gc].name)}, {"control", control_name(control)}});
    map_note_time_ = time_;
}

void App::start_preset_from(int layout)
{
    const Mapping m = porpoise::pad::preset(layout);
    for (int i = 0; i < GcCount; ++i)
        settings_->presets[map_preset_][i] = m.control[i];
    use_preset(map_preset_);
    map_note_ = trf("{layout} now starts from the {base} layout.",
                    {{"layout", layout_name(LayoutOwn + map_preset_)}, {"base", layout_name(layout)}});
    map_note_time_ = time_;
}

/* ---- input --------------------------------------------------------------------------------- */

App::Action App::update_mapping(bool up, bool down, bool left, bool right)
{
    const bool global = map_target_ == settings_;
    const Action changed = global ? Action::SettingsChanged : Action::None;
    if (map_capture_)
    {
        /* The press that started the capture doesn't count: wait for every
         * button to be let go, then take the first one pressed. */
        const std::uint32_t mask = 0xFFFFu;
        if (time_ - map_capture_start_ > kCaptureSeconds)
        {
            map_capture_ = false;
            map_note_ = tr("Nothing pressed; the button is unchanged.");
            map_note_time_ = time_;
            return Action::None;
        }
        if (!map_armed_)
        {
            map_armed_ = (raw_held_ & mask) == 0;
            if (map_armed_)
                map_capture_start_ = time_; /* the countdown starts once the hands are free */
            return Action::None;
        }
        const std::uint32_t fresh = (raw_held_ & ~raw_prev_) & mask;
        if (fresh)
            for (int c = 0; c < CtlCount; ++c)
                if (fresh & control_bit(c))
                {
                    assign_control(map_row_ - kFirstButton, c);
                    map_capture_ = false;
                    sfx(Sound::LaunchGame);
                    return changed;
                }
        return Action::None;
    }

    if (up && map_row_ > 0)
    {
        --map_row_;
        sfx(Sound::MenuScroll);
    }
    if (down && map_row_ + 1 < kMapRows)
    {
        ++map_row_;
        sfx(Sound::MenuScroll);
    }
    if (map_row_ == kSelectRow && (left || right))
    {
        map_preset_ = (map_preset_ + (left ? Settings::kPresets - 1 : 1)) % Settings::kPresets;
        sfx(Sound::MovingTab);
    }
    if (pressed(BtnCircle))
    {
        close_mapping();
        return Action::None;
    }
    if (pressed(BtnCross))
    {
        if (map_row_ == kSelectRow)
        {
            use_preset(map_preset_);
            sfx(Sound::LaunchGame);
            return changed;
        }
        if (map_row_ >= kFirstButton && map_row_ < kFromGameCube)
        {
            map_capture_ = true;
            map_armed_ = false;
            map_capture_start_ = time_;
            sfx(Sound::DetailsFlip);
        }
        else
        {
            start_preset_from(map_row_ == kFromGameCube ? LayoutGameCube : LayoutPlayStation);
            sfx(Sound::LaunchGame);
            return changed;
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
    /* The screen shows the layout being edited, whichever is in use. */
    Mapping m{};
    for (int i = 0; i < GcCount; ++i)
        m.control[i] = static_cast<std::int8_t>(std::clamp(settings_->presets[map_preset_][i], 0, CtlCount - 1));
    const bool for_game = map_game_ && map_target_ != settings_;
    const bool editing_in_use = s.button_layout == LayoutOwn + map_preset_;
    if (map_in_game_)
        g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.78f), 1, 0);

    /* Left: the controller. */
    const float lx = 90, ly = 136, lw = 830, lh = 800;
    g.panel(lx, ly, lw, lh, rgba(0x0A1236, 0.55f), 0.85f, kR, rgba(0x3D4F9E, 0.9f), 1.6f, 0, 0.10f);
    g.text_mid(Font::Bold, ts(46), lx + 46, ly + 64, kWhite, Align::Left, tr("Buttons"));
    g.text_mid(Font::Regular, ts(26), lx + 46, ly + 112, kLavender, Align::Left,
               fit(g, Font::Regular, ts(26),
                   for_game ? trf("{game}: {layout}", {{"game", map_game_->title}, {"layout", layout_name(s.button_layout)}})
                            : trf("Every game: {layout}", {{"layout", layout_name(s.button_layout)}}),
                   lw - 92));

    if (!pad_art_tried_)
    {
        pad_art_tried_ = true;
        pad_art_ = g.texture_file(g.asset_dir() + "/ui/dualsense.png");
    }
    const float aw = 760, ah = aw * kArtH / kArtW, ax = lx + (lw - aw) * 0.5f, ay = ly + 160;
    const float k = aw / kArtW;
    auto at = [&](float x, float y) { return std::pair<float, float>{ax + x * k, ay + y * k}; };
    if (pad_art_)
        g.image(pad_art_, ax, ay, aw, ah, rgba(0xDDEBFF));

    /* Which GameCube input is on each control. */
    int on_control[CtlCount];
    std::fill(std::begin(on_control), std::end(on_control), -1);
    for (int gc = 0; gc < GcCount; ++gc)
        if (m.control[gc] >= 0 && m.control[gc] < CtlCount)
            on_control[int(m.control[gc])] = gc;
    const int focus_gc = map_row_ >= kFirstButton && map_row_ < kFromGameCube ? map_row_ - kFirstButton : -1;
    const int focus_ctl = focus_gc >= 0 ? m.control[focus_gc] : -1;
    const float pulse = settings_->reduced_motion ? 1.0f : 0.75f + 0.25f * std::sin(float(time) * 5.0f);

    /* The focused control glows; while waiting for a press, every control does. */
    for (int c = 0; c < CtlCount; ++c)
    {
        const bool lit = map_capture_ ? true : c == focus_ctl;
        if (!lit)
            continue;
        const auto [x, y] = at(kSpots[c].x, kSpots[c].y);
        const float r = map_capture_ ? 30 : 46;
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
    {
        /* The sticks, with their icons. */
        const float sy = ly + lh - 56;
        if (g.has_icons())
        {
            const std::string a = tr("Control stick"), b = tr("C-stick");
            const float wa = g.measure(Font::Regular, ts(24), a), wb = g.measure(Font::Regular, ts(24), b);
            const float total = 44 + 10 + wa + 50 + 44 + 10 + wb;
            float x = lx + (lw - total) * 0.5f;
            g.icon(Icon::LStick, x + 22, sy, 50, kLavender);
            x += 54;
            x += g.text_mid(Font::Regular, ts(24), x, sy, kLavender, Align::Left, a) + 50;
            g.icon(Icon::RStick, x + 22, sy, 50, kLavender);
            g.text_mid(Font::Regular, ts(24), x + 54, sy, kLavender, Align::Left, b);
        }
        else
            g.text_mid(Font::Regular, ts(24), lx + lw * 0.5f, sy, kLavender, Align::Center,
                       tr("Left stick: control stick   \xE2\x80\xA2   Right stick: C-stick"));
    }

    /* Right: the list. */
    const float px = 950, py = 136, pw = 880, ph = 800;
    g.panel(px, py, pw, ph, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    const float row_h = 44, row_x = px + 22, row_w = pw - 44;
    float y = py + 22;
    for (int i = 0; i < kMapRows; ++i)
    {
        if (i == kFirstButton || i == kFromGameCube)
        {
            g.panel(row_x + 20, y + 8, row_w - 40, 1.5f, rgba(0x3D4F9E, 0.8f), 1, 0);
            y += 18;
        }
        const bool on = i == map_row_;
        const float h_row = i == kSelectRow ? row_h + 12 : row_h;
        const float cy = y + h_row * 0.5f;
        if (on)
            g.panel(row_x, y + 2, row_w, h_row - 4, rgba(0x1D45B8, 0.88f), 0.7f, kR, kIcy, 2.2f, 8, 0.18f);
        const float right = row_x + row_w - 22;
        if (i == kSelectRow)
        {
            /* ◀ My layout N ▶, and whether it's the one in use. */
            g.text_mid(Font::SemiBold, ts(27), row_x + 34, cy, on ? kWhite : kSoft, Align::Left, tr("Layout"));
            const std::string name = layout_name(LayoutOwn + map_preset_);
            const float nw = g.measure(Font::Bold, ts(28), name);
            const float ncx = row_x + row_w * 0.5f + 40;
            g.text_mid(Font::Bold, ts(28), ncx, cy, kWhite, Align::Center, name);
            if (on)
            {
                g.glyph(Glyph::Arrow, ncx - nw * 0.5f - 28, cy, 18, kCyan, -kPi * 0.5f);
                g.glyph(Glyph::Arrow, ncx + nw * 0.5f + 28, cy, 18, kCyan, kPi * 0.5f);
            }
            if (editing_in_use)
            {
                const std::string in_use = tr("In use");
                const float bw = g.measure(Font::Bold, ts(20), in_use) + 30;
                g.panel(right - bw, cy - 16, bw, 32, rgba(0x2FB574, 0.85f), 0.8f, 16, rgba(0xBDF5D8, 0.9f), 1.4f);
                g.text_mid(Font::Bold, ts(20), right - bw * 0.5f, cy, kWhite, Align::Center, in_use);
            }
        }
        else if (i < kFromGameCube)
        {
            /* The GameCube button's chip, its name, and its control. */
            const int gc = i - kFirstButton;
            const float h = 30, cx = row_x + 40;
            const std::string label = kGc[gc].chip;
            const float w = label.empty() ? h : std::max(h, g.measure(Font::Bold, h * 0.55f, label) + h * 0.7f);
            g.panel(cx - w * 0.5f, cy - h * 0.5f, w, h, rgba(kGc[gc].colour), 0.8f, h * 0.5f, rgba(0xFFFFFF, 0.6f), 1.2f);
            if (label.empty())
                g.glyph(Glyph::Arrow, cx, cy, h * 0.5f, rgba(0x0A1236), arrow_rotation(gc));
            else
                g.text_mid(Font::Bold, h * 0.55f, cx, cy, rgba(0x0A1236), Align::Center, label);
            g.text_mid(Font::SemiBold, ts(27), row_x + 92, cy, on ? kWhite : kSoft, Align::Left, tr(kGc[gc].name));
            if (on && map_capture_)
                g.text_mid(Font::Bold, ts(25), right, cy, kCyan, Align::Right, tr("Press a button\xE2\x80\xA6"));
            else
                draw_control(m.control[gc], right, cy, on, 54);
        }
        else
        {
            const bool from_gc = i == kFromGameCube;
            g.text_mid(Font::SemiBold, ts(26), row_x + 34, cy, on ? kWhite : kSoft, Align::Left,
                       tr(from_gc ? "Start over from the GameCube layout" : "Start over from the PlayStation layout"));
            g.text_mid(Font::Regular, ts(21), right, cy, with_alpha(kLavender, 0.9f), Align::Right,
                       tr(from_gc ? "Circle A \xE2\x80\xA2 Cross B" : "Cross A \xE2\x80\xA2 Circle B"));
        }
        y += h_row;
    }
    /* A line after a change, fading out. */
    const double since = time_ - map_note_time_;
    if (!map_note_.empty() && since < 4.0)
    {
        const float a = since < 3.0 ? 1.0f : float(4.0 - since);
        g.text_mid(Font::SemiBold, ts(24), px + pw * 0.5f, py + ph - 32, with_alpha(kCyan, a), Align::Center,
                   fit(g, Font::SemiBold, ts(24), map_note_, pw - 60));
    }

    if (map_capture_)
    {
        const int left = int(std::ceil(kCaptureSeconds - (time_ - map_capture_start_)));
        draw_prompts({}, {}, trf("Press the DualSense button for {button} ({n})",
                                 {{"button", tr(kGc[map_row_ - kFirstButton].name)},
                                  {"n", std::to_string(std::max(1, left))}}));
    }
    else if (map_row_ == kSelectRow)
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Cross, "Use this layout"}, {Glyph::Circle, "Back"}}, {}, "");
    else if (map_row_ < kFromGameCube)
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Cross, "Change"}, {Glyph::Circle, "Back"}}, {}, "");
    else
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Cross, "Start over"}, {Glyph::Circle, "Back"}}, {}, "");
}

/* ---- a Wii game's controls ------------------------------------------------------------- */

namespace
{
/* A Wii input's chip text and its name in the list. */
struct WiiInfo
{
    const char *chip;
    const char *name;
    std::uint32_t colour;
};
constexpr std::uint32_t kWiiWhite = 0xEEF3FF, kWiiBlue = 0x7FD9FF, kWiiAmber = 0xFFC85C, kWiiGreen = 0x6BE3A8;
constexpr WiiInfo kWiiInfo[WiInputCount] = {
    {"A", "A button", kWiiBlue},
    {"B", "B, the trigger", kWiiBlue},
    {"1", "1 button", kWiiWhite},
    {"2", "2 button", kWiiWhite},
    {"-", "Minus", kWiiWhite},
    {"+", "Plus", kWiiWhite},
    {"HOME", "HOME button", kWiiWhite},
    {"", "D-pad up", kWiiWhite},
    {"", "D-pad down", kWiiWhite},
    {"", "D-pad left", kWiiWhite},
    {"", "D-pad right", kWiiWhite},
    {"SHAKE", "Shakes the Remote", kWiiAmber},
    {"C", "Nunchuk C", kWiiWhite},
    {"Z", "Nunchuk Z", kWiiWhite},
    {"SHAKE", "Shakes the Nunchuk", kWiiAmber},
    {"a", "a button", kWiiBlue},
    {"b", "b button", kWiiBlue},
    {"x", "x button", kWiiWhite},
    {"y", "y button", kWiiWhite},
    {"ZL", "ZL button", kWiiWhite},
    {"ZR", "ZR button", kWiiWhite},
    {"L", "L trigger", kWiiWhite},
    {"R", "R trigger", kWiiWhite},
    {"CENTRE", "Hold: centre and level", kWiiGreen},
};
float wii_arrow(int input)
{
    switch (input)
    {
    case WiDown: return kPi;
    case WiLeft: return -kPi * 0.5f;
    case WiRight: return kPi * 0.5f;
    default: return 0;
    }
}
const char *stick_name(int role)
{
    switch (role)
    {
    case StickDPad: return "D-pad";
    case StickNunchuk: return "Nunchuk stick";
    case StickClassicLeft: return "Left stick";
    case StickClassicRight: return "Right stick";
    case StickPointer: return "Aims the pointer";
    case StickTilt: return "Tilts the Remote";
    default: return "";
    }
}
const char *wii_controller_name(int controller)
{
    switch (controller)
    {
    case WiiRemote: return "Remote";
    case WiiSideways: return "Remote sideways";
    case WiiClassic: return "Classic Controller";
    case WiiTwoControllers: return "Two controllers (beta)";
    default: return "Remote + Nunchuk";
    }
}
} // namespace

namespace
{
const char *pose_words(int pose)
{
    switch (pose)
    {
    case PoseTriggerRight: return "Upright in your right hand, the trigger edge toward the TV";
    case PoseTriggerLeft: return "Upright in your left hand, the trigger edge toward the TV";
    case PoseFacingRight: return "Upright in your right hand, its face toward you";
    case PoseFacingLeft: return "Upright in your left hand, its face toward you";
    default: return "Face up, in both hands";
    }
}
} // namespace

/* One DualSense, drawn as held, with each Wii input on its control. (bx, by)
 * is the top left of its box; art_h the art's long side on screen. Returns
 * the box's width and height. */
std::pair<float, float> App::draw_wii_pad(const WiiLayout &lay, int pose, float bx, float by, float art_h,
                                          float alpha)
{
    Gfx &g = *g_;
    const bool right_up = pose == PoseTriggerRight || pose == PoseFacingRight;
    const bool left_up = pose == PoseTriggerLeft || pose == PoseFacingLeft;
    const bool upright = right_up || left_up;
    const float k = art_h / kArtW;
    const float bw = upright ? kArtH * k : kArtW * k, bh = upright ? kArtW * k : kArtH * k;
    /* Art units -> screen: the right grip up is a quarter turn anticlockwise. */
    auto at = [&](float u, float v) -> std::pair<float, float> {
        if (right_up)
            return {bx + v * k, by + (kArtW - u) * k};
        if (left_up)
            return {bx + (kArtH - v) * k, by + u * k};
        return {bx + u * k, by + v * k};
    };
    if (!pad_art_tried_)
    {
        pad_art_tried_ = true;
        pad_art_ = g.texture_file(g.asset_dir() + "/ui/dualsense.png");
    }
    if (pad_art_)
    {
        const Color tint = with_alpha(rgba(0xDDEBFF), alpha);
        if (!upright)
            g.image(pad_art_, bx, by, bw, bh, tint);
        else
        {
            const auto tl = at(0, 0), tr_ = at(kArtW, 0), br = at(kArtW, kArtH), bl = at(0, kArtH);
            const Corner c[4] = {{tl.first, tl.second, 1}, {tr_.first, tr_.second, 1}, {br.first, br.second, 1},
                                 {bl.first, bl.second, 1}};
            g.quad3d(pad_art_, c, kArtW * k, kArtH * k, tint, 0, false, false);
        }
    }
    const float chip_h = std::clamp(52 * k / 0.48f, 24.0f, 44.0f);
    /* The shoulder buttons sit on the edge, one behind the other: their tags
     * go outside the outline (art units), joined to the button by a line. */
    auto shoulder = [](int c, float &u, float &v) {
        switch (c)
        {
        case CtlL2: u = 300; v = 18; return true;
        case CtlL1: u = 110; v = 128; return true;
        case CtlR2: u = 1100; v = 18; return true;
        case CtlR1: u = 1290; v = 128; return true;
        default: return false;
        }
    };
    for (int i = 0; i < lay.count; ++i)
    {
        const WiiBinding &b = lay.binds[i];
        if (b.control < 0 || b.control >= CtlCount)
            continue;
        float u, v;
        if (shoulder(b.control, u, v))
        {
            const auto [sx, sy] = at(kSpots[b.control].x, kSpots[b.control].y - 14);
            const auto [tx, ty] = at(u, v);
            const float dx = tx - sx, dy = ty - sy, len = std::sqrt(dx * dx + dy * dy);
            if (len > 1)
            {
                const float nx = -dy / len * 1.3f, ny = dx / len * 1.3f;
                const Corner c[4] = {{sx + nx, sy + ny, 1}, {tx + nx, ty + ny, 1}, {tx - nx, ty - ny, 1},
                                     {sx - nx, sy - ny, 1}};
                g.quad3d(nullptr, c, len, 2.6f, with_alpha(rgba(0x8BD9FF), 0.85f * alpha), 0, false, false);
            }
            g.blob(sx, sy, 18, 18, with_alpha(rgba(0x8BD9FF), 0.9f * alpha));
            const WiiInfo &info = kWiiInfo[b.input];
            const std::string label = info.chip;
            const float cw = label.size() <= 1 ? chip_h : g.measure(Font::Bold, chip_h * 0.46f, label) + chip_h * 0.8f;
            const float pw = cw + chip_h + 14;
            g.panel(tx - pw * 0.5f, ty - chip_h * 0.5f - 5, pw, chip_h + 10, with_alpha(rgba(0x13308A), 0.96f * alpha),
                    0.8f, (chip_h + 10) * 0.5f, with_alpha(rgba(0x8BD9FF), 0.9f * alpha), 1.4f);
            if (g.has_icons())
                g.icon(control_icon(b.control), tx - pw * 0.5f + chip_h * 0.5f + 6, ty, chip_h + 4,
                       with_alpha(rgba(0xDCE8FF), alpha));
            wii_chip(b.input, tx + pw * 0.5f - cw * 0.5f - 6, ty, chip_h, alpha);
            continue;
        }
        const auto [cx, cy] = at(kSpots[b.control].x, kSpots[b.control].y);
        wii_chip(b.input, cx, cy, chip_h, alpha);
    }
    /* The sticks: a word just below each, clear of its L3 / R3 chip. */
    auto stick_tag = [&](int role, float u, float v) {
        if (role == StickNothing)
            return;
        const auto [cx, sy] = at(u, v);
        const float cy = sy + chip_h * 1.2f;
        const std::string t = tr(stick_name(role));
        const float tw = g.measure(Font::SemiBold, ts(20), t) + 24;
        g.panel(cx - tw * 0.5f, cy - 16, tw, 32, with_alpha(rgba(0x13308A), 0.94f * alpha), 0.8f, 16,
                with_alpha(rgba(0x8BD9FF), 0.9f * alpha), 1.4f);
        g.text_mid(Font::SemiBold, ts(20), cx, cy, with_alpha(kWhite, alpha), Align::Center, t);
    };
    stick_tag(lay.left_stick, 525, 468);
    stick_tag(lay.right_stick, 873, 468);
    return {bw, bh};
}

/* A Wii input's chip: its letter, an arrow, or a drawn - / +. */
void App::wii_chip(int input, float cx, float cy, float ch, float alpha)
{
    Gfx &g = *g_;
    const WiiInfo &info = kWiiInfo[input];
    const std::string label = info.chip;
    const float cw = label.size() <= 1 ? ch : g.measure(Font::Bold, ch * 0.46f, label) + ch * 0.8f;
    g.panel(cx - cw * 0.5f, cy - ch * 0.5f, cw, ch, with_alpha(rgba(info.colour), 0.97f * alpha), 0.8f, ch * 0.5f,
            with_alpha(kWhite, 0.85f * alpha), 1.6f, 6, 0.35f);
    const Color ink = with_alpha(rgba(0x0A1236), alpha);
    if (label.empty())
        g.glyph(Glyph::Arrow, cx, cy, ch * 0.5f, ink, wii_arrow(input));
    else if (input == WiMinus || input == WiPlus)
    {
        const float bar = ch * 0.40f, th = std::max(2.0f, ch * 0.10f);
        g.panel(cx - bar * 0.5f, cy - th * 0.5f, bar, th, ink, 1, th * 0.5f);
        if (input == WiPlus)
            g.panel(cx - th * 0.5f, cy - bar * 0.5f, th, bar, ink, 1, th * 0.5f);
    }
    else
        g.text_mid(Font::Bold, label.size() == 1 ? ch * 0.62f : ch * 0.46f, cx, cy, ink, Align::Center, label);
}

/* The list beside or below a drawing. Plain buttons go in a grid of
 * "control -> Wii button" pairs; the rest (shake, centring, the D-pad and the
 * sticks) get a line with words. Controls with the same job share an entry. */
float App::draw_wii_list(const WiiLayout &lay, float lx, float ly, float lw, int columns, float alpha)
{
    Gfx &g = *g_;
    struct Entry
    {
        std::vector<Icon> icons;
        int input = -1;
        std::string text;
    };
    std::vector<Entry> plain, worded;
    auto add = [&](Icon icon, int input, const std::string &text) {
        std::vector<Entry> &list = text.empty() ? plain : worded;
        for (Entry &e : list)
            if (e.input == input && e.text == text && e.icons.size() < 3)
            {
                e.icons.push_back(icon);
                return;
            }
        list.push_back({{icon}, input, text});
    };
    bool dpad_listed = false;
    for (int i = 0; i < lay.count; ++i)
    {
        const WiiBinding &b = lay.binds[i];
        const bool arrow_home = b.input >= WiUp && b.input <= WiRight && b.control == CtlUp + (b.input - WiUp);
        if (arrow_home)
        {
            if (!dpad_listed)
                add(Icon::DPad, -1, tr("D-pad"));
            dpad_listed = true;
            continue;
        }
        const bool simple = b.input <= WiHome || b.input == WiC || b.input == WiZ || (b.input >= WiClA && b.input <= WiClR);
        add(control_icon(b.control), b.input, simple ? std::string() : tr(kWiiInfo[b.input].name));
    }
    if (lay.left_stick != StickNothing)
        add(Icon::LStick, -1, tr(stick_name(lay.left_stick)));
    if (lay.right_stick != StickNothing)
        add(Icon::RStick, -1, tr(stick_name(lay.right_stick)));

    const float icon = 38, ch = 30, row_h = 44;
    auto chip_w = [&](int input) {
        const std::string label = kWiiInfo[input].chip;
        return label.size() <= 1 ? ch : g.measure(Font::Bold, ch * 0.46f, label) + ch * 0.8f;
    };
    auto draw_icons = [&](const Entry &e, float x, float y) {
        for (std::size_t k = 0; k < e.icons.size(); ++k)
        {
            if (k > 0)
            {
                g.text_mid(Font::Regular, ts(20), x - 2, y, with_alpha(kLavender, alpha), Align::Left, "/");
                x += 12;
            }
            if (g.has_icons())
                g.icon(e.icons[k], x + icon * 0.5f, y, icon, with_alpha(rgba(0xDCE8FF), alpha));
            x += icon + 4;
        }
        return x;
    };
    /* The grid: as many pairs to a row as fit. */
    float x = lx, y = ly;
    const float cell = std::max(124.0f, lw / float(std::max(1, columns * 3)));
    for (const Entry &e : plain)
    {
        const float need = float(e.icons.size()) * (icon + 16) + 26 + chip_w(e.input);
        const float take = std::ceil(need / cell) * cell;
        if (x > lx && x + take > lx + lw + 1)
        {
            x = lx;
            y += row_h;
        }
        float cx = draw_icons(e, x, y);
        g.glyph(Glyph::Arrow, cx + 8, y, 12, with_alpha(kLavender, 0.8f * alpha), kPi * 0.5f);
        cx += 22;
        wii_chip(e.input, cx + chip_w(e.input) * 0.5f, y, ch, alpha);
        x += take;
    }
    if (!plain.empty())
        y += row_h;
    /* The worded ones, in columns. */
    const float col_w = lw / float(columns);
    int n = 0;
    for (const Entry &e : worded)
    {
        const int col = n % columns, row = n / columns;
        ++n;
        const float ix = lx + col * col_w, iy = y + row * row_h;
        float tx = draw_icons(e, ix, iy) + 8;
        if (e.input >= 0)
        {
            wii_chip(e.input, tx + chip_w(e.input) * 0.5f, iy, ch, alpha);
            tx += chip_w(e.input) + 12;
        }
        g.text_mid(Font::SemiBold, ts(23), tx, iy, with_alpha(kSoft, alpha), Align::Left,
                   fit(g, Font::SemiBold, ts(23), e.text, ix + col_w - tx - 8));
    }
    return (y - ly) + float((n + columns - 1) / columns) * row_h;
}

void App::draw_wii_controls(float x, float y, float w, float h, const WiiConfig &wii, float alpha, int live_pose,
                            int live_pose_second)
{
    Gfx &g = *g_;
    if (alpha <= 0.01f)
        return;
    g.panel(x, y, w, h, rgba(0x0A1236, 0.86f * alpha), 0.85f, kR, with_alpha(rgba(0x3D4F9E), 0.9f * alpha), 1.6f, 0,
            0.10f);
    const bool two = wii.controller == WiiTwoControllers;
    const int pose = live_pose >= 0 ? live_pose : expected_pose(wii, false);
    const std::string auto_word = wii.grip == GripAuto ? tr("Auto: ") : "";
    g.text_mid(Font::Bold, ts(40), x + 40, y + 52, with_alpha(kWhite, alpha), Align::Left,
               tr(wii_controller_name(wii.controller)));

    if (two)
    {
        /* Two DualSenses, mirror images: the Nunchuk on the left, the Remote on the right. */
        const int pose2 = live_pose_second >= 0 ? live_pose_second : expected_pose(wii, true);
        g.text_mid(Font::Regular, ts(23), x + 40, y + 96, with_alpha(kLavender, alpha), Align::Left,
                   fit(g, Font::Regular, ts(23), auto_word + tr("one in each hand, both stood on end"), w - 80));
        const WiiLayout left = wii_layout(wii, true, pose2), right = wii_layout(wii, false, pose);
        const float art_h = std::min(400.0f, h * 0.38f);
        const float pw = kArtH * art_h / kArtW; /* an upright pad's width */
        const float gap = (w - 2 * pw) / 3;
        const float ay = y + 140;
        draw_wii_pad(left, pose2, x + gap, ay, art_h, alpha);
        draw_wii_pad(right, pose, x + 2 * gap + pw, ay, art_h, alpha);
        const float hy = ay + art_h + 30;
        g.text_mid(Font::Bold, ts(24), x + 40, hy, with_alpha(kWhite, alpha), Align::Left, tr("Left hand: the Nunchuk"));
        g.text_mid(Font::Bold, ts(24), x + w * 0.5f + 10, hy, with_alpha(kWhite, alpha), Align::Left,
                   tr("Right hand: the Remote"));
        draw_wii_list(left, x + 40, hy + 50, w * 0.5f - 60, 1, alpha);
        draw_wii_list(right, x + w * 0.5f + 10, hy + 50, w * 0.5f - 50, 1, alpha);
    }
    else
    {
        g.text_mid(Font::Regular, ts(23), x + 40, y + 96, with_alpha(kLavender, alpha), Align::Left,
                   fit(g, Font::Regular, ts(23), auto_word + tr(pose_words(pose)), w - 80));
        const WiiLayout lay = wii_layout(wii, false, pose);
        const bool upright = pose != PoseFlat;
        if (upright)
        {
            const float art_h = std::min(h - 230.0f, 620.0f);
            const bool right_up = pose == PoseTriggerRight || pose == PoseFacingRight;
            const auto [bw, bh] = draw_wii_pad(lay, pose, x + (right_up ? 80 : 30), y + 150, art_h, alpha);
            (void)bh;
            const float lx = x + (right_up ? 80 : 30) + bw + (right_up ? 60 : 140);
            draw_wii_list(lay, lx, y + 160, x + w - 30 - lx, 1, alpha);
        }
        else
        {
            const float art_w = std::min({w - 120.0f, 680.0f, (h - 420.0f) * kArtW / kArtH});
            const float art_h = art_w; /* the long side */
            const auto [bw, bh] = draw_wii_pad(lay, pose, x + (w - art_w) * 0.5f, y + 130, art_h, alpha);
            (void)bw;
            draw_wii_list(lay, x + 50, y + 130 + bh + 30, w - 100, 2, alpha);
        }
    }

    /* How the motion works. */
    std::string how;
    const bool held = wii.controller != WiiSideways && wii.controller != WiiClassic;
    if (wii.pointer == PointerGyro && held)
        how = pose_left_hand(pose) ? tr("Point at the screen to aim. Hold L1 a moment to centre the pointer and level the hold.")
                                   : tr("Point at the screen to aim. Hold R1 a moment to centre the pointer and level the hold.");
    else if (wii.pointer == PointerTouch && held)
        how = tr("Slide a finger on the touch pad to aim. Flick the controller to shake.");
    else
        how = tr("Tilt and flick the controller: the game feels it.");
    const auto lines = wrap(g, Font::Regular, ts(23), how, w - 80, 2);
    float hy = y + h - 34 - 30 * float(lines.size() - 1);
    for (const std::string &l : lines)
    {
        g.text_mid(Font::Regular, ts(23), x + 40, hy, with_alpha(kLavender, alpha), Align::Left, l);
        hy += 30;
    }
}

/* ---- Settings > Wii Remote > How to hold it ------------------------------------------- */

namespace
{
const char *wii_howto(int controller)
{
    switch (controller)
    {
    case WiiRemote:
        return "Stand the DualSense on end in your right hand with the trigger edge toward the TV: R2 under your "
               "index finger is B, Cross under your thumb is A. Or hold it face up in both hands; with Grip on Auto, "
               "Porpoise reads which. To start, hold it the way you'll play, point at the middle of the "
               "screen and hold R1 for a moment: that hold becomes the Remote held level. Do it again whenever the "
               "pointer wanders.";
    case WiiSideways:
        return "Hold the DualSense face up in both hands, like an NES pad. Tilt it left and right to steer. 1 and 2 "
               "are Square and Cross.";
    case WiiClassic:
        return "Hold it as a normal pad: the buttons are where a Classic Controller has them.";
    case WiiTwoControllers:
        return "One DualSense in each hand, both stood on end with their faces toward you. The right one is the "
               "Remote (its grips pointing right): aim with its back. Holding R1 a moment centres the pointer and levels both controllers as you hold them. The left one is the Nunchuk (its "
               "grips pointing left): its stick, C and Z, and its motion. Both feel the game's rumble. The second "
               "controller needs a second signed-in user.";
    default:
        return "Hold the DualSense face up in both hands. The right half is the Remote, the left half the Nunchuk: "
               "the left stick is its stick, L1 is C, L2 is Z. Point the controller at the screen to aim; hold R1 a moment to centre.";
    }
}
} // namespace

void App::open_wii_guide()
{
    const bool game = screen_ == Screen::GameSettings && game_for_;
    guide_target_ = game ? &game_ : settings_;
    guide_return_ = game ? Screen::GameSettings : Screen::Main;
    guide_controller_ = std::clamp(guide_target_->wii_controller, 0, int(WiiControllerCount) - 1);
    sfx(Sound::DetailsFlip);
    open_screen(Screen::WiiGuide);
}

App::Action App::update_wii_guide(bool left, bool right)
{
    if (left || right)
    {
        guide_controller_ = (guide_controller_ + (left ? WiiControllerCount - 1 : 1)) % WiiControllerCount;
        sfx(Sound::MovingTab);
    }
    if (pressed(BtnCircle) || pressed(BtnCross))
    {
        sfx(Sound::DetailsFlip);
        if (guide_return_ == Screen::GameSettings)
        {
            build_game_settings();
            open_screen(Screen::GameSettings);
        }
        else
            open_screen(Screen::Main);
    }
    return Action::None;
}

void App::draw_wii_guide(double)
{
    Gfx &g = *g_;
    if (!guide_target_)
        return;
    WiiConfig wii = guide_target_->wii_config(true);
    wii.controller = guide_controller_;
    /* Left: what it is and how to hold it; right: the drawing. */
    const float lx = 90, ly = 136, lw = 750, lh = 800;
    g.panel(lx, ly, lw, lh, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    g.text_mid(Font::Bold, ts(46), lx + 44, ly + 64, kWhite, Align::Left, tr("How to hold it"));
    {
        /* Beta, plainly. */
        const std::string beta = tr("Beta");
        const float bw = g.measure(Font::Bold, ts(20), beta) + 28;
        const float bx = lx + 44 + g.measure(Font::Bold, ts(46), tr("How to hold it")) + 20;
        g.panel(bx, ly + 64 - 17, bw, 34, rgba(0xFFC85C, 0.9f), 0.8f, 17, rgba(0xFFE7B0), 1.4f);
        g.text_mid(Font::Bold, ts(20), bx + bw * 0.5f, ly + 64, rgba(0x2A1A00), Align::Center, beta);
    }
    /* The controller, chosen with left / right. */
    const std::string name = tr(wii_controller_name(guide_controller_));
    const float ny = ly + 140;
    g.panel(lx + 40, ny - 30, lw - 80, 60, rgba(0x1D45B8, 0.88f), 0.7f, kR, kIcy, 2.2f, 8, 0.18f);
    g.text_mid(Font::Bold, ts(30), lx + lw * 0.5f, ny, kWhite, Align::Center, name);
    const float nw = g.measure(Font::Bold, ts(30), name);
    g.glyph(Glyph::Arrow, lx + lw * 0.5f - nw * 0.5f - 34, ny, 20, kCyan, -kPi * 0.5f);
    g.glyph(Glyph::Arrow, lx + lw * 0.5f + nw * 0.5f + 34, ny, 20, kCyan, kPi * 0.5f);
    const bool in_use = guide_controller_ == guide_target_->wii_controller;
    g.text_mid(Font::Regular, ts(22), lx + lw * 0.5f, ny + 52, in_use ? kCyan : kLavender, Align::Center,
               in_use ? tr("The Wii controller you have chosen") : tr("Another Wii controller, to look at"));
    const auto lines = wrap(g, Font::Regular, ts(27), tr(wii_howto(guide_controller_)), lw - 90, 16);
    float ty = ny + 120;
    for (const std::string &l : lines)
    {
        g.text_mid(Font::Regular, ts(27), lx + 44, ty, kSoft, Align::Left, l);
        ty += 40;
    }
    {
        const auto beta = wrap(g, Font::Regular, ts(23),
                               tr("Wii motion controls are in beta: still being tuned. Your logs in the debug folder "
                                  "help."),
                               lw - 90, 3);
        float by = ly + lh - 40 - 32 * float(beta.size() - 1);
        for (const std::string &l : beta)
        {
            g.text_mid(Font::Regular, ts(23), lx + 44, by, kLavender, Align::Left, l);
            by += 32;
        }
    }
    draw_wii_controls(870, 120, 960, 850, wii, 1.0f);
    draw_prompts({{Glyph::DPad, "Other Wii controllers"}, {Glyph::Circle, "Back"}}, {}, "");
}

} // namespace porpoise::ui
