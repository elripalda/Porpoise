/* Porpoise - a Wii disc's own tile and banner: its opening.bnr read from the
 * disc (porpoise_disc), its layouts and animations played in software, a
 * frame at a time, and its jingle.
 *
 * The opening.bnr, layout (brlyt), animation (brlan) and sound (BNS) formats
 * follow the Wii Banner Player Project (zlib licence; see
 * third_party/wii-banner-player), rewritten here as a small software
 * renderer with its own TEV.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace porpoise::banner
{
/* The opening.bnr of a disc image: from the cache (<cache_dir>/<id>.bnr)
 * or read from the disc and kept there. false when the disc has none (also
 * remembered, as <id>.none, so a disc is read once). */
bool load(const std::string &image, const std::string &id, const std::string &cache_dir,
          std::vector<std::uint8_t> &bnr, std::string &error);

/* A tile (icon) or banner, played: its layout and animations, drawn into a
 * w x h (16:9) picture at any moment. */
class Player
{
public:
    Player();
    ~Player();
    /* lang: the layout's language group to show ("ENG", "FRA", ...). */
    bool open(const std::vector<std::uint8_t> &bnr, bool icon, const std::string &lang, std::string &error);
    /* frame: sixtieths of a second since it started (its intro plays once,
     * then its loop). rgba: w * h * 4 bytes. */
    void render(double frame, int w, int h, std::uint8_t *rgba);
    float intro_frames() const;
    float loop_frames() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/* The banner's jingle, as 48 kHz stereo samples. */
bool sound(const std::vector<std::uint8_t> &bnr, std::vector<std::int16_t> &pcm, std::string &error);

/* ---- playing in the background ------------------------------------------------------------------- */

enum class Kind
{
    Icon,   /* 160 x 90, 15 frames a second */
    Banner, /* 448 x 252, 20 frames a second, and its jingle */
};
void set_cache_dir(const std::string &dir);
void set_language(const std::string &lang);

/* The menus want this disc's tile or banner on screen now: workers read and
 * play it, a frame at a time, as long as it keeps being wanted (a second
 * without and it stops). restart: from its first frame (a banner opening). */
void want(const std::string &id, const std::string &image, Kind kind, bool restart = false);
/* The newest frame, when it is newer than serial (which it updates). */
bool frame(const std::string &id, Kind kind, std::vector<std::uint8_t> &rgba, int &w, int &h, std::uint64_t &serial);
/* The disc has no tile or banner Porpoise can play (or it failed). */
bool failed(const std::string &id, Kind kind);
/* The banner's jingle, once it is decoded (taken once). */
bool take_jingle(const std::string &id, std::vector<std::int16_t> &pcm);
/* Whether a disc is known to have no banner (a .none in the cache). */
bool known_none(const std::string &id);
/* Pauses the workers (a game is starting) or lets them go on. */
void pause(bool paused);
} // namespace porpoise::banner
