/* Porpoise UI - a game's cheats and patches, as Dolphin ships them.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Dolphin's Sys/GameSettings files name codes for many games: memory patches
 * ([OnFrame], such as widescreen and 60 fps patches) and Action Replay and
 * Gecko cheats. Each has a name line starting with '$'. A code is turned on
 * by naming it in the game's [<kind>_Enabled] section (Porpoise writes those
 * into its own per-game Dolphin file, as dolphin.<kind>_Enabled.<$name>), and
 * one Dolphin turns on itself is turned off in [<kind>_Disabled]. Action
 * Replay and Gecko codes also need Dolphin's cheats on; patches don't. */
#pragma once

#include <string>
#include <vector>

namespace porpoise::ui
{
struct Cheat
{
    std::string kind; /* OnFrame, ActionReplay or Gecko */
    std::string name; /* with its '$' */
    bool default_on = false; /* Dolphin turns it on by itself */
};
/* The codes for a game (its six-character ID), from <sys>/GameSettings:
 * the ID's first three characters' file, then its own. */
std::vector<Cheat> cheats_for(const std::string &sys_dir, const std::string &game_id);
/* The setting a code's switch is kept under for the game. */
std::string cheat_key(const Cheat &cheat, bool on);
/* What a code is, from its name and kind, for its row's help. */
std::string cheat_help(const Cheat &cheat);
} // namespace porpoise::ui
