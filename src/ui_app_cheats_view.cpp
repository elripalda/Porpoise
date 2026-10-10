/* Porpoise UI - a game's Cheats and Patches as cards: what each code is (a
 * patch, a Gecko or Action Replay cheat, the widescreen collection's, your
 * own), its name and author, and a big switch. The rows are Settings' own
 * (add_cheat_rows): the same focus, Cross and Apply; only the look differs.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_app.hpp"

#include <algorithm>
#include <cmath>

#include "ui_app_common.hpp"
#include "ui_cheats.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;

namespace
{
struct Badge
{
    const char *label;
    Color fill, ink;
};

Badge badge_of(const Cheat &c)
{
    if (c.pack)
        return {"16:9", rgba(0x5CD3FF, 0.9f), rgba(0x04203A)};
    if (c.kind == "OnFrame")
        return {"Patch", rgba(0xB89BFF, 0.9f), rgba(0x1A0E3A)};
    if (c.kind == "ActionReplay")
        return {"Action Replay", rgba(0xFFC857, 0.9f), rgba(0x2A1600)};
    return {"Gecko", rgba(0xFF9F6E, 0.9f), rgba(0x2A1000)};
}

/* "$Infinite Lives [Ralf]" -> "Infinite Lives", "Ralf". */
void split_name(const std::string &label, std::string &name, std::string &author)
{
    name = label;
    author.clear();
    const std::size_t open = label.rfind('[');
    if (open != std::string::npos && label.back() == ']' && open > 0)
    {
        author = label.substr(open + 1, label.size() - open - 2);
        name = label.substr(0, open);
        while (!name.empty() && name.back() == ' ')
            name.pop_back();
    }
}
} // namespace

void App::draw_cheat_cards(const std::vector<int> &section_rows, float px, float py, float pw, float ph)
{
    Gfx &g = *g_;
    (void)ph;
    const std::vector<std::string> pending = pending_keys();
    const Color kPending = rgba(0xFFC857), good = rgba(0x7CF0A6), hint = rgba(0x8A96C8);

    /* The count, and how many are on. */
    int on_count = 0, codes = 0;
    for (int i : section_rows)
    {
        const SettingRow &r = rows_[std::size_t(i)];
        if (r.key == "cheat" && r.bool_value)
        {
            ++codes;
            on_count += *r.bool_value ? 1 : 0;
        }
    }
    const float top = py + 150;
    {
        const std::string count = plural((long long)codes, "1 code", "{n} codes");
        const std::string lit = plural((long long)on_count, "1 on", "{n} on");
        float cx = px + 50;
        for (const auto &[text, c] : {std::pair<std::string, Color>{count, kSoft}, {lit, on_count ? good : hint}})
        {
            const float w = g.measure(Font::SemiBold, ts(22), text) + 34;
            g.panel(cx, top, w, 38, rgba(0x07102E, 0.55f), 1, 19, with_alpha(c, 0.7f), 1.4f);
            g.text_mid(Font::SemiBold, ts(22), cx + w * 0.5f, top + 19, c, Align::Center, text);
            cx += w + 12;
        }
        g.text_mid(Font::Regular, ts(21), px + pw - 50, top + 19, hint, Align::Right,
                   fit(g, Font::Regular, ts(21),
                       trf("Your own: /data/porpoise/cheats/{id}.ini", {{"id", game_for_ ? game_for_->id : ""}}),
                       pw * 0.55f));
    }

    /* Four cards at a time, moved along with the focus. */
    constexpr int kShown = 4;
    const float card_h = 100, gap = 10, x = px + 40, w = pw - 80, y0 = top + 56;
    int focus_k = -1;
    for (std::size_t k = 0; k < section_rows.size(); ++k)
        if (!on_rail_ && section_rows[k] == settings_row_)
            focus_k = int(k);
    int first = int(cheat_cards_first_);
    if (focus_k >= 0)
    {
        if (focus_k < first)
            first = focus_k;
        else if (focus_k >= first + kShown)
            first = focus_k - kShown + 1;
    }
    first = std::clamp(first, 0, std::max(0, int(section_rows.size()) - kShown));
    cheat_cards_first_ = float(first);

    for (int k = first; k < int(section_rows.size()) && k < first + kShown; ++k)
    {
        const int i = section_rows[std::size_t(k)];
        const SettingRow &r = rows_[std::size_t(i)];
        const bool focus = !on_rail_ && i == settings_row_;
        const float cy = y0 + float(k - first) * (card_h + gap);
        const bool is_code = r.key == "cheat" && r.folder >= 0 && r.folder < int(cheats_.size());
        const bool lit = r.bool_value && *r.bool_value;
        g.panel(x, cy, w, card_h, focus ? rgba(0x1D45B8, 0.9f) : lit ? rgba(0x0F2E5E, 0.7f) : rgba(0x0B1848, 0.6f),
                0.8f, kR, focus ? kIcy : lit ? with_alpha(good, 0.55f) : rgba(0x3D5AB0, 0.7f), focus ? 2.4f : 1.3f,
                focus ? 10 : 0, focus ? 0.18f : 0.0f);
        if (row_pending(r, pending))
            g.panel(x + 10, cy + card_h * 0.5f - 7, 14, 14, kPending, 1, 7); /* changed, not applied yet */

        float tx = x + 34;
        if (is_code)
        {
            /* What it is, and "Yours" when it's from your own file. */
            const Cheat &code = cheats_[std::size_t(r.folder)];
            float bw = 0;
            auto pill = [&](const Badge &b) {
                const std::string label = tr(b.label);
                const float w1 = g.measure(Font::Bold, ts(19), label) + 24;
                g.panel(tx + bw, cy + 18, w1, 30, b.fill, 1, 15);
                g.text_mid(Font::Bold, ts(19), tx + bw + w1 * 0.5f, cy + 33, b.ink, Align::Center, label);
                bw += w1 + 8;
            };
            pill(badge_of(code));
            if (code.own)
                pill({"Yours", rgba(0x2FB574, 0.9f), rgba(0x06200F)});
            bw -= 8;
            std::string name, author;
            split_name(r.label, name, author);
            g.text_mid(Font::Bold, ts(29), tx, cy + 68, focus ? kWhite : kSoft, Align::Left,
                       fit(g, Font::Bold, ts(29), name, w - 260));
            if (!author.empty())
                g.text_mid(Font::Regular, ts(21), tx + bw + 14, cy + 33, hint, Align::Left,
                           fit(g, Font::Regular, ts(21), trf("by {name}", {{"name", author}}), w - bw - 320));
        }
        else
        {
            /* The section's other rows (Widescreen): a label and its value. */
            g.text_mid(Font::SemiBold, ts(22), tx, cy + 33, hint, Align::Left, r.label);
            g.text_mid(Font::Bold, ts(29), tx, cy + 68, focus ? kWhite : kSoft, Align::Left,
                       fit(g, Font::Bold, ts(29), r.values.empty() ? std::string() : r.values.front(), w - 120));
        }

        if (r.bool_value)
        {
            /* The switch, larger than Settings' own. */
            const float tw = 112, th = 54, sx = x + w - tw - 28, sy = cy + card_h * 0.5f;
            g.panel(sx, sy - th * 0.5f, tw, th, lit ? rgba(0x2FB574, 0.95f) : rgba(0x07102E, 0.6f), 0.85f, th * 0.5f,
                    lit ? rgba(0xBDF5D8) : (focus ? rgba(0x8BD9FF) : rgba(0x3D5AB0, 0.85f)), focus ? 2.0f : 1.4f,
                    lit ? 10 : 0);
            const float kx = lit ? sx + tw - th * 0.5f : sx + th * 0.5f;
            g.blob(kx, sy, th * 0.9f, th * 0.9f, rgba(0xFFFFFF, lit ? 0.35f : 0.15f));
            g.panel(kx - th * 0.38f, sy - th * 0.38f, th * 0.76f, th * 0.76f, lit ? kWhite : rgba(0xA9B8E8), 1,
                    th * 0.38f);
        }
    }
    if (first > 0)
        g.glyph(Glyph::Arrow, px + pw - 24, y0 + 10, 18, rgba(0x58B8FF), 0);
    if (first + kShown < int(section_rows.size()))
        g.glyph(Glyph::Arrow, px + pw - 24, y0 + float(kShown) * (card_h + gap) - 20, 18, rgba(0x58B8FF), kPi);
}
} // namespace porpoise::ui
