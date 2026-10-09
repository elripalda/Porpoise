/* Porpoise - the menus' sound sets, made in code: no recordings, almost no
 * space.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_sfx.hpp"

#include <algorithm>
#include <cmath>

namespace porpoise::sfx
{
namespace
{
constexpr float kRate = 48000.0f;
constexpr float kPi = 3.14159265358979f;

/* A mono buffer that sounds are added into. */
struct Buf
{
    std::vector<float> s;
    explicit Buf(float seconds) : s(std::size_t(seconds * kRate) + 1, 0.0f) {}
};

/* Attack, then an exponential decay to silence by `length`. */
float env(float t, float attack, float length)
{
    if (t < 0 || t > length)
        return 0.0f;
    const float a = attack > 0 ? std::min(1.0f, t / attack) : 1.0f;
    const float d = std::exp(-5.0f * t / length);
    const float tail = std::min(1.0f, (length - t) / 0.004f); /* no click at the end */
    return a * d * tail;
}

enum Wave
{
    Sine,
    Triangle,
};

float osc(Wave w, float phase)
{
    const float p = phase - std::floor(phase);
    return w == Triangle ? 4.0f * std::fabs(p - 0.5f) - 1.0f : std::sin(2 * kPi * p);
}

/* A tone from f0 gliding to f1, starting at `at` seconds. */
void tone(Buf &b, Wave w, float at, float f0, float f1, float length, float attack, float gain)
{
    float phase = 0;
    const std::size_t from = std::size_t(at * kRate), n = std::size_t(length * kRate);
    for (std::size_t i = 0; i < n && from + i < b.s.size(); ++i)
    {
        const float t = float(i) / kRate;
        const float f = f0 + (f1 - f0) * (t / length);
        phase += f / kRate;
        b.s[from + i] += gain * env(t, attack, length) * osc(w, phase);
    }
}

/* Noise through a one-pole filter: low (lowpass) or high (highpass). */
void noise(Buf &b, float at, float length, float attack, float cutoff, bool high, float gain, unsigned seed)
{
    const std::size_t from = std::size_t(at * kRate), n = std::size_t(length * kRate);
    const float k = 1.0f - std::exp(-2 * kPi * cutoff / kRate);
    float lp = 0;
    unsigned x = seed * 2654435761u + 1;
    for (std::size_t i = 0; i < n && from + i < b.s.size(); ++i)
    {
        x = x * 1664525u + 1013904223u;
        const float w = float(int(x >> 8) - (1 << 23)) / float(1 << 23);
        lp += k * (w - lp);
        const float v = high ? w - lp : lp;
        b.s[from + i] += gain * env(float(i) / kRate, attack, length) * v;
    }
}

/* To 48 kHz stereo, at `peak` of full scale, the right a touch later (width). */
std::vector<std::int16_t> stereo(const Buf &b, float peak = 0.5f)
{
    float m = 1e-6f;
    for (float v : b.s)
        m = std::max(m, std::fabs(v));
    const float g = peak / m;
    std::vector<std::int16_t> out(b.s.size() * 2);
    for (std::size_t i = 0; i < b.s.size(); ++i)
    {
        const float l = b.s[i] * g, r = (i >= 24 ? b.s[i - 24] : 0.0f) * g;
        out[i * 2] = std::int16_t(std::clamp(l, -1.0f, 1.0f) * 32767.0f);
        out[i * 2 + 1] = std::int16_t(std::clamp(0.85f * r + 0.15f * l, -1.0f, 1.0f) * 32767.0f);
    }
    return out;
}

float note(int semitones_from_a4)
{
    return 440.0f * std::pow(2.0f, float(semitones_from_a4) / 12.0f);
}
} // namespace

void make(Set set, std::vector<std::int16_t> out[5])
{
    for (int i = 0; i < 5; ++i)
        out[i].clear();
    switch (set)
    {
    case Soft:
    {
        Buf row(0.09f), scroll(0.07f), tab(0.2f), flip(0.3f), launch(0.8f);
        tone(row, Sine, 0, 700, 640, 0.08f, 0.004f, 1);
        tone(scroll, Sine, 0, 980, 940, 0.06f, 0.003f, 1);
        tone(tab, Sine, 0, note(3), note(3), 0.09f, 0.005f, 1);    /* C5 */
        tone(tab, Sine, 0.07f, note(10), note(10), 0.12f, 0.005f, 0.9f); /* G5 */
        noise(flip, 0, 0.28f, 0.08f, 900, false, 0.5f, 3);
        tone(flip, Sine, 0.02f, 440, 520, 0.26f, 0.05f, 0.8f);
        for (int k = 0; k < 3; ++k) /* C5 E5 G5, then C6 */
            tone(launch, Sine, 0.07f * float(k), note(3 + (k == 1 ? 4 : k == 2 ? 7 : 0)), note(3 + (k == 1 ? 4 : k == 2 ? 7 : 0)),
                 0.5f, 0.006f, 0.7f);
        tone(launch, Sine, 0.21f, note(15), note(15), 0.55f, 0.006f, 0.8f);
        out[0] = stereo(row, 0.42f);
        out[1] = stereo(scroll, 0.32f);
        out[2] = stereo(tab, 0.45f);
        out[3] = stereo(flip, 0.45f);
        out[4] = stereo(launch, 0.5f);
        break;
    }
    case Crisp:
    {
        Buf row(0.04f), scroll(0.03f), tab(0.1f), flip(0.16f), launch(0.45f);
        noise(row, 0, 0.012f, 0, 3000, true, 0.6f, 1);
        tone(row, Sine, 0, 2200, 2000, 0.035f, 0.0005f, 0.6f);
        noise(scroll, 0, 0.008f, 0, 4000, true, 0.6f, 2);
        tone(scroll, Sine, 0, 2900, 2800, 0.025f, 0.0005f, 0.5f);
        tone(tab, Triangle, 0, 1400, 1400, 0.04f, 0.0005f, 0.8f);
        tone(tab, Triangle, 0.045f, 1900, 1900, 0.05f, 0.0005f, 0.8f);
        noise(flip, 0, 0.14f, 0.01f, 2500, true, 0.5f, 4);
        tone(flip, Sine, 0, 1600, 2400, 0.12f, 0.002f, 0.6f);
        tone(launch, Sine, 0, 1568, 1568, 0.4f, 0.001f, 0.7f);
        tone(launch, Sine, 0.05f, 2349, 2349, 0.4f, 0.001f, 0.6f);
        noise(launch, 0, 0.02f, 0, 5000, true, 0.4f, 5);
        out[0] = stereo(row, 0.4f);
        out[1] = stereo(scroll, 0.3f);
        out[2] = stereo(tab, 0.42f);
        out[3] = stereo(flip, 0.42f);
        out[4] = stereo(launch, 0.48f);
        break;
    }
    default:
        break;
    }
}

} // namespace porpoise::sfx
