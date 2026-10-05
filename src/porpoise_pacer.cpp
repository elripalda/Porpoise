/* Porpoise - frame pacing: one frame per vblank when the display allows it.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_pacer.hpp"

#include <cmath>
#include <cstdio>
#include <time.h>

#include "porpoise_vk.hpp"
#include "trace.hpp"

namespace porpoise::pacer
{
namespace
{
constexpr int kProbeSkip = 20;     /* the first presents find free images anyway */
constexpr int kProbeFrames = 120;  /* about two seconds */
constexpr double kHeldMs = 0.25;   /* a present that waited at least this was held */
constexpr long long kReprobeNs = 20000000000LL; /* on its own clock, try the vblank again this often */
bool g_vsync = true;
} // namespace

void set_vsync(bool on)
{
    g_vsync = on;
}

long long now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<long long>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

void sleep_until_ns(long long deadline)
{
    for (;;)
    {
        const long long left = deadline - now_ns();
        if (left <= 0)
            return;
        if (left > 1500000)
        {
            const long long nap = left - 1000000;
            timespec ts{static_cast<time_t>(nap / 1000000000LL), static_cast<long>(nap % 1000000000LL)};
            nanosleep(&ts, nullptr);
        }
    }
}

void Pacer::start(double content_hz, const char *who)
{
    who_ = who;
    const double content = content_hz > 10.0 && content_hz < 240.0 ? content_hz : 60.0;
    const double display = porpoise::vk::display_hz();
    period_ns_ = static_cast<long long>(1e9 / content);
    vblank_ns_ = static_cast<long long>(1e9 / (display > 10.0 ? display : 60.0));
    /* The display can be the clock when it runs at the content's own rate
     * (59.94 against 60 is close enough; 50 against 60 or 120 is not). */
    const double ratio = display / content;
    compatible_ = ratio > 0.995 && ratio < 1.012;
    mode_ = compatible_ ? Mode::Probing : Mode::Clock;
    probe_frames_ = probe_held_ = 0;
    misses_ = 0;
    vblank_retry_ = 0;
    char line[160];
    std::snprintf(line, sizeof line, "pacer: %s at %.3f Hz on a %.3f Hz display: %s", who_, content, display,
                  !g_vsync          ? "own clock (V-Sync off)"
                  : mode_ == Mode::Probing ? "V-Sync" : "own clock (rates differ)");
    ps5::debug::mark(line);
    if (!g_vsync)
        mode_ = Mode::Clock;
    resync();
    if (g_vsync && compatible_ && porpoise::vk::vblank_ready())
        enter_vblank();
}

void Pacer::enter_vblank()
{
    mode_ = Mode::Vblank;
    quick_vblanks_ = 0;
    if (porpoise::vk::wait_vblank())
        last_vblank_ns_ = now_ns();
    else
        last_vblank_ns_ = now_ns() - vblank_ns_ / 2;
    char line[128];
    std::snprintf(line, sizeof line, "pacer: %s: V-Sync on the display's vblank", who_);
    ps5::debug::mark(line);
}

/* V-Sync: a frame done before the vblank it is meant for waits for it, so
 * the next one starts just after it and every frame is on screen for one
 * vblank. A frame done after it doesn't wait (waiting would cost a whole
 * vblank more and halve the speed of a game that is just short of 60): the
 * vblank it missed is counted and the next one becomes the aim. */
void Pacer::vblank_frame()
{
    const long long now = now_ns();
    const long long due = last_vblank_ns_ + vblank_ns_;
    if (now < due)
    {
        if (!porpoise::vk::wait_vblank())
        {
            mode_ = Mode::Clock;
            clock_since_ns_ = now;
            deadline_ns_ = now;
            ps5::debug::mark("pacer: the vblank wait failed: own clock");
            return;
        }
        const long long after = now_ns();
        /* A wait that ends at once when the vblank was still well away is not
         * a vblank wait: after a few, back to the own clock (a loop that
         * isn't held mustn't run fast). */
        if (after - now < 300000 && due - now > 3000000)
        {
            if (++quick_vblanks_ > 20)
            {
                mode_ = Mode::Clock;
                clock_since_ns_ = after;
                deadline_ns_ = after;
                ps5::debug::mark("pacer: the vblank wait doesn't wait: own clock");
                return;
            }
        }
        else
            quick_vblanks_ = 0;
        last_vblank_ns_ = after;
        last_ns_ = after;
        return;
    }
    ++misses_;
    last_vblank_ns_ += ((now - last_vblank_ns_) / vblank_ns_) * vblank_ns_;
    last_ns_ = now;
}

void Pacer::resync()
{
    clock_since_ns_ = now_ns();
    last_ns_ = deadline_ns_ = window_start_ns_ = now_ns();
    last_vblank_ns_ = now_ns() - vblank_ns_ / 2;
    window_frames_ = 0;
    if (mode_ == Mode::Probing)
        probe_frames_ = probe_held_ = 0;
}

void Pacer::frame_done()
{
    if (mode_ == Mode::Vblank)
    {
        vblank_frame();
        return;
    }
    /* V-Sync wanted but the display's output wasn't found yet (it opens with
     * the first swapchain): look again now and then. */
    if (g_vsync && compatible_ && ++vblank_retry_ % 60 == 0 && porpoise::vk::vblank_ready())
    {
        enter_vblank();
        return;
    }
    if (g_vsync && mode_ == Mode::Clock && compatible_ && now_ns() - clock_since_ns_ > kReprobeNs)
    {
        /* A slow stretch (loading, a heavy scene) can make the probe miss a
         * display that does hold the loop: try again now and then. */
        mode_ = Mode::Probing;
        probe_frames_ = probe_held_ = 0;
        last_ns_ = now_ns();
    }
    if (mode_ == Mode::Clock)
    {
        deadline_ns_ += period_ns_;
        const long long now = now_ns();
        if (now > deadline_ns_ + 4 * period_ns_)
            deadline_ns_ = now; /* fell behind: start again from here */
        else
            sleep_until_ns(deadline_ns_);
        last_ns_ = now_ns();
        return;
    }

    if (mode_ == Mode::Probing)
    {
        ++probe_frames_;
        if (probe_frames_ > kProbeSkip && porpoise::vk::last_present_wait_ms() >= kHeldMs)
            ++probe_held_;
        if (probe_frames_ >= kProbeSkip + kProbeFrames)
        {
            const bool held = probe_held_ * 2 >= kProbeFrames;
            mode_ = held ? Mode::Locked : Mode::Clock;
            clock_since_ns_ = now_ns();
            char line[160];
            std::snprintf(line, sizeof line, "pacer: %s: the display held %d of %d presents: %s", who_, probe_held_,
                          kProbeFrames, held ? "locked to the vblank" : "own clock");
            ps5::debug::mark(line);
            deadline_ns_ = now_ns();
            window_start_ns_ = deadline_ns_;
            window_frames_ = 0;
        }
    }
    else if (mode_ == Mode::Locked)
    {
        /* Held to the vblank, the loop runs at the display's rate. If it runs
         * clearly faster over a few seconds, the display isn't holding it
         * after all: back to a clock of our own. */
        ++window_frames_;
        const long long now = now_ns();
        const long long span = now - window_start_ns_;
        if (span >= 3000000000LL)
        {
            const double rate = window_frames_ * 1e9 / double(span);
            if (rate > 1.012e9 / double(vblank_ns_))
            {
                mode_ = Mode::Clock;
                clock_since_ns_ = now;
                char line[128];
                std::snprintf(line, sizeof line, "pacer: %s ran at %.1f frames/s, faster than the display: own clock",
                              who_, rate);
                ps5::debug::mark(line);
                deadline_ns_ = now;
                last_ns_ = now;
                return;
            }
            window_start_ns_ = now;
            window_frames_ = 0;
        }
    }
    /* The floor: a frame never takes less than 96% of a vblank. A loop the
     * display holds waits in the acquire anyway; one it doesn't can run at
     * most 4% fast until the checks above catch it. */
    sleep_until_ns(last_ns_ + vblank_ns_ * 24 / 25);
    last_ns_ = now_ns();
}
} // namespace porpoise::pacer
