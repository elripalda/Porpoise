/* Porpoise - save states: three slots per game, each with a thumbnail.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A slot is <data>/states/<game key>/slot<N>.state (what Dolphin's
 * retro_serialize wrote) beside slot<N>.png, a small picture of the game at
 * that moment. Saving and loading happen while the game is paused in the
 * in-game menu; a slot chosen in Details is loaded once the game is up. */
#pragma once

#include <string>

namespace porpoise::states
{
constexpr int kSlots = 3;

struct Slot
{
    bool exists = false;
    long long time = 0; /* when it was saved (seconds since 1970) */
    std::string state_path, picture_path;
};

void set_data_dir(const std::string &data_dir);
Slot slot(const std::string &game_key, int index);
/* While the game is paused: save its state and a thumbnail into a slot. */
bool save(const std::string &game_key, int index);
bool load(const std::string &game_key, int index);
bool remove(const std::string &game_key, int index);
} // namespace porpoise::states
