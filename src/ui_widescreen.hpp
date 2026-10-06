/* Porpoise UI - widescreen done per game: the game's own 16:9 code where one
 * exists (Warped Polygon's collection in assets/widescreen, or Dolphin's own
 * list), the game's own 16:9 option where it has one, and Dolphin's emulated
 * widescreen hack only when the player asks for it.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why: Dolphin's widescreen hack widens the camera, but the game still only
 * draws what fits its 4:3 view, so things at the sides pop in and out. A code
 * made for the game makes the game itself draw the wider view. */
#pragma once

#include <string>
#include <vector>

#include "ui_cheats.hpp"

namespace porpoise::ui::widescreen
{
/* The Widescreen setting (Settings::wide). */
enum Mode
{
    ModeAuto = 0, /* the game's code or its own option; 4:3 otherwise */
    ModeOn = 1,   /* the same, or the emulated hack when the game has neither */
    ModeOff = 2,  /* always 4:3 */
};
/* What a game has. */
enum class Kind
{
    None,   /* only the emulated hack */
    Patch,  /* a widescreen code */
    Native, /* a 16:9 option of its own (and Wii games: the Wii's setting) */
};
/* How a game runs, from the setting and what it has. */
enum class Plan
{
    Standard, /* 4:3 */
    Patch,    /* its code on, the picture 16:9, no hack */
    Native,   /* its own option decides; the picture follows */
    Hack,     /* Dolphin's emulated widescreen hack */
    PatchHack, /* its code on with the hack, for the few codes made to work with it */
};

/* Where codes.ini and native.txt are (read once, when first asked). */
void set_dir(const std::string &dir);
Kind kind_of(const std::string &game_id, const std::string &sys_dir, bool wii);
/* The collection marks a few games whose codes work only with the hack on. */
bool code_needs_hack(const std::string &game_id);
Plan plan_for(int mode, Kind kind, bool code_needs_hack = false);
/* The game has its own 16:9 option (the collection's list). */
bool has_native(const std::string &game_id);
/* The collection's codes for the game (exact ID), as Cheats. */
std::vector<Cheat> pack_codes(const std::string &game_id);
/* A widescreen code by its name (16:9 or widescreen in it; an aspect-ratio
 * fix is not one). */
bool is_widescreen_code(const std::string &name);
/* Adds the game's codes to Dolphin's per-game file at ini_path (as the
 * player's own cheats are): the collection's code lines, and as enabled each
 * widescreen code when the plan is Patch (the collection's, or else the first
 * of Dolphin's own) unless turned off (off(code) true). The collection's other
 * codes (60 Hz, a centered HUD...) are on only when the player turns them on
 * in Cheats, which Settings::write_dolphin_game_ini writes. True when an
 * Action Replay or Gecko code is turned on here, which needs Dolphin's cheats. */
bool add_codes(const std::string &game_id, const std::string &sys_dir, const std::string &ini_path, Plan plan,
               bool (*off)(const Cheat &, const void *), const void *user, bool &widescreen_on);
/* A line for the game's settings, about its widescreen. */
std::string about(Kind kind);
} // namespace porpoise::ui::widescreen
