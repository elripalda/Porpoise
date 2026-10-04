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
#include "porpoise_aim.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "libretro.h"
#include "trace.hpp"

namespace aim = porpoise::aim;

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
    float acceleration[3];     /* in g, gravity included: x right, y up out of the face, z toward the player */
    float angular_velocity[3]; /* rad/s, the same axes (read on a PS5: 1.03 g up when flat, ~0 rad/s when still) */
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
static_assert(offsetof(PadSample, acceleration) == 0x1c, "acceleration follows the orientation");
static_assert(offsetof(PadSample, angular_velocity) == 0x28, "angular velocity sits at 0x28");
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
    bool centre_was = false;
    /* The controller's orientation, and where the middle of the screen is. */
    aim::Fusion fusion;
    aim::Angles centre;
    bool centred = false;
    /* How it is held (WiiPose; -1 not yet read), and how long a different
     * hold has been seen, steadily, before switching to it. */
    int pose = -1;
    int pose_seen = -1;
    float pose_wait = 0;
    /* The grip as set at the last centring, made level for that hold. */
    aim::Basis basis = aim::kGripNormal;
    /* The accelerometer smoothed (about a sixth of a second): gravity's up. */
    float up[3] = {0, 0, 0};
    unsigned centre_seq = 0; /* the Remote's centrings seen (two-controller play's Nunchuk) */
    float centre_hold = 0;   /* how long R1 (L1) has been held, calmly */
    unsigned centrings = 0;  /* for the "centred" note on screen */
};

/* ---- the Wii Remote ---- */
WiiConfig g_wii;
bool g_nunchuk_motion = false; /* the core has the Nunchuk device's motion (set_nunchuk_motion) */
unsigned g_centre_seq = 0;     /* bumped when player 1 centres: two-controller play's Nunchuk follows */

/* Each Wii controller's layout: DualSense control -> Wii input. The same
 * tables drive the game and the Controls tab's drawing. */
constexpr WiiBinding kDPadAsDPad[] = {{CtlUp, WiUp}, {CtlDown, WiDown}, {CtlLeft, WiLeft}, {CtlRight, WiRight}};
constexpr WiiBinding kLayRemote[] = {
    /* Held either way on the right: the index finger on R2 (B), the thumb on
     * Cross (A), 1 and 2 beside it. */
    {CtlCross, WiA},    {CtlR2, WiB},       {CtlSquare, WiOne}, {CtlTriangle, WiTwo},
    {CtlCircle, WiShake}, {CtlTouch, WiMinus}, {CtlOptions, WiPlus}, {CtlR3, WiHome},
    {CtlR1, WiCentre},
};
constexpr WiiBinding kLayRemoteLeft[] = {
    /* The mirror image for the left hand: L2 B, the D-pad as the face buttons. */
    {CtlDown, WiA},     {CtlL2, WiB},       {CtlRight, WiOne},  {CtlUp, WiTwo},
    {CtlLeft, WiShake}, {CtlTouch, WiMinus}, {CtlOptions, WiPlus}, {CtlL3, WiHome},
    {CtlL1, WiCentre},
};
constexpr WiiBinding kLaySideways[] = {
    /* Held like an NES pad: 1 on Square, 2 on Cross; A, B above them. */
    {CtlSquare, WiOne}, {CtlCross, WiTwo},  {CtlTriangle, WiA}, {CtlCircle, WiB},
    {CtlTouch, WiMinus}, {CtlOptions, WiPlus}, {CtlR3, WiHome}, {CtlR1, WiShake},
    {CtlR2, WiShake},
};
constexpr WiiBinding kLayNunchuk[] = {
    /* The Remote on the right (A, B on R2), the Nunchuk on the left (C on L1, Z on L2). */
    {CtlCross, WiA},     {CtlR2, WiB},        {CtlL1, WiC},        {CtlL2, WiZ},
    {CtlTouch, WiMinus}, {CtlOptions, WiPlus}, {CtlSquare, WiOne}, {CtlTriangle, WiTwo},
    {CtlR3, WiHome},     {CtlCircle, WiShake}, {CtlL3, WiNunchukShake}, {CtlR1, WiCentre},
};
constexpr WiiBinding kLayClassic[] = {
    /* By position: a Circle, b Cross, x Triangle, y Square. */
    {CtlCircle, WiClA}, {CtlCross, WiClB},  {CtlTriangle, WiClX}, {CtlSquare, WiClY},
    {CtlL1, WiClZL},    {CtlR1, WiClZR},    {CtlL2, WiClL},       {CtlR2, WiClR},
    {CtlOptions, WiPlus}, {CtlTouch, WiMinus}, {CtlR3, WiHome},
};
constexpr WiiBinding kLayTwoNunchuk[] = {
    /* The second controller is the Nunchuk: Z on its triggers, C on its shoulders or Cross. */
    {CtlL2, WiZ}, {CtlR2, WiZ}, {CtlL1, WiC}, {CtlR1, WiC}, {CtlCross, WiC}, {CtlCircle, WiNunchukShake},
};

/* The libretro button the Dolphin core reads for each Wii input, per device
 * (Source/Core/DolphinLibretro/Input.cpp, retro_set_controller_port_device_wii);
 * -1: none. */
constexpr int R(unsigned id) { return int(id); }
constexpr int kRetroRemote[WiInputCount] = {
    R(RETRO_DEVICE_ID_JOYPAD_A), R(RETRO_DEVICE_ID_JOYPAD_B), R(RETRO_DEVICE_ID_JOYPAD_X), R(RETRO_DEVICE_ID_JOYPAD_Y),
    R(RETRO_DEVICE_ID_JOYPAD_SELECT), R(RETRO_DEVICE_ID_JOYPAD_START), R(RETRO_DEVICE_ID_JOYPAD_R3),
    R(RETRO_DEVICE_ID_JOYPAD_UP), R(RETRO_DEVICE_ID_JOYPAD_DOWN), R(RETRO_DEVICE_ID_JOYPAD_LEFT), R(RETRO_DEVICE_ID_JOYPAD_RIGHT),
    R(RETRO_DEVICE_ID_JOYPAD_R2), -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
constexpr int kRetroSideways[WiInputCount] = {
    R(RETRO_DEVICE_ID_JOYPAD_X), R(RETRO_DEVICE_ID_JOYPAD_Y), R(RETRO_DEVICE_ID_JOYPAD_B), R(RETRO_DEVICE_ID_JOYPAD_A),
    R(RETRO_DEVICE_ID_JOYPAD_SELECT), R(RETRO_DEVICE_ID_JOYPAD_START), R(RETRO_DEVICE_ID_JOYPAD_R3),
    R(RETRO_DEVICE_ID_JOYPAD_UP), R(RETRO_DEVICE_ID_JOYPAD_DOWN), R(RETRO_DEVICE_ID_JOYPAD_LEFT), R(RETRO_DEVICE_ID_JOYPAD_RIGHT),
    R(RETRO_DEVICE_ID_JOYPAD_R2), -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
constexpr int kRetroNunchuk[WiInputCount] = {
    R(RETRO_DEVICE_ID_JOYPAD_A), R(RETRO_DEVICE_ID_JOYPAD_B), R(RETRO_DEVICE_ID_JOYPAD_START), R(RETRO_DEVICE_ID_JOYPAD_SELECT),
    R(RETRO_DEVICE_ID_JOYPAD_L), R(RETRO_DEVICE_ID_JOYPAD_R), R(RETRO_DEVICE_ID_JOYPAD_R3),
    R(RETRO_DEVICE_ID_JOYPAD_UP), R(RETRO_DEVICE_ID_JOYPAD_DOWN), R(RETRO_DEVICE_ID_JOYPAD_LEFT), R(RETRO_DEVICE_ID_JOYPAD_RIGHT),
    R(RETRO_DEVICE_ID_JOYPAD_R2), R(RETRO_DEVICE_ID_JOYPAD_X), R(RETRO_DEVICE_ID_JOYPAD_Y), R(RETRO_DEVICE_ID_JOYPAD_L2),
    -1, -1, -1, -1, -1, -1, -1, -1, -1};
constexpr int kRetroClassic[WiInputCount] = {
    -1, -1, -1, -1, R(RETRO_DEVICE_ID_JOYPAD_SELECT), R(RETRO_DEVICE_ID_JOYPAD_START), R(RETRO_DEVICE_ID_JOYPAD_R3),
    R(RETRO_DEVICE_ID_JOYPAD_UP), R(RETRO_DEVICE_ID_JOYPAD_DOWN), R(RETRO_DEVICE_ID_JOYPAD_LEFT), R(RETRO_DEVICE_ID_JOYPAD_RIGHT),
    -1, -1, -1, -1,
    R(RETRO_DEVICE_ID_JOYPAD_A), R(RETRO_DEVICE_ID_JOYPAD_B), R(RETRO_DEVICE_ID_JOYPAD_X), R(RETRO_DEVICE_ID_JOYPAD_Y),
    R(RETRO_DEVICE_ID_JOYPAD_L), R(RETRO_DEVICE_ID_JOYPAD_R), R(RETRO_DEVICE_ID_JOYPAD_L2), R(RETRO_DEVICE_ID_JOYPAD_R2), -1};
static_assert(WiCentre == 23 && WiInputCount == 24, "the retro tables list every WiiInput");

const int *retro_table(int controller)
{
    switch (controller)
    {
    case WiiRemote: return kRetroRemote;
    case WiiSideways: return kRetroSideways;
    case WiiClassic: return kRetroClassic;
    default: return kRetroNunchuk; /* the Nunchuk device, also for two controllers */
    }
}

/* Sticks pushed past half way are the D-pad. */
std::uint16_t stick_dpad(std::int16_t x, std::int16_t y, const int *retro)
{
    constexpr int kPush = 16000;
    std::uint16_t out = 0;
    auto set = [&](int input) {
        if (retro[input] >= 0)
            out |= static_cast<std::uint16_t>(1u << retro[input]);
    };
    if (y < -kPush)
        set(WiUp);
    if (y > kPush)
        set(WiDown);
    if (x < -kPush)
        set(WiLeft);
    if (x > kPush)
        set(WiRight);
    return out;
}

const aim::Basis &pose_basis(int pose)
{
    switch (pose)
    {
    case PoseTriggerRight: return aim::kGripUprightRight;
    case PoseTriggerLeft: return aim::kGripUprightLeft;
    case PoseFacingRight: return aim::kGripFacingRight;
    case PoseFacingLeft: return aim::kGripFacingLeft;
    default: return aim::kGripNormal;
    }
}

/* The hold gravity shows, for the grip setting: -1 when it isn't clear (or
 * the setting doesn't allow what is seen). */
int classify_pose(const float accel[3], const WiiConfig &c, bool second)
{
    if (c.controller == WiiSideways || c.controller == WiiClassic || c.grip == GripBothHands)
        return PoseFlat; /* always held in both hands (Dolphin turns a sideways Remote itself) */
    const float n = std::sqrt(accel[0] * accel[0] + accel[1] * accel[1] + accel[2] * accel[2]);
    if (n < 0.6f || n > 1.4f)
        return -1;
    const float ux = accel[0] / n, uy = accel[1] / n;
    const bool facing = c.grip == GripUprightFacing || (c.grip == GripAuto && c.controller == WiiTwoControllers);
    if (ux > 0.75f) /* its right side up: in the right hand */
        return facing ? PoseFacingRight : PoseTriggerRight;
    if (ux < -0.75f)
        return facing ? PoseFacingLeft : PoseTriggerLeft;
    if (uy > 0.75f && c.grip == GripAuto)
        return PoseFlat;
    (void)second;
    return -1;
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

/* The motion in this poll's readings: each one fused into the controller's
 * orientation, in order; the newest as it is for the readout and the core. */
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
    float elapsed = 0;
    for (int i = 0; i < n; ++i)
    {
        const PadSample &p = *order[i];
        if (slot.last_motion_us && p.timestamp_us <= slot.last_motion_us)
            continue; /* already seen */
        const float dt = slot.last_motion_us ? float(p.timestamp_us - slot.last_motion_us) * 1e-6f : 0.0f;
        aim::fuse(slot.fusion, p.angular_velocity, p.acceleration, dt);
        slot.last_motion_us = p.timestamp_us;
        elapsed += std::min(dt, 0.05f);
        const bool first = slot.up[0] == 0 && slot.up[1] == 0 && slot.up[2] == 0;
        const float k = first ? 1.0f : std::min(1.0f, std::min(dt, 0.05f) / 0.16f);
        for (int a = 0; a < 3; ++a)
            slot.up[a] += (p.acceleration[a] - slot.up[a]) * k;
    }
    const PadSample &last = *order[n - 1];
    m.valid = true;
    m.samples = unsigned(n);
    for (int i = 0; i < 3; ++i)
    {
        m.raw_accel[i] = last.acceleration[i];
        m.raw_gyro[i] = last.angular_velocity[i];
        m.gyro_bias[i] = slot.fusion.bias[i];
    }
    for (int i = 0; i < 4; ++i)
        m.orientation[i] = last.orientation[i];
    const float mag = std::sqrt(m.raw_accel[0] * m.raw_accel[0] + m.raw_accel[1] * m.raw_accel[1] +
                                m.raw_accel[2] * m.raw_accel[2]);

    /* How it is held. Read when the pointer is centred; after that a
     * different hold has to be seen steadily for a few seconds (so a punch or
     * a swing never flips it). */
    const int index = int(&slot - g_slots);
    const bool second = g_wii.controller == WiiTwoControllers && index == 1;
    float g[3];
    for (int i = 0; i < 3; ++i)
        g[i] = m.raw_gyro[i] - slot.fusion.bias[i];
    const float turning = std::sqrt(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
    const int seen = classify_pose(slot.up, g_wii, second);
    bool reposed = false;
    if (slot.pose < 0)
        slot.pose = seen >= 0 ? seen : expected_pose(g_wii, second);
    else if (slot.centred && seen >= 0 && seen != slot.pose && turning < 1.0f)
    {
        if (seen != slot.pose_seen)
            slot.pose_wait = 0;
        slot.pose_seen = seen;
        slot.pose_wait += elapsed;
        if (slot.pose_wait > 3.0f)
        {
            slot.pose = seen;
            slot.pose_wait = 0;
            reposed = true;
        }
    }
    else
        slot.pose_wait = 0;

    /* Centring: half a second in (once gravity has settled, and the
     * controller is fairly still - or after three seconds regardless), when
     * the hold changes, and on R1 (L1 in the left hand). It makes the hold of
     * that moment the Remote held level and pointing at the middle of the
     * screen, so a natural grip reads as one. In two-controller play the
     * Remote's R1 centres the Nunchuk too. */
    /* R1 has to be held a moment with the controller fairly still: a squeeze
     * of the grip mid-swing (R1 sits just above R2) never centres. */
    const bool centre_held = (last.buttons & (pose_left_hand(slot.pose) ? pad_l1 : pad_r1)) != 0;
    const bool centre_button = g_wii.controller != WiiSideways && g_wii.controller != WiiClassic && !second;
    bool pressed_centre = false;
    if (!centre_held || !centre_button)
    {
        slot.centre_hold = 0;
        slot.centre_was = false; /* here: centred during this hold already */
    }
    else if (!slot.centre_was)
    {
        slot.centre_hold = turning < 1.2f ? slot.centre_hold + elapsed : 0.0f;
        if (slot.centre_hold >= 0.3f)
            pressed_centre = slot.centre_was = true;
    }
    const bool settled = slot.fusion.started && slot.fusion.age > 0.5f && (turning < 0.8f || slot.fusion.age > 3.0f);
    const bool follow = second && slot.centre_seq != g_centre_seq;
    if ((!slot.centred && settled) || reposed || pressed_centre || follow)
    {
        if ((pressed_centre || follow) && seen >= 0)
            slot.pose = seen; /* centring also reads the hold again */
        aim::level_grip(pose_basis(slot.pose), slot.up, slot.basis);
        slot.centre = aim::remote_angles(slot.fusion, slot.basis);
        slot.centred = true;
        slot.pose_wait = 0;
        slot.centre_seq = g_centre_seq;
        if (pressed_centre && g_wii.controller == WiiTwoControllers && index == 0)
            ++g_centre_seq;
        if (pressed_centre)
            ++slot.centrings;
    }
    m.centrings = slot.centrings;
    if (!slot.centred)
        slot.basis = pose_basis(slot.pose);
    m.pose = slot.pose;

    /* For the core: the Remote's (or the Nunchuk's) own axes, the gyroscope
     * without its drift. */
    aim::to_remote(slot.basis, m.raw_accel, m.accel);
    aim::to_remote(slot.basis, g, m.gyro);

    /* Where it points, against the centre. */
    const aim::Angles now = aim::remote_angles(slot.fusion, slot.basis);
    m.roll = now.roll;
    if (g_wii.pointer == PointerGyro)
    {
        float x = 0, y = 0;
        if (slot.centred)
        {
            /* Held calmly against an edge, a drifted pointer eases back. */
            if (turning < 0.5f)
                aim::ease_edge(now, slot.centre, g_wii.speed, elapsed);
            aim::pointer(now, slot.centre, g_wii.speed, x, y);
        }
        m.aim_x = g_wii.invert_x ? -x : x;
        m.aim_y = g_wii.invert_y ? -y : y;
    }
    m.touching = (last.touch_count & 0x7f) > 0;
    if (m.touching)
    {
        m.touch_x = last.touch[0].x;
        m.touch_y = last.touch[0].y;
    }
    if (g_wii.pointer == PointerTouch)
    {
        if (m.touching)
        {
            slot.pointer_x = float(m.touch_x) / 1919.0f * 2.0f - 1.0f;
            slot.pointer_y = float(m.touch_y) / 1030.0f * 2.0f - 1.0f; /* the pad reads 0..~1030 down */
        }
        m.aim_x = slot.pointer_x;
        m.aim_y = slot.pointer_y;
    }
    m.pointer_x = std::clamp(m.aim_x, -1.0f, 1.0f);
    m.pointer_y = std::clamp(m.aim_y, -1.0f, 1.0f);
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
        auto bits = [&](const WiiLayout &lay, const int *retro) {
            std::uint16_t out = 0;
            for (int k = 0; k < lay.count; ++k)
            {
                const WiiBinding &w = lay.binds[k];
                if (w.control >= 0 && w.control < CtlCount && (b & kControlPadBit[w.control]) && retro[w.input] >= 0)
                    out |= static_cast<std::uint16_t>(1u << retro[w.input]);
            }
            if (lay.left_stick == StickDPad)
                out |= stick_dpad(next.left_x, next.left_y, retro);
            if (lay.right_stick == StickDPad)
                out |= stick_dpad(next.right_x, next.right_y, retro);
            return out;
        };
        const int *retro = retro_table(g_wii.controller);
        next.joypad = bits(wii_layout(g_wii, false, slot.pose), retro);
        /* A flick is sent as a shake only where the game doesn't feel the
         * motion itself. */
        const bool with_nunchuk = g_wii.controller == WiiRemoteNunchuk || g_wii.controller == WiiTwoControllers;
        const bool remote_feels = g_wii.motion && (!with_nunchuk || g_nunchuk_motion);
        if (motion.shaking && g_wii.controller != WiiClassic && !remote_feels)
            next.joypad |= static_cast<std::uint16_t>(1u << RETRO_DEVICE_ID_JOYPAD_R2);
        if (g_wii.controller == WiiClassic)
        {
            next.l2 = static_cast<std::int16_t>(newest->left_trigger * 0x7fff / 255);
            next.r2 = static_cast<std::int16_t>(newest->right_trigger * 0x7fff / 255);
        }
        if (g_wii.controller == WiiTwoControllers)
        {
            /* The same controller as the second one: its Nunchuk buttons. */
            next.nunchuk = bits(wii_layout(g_wii, true, slot.pose), kRetroNunchuk);
            if (motion.shaking && !(g_wii.motion && g_nunchuk_motion))
                next.nunchuk |= static_cast<std::uint16_t>(1u << RETRO_DEVICE_ID_JOYPAD_L2);
        }
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
    const bool turned = config.grip != g_wii.grip || config.controller != g_wii.controller;
    g_wii = config;
    if ((config.active && !was) || turned)
        for (Slot &slot : g_slots)
        {
            /* A new game or a new way of holding it: find the middle again. */
            slot.pointer_x = slot.pointer_y = 0;
            slot.centred = false;
            slot.pose = -1;
            slot.fusion.age = 0.0f;
            slot.centre_seq = g_centre_seq;
        }
}

int expected_pose(const WiiConfig &config, bool second)
{
    if (config.controller == WiiSideways || config.controller == WiiClassic)
        return PoseFlat;
    switch (config.grip)
    {
    case GripBothHands: return PoseFlat;
    case GripUprightTrigger: return second ? PoseTriggerLeft : PoseTriggerRight;
    case GripUprightFacing: return second ? PoseFacingLeft : PoseFacingRight;
    default:
        if (config.controller == WiiTwoControllers)
            return second ? PoseFacingLeft : PoseFacingRight;
        return config.controller == WiiRemote ? PoseTriggerRight : PoseFlat;
    }
}

WiiLayout wii_layout(const WiiConfig &config, bool second, int pose)
{
    WiiLayout lay;
    if (pose < 0)
        pose = expected_pose(config, second);
    auto add = [&](const WiiBinding *b, std::size_t n) {
        for (std::size_t i = 0; i < n && lay.count < int(std::size(lay.binds)); ++i)
            lay.binds[lay.count++] = b[i];
    };
    const bool stick_aims = config.pointer == PointerStick;
    if (second)
    {
        add(kLayTwoNunchuk, std::size(kLayTwoNunchuk));
        lay.left_stick = StickNunchuk;
        lay.second_controller = true;
        return lay;
    }
    switch (config.controller)
    {
    case WiiRemote:
    case WiiTwoControllers:
        if (pose_left_hand(pose))
            add(kLayRemoteLeft, std::size(kLayRemoteLeft));
        else
        {
            add(kLayRemote, std::size(kLayRemote));
            add(kDPadAsDPad, std::size(kDPadAsDPad));
        }
        lay.left_stick = StickDPad;
        lay.right_stick = stick_aims ? StickPointer : StickDPad;
        break;
    case WiiSideways:
        add(kLaySideways, std::size(kLaySideways));
        add(kDPadAsDPad, std::size(kDPadAsDPad));
        lay.left_stick = StickDPad;
        lay.right_stick = stick_aims ? StickPointer : StickNothing;
        break;
    case WiiClassic:
        add(kLayClassic, std::size(kLayClassic));
        add(kDPadAsDPad, std::size(kDPadAsDPad));
        lay.left_stick = StickClassicLeft;
        lay.right_stick = StickClassicRight;
        break;
    default:
        add(kLayNunchuk, std::size(kLayNunchuk));
        add(kDPadAsDPad, std::size(kDPadAsDPad));
        lay.left_stick = StickNunchuk;
        /* The core tilts the Remote with the right stick unless it aims. */
        lay.right_stick = stick_aims ? StickPointer : StickTilt;
        break;
    }
    return lay;
}

void set_nunchuk_motion(bool on)
{
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    g_nunchuk_motion = on;
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
    {
        g_slots[player].pointer_x = g_slots[player].pointer_y = 0;
        g_slots[player].centred = false;
    }
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
