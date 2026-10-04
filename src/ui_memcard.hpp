/* Porpoise UI - GameCube memory cards: saves, with their own icons and banners.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace porpoise::ui
{
struct Texture;

struct Save
{
    std::string path;      /* .gci file, or the .raw card it is in */
    std::string game_code; /* e.g. GZLE + maker 01 */
    std::string file_name; /* the save's name on the card */
    std::string title;     /* comment line 1: usually the game's name */
    std::string detail;    /* comment line 2 */
    int blocks = 0;
    long long modified = 0; /* unix time */
    /* Decoded pictures, RGBA8. Empty when the save has none. */
    std::vector<std::uint8_t> icon;   /* 32x32, first frame */
    std::vector<std::uint8_t> banner; /* 96x32 */
    Texture *icon_tex = nullptr;
    Texture *banner_tex = nullptr;
};

struct Card
{
    std::string slot; /* "A" or "B" */
    bool present = false;
    bool folder = false; /* a Dolphin GCI folder rather than a .raw image */
    int total_blocks = 2043;
    int free_blocks = 2043;
    std::vector<Save> saves;
};

/* Finds cards A and B under Dolphin's user folder (<saves>/User/GC). */
void load_cards(const std::string &saves_dir, Card &a, Card &b);

/* Parsing, public for the preview tool and tests. */
bool parse_gci(const std::string &path, Save &out);
std::string format_date(long long unix_time);
} // namespace porpoise::ui
