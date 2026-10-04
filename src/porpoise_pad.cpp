/* Porpoise - the DualSense, as libretro joypad and analog state.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The console's pad ABI (scePadInit/Open/Read, the 120-byte sample and its
 * button bits) is the one Mihawk's PS5 RetroArch uses, verified on hardware by
 * ProsperoLight (GPL-3.0-or-later). Rumble uses the compatible dual-motor mode
 * (scePadSetVibrationMode 2), the path both of those projects fall back to. */
#include "porpoise_pad.hpp"

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
    std::int32_t scePadRead(std::int32_t handle, void *samples, std::int32_t capacity);
    std::int32_t scePadClose(std::int32_t handle);
    std::int32_t scePadSetVibration(std::int32_t handle, const ScePadVibrationParam *param);
    std::int32_t scePadSetVibrationMode(std::int32_t handle, std::int32_t mode);
    std::int32_t sceUserServiceInitialize(const void *params);
    std::int32_t sceUserServiceGetInitialUser(std::int32_t *user_id);
    std::int32_t sceKernelUsleep(std::uint32_t microseconds);
}

namespace
{
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

std::int32_t g_handle = -1;
PadSample g_samples[sample_capacity];
porpoise::pad::State g_state;
porpoise::pad::Layout g_layout = porpoise::pad::Layout::GameCube;
std::atomic<bool> g_rumble_enabled{true};
std::uint8_t g_motor_large = 0, g_motor_small = 0;

inline std::uint16_t bit(unsigned id)
{
    return static_cast<std::uint16_t>(1u << id);
}

std::int16_t stick(std::uint8_t value)
{
    int v = (static_cast<int>(value) - 128) * 256;
    if (v > 32767)
        v = 32767;
    if (v < -32768)
        v = -32768;
    return static_cast<std::int16_t>(v);
}

std::uint16_t to_joypad(std::uint32_t pad, porpoise::pad::Layout layout)
{
    std::uint16_t out = 0;
    if (layout == porpoise::pad::Layout::GameCube)
    {
        if (pad & pad_cross) out |= bit(RETRO_DEVICE_ID_JOYPAD_A);
        if (pad & pad_square) out |= bit(RETRO_DEVICE_ID_JOYPAD_B);
        if (pad & pad_circle) out |= bit(RETRO_DEVICE_ID_JOYPAD_X);
        if (pad & pad_triangle) out |= bit(RETRO_DEVICE_ID_JOYPAD_Y);
    }
    else
    {
        if (pad & pad_circle) out |= bit(RETRO_DEVICE_ID_JOYPAD_A);
        if (pad & pad_cross) out |= bit(RETRO_DEVICE_ID_JOYPAD_B);
        if (pad & pad_triangle) out |= bit(RETRO_DEVICE_ID_JOYPAD_X);
        if (pad & pad_square) out |= bit(RETRO_DEVICE_ID_JOYPAD_Y);
    }
    if (pad & pad_up) out |= bit(RETRO_DEVICE_ID_JOYPAD_UP);
    if (pad & pad_down) out |= bit(RETRO_DEVICE_ID_JOYPAD_DOWN);
    if (pad & pad_left) out |= bit(RETRO_DEVICE_ID_JOYPAD_LEFT);
    if (pad & pad_right) out |= bit(RETRO_DEVICE_ID_JOYPAD_RIGHT);
    if (pad & pad_options) out |= bit(RETRO_DEVICE_ID_JOYPAD_START);
    if (pad & pad_touch_pad) out |= bit(RETRO_DEVICE_ID_JOYPAD_SELECT);
    if (pad & pad_l1) out |= bit(RETRO_DEVICE_ID_JOYPAD_L);
    if (pad & pad_r1) out |= bit(RETRO_DEVICE_ID_JOYPAD_R);   /* GameCube Z */
    if (pad & pad_l2) out |= bit(RETRO_DEVICE_ID_JOYPAD_L2);
    if (pad & pad_r2) out |= bit(RETRO_DEVICE_ID_JOYPAD_R2);
    if (pad & pad_l3) out |= bit(RETRO_DEVICE_ID_JOYPAD_L3);
    if (pad & pad_r3) out |= bit(RETRO_DEVICE_ID_JOYPAD_R3);
    return out;
}

void push_rumble()
{
    if (g_handle < 0)
        return;
    ScePadVibrationParam param{};
    if (g_rumble_enabled.load(std::memory_order_relaxed))
    {
        param.largeMotor = g_motor_large;
        param.smallMotor = g_motor_small;
    }
    (void)scePadSetVibration(g_handle, &param);
}
} // namespace

namespace porpoise::pad
{
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
    /* A title started from the shell can arrive before the pad service has
     * published the device, so the open is retried, as ProsperoLight does. */
    for (int attempt = 0; attempt < 10 && g_handle < 0; ++attempt)
    {
        g_handle = scePadOpen(user_id, 0, 0, nullptr);
        if (g_handle < 0)
            (void)sceKernelUsleep(100000);
    }
    if (g_handle < 0)
    {
        ps5::debug::mark_value("pad: scePadOpen failed", g_handle);
        return false;
    }
    ps5::debug::mark_value("pad: vibration mode 2 (compatible)", scePadSetVibrationMode(g_handle, 2));
    ps5::debug::mark_value("pad: opened, handle", g_handle);
    return true;
}

void close()
{
    if (g_handle >= 0)
    {
        g_motor_large = g_motor_small = 0;
        push_rumble();
        (void)scePadClose(g_handle);
        g_handle = -1;
    }
}

void set_layout(Layout layout)
{
    g_layout = layout;
}

Layout layout()
{
    return g_layout;
}

void set_rumble_enabled(bool enabled)
{
    g_rumble_enabled.store(enabled, std::memory_order_relaxed);
    push_rumble();
}

const State &poll()
{
    if (g_handle < 0)
        return g_state;
    const std::int32_t count = scePadRead(g_handle, g_samples, sample_capacity);
    if (count == 0)
        return g_state; /* nothing new: keep the last state */
    if (count < 0 || count > sample_capacity)
    {
        /* Refused, or the pad went away: report nothing rather than a stuck button. */
        g_state = State{};
        return g_state;
    }
    const PadSample *newest = nullptr;
    for (std::int32_t i = 0; i < count; ++i)
        if (newest == nullptr || g_samples[i].timestamp_us > newest->timestamp_us)
            newest = &g_samples[i];
    if (newest == nullptr || !newest->connected || (newest->buttons & pad_intercepted) != 0)
    {
        /* Disconnected, or the system menu has the pad: release everything. */
        g_state = State{};
        return g_state;
    }
    State next;
    next.connected = true;
    next.joypad = to_joypad(newest->buttons, g_layout);
    next.left_x = stick(newest->left_x);
    next.left_y = stick(newest->left_y);
    next.right_x = stick(newest->right_x);
    next.right_y = stick(newest->right_y);
    next.l2 = static_cast<std::int16_t>(newest->left_trigger * 0x7fff / 255);
    next.r2 = static_cast<std::int16_t>(newest->right_trigger * 0x7fff / 255);
    {
        const std::uint32_t b = newest->buttons;
        std::uint32_t u = 0;
        if (b & pad_up) u |= BtnUp;
        if (b & pad_down) u |= BtnDown;
        if (b & pad_left) u |= BtnLeft;
        if (b & pad_right) u |= BtnRight;
        if (b & pad_cross) u |= BtnCross;
        if (b & pad_circle) u |= BtnCircle;
        if (b & pad_square) u |= BtnSquare;
        if (b & pad_triangle) u |= BtnTriangle;
        if (b & pad_l1) u |= BtnL1;
        if (b & pad_r1) u |= BtnR1;
        if (b & pad_l2) u |= BtnL2;
        if (b & pad_r2) u |= BtnR2;
        if (b & pad_options) u |= BtnOptions;
        if (b & pad_touch_pad) u |= BtnTouch;
        next.buttons = u;
    }
    next.ps_menu_combo = (newest->buttons & (pad_options | pad_touch_pad)) ==
                         (pad_options | pad_touch_pad);
    g_state = next;
    return g_state;
}

const State &state()
{
    return g_state;
}

void set_rumble(bool strong, std::uint16_t strength)
{
    const std::uint8_t level = static_cast<std::uint8_t>(strength >> 8);
    std::uint8_t &motor = strong ? g_motor_large : g_motor_small;
    if (motor == level)
        return;
    motor = level;
    push_rumble();
}
} // namespace porpoise::pad
