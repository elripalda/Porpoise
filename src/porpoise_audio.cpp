/* Porpoise - game audio to the console's speakers.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * AudioOut calls and their order follow Mihawk's PS5 RetroArch (src/audio_ps5.cpp),
 * which follows ProsperoLight's moonlight_stream.cpp (GPL-3.0-or-later): a
 * 48 kHz stereo s16 port written 256 frames at a time by its own thread, the
 * blocking write being what paces it.
 *
 * Dolphin pushes samples at its own rate once a frame (32 kHz by default). They
 * are resampled to 48 kHz here, linearly, with a ratio nudged by at most 0.5%
 * toward a target queue depth. That absorbs the difference between the game's
 * frame rate (59.94 Hz) and the display's (60 Hz) without pops or drift. */
#include "porpoise_audio.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <pthread.h>
#include <time.h>

#include "title_threads.hpp"
#include "trace.hpp"

extern "C"
{
    std::int32_t sceAudioOutInit();
    std::int32_t sceAudioOutOpen(std::int32_t, std::int32_t, std::int32_t, std::uint32_t,
                                 std::uint32_t, std::uint32_t);
    std::int32_t sceAudioOutOutput(std::int32_t, const void *);
    std::int32_t sceAudioOutClose(std::int32_t);
}

namespace
{
constexpr unsigned out_rate = 48000;
constexpr std::size_t grain = 256;                 /* frames per AudioOut write */
constexpr std::size_t ring_frames = 16384;         /* ~340 ms */
constexpr std::size_t target_frames = 2304;        /* ~48 ms queued */
constexpr std::uint32_t already_initialized = 0x8026000e;

struct Audio
{
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_t thread{};
    bool thread_started = false;
    std::atomic<bool> stop{false};
    int port = -1;
    std::int16_t ring[ring_frames * 2] = {};
    std::size_t head = 0, count = 0; /* guarded by mutex */
    alignas(16) std::int16_t out[grain * 2] = {};
    /* Resampler, touched only by push(). */
    double source_rate = 32000.0;
    double position = 0.0;            /* fractional read position between prev and next */
    std::int16_t prev[2] = {0, 0};
    std::atomic<float> volume{1.0f};
    std::atomic<bool> muted{false};
};

Audio *g = nullptr;

void *worker(void *)
{
    Audio &a = *g;
    while (!a.stop.load(std::memory_order_relaxed))
    {
        pthread_mutex_lock(&a.mutex);
        const std::size_t take = std::min(grain, a.count);
        std::size_t tail = (a.head + ring_frames - a.count) % ring_frames;
        for (std::size_t i = 0; i < take; ++i)
        {
            a.out[i * 2] = a.ring[tail * 2];
            a.out[i * 2 + 1] = a.ring[tail * 2 + 1];
            tail = (tail + 1) % ring_frames;
        }
        a.count -= take;
        pthread_mutex_unlock(&a.mutex);
        if (take < grain)
            std::memset(a.out + take * 2, 0, (grain - take) * 2 * sizeof(std::int16_t));

        const float volume = a.muted.load(std::memory_order_relaxed)
                                 ? 0.0f
                                 : a.volume.load(std::memory_order_relaxed);
        if (volume < 0.999f)
            for (std::size_t i = 0; i < grain * 2; ++i)
                a.out[i] = static_cast<std::int16_t>(a.out[i] * volume);

        if (sceAudioOutOutput(a.port, a.out) < 0)
        {
            ps5::debug::mark("audio: output failed; audio stops");
            break;
        }
    }
    return nullptr;
}

void put(Audio &a, std::int16_t l, std::int16_t r)
{
    /* Caller holds the mutex. A full ring drops the oldest frame. */
    a.ring[a.head * 2] = l;
    a.ring[a.head * 2 + 1] = r;
    a.head = (a.head + 1) % ring_frames;
    if (a.count < ring_frames)
        ++a.count;
}
} // namespace

namespace porpoise::audio
{
bool open()
{
    if (g)
        return true;
    g = new Audio();
    const int init = sceAudioOutInit();
    if (init < 0 && static_cast<std::uint32_t>(init) != already_initialized)
    {
        ps5::debug::mark_value("audio: sceAudioOutInit failed", init);
        return false;
    }
    /* 0xff: the system user; type 0: main output; param 1: s16 stereo. */
    g->port = sceAudioOutOpen(0xff, 0, 0, grain, out_rate, 1);
    if (g->port < 0)
    {
        ps5::debug::mark_value("audio: sceAudioOutOpen failed", g->port);
        return false;
    }
    g->thread_started = create_title_thread(&g->thread, worker, nullptr) == 0;
    ps5::debug::mark_value("audio: 48 kHz port open, thread", g->thread_started ? 1 : 0);
    return g->thread_started;
}

void close()
{
    if (!g)
        return;
    g->stop.store(true);
    if (g->thread_started)
        pthread_join(g->thread, nullptr);
    if (g->port >= 0)
    {
        (void)sceAudioOutOutput(g->port, nullptr);
        (void)sceAudioOutClose(g->port);
    }
    delete g;
    g = nullptr;
}

void set_source_rate(double hz)
{
    if (g && hz > 1000.0)
        g->source_rate = hz;
}

double source_rate()
{
    return g ? g->source_rate : 48000.0;
}

void push(const std::int16_t *frames, std::size_t count)
{
    if (!g || !g->thread_started || count == 0)
        return;
    Audio &a = *g;
    pthread_mutex_lock(&a.mutex);
    /* Nudge the ratio toward the target depth: a fuller queue plays slightly
     * slower input (fewer output frames), an emptier one slightly more. */
    const double fill_error = (static_cast<double>(target_frames) - static_cast<double>(a.count)) /
                              static_cast<double>(target_frames);
    const double nudge = std::clamp(fill_error * 0.005, -0.005, 0.005);
    const double step = a.source_rate / out_rate * (1.0 - nudge);

    for (std::size_t i = 0; i < count; ++i)
    {
        const std::int16_t nl = frames[i * 2], nr = frames[i * 2 + 1];
        while (a.position < 1.0)
        {
            const double t = a.position;
            put(a, static_cast<std::int16_t>(a.prev[0] + (nl - a.prev[0]) * t),
                static_cast<std::int16_t>(a.prev[1] + (nr - a.prev[1]) * t));
            a.position += step;
        }
        a.position -= 1.0;
        a.prev[0] = nl;
        a.prev[1] = nr;
    }
    pthread_mutex_unlock(&a.mutex);
}

void set_volume(float volume)
{
    if (g)
        g->volume.store(std::clamp(volume, 0.0f, 1.0f));
}

void set_muted(bool muted)
{
    if (g)
        g->muted.store(muted);
}

std::size_t queued()
{
    if (!g)
        return 0;
    pthread_mutex_lock(&g->mutex);
    const std::size_t n = g->count;
    pthread_mutex_unlock(&g->mutex);
    return n;
}

long wait_below(std::size_t frames, int timeout_ms)
{
    if (!g || !g->thread_started)
        return 0;
    long waited = 0;
    while (queued() > frames && waited < long(timeout_ms) * 1000)
    {
        timespec ts{0, 500000}; /* half a millisecond: a 256-frame grain is 5.3 ms */
        nanosleep(&ts, nullptr);
        waited += 500;
    }
    return waited;
}

void flush()
{
    if (!g)
        return;
    pthread_mutex_lock(&g->mutex);
    g->count = 0;
    pthread_mutex_unlock(&g->mutex);
}
} // namespace porpoise::audio
