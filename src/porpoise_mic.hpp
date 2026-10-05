/* Porpoise - the DualSense's microphone for the Dolphin core (libretro's
 * microphone interface). See porpoise_mic.cpp.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "libretro.h"

namespace porpoise::mic
{
/* Fills in the interface the core asks for (RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE). */
void fill(retro_microphone_interface &iface);
/* Stops listening (the game is over). */
void close_all();
} // namespace porpoise::mic
