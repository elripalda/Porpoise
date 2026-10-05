/* Porpoise - save states: three slots per game, each with a thumbnail.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_states.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
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

namespace porpoise::states
{
namespace
{
std::string g_data_dir = "/data/porpoise";

/* The save being written: the worker owns it until it sets done. */
struct Job
{
    std::vector<unsigned char> state, rgba;
    unsigned width = 0, height = 0;
    float aspect = 4.0f / 3.0f;
    std::string state_path, picture_path;
    int index = 0;
};
Job g_job;
pthread_t g_thread{};
std::atomic<bool> g_running{false};  /* a worker exists and hasn't been joined */
std::atomic<bool> g_done{false};     /* it has finished */
std::atomic<bool> g_ok{false};
std::atomic<bool> g_reported{true};  /* take_finished has told of it */

std::string dir_of(const std::string &game_key)
{
    return g_data_dir + "/states/" + game_key;
}

long long now_ms()
{
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

bool write_file(const std::string &path, const std::vector<unsigned char> &data)
{
    const std::string tmp = path + ".part";
    std::FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    /* All of it on disk before it takes the old one's place: a full disk
     * leaves the slot as it was. */
    bool written =
        std::fwrite(data.data(), 1, data.size(), f) == data.size() && std::fflush(f) == 0 && fsync(fileno(f)) == 0;
    written = std::fclose(f) == 0 && written;
    if (!written || std::rename(tmp.c_str(), path.c_str()) != 0)
    {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

/* Rows (or columns) at an edge that are all near black: the bars many games
 * draw around their picture, which the thumbnail leaves out. */
bool dark_line(const std::vector<unsigned char> &rgba, unsigned w, unsigned x0, unsigned y0, unsigned dx, unsigned dy,
               unsigned n)
{
    unsigned bright = 0;
    for (unsigned i = 0; i < n; i += 2)
    {
        const unsigned char *p = &rgba[(std::size_t(y0 + dy * i) * w + (x0 + dx * i)) * 4];
        if (p[0] > 24 || p[1] > 24 || p[2] > 24)
            ++bright;
    }
    return bright * 100 <= n; /* fewer than 2% of the samples bright */
}

/* A box-filtered copy of the picture without its black edges, 320 wide, at
 * the shape it has on screen. */
bool write_thumbnail(const Job &job)
{
    const unsigned w = job.width, h = job.height;
    if (w < 16 || h < 16 || job.rgba.size() < std::size_t(w) * h * 4)
        return false;
    unsigned top = 0, bottom = h, left = 0, right = w;
    while (bottom - top > h / 2 && dark_line(job.rgba, w, 0, bottom - 1, 1, 0, w))
        --bottom;
    while (bottom - top > h / 2 && dark_line(job.rgba, w, 0, top, 1, 0, w))
        ++top;
    while (right - left > w / 2 && dark_line(job.rgba, w, right - 1, top, 0, 1, bottom - top))
        --right;
    while (right - left > w / 2 && dark_line(job.rgba, w, left, top, 0, 1, bottom - top))
        ++left;
    if (bottom - top == h / 2 || right - left == w / 2)
    {
        /* A dark scene, not bars: keep it whole. */
        top = left = 0;
        bottom = h;
        right = w;
    }
    const unsigned cw = right - left, ch = bottom - top;
    /* The screen shape of what's left: the picture's own aspect, scaled by
     * how much of each side remains. */
    const float aspect = job.aspect * (float(cw) / float(w)) / (float(ch) / float(h));
    const int tw = 320, th = std::clamp(int(320.0f / std::max(aspect, 0.5f) + 0.5f), 120, 320);
    std::vector<unsigned char> out(std::size_t(tw) * th * 4);
    for (int y = 0; y < th; ++y)
    {
        const unsigned y0 = top + unsigned(std::size_t(y) * ch / th);
        const unsigned y1 = std::max(y0 + 1, top + unsigned(std::size_t(y + 1) * ch / th));
        for (int x = 0; x < tw; ++x)
        {
            const unsigned x0 = left + unsigned(std::size_t(x) * cw / tw);
            const unsigned x1 = std::max(x0 + 1, left + unsigned(std::size_t(x + 1) * cw / tw));
            unsigned sum[3] = {0, 0, 0}, n = 0;
            /* At most a 4x4 sample per pixel: plenty for a thumbnail. */
            const unsigned sx = std::max(1u, (x1 - x0) / 4), sy = std::max(1u, (y1 - y0) / 4);
            for (unsigned yy = y0; yy < y1; yy += sy)
                for (unsigned xx = x0; xx < x1; xx += sx)
                {
                    const unsigned char *p = &job.rgba[(std::size_t(yy) * w + xx) * 4];
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
    const std::string tmp = job.picture_path + ".part";
    if (!stbi_write_png(tmp.c_str(), tw, th, 4, out.data(), tw * 4))
        return false;
    return std::rename(tmp.c_str(), job.picture_path.c_str()) == 0;
}

void *writer(void *)
{
    const long long t0 = now_ms();
    bool ok = write_file(g_job.state_path, g_job.state);
    const long long t1 = now_ms();
    /* The picture goes with the state, or not at all: a slot without one still loads. */
    if (ok && !write_thumbnail(g_job))
        std::remove(g_job.picture_path.c_str());
    const long long t2 = now_ms();
    char line[128];
    std::snprintf(line, sizeof line, "states: slot %d written in %lld ms, picture in %lld ms", g_job.index + 1,
                  t1 - t0, t2 - t1);
    ps5::debug::mark(line);
    std::vector<unsigned char>().swap(g_job.state);
    std::vector<unsigned char>().swap(g_job.rgba);
    g_ok = ok;
    g_done = true;
    return nullptr;
}
} // namespace

bool write_resume_picture(const std::string &state_path)
{
    if (state_path.size() < 6)
        return false;
    Job job;
    job.picture_path = state_path.substr(0, state_path.size() - 6) + ".png";
    job.aspect = porpoise::core::picture_aspect() > 0.1f ? porpoise::core::picture_aspect() : 4.0f / 3.0f;
    if (!porpoise::core::capture_picture(job.rgba, job.width, job.height) || !write_thumbnail(job))
    {
        std::remove(job.picture_path.c_str());
        return false;
    }
    return true;
}

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

void wait()
{
    if (g_running.exchange(false))
        pthread_join(g_thread, nullptr);
}

bool busy()
{
    return g_running.load() && !g_done.load();
}

bool take_finished(int &index, bool &ok)
{
    if (!g_done.load() || g_reported.load())
        return false;
    wait();
    g_reported = true;
    index = g_job.index;
    ok = g_ok.load();
    return true;
}

bool save(const std::string &game_key, int index)
{
    if (index < 0 || index >= kSlots)
        return false;
    wait(); /* one save at a time */
    mkdir((g_data_dir + "/states").c_str(), 0777);
    mkdir(dir_of(game_key).c_str(), 0777);
    const Slot s = slot(game_key, index);
    g_job = Job{};
    g_job.index = index;
    g_job.state_path = s.state_path;
    g_job.picture_path = s.picture_path;
    const long long t0 = now_ms();
    if (!porpoise::core::serialize(g_job.state))
        return false;
    const long long t1 = now_ms();
    if (!porpoise::core::capture_picture(g_job.rgba, g_job.width, g_job.height))
        g_job.rgba.clear();
    g_job.aspect = porpoise::core::picture_aspect() > 0.1f ? porpoise::core::picture_aspect() : 4.0f / 3.0f;
    char line[128];
    std::snprintf(line, sizeof line, "states: slot %d taken in %lld ms, picture copied in %lld ms", index + 1, t1 - t0,
                  now_ms() - t1);
    ps5::debug::mark(line);
    g_done = false;
    g_reported = false;
    g_ok = false;
    if (create_title_thread(&g_thread, writer, nullptr) == 0)
        g_running = true;
    else
        writer(nullptr); /* no thread: write it here */
    return true;
}

bool load(const std::string &game_key, int index)
{
    wait();
    const Slot s = slot(game_key, index);
    return s.exists && porpoise::core::load_state(s.state_path.c_str());
}

bool remove(const std::string &game_key, int index)
{
    wait();
    const Slot s = slot(game_key, index);
    std::remove(s.picture_path.c_str());
    return std::remove(s.state_path.c_str()) == 0;
}
std::string resume_path(const std::string &game_key)
{
    mkdir((g_data_dir + "/states").c_str(), 0777);
    mkdir(dir_of(game_key).c_str(), 0777);
    return dir_of(game_key) + "/resume.state";
}
} // namespace porpoise::states
