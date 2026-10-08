/* Porpoise - pictures read and written for other parts (stb, compiled once in
 * porpoise_covers.cpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace porpoise::image
{
/* A PNG or JPEG as 8-bit RGBA; false when it can't be read. */
bool load_rgba(const std::string &path, std::vector<std::uint8_t> &rgba, int &width, int &height);
bool write_png(const std::string &path, const std::vector<std::uint8_t> &rgba, int width, int height);
} // namespace porpoise::image
