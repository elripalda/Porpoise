/* Porpoise - Dolphin's built-in graphics mods, as a few settings per game.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Dolphin ships graphics mods in Sys/Load/GraphicMods: texture definitions for
 * about fifty games (which of their textures are bloom, depth of field or the
 * HUD) and "All Games" mods that act on those (remove, blur, or draw at the
 * game's own resolution). Porpoise turns them into a row or two in a game's
 * Graphics settings, and before the game starts writes Dolphin's own list of
 * the game's mods (User/Config/GraphicMods/<game ID>.json) with those on. */
#pragma once

#include <string>

namespace porpoise::gfxmods
{
/* What a game's mods offer (Porpoise's rows). */
struct Offer
{
    bool bloom = false;        /* Bloom: Game's own, Off, Blurred (and Native resolution unless own_bloom) */
    bool own_bloom = false;    /* the game's own bloom mods (De Blob): Off and Blurred only */
    bool dof = false;          /* Depth of field: Game's own, Off, Blurred, Native resolution */
    bool hud = false;          /* Hide the HUD */
    std::string extra_title;   /* a game's own mod, by its title ("" none) */
    bool any() const { return bloom || dof || hud || !extra_title.empty(); }
};
Offer offer(const std::string &game_id);

/* The settings: bloom and dof 0 game's own, 1 off, 2 blurred, 3 native
 * resolution; hud and extra on or off. */
struct Choice
{
    int bloom = 0, dof = 0;
    bool hud = false, extra = false;
};
/* Whether any mod is on for the game (Dolphin's graphics mods switch). */
bool wanted(const std::string &game_id, const Choice &choice);
/* Dolphin's list of the game's mods, the chosen ones on, in config_dir
 * (<User>/Config). With none chosen nothing is written (and Dolphin's
 * graphics mods are off for the game, so its list isn't read). */
void write_profile(const std::string &config_dir, const std::string &game_id, const Choice &choice);
} // namespace porpoise::gfxmods
