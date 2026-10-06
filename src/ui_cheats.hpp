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
    bool own = false;        /* the player's own, from Porpoise's cheats folder: on unless turned off */
    bool pack = false;       /* from the widescreen collection (ui_widescreen) */
};
/* The codes for a game (its six-character ID), from <sys>/GameSettings:
 * the ID's first three characters' file, then its own; then the player's
 * own from own_dir, when given. */
std::vector<Cheat> cheats_for(const std::string &sys_dir, const std::string &game_id, const std::string &own_dir = "");
/* Adds the player's own codes for the game (own_dir/<ID>.ini, or its first
 * four or three letters, in Dolphin's format) to Dolphin's per-game file at
 * ini_path: their code lines, and each one that isn't turned off (off(code)
 * true) as enabled. True when one of them is an Action Replay or Gecko cheat,
 * which needs Dolphin's cheats on. */
bool add_own_cheats(const std::string &own_dir, const std::string &game_id, const std::string &ini_path,
                    bool (*off)(const Cheat &cheat, const void *user), const void *user);
/* The setting a code's switch is kept under for the game. */
std::string cheat_key(const Cheat &cheat, bool on);
/* What a code is, from its name and kind, for its row's help. */
std::string cheat_help(const Cheat &cheat);
} // namespace porpoise::ui
