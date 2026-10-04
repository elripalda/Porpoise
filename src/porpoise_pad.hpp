/* Porpoise - the DualSense controllers, as libretro joypad and analog state.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>

namespace porpoise::pad
{
/* One controller per signed-in PS5 user: player 1 is the user who started
 * Porpoise, the others join as their users sign in. */
constexpr int kMaxPlayers = 4;

/* Physical buttons, for Porpoise's own screens (independent of the mapping). */
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
    BtnL3 = 1u << 14,
    BtnR3 = 1u << 15,
};

/* The DualSense controls a GameCube input can be given to. */
enum Control : int
{
    CtlCross,
    CtlCircle,
    CtlSquare,
    CtlTriangle,
    CtlL1,
    CtlR1,
    CtlL2,
    CtlR2,
    CtlL3,
    CtlR3,
    CtlOptions,
    CtlTouch,
    CtlUp,
    CtlDown,
    CtlLeft,
    CtlRight,
    CtlCount,
};

/* The GameCube controller's buttons. The control stick and the C-stick are
 * always the left and right sticks. */
enum GcInput : int
{
    GcA,
    GcB,
    GcX,
    GcY,
    GcZ,
    GcL,
    GcR,
    GcStart,
    GcUp,
    GcDown,
    GcLeft,
    GcRight,
    GcCount,
};

/* Which DualSense control each GameCube input is on. */
struct Mapping
{
    std::int8_t control[GcCount];
};

/* The ready-made layouts:
 *   GameCube    Cross A, Square B, Circle X, Triangle Y: A where a PlayStation
 *               player confirms, B beside it as on the GameCube pad. The default.
 *   PlayStation Cross A, Circle B, Square X, Triangle Y: confirm and back
 *               where PlayStation games put them.
 * Both: R1 Z, L2 / R2 the analog L and R, Options Start, the D-pad the D-pad. */
enum Layout : int
{
    LayoutGameCube = 0,
    LayoutPlayStation = 1,
    LayoutOwn = 2, /* 2..5: the player's own layouts 1..4 (Settings::presets) */
};
Mapping preset(int layout);

/* The bit in State::buttons for a control. */
std::uint32_t control_bit(int control);

struct State
{
    bool connected = false;
    std::uint16_t joypad = 0; /* bit n = RETRO_DEVICE_ID_JOYPAD_n, through the mapping */
    std::int16_t left_x = 0, left_y = 0, right_x = 0, right_y = 0;
    std::int16_t l2 = 0, r2 = 0; /* the GameCube L and R analog, 0..0x7fff */
    /* Raw buttons this poll, for Porpoise's own shortcuts and screens. */
    bool ps_menu_combo = false; /* Options + touch pad: open the Porpoise menu */
    std::uint32_t buttons = 0;  /* Button bits */
};

bool open();
void close();
void set_mapping(const Mapping &mapping);
void set_rumble_enabled(bool enabled);
/* Read every controller. Call once a frame; returns player 1. */
const State &poll();
const State &state(int player = 0);
/* A copy taken under the lock, for the core's own thread. */
State snapshot(int player);
bool connected(int player);
int connected_count();
/* libretro rumble for one player: strength 0..0xffff per motor. */
void set_rumble(int player, bool strong, std::uint16_t strength);
} // namespace porpoise::pad
