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
} // namespace porpoise::ui
