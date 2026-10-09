/* Porpoise UI - the on-screen keyboard: typed with the controller, for the
 * RetroAchievements sign-in and a network share's details.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_pad.hpp"
#include "ui_app.hpp"

#include <cstring>

#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

namespace
{
constexpr int kKeyCols = 10;
constexpr float kKeyW = 92, kKeyH = 70, kKeyGap = 10;
/* Letters (and Shift for capitals) or symbols; the last row is the wide keys. */
const char *const kLetters[4] = {"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm_.@"};
const char *const kSymbols[4] = {"!#$%&*()+=", "~`^'\":;,?/", "<>[]{}|\\_.", "@-"};
enum WideKey
{
    KeyShift,
    KeySymbols,
    KeySpace,
    KeyDelete,
    KeyDone,
};
/* The wide keys under the columns they cover. */
WideKey wide_key(int col)
{
    return col < 2 ? KeyShift : col < 4 ? KeySymbols : col < 7 ? KeySpace : col < 9 ? KeyDelete : KeyDone;
}
float wide_first(WideKey k)
{
    return k == KeyShift ? 0 : k == KeySymbols ? 2 : k == KeySpace ? 4 : k == KeyDelete ? 7 : 9;
}
float wide_span(WideKey k)
{
    return k == KeyShift || k == KeySymbols || k == KeyDelete ? 2 : k == KeySpace ? 3 : 1;
}
/* The character a key types, or 0. */
char key_char(int r, int c, bool shift, bool symbols)
{
    if (r < 0 || r >= 4 || c < 0 || c >= kKeyCols)
        return 0;
    const char *row = symbols ? kSymbols[r] : kLetters[r];
    if (c >= int(std::strlen(row)))
        return 0;
    char ch = row[c];
    if (shift && !symbols && ch >= 'a' && ch <= 'z')
        ch = char(ch - 'a' + 'A');
    return ch;
}
} // namespace

float App::keyboard_height()
{
    return kKeyboardRows * kKeyH + (kKeyboardRows - 1) * kKeyGap;
}

bool App::keyboard_update(Keyboard &kb, std::string &text, std::size_t limit, bool spaces, bool up, bool down,
                          bool left, bool right)
{
    if (up)
        kb.kr = (kb.kr + kKeyboardRows - 1) % kKeyboardRows;
    if (down)
        kb.kr = (kb.kr + 1) % kKeyboardRows;
    if (kb.kr == 4 && (left || right))
    {
        /* Across the wide keys, a key at a time. */
        const WideKey k = wide_key(kb.kc);
        const int next = left ? (k == KeyShift ? int(KeyDone) : int(k) - 1) : (k == KeyDone ? 0 : int(k) + 1);
        kb.kc = int(wide_first(WideKey(next)));
    }
    else
    {
        if (left)
            kb.kc = (kb.kc + kKeyCols - 1) % kKeyCols;
        if (right)
            kb.kc = (kb.kc + 1) % kKeyCols;
    }
    if (kb.kr < 4)
    {
        /* A shorter row (the symbols' last, "@-"): only its own keys. */
        const int len = int(std::strlen(kb.symbols ? kSymbols[kb.kr] : kLetters[kb.kr]));
        if (len > 0 && kb.kc >= len)
            kb.kc = right ? 0 : len - 1;
    }
    if (up || down || left || right)
        sfx(Sound::MenuScroll);

    auto press = [&](int kr, int kc) {
        if (kr == 4)
        {
            sfx(Sound::MenuScroll);
            switch (wide_key(kc))
            {
            case KeyShift:
                kb.shift = !kb.shift;
                return false;
            case KeySymbols:
                kb.symbols = !kb.symbols;
                return false;
            case KeySpace:
                if (spaces && text.size() < limit)
                    text += ' ';
                return false;
            case KeyDelete:
                if (!text.empty())
                    text.pop_back();
                return false;
            case KeyDone:
                return true;
            }
            return false;
        }
        const char ch = key_char(kr, kc, kb.shift, kb.symbols);
        if (ch && text.size() < limit)
        {
            text += ch;
            sfx(Sound::MenuScroll);
        }
        return false;
    };
    bool done = false;
    if (pressed(BtnCross))
        done |= press(kb.kr, kb.kc);
    if (pressed(BtnSquare))
        done |= press(4, int(wide_first(KeyDelete)));
    if (pressed(BtnTriangle))
        done |= press(4, int(wide_first(KeySpace)));
    if (pressed(BtnL2))
        done |= press(4, int(wide_first(KeyShift)));
    if (pressed(BtnR2))
        done |= press(4, int(wide_first(KeySymbols)));
    if (pressed(BtnOptions | BtnCircle))
        done |= press(4, int(wide_first(KeyDone)));
    return done;
}

void App::draw_keyboard(const Keyboard &kb, float y, const Glass &face)
{
    Gfx &g = *g_;
    const float kx = 960 - (kKeyCols * kKeyW + (kKeyCols - 1) * kKeyGap) * 0.5f;
    const float keys_h = keyboard_height();
    Glass board = face;
    board.tint = rgba(0x0F2770, 0.94f);
    const float bw = kKeyCols * kKeyW + (kKeyCols - 1) * kKeyGap + 48;
    glass_block(g, 960, y + keys_h * 0.5f, bw, keys_h + 44, 14, 0, 0, 30, board);
    for (int r = 0; r < kKeyboardRows; ++r)
    {
        const float ry = y + float(r) * (kKeyH + kKeyGap);
        if (r < 4)
            for (int c = 0; c < kKeyCols; ++c)
            {
                const char ch = key_char(r, c, kb.shift, kb.symbols);
                if (!ch)
                    continue;
                const bool on = kb.kr == r && kb.kc == c;
                const float cx = kx + float(c) * (kKeyW + kKeyGap);
                g.panel(cx, ry, kKeyW, kKeyH, on ? rgba(0x1F63F0, 0.95f) : rgba(0x07102E, 0.6f), 1, kR,
                        on ? kIcy : rgba(0x3D5AB0, 0.7f), on ? 2.2f : 1.2f, on ? 8 : 0, on ? 0.25f : 0.0f);
                g.text_mid(Font::Bold, ts(32), cx + kKeyW * 0.5f, ry + kKeyH * 0.5f, kWhite, Align::Center,
                           std::string(1, ch));
            }
        else
            for (int k = KeyShift; k <= KeyDone; ++k)
            {
                const WideKey wk = WideKey(k);
                const bool on = kb.kr == 4 && wide_key(kb.kc) == wk;
                const bool lit = (wk == KeyShift && kb.shift) || (wk == KeySymbols && kb.symbols);
                const float cx = kx + wide_first(wk) * (kKeyW + kKeyGap);
                const float cw = wide_span(wk) * kKeyW + (wide_span(wk) - 1) * kKeyGap;
                g.panel(cx, ry, cw, kKeyH,
                        on ? rgba(0x1F63F0, 0.95f) : lit ? rgba(0x2A4FB8, 0.8f) : rgba(0x07102E, 0.6f), 1, kR,
                        on || lit ? kIcy : rgba(0x3D5AB0, 0.7f), on ? 2.2f : 1.2f, on ? 8 : 0, on ? 0.25f : 0.0f);
                const char *label = wk == KeyShift     ? "Shift"
                                     : wk == KeySymbols ? (kb.symbols ? "abc" : "#+=")
                                     : wk == KeySpace   ? "Space"
                                     : wk == KeyDelete  ? "Delete"
                                                        : "OK";
                g.text_mid(Font::Bold, ts(26), cx + cw * 0.5f, ry + kKeyH * 0.5f, kWhite, Align::Center,
                           fit(g, Font::Bold, ts(26), tr(label), cw - 16));
            }
    }
}

void App::draw_keyboard_prompts()
{
    draw_prompts({{Glyph::Cross, "Type"}, {Glyph::Square, "Delete"}, {Glyph::Triangle, "Space"},
                  {Glyph::L2, "Capitals"}, {Glyph::R2, "Symbols"}},
                 {{Glyph::Options, "Done"}}, "");
}
} // namespace porpoise::ui
