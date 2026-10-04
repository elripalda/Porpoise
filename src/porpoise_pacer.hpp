/* Porpoise - frame pacing: one frame per vblank when the display allows it.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A frame looks smooth when every frame stays on screen for the same number
 * of vblanks. Timing frames with a clock of our own cannot promise that: the
 * clock and the display drift apart, a frame now and then lands a vblank late
 * and the next one early, and movement judders for a moment every few
 * seconds - with the game itself at full speed.
 *
 * The console's swapchain flips at the vblank and an acquire waits for a free
 * image, so when the content rate matches the display (a 59.94 or 60 Hz game
 * on a 60 Hz mode) the display itself can be the clock: present, and the next
 * acquire holds the loop until the vblank. The pacer checks that the
 * swapchain really does hold it (the present waits) for its first couple of
 * seconds; if it doesn't, or the rates don't match (a 50 Hz game, a 120 Hz
 * mode), it falls back to a clock of its own, the way Porpoise always paced.
 * A floor of 80% of a vblank keeps a loop that is not held from racing.
 *
 * Locked to the display, a 59.94 Hz game runs 0.1% fast; the audio
 * resampler's rate control absorbs that without anyone hearing it. */
#pragma once

namespace porpoise::pacer
{
class Pacer
{
public:
    /* content_hz: how often a new frame is due (a game's rate, capped at 60). */
    void start(double content_hz, const char *who);
    /* After every present: waits as long as this frame needs to. */
    void frame_done();
    /* After a pause or a stall: start counting again from now. */
    void resync();
    bool locked() const { return mode_ == Mode::Locked || mode_ == Mode::Probing; }
    bool display_locked() const { return mode_ == Mode::Locked; }

private:
    enum class Mode
    {
        Probing,
        Locked,
        Clock,
    };
    Mode mode_ = Mode::Clock;
    long long period_ns_ = 16666667;  /* content */
    long long vblank_ns_ = 16666667;  /* display */
    long long last_ns_ = 0, deadline_ns_ = 0;
    int probe_frames_ = 0, probe_held_ = 0;
    long long window_start_ns_ = 0; /* locked: frames over the last window, to catch a loop the display stopped holding */
    int window_frames_ = 0;
    const char *who_ = "";
};

long long now_ns();
/* Sleeps most of the way, then spins the last stretch: a sleep can overshoot
 * by up to a millisecond. */
void sleep_until_ns(long long deadline);
} // namespace porpoise::pacer
