/* Porpoise - screenshots (porpoise_shots.hpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_paths.hpp"
#include "porpoise_shots.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <sys/stat.h>
#include <vector>

#include "porpoise_core.hpp"
#include "title_threads.hpp"
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

namespace porpoise::shots
{
namespace
{
std::string g_data_dir = PORPOISE_DATA;

struct Job
{
    std::vector<unsigned char> rgba;
    unsigned width = 0, height = 0;
    float aspect = 4.0f / 3.0f;
    std::string path, thumb_path;
};
Job g_job;
pthread_t g_thread{};
std::atomic<bool> g_running{false};
std::atomic<bool> g_done{false};
std::atomic<bool> g_ok{false};
std::atomic<bool> g_reported{true};

/* The picture at its shape on screen (the core's frame may be stored at
 * another), out_w wide: each pixel the average of the ones it covers. */
std::vector<unsigned char> resized(const Job &job, int out_w, int out_h)
{
    std::vector<unsigned char> out(std::size_t(out_w) * std::size_t(out_h) * 4);
    const unsigned w = job.width, h = job.height;
    for (int y = 0; y < out_h; ++y)
    {
        const unsigned y0 = unsigned(std::size_t(y) * h / unsigned(out_h));
        const unsigned y1 = std::max(y0 + 1, unsigned(std::size_t(y + 1) * h / unsigned(out_h)));
        for (int x = 0; x < out_w; ++x)
        {
            const unsigned x0 = unsigned(std::size_t(x) * w / unsigned(out_w));
            const unsigned x1 = std::max(x0 + 1, unsigned(std::size_t(x + 1) * w / unsigned(out_w)));
            const unsigned sx = std::max(1u, (x1 - x0) / 4), sy = std::max(1u, (y1 - y0) / 4);
            unsigned sum[3] = {0, 0, 0}, n = 0;
            for (unsigned yy = y0; yy < y1; yy += sy)
                for (unsigned xx = x0; xx < x1; xx += sx)
                {
                    const unsigned char *p = &job.rgba[(std::size_t(yy) * w + xx) * 4];
                    sum[0] += p[0];
                    sum[1] += p[1];
                    sum[2] += p[2];
                    ++n;
                }
            unsigned char *q = &out[(std::size_t(y) * unsigned(out_w) + unsigned(x)) * 4];
            q[0] = static_cast<unsigned char>(sum[0] / n);
            q[1] = static_cast<unsigned char>(sum[1] / n);
            q[2] = static_cast<unsigned char>(sum[2] / n);
            q[3] = 255;
        }
    }
    return out;
}

bool write_png(const std::string &path, const std::vector<unsigned char> &rgba, int w, int h)
{
    const std::string part = path + ".part";
    if (!stbi_write_png(part.c_str(), w, h, 4, rgba.data(), w * 4))
    {
        std::remove(part.c_str());
        return false;
    }
    return std::rename(part.c_str(), path.c_str()) == 0;
}

void *writer(void *)
{
    const Job &job = g_job;
    bool ok = job.width >= 16 && job.height >= 16 && job.rgba.size() >= std::size_t(job.width) * job.height * 4;
    if (ok)
    {
        /* Full size: the picture's height, at its shape on screen. */
        const int h = int(job.height);
        const int w = std::clamp(int(float(h) * std::max(job.aspect, 0.5f) + 0.5f), 16, 7680);
        ok = write_png(job.path, resized(job, w, h), w, h);
        if (ok)
        {
            const int tw = 320, th = std::clamp(int(320.0f / std::max(job.aspect, 0.5f) + 0.5f), 90, 320);
            if (!write_png(job.thumb_path, resized(job, tw, th), tw, th))
                std::remove(job.thumb_path.c_str()); /* the gallery makes do without it */
        }
    }
    ps5::debug::mark(ok ? "shots: written" : "shots: couldn't be written");
    std::vector<unsigned char>().swap(g_job.rgba);
    g_ok = ok;
    g_done = true;
    return nullptr;
}
} // namespace

void set_data_dir(const std::string &data_dir)
{
    g_data_dir = data_dir;
}

void wait()
{
    if (g_running.exchange(false))
        pthread_join(g_thread, nullptr);
}

void forget()
{
    wait();
    g_reported = true; /* the game is over: nothing left to say about it */
}

bool take(const std::string &game_key)
{
    if (game_key.empty() || (g_running.load() && !g_done.load()))
        return false;
    wait();
    Job job;
    if (!porpoise::core::capture_picture(job.rgba, job.width, job.height))
        return false;
    job.aspect = porpoise::core::picture_aspect() > 0.1f ? porpoise::core::picture_aspect() : 4.0f / 3.0f;
    const std::string root = g_data_dir + "/screenshots", dir = root + "/" + game_key, thumbs = dir + "/.thumbs";
    mkdir(root.c_str(), 0777);
    mkdir(dir.c_str(), 0777);
    mkdir(thumbs.c_str(), 0777);
    char name[64];
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    std::strftime(name, sizeof name, "%Y-%m-%d_%H-%M-%S", &tm);
    std::string base = name;
    struct stat st;
    for (int i = 2; stat((dir + "/" + base + ".png").c_str(), &st) == 0 && i < 100; ++i)
        base = std::string(name) + "_" + std::to_string(i); /* two in one second */
    job.path = dir + "/" + base + ".png";
    job.thumb_path = thumbs + "/" + base + ".png";
    g_job = std::move(job);
    g_done = false;
    g_reported = false;
    g_ok = false;
    if (create_title_thread(&g_thread, writer, nullptr) == 0)
        g_running = true;
    else
        writer(nullptr);
    return true;
}

bool take_finished(bool &ok)
{
    if (!g_done.load() || g_reported.load())
        return false;
    wait();
    g_reported = true;
    ok = g_ok.load();
    return true;
}
} // namespace porpoise::shots
