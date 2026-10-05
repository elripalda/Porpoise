/* Porpoise - the Wii Remotes' speakers on the DualSense's own speaker. See
 * porpoise_speaker.cpp.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstddef>
#include <cstdint>

namespace porpoise::speaker
{
/* The patched core's: a Remote's sound (stereo frames at the mixer's rate), and that rate. */
using MixFn = std::size_t (*)(unsigned index, std::int16_t *stereo, std::size_t frames);
using RateFn = unsigned (*)();

/* Opens the speaker of each connected player's controller; how many opened. */
int open_ports();
/* Whether player's controller speaker is open. */
bool open(int player);
/* Starts sending each Remote's sound to its player's controller. */
void start(MixFn mix, RateFn rate);
/* Stops sending (before the core goes). */
void stop();
/* Stops and closes the controller speakers. */
void close_ports();
} // namespace porpoise::speaker
