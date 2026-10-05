/* Porpoise UI - the themes and their colours.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_theme.hpp"

#include <algorithm>

#include "ui_settings.hpp"

namespace porpoise::ui
{
namespace
{
Color rgb(float r, float g, float b)
{
    return Color{r, g, b, 1.0f};
}

Theme make_porpoise()
{
    Theme t;
    t.name = "Porpoise";
    t.about = "Sapphire glass in a grid room, the boxes in a cover flow.";
    t.look.background = 0;
    t.look.panel_style = 1;
    t.radius = 24;
    t.pills = true;
    t.palettes = true;
    return t;
}

Theme make_revolution()
{
    Theme t;
    t.name = "Revolution";
    t.about = "A bright grid of game tiles you point at by moving the controller, with the date and time below.";
    t.radius = 14;
    t.prompts = 0.9f;
    return t;
}

Theme make_midnight()
{
    Theme t;
    t.name = "OLED";
    t.about = "True black for OLED screens: crisp text, no glow, colour only where it matters.";
    t.look.background = 1;
    t.look.dark = 0.12f;
    t.look.fill_alpha = 1.5f;
    t.look.saturation = 0.9f;
    t.look.flat = true;
    t.radius = 22;
    t.pills = true;
    t.palettes = true;
    return t;
}

Theme make_minimal()
{
    Theme t;
    t.name = "Minimal";
    t.about = "Quiet charcoal and soft grey. Nothing on screen but what you need.";
    t.look.background = 2;
    t.look.light0 = rgb(0.085f, 0.090f, 0.100f);
    t.look.light1 = rgb(0.040f, 0.043f, 0.050f);
    t.look.saturation = 0.12f;
    t.look.dark = 0.85f;
    t.look.fill_alpha = 1.25f;
    t.look.flat = true;
    t.radius = 16;
    t.prompts = 0.78f;
    return t;
}

Theme make_cube()
{
    Theme t;
    t.name = "Cube";
    t.about = "Indigo and violet on black: a glass cube turns over a grid that never ends.";
    t.look.background = 3;
    t.look.panel_style = 1;
    t.look.light0 = rgb(0.42f, 0.22f, 0.95f);
    t.look.light1 = rgb(0.66f, 0.48f, 1.0f);
    t.look.hue = 0.735f;
    t.look.hue_spread = 0.35f;
    t.radius = 20;
    t.pills = true;
    return t;
}

Theme make_broadcast()
{
    Theme t;
    t.name = "Broadcast";
    t.about = "A tube TV from the 2000s: a curved screen in its frame, scanlines and a worn tape's tracking.";
    t.look.background = 4;
    t.look.light0 = rgb(0.06f, 0.12f, 0.26f);
    t.look.light1 = rgb(0.02f, 0.04f, 0.10f);
    t.look.effect = 1.0f;
    t.look.hue = 0.56f;
    t.look.hue_spread = 0.6f;
    t.look.saturation = 0.75f;
    t.look.flat = true;
    t.radius = 10;
    t.overlay = 3;
    t.bezel = true;
    t.fonts = "vt";
    return t;
}

Theme make_terminal()
{
    Theme t;
    t.name = "Terminal";
    t.about = "Green phosphor on black, like a computer terminal: one colour, scanlines, sharp corners.";
    t.look.background = 5;
    t.look.light0 = rgb(0.25f, 1.0f, 0.45f);
    t.look.light1 = rgb(0.10f, 0.45f, 0.20f);
    t.look.mono = true;
    t.look.mono_color = rgb(0.30f, 1.0f, 0.50f);
    t.look.dark = 0.5f;
    t.look.fill_alpha = 1.4f;
    t.look.flat = true;
    t.radius = 0;
    t.overlay = 2;
    t.fonts = "mono";
    return t;
}

Theme make_depth()
{
    Theme t;
    t.name = "Depth";
    t.about = "Panes of glass drifting through a deep space, liquid glass up front.";
    t.look.background = 6;
    t.look.panel_style = 1;
    t.look.light0 = rgb(0.10f, 0.75f, 1.0f);
    t.look.light1 = rgb(0.75f, 0.35f, 1.0f);
    t.radius = 28;
    t.pills = true;
    t.palettes = true;
    return t;
}

Theme make_aurora()
{
    Theme t;
    t.name = "Aurora";
    t.about = "Northern lights over a starry night, seen through liquid glass.";
    t.look.background = 7;
    t.look.panel_style = 1;
    t.look.light0 = rgb(0.15f, 0.95f, 0.62f);
    t.look.light1 = rgb(0.55f, 0.35f, 1.0f);
    t.look.hue = 0.50f;
    t.look.hue_spread = 0.7f;
    t.radius = 24;
    t.pills = true;
    return t;
}

Theme make_aero()
{
    Theme t;
    t.name = "Aero";
    t.about = "A bright sky, glossy bubbles and white glass, the way the 2000s pictured the future.";
    t.look.background = 8;
    t.look.panel_style = 1;
    t.look.light0 = rgb(0.30f, 0.62f, 0.98f);
    t.look.light1 = rgb(0.86f, 0.95f, 1.0f);
    t.radius = 22;
    t.pills = true;
    t.light = true;
    return t;
}

const Theme *themes()
{
    static const Theme all[kThemes] = {make_porpoise(), make_revolution(), make_midnight(), make_minimal(),
                                       make_cube(),     make_broadcast(),  make_terminal(), make_depth(),
                                       make_aurora(),   make_aero()};
    return all;
}

const Palette kPaletteList[kPalettes] = {
    {"Sapphire", -1.0f, 1.0f, 1.0f, 1.0f, rgb(0.05f, 0.62f, 1.0f), rgb(0.48f, 0.36f, 1.0f)},
    {"Indigo", 0.72f, 0.55f, 1.0f, 1.0f, rgb(0.45f, 0.30f, 1.0f), rgb(0.72f, 0.42f, 1.0f)},
    {"Spice", 0.075f, 0.45f, 1.0f, 1.0f, rgb(1.0f, 0.52f, 0.10f), rgb(1.0f, 0.26f, 0.32f)},
    {"Emerald", 0.42f, 0.50f, 1.0f, 1.0f, rgb(0.10f, 0.95f, 0.58f), rgb(0.10f, 0.68f, 0.95f)},
    {"Platinum", -1.0f, 1.0f, 0.10f, 1.0f, rgb(0.78f, 0.82f, 0.90f), rgb(0.56f, 0.60f, 0.70f)},
    {"Jet", -1.0f, 1.0f, 0.0f, 0.55f, rgb(0.36f, 0.36f, 0.40f), rgb(0.20f, 0.20f, 0.23f)},
    {"Rose", 0.93f, 0.45f, 1.0f, 1.0f, rgb(1.0f, 0.36f, 0.62f), rgb(0.72f, 0.36f, 1.0f)},
    {"Gold", 0.12f, 0.35f, 0.9f, 1.0f, rgb(1.0f, 0.80f, 0.30f), rgb(1.0f, 0.55f, 0.20f)},
};
} // namespace

const Theme &theme(int id)
{
    return themes()[std::clamp(id, 0, kThemes - 1)];
}

const Palette &palette(int id)
{
    return kPaletteList[std::clamp(id, 0, kPalettes - 1)];
}

Look look_for(const porpoise::Settings &s)
{
    const Theme &t = theme(s.ui_theme);
    Look l = t.look;
    if (t.palettes && s.ui_palette > 0)
    {
        const Palette &p = palette(s.ui_palette);
        l.hue = p.hue;
        l.hue_spread = p.spread;
        l.saturation *= p.saturation;
        l.dark *= p.dark;
        /* Only the rooms with coloured light take the palette's lights. */
        if (l.background == 0 || l.background == 1 || l.background == 6)
        {
            l.light0 = p.light0;
            l.light1 = p.light1;
        }
    }
    l.colour_filter = std::clamp(s.colour_filter, 0, 4);
    l.high_contrast = s.high_contrast;
    l.still = s.still_background || s.reduced_motion;
    return l;
}
} // namespace porpoise::ui
