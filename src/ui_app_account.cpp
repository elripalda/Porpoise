/* Porpoise UI - the RetroAchievements account panel: signing in with the
 * controller (a keyboard of its own), the account once signed in, signing out.
 * L1 + Square in the library opens it, as does Settings > Games.
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
constexpr int kKeyCols = 10, kKeyRows = 5;
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

void App::open_account()
{
    if (!ra_state_)
        return;
    acct_ = AccountPanel{};
    acct_.open = true;
    const RaState s = ra_state_();
    acct_.user = s.signed_in ? std::string() : s.user;
    acct_.was_busy = s.busy;
    sfx(Sound::DetailsFlip);
}

void App::account_key(int kr, int kc)
{
    std::string &text = acct_.field == 0 ? acct_.user : acct_.pass;
    const std::size_t limit = acct_.field == 0 ? 32 : 128;
    if (kr == 4)
    {
        switch (wide_key(kc))
        {
        case KeyShift:
            acct_.shift = !acct_.shift;
            break;
        case KeySymbols:
            acct_.symbols = !acct_.symbols;
            break;
        case KeySpace:
            if (text.size() < limit && acct_.field == 1)
                text += ' ';
            break;
        case KeyDelete:
            if (!text.empty())
                text.pop_back();
            break;
        case KeyDone:
            acct_.typing = false;
            acct_.row = acct_.field == 0 && acct_.pass.empty() ? 1 : 2;
            break;
        }
        sfx(Sound::MenuScroll);
        return;
    }
    const char ch = key_char(kr, kc, acct_.shift, acct_.symbols);
    if (ch && text.size() < limit)
    {
        text += ch;
        sfx(Sound::MenuScroll);
    }
}

App::Action App::update_account(bool up, bool down, bool left, bool right)
{
    const RaState s = ra_state_ ? ra_state_() : RaState{};
    if (acct_.was_busy && !s.busy)
    {
        /* The sign-in has answered. */
        sfx(s.signed_in ? Sound::LaunchGame : Sound::MovingTab);
        if (s.signed_in)
        {
            acct_.pass.assign(acct_.pass.size(), '\0');
            acct_.pass.clear();
            acct_.row = 0;
        }
    }
    acct_.was_busy = s.busy;

    if (acct_.typing)
    {
        if (up)
            acct_.kr = (acct_.kr + kKeyRows - 1) % kKeyRows;
        if (down)
            acct_.kr = (acct_.kr + 1) % kKeyRows;
        if (acct_.kr == 4 && (left || right))
        {
            /* Across the wide keys, a key at a time. */
            const WideKey k = wide_key(acct_.kc);
            const int next = left ? (k == KeyShift ? int(KeyDone) : int(k) - 1) : (k == KeyDone ? 0 : int(k) + 1);
            acct_.kc = int(wide_first(WideKey(next)));
        }
        else
        {
            if (left)
                acct_.kc = (acct_.kc + kKeyCols - 1) % kKeyCols;
            if (right)
                acct_.kc = (acct_.kc + 1) % kKeyCols;
        }
        if (acct_.kr < 4)
        {
            /* A shorter row (the symbols' last, "@-"): only its own keys. */
            const int len = int(std::strlen(acct_.symbols ? kSymbols[acct_.kr] : kLetters[acct_.kr]));
            if (len > 0 && acct_.kc >= len)
                acct_.kc = right ? 0 : len - 1;
        }
        if (up || down || left || right)
            sfx(Sound::MenuScroll);
        if (pressed(BtnCross))
            account_key(acct_.kr, acct_.kc);
        if (pressed(BtnSquare))
            account_key(4, int(wide_first(KeyDelete)));
        if (pressed(BtnTriangle))
            account_key(4, int(wide_first(KeySpace)));
        if (pressed(BtnL2))
            account_key(4, int(wide_first(KeyShift)));
        if (pressed(BtnR2))
            account_key(4, int(wide_first(KeySymbols)));
        if (pressed(BtnOptions | BtnCircle))
            account_key(4, int(wide_first(KeyDone)));
        return Action::None;
    }

    if (pressed(BtnCircle))
    {
        acct_.pass.assign(acct_.pass.size(), '\0');
        acct_ = AccountPanel{};
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    if (s.busy)
        return Action::None;
    if (s.signed_in)
    {
        if (pressed(BtnCross) && ra_logout_)
        {
            ra_logout_();
            acct_.row = 0;
            sfx(Sound::MovingTab);
            build_settings(); /* its row says who is signed in */
        }
        return Action::None;
    }
    if (up && acct_.row > 0)
    {
        --acct_.row;
        sfx(Sound::MenuScroll);
    }
    if (down && acct_.row < 2)
    {
        ++acct_.row;
        sfx(Sound::MenuScroll);
    }
    if (pressed(BtnCross))
    {
        if (acct_.row == 2 && !acct_.user.empty() && !acct_.pass.empty())
        {
            if (ra_login_ && ra_login_(acct_.user, acct_.pass))
            {
                acct_.was_busy = true;
                sfx(Sound::LaunchGame);
            }
        }
        else
        {
            /* A field, or Sign in with one still empty: type into it. */
            acct_.field = acct_.row == 2 ? (acct_.user.empty() ? 0 : 1) : acct_.row;
            acct_.row = acct_.field;
            acct_.typing = true;
            acct_.kr = 1;
            acct_.kc = 0;
            acct_.shift = false;
            acct_.symbols = false;
            sfx(Sound::DetailsFlip);
        }
    }
    return Action::None;
}

void App::draw_account()
{
    if (!acct_.open)
        return;
    Gfx &g = *g_;
    acct_.anim = settings_ && settings_->reduced_motion ? 1.0f : std::min(1.0f, acct_.anim + 1.0f / 10.0f);
    const float t = ease_out(acct_.anim);
    const RaState s = ra_state_ ? ra_state_() : RaState{};
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.62f * t), 1, 0);
    g.set_layer(0, 24.0f * (1.0f - t), t);

    const float w = 1100, x = 960 - w * 0.5f, pad = 56;
    const float text_w = w - pad * 2;
    const bool signed_in = s.signed_in;
    std::vector<std::string> about;
    if (acct_.typing)
        ; /* room for the keyboard */
    else if (signed_in)
        about = wrap(g, Font::Regular, ts(27),
                     tr("Unlocks pop up like PS5 trophies, with the badge and the trophy sound. That needs your "
                        "jailbreak's ELF loader (port 9021); without it they're plain notifications. Softcore only: "
                        "hardcore mode isn't available."),
                     text_w, 5);
    else
        about = wrap(g, Font::Regular, ts(27),
                     tr("Earn achievements as you play, in softcore. Unlocks pop up like PS5 trophies. No account? "
                        "Make one for free at retroachievements.org."),
                     text_w, 4);
    const std::string note_text = s.busy ? tr("Signing in\xE2\x80\xA6") : tr(s.message);
    const auto note = note_text.empty() ? std::vector<std::string>{} : wrap(g, Font::SemiBold, ts(26), note_text, text_w, 2);
    const float rows_h = signed_in ? 118 : 2 * 88 + 30;
    const float h = 132 + float(about.size()) * 36 + (acct_.typing ? 0.0f : 26.0f) + rows_h + 96 + float(note.size()) * 34 + 30;
    constexpr float kw = 92, kh = 70, gap = 10;
    const float keys_h = kKeyRows * kh + (kKeyRows - 1) * gap;
    const float y = acct_.typing ? 34.0f : 540 - h * 0.5f - 20;

    Glass face;
    face.tint = rgba(0x16348F, 0.92f);
    face.rim = rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.4f;
    face.glow = 14;
    face.phase = 0.4f;
    glass_block(g, 960, y + h * 0.5f, w, h, 18, 0, 0, 42, face);

    float ly = y + 66;
    const float title_w = g.text_mid(Font::Bold, ts(40), x + pad, ly, kWhite, Align::Left, "RetroAchievements");
    {
        const std::string tag = tr("Softcore");
        const float tw = g.measure(Font::Bold, ts(22), tag) + 28;
        g.panel(x + pad + title_w + 20, ly - 19, tw, 38, rgba(0x07102E, 0.55f), 1, 19, rgba(0x9DEBFF, 0.8f), 1.4f);
        g.text_mid(Font::Bold, ts(22), x + pad + title_w + 20 + tw * 0.5f, ly, kIcy, Align::Center, tag);
    }
    ly += 66;
    for (const std::string &l : about)
    {
        g.text_mid(Font::Regular, ts(27), x + pad, ly, kSoft, Align::Left, l);
        ly += 36;
    }
    ly += acct_.typing ? 0.0f : 26.0f;

    auto button = [&](float bx, float by, float bw, const std::string &label, bool on, bool danger, bool dim) {
        const float bh = 62;
        g.panel(bx, by - bh * 0.5f, bw, bh,
                on ? (danger ? rgba(0xB0305A, 0.92f) : rgba(0x1F63F0, 0.92f)) : rgba(0x07102E, dim ? 0.3f : 0.5f), 0.7f,
                kR, on ? (danger ? kDanger : kIcy) : rgba(0x3D5AB0, 0.8f), on ? 2.2f : 1.4f, on ? 8 : 0,
                on ? 0.25f : 0.0f);
        g.text_mid(Font::Bold, ts(28), bx + bw * 0.5f, by, on ? kWhite : (dim ? rgba(0x8A96C8) : kSoft), Align::Center,
                   title_case(label));
    };

    if (signed_in)
    {
        g.text_mid(Font::Bold, ts(34), x + pad, ly + 8, kWhite, Align::Left,
                   fit(g, Font::Bold, ts(34), trf("Signed in as {user}", {{"user", s.user}}), text_w));
        g.text_mid(Font::SemiBold, ts(27), x + pad, ly + 60, kIcy, Align::Left,
                   plural((long long)s.points, "1 point", "{n} points"));
        ly += rows_h;
        button(x + w - pad - 260, ly + 30, 260, tr("Sign out"), true, true, false);
    }
    else
    {
        auto field = [&](int row, const std::string &label, const std::string &text, bool secret) {
            const bool focus = acct_.row == row && !s.busy;
            const bool typing_here = acct_.typing && acct_.field == row;
            g.text_mid(Font::SemiBold, ts(28), x + pad, ly + 36, focus ? kWhite : kSoft, Align::Left, label);
            const float fx = x + pad + 280, fw = w - pad * 2 - 280, fh = 64;
            g.panel(fx, ly + 4, fw, fh, rgba(0x07102E, typing_here ? 0.85f : 0.6f), 1, kR,
                    focus || typing_here ? kIcy : rgba(0x3D5AB0, 0.8f), focus || typing_here ? 2.2f : 1.4f,
                    typing_here ? 8 : 0, typing_here ? 0.2f : 0.0f);
            std::string shown = secret ? std::string() : text;
            if (secret)
                for (std::size_t i = 0; i < text.size(); ++i)
                    shown += "\xE2\x80\xA2"; /* a dot each */
            float tx = fx + 22;
            if (!shown.empty())
                tx += g.text_mid(Font::SemiBold, ts(28), tx, ly + 36, kWhite, Align::Left,
                                 fit(g, Font::SemiBold, ts(28), shown, fw - 60));
            else if (!typing_here)
                g.text_mid(Font::Regular, ts(26), tx, ly + 36, rgba(0x8A96C8), Align::Left,
                           tr(row == 0 ? "Your RetroAchievements username" : "Your password"));
            if (typing_here && std::fmod(time_, 1.0) < 0.6)
                g.panel(tx + 3, ly + 18, 3, 36, kIcy, 1, 0);
        };
        field(0, tr("Username"), acct_.user, false);
        ly += 88;
        field(1, tr("Password"), acct_.pass, true);
        ly += 88 + 30;
        const bool ready = !acct_.user.empty() && !acct_.pass.empty();
        button(x + w - pad - 260, ly + 30, 260, tr("Sign in"), acct_.row == 2 && !acct_.typing && !s.busy, false,
               !ready || s.busy);
        g.text_mid(Font::Regular, ts(24), x + pad, ly + 30, rgba(0x8A96C8), Align::Left,
                   fit(g, Font::Regular, ts(24), tr("Porpoise keeps a sign-in token, never your password."),
                       w - pad * 2 - 300));
    }
    ly += 96;
    for (const std::string &l : note)
    {
        g.text_mid(Font::SemiBold, ts(26), x + pad, ly, s.busy ? kIcy : kDanger, Align::Left, l);
        ly += 34;
    }

    if (acct_.typing)
    {
        /* The keyboard, under the panel. */
        const float kx = 960 - (kKeyCols * kw + (kKeyCols - 1) * gap) * 0.5f;
        const float ky = y + h + 44;
        Glass board = face;
        board.tint = rgba(0x0F2770, 0.94f);
        const float bw = kKeyCols * kw + (kKeyCols - 1) * gap + 48;
        glass_block(g, 960, ky + keys_h * 0.5f, bw, keys_h + 44, 14, 0, 0, 30, board);
        for (int r = 0; r < kKeyRows; ++r)
        {
            const float ry = ky + float(r) * (kh + gap);
            if (r < 4)
                for (int c = 0; c < kKeyCols; ++c)
                {
                    const char ch = key_char(r, c, acct_.shift, acct_.symbols);
                    if (!ch)
                        continue;
                    const bool on = acct_.kr == r && acct_.kc == c;
                    const float cx = kx + float(c) * (kw + gap);
                    g.panel(cx, ry, kw, kh, on ? rgba(0x1F63F0, 0.95f) : rgba(0x07102E, 0.6f), 1, kR,
                            on ? kIcy : rgba(0x3D5AB0, 0.7f), on ? 2.2f : 1.2f, on ? 8 : 0, on ? 0.25f : 0.0f);
                    g.text_mid(Font::Bold, ts(32), cx + kw * 0.5f, ry + kh * 0.5f, kWhite, Align::Center,
                               std::string(1, ch));
                }
            else
                for (int k = KeyShift; k <= KeyDone; ++k)
                {
                    const WideKey wk = WideKey(k);
                    const bool on = acct_.kr == 4 && wide_key(acct_.kc) == wk;
                    const bool lit = (wk == KeyShift && acct_.shift) || (wk == KeySymbols && acct_.symbols);
                    const float cx = kx + wide_first(wk) * (kw + gap);
                    const float cw = wide_span(wk) * kw + (wide_span(wk) - 1) * gap;
                    g.panel(cx, ry, cw, kh,
                            on ? rgba(0x1F63F0, 0.95f) : lit ? rgba(0x2A4FB8, 0.8f) : rgba(0x07102E, 0.6f), 1, kR,
                            on || lit ? kIcy : rgba(0x3D5AB0, 0.7f), on ? 2.2f : 1.2f, on ? 8 : 0, on ? 0.25f : 0.0f);
                    const char *label = wk == KeyShift     ? "Shift"
                                         : wk == KeySymbols ? (acct_.symbols ? "abc" : "#+=")
                                         : wk == KeySpace   ? "Space"
                                         : wk == KeyDelete  ? "Delete"
                                                            : "OK";
                    g.text_mid(Font::Bold, ts(26), cx + cw * 0.5f, ry + kh * 0.5f, kWhite, Align::Center,
                               fit(g, Font::Bold, ts(26), tr(label), cw - 16));
                }
        }
    }
    g.set_layer();
    if (dialog_.open)
        return; /* the dialog over it brings its own prompts */
    drawing_dialog_ = true;
    if (acct_.typing)
        draw_prompts({{Glyph::Cross, "Type"}, {Glyph::Square, "Delete"}, {Glyph::Triangle, "Space"},
                      {Glyph::L2, "Capitals"}, {Glyph::R2, "Symbols"}},
                     {{Glyph::Options, "Done"}}, "");
    else if (s.busy)
        draw_prompts({}, {{Glyph::Circle, "Close"}}, "");
    else if (signed_in)
        draw_prompts({{Glyph::Cross, "Sign out"}}, {{Glyph::Circle, "Close"}}, "");
    else
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Select"}}, {{Glyph::Circle, "Close"}}, "");
    drawing_dialog_ = false;
}
} // namespace porpoise::ui
