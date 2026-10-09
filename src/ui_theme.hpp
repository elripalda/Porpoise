/* Porpoise UI - the themes: how each one looks (its room, panels, colours,
 * corners and overlays) and the colour choices some of them offer.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Every theme but Revolution keeps the Porpoise screens (the top bar, the
 * library, Memory Cards, Settings) and changes how they look: the renderer
 * maps the colours the screens were drawn with to the theme's (Look), draws
 * the theme's room behind them and lays its overlay over them. */
#pragma once

#include "ui_gfx.hpp"

namespace porpoise
{
struct Settings;
}

namespace porpoise::ui
{
/* The Theme setting's numbering: new themes go at the end. */
enum class ThemeId
{
    Porpoise,
    Revolution,
    Midnight,  /* OLED */
    Minimal,
    Cube,
    Broadcast, /* a tube TV and tape */
    Terminal,
    Depth,
    Aurora,
    Aero,
    DotMatrix,
    Synthwave,
    Paper,
    Crystal,
    StarCube, /* its own interface: a glass cube with a section on each edge */
    EightBit,
    Beige,
    Translucent,
    Arcade,
};
constexpr int kThemes = 19;

struct Theme
{
    const char *name;  /* English, as the setting lists it */
    const char *about; /* its help line */
    Look look;
    float radius = 14;    /* the corners of panels and buttons */
    bool pills = false;   /* buttons and the tab bar fully rounded */
    bool light = false;   /* the dark screens drawn light (as Revolution's are) */
    int overlay = 0;      /* Gfx::fx over everything: 1 tube, 2 terminal, 3 tape */
    bool bezel = false;   /* a TV's frame around the picture */
    bool palettes = false; /* offers the Colours setting */
    float prompts = 0.82f; /* the button hints along the bottom, against 1.0 in 2.0 */
    const char *fonts = "";  /* its own letters (Gfx::set_theme_fonts), or Nunito */
};

const Theme &theme(int id);

/* Colours for the themes that offer them. */
struct Palette
{
    const char *name;
    float hue;    /* -1 keeps the blues */
    float spread;
    float saturation;
    float dark;
    Color light0, light1;
};
constexpr int kPalettes = 23;
const Palette &palette(int id);

/* The fonts any theme can use (Settings > Interface > Font); each theme
 * starts with its own. set: the theme font set Gfx bakes ("" Nunito). */
struct FontSet
{
    const char *name;
    const char *set;
};
constexpr int kFontSets = 8;
const FontSet &font_set(int id);
/* The font set for these settings: the chosen one, or the theme's own. */
const char *fonts_for(const porpoise::Settings &s);

/* The renderer's Look for these settings: the theme, its colours and the
 * accessibility settings. */
Look look_for(const porpoise::Settings &s);
} // namespace porpoise::ui
