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

/* ---- the Wii Remote, from a DualSense --------------------------------------------------- */

/* What a Wii game's port 0..3 is (the Dolphin core's libretro devices). */
enum WiiController : int
{
    WiiRemoteNunchuk = 0, /* the Remote's pointer and buttons on the right, the Nunchuk on the left */
    WiiRemote = 1,        /* the Remote alone, held pointing at the TV: real motion */
    WiiSideways = 2,      /* the Remote held sideways (NES-style, steering by tilting) */
    WiiClassic = 3,       /* the Classic Controller */
    WiiTwoControllers = 4, /* beta: controller 1 the Remote, controller 2 the Nunchuk */
    WiiControllerCount,
};
/* Where the Remote's pointer comes from. */
enum WiiPointer : int
{
    PointerGyro = 0,  /* turn the controller; R1 re-centres */
    PointerTouch = 1, /* the touch pad, as a little screen */
    PointerStick = 2, /* the right stick (the core's own) */
};
/* How the DualSense is held (the setting). Auto reads it from gravity. */
enum WiiGrip : int
{
    GripAuto = 0,
    GripBothHands = 1,      /* face up in both hands, the trigger edge toward the TV */
    GripUprightTrigger = 2, /* stood on end in one hand, the trigger edge toward the TV */
    GripUprightFacing = 3,  /* stood on end in one hand, its face toward the player */
    GripCount,
};
/* How it is actually held, as read: which way it points and which hand. */
enum WiiPose : int
{
    PoseFlat = 0,         /* face up, in both hands */
    PoseTriggerRight = 1, /* upright in the right hand, the face to the left, R2 under the index finger */
    PoseTriggerLeft = 2,  /* its mirror image in the left hand */
    PoseFacingRight = 3,  /* upright in the right hand, the face toward the player, the grips pointing right */
    PoseFacingLeft = 4,   /* its mirror image in the left hand, the grips pointing left */
    PoseCount,
};
inline bool pose_left_hand(int pose) { return pose == PoseTriggerLeft || pose == PoseFacingLeft; }
/* What a DualSense control is on the Wii controller being played. */
enum WiiInput : int
{
    WiA, WiB, WiOne, WiTwo, WiMinus, WiPlus, WiHome,
    WiUp, WiDown, WiLeft, WiRight,
    WiShake,        /* shakes the Remote */
    WiC, WiZ,       /* the Nunchuk's buttons */
    WiNunchukShake,
    WiClA, WiClB, WiClX, WiClY, WiClZL, WiClZR, WiClL, WiClR, /* the Classic Controller's */
    WiCentre,       /* Porpoise: the pointer back to the middle */
    WiInputCount,
};
struct WiiBinding
{
    int control; /* Control */
    int input;   /* WiiInput */
};
/* What each stick does. */
enum WiiStick : int
{
    StickNothing,
    StickDPad,        /* pushed, it is the D-pad */
    StickNunchuk,     /* the Nunchuk's stick */
    StickClassicLeft, /* the Classic Controller's sticks */
    StickClassicRight,
    StickPointer,     /* aims the pointer (Pointer: right stick) */
    StickTilt,        /* tilts the Remote (the core's, with the Nunchuk) */
};
struct WiiLayout
{
    WiiBinding binds[24];
    int count = 0;
    int left_stick = StickNothing, right_stick = StickNothing;
    bool second_controller = false; /* two controllers: this is the Nunchuk one's */
};
struct WiiConfig
{
    bool active = false; /* a Wii game is running */
    int controller = WiiRemoteNunchuk;
    int pointer = PointerGyro;
    int speed = 5;       /* 1..10 */
    int grip = GripAuto;
    bool motion = true;  /* the sensors go to the core */
    bool shake = true;   /* a flick of the controller is a shake */
    bool invert_x = false, invert_y = false;
    /* The screen as measured by the Wii Remote setup: radians from its middle
     * to its edges (0: use speed). */
    float half_x = 0, half_y = 0;
};
void set_wii(const WiiConfig &config);
/* Centre player's pointer now, as R1 held would (the Wii Remote setup), and
 * move the centre by (yaw, pitch) radians (yaw + right, pitch + up). */
void centre_now(int player);
void shift_centre(int player, float yaw, float pitch);

/* The core now gets the real motion of a Remote with the Nunchuk (and, in
 * two-controller play, of the Nunchuk): flicks needn't be sent as shakes. */
void set_nunchuk_motion(bool on);
WiiConfig wii();

/* The buttons and sticks of a Wii controller, as played and as drawn. The
 * second controller of "two controllers" has its own (second = true). */
WiiLayout wii_layout(const WiiConfig &config, bool second = false, int pose = -1);
/* The pose a grip setting means before anything is read (for drawings): the
 * Remote's, or with second the two-controller Nunchuk's. */
int expected_pose(const WiiConfig &config, bool second = false);

/* The libretro device a Wii game's port gets for a WiiController. */
unsigned wii_device(int controller);

struct Motion
{
    bool valid = false;
    /* As the console gives them: g and rad/s, in the DualSense's own axes
     * (x right, y out of its face, z toward the player). */
    float raw_accel[3] = {0, 0, 0}, raw_gyro[3] = {0, 0, 0};
    float orientation[4] = {0, 0, 0, 1};
    /* The same, turned into the Wii Remote's axes for the grip (x left, y back
     * toward the player, z up), as the core wants them. */
    float accel[3] = {0, 0, 0}, gyro[3] = {0, 0, 0};
    /* The pointer, -1..1 across the screen (0 the centre, y down), held to the
     * screen; aim_x / aim_y are the same past the edges (pointing off the
     * screen), roll the Remote's turn about where it points (radians, + its
     * top to the right). */
    float pointer_x = 0, pointer_y = 0;
    float aim_x = 0, aim_y = 0, roll = 0;
    float gyro_bias[3] = {0, 0, 0}; /* the drift learnt so far, rad/s */
    int pose = PoseFlat;            /* how it is held, as read (WiiPose) */
    unsigned centrings = 0;         /* times the player has centred it (R1 held) */
    bool centred = false;           /* the pointer has a middle */
    float rel_yaw = 0, rel_pitch = 0; /* radians from the middle: + right, + up */
    bool touching = false;
    int touch_x = 0, touch_y = 0; /* the first finger, 0..1919 x 0..1079 */
    bool shaking = false;         /* a flick was felt in the last few frames */
    unsigned samples = 0;         /* readings this poll */
};

struct State
{
    Motion motion;
    bool connected = false;
    std::uint16_t joypad = 0; /* bit n = RETRO_DEVICE_ID_JOYPAD_n, through the mapping */
    std::uint16_t nunchuk = 0; /* two-controller Wii play: this controller's Nunchuk buttons */
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
/* Put the pointer back in the middle (R1 does it too). */
void recenter(int player);
/* libretro rumble for one player: strength 0..0xffff per motor. */
void set_rumble(int player, bool strong, std::uint16_t strength);
} // namespace porpoise::pad
