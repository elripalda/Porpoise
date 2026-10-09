/* Porpoise UI - Memory Cards' other views (Settings > Interface > Memory
 * Cards view): Blocks, one card drawn as a card with its saves listed and
 * its blocks in a bar; and By game, every save on both cards and the Wii
 * grouped by the game it belongs to.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cmath>
#include <map>

#include "porpoise_pad.hpp"
#include "ui_app.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

namespace
{
std::string size_of(long long bytes)
{
    char buf[32];
    if (bytes >= (1LL << 20))
        std::snprintf(buf, sizeof buf, "%.1f MB", double(bytes) / double(1 << 20));
    else
        std::snprintf(buf, sizeof buf, "%lld KB", std::max(1LL, (bytes + 1023) / 1024));
    return buf;
}

/* Colours for the saves' blocks, in turn. */
const unsigned kBlockColours[] = {0x5CD3FF, 0x8B7CFF, 0x4CE0B0, 0xFFC85C, 0xFF8AB8, 0x7FB6FF, 0xB4F06A};
} // namespace

/* ---- Blocks ---------------------------------------------------------------------------- */

void App::draw_card_blocks(double time)
{
    Gfx &g = *g_;
    (void)time;
    ensure_saves(false);
    Card &card = mc_card_ == 0 ? card_a_ : card_b_;
    const int n = int(card.saves.size());
    const int sel = std::clamp(mc_sel_[mc_card_], 0, std::max(0, n - 1));

    /* The card itself: an original shape, the slot's letter on its label. */
    const float cx = 150, cy = 150, cw = 420, ch = 600;
    g.panel(cx, cy, cw, ch, rgba(0x1A2C7A, 0.85f), 0.7f, kR + 10, rgba(0x8BD9FF, 0.9f), 2.0f, 6, 0.3f);
    g.panel(cx + 34, cy + 40, cw - 68, 300, rgba(0x07102E, 0.75f), 1, kR, rgba(0x4C6FD8, 0.9f), 1.6f);
    g.text_mid(Font::ExtraBold, 170, cx + cw * 0.5f, cy + 175, kWhite, Align::Center, card.slot);
    g.text_mid(Font::SemiBold, ts(24), cx + cw * 0.5f, cy + 290, kLavender, Align::Center,
               trf("Slot {slot}", {{"slot", card.slot}}));
    draw_mark(cx + cw * 0.5f, cy + 410, 120, with_alpha(kCyan, 0.8f));
    const std::string free = card.present ? std::to_string(card.free_blocks) : std::to_string(card.total_blocks);
    g.text_mid(Font::Bold, ts(34), cx + cw * 0.5f, cy + 490, kWhite, Align::Center, free);
    g.text_mid(Font::Regular, ts(22), cx + cw * 0.5f, cy + 528, kSoft, Align::Center,
               trf("of {n} blocks free", {{"n", std::to_string(card.total_blocks)}}));
    /* Its contacts along the bottom. */
    for (int i = 0; i < 9; ++i)
        g.panel(cx + 66 + float(i) * 32, cy + ch - 26, 20, 14, rgba(0xFFC85C, 0.85f), 0.8f, 3);
    /* Left / right: the other card. */
    g.glyph(Glyph::Arrow, cx - 34, cy + ch * 0.5f, 32, rgba(0x58B8FF), -kPi * 0.5f);
    g.glyph(Glyph::Arrow, cx + cw + 34, cy + ch * 0.5f, 32, rgba(0x58B8FF), kPi * 0.5f);

    /* Its saves, listed. */
    const float lx = 640, ly = 136, lw = 1130, lh = 648, row_h = 82;
    g.panel(lx, ly, lw, lh, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    g.text_mid(Font::Bold, ts(36), lx + 40, ly + 52, kWhite, Align::Left, trf("Slot {slot}", {{"slot", card.slot}}));
    g.text_mid(Font::Regular, ts(24), lx + lw - 40, ly + 52, kLavender, Align::Right,
               card.present ? plural(n, "1 save", "{n} saves") : tr("No saves yet"));
    const int visible = 6;
    int first = std::clamp(sel - visible / 2, 0, std::max(0, n - visible));
    float y = ly + 100;
    if (n == 0)
        g.text(Font::Regular, ts(26), lx + 40, y + 20, kSoft, Align::Left,
               tr("Your saves appear here after you save in a game."));
    for (int i = first; i < std::min(n, first + visible); ++i)
    {
        Save &s = card.saves[std::size_t(i)];
        const bool on = i == sel;
        if (on)
            g.panel(lx + 20, y, lw - 40, row_h - 8, rgba(0x1F63F0, 0.9f), 0.62f, kR, rgba(0x7FD9FF), 1.6f, 6, 0.3f);
        const Color dot = rgba(kBlockColours[std::size_t(i) % 7]);
        g.panel(lx + 36, y + (row_h - 8) * 0.5f - 6, 12, 12, dot, 1, 6);
        if (Texture *icon = save_icon(s))
            g.image(icon, lx + 64, y + 5, 64, 64, kWhite, 6);
        const std::string title = s.title.empty() ? s.file_name : s.title;
        g.text_mid(Font::Bold, ts(28), lx + 148, y + 24, kWhite, Align::Left,
                   fit(g, Font::Bold, ts(28), title, lw - 520));
        g.text_mid(Font::Regular, ts(22), lx + 148, y + 54, on ? kWhite : kLavender, Align::Left,
                   fit(g, Font::Regular, ts(22), s.detail.empty() ? s.game_code : s.detail, lw - 520));
        g.text_mid(Font::Bold, ts(26), lx + lw - 44, y + 24, kWhite, Align::Right, plural(s.blocks, "1 block", "{n} blocks"));
        g.text_mid(Font::Regular, ts(22), lx + lw - 44, y + 54, on ? kWhite : kLavender, Align::Right,
                   format_date(s.modified));
        y += row_h;
    }

    /* What fills the card: each save's share of the blocks in use, the
     * chosen one bright. */
    const float bx = 150, by = 810, bw = 1620, bh = 42;
    g.panel(bx, by, bw, bh, rgba(0x050C26, 0.8f), 1, bh * 0.5f, rgba(0x3D4F9E, 0.9f), 1.4f);
    int used_by_saves = 0;
    for (const Save &s : card.saves)
        used_by_saves += std::max(1, s.blocks);
    float at = bx + 4;
    const float inner = bw - 8 - float(std::max(0, n - 1)) * 3;
    for (int i = 0; i < n; ++i)
    {
        const float wseg = inner * float(std::max(1, card.saves[std::size_t(i)].blocks)) / float(std::max(1, used_by_saves));
        const Color c = rgba(kBlockColours[std::size_t(i) % 7], i == sel ? 1.0f : 0.55f);
        g.panel(at, by + 4, std::max(4.0f, wseg), bh - 8, c, 0.8f, std::min((bh - 8) * 0.5f, wseg * 0.5f),
                i == sel ? kWhite : kClear, i == sel ? 2.0f : 0.0f);
        at += wseg + 3;
    }
    const int used = card.total_blocks - card.free_blocks;
    g.text_mid(Font::SemiBold, ts(24), bx, by + bh + 34, kSoft, Align::Left,
               trf("{n} blocks used", {{"n", std::to_string(std::max(0, used))}}));
    g.text_mid(Font::SemiBold, ts(24), bx + bw, by + bh + 34, kSoft, Align::Right,
               trf("{n} free", {{"n", free}}));
    /* And how full the card is, on the card. */
    {
        const float mx = cx + 60, my = cy + 548, mw = cw - 120, mh = 10;
        const float frac = card.total_blocks > 0 ? float(std::max(0, used)) / float(card.total_blocks) : 0.0f;
        g.panel(mx, my, mw, mh, rgba(0x050C26, 0.8f), 1, mh * 0.5f);
        g.panel(mx, my, std::max(mh, mw * frac), mh, kCyan, 0.8f, mh * 0.5f);
    }

    if (sel < n)
        draw_prompts({{Glyph::DPad, "Browse"}, {kKeyL2R2, "Wii Saves"}, {Glyph::Circle, "Back"}},
                     {{Glyph::Options, "To USB"},
                      {Glyph::Square, trf("Copy to {slot}", {{"slot", mc_card_ == 0 ? "B" : "A"}})},
                      {Glyph::Triangle, "Delete"}},
                     "");
    else
        draw_prompts({{Glyph::DPad, "Browse"}, {kKeyL2R2, "Wii Saves"}, {Glyph::Circle, "Back"}}, {}, "");
}

/* ---- By game --------------------------------------------------------------------------- */

std::vector<App::SaveGroup> App::saves_by_game()
{
    std::vector<SaveGroup> groups;
    std::map<std::string, std::size_t> at;
    auto &games = lib_->games();
    auto group_for = [&](const std::string &code, const std::string &fallback) -> SaveGroup & {
        const std::string key = code.substr(0, std::min<std::size_t>(4, code.size()));
        auto it = at.find(key);
        if (it != at.end())
            return groups[it->second];
        SaveGroup grp;
        grp.key = key;
        grp.title = fallback;
        for (Game &game : games)
            if (!key.empty() && game.id.compare(0, key.size(), key) == 0)
            {
                grp.title = game.title;
                grp.game = &game;
                break;
            }
        at[key] = groups.size();
        groups.push_back(grp);
        return groups.back();
    };
    for (int k = 0; k < 2; ++k)
    {
        Card &card = k == 0 ? card_a_ : card_b_;
        for (int i = 0; i < int(card.saves.size()); ++i)
        {
            const Save &s = card.saves[std::size_t(i)];
            group_for(s.game_code, s.title.empty() ? s.file_name : s.title).saves.push_back({k, i});
        }
    }
    for (int i = 0; i < int(wii_saves_.size()); ++i)
    {
        const WiiSave &s = wii_saves_[std::size_t(i)];
        group_for(s.game_code, s.title).saves.push_back({2, i});
    }
    std::sort(groups.begin(), groups.end(), [](const SaveGroup &a, const SaveGroup &b) { return a.title < b.title; });
    return groups;
}

void App::draw_saves_by_game(double time)
{
    Gfx &g = *g_;
    (void)time;
    ensure_saves(false);
    ensure_saves(true);
    const std::vector<SaveGroup> groups = saves_by_game();
    int count = 0;
    for (const SaveGroup &grp : groups)
        count += int(grp.saves.size());
    mc_game_sel_ = std::clamp(mc_game_sel_, 0, std::max(0, count - 1));

    const float lx = 150, ly = 136, lw = 1010, lh = 800;
    g.panel(lx, ly, lw, lh, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    g.text_mid(Font::Bold, ts(36), lx + 40, ly + 52, kWhite, Align::Left, tr("By Game"));
    g.text_mid(Font::Regular, ts(24), lx + lw - 40, ly + 52, kLavender, Align::Right,
               plural((long long)groups.size(), "1 game", "{n} games") + "   \xE2\x80\xA2   " +
                   plural(count, "1 save", "{n} saves"));

    /* The rows, laid out once to find the focused one and keep it in view. */
    const float head_h = 70, row_h = 74, top = ly + 96, bottom = ly + lh - 20;
    float y = 0, focus_y = 0;
    int idx = 0;
    const SaveGroup *focus_group = nullptr;
    SaveRef focus{};
    for (const SaveGroup &grp : groups)
    {
        y += head_h;
        for (const SaveRef &r : grp.saves)
        {
            if (idx == mc_game_sel_)
            {
                focus_y = y;
                focus = r;
                focus_group = &grp;
            }
            y += row_h;
            ++idx;
        }
    }
    const float view_h = bottom - top;
    const float target = std::clamp(focus_y - view_h * 0.4f, 0.0f, std::max(0.0f, y - view_h));
    mc_game_scroll_ = settings_->reduced_motion ? target : mc_game_scroll_ + (target - mc_game_scroll_) * 0.25f;

    y = top - mc_game_scroll_;
    idx = 0;
    for (const SaveGroup &grp : groups)
    {
        if (y > top - head_h && y < bottom)
        {
            float tx = lx + 40;
            if (grp.game)
                if (Texture *cover = cover_of(*grp.game))
                {
                    g.image(cover, lx + 36, y + 8, 40, 56, kWhite, 4);
                    tx = lx + 92;
                }
            g.text_mid(Font::Bold, ts(28), tx, y + head_h * 0.5f, kWhite, Align::Left,
                       fit(g, Font::Bold, ts(28), grp.title, lw - 200));
            g.text_mid(Font::Regular, ts(22), lx + lw - 40, y + head_h * 0.5f, kLavender, Align::Right,
                       plural((long long)grp.saves.size(), "1 save", "{n} saves"));
        }
        y += head_h;
        for (const SaveRef &r : grp.saves)
        {
            const bool on = idx == mc_game_sel_;
            if (y > top - row_h && y < bottom)
            {
                if (on)
                    g.panel(lx + 60, y, lw - 80, row_h - 8, rgba(0x1F63F0, 0.9f), 0.62f, kR, rgba(0x7FD9FF), 1.6f, 6,
                            0.3f);
                Texture *icon = nullptr;
                std::string title, detail, where, size;
                if (r.kind < 2)
                {
                    Save &s = (r.kind == 0 ? card_a_ : card_b_).saves[std::size_t(r.index)];
                    icon = save_icon(s);
                    title = s.title.empty() ? s.file_name : s.title;
                    detail = s.detail;
                    where = trf("Slot {slot}", {{"slot", r.kind == 0 ? "A" : "B"}});
                    size = plural(s.blocks, "1 block", "{n} blocks");
                }
                else
                {
                    WiiSave &s = wii_saves_[std::size_t(r.index)];
                    icon = wii_save_icon(s);
                    title = s.title;
                    detail = s.detail;
                    where = "Wii";
                    size = size_of(s.bytes);
                }
                if (icon)
                    g.image(icon, lx + 76, y + 6, 54, 54, kWhite, 6);
                const float bw = g.measure(Font::Bold, ts(20), where) + 24;
                g.panel(lx + 146, y + (row_h - 8) * 0.5f - 15, bw, 30, rgba(0x07102E, 0.7f), 1, 15,
                        r.kind == 2 ? rgba(0x7FD9FF) : rgba(0xB89BFF), 1.4f);
                g.text_mid(Font::Bold, ts(20), lx + 146 + bw * 0.5f, y + (row_h - 8) * 0.5f, kWhite, Align::Center, where);
                const float tx = lx + 146 + bw + 20;
                g.text_mid(Font::SemiBold, ts(25), tx, y + 22, kWhite, Align::Left,
                           fit(g, Font::SemiBold, ts(25), title, lx + lw - 200 - tx));
                g.text_mid(Font::Regular, ts(21), tx, y + 48, on ? kWhite : kLavender, Align::Left,
                           fit(g, Font::Regular, ts(21), detail, lx + lw - 200 - tx));
                g.text_mid(Font::SemiBold, ts(22), lx + lw - 40, y + (row_h - 8) * 0.5f, on ? kWhite : kSoft,
                           Align::Right, size);
            }
            y += row_h;
            ++idx;
        }
    }
    if (count == 0)
        g.text(Font::Regular, ts(26), lx + 40, top + 20, kSoft, Align::Left,
               tr("Your saves appear here after you save in a game."));

    /* The focused save, large. */
    const float px = 1190, py = 136, pw = 580, ph = 800;
    g.panel(px, py, pw, ph, rgba(0x0A1236, 0.72f), 0.85f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    if (focus_group)
    {
        Texture *banner = nullptr;
        std::string title, detail, where, size, date;
        if (focus.kind < 2)
        {
            Save &s = (focus.kind == 0 ? card_a_ : card_b_).saves[std::size_t(focus.index)];
            banner = save_banner(s);
            title = s.title.empty() ? s.file_name : s.title;
            detail = s.detail;
            where = trf("Memory card, Slot {slot}", {{"slot", focus.kind == 0 ? "A" : "B"}});
            size = plural(s.blocks, "1 block", "{n} blocks");
            date = format_date(s.modified);
        }
        else
        {
            WiiSave &s = wii_saves_[std::size_t(focus.index)];
            banner = wii_save_banner(s);
            title = s.title;
            detail = s.detail;
            where = tr("Wii System Memory");
            size = size_of(s.bytes);
            date = format_date(s.modified);
        }
        if (banner)
            g.image(banner, px + 50, py + 50, pw - 100, (pw - 100) / 3.0f, kWhite, kR);
        float ty = py + 50 + (pw - 100) / 3.0f + 40;
        for (const std::string &line : wrap(g, Font::Bold, ts(32), title, pw - 100, 2))
        {
            g.text(Font::Bold, ts(32), px + 50, ty, kWhite, Align::Left, line);
            ty += ts(32) * 1.2f;
        }
        for (const std::string &line : wrap(g, Font::Regular, ts(24), detail, pw - 100, 2))
        {
            g.text(Font::Regular, ts(24), px + 50, ty, kLavender, Align::Left, line);
            ty += ts(24) * 1.3f;
        }
        ty += 20;
        for (const std::string &line : {where, size, date})
        {
            g.text(Font::SemiBold, ts(24), px + 50, ty, kSoft, Align::Left, line);
            ty += 40;
        }
        if (focus_group->game)
            if (Texture *cover = cover_of(*focus_group->game))
            {
                const float cw = 170, chh = cw * 190.0f / 135.0f;
                g.image(cover, px + pw - 50 - cw, py + ph - 40 - chh, cw, chh, kWhite, 8);
            }
    }
    if (count > 0)
        draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Circle, "Back"}},
                     {{Glyph::Options, "To USB"}, {Glyph::Square, focus.kind == 2 ? "Back Up" : "Copy"},
                      {Glyph::Triangle, "Delete"}},
                     "");
    else
        draw_prompts({{Glyph::Circle, "Back"}}, {}, "");
}

/* Up and down through every save; the buttons act on the focused one as
 * they do in the other views. */
void App::update_saves_by_game(bool up, bool down)
{
    const std::vector<SaveGroup> groups = saves_by_game();
    std::vector<SaveRef> all;
    for (const SaveGroup &grp : groups)
        all.insert(all.end(), grp.saves.begin(), grp.saves.end());
    if (all.empty())
        return;
    const int before = mc_game_sel_;
    if (up && mc_game_sel_ > 0)
        --mc_game_sel_;
    if (down && mc_game_sel_ + 1 < int(all.size()))
        ++mc_game_sel_;
    mc_game_sel_ = std::clamp(mc_game_sel_, 0, int(all.size()) - 1);
    if (mc_game_sel_ != before)
        sfx(Sound::MenuScroll);
    const SaveRef &r = all[std::size_t(mc_game_sel_)];
    if (r.kind == 2)
    {
        wii_sel_ = r.index;
        update_wii_saves(false, false, false, false); /* its buttons */
        return;
    }
    mc_card_ = r.kind;
    mc_sel_[r.kind] = r.index;
    if (pressed(BtnTriangle))
        ask_delete_save();
    if (pressed(BtnSquare))
        ask_copy_save();
    if (pressed(BtnOptions))
        export_save_to_usb();
}

/* ---- Cubes ------------------------------------------------------------------------------- */

/* Both cards side by side, each save a little glass cube with its icon on
 * the front; the chosen one rises and turns toward you. */
void App::draw_card_cubes(Card &card, int which, float x, float y, double time)
{
    Gfx &g = *g_;
    const bool active = mc_card_ == which;
    const int n = int(card.saves.size());
    /* The slot letter and its free blocks, over the grid. */
    g.text_mid(Font::ExtraBold, ts(60), x + 40, y + 40, kWhite, Align::Left, card.slot);
    const std::string free = card.present ? std::to_string(card.free_blocks) : std::to_string(card.total_blocks);
    g.text_mid(Font::SemiBold, ts(24), x + 120, y + 40, kSoft, Align::Left, trc("memcard", "Open"));
    const float bw = std::max(110.0f, g.measure(Font::Bold, ts(30), free) + 40);
    g.panel(x + 210, y + 16, bw, 48, rgba(0x07102E, 0.6f), 1, 6, rgba(0xDDE6FF, 0.9f), 1.8f);
    g.text_mid(Font::Bold, ts(30), x + 210 + bw * 0.5f, y + 40, kWhite, Align::Center, free);

    const int sel = mc_sel_[which];
    const int rows_total = std::max(kMcRows, (n + kMcCols - 1) / kMcCols);
    int first = int(mc_scroll_[which]);
    if (sel / kMcCols < first)
        first = sel / kMcCols;
    if (sel / kMcCols > first + kMcRows - 1)
        first = sel / kMcCols - (kMcRows - 1);
    first = std::clamp(first, 0, rows_total - kMcRows);
    mc_scroll_[which] = float(first);
    const float cell = 168, size = 104;
    const float gx = x + 40 + cell * 0.5f, gy = y + 150 + cell * 0.5f;
    const float spin = settings_->reduced_motion ? 0.0f : float(time);
    for (int pass = 0; pass < 2; ++pass)
        for (int r = 0; r < kMcRows; ++r)
            for (int c = 0; c < kMcCols; ++c)
            {
                const int idx = (first + r) * kMcCols + c;
                const bool focused = active && idx == sel;
                if ((pass == 1) != focused)
                    continue;
                const float cx = gx + float(c) * cell, cy = gy + float(r) * cell;
                if (idx >= n)
                {
                    /* An empty place: a dim, flat square on the grid. */
                    g.panel(cx - size * 0.42f, cy - size * 0.42f, size * 0.84f, size * 0.84f, rgba(0x0B1A55, 0.55f), 1,
                            6, focused ? kIcy : rgba(0x2A3C80, 0.6f), focused ? 2.2f : 1.2f);
                    continue;
                }
                Save &s = card.saves[std::size_t(idx)];
                const float lift = focused ? 1.0f : 0.0f;
                const float yaw = focused ? 0.35f * std::sin(spin * 1.1f) : -0.32f;
                const float pitch = focused ? 0.18f : 0.38f;
                draw_glass_cube(cx, cy - lift * 22, size * (focused ? 1.18f : 1.0f), yaw, pitch, 0,
                                rgba(0x1C48D8, 0.55f), focused ? kIcy : kEdge, save_icon(s), 1.0f, focused);
            }
}

/* ---- Retro TV ----------------------------------------------------------------------------- */

/* The card in your hand, the other behind it, and its saves on an old TV:
 * big pixel icons on a blue screen with scanlines, the chosen one hopping. */
void App::draw_card_classic(double time)
{
    Gfx &g = *g_;
    ensure_saves(false);
    Card &card = mc_card_ == 0 ? card_a_ : card_b_;
    Card &other = mc_card_ == 0 ? card_b_ : card_a_;
    const int n = int(card.saves.size());
    const int sel = std::clamp(mc_sel_[mc_card_], 0, std::max(0, n - 1));
    const float t = settings_->reduced_motion ? 0.0f : float(time);

    /* The cards: plastic you can see into, a label with the slot's letter, and
     * the contacts along the top edge. */
    auto physical = [&](const Card &c, float x, float y, float fade, bool front) {
        const float w = 300, h = 400;
        g.panel(x, y, w, h, rgba(0x8E98AE, 0.50f * fade), 0.75f, 26, rgba(0xD6DDEB, 0.85f * fade), 2.0f,
                front ? 8.0f : 0.0f, 0.35f);
        g.panel(x + 26, y + 24, w - 52, 26, rgba(0x2A2F3A, 0.55f * fade), 1, 6);
        for (int i = 0; i < 8; ++i)
            g.panel(x + 40 + float(i) * 28, y + 30, 16, 14, rgba(0xE8C25C, 0.9f * fade), 0.8f, 3);
        g.set_solid(true); /* the label is paper */
        g.panel(x + 30, y + 90, w - 60, h - 150, rgba(0xF2EEE2, 0.95f * fade), 0.92f, 10, rgba(0xFFFFFF, 0.6f * fade), 1.2f);
        g.set_solid(false);
        g.text_mid(Font::ExtraBold, 150, x + w * 0.5f, y + 210, with_alpha(rgba(0x1B2A6B), fade), Align::Center, c.slot);
        const std::string free = c.present ? std::to_string(c.free_blocks) : std::to_string(c.total_blocks);
        g.text_mid(Font::SemiBold, ts(22), x + w * 0.5f, y + 312, with_alpha(rgba(0x3A4370), fade), Align::Center,
                   fit(g, Font::SemiBold, ts(22), trf("{n} blocks free", {{"n", free}}), w - 80));
    };
    const bool toned = g.toned();
    g.set_tone(false); /* plastic and paper in every theme */
    physical(other, 210, 250, 0.45f, false);
    physical(card, 150, 200, 1.0f, true);
    g.set_tone(toned);
    g.glyph(Glyph::Arrow, 110, 400, 30, rgba(0x58B8FF), -kPi * 0.5f);
    g.glyph(Glyph::Arrow, 560, 400, 30, rgba(0x58B8FF), kPi * 0.5f);

    /* The TV: its plastic, then the screen. */
    const float tx = 640, ty = 140, tw = 1140, th = 760;
    g.set_tone(false);
    g.set_solid(true);
    g.panel(tx, ty, tw, th, rgba(0x15171D, 0.96f), 0.8f, 44, rgba(0x343844, 0.9f), 2.0f);
    const float sx = tx + 40, sy = ty + 36, sw = tw - 80, sh = th - 72;
    g.set_tone(false); /* the screen is a screen in every theme */
    g.panel(sx, sy, sw, sh, rgba(0x0D2E7A), 0.45f, 30, rgba(0x000000, 0.6f), 3.0f);
    g.text_mid(Font::Bold, ts(30), sx + 40, sy + 46, rgba(0xF4F7FF), Align::Left, trf("Slot {slot}", {{"slot", card.slot}}));
    const std::string free = card.present ? std::to_string(card.free_blocks) : std::to_string(card.total_blocks);
    g.text_mid(Font::SemiBold, ts(24), sx + sw - 40, sy + 46, rgba(0xBFD3FF), Align::Right,
               trf("{n} blocks free", {{"n", free}}));

    /* The saves: twelve to a screen, the rows following the chosen one. */
    const int rows_total = std::max(kClassicRows, (n + kClassicCols - 1) / kClassicCols);
    mc_classic_first_ = std::clamp(mc_classic_first_, sel / kClassicCols - (kClassicRows - 1), sel / kClassicCols);
    mc_classic_first_ = std::clamp(mc_classic_first_, 0, rows_total - kClassicRows);
    const float cell_w = (sw - 80) / float(kClassicCols), cell_h = 150, gx = sx + 40, gy = sy + 90;
    for (int r = 0; r < kClassicRows; ++r)
        for (int c = 0; c < kClassicCols; ++c)
        {
            const int idx = (mc_classic_first_ + r) * kClassicCols + c;
            const float cx = gx + (float(c) + 0.5f) * cell_w, cy = gy + (float(r) + 0.5f) * cell_h;
            if (idx >= n)
            {
                g.panel(cx - 44, cy - 44, 88, 88, rgba(0x0A2466, 0.6f), 1, 8, rgba(0x2E58B8, 0.5f), 1.2f);
                continue;
            }
            Save &s = card.saves[std::size_t(idx)];
            const bool on = idx == sel;
            const float hop = on ? -std::fabs(std::sin(t * 5.0f)) * 16.0f : 0.0f;
            if (on)
                g.panel(cx - 62, cy - 62, 124, 124, kClear, 1, 12,
                        rgba(0xFFF3A0, 0.65f + 0.35f * std::sin(t * 7.0f)), 3.0f, 8);
            if (Texture *icon = save_icon(s))
                g.image(icon, cx - 48, cy - 48 + hop, 96, 96, kWhite);
            else
                g.panel(cx - 44, cy - 44 + hop, 88, 88, rgba(0x2E58B8, 0.8f), 1, 8);
        }
    if (mc_classic_first_ > 0)
        g.glyph(Glyph::Arrow, sx + sw - 28, gy + 14, 18, rgba(0xBFD3FF), 0);
    if ((mc_classic_first_ + kClassicRows) * kClassicCols < n)
        g.glyph(Glyph::Arrow, sx + sw - 28, gy + cell_h * kClassicRows - 14, 18, rgba(0xBFD3FF), kPi);

    /* The chosen save, along the bottom of the screen. */
    const float iy = gy + cell_h * kClassicRows + 24;
    if (sel < n)
    {
        Save &s = card.saves[std::size_t(sel)];
        const std::string title = s.title.empty() ? s.file_name : s.title;
        g.text_mid(Font::Bold, ts(30), sx + 40, iy + 22, rgba(0xF4F7FF), Align::Left,
                   fit(g, Font::Bold, ts(30), title, sw - 380));
        g.text_mid(Font::Regular, ts(23), sx + 40, iy + 62, rgba(0xBFD3FF), Align::Left,
                   fit(g, Font::Regular, ts(23), s.detail.empty() ? s.game_code : s.detail, sw - 380));
        g.text_mid(Font::Bold, ts(28), sx + sw - 40, iy + 22, rgba(0xF4F7FF), Align::Right,
                   plural(s.blocks, "1 block", "{n} blocks"));
        g.text_mid(Font::Regular, ts(23), sx + sw - 40, iy + 62, rgba(0xBFD3FF), Align::Right, format_date(s.modified));
    }
    else
        g.text_mid(Font::Regular, ts(26), sx + 40, iy + 40, rgba(0xBFD3FF), Align::Left,
                   tr("Your saves appear here after you save in a game."));
    /* Scanlines and the glass's shine. */
    for (float y = sy + 4; y < sy + sh - 4; y += 4)
        g.panel(sx + 6, y, sw - 12, 1.6f, rgba(0x000000, 0.10f), 1, 0);
    g.panel(sx + 10, sy + 8, sw - 20, 60, rgba(0xFFFFFF, 0.025f), 1.6f, 22); /* the glass's shine, up top */
    g.set_solid(false);
    g.set_tone(toned);

    if (sel < n)
        draw_prompts({{Glyph::DPad, "Browse"}, {kKeyL2R2, "Wii Saves"}, {Glyph::Circle, "Back"}},
                     {{Glyph::Options, "To USB"},
                      {Glyph::Square, trf("Copy to {slot}", {{"slot", mc_card_ == 0 ? "B" : "A"}})},
                      {Glyph::Triangle, "Delete"}},
                     "");
    else
        draw_prompts({{Glyph::DPad, "Browse"}, {kKeyL2R2, "Wii Saves"}, {Glyph::Circle, "Back"}}, {}, "");
}

/* Retro TV: through the grid; left from the first save or right from the
 * last goes to the other card. */
void App::update_card_classic(bool left, bool right, bool up, bool down)
{
    const int n = int((mc_card_ == 0 ? card_a_ : card_b_).saves.size());
    int &sel = mc_sel_[mc_card_];
    sel = std::clamp(sel, 0, std::max(0, n - 1));
    const int before = sel;
    bool other = false;
    if (left)
    {
        if (sel % kClassicCols > 0)
            --sel;
        else
            other = mc_card_ == 1;
    }
    if (right)
    {
        if (sel % kClassicCols < kClassicCols - 1 && sel + 1 < n)
            ++sel;
        else
            other = mc_card_ == 0;
    }
    if (up && sel >= kClassicCols)
        sel -= kClassicCols;
    if (down && sel + kClassicCols < n)
        sel += kClassicCols;
    if (other)
    {
        mc_card_ = 1 - mc_card_;
        tab_anim_ = 0.5f;
        tab_dir_ = right ? +1 : -1;
        sfx(Sound::MovingTab);
    }
    else if (sel != before)
        sfx(Sound::MenuScroll);
    if (pressed(BtnTriangle))
        ask_delete_save();
    if (pressed(BtnSquare))
        ask_copy_save();
    if (pressed(BtnOptions))
        export_save_to_usb();
}
} // namespace porpoise::ui
