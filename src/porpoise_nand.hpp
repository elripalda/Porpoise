/* Porpoise - a Wii's own system data (its NAND, backed up with BootMii) made
 * Dolphin's: the files of the Wii's flash, decrypted with the console's own
 * key, into Dolphin's Wii folder, as Dolphin's "Import BootMii NAND Backup"
 * does. Its settings, Miis, channels and saves, and the certificates some
 * online services (Wiimmfi) check, are then the player's own Wii's.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <atomic>
#include <mutex>
#include <string>

namespace porpoise::nand
{
/* How an import is going, read by the menus while a worker runs it. */
struct Progress
{
    std::atomic<int> done{0}, total{0}; /* files */
    std::atomic<int> state{0};          /* 0 reading, 1 done, 2 failed */
    std::mutex m;
    std::string error; /* what went wrong, for the player (state 2) */
    int files = 0;     /* written */
    int kept = 0;      /* Porpoise's own files that were in the way, moved aside */
    bool certificates = false;
};

/* Where a backup is: nand.bin in <data>/nand, else at a USB drive's top;
 * keys.bin beside it (or in the same folder), when the backup has no keys
 * of its own. False when there is none. */
bool find_backup(const std::string &data_dir, std::string &bin, std::string &keys);

/* The backup into wii_root (Dolphin's User/Wii). A file Porpoise already has
 * there is moved into kept_root (same path) before the Wii's replaces it.
 * Fills p as it goes; run it on a worker. */
void import(const std::string &bin, const std::string &keys, const std::string &wii_root,
            const std::string &kept_root, Progress &p);

/* Whether a Wii's system data was imported (its keys.bin is there). */
bool imported(const std::string &wii_root);
} // namespace porpoise::nand
