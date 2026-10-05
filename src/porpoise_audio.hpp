/* Porpoise - game audio to the console's speakers.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstddef>
#include <cstdint>

namespace porpoise::audio
{
bool open();
void close();
/* The rate the core produces, from retro_system_av_info. */
void set_source_rate(double hz);
double source_rate();
/* Interleaved stereo s16 frames from the core. Never blocks. */
void push(const std::int16_t *frames, std::size_t count);
/* 0.0 .. 1.0, applied at output. */
/* How much sound is kept queued: 0 low, 1 normal, 2 safe (Settings > Audio). */
void set_buffer(int level);
/* The queue depth the resampler aims for, in 48 kHz frames. */
std::size_t target_frames();
/* Drawing the sound out when the game runs slow, rather than crackling. */
void set_stretching(bool on);
void set_volume(float volume);
void set_muted(bool muted);
/* Drop what is queued (pause, menu, loading). */
void flush();
/* Frames waiting for the speakers (48 kHz). */
std::size_t queued();
/* Blocks while more than `frames` are waiting, at most timeout_ms: the
 * speakers are the clock that keeps a game at its real speed. Returns the
 * time spent waiting, in microseconds. */
long wait_below(std::size_t frames, int timeout_ms);
} // namespace porpoise::audio
