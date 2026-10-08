/* Porpoise - a home screen tile's art: what its icon and background are made
 * of (the player's choices in the tile art editor), the pictures made from
 * them, and libretro's screenshots and title screens, downloaded on request.
 *
 * Each part (the icon, the background) is a Layer: a picture (the cover, the
 * back of the box, a screenshot, a title screen, the player's own file), and
 * how it sits: filling the space (cropped, moved and zoomed by the player) or
 * whole, with something behind it (the picture blurred, white, black, or a
 * pattern of Porpoise's dolphin). A game's choices are kept in
 * <data>/home-art/<ID>.tile.
 *
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace porpoise::tileart
{
enum Source
{
    Cover = 0,
    Back = 1,        /* the back of the box (GameTDB's full scan) */
    Screenshot = 2,  /* libretro's */
    TitleScreen = 3, /* libretro's */
    File = 4,        /* the player's own picture */
    Porpoise = 5,    /* the background only: Porpoise's own */
};
constexpr int kSources = 6;
enum Fit
{
    Whole = 0, /* all of it, with something behind */
    Fill = 1,  /* filling the space, cropped */
};
enum Behind
{
    Blur = 0,
    White = 1,
    Black = 2,
    Pattern = 3,
};
constexpr int kBehinds = 4;

struct Layer
{
    int source = Cover;
    std::string file; /* Source::File */
    int fit = Whole;
    int behind = Blur;
    /* Whole: the picture's centre on the space (0..1) and its size (1: as
     * big as fits). Fill: the point of the picture at the middle, and how far
     * in (1: just filling). */
    float zoom = 1.0f, x = 0.5f, y = 0.5f;
};
struct Spec
{
    Layer icon, bg;
};

/* As tiles were made before the editor: the cover, whole, over its blur; on
 * the background, to the right. */
Spec defaults();
void reset_position(Layer &layer, bool background);
/* A game's choices; the defaults (and an own picture already in home-art
 * under the old names, <ID>-icon / <ID>-background) when it has none. */
Spec load(const std::string &data_dir, const std::string &game_id);
bool save(const std::string &data_dir, const std::string &game_id, const Spec &spec);

struct Picture
{
    int w = 0, h = 0;
    std::vector<std::uint8_t> px; /* RGBA */
    bool empty() const { return w <= 0 || h <= 0; }
};
bool load_picture(const std::string &path, Picture &out, int max_side = 0);
Picture scaled(const Picture &src, int w, int h);

/* The file a layer's picture is in ("" when it has none yet). */
std::string picture_path(const Layer &layer, const std::string &data_dir, const std::string &game_id,
                         const std::string &cover_path);
/* Where libretro's screenshot / title screen of a game is kept. */
std::string download_path(const std::string &data_dir, const std::string &game_id, int source);

/* The layer made into a w x h picture (opaque). False when src is empty. */
bool compose(const Layer &layer, const Picture &src, int w, int h, bool background, Picture &out);
/* Where Porpoise's own pictures are (its dolphin, for the pattern). */
void set_asset_dir(const std::string &dir);

/* libretro's screenshot or title screen of a game, downloaded in the
 * background into download_path(). One at a time. */
struct Download
{
    std::string data_dir, game_id, game_path, title;
    bool wii = false;
    int source = Screenshot;
};
void download(const Download &request);
/* 0 idle, 1 downloading, 2 saved, 3 libretro hasn't got it, 4 offline or failed.
 * Reading 2-4 makes it idle again. */
int download_state(bool take = false);
} // namespace porpoise::tileart
