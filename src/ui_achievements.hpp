/* Porpoise UI - a game's RetroAchievements, as the in-game menu's
 * Achievements tab and the library's Achievements page show them.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The core lists the running game's set (DolphinLibretro/Achievements.cpp,
 * porpoise_ra_list) as text; porpoise_ra keeps the last list of each game in
 * <data>/achievements/<disc ID>.txt for the library, and the badges in
 * <data>/achievements/badges. */
#pragma once

#include <string>
#include <vector>

namespace porpoise::ui
{
struct Achievement
{
    unsigned id = 0;
    int points = 0;
    bool unlocked = false;
    long long unlock_time = 0; /* seconds since 1970, when unlocked */
    int type = 0;              /* 0 standard, 1 missable, 2 progression, 3 win condition */
    float rarity = 0;          /* % of players who have it */
    std::string progress;      /* "3/10" for one that counts, else "" */
    std::string title, description;
    std::string badge_url, badge_locked_url;
};

struct AchievementSet
{
    unsigned game_id = 0; /* RetroAchievements' own */
    std::string title, badge_url;
    int unlocked = 0, total = 0, points = 0, points_total = 0;
    std::vector<Achievement> list;
    bool valid() const { return game_id != 0 && total > 0; }
};

/* Reads the core's text (or the kept copy of it). False when it holds no set. */
bool parse_achievements(const std::string &text, AchievementSet &out);
/* A badge's file name in the badge folder ("" for an empty URL). */
std::string badge_file(const std::string &url);
} // namespace porpoise::ui
