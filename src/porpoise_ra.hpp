/* Porpoise - RetroAchievements (softcore): the account, and the bridge to the
 * core, which earns the achievements (Dolphin's own AchievementManager), while
 * Porpoise does the HTTPS and shows the unlocks as PS5 trophy popups.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Signing in sends the password once, for a token; only the token is kept
 * (<data>/retroachievements.ini, readable by Porpoise alone). */
#pragma once

#include <string>

#include "ui_achievements.hpp"

namespace porpoise::ra
{
struct Account
{
    bool signed_in = false;
    bool busy = false;   /* signing in */
    std::string user;    /* as RetroAchievements writes it */
    int points = 0;      /* softcore points, as of the last sign-in */
    std::string message; /* why the last sign-in didn't work, or "" */
};

/* Where the account is kept (Porpoise's folder); reads it. */
void init(const std::string &data_dir);
Account account();
/* Starts signing in on a worker; false when one is already going. */
bool begin_login(const std::string &user, const std::string &password);
/* True once, when a sign-in has finished (either way). */
bool take_login_done();
void logout();

/* The game about to start (its disc ID), for the list kept for the library. */
void prepare_game(const std::string &disc_id);
/* Before retro_load_game: the account, the network and the popups go to the
 * core (when signed in, and the core knows how). */
void start_game(void *core_library);
/* Each frame of the game (its own thread): keeps the game's list and fetches
 * its badges after it loads and after each unlock. */
void pump();
/* The running game's set (false when none), its badges' files as far as
 * they're downloaded. */
bool live_list(porpoise::ui::AchievementSet &out);
/* A game's set as kept from its last play (false when none). */
bool kept_list(const std::string &disc_id, porpoise::ui::AchievementSet &out);
/* A popup Porpoise draws over the game itself, when the console's own rich
 * notifications don't reach the screen (no ELF loader): the badge, a title
 * and a line. */
struct GameToast
{
    std::string caption, title, text, badge_path;
    bool trophy = false; /* an unlock or the game completed: gold */
};
bool take_game_toast(GameToast &out);
/* Before retro_unload_game: up to max_ms for unlocks still being sent. */
void finish_game(void *core_library, int max_ms);
/* After it: the game's connection closes. */
void end_game();
} // namespace porpoise::ra
