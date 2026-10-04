/* Porpoise UI - recommended settings for a game, by its disc ID.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Two sources:
 *   Dolphin's fixes   Dolphin's own per-game database (Sys/GameSettings/<ID>.ini,
 *                     shipped with the core): settings Dolphin's team found a
 *                     game needs. Dolphin applies them on its own; Porpoise
 *                     only shows them.
 *   Porpoise's picks  data/recommended.ini in Porpoise's GitHub repository,
 *                     downloaded once a day: Porpoise settings that run a game
 *                     best on PS5, as tested. The player applies them from the
 *                     game's settings.
 * Both are looked up by the six-character ID first (one region), then by its
 * first three characters (every region of the game). */
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace porpoise::ui
{
namespace recommend
{
struct Fix
{
    std::string label; /* English, for tr() */
    std::string value; /* English for On / Off and the like */
    std::string why;   /* the database's own comment, when it has one (English, not translated) */
};

struct Pick
{
    std::string note; /* why, from the feed */
    std::vector<std::pair<std::string, std::string>> values; /* Settings keys and values */
};

/* feed: the downloaded data/recommended.ini; game_settings: Dolphin's Sys/GameSettings. */
void set_paths(const std::string &feed, const std::string &game_settings);
/* The feed was downloaded again: read it afresh next time. */
void feed_changed();
std::vector<Fix> dolphin_fixes(const std::string &id);
bool pick_for(const std::string &id, Pick &out);
} // namespace recommend
} // namespace porpoise::ui
