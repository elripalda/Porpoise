/* Porpoise - the player's own GameCube BIOS, found by what is in it.
 * Porpoise ships no BIOS and none of its data.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace porpoise::bios
{
/* A GameCube BIOS (IPL.bin) found in the player's bios folder. */
struct GameCubeBios
{
    std::string path;
    bool pal = false;      /* PAL (Europe); otherwise NTSC, for US and Japanese games */
    std::string revision;  /* "1.0", "1.1", "1.2" as its header says, or "" */
};

/* Every GameCube BIOS in dir and the folders right inside it (USA, EUR, JAP
 * or any other name), whatever the files are called: a 2 MiB file whose
 * header is a GameCube IPL's. */
std::vector<GameCubeBios> find_gamecube(const std::string &dir);

/* The one for a game's region (USA, Japan, Korea: NTSC; the rest: PAL), or
 * nullptr. */
const GameCubeBios *for_region(const std::vector<GameCubeBios> &found, bool pal);

/* Whether the 256-byte header of a file is a GameCube IPL's, and if so
 * whether it is PAL and its revision. */
bool read_header(const unsigned char *head, std::size_t size, bool &pal, std::string &revision);
} // namespace porpoise::bios
