/* Porpoise - the DualSense's microphone for the Dolphin core: libretro's
 * microphone interface, behind which the console's audio input is read by a
 * thread of its own (the GameCube Microphone, the Wii Speak).
 *
 * libSceAudioIn isn't among the SDK's link libraries, so it is loaded when a
 * game first opens a microphone and its functions are looked up by name; on a
 * console or firmware without it, or when no microphone can be opened, the
 * game simply hears silence. Everything it does goes to trace.txt.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_mic.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

#include "title_threads.hpp"
#include "trace.hpp"

extern "C"
{
    int sceSysmoduleLoadModuleInternal(std::uint32_t id);
    int sceKernelLoadStartModule(const char *path, std::size_t args, const void *argp, std::uint32_t flags,
                                 void *option, int *result);
    int sceKernelDlsym(int handle, const char *name, void **address);
    std::int32_t sceUserServiceGetInitialUser(std::int32_t *user_id);
}

namespace porpoise::mic
{
namespace
{
using OpenFn = int (*)(std::int32_t user, std::uint32_t type, std::uint32_t index, std::uint32_t len,
                       std::uint32_t freq, std::uint32_t param);
using InputFn = int (*)(std::int32_t handle, void *dest);
using CloseFn = int (*)(std::int32_t handle);

constexpr std::uint32_t kGrain = 256;      /* samples per read from the console */
constexpr std::size_t kRing = 16000 * 2;   /* two seconds of 16 kHz mono */
constexpr int kMics = 4;                   /* the core opens one or two at a time */

/* One microphone as the core sees it: its rate, whether it's listening, and
 * where it is in the input (in the console's samples, fractional). Kept in
 * a fixed pool, never freed, so a read that races a close finds a valid one. */
struct Mic
{
    std::atomic<bool> used{false};
    unsigned rate = 16000;
    std::atomic<bool> active{false};
    double position = 0; /* under Capture::lock */
};

struct Capture
{
    /* lock: the ring and the mics' positions. state: opening, closing and
     * the count of microphones open (the core's emulation thread opens and
     * closes them; its own thread reads them). */
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t state = PTHREAD_MUTEX_INITIALIZER;
    bool loaded = false, failed = false;
    OpenFn open = nullptr;
    InputFn input = nullptr;
    CloseFn close = nullptr;
    int port = -1;
    unsigned rate = 16000; /* the console's input rate */
    pthread_t thread{};
    bool thread_started = false;
    std::atomic<bool> running{false}; /* the capture thread is reading */
    std::atomic<bool> stop{false};
    std::int16_t ring[kRing] = {};
    std::size_t head = 0, count = 0; /* under lock */
    int users = 0;                   /* under state */
    Mic mics[kMics];
};
Capture g;

bool load()
{
    if (g.loaded || g.failed)
        return g.loaded;
    const int sys = sceSysmoduleLoadModuleInternal(0x80000002); /* libSceAudioIn */
    int result = 0;
    const int handle =
        sceKernelLoadStartModule("/system/common/lib/libSceAudioIn.sprx", 0, nullptr, 0, nullptr, &result);
    ps5::debug::mark_value("mic: audio input module", handle);
    if (handle < 0)
    {
        ps5::debug::mark_value("mic: no audio input module (sysmodule said)", sys);
        g.failed = true;
        return false;
    }
    void *o = nullptr, *i = nullptr, *c = nullptr;
    sceKernelDlsym(handle, "sceAudioInOpen", &o);
    sceKernelDlsym(handle, "sceAudioInInput", &i);
    sceKernelDlsym(handle, "sceAudioInClose", &c);
    if (!o || !i || !c)
    {
        ps5::debug::mark("mic: the audio input functions aren't there");
        g.failed = true;
        return false;
    }
    g.open = reinterpret_cast<OpenFn>(o);
    g.input = reinterpret_cast<InputFn>(i);
    g.close = reinterpret_cast<CloseFn>(c);
    g.loaded = true;
    return true;
}

void *capture(void *)
{
    std::int16_t block[kGrain];
    while (!g.stop.load(std::memory_order_relaxed))
    {
        std::memset(block, 0, sizeof block);
        const int got = g.input(g.port, block); /* blocks for one grain */
        if (got < 0)
        {
            ps5::debug::mark_value("mic: input failed; the microphone stops", got);
            break;
        }
        /* The count it read; a firmware that answers 0 with the block filled
         * still counts as a grain, and an empty block isn't waited on. */
        std::uint32_t n = std::min<std::uint32_t>(kGrain, std::uint32_t(got));
        if (n == 0)
        {
            bool sound = false;
            for (std::uint32_t k = 0; k < kGrain && !sound; ++k)
                sound = block[k] != 0;
            if (!sound)
            {
                usleep(4000); /* nothing yet: don't spin */
                continue;
            }
            n = kGrain;
        }
        pthread_mutex_lock(&g.lock);
        for (std::uint32_t k = 0; k < n; ++k)
        {
            g.ring[g.head] = block[k];
            g.head = (g.head + 1) % kRing;
            if (g.count < kRing)
                ++g.count;
        }
        pthread_mutex_unlock(&g.lock);
    }
    g.running.store(false); /* a later open starts it again */
    return nullptr;
}

/* Under state. */
void finish_locked()
{
    if (!g.thread_started)
        return;
    g.stop.store(true);
    pthread_join(g.thread, nullptr); /* the blocking read returns within a grain */
    g.thread_started = false;
    g.running.store(false);
    if (g.port >= 0)
        g.close(g.port);
    g.port = -1;
    ps5::debug::mark("mic: closed");
}

/* Under state. */
bool start_locked()
{
    if (g.running.load())
        return true;
    finish_locked(); /* a thread that stopped by itself: tidy it away first */
    if (!load())
        return false;
    std::int32_t user = -1;
    sceUserServiceGetInitialUser(&user);
    /* The voice input at 16 kHz, then the general one; mono, 16-bit. */
    const struct
    {
        std::uint32_t type, rate;
    } tries[] = {{0, 16000}, {1, 16000}, {1, 48000}, {2, 16000}};
    for (const auto &t : tries)
    {
        g.port = g.open(user, t.type, 0, kGrain, t.rate, 0);
        char line[96];
        std::snprintf(line, sizeof line, "mic: open type %u at %u Hz: %d", t.type, t.rate, g.port);
        ps5::debug::mark(line);
        if (g.port >= 0)
        {
            g.rate = t.rate;
            break;
        }
    }
    if (g.port < 0)
        return false;
    g.stop.store(false);
    pthread_mutex_lock(&g.lock);
    g.head = g.count = 0;
    pthread_mutex_unlock(&g.lock);
    g.running.store(true);
    g.thread_started = create_title_thread(&g.thread, capture, nullptr) == 0;
    if (!g.thread_started)
    {
        g.running.store(false);
        g.close(g.port);
        g.port = -1;
    }
    return g.thread_started;
}

retro_microphone_t *RETRO_CALLCONV open_mic(const retro_microphone_params_t *params)
{
    pthread_mutex_lock(&g.state);
    Mic *m = nullptr;
    if (start_locked())
        for (Mic &slot : g.mics)
        {
            bool expected = false;
            if (slot.used.compare_exchange_strong(expected, true))
            {
                m = &slot;
                break;
            }
        }
    if (m)
    {
        m->rate = params && params->rate >= 4000 && params->rate <= 96000 ? params->rate : 16000;
        m->active.store(false);
        pthread_mutex_lock(&g.lock);
        m->position = 0;
        pthread_mutex_unlock(&g.lock);
        ++g.users;
    }
    pthread_mutex_unlock(&g.state);
    return reinterpret_cast<retro_microphone_t *>(m);
}

void RETRO_CALLCONV close_mic(retro_microphone_t *mic)
{
    if (!mic)
        return;
    Mic *m = reinterpret_cast<Mic *>(mic);
    pthread_mutex_lock(&g.state);
    m->active.store(false);
    if (m->used.exchange(false) && --g.users <= 0)
    {
        g.users = 0;
        finish_locked();
    }
    pthread_mutex_unlock(&g.state);
}

bool RETRO_CALLCONV get_params(const retro_microphone_t *mic, retro_microphone_params_t *params)
{
    if (!mic || !params)
        return false;
    params->rate = reinterpret_cast<const Mic *>(mic)->rate;
    return true;
}

bool RETRO_CALLCONV set_state(retro_microphone_t *mic, bool state)
{
    if (!mic)
        return false;
    Mic *m = reinterpret_cast<Mic *>(mic);
    m->active.store(state);
    if (state)
    {
        /* Fresh sound from now: what was said before it listened isn't heard. */
        pthread_mutex_lock(&g.lock);
        g.count = 0;
        m->position = 0;
        pthread_mutex_unlock(&g.lock);
    }
    return true;
}

bool RETRO_CALLCONV get_state(const retro_microphone_t *mic)
{
    return mic && reinterpret_cast<const Mic *>(mic)->active.load();
}

/* As many samples as are there (up to n), at the microphone's own rate. */
int RETRO_CALLCONV read_mic(retro_microphone_t *mic, std::int16_t *out, std::size_t n)
{
    if (!mic || !out)
        return -1;
    Mic &m = *reinterpret_cast<Mic *>(mic);
    if (!m.used.load() || !m.active.load() || !g.running.load())
        return 0;
    const double step = double(g.rate) / double(m.rate);
    pthread_mutex_lock(&g.lock);
    /* No more than about a seventh of a second waiting: a game that reads
     * slowly hears what was said just now, not seconds ago. */
    const std::size_t keep = g.rate / 7;
    if (g.count > keep)
    {
        g.count = keep;
        m.position = 0;
    }
    std::size_t made = 0;
    const std::size_t tail = (g.head + kRing - g.count) % kRing;
    while (made < n)
    {
        const std::size_t at = std::size_t(m.position);
        if (at + 1 >= g.count)
            break;
        const double f = m.position - double(at);
        const std::int16_t a = g.ring[(tail + at) % kRing], b = g.ring[(tail + at + 1) % kRing];
        out[made++] = std::int16_t(a + (b - a) * f);
        m.position += step;
    }
    /* What was read leaves the ring. */
    const std::size_t used = std::min(g.count, std::size_t(m.position));
    g.count -= used;
    m.position -= double(used);
    pthread_mutex_unlock(&g.lock);
    return int(made);
}
} // namespace

void fill(retro_microphone_interface &iface)
{
    iface.open_mic = open_mic;
    iface.close_mic = close_mic;
    iface.get_params = get_params;
    iface.set_mic_state = set_state;
    iface.get_mic_state = get_state;
    iface.read_mic = read_mic;
}

void close_all()
{
    pthread_mutex_lock(&g.state);
    for (Mic &m : g.mics)
    {
        m.active.store(false);
        m.used.store(false);
    }
    g.users = 0;
    finish_locked();
    pthread_mutex_unlock(&g.state);
}
} // namespace porpoise::mic
