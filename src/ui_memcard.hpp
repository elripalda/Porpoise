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
    std::string dir;     /* the GCI folder, when it is one */
    int total_blocks = 2043;
    int free_blocks = 2043;
    std::vector<Save> saves;
};

/* A Wii game's save: Dolphin keeps it in its virtual Wii storage, one folder
 * per game (<saves>/User/Wii/title/00010000/<title id>/data), with a
 * banner.bin that carries its name, a 192x64 banner and a 48x48 icon. */
struct WiiSave
{
    std::string data_dir;  /* the folder with the save's files */
    std::string title_id;  /* eight hex digits, e.g. 524d4345 */
    std::string game_code; /* the same as letters: RMCE */
    std::string title;     /* the save's own name */
    std::string detail;    /* its second line */
    int files = 0;
    long long bytes = 0;
    long long modified = 0; /* newest file, unix time */
    std::vector<std::uint8_t> icon;   /* 48x48 RGBA, first frame */
    std::vector<std::uint8_t> banner; /* 192x64 RGBA */
    Texture *icon_tex = nullptr;
    Texture *banner_tex = nullptr;
};
/* Every Wii save under <saves>/User/Wii, by name. */
void load_wii_saves(const std::string &saves_dir, std::vector<WiiSave> &out);
bool parse_wii_banner(const std::string &path, WiiSave &out);
/* Copies a save's folder to <saves>/User/Wii/backups/<code>-<date>; false if
 * it could not. where: the copy's folder. */
bool backup_wii_save(const std::string &saves_dir, const WiiSave &save, std::string &where);
/* Removes the save's files (the game starts fresh). */
bool delete_wii_save(const WiiSave &save);

/* ---- saves on a USB drive (beta) ----
 * <usb>/Porpoise Saves/GameCube/<save>.gci, and for Wii saves
 * <usb>/Porpoise Saves/Wii/<game code> <title id>/ with the save's files and
 * porpoise-wii-path.txt (where it goes under the saves folder). */
/* The first USB drive's folder, "" when none is in. */
std::string usb_root();
bool export_gc_save(const Save &save, std::string &where);
bool export_wii_save(const std::string &saves_dir, const WiiSave &save, std::string &where);
/* Copies the USB drive's saves in: GameCube ones onto card (a GCI folder),
 * Wii ones to their place. A save already there is left as it is (skipped). */
bool import_usb_saves(const std::string &saves_dir, const Card &card, int &gc, int &wii, int &skipped);

/* Finds cards A and B under Dolphin's user folder (<saves>/User/GC). */
void load_cards(const std::string &saves_dir, Card &a, Card &b);

/* Parsing, public for the preview tool and tests. */
bool parse_gci(const std::string &path, Save &out);
std::string format_date(long long unix_time);
} // namespace porpoise::ui
