/* Porpoise - save states: three slots per game, each with a thumbnail.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_states.hpp"

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>
#include <vector>

#include "porpoise_core.hpp"
#include "trace.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace porpoise::states
{
namespace
{
std::string g_data_dir = "/data/porpoise";

std::string dir_of(const std::string &game_key)
{
    return g_data_dir + "/states/" + game_key;
}

/* A box-filtered copy of the picture at the game's shape, 320 wide. */
bool write_thumbnail(const std::string &path)
{
    std::vector<unsigned char> rgba;
    unsigned w = 0, h = 0;
    if (!porpoise::core::capture_picture(rgba, w, h) || w == 0 || h == 0)
        return false;
    const float aspect = porpoise::core::picture_aspect() > 0.1f ? porpoise::core::picture_aspect() : 4.0f / 3.0f;
    const int tw = 320, th = std::clamp(int(320.0f / aspect + 0.5f), 120, 320);
    std::vector<unsigned char> out(std::size_t(tw) * th * 4);
    for (int y = 0; y < th; ++y)
    {
        const unsigned y0 = unsigned(std::size_t(y) * h / th), y1 = std::max(y0 + 1, unsigned(std::size_t(y + 1) * h / th));
        for (int x = 0; x < tw; ++x)
        {
            const unsigned x0 = unsigned(std::size_t(x) * w / tw),
                           x1 = std::max(x0 + 1, unsigned(std::size_t(x + 1) * w / tw));
            unsigned sum[3] = {0, 0, 0}, n = 0;
            /* At most a 4x4 sample per pixel: plenty for a thumbnail. */
            const unsigned sx = std::max(1u, (x1 - x0) / 4), sy = std::max(1u, (y1 - y0) / 4);
            for (unsigned yy = y0; yy < y1; yy += sy)
                for (unsigned xx = x0; xx < x1; xx += sx)
                {
                    const unsigned char *p = &rgba[(std::size_t(yy) * w + xx) * 4];
                    sum[0] += p[0];
                    sum[1] += p[1];
                    sum[2] += p[2];
                    ++n;
                }
            unsigned char *q = &out[(std::size_t(y) * tw + x) * 4];
            q[0] = static_cast<unsigned char>(sum[0] / n);
            q[1] = static_cast<unsigned char>(sum[1] / n);
            q[2] = static_cast<unsigned char>(sum[2] / n);
            q[3] = 255;
        }
    }
    return stbi_write_png(path.c_str(), tw, th, 4, out.data(), tw * 4) != 0;
}
} // namespace

void set_data_dir(const std::string &data_dir)
{
    g_data_dir = data_dir;
}

Slot slot(const std::string &game_key, int index)
{
    Slot s;
    const std::string base = dir_of(game_key) + "/slot" + std::to_string(index + 1);
    s.state_path = base + ".state";
    s.picture_path = base + ".png";
    struct stat st;
    if (stat(s.state_path.c_str(), &st) == 0 && st.st_size > 0)
    {
        s.exists = true;
        s.time = static_cast<long long>(st.st_mtime);
    }
    return s;
}

bool save(const std::string &game_key, int index)
{
    if (index < 0 || index >= kSlots)
        return false;
    mkdir((g_data_dir + "/states").c_str(), 0777);
    mkdir(dir_of(game_key).c_str(), 0777);
    const Slot s = slot(game_key, index);
    if (!porpoise::core::save_state(s.state_path.c_str()))
        return false;
    if (!write_thumbnail(s.picture_path))
        std::remove(s.picture_path.c_str()); /* a slot without a picture still loads */
    return true;
}

bool load(const std::string &game_key, int index)
{
    const Slot s = slot(game_key, index);
    return s.exists && porpoise::core::load_state(s.state_path.c_str());
}

bool remove(const std::string &game_key, int index)
{
    const Slot s = slot(game_key, index);
    std::remove(s.picture_path.c_str());
    return std::remove(s.state_path.c_str()) == 0;
}
} // namespace porpoise::states
