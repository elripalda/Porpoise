/* Porpoise - home screen forwarders: a tile of its own for one game.
 * A game's settings > Add to home screen writes a small app folder in
 * /data/homebrew (tools/forwarder/forwarder.cpp as its eboot.bin, a
 * forward.txt naming the game, the tile's icon and background); the player
 * registers it like any homebrew, and opening it opens Porpoise on that game
 * (--rom, docs/FORWARDER.md).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>

namespace porpoise::forwarders
{
struct Options
{
    std::string title;      /* the tile's name */
    std::string game_id;    /* the disc ID, for the player's own art's names */
    std::string game_path;  /* the game's file */
    std::string cover_path; /* its cover, "" when it has none */
    int icon = 0;           /* 0 the cover, 1 the player's own */
    int background = 0;     /* 0 Porpoise's, 1 the cover, 2 the player's own */
    bool exit_after_game = true;
};
struct Result
{
    bool ok = false;
    std::string title_id, folder, error;
};
/* Where the player's own art goes: <data>/home-art, as <ID>-icon and
 * <ID>-background (.png or .jpg). */
std::string art_dir(const std::string &data_dir);
std::string own_icon(const std::string &data_dir, const std::string &game_id);
std::string own_background(const std::string &data_dir, const std::string &game_id);
/* The title ID of the forwarder already made for this game, or "". */
std::string existing(const std::string &data_dir, const std::string &game_path);
/* Makes (or remakes) the game's forwarder. Slow-ish (its art): a second or two. */
Result make(const std::string &data_dir, const Options &options);
/* Gives the tiles made before 2.7's fix the file modes the console starts
 * an app with. Cheap: a stat per tile when nothing needs doing. */
void repair(const std::string &data_dir);
} // namespace porpoise::forwarders
