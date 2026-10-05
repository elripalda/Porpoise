/* Porpoise - reading files out of GameCube and Wii disc images, for the
 * launcher (a Wii disc's own banner, opening.bnr). Plain .iso / .gcm, .wbfs,
 * .ciso, .gcz, and .rvz / .wia (Zstandard or uncompressed); a Wii disc's data
 * partition is decrypted as it is read.
 *
 * The container formats follow Dolphin's DiscIO (GPL-2.0-or-later), written
 * again here in a small, read-only form.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace porpoise::disc
{
/* One file from the disc's file system (its data partition, on a Wii disc),
 * by its path from the root ("opening.bnr"). false and a reason when it
 * can't be had. */
bool read_file(const std::string &image, const std::string &name, std::vector<std::uint8_t> &out,
               std::string &error);

/* The first 0x100 bytes of a disc image of any kind it reads (its ID at 0,
 * its title at 0x20): for the containers that don't keep them in the open
 * (GCZ, WBFS). */
bool read_header(const std::string &image, std::uint8_t out[0x100], std::string &error);

/* A WAD (a WiiWare, Virtual Console or channel title's installer): its
 * title ID, the six-character ID Dolphin gives it (the title's four letters
 * and its maker), its names in the Wii's ten languages (0 Japanese, 1
 * English, 2 German, 3 French, 4 Spanish, 5 Italian, 6 Dutch, 7 Simplified
 * Chinese, 8 Traditional Chinese, 9 Korean), and, when banner is given, its
 * banner (the first content, laid out as a disc's opening.bnr). */
struct WadInfo
{
    std::uint64_t title_id = 0;
    std::string id;
    std::string names[10];
};
bool read_wad(const std::string &path, WadInfo &info, std::vector<std::uint8_t> *banner, std::string &error);

/* AES-128-CBC decryption, in place (public for the tests). */
void aes_cbc_decrypt(const std::uint8_t key[16], std::uint8_t iv[16], std::uint8_t *data, std::size_t size);
} // namespace porpoise::disc
