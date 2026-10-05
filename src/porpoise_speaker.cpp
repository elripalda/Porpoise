/* Porpoise - the Wii Remotes' speakers on the DualSense's own speaker.
 *
 * With Dolphin's audio routing on (Dolphin.ini: Core/WiimoteAudioRoutingEnabled
 * and WiimoteNAudioOutputEnabled), the core keeps each Remote's sound out of
 * the TV's; the patched core hands it over (porpoise_mix_wiimote_speaker, in
 * patches/dolphin). A thread pulls it, makes it 48 kHz mono and writes it to
 * the speaker port of the controller of that player (AudioOut's pad speaker,
 * as PS4 games use it). When no controller speaker opens, Porpoise leaves the
 * routing off and the Remote's sounds stay in the TV's sound.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_speaker.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <pthread.h>

#include "porpoise_pad.hpp"
#include "title_threads.hpp"
#include "trace.hpp"

extern "C"
{
    std::int32_t sceAudioOutOpen(std::int32_t, std::int32_t, std::int32_t, std::uint32_t, std::uint32_t,
                                 std::uint32_t);
    std::int32_t sceAudioOutOutput(std::int32_t, const void *);
    std::int32_t sceAudioOutClose(std::int32_t);
}

namespace porpoise::speaker
{
namespace
{
constexpr std::int32_t kPadSpeaker = 4; /* SCE_AUDIO_OUT_PORT_TYPE_PADSPK */
constexpr std::uint32_t kMono16 = 0;    /* SCE_AUDIO_OUT_PARAM_FORMAT_S16_MONO */
constexpr std::uint32_t kGrain = 256;
constexpr int kPlayers = 4;

struct State
{
    int port[kPlayers] = {-1, -1, -1, -1};
    MixFn mix = nullptr;
    RateFn rate = nullptr;
    pthread_t thread{};
    bool running = false;
    std::atomic<bool> stop{false};
    double position[kPlayers] = {};
    std::int16_t last[kPlayers] = {};
};
State s;

void *worker(void *)
{
    std::int16_t stereo[kGrain * 2 * 2];
    std::int16_t mono[kGrain];
    while (!s.stop.load(std::memory_order_relaxed))
    {
        const unsigned in_rate = s.rate ? s.rate() : 0;
        bool wrote = false;
        for (int p = 0; p < kPlayers; ++p)
        {
            if (s.port[p] < 0)
                continue;
            /* The Remote's sound at the mixer's rate, made 48 kHz for the speaker. */
            std::size_t got = 0;
            if (in_rate >= 8000)
            {
                const std::size_t want = std::min<std::size_t>(kGrain * 2, (kGrain * in_rate + 47999) / 48000);
                got = s.mix(unsigned(p), stereo, want);
            }
            if (got == 0)
                std::memset(mono, 0, sizeof mono);
            else
            {
                const double step = double(got) / double(kGrain);
                for (std::uint32_t i = 0; i < kGrain; ++i)
                {
                    const std::size_t at = std::min(got - 1, std::size_t(double(i) * step));
                    mono[i] = std::int16_t((int(stereo[at * 2]) + int(stereo[at * 2 + 1])) / 2);
                }
            }
            if (sceAudioOutOutput(s.port[p], mono) < 0)
            {
                ps5::debug::mark_value("speaker: output failed for player", p + 1);
                sceAudioOutClose(s.port[p]);
                s.port[p] = -1;
                continue;
            }
            wrote = true;
        }
        if (!wrote)
            break; /* every port closed */
    }
    return nullptr;
}
} // namespace

int open_ports()
{
    close_ports();
    int opened = 0;
    for (int p = 0; p < kPlayers; ++p)
    {
        const std::int32_t user = porpoise::pad::user_of(p);
        if (user < 0)
            continue;
        s.port[p] = sceAudioOutOpen(user, kPadSpeaker, 0, kGrain, 48000, kMono16);
        char line[96];
        std::snprintf(line, sizeof line, "speaker: player %d's controller speaker: %d", p + 1, s.port[p]);
        ps5::debug::mark(line);
        if (s.port[p] >= 0)
            ++opened;
    }
    return opened;
}

bool open(int player)
{
    return player >= 0 && player < kPlayers && s.port[player] >= 0;
}

void start(MixFn mix, RateFn rate)
{
    if (s.running || !mix || !rate)
        return;
    bool any = false;
    for (int p = 0; p < kPlayers; ++p)
        any |= s.port[p] >= 0;
    if (!any)
        return;
    s.mix = mix;
    s.rate = rate;
    s.stop.store(false);
    s.running = create_title_thread(&s.thread, worker, nullptr) == 0;
    ps5::debug::mark_value("speaker: Remote sounds on the controllers", s.running ? 1 : 0);
}

void stop()
{
    if (s.running)
    {
        s.stop.store(true);
        pthread_join(s.thread, nullptr);
        s.running = false;
    }
    s.mix = nullptr;
    s.rate = nullptr;
}

void close_ports()
{
    stop();
    for (int p = 0; p < kPlayers; ++p)
        if (s.port[p] >= 0)
        {
            sceAudioOutClose(s.port[p]);
            s.port[p] = -1;
        }
}
} // namespace porpoise::speaker
