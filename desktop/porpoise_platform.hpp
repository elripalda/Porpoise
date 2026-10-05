/* Porpoise on a desktop: the window, events and where things are.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>
#include <utility>
#include <vector>

struct SDL_Window;

namespace porpoise::platform
{
/* SDL, the window and the working directory (the program's folder). */
bool init(int argc, char **argv);
/* Window and controller events; on the main thread, once a frame. */
void pump();
/* The window was closed (or Alt+F4): Porpoise should leave. */
bool quit_requested();
SDL_Window *window();
/* The window's drawable size changed since the last call. */
bool take_resized();
/* The refresh rate of the display the window is on. */
double display_hz();
/* A path made whole (from the program's folder). */
std::string absolute(const std::string &path);
/* A game is running: Escape opens the in-game menu instead of going back. */
void set_in_game(bool in_game);
void shutdown();
/* For the folder browser: the computer's drives ("C:/") and the player's own
 * folders (Downloads, Documents, Desktop), as label and path. */
std::vector<std::string> drives();
std::vector<std::pair<std::string, std::string>> user_folders();
} // namespace porpoise::platform
