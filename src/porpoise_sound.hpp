/* Porpoise - the launcher's own sound: menu effects and the menu music.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace porpoise::sound
{
enum class Effect
{
    GameRow,     /* moving along the library */
    MenuScroll,  /* moving through a menu, changing a value */
    MovingTab,   /* Library / Memory Cards / Settings */
    DetailsFlip, /* into and out of a game's details, the in-game menu */
    LaunchGame,  /* Play */
    Count,
};

/* Loads <assets>/sounds/: the effects (48 kHz stereo WAV) and the music (Ogg). */
bool load(const std::string &asset_dir);
void play(Effect e);
/* An effect's sound in place of its own (48 kHz interleaved stereo); a sound
 * set (porpoise_sfx). */
void set_effect(Effect e, const std::vector<std::int16_t> &frames);
/* Back to the effects load() read: Porpoise's own. */
void use_own_effects();
/* A Wii disc's banner jingle (48 kHz stereo frames, copied); null stops it.
 * The music steps back while it plays. */
void play_jingle(const std::int16_t *frames, std::size_t count);
/* volume 0..1 */
void set_music(bool on, float volume);
void set_effects(bool on, float volume);
/* Fades the music toward 0..1 over seconds (out before a game, in after). */
void fade_music(float to, float seconds);
/* Once per frame while Porpoise's screens are up: mixes what the speakers
 * will need next and hands it to the audio port. */
void pump();
} // namespace porpoise::sound
