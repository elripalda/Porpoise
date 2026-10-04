/* Porpoise - aiming a DualSense like a Wii Remote (see porpoise_aim.hpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_aim.hpp"

#include <algorithm>
#include <cmath>

namespace porpoise::aim
{
namespace
{
constexpr float kPi = 3.14159265358979f;

struct V3
{
    float x, y, z;
};
V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

/* q (x, y, z, w) applied to v. */
V3 rotate(const float q[4], V3 v)
{
    const V3 u{q[0], q[1], q[2]};
    const float s = q[3];
    return u * (2.0f * dot(u, v)) + v * (s * s - dot(u, u)) + cross(u, v) * (2.0f * s);
}
V3 rotate_back(const float q[4], V3 v)
{
    const float c[4] = {-q[0], -q[1], -q[2], q[3]};
    return rotate(c, v);
}
void normalise(float q[4])
{
    const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (n <= 0)
    {
        q[0] = q[1] = q[2] = 0;
        q[3] = 1;
        return;
    }
    for (int i = 0; i < 4; ++i)
        q[i] /= n;
}
float wrap(float a)
{
    while (a > kPi)
        a -= 2 * kPi;
    while (a < -kPi)
        a += 2 * kPi;
    return a;
}
V3 row(const Basis &b, int r) { return {b.m[r][0], b.m[r][1], b.m[r][2]}; }

/* The accelerometer at rest reads a little over 1 g on a PS5. */
constexpr float kRestG = 1.035f;
} // namespace

void to_remote(const Basis &b, const float in[3], float out[3])
{
    const V3 v{in[0], in[1], in[2]};
    out[0] = dot(row(b, 0), v);
    out[1] = dot(row(b, 1), v);
    out[2] = dot(row(b, 2), v);
}

void fuse(Fusion &f, const float gyro[3], const float accel[3], float dt)
{
    const V3 a{accel[0], accel[1], accel[2]};
    const float an = std::sqrt(dot(a, a));
    if (!f.started)
    {
        /* Start level with gravity: the accelerometer reads "up". */
        if (an < 0.5f)
            return;
        const V3 up = a * (1.0f / an);
        const V3 z{0, 0, 1};
        const V3 axis = cross(up, z);
        const float s = std::sqrt(dot(axis, axis)), c = dot(up, z);
        if (s < 1e-6f)
        {
            f.q[0] = c > 0 ? 0.0f : 1.0f;
            f.q[1] = f.q[2] = 0;
            f.q[3] = c > 0 ? 1.0f : 0.0f;
        }
        else
        {
            const float half = std::atan2(s, c) / 2;
            const V3 n = axis * (1.0f / s);
            f.q[0] = n.x * std::sin(half);
            f.q[1] = n.y * std::sin(half);
            f.q[2] = n.z * std::sin(half);
            f.q[3] = std::cos(half);
        }
        f.started = true;
        f.age = f.still = 0;
        return;
    }
    dt = std::clamp(dt, 0.0f, 0.05f);
    f.age += dt;
    const V3 raw{gyro[0], gyro[1], gyro[2]};
    V3 w = raw - V3{f.bias[0], f.bias[1], f.bias[2]};

    /* The gyroscope's drift: what it reads while the controller lies still. */
    const bool quiet = std::fabs(w.x) < 0.04f && std::fabs(w.y) < 0.04f && std::fabs(w.z) < 0.04f &&
                       std::fabs(an - kRestG) < 0.06f;
    f.still = quiet ? f.still + dt : 0.0f;
    if (f.still > 0.4f)
    {
        const float k = std::min(1.0f, dt / 1.5f);
        f.bias[0] += (raw.x - f.bias[0]) * k;
        f.bias[1] += (raw.y - f.bias[1]) * k;
        f.bias[2] += (raw.z - f.bias[2]) * k;
        w = raw - V3{f.bias[0], f.bias[1], f.bias[2]};
    }

    /* Gravity pulls the estimate of "up" back, gently once settled. */
    if (an > 0.8f && an < 1.25f)
    {
        const V3 measured = a * (1.0f / an);
        const V3 estimated = rotate_back(f.q, {0, 0, 1});
        const float kp = f.age < 1.0f ? 5.0f : 0.5f;
        w = w + cross(measured, estimated) * kp;
    }

    /* q += q * (0, w) dt / 2 */
    const float hx = w.x * dt / 2, hy = w.y * dt / 2, hz = w.z * dt / 2;
    const float x = f.q[0], y = f.q[1], z = f.q[2], s = f.q[3];
    f.q[0] = x + s * hx + y * hz - z * hy;
    f.q[1] = y + s * hy + z * hx - x * hz;
    f.q[2] = z + s * hz + x * hy - y * hx;
    f.q[3] = s - x * hx - y * hy - z * hz;
    normalise(f.q);
}

Angles remote_angles(const Fusion &f, const Basis &b)
{
    /* The world: x "left", y "back", z up (yaw's zero is arbitrary). */
    const V3 left = rotate(f.q, row(b, 0));
    const V3 back = rotate(f.q, row(b, 1));
    const V3 up = rotate(f.q, row(b, 2));
    const V3 fwd = back * -1.0f;
    Angles out;
    out.yaw = std::atan2(-fwd.x, -fwd.y);
    out.pitch = std::asin(std::clamp(fwd.z, -1.0f, 1.0f));
    /* Roll: the Remote's top against the vertical through where it points. */
    const V3 level_left{std::cos(out.yaw), -std::sin(out.yaw), 0};
    out.roll = -std::atan2(dot(level_left, up), dot(level_left, left));
    return out;
}

Scale scale_for(int speed, float measured_x, float measured_y)
{
    constexpr float kTall = 0.65f; /* up-down turning to the edge, against left-right's */
    if (measured_x > 0.03f && measured_y > 0.03f)
        return {measured_x, measured_y};
    /* Degrees from the middle to the edge; 5 is about a real Remote's. */
    static constexpr float kDegrees[10] = {30, 26, 22, 18.5f, 15.5f, 13, 11, 9.5f, 8, 7};
    const float h = kDegrees[std::clamp(speed, 1, 10) - 1] * kPi / 180;
    return {h, h * kTall};
}

void pointer(const Angles &now, const Angles &centre, const Scale &scale, float &x, float &y)
{
    x = wrap(now.yaw - centre.yaw) / scale.half_x;
    y = -(now.pitch - centre.pitch) / scale.half_y;
}

void ease_edge(const Angles &now, Angles &centre, const Scale &scale, float dt)
{
    constexpr float kEdge = 1.02f, kFar = 1.6f, kRate = 0.4f; /* screen halves a second */
    float x, y;
    pointer(now, centre, scale, x, y);
    const float step = kRate * std::clamp(dt, 0.0f, 0.05f);
    if (std::fabs(x) > kEdge && std::fabs(x) < kFar)
    {
        const float move = std::min(step, std::fabs(x) - kEdge);
        centre.yaw = wrap(centre.yaw + std::copysign(move, x) * scale.half_x);
    }
    if (std::fabs(y) > kEdge && std::fabs(y) < kFar)
    {
        const float move = std::min(step, std::fabs(y) - kEdge);
        centre.pitch -= std::copysign(move, y) * scale.half_y;
    }
}

bool level_grip(const Basis &grip, const float up[3], Basis &out)
{
    out = grip;
    V3 u{up[0], up[1], up[2]};
    const float n = std::sqrt(dot(u, u));
    if (n < 0.5f)
        return false;
    u = u * (1.0f / n);
    const V3 fwd = row(grip, 1) * -1.0f;
    const float along = dot(fwd, u);
    if (std::fabs(along) > 0.82f) /* more than ~55 degrees up or down */
        return false;
    V3 f = fwd - u * along;
    f = f * (1.0f / std::sqrt(dot(f, f)));
    const V3 back = f * -1.0f, left = cross(back, u);
    const V3 rows[3] = {left, back, u};
    for (int r = 0; r < 3; ++r)
    {
        out.m[r][0] = rows[r].x;
        out.m[r][1] = rows[r].y;
        out.m[r][2] = rows[r].z;
    }
    return true;
}

namespace
{
void sensor_bar_at(float x, float y, float roll, Dot out[2])
{
    /* Dolphin's pointer geometry (Core/HW/WiimoteEmu: Dynamics.cpp EmulatePoint,
     * Camera.cpp GetCameraPoints): the Remote 2 m from the bar, the bar 10 cm
     * above it; a 42 degree camera, 4:3. Here the Remote's orientation is built exactly
     * (yaw, then pitch, then roll about where it points) and the bar seen
     * through its inverse. */
    /* The screen's edges at 12.5 degrees either side and 10 above and below
     * (Dolphin's own defaults, chosen to reach the edges in most games), and a
     * little past them so the pointer's edge is surely the game's; both lights
     * stay in the camera's view even in the corners. */
    constexpr float kEdgeX = 12.5f * kPi / 180.0f, kEdgeY = 10.0f * kPi / 180.0f, kPast = 1.15f;
    constexpr float kDistance = 2.0f, kHeight = 0.10f, kSeparation = 0.2f;
    constexpr float kFovX = 42.0f * kPi / 180.0f, kFovY = kFovX / (4.0f / 3.0f);
    const float yaw = x * kEdgeX * kPast, pitch = -y * kEdgeY * kPast;
    /* M = Rz(-yaw) Rx(-pitch) Ry(-roll), axes x left, y back, z up. */
    const float cz = std::cos(-yaw), sz = std::sin(-yaw);
    const float cx = std::cos(-pitch), sx = std::sin(-pitch);
    const float cy = std::cos(-roll), sy = std::sin(-roll);
    const float rz[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
    const float rx[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
    const float ry[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
    float t[3][3] = {}, m[3][3] = {};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                t[i][j] += rz[i][k] * rx[k][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                m[i][j] += t[i][k] * ry[k][j];
    const float tan_half = std::tan(kFovY / 2.0f);
    for (int i = 0; i < 2; ++i)
    {
        out[i] = Dot{};
        /* The light relative to the Remote, (0, 2, -height) away, in the
         * Remote's axes: M transposed. */
        const V3 v{i == 0 ? -kSeparation / 2 : kSeparation / 2, -kDistance, kHeight};
        const V3 r{m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z, m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
                   m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z};
        /* The camera looks along -y: (x, y, z) -> (x, -z, y), then perspective. */
        const float px = r.x, py = -r.z, pz = r.y;
        const float w = -pz;
        if (w <= 0)
            continue;
        const float clip_x = px / (tan_half * (kFovX / kFovY)), clip_y = py / tan_half;
        const float u = (1 - clip_x / w) / 2, v2 = (1 - clip_y / w) / 2;
        if (u < 0 || v2 < 0 || u >= 1 || v2 >= 1)
            continue;
        out[i] = Dot{u, v2, true};
    }
}
} // namespace

void sensor_bar(float x, float y, float roll, Dot out[2])
{
    /* Up to a little past the screen's edges the game's cursor stays pinned
     * at the edge (a real Remote would lose the bar there, but on a menu's
     * top row that only makes the cursor blink away); further out, the lights
     * go, as when a Remote points away from the TV. The roll the camera sees
     * is held to 30 degrees: enough for the cursor to tilt with the hand,
     * without turning the bar out of the camera's narrow view. */
    if (std::fabs(x) > 1.3f || std::fabs(y) > 1.3f)
    {
        out[0] = out[1] = Dot{};
        return;
    }
    x = std::clamp(x, -1.06f, 1.06f);
    y = std::clamp(y, -1.06f, 1.06f);
    roll = std::clamp(roll, -0.52f, 0.52f);
    sensor_bar_at(x, y, roll, out);
    for (int i = 1; i <= 20 && !(out[0].visible && out[1].visible); ++i)
    {
        const float k = 1.0f - 0.02f * float(i);
        sensor_bar_at(x * k, y * k, roll * k, out);
    }
}

void Smoother::reset()
{
    started = false;
}

void Smoother::step(float &x, float &y, float dt, int strength)
{
    /* A "one euro" filter: strong when the pointer is nearly still (hand
     * tremor), light when it moves (no lag on a sweep). */
    static constexpr float kMin[4] = {0, 3.0f, 1.6f, 0.9f}, kBeta[4] = {0, 0.35f, 0.18f, 0.09f};
    strength = std::clamp(strength, 0, 3);
    if (strength == 0 || dt <= 0)
    {
        sx = x;
        sy = y;
        started = true;
        return;
    }
    if (!started)
    {
        sx = x;
        sy = y;
        dx = dy = 0;
        started = true;
        return;
    }
    auto alpha = [&](float cutoff) {
        const float tau = 1.0f / (2 * kPi * cutoff);
        return 1.0f / (1.0f + tau / dt);
    };
    const float ad = alpha(1.0f);
    dx += ((x - sx) / dt - dx) * ad;
    dy += ((y - sy) / dt - dy) * ad;
    const float speed = std::sqrt(dx * dx + dy * dy);
    const float a = alpha(kMin[strength] + kBeta[strength] * speed);
    /* A jump (a re-centring, back from off the screen) is taken at once. */
    if (std::fabs(x - sx) > 0.8f || std::fabs(y - sy) > 0.8f)
        sx = x, sy = y;
    else
    {
        sx += (x - sx) * a;
        sy += (y - sy) * a;
    }
    x = sx;
    y = sy;
}
} // namespace porpoise::aim
