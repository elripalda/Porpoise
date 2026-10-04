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
};
/* Starts a worker for whatever of that is missing. Covers first, then the
 * game info, then disc art. Does nothing when nothing is missing. */
void start(const Request &request);
/* Art that has just been saved for this ID (main thread polls every frame). */
bool take_ready(std::string &id);
/* The game info table was just written. */
bool take_info_ready();
/* "Getting covers 3 of 12", or empty when idle. */
std::string status();
/* The same as numbers, for a translated line: phase 0 covers, 1 game info,
 * 2 disc art. False when idle. */
bool progress(int &phase, int &done, int &total);
/* Asks the worker to finish after the request in flight (before a game starts). */
void stop();
} // namespace porpoise::covers
