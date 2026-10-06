/* Porpoise UI - a game's RetroAchievements: the in-game menu's Achievements
 * tab, and the library's Achievements page (Square on a game's Details).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A list on the left, in the menu's glass: the game's badge and how much of
 * the set is unlocked, then every achievement with its badge, its title and
 * description and its points, unlocked ones bright and the rest dimmed with a
 * lock. On the right, the one in focus, large. */
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

#include "porpoise_pad.hpp"
#include "ui_achievements.hpp"
#include "ui_app.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

/* ---- the text the core gives ------------------------------------------------------------- */

namespace
{
std::vector<std::string> split_tabs(const std::string &line)
{
    std::vector<std::string> out;
    std::size_t at = 0;
    for (;;)
    {
        const std::size_t tab = line.find('\t', at);
        out.push_back(line.substr(at, tab == std::string::npos ? std::string::npos : tab - at));
        if (tab == std::string::npos)
            break;
        at = tab + 1;
    }
    return out;
}

const Color kGold = rgba(0xFFD45C);

std::string unlock_date(long long when)
{
    if (when <= 0)
        return "";
    const std::time_t t = std::time_t(when);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", &tm);
    return buf;
}
} // namespace

bool parse_achievements(const std::string &text, AchievementSet &out)
{
    out = AchievementSet{};
    std::size_t at = 0;
    while (at < text.size())
    {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        const std::vector<std::string> f = split_tabs(text.substr(at, end - at));
        at = end + 1;
        if (f.empty())
            continue;
        if (f[0] == "G" && f.size() >= 8)
        {
            out.game_id = unsigned(std::strtoul(f[1].c_str(), nullptr, 10));
            out.title = f[2];
            out.badge_url = f[3];
            out.unlocked = std::atoi(f[4].c_str());
            out.total = std::atoi(f[5].c_str());
            out.points = std::atoi(f[6].c_str());
            out.points_total = std::atoi(f[7].c_str());
        }
        else if (f[0] == "A" && f.size() >= 12)
        {
            Achievement a;
            a.id = unsigned(std::strtoul(f[1].c_str(), nullptr, 10));
            a.points = std::atoi(f[2].c_str());
            a.unlocked = f[3] == "1";
            a.unlock_time = std::atoll(f[4].c_str());
            a.type = std::atoi(f[5].c_str());
            a.rarity = float(std::atof(f[6].c_str()));
            a.progress = f[7];
            a.title = f[8];
            a.description = f[9];
            a.badge_url = f[10];
            a.badge_locked_url = f[11];
            out.list.push_back(a);
        }
    }
    /* Unlocked first, newest first; then the rest in the set's own order. */
    std::stable_sort(out.list.begin(), out.list.end(), [](const Achievement &a, const Achievement &b) {
        if (a.unlocked != b.unlocked)
            return a.unlocked;
        return a.unlocked && a.unlock_time > b.unlock_time;
    });
    return out.valid();
}

std::string badge_file(const std::string &url)
{
    const std::size_t slash = url.rfind('/');
    std::string name = slash == std::string::npos ? url : url.substr(slash + 1);
    for (char &c : name)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-'))
            c = '_';
    return name.empty() || name.size() > 64 ? std::string() : name;
}

/* ---- loading ----------------------------------------------------------------------------- */

void App::load_achievements(const std::string &disc_id, bool live)
{
    ach_ = ach_source_ ? ach_source_(disc_id, live) : AchievementSet{};
    ach_for_ = disc_id;
    ach_live_ = live;
    ach_loaded_at_ = time_;
    ach_focus_ = std::clamp(ach_focus_, 0, std::max(0, int(ach_.list.size()) - 1));
}

Texture *App::badge_tex(const std::string &url)
{
    const std::string name = badge_file(url);
    if (name.empty())
        return nullptr;
    const std::string path = data_dir_ + "/achievements/badges/" + name;
    auto it = ach_tex_.find(path);
    if (it != ach_tex_.end() && it->second)
        return it->second;
    struct stat st{};
    if (stat(path.c_str(), &st) != 0 || st.st_size <= 0)
        return nullptr; /* not downloaded yet */
    bool pending = false;
    Texture *t = g_->texture_file_async(path, &pending, 256);
    if (t || !pending)
        ach_tex_[path] = t;
    return t;
}

void App::free_badges()
{
    for (auto &kv : ach_tex_)
        if (kv.second)
            g_->free_texture(kv.second);
    ach_tex_.clear();
}

bool App::achievements_nav(bool up, bool down)
{
    const int n = int(ach_.list.size());
    if (n == 0)
        return false;
    const int before = ach_focus_;
    if (up)
        ach_focus_ = std::max(0, ach_focus_ - 1);
    if (down)
        ach_focus_ = std::min(n - 1, ach_focus_ + 1);
    if (pressed(BtnL2))
        ach_focus_ = std::max(0, ach_focus_ - 6);
    if (pressed(BtnR2))
        ach_focus_ = std::min(n - 1, ach_focus_ + 6);
    return ach_focus_ != before;
}

/* ---- drawing ----------------------------------------------------------------------------- */

void App::draw_badge(const std::string &url, bool unlocked, float x, float y, float size)
{
    Gfx &g = *g_;
    Texture *t = badge_tex(url);
    if (t)
        g.image(t, x, y, size, size, unlocked ? kWhite : rgba(0xFFFFFF, 0.32f), size * 0.12f);
    else
        g.panel(x, y, size, size, rgba(0x07102E, 0.7f), 1, size * 0.12f, rgba(0x3D5AB0, 0.8f), 1.4f);
    if (!unlocked)
    {
        /* A small lock, drawn: a body and its shackle. */
        const float s = std::min(size * 0.34f, 34.0f), lx = x + size - s * 0.78f, ly = y + size - s * 0.62f;
        g.panel(lx - s * 0.42f, ly - s * 0.08f, s * 0.84f, s * 0.62f, rgba(0x0B1440, 0.95f), 1, s * 0.12f,
                rgba(0xC9D6FF, 0.95f), 1.6f);
        g.panel(lx - s * 0.26f, ly - s * 0.44f, s * 0.52f, s * 0.48f, kClear, 1, s * 0.24f,
                rgba(0xC9D6FF, 0.95f), 2.0f);
    }
}

void App::draw_achievement_list(float x, float y, float w, float h)
{
    Gfx &g = *g_;
    const AchievementSet &s = ach_;
    /* The set: its badge, how far along, a bar. */
    const float bs = 64;
    draw_badge(s.badge_url, true, x, y, bs);
    g.text_mid(Font::Bold, ts(27), x + bs + 20, y + 18, kWhite, Align::Left,
               trf("{done} of {total} unlocked", {{"done", std::to_string(s.unlocked)}, {"total", std::to_string(s.total)}}));
    g.text_mid(Font::SemiBold, ts(21), x + bs + 20, y + 48, kLavender, Align::Left,
               trf("{done} of {total} points", {{"done", std::to_string(s.points)}, {"total", std::to_string(s.points_total)}}));
    const float bar_x = x + bs + 20, bar_w = w - bs - 20, bar_y = y + 70;
    g.panel(bar_x, bar_y, bar_w, 8, rgba(0x07102E, 0.8f), 1, 4);
    if (s.total > 0)
        g.panel(bar_x, bar_y, std::max(8.0f, bar_w * float(s.unlocked) / float(s.total)), 8, kGold, 1, 4);

    /* The list, scrolled to keep the focus in view. */
    const float list_y = y + 100, row_h = 88;
    const int n = int(s.list.size());
    const int fit_rows = std::max(1, int((h - (list_y - y)) / row_h));
    const float target = float(std::clamp(ach_focus_ - fit_rows / 2, 0, std::max(0, n - fit_rows)));
    ach_scroll_ = target;
    const int first = std::max(0, int(std::floor(ach_scroll_)));
    for (int i = first; i < n && i < first + fit_rows + 1; ++i)
    {
        const float ry = list_y + (float(i) - ach_scroll_) * row_h;
        if (ry < list_y - 2 || ry + row_h > y + h + 2)
            continue;
        const Achievement &a = s.list[std::size_t(i)];
        const bool on = i == ach_focus_;
        if (on)
            g.panel(x - 12, ry + 2, w + 24, row_h - 4, rgba(0x1D45B8, 0.9f), 0.7f, kR, kIcy, 2.2f, 8, 0.18f);
        draw_badge(a.badge_url, a.unlocked, x, ry + 10, 66);
        const float tx = x + 86, tw = w - 86 - 110;
        g.text_mid(Font::Bold, ts(24), tx, ry + 30, a.unlocked ? kWhite : (on ? kWhite : kSoft), Align::Left,
                   fit(g, Font::Bold, ts(24), a.title, tw));
        g.text_mid(Font::Regular, ts(20), tx, ry + 60, a.unlocked ? kSoft : with_alpha(kLavender, 0.85f), Align::Left,
                   fit(g, Font::Regular, ts(20), a.description, tw));
        g.text_mid(Font::Bold, ts(24), x + w - 6, ry + 30, a.unlocked ? kGold : with_alpha(kGold, 0.55f), Align::Right,
                   std::to_string(a.points));
        const std::string side = a.unlocked ? unlock_date(a.unlock_time) : a.progress;
        if (!side.empty())
            g.text_mid(Font::SemiBold, ts(18), x + w - 6, ry + 60, a.unlocked ? kCyan : kLavender, Align::Right, side);
    }
    if (n == 0)
        g.text_mid(Font::Regular, ts(24), x, list_y + 40, kLavender, Align::Left, tr("No achievements to show yet."));
}

void App::draw_achievement_card(float x, float y, float w, float h)
{
    Gfx &g = *g_;
    if (ach_.list.empty())
        return;
    const Achievement &a = ach_.list[std::size_t(std::clamp(ach_focus_, 0, int(ach_.list.size()) - 1))];
    /* As tall as what it holds, centred where it was given room. */
    const auto title = wrap(g, Font::Bold, ts(36), a.title, w - 96, 2);
    const auto desc = wrap(g, Font::Regular, ts(26), a.description, w - 110, 4);
    const float bs = 200;
    const float content = 56 + bs + 58 + float(title.size()) * 46 + 6 + float(desc.size()) * 36 + 22 + 48 + 40 +
                          (a.rarity > 0.0f ? 34.0f : 0.0f) + (a.type == 1 && !a.unlocked ? 34.0f : 0.0f) + 30;
    const float ch = std::min(h, std::max(520.0f, content));
    y += (h - ch) * 0.5f;
    h = ch;
    Glass face;
    face.tint = rgba(0x13308A, 0.86f);
    face.rim = a.unlocked ? kGold : rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.2f;
    face.glow = a.unlocked ? 14 : 8;
    face.phase = 0.3f;
    glass_block(g, x + w * 0.5f, y + h * 0.5f, w, h, 18, 0, 0, 30, face);
    draw_badge(a.badge_url, a.unlocked, x + (w - bs) * 0.5f, y + 56, bs);
    float ly = y + 56 + bs + 58;
    for (const std::string &l : title)
    {
        g.text_mid(Font::Bold, ts(36), x + w * 0.5f, ly, kWhite, Align::Center, l);
        ly += 46;
    }
    ly += 6;
    for (const std::string &l : desc)
    {
        g.text_mid(Font::Regular, ts(26), x + w * 0.5f, ly, kSoft, Align::Center, l);
        ly += 36;
    }
    ly += 22;
    g.text_mid(Font::Bold, ts(30), x + w * 0.5f, ly, kGold, Align::Center,
               plural((long long)a.points, "1 point", "{n} points"));
    ly += 48;
    std::string state = a.unlocked ? trf("Unlocked {date}", {{"date", unlock_date(a.unlock_time)}})
                                   : a.progress.empty() ? tr("Locked")
                                                        : trf("Locked \xE2\x80\xA2 {progress}", {{"progress", a.progress}});
    g.text_mid(Font::SemiBold, ts(26), x + w * 0.5f, ly, a.unlocked ? kCyan : kLavender, Align::Center, state);
    ly += 40;
    if (a.rarity > 0.0f)
    {
        char pct[16];
        std::snprintf(pct, sizeof pct, "%.1f", double(a.rarity));
        g.text_mid(Font::Regular, ts(22), x + w * 0.5f, ly, kLavender, Align::Center,
                   trf("{pct}% of players have it", {{"pct", pct}}));
        ly += 34;
    }
    if (a.type == 1 && !a.unlocked)
        g.text_mid(Font::SemiBold, ts(22), x + w * 0.5f, ly, rgba(0xFFC266), Align::Center,
                   tr("Missable: it can be missed for good in a playthrough."));
}

/* ---- the library's page ------------------------------------------------------------------ */

const AchievementSet &App::details_achievements(const Game &game)
{
    if (details_ach_for_ != game.id)
    {
        details_ach_for_ = game.id;
        details_ach_ = ach_source_ && !game.id.empty() ? ach_source_(game.id, false) : AchievementSet{};
    }
    return details_ach_;
}

void App::open_achievements(const Game &game)
{
    ach_focus_ = 0;
    ach_scroll_ = 0;
    load_achievements(game.id, false);
    if (!ach_.valid())
        return;
    ach_back_ = screen_;
    open_screen(Screen::Achievements);
    sfx(Sound::DetailsFlip);
}

App::Action App::update_achievements_screen(bool up, bool down)
{
    if (achievements_nav(up, down))
        sfx(Sound::MenuScroll);
    if (pressed(BtnCircle | BtnSquare))
    {
        free_badges();
        open_screen(ach_back_);
        sfx(Sound::DetailsFlip);
    }
    return Action::None;
}

void App::draw_achievements_screen()
{
    Gfx &g = *g_;
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.6f), 1, 0);
    g.set_layer(layer_dx_, layer_dy_, layer_fade_);
    const float x = 60, y = 50, w = 980, h = 940;
    Glass face;
    face.tint = rgba(0x13308A, 0.88f);
    face.rim = rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.2f;
    face.glow = 12;
    face.phase = 0.2f;
    glass_block(g, x + w * 0.5f, y + h * 0.5f, w, h, 22, 0, 0, 36, face);
    g.text_mid(Font::SemiBold, ts(21), x + 48, y + 46, kCyan, Align::Left, "RETROACHIEVEMENTS", 3.0f);
    g.text_mid(Font::Bold, ts(34), x + 48, y + 92, kWhite, Align::Left,
               fit(g, Font::Bold, ts(34), ach_.title, w - 96));
    draw_achievement_list(x + 48, y + 136, w - 96, h - 136 - 40);
    draw_achievement_card(1100, 120, 760, 800);
    g.set_layer();
    drawing_dialog_ = true;
    draw_prompts({{Glyph::DPad, "Browse"}, {kKeyL2R2, "Page"}}, {{Glyph::Circle, "Back"}},
                 tr("As of the last time you played it"));
    drawing_dialog_ = false;
}
} // namespace porpoise::ui
