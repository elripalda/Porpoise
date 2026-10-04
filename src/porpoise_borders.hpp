/* Porpoise - screen borders: what fills the bars beside a 4:3 picture.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A border is a 1920x1080 PNG, transparent where the picture shows (the 4:3
 * picture fills x 240..1680). Porpoise ships a few in <assets>/borders
 * (tools/make-borders.py); players add their own to <data>/borders, and they
 * show up in the list by file name. Settings keep a border by name, without
 * ".png"; a player's file of the same name as a built-in one wins. */
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace porpoise::borders
{
struct Border
{
    std::string name;  /* what settings keep ("" = none) */
    std::string label; /* what the menus show (English; built-ins are translated by the caller) */
    bool built_in = false;
};

void set_dirs(const std::string &asset_dir, const std::string &data_dir);
/* None first, then the built-in ones, then the player's, by name. */
std::vector<Border> list();
/* The PNG for a border's name, or "" when there is none. */
std::string path_of(const std::string &name);
/* The player's own borders folder (created on first use). */
std::string user_dir();
} // namespace porpoise::borders
