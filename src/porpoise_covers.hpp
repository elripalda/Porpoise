/* Porpoise - box art from GameTDB, downloaded in the background.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>
#include <vector>

namespace porpoise::covers
{
struct Request
{
    std::string dir;              /* covers live here: <ID>.png, <ID>.disc.png */
    std::vector<std::string> ids; /* the library's disc IDs */
    bool covers = true;           /* front covers that are missing */
    bool discs = false;           /* disc label art that is missing */
    std::string info_path;        /* GameTDB's table (info.tsv), when wanted; "" = skip */
    std::string info_lang = "EN"; /* the language of its descriptions */
    /* Once a day, when set: Porpoise's recommended settings per game, and
     * GitHub's answer about the newest release (porpoise_update reads it). */
    std::string feed_path;
    std::string release_path;
    /* Asked for by the player: the game info again even if it is recent, and
     * art GameTDB lacked last time looked for again. */
    bool force = false;
};
/* Where they come from. */
constexpr const char *kFeedUrl = "https://raw.githubusercontent.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5/Main/data/recommended.ini";
constexpr const char *kReleaseUrl = "https://api.github.com/repos/elripalda/Porpoise-Dolphin-Emulator-for-PS5/releases?per_page=30";
/* Starts a worker for whatever of that is missing: covers first, then the
 * game info, then disc art. False when a run is still going (ask again when
 * busy() turns false); true when it started or nothing is missing. */
bool start(const Request &request);
bool busy();
/* Art that has just been saved for this ID (main thread polls every frame). */
bool take_ready(std::string &id);
/* The game info table was just written. */
bool take_info_ready();
/* The recommended settings / the newest release were just written. */
bool take_feed_ready();
bool take_release_ready();
/* "Getting covers 3 of 12", or empty when idle. */
std::string status();
/* The same as numbers, for a translated line: phase 0 covers, 1 game info,
 * 2 disc art. False when idle. */
bool progress(int &phase, int &done, int &total);
/* Asks the worker to finish after the request in flight (before a game starts). */
void stop();
} // namespace porpoise::covers
