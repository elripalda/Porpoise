/* Porpoise - the menus' sound sets, made in code: no recordings, almost no
 * space. And the player's own GameCube BIOS's sounds as a set.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace porpoise::sfx
{
/* The sets, as Settings > Audio > Sound Set lists them. */
enum Set
{
    Own,       /* Porpoise's own (the WAVs in assets/sounds) */
    Soft,      /* gentle sine blips */
    Crisp,     /* short, bright ticks */
    Chiptune,  /* square-wave blips and arpeggios */
    Glass,     /* bell-like chimes */
    RetroPC,   /* keyboard and relay clicks, a PC speaker's beep */
    Console,   /* the player's own GameCube BIOS's sounds */
    SetCount,
};

/* The five menu effects of a set, in porpoise::sound::Effect's order
 * (GameRow, MenuScroll, MovingTab, DetailsFlip, LaunchGame), each 48 kHz
 * interleaved stereo. Own and Console give none (the caller has them). */
void make(Set set, std::vector<std::int16_t> out[5]);

/* The Console set from a GameCube BIOS file: its own sounds, each effect from
 * one of them, at 48 kHz stereo. False when the file has none to use. */
bool from_bios(const std::string &bios_path, std::vector<std::int16_t> out[5]);
} // namespace porpoise::sfx
