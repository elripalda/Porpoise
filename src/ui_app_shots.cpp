/* Porpoise UI - Screenshots: every game's screenshots (Settings >
 * Screenshots) or one game's (its Details), as a grid of pictures; Cross
 * shows one over the whole screen, Triangle deletes it.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cstdio>
#include <ctime>

#include <dirent.h>
#include <sys/stat.h>

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
constexpr int kShotCols = 5, kShotRows = 3;
constexpr float kShotW = 300, kShotH = 200, kShotGapX = 30, kShotGapY = 52;
constexpr float kShotX = (1920 - (kShotCols * kShotW + (kShotCols - 1) * kShotGapX)) * 0.5f, kShotY = 262;

bool is_png(const std::string &n)
{
    return n.size() > 4 && n[0] != '.' && n.compare(n.size() - 4, 4, ".png") == 0;
}
} // namespace

std::vector<App::ShotItem> App::find_shots(const std::string &game_key) const
{
    std::vector<ShotItem> out;
    const std::string root = data_dir_ + "/screenshots";
    std::vector<std::string> keys;
    if (!game_key.empty())
        keys.push_back(game_key);
    else if (DIR *d = opendir(root.c_str()))
    {
        while (dirent *e = readdir(d))
            if (e->d_name[0] != '.')
                keys.push_back(e->d_name);
        closedir(d);
    }
    for (const std::string &key : keys)
    {
        const std::string dir = root + "/" + key;
        DIR *d = opendir(dir.c_str());
        if (!d)
            continue;
        std::string title = key;
        for (const Game &g : lib_->games())
            if (Library::key_of(g) == key)
            {
                title = g.title;
                break;
            }
        while (dirent *e = readdir(d))
        {
            const std::string n = e->d_name;
            if (!is_png(n))
                continue;
            ShotItem s;
            s.path = dir + "/" + n;
            s.thumb = dir + "/.thumbs/" + n;
            s.key = key;
            s.title = title;
            struct stat st;
            if (stat(s.path.c_str(), &st) == 0)
                s.time = (long long)st.st_mtime;
            out.push_back(s);
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end(), [](const ShotItem &a, const ShotItem &b) {
        return a.time != b.time ? a.time > b.time : a.path > b.path;
    });
    return out;
}

int App::count_shots(const Game &game) const
{
    return int(find_shots(Library::key_of(game)).size());
}

void App::free_shots()
{
    for (ShotItem &s : shots_)
        if (s.tex)
            g_->free_texture(s.tex);
    shots_.clear();
    if (shot_full_)
        g_->free_texture(shot_full_);
    shot_full_ = nullptr;
    shot_full_for_.clear();
    shot_full_wait_.clear();
}

void App::open_shots(const std::string &game_key, Screen back_to)
{
    free_shots();
    shots_key_ = game_key;
    shots_ = find_shots(game_key);
    shot_sel_ = 0;
    shot_first_ = 0;
    shot_view_ = false;
    shots_back_ = back_to;
    open_screen(Screen::Shots);
    sfx(Sound::DetailsFlip);
}

App::Action App::update_shots(bool left, bool right, bool up, bool down)
{
    const int n = int(shots_.size());
    shot_sel_ = std::clamp(shot_sel_, 0, std::max(0, n - 1));
    const int before = shot_sel_;
    if (shot_view_)
    {
        /* One at a time: left and right (or L1 / R1) go through them. */
        if ((left || pressed(BtnL1)) && shot_sel_ > 0)
            --shot_sel_;
        if ((right || pressed(BtnR1)) && shot_sel_ + 1 < n)
            ++shot_sel_;
        if (pressed(BtnCircle))
        {
            shot_view_ = false;
            sfx(Sound::DetailsFlip);
            return Action::None;
        }
    }
    else
    {
        if (left && shot_sel_ > 0)
            --shot_sel_;
        if (right && shot_sel_ + 1 < n)
            ++shot_sel_;
        if (up && shot_sel_ >= kShotCols)
            shot_sel_ -= kShotCols;
        if (down && shot_sel_ + kShotCols < n)
            shot_sel_ += kShotCols;
        else if (down && shot_sel_ / kShotCols < (n - 1) / kShotCols)
            shot_sel_ = n - 1;
        if (pressed(BtnCross) && n > 0)
        {
            shot_view_ = true;
            sfx(Sound::DetailsFlip);
        }
        if (pressed(BtnCircle))
        {
            free_shots();
            open_screen(shots_back_);
            sfx(Sound::DetailsFlip);
            shots_total_ = -1; /* counted again */
            if (shots_back_ == Screen::Details && lib_->shown() > 0)
            {
                details_shots_ = count_shots(lib_->games()[std::size_t(selected_)]);
                details_row_ = std::min(details_row_, details_shots_ > 0 ? 4 : 3);
            }
            if (tab_ == Tab::Settings && shots_back_ == Screen::Main)
            {
                const int row = settings_row_;
                build_settings(); /* the count */
                settings_row_ = std::clamp(row, 0, int(rows_.size()) - 1);
            }
            return Action::None;
        }
    }
    if (shot_sel_ != before)
        sfx(shot_view_ ? Sound::GameRow : Sound::MenuScroll);
    if (pressed(BtnTriangle) && n > 0)
        open_dialog(DialogKind::DeleteShot, tr("Delete this screenshot?"),
                    trf("This screenshot of {game} will be deleted.", {{"game", shots_[std::size_t(shot_sel_)].title}}),
                    tr("Delete"), true);
    return Action::None;
}

void App::delete_shot()
{
    if (shot_sel_ < 0 || shot_sel_ >= int(shots_.size()))
        return;
    ShotItem &s = shots_[std::size_t(shot_sel_)];
    std::remove(s.path.c_str());
    std::remove(s.thumb.c_str());
    if (s.tex)
        g_->free_texture(s.tex);
    if (shot_full_ && shot_full_for_ == s.path)
    {
        g_->free_texture(shot_full_);
        shot_full_ = nullptr;
        shot_full_for_.clear();
    }
    shots_.erase(shots_.begin() + shot_sel_);
    shots_total_ = -1;
    shot_sel_ = std::clamp(shot_sel_, 0, std::max(0, int(shots_.size()) - 1));
    if (shots_.empty())
        shot_view_ = false;
}

void App::draw_shots(double time)
{
    (void)time;
    Gfx &g = *g_;
    const int n = int(shots_.size());
    const bool one_game = !shots_key_.empty();
    if (shot_view_ && n > 0)
    {
        /* One screenshot over the whole screen. */
        ShotItem &s = shots_[std::size_t(shot_sel_)];
        if (shot_full_for_ != s.path)
        {
            if (shot_full_)
                g.free_texture(shot_full_);
            shot_full_ = nullptr;
            shot_full_for_ = s.path;
            shot_full_wait_ = s.path;
        }
        if (!shot_full_ && !shot_full_wait_.empty())
        {
            bool pending = false;
            shot_full_ = g.texture_file_async(shot_full_wait_, &pending, 1920);
            if (!pending)
                shot_full_wait_.clear();
        }
        g.set_layer();
        g.panel(0, 0, 1920, 1080, rgba(0x000000, 0.94f), 1, 0);
        Texture *pic = shot_full_ ? shot_full_ : s.tex;
        if (pic && pic->height > 0)
        {
            const float a = float(pic->width) / float(pic->height);
            float w = 1920, h = 1920 / a;
            if (h > 940)
            {
                h = 940;
                w = h * a;
            }
            g.image(pic, 960 - w * 0.5f, 40 + (940 - h) * 0.5f, w, h, kWhite);
        }
        char pos[32];
        std::snprintf(pos, sizeof pos, "%02d / %02d", shot_sel_ + 1, n);
        g.text_mid(Font::Bold, ts(26), 60, 1010 - 0, kWhite, Align::Left,
                   fit(g, Font::Bold, ts(26), s.title + "   \xE2\x80\xA2   " + format_date(s.time), 1100));
        draw_prompts({}, {{Glyph::DPad, "Browse"}, {Glyph::Triangle, "Delete"}, {Glyph::Circle, "Back"}}, pos);
        return;
    }

    g.text(Font::Bold, ts(54), 90, 132, kWhite, Align::Left, tr("Screenshots"));
    g.text(Font::SemiBold, ts(30), 92, 200, kLavender, Align::Left,
           (one_game && n > 0 ? shots_.front().title + "   \xE2\x80\xA2   " : std::string()) +
               plural(n, "1 screenshot", "{n} screenshots"));
    if (n == 0)
    {
        g.panel(460, 360, 1000, 300, rgba(0x13256F, 0.55f), 0.6f, kR, rgba(0x5A8CFF, 0.9f), 2.0f, 0, 0.2f);
        g.text(Font::Bold, ts(36), 960, 440, kWhite, Align::Center, tr("No screenshots yet"));
        g.text(Font::Regular, ts(25), 960, 500, kSoft, Align::Center,
               fit(g, Font::Regular, ts(25), tr("In a game, press touch pad + Square, or open its menu and choose Take Screenshot."), 900));
        g.text(Font::SemiBold, ts(24), 960, 560, kIcy, Align::Center, data_dir_ + "/screenshots");
        draw_prompts({{Glyph::Circle, "Back"}}, {}, "");
        return;
    }
    /* The rows follow the chosen one; pictures load as they come into view
     * and go when they leave it. */
    const int rows_total = (n + kShotCols - 1) / kShotCols;
    shot_first_ = std::clamp(shot_first_, shot_sel_ / kShotCols - (kShotRows - 1), shot_sel_ / kShotCols);
    shot_first_ = std::clamp(shot_first_, 0, std::max(0, rows_total - kShotRows));
    for (int i = 0; i < n; ++i)
    {
        ShotItem &s = shots_[std::size_t(i)];
        const int row = i / kShotCols - shot_first_;
        if (row < 0 || row >= kShotRows)
        {
            if (s.tex && std::abs(row) > kShotRows)
            {
                g.free_texture(s.tex);
                s.tex = nullptr;
                s.tried = false;
                s.wait.clear();
            }
            continue;
        }
        const float x = kShotX + float(i % kShotCols) * (kShotW + kShotGapX), y = kShotY + float(row) * (kShotH + kShotGapY);
        const bool on = i == shot_sel_;
        if (!s.tex)
        {
            if (!s.tried)
            {
                s.tried = true;
                s.wait = s.thumb;
            }
            if (!s.wait.empty())
            {
                bool pending = false;
                s.tex = g.texture_file_async(s.wait, &pending, 400);
                if (!pending)
                {
                    /* No thumbnail (one copied in, or not written): the
                     * picture itself, once. */
                    s.wait = !s.tex && s.wait == s.thumb ? s.path : std::string();
                }
            }
        }
        g.panel(x - 4, y - 4, kShotW + 8, kShotH + 8, rgba(0x0A1236, 0.7f), 1, kR * 0.5f + 4,
                on ? kIcy : rgba(0x3D5AB0, 0.7f), on ? 3.0f : 1.4f, on ? 12 : 0);
        if (s.tex && s.tex->height > 0)
        {
            float uv[4];
            cover_uv(s.tex, kShotW, kShotH, uv);
            g.image_part(s.tex, x, y, kShotW, kShotH, uv, kWhite, kR * 0.5f);
        }
        if (!one_game || on)
            g.text_mid(on ? Font::Bold : Font::SemiBold, ts(20), x + 4, y + kShotH + 22, on ? kWhite : kLavender,
                       Align::Left, fit(g, Font::SemiBold, ts(20), on ? format_date(s.time) : s.title, kShotW - 8));
    }
    if (shot_first_ > 0)
        g.glyph(Glyph::Arrow, 960, kShotY - 40, 24, rgba(0x58B8FF), 0);
    if (shot_first_ + kShotRows < rows_total)
        g.glyph(Glyph::Arrow, 960, kShotY + kShotRows * (kShotH + kShotGapY) - 10, 24, rgba(0x58B8FF), kPi);
    char pos[32];
    std::snprintf(pos, sizeof pos, "%02d / %02d", shot_sel_ + 1, n);
    draw_prompts({{Glyph::DPad, "Browse"}, {Glyph::Cross, "View"}},
                 {{Glyph::Triangle, "Delete"}, {Glyph::Circle, "Back"}}, pos);
}
} // namespace porpoise::ui
