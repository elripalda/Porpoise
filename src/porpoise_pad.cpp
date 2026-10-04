/* Porpoise - the DualSense controllers, as libretro joypad and analog state.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The console's pad ABI (scePadInit/Open/Read, the 120-byte sample and its
 * button bits) is the one Mihawk's PS5 RetroArch uses, verified on hardware by
 * ProsperoLight (GPL-3.0-or-later). Rumble uses the compatible dual-motor mode
 * (scePadSetVibrationMode 2), the path both of those projects fall back to.
 *
 * Players: a PS5 controller belongs to a signed-in user, so each player is a
 * user. Player 1 is the user who started Porpoise; the signed-in users are
 * looked at again about once a second, and a newly signed-in user's controller
 * becomes the next free player (the way ProsperoEden's pad.cpp does it). A
 * user who signs out frees their player. */
#include "porpoise_pad.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "libretro.h"
#include "trace.hpp"

namespace
{
struct ScePadVibrationParam
{
    std::uint8_t largeMotor;
    std::uint8_t smallMotor;
};
} // namespace

extern "C"
{
    std::int32_t scePadInit();
    std::int32_t scePadOpen(std::int32_t user_id, std::int32_t port_type, std::int32_t index,
                            const void *params);
    std::int32_t scePadGetHandle(std::int32_t user_id, std::int32_t port_type, std::int32_t index);
    std::int32_t scePadRead(std::int32_t handle, void *samples, std::int32_t capacity);
    std::int32_t scePadClose(std::int32_t handle);
    std::int32_t scePadSetVibration(std::int32_t handle, const ScePadVibrationParam *param);
    std::int32_t scePadSetVibrationMode(std::int32_t handle, std::int32_t mode);
    std::int32_t sceUserServiceInitialize(const void *params);
    std::int32_t sceUserServiceGetInitialUser(std::int32_t *user_id);
    /* The signed-in users: four ids, -1 for none (room to spare is kept). */
    std::int32_t sceUserServiceGetLoginUserIdList(std::int32_t *user_ids);
    std::int32_t sceKernelUsleep(std::uint32_t microseconds);
}

namespace
{
using namespace porpoise::pad;

constexpr std::uint32_t pad_l3 = 0x000002u;
constexpr std::uint32_t pad_r3 = 0x000004u;
constexpr std::uint32_t pad_options = 0x000008u;
constexpr std::uint32_t pad_up = 0x000010u;
constexpr std::uint32_t pad_right = 0x000020u;
constexpr std::uint32_t pad_down = 0x000040u;
constexpr std::uint32_t pad_left = 0x000080u;
constexpr std::uint32_t pad_l2 = 0x000100u;
constexpr std::uint32_t pad_r2 = 0x000200u;
constexpr std::uint32_t pad_l1 = 0x000400u;
constexpr std::uint32_t pad_r1 = 0x000800u;
constexpr std::uint32_t pad_triangle = 0x001000u;
constexpr std::uint32_t pad_circle = 0x002000u;
constexpr std::uint32_t pad_cross = 0x004000u;
constexpr std::uint32_t pad_square = 0x008000u;
constexpr std::uint32_t pad_touch_pad = 0x100000u;
constexpr std::uint32_t pad_intercepted = UINT32_C(0x80000000);

struct PadSample
{
    std::uint32_t buttons;
    std::uint8_t left_x;
    std::uint8_t left_y;
    std::uint8_t right_x;
    std::uint8_t right_y;
    std::uint8_t left_trigger;
    std::uint8_t right_trigger;
    std::uint8_t reserved_to_connected[66];
    std::int32_t connected;
    std::uint64_t timestamp_us;
    std::uint8_t extension[16];
    std::uint8_t connected_count;
    std::uint8_t remaining[15];
};
static_assert(sizeof(PadSample) == 120, "the console's pad samples are 120 bytes");
static_assert(offsetof(PadSample, left_x) == 0x04, "the stick bytes follow the button word");
static_assert(offsetof(PadSample, connected) == 0x4c, "connection state sits at 0x4c");
static_assert(offsetof(PadSample, timestamp_us) == 0x50, "the timestamp sits at 0x50");

constexpr int sample_capacity = 64;
constexpr unsigned polls_per_scan = 60; /* about once a second */

/* The console's button bit for each Control, in Control order. */
constexpr std::uint32_t kControlPadBit[CtlCount] = {
    pad_cross, pad_circle, pad_square,  pad_triangle,  pad_l1, pad_r1,   pad_l2,   pad_r2,
    pad_l3,    pad_r3,     pad_options, pad_touch_pad, pad_up, pad_down, pad_left, pad_right,
};
/* Porpoise's Button bit for each Control. */
constexpr std::uint32_t kControlButton[CtlCount] = {
    BtnCross, BtnCircle, BtnSquare,  BtnTriangle, BtnL1, BtnR1,   BtnL2,   BtnR2,
    BtnL3,    BtnR3,     BtnOptions, BtnTouch,    BtnUp, BtnDown, BtnLeft, BtnRight,
};
/* The libretro button Dolphin reads for each GameCube input
 * (Source/Core/DolphinLibretro/Input.cpp: A A, B B, X X, Y Y, Z R, L L2, R R2). */
constexpr unsigned kGcRetro[GcCount] = {
    RETRO_DEVICE_ID_JOYPAD_A,     RETRO_DEVICE_ID_JOYPAD_B,  RETRO_DEVICE_ID_JOYPAD_X,    RETRO_DEVICE_ID_JOYPAD_Y,
    RETRO_DEVICE_ID_JOYPAD_R,     RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2,   RETRO_DEVICE_ID_JOYPAD_START,
    RETRO_DEVICE_ID_JOYPAD_UP,    RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
};

struct Slot
{
    std::int32_t user = -1;
    std::int32_t handle = -1;
    std::uint8_t motor_large = 0, motor_small = 0;
    State state;
};

Slot g_slots[kMaxPlayers];
PadSample g_samples[sample_capacity];
const State g_none{};
Mapping g_mapping = preset(LayoutGameCube);
std::atomic<bool> g_rumble_enabled{true};
unsigned g_polls_since_scan = 0;
bool g_ready = false;

std::int16_t stick(std::uint8_t value)
{
    int v = (static_cast<int>(value) - 128) * 256;
    return static_cast<std::int16_t>(std::clamp(v, -32768, 32767));
}

void push_rumble(Slot &slot)
{
    if (slot.handle < 0)
        return;
    ScePadVibrationParam param{};
    if (g_rumble_enabled.load(std::memory_order_relaxed))
    {
        param.largeMotor = slot.motor_large;
        param.smallMotor = slot.motor_small;
    }
    (void)scePadSetVibration(slot.handle, &param);
}

void open_slot(int player, std::int32_t user, int attempts)
{
    std::int32_t handle = -1;
    /* A title started from the shell can arrive before the pad service has
     * published the device, so the first open is retried, as ProsperoLight does. */
    for (int attempt = 0; attempt < attempts && handle < 0; ++attempt)
    {
        handle = scePadOpen(user, 0, 0, nullptr);
        if (handle < 0)
            handle = scePadGetHandle(user, 0, 0); /* already open in this process */
        if (handle < 0 && attempt + 1 < attempts)
            (void)sceKernelUsleep(100000);
    }
    char line[96];
    if (handle < 0)
    {
        std::snprintf(line, sizeof line, "pad: player %d (user %d) could not open: %#x", player + 1, int(user),
                      unsigned(handle));
        ps5::debug::mark(line);
        return;
    }
    Slot &slot = g_slots[player];
    slot = Slot{};
    slot.user = user;
    slot.handle = handle;
    (void)scePadSetVibrationMode(handle, 2);
    std::snprintf(line, sizeof line, "pad: player %d is user %d, handle %d", player + 1, int(user), int(handle));
    ps5::debug::mark(line);
}

void close_slot(int player)
{
    Slot &slot = g_slots[player];
    if (slot.handle < 0)
        return;
    slot.motor_large = slot.motor_small = 0;
    push_rumble(slot);
    (void)scePadClose(slot.handle);
    char line[64];
    std::snprintf(line, sizeof line, "pad: player %d left", player + 1);
    ps5::debug::mark(line);
    slot = Slot{};
}

/* Users who signed in join as the next free player; users who signed out leave. */
void rescan()
{
    std::int32_t ids[16];
    for (std::int32_t &id : ids)
        id = -1;
    if (sceUserServiceGetLoginUserIdList(ids) != 0)
        return;
    auto signed_in = [&](std::int32_t user) {
        for (int i = 0; i < 4; ++i)
            if (ids[i] == user)
                return true;
        return false;
    };
    for (int player = 1; player < kMaxPlayers; ++player)
        if (g_slots[player].handle >= 0 && !signed_in(g_slots[player].user))
            close_slot(player);
    for (int i = 0; i < 4; ++i)
    {
        const std::int32_t user = ids[i];
        if (user < 0)
            continue;
        bool taken = false;
        for (const Slot &s : g_slots)
            taken |= s.handle >= 0 && s.user == user;
        if (taken)
            continue;
        for (int player = 1; player < kMaxPlayers; ++player)
            if (g_slots[player].handle < 0)
            {
                open_slot(player, user, 1);
                break;
            }
    }
}

State read_slot(Slot &slot)
{
    const std::int32_t count = scePadRead(slot.handle, g_samples, sample_capacity);
    if (count == 0)
        return slot.state; /* nothing new: keep the last state */
    if (count < 0 || count > sample_capacity)
        return State{}; /* refused, or the pad went away: nothing rather than a stuck button */
    const PadSample *newest = nullptr;
    for (std::int32_t i = 0; i < count; ++i)
        if (newest == nullptr || g_samples[i].timestamp_us > newest->timestamp_us)
            newest = &g_samples[i];
    if (newest == nullptr || !newest->connected || (newest->buttons & pad_intercepted) != 0)
        return State{}; /* off, or the system menu has the pad: release everything */

    const std::uint32_t b = newest->buttons;
    State next;
    next.connected = true;
    for (int c = 0; c < CtlCount; ++c)
        if (b & kControlPadBit[c])
            next.buttons |= kControlButton[c];
    for (int gc = 0; gc < GcCount; ++gc)
    {
        const int c = g_mapping.control[gc];
        if (c >= 0 && c < CtlCount && (b & kControlPadBit[c]))
            next.joypad |= static_cast<std::uint16_t>(1u << kGcRetro[gc]);
    }
    next.left_x = stick(newest->left_x);
    next.left_y = stick(newest->left_y);
    next.right_x = stick(newest->right_x);
    next.right_y = stick(newest->right_y);
    /* The GameCube's L and R are analog: on L2 / R2 they follow the trigger,
     * on any other control they are all or nothing. */
    auto analog = [&](int gc) -> std::int16_t {
        const int c = g_mapping.control[gc];
        if (c == CtlL2)
            return static_cast<std::int16_t>(newest->left_trigger * 0x7fff / 255);
        if (c == CtlR2)
            return static_cast<std::int16_t>(newest->right_trigger * 0x7fff / 255);
        return (c >= 0 && c < CtlCount && (b & kControlPadBit[c])) ? 0x7fff : 0;
    };
    next.l2 = analog(GcL);
    next.r2 = analog(GcR);
    next.ps_menu_combo = (b & (pad_options | pad_touch_pad)) == (pad_options | pad_touch_pad);
    return next;
}
} // namespace

namespace porpoise::pad
{
Mapping preset(int layout)
{
    Mapping m{};
    m.control[GcA] = CtlCross;
    if (layout == LayoutPlayStation)
    {
        m.control[GcB] = CtlCircle;
        m.control[GcX] = CtlSquare;
        m.control[GcY] = CtlTriangle;
    }
    else
    {
        m.control[GcB] = CtlSquare;
        m.control[GcX] = CtlCircle;
        m.control[GcY] = CtlTriangle;
    }
    m.control[GcZ] = CtlR1;
    m.control[GcL] = CtlL2;
    m.control[GcR] = CtlR2;
    m.control[GcStart] = CtlOptions;
    m.control[GcUp] = CtlUp;
    m.control[GcDown] = CtlDown;
    m.control[GcLeft] = CtlLeft;
    m.control[GcRight] = CtlRight;
    return m;
}

std::uint32_t control_bit(int control)
{
    return control >= 0 && control < CtlCount ? kControlButton[control] : 0;
}

bool open()
{
    (void)sceUserServiceInitialize(nullptr);
    std::int32_t user_id = -1;
    if (sceUserServiceGetInitialUser(&user_id) < 0)
    {
        ps5::debug::mark("pad: no initial user; no pad this run");
        return false;
    }
    if (scePadInit() < 0)
    {
        ps5::debug::mark("pad: scePadInit failed");
        return false;
    }
    g_ready = true;
    open_slot(0, user_id, 10);
    rescan();
    return g_slots[0].handle >= 0;
}

void close()
{
    for (int player = 0; player < kMaxPlayers; ++player)
        close_slot(player);
    g_ready = false;
}

void set_mapping(const Mapping &mapping)
{
    g_mapping = mapping;
}

void set_rumble_enabled(bool enabled)
{
    g_rumble_enabled.store(enabled, std::memory_order_relaxed);
    for (Slot &slot : g_slots)
        push_rumble(slot);
}

const State &poll()
{
    if (g_ready && ++g_polls_since_scan >= polls_per_scan)
    {
        g_polls_since_scan = 0;
        rescan();
    }
    for (Slot &slot : g_slots)
        slot.state = slot.handle >= 0 ? read_slot(slot) : State{};
    return g_slots[0].state;
}

const State &state(int player)
{
    return player >= 0 && player < kMaxPlayers ? g_slots[player].state : g_none;
}

bool connected(int player)
{
    return player >= 0 && player < kMaxPlayers && g_slots[player].handle >= 0 && g_slots[player].state.connected;
}

int connected_count()
{
    int n = 0;
    for (int player = 0; player < kMaxPlayers; ++player)
        n += connected(player) ? 1 : 0;
    return n;
}

void set_rumble(int player, bool strong, std::uint16_t strength)
{
    if (player < 0 || player >= kMaxPlayers)
        return;
    Slot &slot = g_slots[player];
    const std::uint8_t level = static_cast<std::uint8_t>(strength >> 8);
    std::uint8_t &motor = strong ? slot.motor_large : slot.motor_small;
    if (motor == level)
        return;
    motor = level;
    push_rumble(slot);
}
} // namespace porpoise::pad
