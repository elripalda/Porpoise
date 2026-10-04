/* Porpoise - aiming a DualSense like a Wii Remote.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The controller's gyroscope and accelerometer are fused into its orientation
 * (gravity keeps "up" honest, the gyroscope's own drift is learnt while it lies
 * still). From that comes where the Remote points: left-right and up-down
 * angles measured against gravity, so it reads the same however the controller
 * is turned in the hand, and relative to the direction that was "the middle of
 * the screen" when it was last centred. Pointing back at the same spot always
 * gives the same place on the screen.
 *
 * The game is then told what a real Remote's camera would see of the sensor
 * bar from there - including the Remote's roll, so the picture agrees with the
 * accelerometer - with the geometry Dolphin uses for its own pointer.
 *
 * Pure functions, no console calls: tools test them on a PC with recorded data. */
#pragma once

namespace porpoise::aim
{
/* A grip: the Remote's axes in the DualSense's (x right, y out of its face,
 * z toward the player). Rows: the Remote's left, back and up. */
struct Basis
{
    float m[3][3];
};
/* Two hands, face up, the trigger edge toward the TV. */
inline constexpr Basis kGripNormal{{{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}}};
/* Stood on its left grip, held in the right hand: the face toward the left,
 * the right grip up, the index finger on R2, the thumb on the face buttons. */
inline constexpr Basis kGripUprightRight{{{0, 1, 0}, {0, 0, 1}, {1, 0, 0}}};
/* The mirror image, in the left hand: the face toward the right, L2 under the
 * index finger. */
inline constexpr Basis kGripUprightLeft{{{0, -1, 0}, {0, 0, 1}, {-1, 0, 0}}};
/* Stood on end in the right hand, its face toward the player and its back
 * toward the TV, the grips pointing right (two-controller play's Remote). */
inline constexpr Basis kGripFacingRight{{{0, 0, -1}, {0, 1, 0}, {1, 0, 0}}};
/* Its mirror image in the left hand, the grips pointing left (the Nunchuk). */
inline constexpr Basis kGripFacingLeft{{{0, 0, 1}, {0, 1, 0}, {-1, 0, 0}}};

/* A DualSense vector into the Remote's axes (x left, y back, z up). */
void to_remote(const Basis &b, const float in[3], float out[3]);

struct Fusion
{
    float q[4] = {0, 0, 0, 1}; /* the controller's orientation: x, y, z, w; DualSense -> world (z up) */
    float bias[3] = {0, 0, 0};  /* the gyroscope's reading at rest, rad/s */
    bool started = false;
    float age = 0;              /* seconds since it started */
    float still = 0;            /* seconds it has lain still */
};

/* One reading: rad/s and g in the DualSense's axes, dt seconds since the last. */
void fuse(Fusion &f, const float gyro[3], const float accel[3], float dt);

/* Where the Remote points, radians: yaw + to the right, pitch + up (both
 * against gravity, yaw from the world's arbitrary zero), roll about the
 * pointing direction, + turning its top to the right. */
struct Angles
{
    float yaw = 0, pitch = 0, roll = 0;
};
Angles remote_angles(const Fusion &f, const Basis &b);

/* How far the Remote turns from the middle of the screen to its edges, in
 * radians: measured for this screen and seat (Wii Remote setup), or from a
 * pointer speed of 1..10 (5 is about a real Remote two metres from a 50" TV;
 * up-down takes less turning than left-right, as a 16:9 screen does). */
struct Scale
{
    float half_x = 0, half_y = 0;
};
Scale scale_for(int speed, float measured_x = 0, float measured_y = 0);

/* The pointer, from the Remote's angles less the centre's: -1..1 across the
 * screen at the edges, x right, y down; beyond 1 is off the screen. */
void pointer(const Angles &now, const Angles &centre, const Scale &scale, float &x, float &y);

/* A pointer held calmly just past an edge (not swung far off the screen) pulls
 * the centre after it, slowly, so a pointer that has drifted comes back in
 * line when it is pushed against the edge. dt: seconds since the last call. */
void ease_edge(const Angles &now, Angles &centre, const Scale &scale, float dt);

/* The grip made level for how it is held right now: its up becomes gravity's
 * up and its forward the grip's forward made horizontal, so this hold reads
 * as the Remote level and pointing straight ahead. up: the accelerometer
 * (any length). False (and out = grip) if the grip's forward is too steep. */
bool level_grip(const Basis &grip, const float up[3], Basis &out);

/* What the Remote's camera sees of the sensor bar when the pointer is at
 * (x, y) and the Remote rolled by roll radians: each light's x and y over 0..1
 * of the 1024x768 view, and whether it is in view. */
struct Dot
{
    float x = 0, y = 0;
    bool visible = false;
};
void sensor_bar(float x, float y, float roll, Dot out[2]);
} // namespace porpoise::aim
