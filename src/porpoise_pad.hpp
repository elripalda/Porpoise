/* Porpoise - the DualSense, as libretro joypad and analog state.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>

namespace porpoise::pad
{
/* How the face buttons reach the GameCube pad.
 *
 * Dolphin binds GameCube A to libretro's A and GameCube B to libretro's B
 * (Source/Core/DolphinLibretro/Input.cpp). By libretro's Super Nintendo naming,
 * A is the right face button, so out of the box GameCube A lands on Circle.
 *
 *   GameCube: Cross = A, Square = B, Circle = X, Triangle = Y. The confirm
 *             button sits where a PlayStation player expects it. The default.
 *   Position: the libretro placement - Circle = A, Cross = B, Triangle = X,
 *             Square = Y. */
enum class Layout
{
    GameCube,
    Position,
};

/* Physical buttons, for Porpoise's own screens (independent of Layout). */
enum Button : std::uint32_t
{
    BtnUp = 1u << 0,
    BtnDown = 1u << 1,
    BtnLeft = 1u << 2,
    BtnRight = 1u << 3,
    BtnCross = 1u << 4,
    BtnCircle = 1u << 5,
    BtnSquare = 1u << 6,
    BtnTriangle = 1u << 7,
    BtnL1 = 1u << 8,
    BtnR1 = 1u << 9,
    BtnL2 = 1u << 10,
    BtnR2 = 1u << 11,
    BtnOptions = 1u << 12,
    BtnTouch = 1u << 13,
};

struct State
{
    bool connected = false;
    std::uint16_t joypad = 0; /* bit n = RETRO_DEVICE_ID_JOYPAD_n */
    std::int16_t left_x = 0, left_y = 0, right_x = 0, right_y = 0;
    std::int16_t l2 = 0, r2 = 0; /* 0..0x7fff */
    /* Raw PS5 buttons this poll, for Porpoise's own shortcuts. */
    bool ps_menu_combo = false; /* Options + touch pad: open the Porpoise menu */
    std::uint32_t buttons = 0;  /* Button bits */
};

bool open();
void close();
void set_layout(Layout layout);
Layout layout();
void set_rumble_enabled(bool enabled);
/* Read the pad. Call once a frame, before the core runs. */
const State &poll();
const State &state();
/* libretro rumble: strength 0..0xffff per motor. */
void set_rumble(bool strong, std::uint16_t strength);
} // namespace porpoise::pad
