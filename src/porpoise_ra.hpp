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

/* Before retro_load_game: the account, the network and the popups go to the
 * core (when signed in, and the core knows how). */
void start_game(void *core_library);
/* Before retro_unload_game: up to max_ms for unlocks still being sent. */
void finish_game(void *core_library, int max_ms);
/* After it: the game's connection closes. */
void end_game();
} // namespace porpoise::ra
