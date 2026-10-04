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
#include <cmath>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "libretro.h"
#include "trace.hpp"

namespace
{
struct ScePadVibrationParam
{
    std::uint8_t largeMotor;
    std::uint8_t smallMotor;
};
struct ScePadLightBarParam
{
    std::uint8_t r, g, b;
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
    std::int32_t scePadSetLightBar(std::int32_t handle, const ScePadLightBarParam *param);
    std::int32_t scePadResetLightBar(std::int32_t handle);
    std::int32_t scePadSetMotionSensorState(std::int32_t handle, bool enable);
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
    std::uint8_t padding[2];
    float orientation[4];      /* a quaternion: x, y, z, w */
    float angular_velocity[3]; /* rad/s: x right, y up out of the face, z toward the player */
    float acceleration[3];     /* in g, the same axes */
    std::uint8_t touch_count;
    std::uint8_t touch_reserved[7];
    struct
    {
        std::uint16_t x, y;
        std::uint8_t id;
        std::uint8_t reserved[3];
    } touch[2];
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
static_assert(offsetof(PadSample, orientation) == 0x0c, "the motion data follows the triggers");
static_assert(offsetof(PadSample, acceleration) == 0x28, "acceleration sits at 0x28");
static_assert(offsetof(PadSample, touch) == 0x3c, "the touches sit at 0x3c");

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
    /* The pointer and its bookkeeping. */
    float pointer_x = 0, pointer_y = 0;
    std::uint64_t last_motion_us = 0;
    int shake_frames = 0;
    bool r1_was = false;
    /* Which way gravity reads, learnt from the first readings at rest. */
    float gravity_sum = 0;
    int gravity_count = 0;
    float gravity_sign = 1.0f;
};

/* ---- the Wii Remote ---- */
WiiConfig g_wii;

struct WiiBind
{
    std::uint32_t pad;
    unsigned retro;
};
/* Each Wii controller's buttons: DualSense button -> the libretro button the
 * Dolphin core reads for it (Source/Core/DolphinLibretro/Input.cpp,
 * retro_set_controller_port_device_wii). */
constexpr WiiBind kWiiNunchuk[] = {
    /* A, B (the trigger under the Remote), C, Z, -, +, 1, 2, Home, shakes. */
    {pad_cross, RETRO_DEVICE_ID_JOYPAD_A},      {pad_r2, RETRO_DEVICE_ID_JOYPAD_B},
    {pad_l1, RETRO_DEVICE_ID_JOYPAD_X},         {pad_l2, RETRO_DEVICE_ID_JOYPAD_Y},
    {pad_touch_pad, RETRO_DEVICE_ID_JOYPAD_L},  {pad_options, RETRO_DEVICE_ID_JOYPAD_R},
    {pad_square, RETRO_DEVICE_ID_JOYPAD_START}, {pad_triangle, RETRO_DEVICE_ID_JOYPAD_SELECT},
    {pad_r3, RETRO_DEVICE_ID_JOYPAD_R3},        {pad_circle, RETRO_DEVICE_ID_JOYPAD_R2},
    {pad_l3, RETRO_DEVICE_ID_JOYPAD_L2},
};
constexpr WiiBind kWiiRemote[] = {
    /* A, B, 1, 2, -, +, Home, shake. */
    {pad_cross, RETRO_DEVICE_ID_JOYPAD_A},       {pad_r2, RETRO_DEVICE_ID_JOYPAD_B},
    {pad_square, RETRO_DEVICE_ID_JOYPAD_X},      {pad_triangle, RETRO_DEVICE_ID_JOYPAD_Y},
    {pad_touch_pad, RETRO_DEVICE_ID_JOYPAD_SELECT}, {pad_options, RETRO_DEVICE_ID_JOYPAD_START},
    {pad_r3, RETRO_DEVICE_ID_JOYPAD_R3},         {pad_circle, RETRO_DEVICE_ID_JOYPAD_R2},
};
constexpr WiiBind kWiiSideways[] = {
    /* Held like an NES pad: 1 on Square, 2 on Cross; A, B; -, +, Home; shake on R1 / R2. */
    {pad_square, RETRO_DEVICE_ID_JOYPAD_B},      {pad_cross, RETRO_DEVICE_ID_JOYPAD_A},
    {pad_triangle, RETRO_DEVICE_ID_JOYPAD_X},    {pad_circle, RETRO_DEVICE_ID_JOYPAD_Y},
    {pad_touch_pad, RETRO_DEVICE_ID_JOYPAD_SELECT}, {pad_options, RETRO_DEVICE_ID_JOYPAD_START},
    {pad_r3, RETRO_DEVICE_ID_JOYPAD_R3},         {pad_r1, RETRO_DEVICE_ID_JOYPAD_R2},
    {pad_r2, RETRO_DEVICE_ID_JOYPAD_R2},
};
constexpr WiiBind kWiiClassic[] = {
    /* By position: a Circle, b Cross, x Triangle, y Square; ZL / ZR on L1 / R1,
     * L / R on the triggers (analog). */
    {pad_circle, RETRO_DEVICE_ID_JOYPAD_A},  {pad_cross, RETRO_DEVICE_ID_JOYPAD_B},
    {pad_triangle, RETRO_DEVICE_ID_JOYPAD_X}, {pad_square, RETRO_DEVICE_ID_JOYPAD_Y},
    {pad_l1, RETRO_DEVICE_ID_JOYPAD_L},      {pad_r1, RETRO_DEVICE_ID_JOYPAD_R},
    {pad_options, RETRO_DEVICE_ID_JOYPAD_START}, {pad_touch_pad, RETRO_DEVICE_ID_JOYPAD_SELECT},
    {pad_r3, RETRO_DEVICE_ID_JOYPAD_R3},
};
/* Two controllers: the first is the Remote (the Nunchuk device's Remote buttons)... */
constexpr WiiBind kWiiTwoRemote[] = {
    {pad_cross, RETRO_DEVICE_ID_JOYPAD_A},       {pad_r2, RETRO_DEVICE_ID_JOYPAD_B},
    {pad_square, RETRO_DEVICE_ID_JOYPAD_START},  {pad_triangle, RETRO_DEVICE_ID_JOYPAD_SELECT},
    {pad_touch_pad, RETRO_DEVICE_ID_JOYPAD_L},   {pad_options, RETRO_DEVICE_ID_JOYPAD_R},
    {pad_r3, RETRO_DEVICE_ID_JOYPAD_R3},         {pad_circle, RETRO_DEVICE_ID_JOYPAD_R2},
};
/* ...and the second the Nunchuk: Z on its triggers, C on its shoulders or Cross,
 * a shake on Circle (or a flick). */
constexpr WiiBind kWiiTwoNunchuk[] = {
    {pad_l2, RETRO_DEVICE_ID_JOYPAD_Y},    {pad_r2, RETRO_DEVICE_ID_JOYPAD_Y},
    {pad_l1, RETRO_DEVICE_ID_JOYPAD_X},    {pad_r1, RETRO_DEVICE_ID_JOYPAD_X},
    {pad_cross, RETRO_DEVICE_ID_JOYPAD_X}, {pad_circle, RETRO_DEVICE_ID_JOYPAD_L2},
};

template <std::size_t N> std::uint16_t wii_bits(const WiiBind (&binds)[N], std::uint32_t b)
{
    std::uint16_t out = 0;
    for (const WiiBind &w : binds)
        if (b & w.pad)
            out |= static_cast<std::uint16_t>(1u << w.retro);
    return out;
}

/* The DualSense's axes (x right, y out of its face, z toward the player)
 * into the Remote's (x left, y back, z up) for how it is held. */
void to_remote(int grip, const float in[3], float out[3])
{
    switch (grip)
    {
    case GripLeftEdge: /* its left edge points at the TV */
        out[0] = in[2];
        out[1] = in[0];
        out[2] = in[1];
        break;
    case GripRightEdge:
        out[0] = -in[2];
        out[1] = -in[0];
        out[2] = in[1];
        break;
    default: /* its back edge (the light bar) points at the TV */
        out[0] = -in[0];
        out[1] = in[2];
        out[2] = in[1];
        break;
    }
}

Slot g_slots[kMaxPlayers];
PadSample g_samples[sample_capacity];
const State g_none{};
Mapping g_mapping = preset(LayoutPlayStation);
std::atomic<bool> g_rumble_enabled{true};
unsigned g_polls_since_scan = 0;
bool g_ready = false;
std::int32_t g_initial_user = -1; /* player 1, always */
/* Dolphin reads input and sets rumble from its own CPU thread while the main
 * thread polls and players come and go: slots change under this lock. */
std::recursive_mutex g_lock;

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
    /* The gyroscope and accelerometer, for the Wii Remote. */
    const std::int32_t motion = scePadSetMotionSensorState(handle, true);
    /* Each player's light bar has their colour, so everyone knows which
     * controller is theirs: blue, red, green, pink. */
    static const ScePadLightBarParam kColours[kMaxPlayers] = {{0, 96, 255}, {255, 36, 48}, {0, 210, 80}, {255, 60, 190}};
    (void)scePadSetLightBar(handle, &kColours[player]);
    std::snprintf(line, sizeof line, "pad: player %d is user %d, handle %d, motion %#x", player + 1, int(user),
                  int(handle), unsigned(motion));
    ps5::debug::mark(line);
}

void close_slot(int player)
{
    Slot &slot = g_slots[player];
    if (slot.handle < 0)
        return;
    slot.motor_large = slot.motor_small = 0;
    push_rumble(slot);
    (void)scePadResetLightBar(slot.handle);
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
    /* Player 1 is the user who started Porpoise, even if the pad service was
     * late for them at start. */
    if (g_slots[0].handle < 0 && g_initial_user >= 0)
        open_slot(0, g_initial_user, 1);
    for (int i = 0; i < 4; ++i)
    {
        const std::int32_t user = ids[i];
        if (user < 0 || user == g_initial_user)
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

/* The motion in this poll's readings: the newest as it is, the gyro over all
 * of them for the pointer. */
Motion read_motion(Slot &slot, std::int32_t count)
{
    Motion m;
    const PadSample *order[sample_capacity];
    int n = 0;
    for (std::int32_t i = 0; i < count; ++i)
        if (g_samples[i].connected)
            order[n++] = &g_samples[i];
    std::sort(order, order + n, [](const PadSample *a, const PadSample *b) { return a->timestamp_us < b->timestamp_us; });
    if (n == 0)
        return m;
    const PadSample &last = *order[n - 1];
    m.valid = true;
    m.samples = unsigned(n);
    for (int i = 0; i < 3; ++i)
    {
        m.raw_accel[i] = last.acceleration[i];
        m.raw_gyro[i] = last.angular_velocity[i];
    }
    for (int i = 0; i < 4; ++i)
        m.orientation[i] = last.orientation[i];
    /* Gravity's direction: the first readings with the controller still. */
    const float mag = std::sqrt(m.raw_accel[0] * m.raw_accel[0] + m.raw_accel[1] * m.raw_accel[1] +
                                m.raw_accel[2] * m.raw_accel[2]);
    if (slot.gravity_count < 60 && std::fabs(mag - 1.0f) < 0.12f && std::fabs(m.raw_accel[1]) > 0.7f)
    {
        slot.gravity_sum += m.raw_accel[1];
        if (++slot.gravity_count == 60)
            slot.gravity_sign = slot.gravity_sum >= 0 ? 1.0f : -1.0f;
    }
    float a[3];
    for (int i = 0; i < 3; ++i)
        a[i] = m.raw_accel[i] * slot.gravity_sign;
    const int grip = g_wii.grip;
    to_remote(grip, a, m.accel);
    to_remote(grip, m.raw_gyro, m.gyro);

    /* The pointer. */
    const float k = 2.86f * float(std::clamp(g_wii.speed, 1, 10)) / 5.0f; /* screen widths per radian */
    const float sx = g_wii.invert_x ? -1.0f : 1.0f, sy = g_wii.invert_y ? -1.0f : 1.0f;
    for (int i = 0; i < n; ++i)
    {
        const PadSample &p = *order[i];
        if (slot.last_motion_us && p.timestamp_us > slot.last_motion_us)
        {
            const float dt = std::min(0.05f, float(p.timestamp_us - slot.last_motion_us) * 1e-6f);
            if (g_wii.pointer == PointerGyro)
            {
                float g[3];
                to_remote(grip, p.angular_velocity, g);
                /* Turning left (+z) moves it left; the nose down (+x) moves it down. */
                slot.pointer_x -= sx * g[2] * k * dt;
                slot.pointer_y += sy * g[0] * k * dt;
            }
        }
        if (p.timestamp_us > slot.last_motion_us)
            slot.last_motion_us = p.timestamp_us;
    }
    m.touching = (last.touch_count & 0x7f) > 0;
    if (m.touching)
    {
        m.touch_x = last.touch[0].x;
        m.touch_y = last.touch[0].y;
        if (g_wii.pointer == PointerTouch)
        {
            slot.pointer_x = float(m.touch_x) / 1919.0f * 2.0f - 1.0f;
            slot.pointer_y = float(m.touch_y) / 1079.0f * 2.0f - 1.0f;
        }
    }
    /* R1 puts the pointer back in the middle. */
    const bool r1 = (last.buttons & pad_r1) != 0;
    if (r1 && !slot.r1_was && g_wii.pointer == PointerGyro)
        slot.pointer_x = slot.pointer_y = 0;
    slot.r1_was = r1;
    slot.pointer_x = std::clamp(slot.pointer_x, -1.0f, 1.0f);
    slot.pointer_y = std::clamp(slot.pointer_y, -1.0f, 1.0f);
    m.pointer_x = slot.pointer_x;
    m.pointer_y = slot.pointer_y;
    /* A flick: well over 1 g. */
    if (g_wii.shake && mag > 2.0f)
        slot.shake_frames = 8;
    m.shaking = slot.shake_frames > 0;
    if (slot.shake_frames > 0)
        --slot.shake_frames;
    return m;
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
    const Motion motion = read_motion(slot, count);

    const std::uint32_t b = newest->buttons;
    State next;
    next.connected = true;
    for (int c = 0; c < CtlCount; ++c)
        if (b & kControlPadBit[c])
            next.buttons |= kControlButton[c];
    bool mapped[CtlCount] = {};
    for (int gc = 0; gc < GcCount; ++gc)
    {
        const int c = g_mapping.control[gc];
        if (c < 0 || c >= CtlCount)
            continue;
        mapped[c] = true;
        if (b & kControlPadBit[c])
            next.joypad |= static_cast<std::uint16_t>(1u << kGcRetro[gc]);
    }
    /* Controls no GameCube button uses keep the libretro meaning Dolphin gives
     * them: the Wii's minus (Select) and Home (R3), the GameCube's soft L / R
     * (L3 / R3) and the Triforce's test and coin (L, Select). */
    struct Pass
    {
        int control;
        unsigned retro;
    };
    constexpr Pass kPass[] = {{CtlL1, RETRO_DEVICE_ID_JOYPAD_L},
                              {CtlTouch, RETRO_DEVICE_ID_JOYPAD_SELECT},
                              {CtlL3, RETRO_DEVICE_ID_JOYPAD_L3},
                              {CtlR3, RETRO_DEVICE_ID_JOYPAD_R3}};
    for (const Pass &pass : kPass)
        if (!mapped[pass.control] && (b & kControlPadBit[pass.control]))
            next.joypad |= static_cast<std::uint16_t>(1u << pass.retro);
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
    next.motion = motion;
    if (g_wii.active)
    {
        /* A Wii game: the Wii controller's buttons instead of the GameCube's. */
        const bool shake = motion.shaking;
        switch (g_wii.controller)
        {
        case WiiRemote: next.joypad = wii_bits(kWiiRemote, b); break;
        case WiiSideways: next.joypad = wii_bits(kWiiSideways, b); break;
        case WiiClassic:
            next.joypad = wii_bits(kWiiClassic, b);
            next.l2 = static_cast<std::int16_t>(newest->left_trigger * 0x7fff / 255);
            next.r2 = static_cast<std::int16_t>(newest->right_trigger * 0x7fff / 255);
            break;
        case WiiTwoControllers:
            next.joypad = wii_bits(kWiiTwoRemote, b) | (shake ? (1u << RETRO_DEVICE_ID_JOYPAD_R2) : 0u);
            next.nunchuk = wii_bits(kWiiTwoNunchuk, b) | (shake ? (1u << RETRO_DEVICE_ID_JOYPAD_L2) : 0u);
            break;
        default:
            next.joypad = wii_bits(kWiiNunchuk, b) | (shake ? (1u << RETRO_DEVICE_ID_JOYPAD_R2) : 0u);
            break;
        }
        /* The D-pad is the D-pad on every Wii controller. */
        constexpr WiiBind kDpad[] = {{pad_up, RETRO_DEVICE_ID_JOYPAD_UP},
                                     {pad_down, RETRO_DEVICE_ID_JOYPAD_DOWN},
                                     {pad_left, RETRO_DEVICE_ID_JOYPAD_LEFT},
                                     {pad_right, RETRO_DEVICE_ID_JOYPAD_RIGHT}};
        next.joypad |= wii_bits(kDpad, b);
    }
    return next;
}
} // namespace

namespace porpoise::pad
{
Mapping preset(int layout)
{
    Mapping m{};
    if (layout == LayoutPlayStation)
    {
        /* Cross confirms, as on PlayStation. */
        m.control[GcA] = CtlCross;
        m.control[GcB] = CtlCircle;
        m.control[GcX] = CtlSquare;
        m.control[GcY] = CtlTriangle;
    }
    else
    {
        /* Dolphin's own: A on Circle, B on Cross. */
        m.control[GcA] = CtlCircle;
        m.control[GcB] = CtlCross;
        m.control[GcX] = CtlSquare;
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
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_ready = true;
    g_initial_user = user_id;
    open_slot(0, user_id, 10);
    rescan();
    return g_slots[0].handle >= 0;
}

void close()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    for (int player = 0; player < kMaxPlayers; ++player)
        close_slot(player);
    g_ready = false;
}

void set_mapping(const Mapping &mapping)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_mapping = mapping;
}

void set_rumble_enabled(bool enabled)
{
    g_rumble_enabled.store(enabled, std::memory_order_relaxed);
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    for (Slot &slot : g_slots)
        push_rumble(slot);
}

const State &poll()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
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

State snapshot(int player)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    return player >= 0 && player < kMaxPlayers ? g_slots[player].state : State{};
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

void set_wii(const WiiConfig &config)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    const bool was = g_wii.active;
    g_wii = config;
    if (config.active && !was)
        for (Slot &slot : g_slots)
            slot.pointer_x = slot.pointer_y = 0;
}

WiiConfig wii()
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    return g_wii;
}

unsigned wii_device(int controller)
{
    /* The Dolphin core's devices (Input.cpp): a plain joypad is the Remote. */
    switch (controller)
    {
    case WiiRemote: return RETRO_DEVICE_JOYPAD;
    case WiiSideways: return (2 << 8) | RETRO_DEVICE_JOYPAD;
    case WiiClassic: return (4 << 8) | RETRO_DEVICE_JOYPAD;
    default: return (3 << 8) | RETRO_DEVICE_JOYPAD; /* with the Nunchuk */
    }
}

void recenter(int player)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (player >= 0 && player < kMaxPlayers)
        g_slots[player].pointer_x = g_slots[player].pointer_y = 0;
}

void set_rumble(int player, bool strong, std::uint16_t strength)
{
    if (player < 0 || player >= kMaxPlayers)
        return;
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    Slot &slot = g_slots[player];
    const std::uint8_t level = static_cast<std::uint8_t>(strength >> 8);
    std::uint8_t &motor = strong ? slot.motor_large : slot.motor_small;
    if (motor == level)
        return;
    motor = level;
    push_rumble(slot);
}
} // namespace porpoise::pad
