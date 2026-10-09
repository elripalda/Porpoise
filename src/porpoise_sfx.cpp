/* Porpoise - the menus' sound sets, made in code: no recordings, almost no
 * space. And the player's own GameCube BIOS's sounds as a set.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_sfx.hpp"

#include <algorithm>
#include <cmath>

#include "porpoise_bios.hpp"

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
    Square,   /* 50% */
    Pulse,    /* 12.5%, the chiptune duty */
    Triangle,
};

float osc(Wave w, float phase)
{
    const float p = phase - std::floor(phase);
    switch (w)
    {
    case Square:
        return p < 0.5f ? 0.6f : -0.6f;
    case Pulse:
        return p < 0.125f ? 0.6f : -0.6f;
    case Triangle:
        return 4.0f * std::fabs(p - 0.5f) - 1.0f;
    default:
        return std::sin(2 * kPi * p);
    }
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

/* A bell: a sine carrier with a sine modulator at `ratio`, its brightness
 * falling as it rings. */
void bell(Buf &b, float at, float f, float ratio, float index, float length, float gain)
{
    const std::size_t from = std::size_t(at * kRate), n = std::size_t(length * kRate);
    for (std::size_t i = 0; i < n && from + i < b.s.size(); ++i)
    {
        const float t = float(i) / kRate;
        const float e = env(t, 0.002f, length);
        const float mod = index * e * std::sin(2 * kPi * f * ratio * t);
        b.s[from + i] += gain * e * std::sin(2 * kPi * f * t + mod);
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
    case Chiptune:
    {
        Buf row(0.06f), scroll(0.05f), tab(0.14f), flip(0.2f), launch(0.5f);
        tone(row, Pulse, 0, note(15), note(15), 0.05f, 0, 1);         /* C6 */
        tone(scroll, Pulse, 0, note(22), note(22), 0.035f, 0, 1);     /* G6 */
        tone(tab, Square, 0, note(10), note(10), 0.06f, 0, 1);        /* G5 */
        tone(tab, Square, 0.06f, note(15), note(15), 0.07f, 0, 1);    /* C6 */
        static const int kUp[4] = {3, 7, 10, 15}; /* C5 E5 G5 C6 */
        for (int k = 0; k < 4; ++k)
            tone(flip, Pulse, 0.04f * float(k), note(kUp[k]), note(kUp[k]), 0.06f, 0, 0.9f);
        tone(launch, Square, 0, note(14), note(14), 0.08f, 0, 1);     /* B5 */
        tone(launch, Square, 0.08f, note(19), note(19), 0.4f, 0, 1);  /* E6, the coin */
        out[0] = stereo(row, 0.3f);
        out[1] = stereo(scroll, 0.22f);
        out[2] = stereo(tab, 0.3f);
        out[3] = stereo(flip, 0.3f);
        out[4] = stereo(launch, 0.34f);
        break;
    }
    case Glass:
    {
        Buf row(0.25f), scroll(0.18f), tab(0.4f), flip(0.6f), launch(1.2f);
        bell(row, 0, 1320, 3.5f, 1.2f, 0.24f, 1);
        bell(scroll, 0, 1760, 3.5f, 0.8f, 0.17f, 1);
        bell(tab, 0, 1047, 2.0f, 1.5f, 0.35f, 1);
        bell(tab, 0.06f, 1568, 2.0f, 1.5f, 0.33f, 0.8f);
        bell(flip, 0, 784, 3.5f, 2.0f, 0.55f, 1);
        bell(flip, 0.05f, 1175, 3.5f, 1.5f, 0.5f, 0.6f);
        bell(launch, 0, 1047, 3.5f, 2.0f, 1.1f, 0.8f);
        bell(launch, 0.08f, 1319, 3.5f, 2.0f, 1.0f, 0.7f);
        bell(launch, 0.16f, 1568, 3.5f, 2.0f, 1.0f, 0.7f);
        bell(launch, 0.24f, 2093, 3.5f, 1.5f, 0.95f, 0.8f);
        out[0] = stereo(row, 0.36f);
        out[1] = stereo(scroll, 0.28f);
        out[2] = stereo(tab, 0.4f);
        out[3] = stereo(flip, 0.42f);
        out[4] = stereo(launch, 0.48f);
        break;
    }
    case RetroPC:
    {
        Buf row(0.05f), scroll(0.04f), tab(0.12f), flip(0.22f), launch(0.4f);
        /* A key: a click on top, a thump under it. */
        noise(row, 0, 0.006f, 0, 2500, true, 0.9f, 7);
        tone(row, Sine, 0, 180, 120, 0.04f, 0.001f, 0.6f);
        noise(scroll, 0, 0.004f, 0, 3000, true, 0.8f, 8);
        tone(scroll, Sine, 0, 220, 160, 0.03f, 0.001f, 0.4f);
        /* A relay: two clacks. */
        noise(tab, 0, 0.008f, 0, 1500, true, 1, 9);
        noise(tab, 0.05f, 0.01f, 0, 1200, true, 0.8f, 10);
        tone(tab, Square, 0, 90, 90, 0.06f, 0.001f, 0.2f);
        /* A drive seeking. */
        for (int k = 0; k < 6; ++k)
            noise(flip, 0.03f * float(k), 0.012f, 0, 900, false, 0.9f, 11u + unsigned(k));
        tone(flip, Square, 0, 60, 60, 0.2f, 0.005f, 0.15f);
        /* The PC speaker's beep. */
        tone(launch, Square, 0, 1000, 1000, 0.18f, 0, 0.6f);
        tone(launch, Square, 0.2f, 1500, 1500, 0.16f, 0, 0.6f);
        out[0] = stereo(row, 0.42f);
        out[1] = stereo(scroll, 0.32f);
        out[2] = stereo(tab, 0.42f);
        out[3] = stereo(flip, 0.4f);
        out[4] = stereo(launch, 0.3f);
        break;
    }
    default:
        break;
    }
}

namespace
{
/* One of the BIOS's sounds at 48 kHz stereo: `rate` changes its pitch (1 its
 * own), from `from` seconds for at most `seconds`, faded out at the end. */
std::vector<std::int16_t> resample(const bios::Sound &s, float rate, float from, float seconds, float gain)
{
    std::vector<std::int16_t> out;
    if (s.samples.empty())
        return out;
    const double step = double(s.rate) * rate / 48000.0;
    const std::size_t n = std::size_t(seconds * 48000.0f);
    const double start = double(from) * s.rate;
    out.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double at = start + double(i) * step;
        const std::size_t k = std::size_t(at);
        if (k + 1 >= s.samples.size())
            break;
        const float frac = float(at - double(k));
        float v = (float(s.samples[k]) * (1 - frac) + float(s.samples[k + 1]) * frac) * gain;
        const float left = float(n - i) / 48000.0f;
        if (left < 0.03f)
            v *= left / 0.03f;
        const std::int16_t x = std::int16_t(std::clamp(v, -32767.0f, 32767.0f));
        out.push_back(x);
        out.push_back(x);
    }
    return out;
}
} // namespace

bool from_bios(const std::string &bios_path, std::vector<std::int16_t> out[5])
{
    const std::vector<bios::Sound> s = bios::read_sounds(bios_path);
    if (s.size() < 12)
        return false;
    /* The menu's own short sounds: 6 a soft tone, 5 a bright tap, 4 a long
     * shimmer, 11 a rounder tone. The menu plays them through its own note
     * sequences; these are the sounds as they are, a little higher for the
     * quickest steps. */
    out[0] = resample(s[6], 1.0f, 0, 0.18f, 0.55f);  /* GameRow */
    out[1] = resample(s[6], 1.5f, 0, 0.12f, 0.45f);  /* MenuScroll */
    out[2] = resample(s[5], 1.0f, 0.05f, 0.35f, 0.6f); /* MovingTab */
    out[3] = resample(s[4], 1.0f, 0.1f, 0.6f, 0.55f);  /* DetailsFlip */
    out[4] = resample(s[11], 1.0f, 0, 0.9f, 0.6f);   /* LaunchGame */
    for (int i = 0; i < 5; ++i)
        if (out[i].empty())
            return false;
    return true;
}
} // namespace porpoise::sfx
