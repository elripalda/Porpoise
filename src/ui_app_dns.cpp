/* Porpoise UI - the DNS server for Wii games online: an address typed on the
 * number pad, as a Wii's own network settings take one for a custom server,
 * or empty for the console's own. Settings > System opens it; it applies at
 * once (porpoise_dns for the next lookup).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_pad.hpp"
#include "ui_app.hpp"

#include "ui_app_common.hpp"
#include "ui_i18n.hpp"
#ifndef PORPOISE_DESKTOP
#include "porpoise_dns.hpp"
#endif

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

namespace
{
/* Four numbers 0-255 with dots between: an IPv4 address. */
bool plain_address(const std::string &text)
{
    int parts = 0, value = -1;
    for (char c : text)
    {
        if (c >= '0' && c <= '9')
        {
            value = (value < 0 ? 0 : value) * 10 + (c - '0');
            if (value > 255)
                return false;
        }
        else if (c == '.' && value >= 0 && parts < 3)
        {
            ++parts;
            value = -1;
        }
        else
            return false;
    }
    return parts == 3 && value >= 0;
}
} // namespace

void App::open_dns()
{
    dns_ = DnsPanel{};
    dns_.open = true;
    dns_.text = settings_ ? settings_->online_dns : std::string();
    dns_.kb.numeric = true;
    dns_.kb.kr = 0;
    dns_.kb.kc = 0;
    sfx(Sound::DetailsFlip);
}

App::Action App::update_dns(bool up, bool down, bool left, bool right)
{
    DnsPanel &p = dns_;
    if (pressed(BtnCircle))
    {
        /* Cancel: the address stays as it was. */
        dns_ = DnsPanel{};
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    if (!keyboard_update(p.kb, p.text, 15, false, up, down, left, right))
        return Action::None;
    /* Done: an address, or nothing for the console's own. */
    std::string text = p.text;
    while (!text.empty() && text.back() == '.')
        text.pop_back();
    if (!text.empty() && !plain_address(text))
    {
        p.message = tr("That isn't an address. Type one like 192.168.1.2, or delete it all for Automatic.");
        p.kb.numeric = true;
        sfx(Sound::MovingTab);
        return Action::None;
    }
    if (settings_)
    {
        settings_->online_dns = text;
        draft_.online_dns = text;
        base_.online_dns = text;
        settings_->save(settings_path_);
    }
#ifndef PORPOISE_DESKTOP
    porpoise::dns::set_server(text);
#endif
    dns_ = DnsPanel{};
    sfx(Sound::LaunchGame);
    build_settings();
    return Action::None;
}

void App::draw_dns()
{
    if (!dns_.open)
        return;
    DnsPanel &p = dns_;
    Gfx &g = *g_;
    p.anim = settings_ && settings_->reduced_motion ? 1.0f : std::min(1.0f, p.anim + 1.0f / 10.0f);
    const float t = ease_out(p.anim);
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.62f * t), 1, 0);
    g.set_layer(0, 24.0f * (1.0f - t), t);

    const float w = 1180, x = 960 - w * 0.5f, pad = 56, text_w = w - pad * 2;
    const auto about = wrap(g, Font::Regular, ts(26),
                            tr("For a custom server for Wii games online: the DNS address its instructions give, as "
                               "on a Wii's own network settings. Empty: Automatic, the console's own."),
                            text_w, 3);
    const auto note = p.message.empty() ? std::vector<std::string>{}
                                        : wrap(g, Font::SemiBold, ts(26), p.message, text_w, 2);
    const float h = 124 + float(about.size()) * 34 + 20 + 80 + float(note.size()) * 34 + 30;
    const float y = 34;

    Glass face;
    face.tint = rgba(0x16348F, 0.92f);
    face.rim = rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.4f;
    face.glow = 14;
    face.phase = 0.4f;
    glass_block(g, 960, y + h * 0.5f, w, h, 18, 0, 0, 42, face);

    float ly = y + 66;
    g.text_mid(Font::Bold, ts(40), x + pad, ly, kWhite, Align::Left,
               fit(g, Font::Bold, ts(40), tr("DNS Server for Online Play"), text_w));
    ly += 62;
    for (const std::string &l : about)
    {
        g.text_mid(Font::Regular, ts(26), x + pad, ly, kSoft, Align::Left, l);
        ly += 34;
    }
    ly += 20;
    const float fh = 64;
    g.panel(x + pad, ly + 4, text_w, fh, rgba(0x07102E, 0.85f), 1, kR, kIcy, 2.2f, 8, 0.2f);
    float tx = x + pad + 22;
    if (!p.text.empty())
        tx += g.text_mid(Font::SemiBold, ts(30), tx, ly + 36, kWhite, Align::Left, p.text);
    else
        g.text_mid(Font::Regular, ts(26), tx + 6, ly + 36, rgba(0x8A96C8), Align::Left,
                   fit(g, Font::Regular, ts(26), tr("Automatic"), text_w - 60));
    if (std::fmod(time_, 1.0) < 0.6)
        g.panel(tx + 3, ly + 18, 3, 36, kIcy, 1, 0);
    ly += 80;
    for (const std::string &l : note)
    {
        g.text_mid(Font::SemiBold, ts(26), x + pad, ly + 10, kDanger, Align::Left, l);
        ly += 34;
    }

    draw_keyboard(p.kb, y + h + 44, face);
    g.set_layer();
    if (dialog_.open)
        return;
    drawing_dialog_ = true;
    if (p.kb.numeric)
        draw_prompts({{Glyph::Cross, "Type"}, {Glyph::Square, "Delete"}, {Glyph::Triangle, "Letters"}},
                     {{Glyph::Circle, "Cancel"}, {Glyph::Options, "Done"}}, "");
    else
        draw_prompts({{Glyph::Cross, "Type"}, {Glyph::Square, "Delete"}, {Glyph::Triangle, "Space"}},
                     {{Glyph::Circle, "Cancel"}, {Glyph::Options, "Done"}}, "");
    drawing_dialog_ = false;
}
} // namespace porpoise::ui
