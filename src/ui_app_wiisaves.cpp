/* Porpoise UI - Memory Cards' other half: the Wii saves Dolphin keeps in its
 * virtual Wii storage, each with the banner and name the game gave it.
 * Square backs one up, Triangle deletes it; L2 / R2 go back to the
 * GameCube memory cards.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cmath>
#include <cstdio>

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
constexpr int kCols = 3, kRowsShown = 3;
constexpr float kPanelX = kMcX[0], kPanelY = kMcY, kPanelW = kMcX[1] + kMcW - kMcX[0], kPanelH = kMcH;
constexpr float kGap = 26, kPad = 40, kHead = 96;
constexpr float kCellW = (kPanelW - kPad * 2 - kGap * (kCols - 1)) / kCols;
constexpr float kCellH = kCellW / 3.0f; /* a save banner is 192 x 64 */

std::string size_text(long long bytes)
{
    char buf[32];
    if (bytes >= (1LL << 20))
        std::snprintf(buf, sizeof buf, "%.1f MB", double(bytes) / double(1 << 20));
    else
        std::snprintf(buf, sizeof buf, "%lld KB", std::max(1LL, (bytes + 1023) / 1024));
    return buf;
}
} // namespace

void App::free_wii_textures()
{
    for (WiiSave &s : wii_saves_)
    {
        if (s.icon_tex)
            g_->free_texture(s.icon_tex);
        if (s.banner_tex)
            g_->free_texture(s.banner_tex);
        s.icon_tex = s.banner_tex = nullptr;
    }
}

void App::scan_wii_saves()
{
    save_scan_.reset(); /* read now: a scan still running is stale */
    free_wii_textures();
    load_wii_saves(saves_dir_, wii_saves_);
    finish_wii_scan();
}

void App::finish_wii_scan()
{
    wii_scanned_ = true;
    wii_sel_ = std::clamp(wii_sel_, 0, std::max(0, int(wii_saves_.size()) - 1));
    if (!wii_focus_code_.empty())
    {
        for (std::size_t i = 0; i < wii_saves_.size(); ++i)
            if (wii_saves_[i].game_code.compare(0, 3, wii_focus_code_, 0, 3) == 0)
            {
                wii_sel_ = int(i);
                break;
            }
        wii_focus_code_.clear();
    }
}

Texture *App::wii_save_banner(WiiSave &s)
{
    if (!s.banner_tex && s.banner.size() == 192u * 64u * 4u)
    {
        const auto big = enlarge(s.banner, 192, 64, 2);
        s.banner_tex = g_->texture_rgba(big.data(), 384, 128);
    }
    return s.banner_tex;
}

Texture *App::wii_save_icon(WiiSave &s)
{
    if (!s.icon_tex && s.icon.size() == 48u * 48u * 4u)
    {
        const auto big = enlarge(s.icon, 48, 48, 2);
        s.icon_tex = g_->texture_rgba(big.data(), 96, 96);
    }
    return s.icon_tex;
}

void App::update_wii_saves(bool left, bool right, bool up, bool down)
{
    if (!wii_scanned_)
        return;
    const int n = int(wii_saves_.size());
    if (n == 0)
        return;
    const int before = wii_sel_;
    if (left && wii_sel_ % kCols > 0)
        --wii_sel_;
    if (right && wii_sel_ % kCols < kCols - 1 && wii_sel_ + 1 < n)
        ++wii_sel_;
    if (up && wii_sel_ >= kCols)
        wii_sel_ -= kCols;
    if (down && wii_sel_ + kCols < n)
        wii_sel_ += kCols;
    else if (down && wii_sel_ / kCols < (n - 1) / kCols)
        wii_sel_ = n - 1; /* the last row is shorter */
    if (wii_sel_ != before)
    {
        sfx(Sound::GameRow);
        mc_lift_ = 0.4f;
    }
    WiiSave &s = wii_saves_[std::size_t(wii_sel_)];
    if (pressed(BtnTriangle))
        open_dialog(DialogKind::DeleteWiiSave, tr("Delete this Wii save?"),
                    trf("{save}. Its files are removed for good; Square backs it up first if you want to keep a "
                        "copy.",
                        {{"save", s.title + (s.detail.empty() ? "" : " - " + s.detail)}}),
                    tr("Delete"), true);
    if (pressed(BtnOptions))
    {
        /* Beta: the save onto a USB drive, for a computer or another console. */
        std::string where;
        if (usb_root().empty())
            open_dialog(DialogKind::Info, tr("No USB drive"),
                        tr("Plug in a USB drive (exFAT) to copy saves to it."), "");
        else if (export_wii_save(saves_dir_, s, where))
        {
            flash_note(trf("Copied to the USB drive: {path}", {{"path", where.substr(where.find("Porpoise Saves"))}}));
            sfx(Sound::LaunchGame);
        }
        else
            open_dialog(DialogKind::Info, tr("Could not copy the save"),
                        tr("Copying it to the USB drive failed. Is the drive full or read-only?"), "");
    }
    if (pressed(BtnSquare))
    {
        std::string where;
        if (backup_wii_save(saves_dir_, s, where))
        {
            flash_note(trf("Backed up to {path}", {{"path", where}}));
            sfx(Sound::LaunchGame);
        }
        else
            open_dialog(DialogKind::Info, tr("Could not back up the save"),
                        trf("Copying it to {path} failed.", {{"path", where}}), "");
    }
    /* Keep the focus in view. */
    const int row = wii_sel_ / kCols;
    if (row < int(wii_scroll_))
        wii_scroll_ = float(row);
    if (row >= int(wii_scroll_) + kRowsShown)
        wii_scroll_ = float(row - kRowsShown + 1);
}

void App::draw_wii_saves(double time)
{
    Gfx &g = *g_;
    (void)time;
    ensure_saves(true);
    const int n = int(wii_saves_.size());
    g.panel(kPanelX, kPanelY, kPanelW, kPanelH, rgba(0x0A1236, 0.62f), 0.85f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    long long total = 0;
    for (const WiiSave &s : wii_saves_)
        total += s.bytes;
    g.text_mid(Font::Bold, ts(36), kPanelX + kPad, kPanelY + 52, kWhite, Align::Left, title_case(tr("Wii saves")));
    g.text_mid(Font::Regular, ts(24), kPanelX + kPanelW - kPad, kPanelY + 52, kLavender, Align::Right,
               n ? plural(n, "1 game", "{n} games") + "   \xE2\x80\xA2   " + size_text(total) : std::string());

    const float gx = kPanelX + kPad, gy = kPanelY + kHead;
    const int first_row = int(wii_scroll_);
    for (int i = first_row * kCols; i < n && i < (first_row + kRowsShown) * kCols; ++i)
    {
        const int r = i / kCols - first_row, c = i % kCols;
        const bool on = i == wii_sel_;
        const float lift = on ? mc_lift_ * 6.0f : 0.0f;
        const float x = gx + c * (kCellW + kGap), y = gy + r * (kCellH + kGap) - lift;
        WiiSave &s = wii_saves_[std::size_t(i)];
        g.panel(x - 4, y - 4, kCellW + 8, kCellH + 8, on ? rgba(0x1D45B8, 0.9f) : rgba(0x0E1C55, 0.6f), 0.8f, kR + 4,
                on ? kIcy : rgba(0x3D5AB0, 0.8f), on ? 2.6f : 1.4f, on ? 10.0f : 0.0f);
        if (Texture *b = wii_save_banner(s))
            g.image(b, x, y, kCellW, kCellH, kWhite, kR);
        else
        {
            if (Texture *ic = wii_save_icon(s))
                g.image(ic, x + 14, y + kCellH * 0.5f - 36, 72, 72, kWhite);
            g.text_mid(Font::SemiBold, ts(24), x + 100, y + kCellH * 0.5f, kWhite, Align::Left,
                       fit(g, Font::SemiBold, ts(24), s.title, kCellW - 116));
        }
    }
    if (n == 0 && !(saves_loading() && !wii_scanned_))
    {
        draw_mark(960, kPanelY + kPanelH * 0.42f, 150, with_alpha(kCyan, 0.5f));
        g.text_mid(Font::Bold, ts(32), 960, kPanelY + kPanelH * 0.66f, kWhite, Align::Center, tr("No Wii saves yet"));
        g.text_mid(Font::Regular, ts(24), 960, kPanelY + kPanelH * 0.66f + 46, kLavender, Align::Center,
                   tr("They appear here after you save in a Wii game."));
    }
    else if ((n + kCols - 1) / kCols > kRowsShown)
    {
        /* More rows than fit: a thin bar for where this is. */
        const int rows = (n + kCols - 1) / kCols;
        const float th = kPanelH - kHead - 30, bh = th * float(kRowsShown) / float(rows);
        const float by = kPanelY + kHead + (th - bh) * (wii_scroll_ / float(rows - kRowsShown));
        g.panel(kPanelX + kPanelW - 16, kPanelY + kHead, 6, th, rgba(0x3D5AB0, 0.5f), 1, 3);
        g.panel(kPanelX + kPanelW - 16, by, 6, bh, kCyan, 1, 3);
    }

    /* The focused save, its icon and what it is. */
    const float ix = kPanelX, iy = 812, iw = kPanelW, ih = 128, icy = iy + ih * 0.5f;
    g.panel(ix, iy, iw, ih, rgba(0x0A1236, 0.72f), 0.85f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    if (wii_sel_ < n)
    {
        WiiSave &s = wii_saves_[std::size_t(wii_sel_)];
        float tx = ix + 30;
        if (Texture *ic = wii_save_icon(s))
        {
            g.image(ic, tx, icy - 44, 88, 88, kWhite, 10);
            tx += 112;
        }
        const std::string size = size_text(s.bytes);
        const float right_x = ix + iw - 32, tw = right_x - 300 - tx;
        g.text_mid(Font::Bold, ts(34), tx, icy - 19, kWhite, Align::Left, fit(g, Font::Bold, ts(34), s.title, tw));
        g.text_mid(Font::Regular, ts(25), tx, icy + 23, kLavender, Align::Left,
                   fit(g, Font::Regular, ts(25), s.detail.empty() ? s.game_code : s.detail + "   \xE2\x80\xA2   " + s.game_code, tw));
        g.text_mid(Font::Bold, ts(30), right_x, icy - 19, kWhite, Align::Right,
                   size + "   \xE2\x80\xA2   " + plural(s.files, "1 file", "{n} files"));
        g.text_mid(Font::Regular, ts(25), right_x, icy + 23, kLavender, Align::Right, format_date(s.modified));
    }
    else
        g.text_mid(Font::Regular, ts(26), ix + 30, icy, kLavender, Align::Left,
                   tr("Dolphin keeps Wii saves in /data/porpoise/saves/User/Wii."));
    const std::string note = library_note();
    if (!note.empty())
        g.text_mid(Font::SemiBold, ts(22), 960, iy - 14, kIcy, Align::Center, note);

    char pos[32] = "";
    if (n > 0)
        std::snprintf(pos, sizeof pos, "%02d / %02d", wii_sel_ + 1, n);
    if (wii_sel_ < n)
        draw_prompts({{Glyph::DPad, "Browse"}, {kKeyL2R2, "GameCube cards"}, {Glyph::Circle, "Back"}},
                     {{Glyph::Options, "To USB"}, {Glyph::Square, "Back up"}, {Glyph::Triangle, "Delete"}}, pos);
    else
        draw_prompts({{Glyph::DPad, "Browse"}, {kKeyL2R2, "GameCube cards"}, {Glyph::Circle, "Back"}}, {}, pos);
}
} // namespace porpoise::ui
