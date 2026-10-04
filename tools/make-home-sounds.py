#!/usr/bin/env python3
"""Porpoise - the Revolution look's two sounds, made from nothing but maths.

home-hover.wav  a soft, glassy tick when the pointer (or the D-pad) lands on a tile
home-page.wav   a short airy sweep when the home screen turns a page

48 kHz stereo 16-bit, like the other effects. Run from the repository root:
    python3 tools/make-home-sounds.py
Copyright (C) 2026 Ruben (Project Porpoise)
SPDX-License-Identifier: GPL-3.0-or-later
"""
import math
import random
import struct
import wave

RATE = 48000


def write(path, left, right):
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = bytearray()
        for a, b in zip(left, right):
            frames += struct.pack("<hh", int(max(-1, min(1, a)) * 32767), int(max(-1, min(1, b)) * 32767))
        w.writeframes(bytes(frames))


def hover():
    n = int(RATE * 0.14)
    out = []
    for i in range(n):
        t = i / RATE
        attack = min(1.0, t / 0.002)
        body = math.exp(-t / 0.028)
        ring = math.exp(-t / 0.060)
        s = (0.55 * math.sin(2 * math.pi * 1760 * t) * body
             + 0.22 * math.sin(2 * math.pi * 2640 * t) * body
             + 0.12 * math.sin(2 * math.pi * 3520 * t) * ring)
        out.append(0.32 * attack * s)
    return out, out


def page():
    n = int(RATE * 0.30)
    rnd = random.Random(2006)
    lp_l = lp_r = 0.0
    bp_l = bp_r = 0.0
    left, right = [], []
    for i in range(n):
        t = i / RATE
        x = t / 0.30
        env = math.sin(math.pi * x) ** 2
        # a one-pole low-pass whose corner rises: the sweep
        cut = 400 + 2600 * x
        k = 1 - math.exp(-2 * math.pi * cut / RATE)
        nl, nr = rnd.uniform(-1, 1), rnd.uniform(-1, 1)
        lp_l += k * (nl - lp_l)
        lp_r += k * (nr - lp_r)
        # take away the lowest rumble
        bp_l += 0.02 * (lp_l - bp_l)
        bp_r += 0.02 * (lp_r - bp_r)
        tone = 0.10 * math.sin(2 * math.pi * (660 + 440 * x) * t) * math.exp(-t / 0.12)
        pan = x  # left to right as the page goes by
        left.append(0.55 * env * (lp_l - bp_l) * (1.2 - pan) + tone)
        right.append(0.55 * env * (lp_r - bp_r) * (0.2 + pan) + tone)
    return left, right


if __name__ == "__main__":
    write("assets/sounds/home-hover.wav", *hover())
    write("assets/sounds/home-page.wav", *page())
    print("wrote assets/sounds/home-hover.wav and assets/sounds/home-page.wav")
