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

/* AES-128-CBC decryption, in place (public for the tests). */
void aes_cbc_decrypt(const std::uint8_t key[16], std::uint8_t iv[16], std::uint8_t *data, std::size_t size);
} // namespace porpoise::disc
