/* Porpoise UI - setups: a game's video and graphics settings, saved to use
 * again on other games (or on every game).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Four of them, <data>/setups/setup<N>.ini, written with Settings::save_keys
 * and a first line naming the game they came from. */
#pragma once

#include <string>
#include <vector>

#include "ui_settings.hpp"

namespace porpoise::ui::setups
{
constexpr int kCount = 4;

struct Setup
{
    bool exists = false;
    std::string from; /* the game it was saved from */
    std::string path;
};

void set_dir(const std::string &data_dir);
/* The keys a setup holds: the Video and Graphics settings. */
const std::vector<std::string> &keys();
Setup get(int index);
bool save(int index, const porpoise::Settings &settings, const std::string &from);
/* Puts the setup's values into settings; adds its keys to keys (a game's own
 * keys) when given. */
bool apply(int index, porpoise::Settings &settings, std::vector<std::string> *keys = nullptr);
} // namespace porpoise::ui::setups
