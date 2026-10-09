/* Porpoise - screenshots: the game's picture written to
 * <data>/screenshots/<game>/<date and time>.png, with a small copy in
 * .thumbs beside it for the gallery. Taken from the in-game menu or with
 * touch pad + Square.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>

namespace porpoise::shots
{
void set_data_dir(const std::string &data_dir);
/* Copies the game's last picture now and writes it on a worker thread. False
 * when there is no picture yet or the last one is still being written. */
bool take(const std::string &game_key);
/* Once, when a shot taken with take() is on disk (ok) or failed. */
bool take_finished(bool &ok);
/* Waits for a shot still being written. */
void wait();
/* The same, as the game closes: its note isn't shown in the next game. */
void forget();
} // namespace porpoise::shots
