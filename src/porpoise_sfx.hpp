/* Porpoise - the menus' sound sets, made in code: no recordings, almost no
 * space.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <vector>

namespace porpoise::sfx
{
/* The sets, as Settings > Audio > Sound Set lists them (Crisp the default). */
enum Set
{
    Crisp,     /* short, bright ticks */
    Soft,      /* gentle sine blips */
    Own,       /* Porpoise's own (the WAVs in assets/sounds) */
    SetCount,
};

/* The five menu effects of a set, in porpoise::sound::Effect's order
 * (GameRow, MenuScroll, MovingTab, DetailsFlip, LaunchGame), each 48 kHz
 * interleaved stereo. Own gives none (the caller has them). */
void make(Set set, std::vector<std::int16_t> out[5]);
} // namespace porpoise::sfx
