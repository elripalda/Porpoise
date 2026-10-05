/* Porpoise UI - the library's views: cover flow, wheel, disc flow, shelf and
 * box, chosen in Settings > Interface > Library view.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cmath>
#include <ctime>

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
float ease_inout(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

std::string App::game_meta(const Game &g) const
{
    std::string meta = g.platform + "   \xE2\x80\xA2   " + relative_time(g.last_played, (long long)std::time(nullptr));
    if (g.play_seconds >= 60)
        meta += "   \xE2\x80\xA2   " + play_time_text(g.play_seconds);
    return meta;
}

/* Art far from the selection gives its memory back; it reloads when it
 * comes into view again. */
void App::release_far_art(int keep)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    const int shown = lib_->shown();
    const int view = settings_->lib_view;
    for (int i = 0; i < int(games.size()); ++i)
    {
        Game &game = games[std::size_t(i)];
        const bool far = std::abs(i - selected_) > keep || i >= shown;
        if (far && game.cover)
        {
            g.free_texture(game.cover);
            game.cover = nullptr;
            game.cover_tried = false;
        }
        /* Discs, backs and spines are only kept near the selection in the
         * views that show them (Details keeps the chosen game's). */
        const bool near_disc = view == 2 && std::abs(i - selected_) <= 5;
        if (!near_disc && i != selected_ && game.disc)
        {
            g.free_texture(game.disc);
            game.disc = nullptr;
            game.disc_tried = false;
        }
        if (i != selected_ && game.spine)
        {
            g.free_texture(game.spine);
            game.spine = nullptr;
            game.spine_tried = false;
        }
        if (i != selected_ && game.back && screen_ == Screen::Main)
        {
            g.free_texture(game.back);
            game.back = nullptr;
            game.back_tried = false;
        }
    }
}

/* ---- cover flow ------------------------------------------------------------------------- */

void App::draw_cover_flow(double time)
{
    auto &games = lib_->games();
    const int shown = lib_->shown();
    /* Layout of a tile at offset k from the selection (k may be fractional
     * while sliding): x, scale and yaw. */
    auto layout = [](float k, float &x, float &scale, float &yaw) {
        const float a = std::fabs(k), s = k < 0 ? -1.0f : 1.0f;
        if (a <= 1.0f)
        {
            x = kCx + s * a * 372.0f;
            scale = 1.0f - 0.14f * a;
            yaw = s * a * 0.22f;
        }
        else
        {
            x = kCx + s * (372.0f + (a - 1.0f) * 284.0f);
            scale = std::max(0.5f, 0.86f - 0.10f * (a - 1.0f));
            yaw = s * 0.22f;
        }
    };

    /* Back to front: far tiles first, the selection last. */
    std::vector<int> order;
    for (int i = 0; i < shown; ++i)
        if (std::fabs(float(i) - scroll_) < 4.2f)
            order.push_back(i);
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return std::fabs(float(a) - scroll_) > std::fabs(float(b) - scroll_); });
    for (int pass = 0; pass < 2; ++pass) /* reflections, then tiles */
        for (int i : order)
        {
            const float k = float(i) - scroll_;
            float x, s, yaw;
            layout(k, x, s, yaw);
            const float a = std::fabs(k);
            const float alpha = std::clamp(1.0f - (a - 3.0f), 0.0f, 1.0f);
            const bool focused = i == selected_ && a < 0.5f;
            const float lift = focused ? (lift_ * 6.0f) : 0.0f;
            /* The focused block turns gently, showing its glass edge. */
            const float sway =
                settings_->reduced_motion ? 0.0f : std::sin(float(time) * 0.9f) * 0.06f * (1.0f - std::min(a * 2.0f, 1.0f));
            draw_tile(&games[std::size_t(i)], x, kCy - lift, kTileW * s, kTileH * s, yaw + sway, alpha, focused,
                      pass == 0);
        }
}

/* ---- wheel ------------------------------------------------------------------------------ */

/* The boxes stand around a big wheel lying on the floor, facing out; the
 * wheel turns to bring the chosen one to the front. */
void App::draw_wheel(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    const int shown = lib_->shown();
    const float step = 0.36f, radius = 1150.0f, reach = 1.75f;

    /* The wheel's rim on the floor, glowing faintly. */
    const float floor_y = kCy + kTileH * 0.5f + 34;
    g.panel(kCx - 860, floor_y - 46, 1720, 92, kClear, 1, 46, rgba(0x5CD3FF, 0.22f), 1.6f, 10);
    g.panel(kCx - 560, floor_y - 30, 1120, 60, kClear, 1, 30, rgba(0x8BD9FF, 0.16f), 1.2f);

    struct Item
    {
        int i;
        float a, z;
    };
    std::vector<Item> items;
    for (int i = 0; i < shown; ++i)
    {
        const float a = (float(i) - scroll_) * step;
        if (std::fabs(a) < reach)
            items.push_back({i, a, radius * (1.0f - std::cos(a))});
    }
    std::sort(items.begin(), items.end(), [](const Item &x, const Item &y) { return x.z > y.z; });
    for (int pass = 0; pass < 2; ++pass)
        for (const Item &it : items)
        {
            const float s = kFocal / (kFocal + it.z);
            const float x = kCx + radius * std::sin(it.a) * s;
            const float y = kCy + 24 - it.z * 0.16f;
            const float alpha = std::clamp((reach - std::fabs(it.a)) / 0.45f, 0.0f, 1.0f);
            const bool focused = it.i == selected_ && std::fabs(it.a) < step * 0.5f;
            const float lift = focused ? lift_ * 8.0f : 0.0f;
            const float sway = settings_->reduced_motion || !focused ? 0.0f : std::sin(float(time) * 0.9f) * 0.05f;
            draw_tile(&games[std::size_t(it.i)], x, y - lift, kTileW * 0.92f * s, kTileH * 0.92f * s,
                      it.a * 0.9f + sway, alpha, focused, pass == 0);
        }
}

/* ---- discs ------------------------------------------------------------------------------ */

/* One disc: its label art (or a silver disc printed with the cover), the
 * hub, a spinning sheen. spin turns it in its plane; yaw turns it in the
 * room. */
void App::draw_disc(Game *game, float cx, float cy, float d, float spin, float yaw, float alpha, bool focused)
{
    Gfx &g = *g_;
    auto corners = [&](float size, Corner out[4]) {
        const float h = size * 0.5f;
        const float pts[4][2] = {{-h, -h}, {h, -h}, {h, h}, {-h, h}};
        const float c = std::cos(spin), s = std::sin(spin);
        for (int i = 0; i < 4; ++i)
            out[i] = project(cx, cy, pts[i][0] * c - pts[i][1] * s, pts[i][0] * s + pts[i][1] * c, yaw);
    };
    if (focused)
        g.blob(cx, cy, d * 1.5f, d * 1.5f, rgba(0x5CD3FF, 0.18f * alpha));
    /* A soft shadow below. */
    g.blob(cx, cy + d * 0.52f, d * 0.9f, d * 0.12f, rgba(0x000000, 0.45f * alpha));

    Corner c[4];
    corners(d, c);
    Texture *art = game ? disc_of(*game) : nullptr;
    if (art)
        g.quad3d(art, c, d, d, with_alpha(kWhite, alpha), d * 0.5f, false, false);
    else
    {
        /* Silver, with the cover printed on the label. */
        g.quad3d(nullptr, c, d, d, rgba(0xC9D2E0, alpha), d * 0.5f, false, false);
        Corner l[4];
        corners(d * 0.80f, l);
        Texture *cover = game ? cover_of(*game) : nullptr;
        if (cover)
        {
            float uv[4];
            cover_uv(cover, 1, 1, uv);
            g.quad3d(cover, l, d * 0.80f, d * 0.80f, with_alpha(kWhite, alpha), d * 0.40f, false, false, uv);
        }
        else
            g.quad3d(nullptr, l, d * 0.80f, d * 0.80f, rgba(0x16328F, alpha), d * 0.40f, false, false);
    }
    /* The clear ring and the hole. */
    Corner hub[4];
    corners(d * 0.30f, hub);
    g.quad3d(nullptr, hub, d * 0.30f, d * 0.30f, rgba(0xD8E2F0, 0.85f * alpha), d * 0.15f, false, false);
    corners(d * 0.11f, hub);
    g.quad3d(nullptr, hub, d * 0.11f, d * 0.11f, rgba(0x05070D, alpha), d * 0.055f, false, false);
    /* The sheen: it doesn't turn with the disc, the light stays put. */
    Corner flat[4];
    {
        const float h = d * 0.5f;
        flat[0] = project(cx, cy, -h, -h, yaw);
        flat[1] = project(cx, cy, h, -h, yaw);
        flat[2] = project(cx, cy, h, h, yaw);
        flat[3] = project(cx, cy, -h, h, yaw);
    }
    gloss_over(g, flat, d, d, d * 0.5f, cx / 1920.0f + spin * 0.05f, alpha);
}

/* The discs in a row; the chosen one slides out of its box and spins. */
void App::draw_disc_flow(double time)
{
    auto &games = lib_->games();
    const int shown = lib_->shown();
    const float spin = settings_->reduced_motion ? 0.0f : float(time) * 0.55f;
    std::vector<int> order;
    for (int i = 0; i < shown; ++i)
        if (std::fabs(float(i) - scroll_) < 5.2f)
            order.push_back(i);
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return std::fabs(float(a) - scroll_) > std::fabs(float(b) - scroll_); });
    for (int i : order)
    {
        const float k = float(i) - scroll_;
        const float a = std::fabs(k), side = k < 0 ? -1.0f : 1.0f;
        Game *game = &games[std::size_t(i)];
        if (a < 1.0f)
        {
            /* Near the middle: the box behind, the disc sliding out to its
             * right; as it moves off, the box fades and the disc takes its
             * place in the row. */
            const float out = ease_inout(1.0f - a) * std::min(1.0f, lift_ * 1.4f);
            const float bx = kCx - 120.0f * out + side * a * 420.0f;
            const float box_alpha = ease_inout(1.0f - a * 1.6f);
            if (box_alpha > 0.01f)
                draw_tile(game, bx, kCy, kTileW * 0.92f, kTileH * 0.92f, -0.20f * out + side * a * 0.3f, box_alpha,
                          false, false);
            const float d = 250.0f + 160.0f * (1.0f - a) * std::min(1.0f, lift_ * 1.4f);
            const float dx = kCx + 130.0f * out + side * a * (480.0f - 130.0f * out);
            const float dy = kCy + 10.0f + 30.0f * a;
            draw_disc(game, dx, dy, d, spin * out + (1.0f - out) * (spin * 0.25f + float(i)), side * 0.55f * a, 1.0f,
                      i == selected_ && a < 0.5f);
            continue;
        }
        const float x = kCx + side * (480.0f + (a - 1.0f) * 250.0f);
        const float d = std::max(150.0f, 250.0f - (a - 1.0f) * 22.0f);
        const float alpha = std::clamp(1.0f - (a - 4.0f), 0.0f, 1.0f);
        draw_disc(game, x, kCy + 40, d, spin * 0.25f + float(i), side * 0.55f, alpha, false);
    }
}

/* ---- shelf ------------------------------------------------------------------------------ */

/* Rows of boxes, six to a row: many at once. The chosen one comes forward. */
void App::draw_shelf(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    const int shown = lib_->shown();
    const int cols = kShelfCols;
    const float tw = 214, th = 300, gap_x = 50, row_h = 352;
    const float x0 = kCx - (float(cols) * tw + float(cols - 1) * gap_x) * 0.5f + tw * 0.5f;
    const float y0 = 420;

    /* The chosen game's name, top right. */
    if (shown > 0)
    {
        Game &sel = games[std::size_t(selected_)];
        g.text(Font::Bold, ts(40), 1830, 128, kWhite, Align::Right, fit(g, Font::Bold, ts(40), sel.title, 1000));
        g.text(Font::SemiBold, ts(26), 1830, 192, kLavender, Align::Right, game_meta(sel));
    }

    /* Shelves under each row. */
    const int first = std::max(0, int(std::floor(row_scroll_)) - 1);
    const int rows = (shown + cols - 1) / cols;
    for (int r = first; r < std::min(rows, first + 4); ++r)
    {
        const float y = y0 + (float(r) - row_scroll_) * row_h;
        if (y < 140 || y > 1150)
            continue;
        const float fade = std::clamp((1040 - y) / 160.0f, 0.0f, 1.0f) * std::clamp((y - 150) / 120.0f, 0.0f, 1.0f);
        g.panel(x0 - tw * 0.5f - 30, y + th * 0.5f + 14, float(cols) * (tw + gap_x) - gap_x + 60, 16,
                rgba(0x0A1236, 0.65f * fade), 0.7f, 8, rgba(0x5CD3FF, 0.35f * fade), 1.2f);
    }
    std::vector<int> order;
    for (int i = 0; i < shown; ++i)
    {
        const float y = y0 + (float(i / cols) - row_scroll_) * row_h;
        if (y > 140 && y < 1150)
            order.push_back(i);
    }
    /* The chosen one last, over its neighbours. */
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return a != selected_ && b == selected_; });
    for (int i : order)
    {
        const int r = i / cols, c = i % cols;
        const float x = x0 + float(c) * (tw + gap_x);
        const float y = y0 + (float(r) - row_scroll_) * row_h;
        const bool focused = i == selected_;
        const float fade = std::clamp((1040 - y) / 160.0f, 0.0f, 1.0f) * std::clamp((y - 150) / 120.0f, 0.0f, 1.0f);
        const float grow = focused ? 1.0f + 0.10f * std::min(1.0f, lift_) : 1.0f;
        const float sway = focused && !settings_->reduced_motion ? std::sin(float(time) * 0.9f) * 0.05f : 0.0f;
        draw_tile(&games[std::size_t(i)], x, y - (focused ? 14.0f * lift_ : 0.0f), tw * grow, th * grow, sway, fade,
                  focused, false);
    }
}

/* ---- box -------------------------------------------------------------------------------- */

/* A game case: the front cover, the spine down its left side, the back,
 * and the plastic of the open side, each face drawn when it faces you. */
void App::draw_box3d(Game &game, float cx, float cy, float w, float h, float depth, float yaw, float alpha)
{
    Gfx &g = *g_;
    const float hw = w * 0.5f, hh = h * 0.5f;
    const float c = std::cos(yaw), s = std::sin(yaw);
    /* A face is seen when its normal (nx, nz), turned by yaw, points at us (-z). */
    auto facing = [&](float nx, float nz) { return nx * s + nz * c < -0.02f; };
    const Color plastic = rgba(0x0D1220, alpha);

    /* Its shadow on the floor. */
    g.blob(cx, cy + hh + 26, w * 1.3f, 60, rgba(0x000000, 0.5f * alpha));

    if (facing(-1, 0))
    {
        /* The spine: its art, or the cover's left edge. */
        Corner q[4] = {project(cx, cy, -hw, -hh, yaw, depth), project(cx, cy, -hw, -hh, yaw, 0),
                       project(cx, cy, -hw, hh, yaw, 0), project(cx, cy, -hw, hh, yaw, depth)};
        if (Texture *spine = spine_of(game))
            g.quad3d(spine, q, depth, h, with_alpha(kWhite, alpha), 2, false, false);
        else if (Texture *cover = cover_of(game))
        {
            const float uv[4] = {0.0f, 0.0f, 0.07f, 1.0f};
            g.quad3d(cover, q, depth, h, with_alpha(rgba(0xB8C0D0), alpha), 2, false, false, uv);
        }
        else
            g.quad3d(nullptr, q, depth, h, plastic, 2, false, false);
    }
    if (facing(1, 0))
    {
        /* The open side: the case's plastic, the pages' edge inside. */
        Corner q[4] = {project(cx, cy, hw, -hh, yaw, 0), project(cx, cy, hw, -hh, yaw, depth),
                       project(cx, cy, hw, hh, yaw, depth), project(cx, cy, hw, hh, yaw, 0)};
        g.quad3d(nullptr, q, depth, h, plastic, 3, false, false);
        Corner p[4] = {project(cx, cy, hw, -hh + 8, yaw, depth * 0.25f), project(cx, cy, hw, -hh + 8, yaw, depth * 0.75f),
                       project(cx, cy, hw, hh - 8, yaw, depth * 0.75f), project(cx, cy, hw, hh - 8, yaw, depth * 0.25f)};
        g.quad3d(nullptr, p, depth * 0.5f, h - 16, rgba(0xDDE3EC, 0.75f * alpha), 1, false, false);
    }
    if (facing(0, 1))
    {
        Corner q[4] = {project(cx, cy, hw, -hh, yaw, depth), project(cx, cy, -hw, -hh, yaw, depth),
                       project(cx, cy, -hw, hh, yaw, depth), project(cx, cy, hw, hh, yaw, depth)};
        if (Texture *back = back_of(game))
        {
            float uv[4];
            cover_uv(back, w, h, uv);
            g.quad3d(back, q, w, h, with_alpha(kWhite, alpha), 6, false, false, uv);
        }
        else
        {
            g.quad3d(nullptr, q, w, h, rgba(0x13286F, alpha), 6, false, false);
            if (Texture *mark = logo_ ? logo_ : g.brand_mask())
            {
                const float lw = w * 0.42f, lh = lw * float(mark->height) / float(std::max(1, mark->width));
                Corner l[4] = {project(cx, cy, lw * 0.5f, -lh * 0.5f, yaw, depth),
                               project(cx, cy, -lw * 0.5f, -lh * 0.5f, yaw, depth),
                               project(cx, cy, -lw * 0.5f, lh * 0.5f, yaw, depth),
                               project(cx, cy, lw * 0.5f, lh * 0.5f, yaw, depth)};
                g.quad3d(mark, l, lw, lh, with_alpha(logo_ ? kWhite : kCyan, 0.85f * alpha), 0, false, false);
            }
        }
        gloss_over(g, q, w, h, 6, cx / 1920.0f + yaw * 0.1f, alpha);
    }
    if (facing(0, -1))
    {
        Corner q[4] = {project(cx, cy, -hw, -hh, yaw, 0), project(cx, cy, hw, -hh, yaw, 0),
                       project(cx, cy, hw, hh, yaw, 0), project(cx, cy, -hw, hh, yaw, 0)};
        if (Texture *cover = cover_of(game))
        {
            float uv[4];
            cover_uv(cover, w, h, uv);
            g.quad3d(cover, q, w, h, with_alpha(kWhite, alpha), 6, false, false, uv);
        }
        else
        {
            g.quad3d(nullptr, q, w, h, rgba(0x16328F, alpha), 6, false, false);
            draw_mark(cx, cy - h * 0.12f, w * 0.42f, with_alpha(rgba(0x6FD8FF), 0.9f * alpha));
        }
        gloss_over(g, q, w, h, 6, cx / 1920.0f + yaw * 0.1f, alpha);
    }
}

/* One game at a time: its box, big, on the left - the right stick turns it
 * round - and what's known about it on the right. */
void App::draw_box_view(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    const int shown = lib_->shown();
    if (shown == 0)
        return;
    const float k = scroll_ - float(selected_); /* -1..1 while sliding */
    const float bw = 400, bh = bw * 190.0f / 135.0f, depth = 58;
    const float sway = settings_->reduced_motion ? 0.0f : std::sin(float(time) * 0.6f) * 0.10f;
    const float yaw = view_yaw_ + (std::fabs(view_yaw_) < 0.05f ? sway : 0.0f) + 0.48f;
    /* Neighbours, small and faint, to show there are more. */
    for (int n : {selected_ - 1, selected_ + 1})
        if (n >= 0 && n < shown)
        {
            const float side = n < selected_ ? -1.0f : 1.0f;
            const float x = 740 + side * 560 - k * 560;
            if (side > 0 && x > 1000)
                continue; /* the details panel is there */
            draw_tile(&games[std::size_t(n)], x, kCy + 40, kTileW * 0.5f, kTileH * 0.5f, side * 0.5f, 0.35f, false,
                      false);
        }
    Game &sel = games[std::size_t(selected_)];
    draw_box3d(sel, 740 - k * 560, kCy + 20, bw, bh, depth, yaw, std::clamp(1.0f - std::fabs(k), 0.0f, 1.0f));

    /* What's known about it. */
    const float px = 1150, py = 250, pw = 690, ph = 640;
    g.panel(px, py, pw, ph, rgba(0x0F1F63, 0.62f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    float y = py + 40;
    for (const std::string &line : wrap(g, Font::Bold, ts(40), sel.title, pw - 80, 2))
    {
        g.text(Font::Bold, ts(40), px + 40, y, kWhite, Align::Left, line);
        y += ts(40) * 1.18f;
    }
    if (sel.favourite)
        g.glyph(Glyph::Star, px + pw - 40, py + 60, 34, rgba(0xFFD45C));
    y += 8;
    g.text(Font::SemiBold, ts(25), px + 40, y, kLavender, Align::Left, game_meta(sel));
    y += 56;
    std::vector<std::pair<std::string, std::string>> facts;
    if (!sel.developer.empty())
        facts.push_back({tr("Developer"), sel.developer});
    if (!sel.publisher.empty() && sel.publisher != sel.developer)
        facts.push_back({tr("Publisher"), sel.publisher});
    if (!sel.released.empty())
        facts.push_back({tr("Released"), sel.released});
    if (sel.players > 0)
        facts.push_back({tr("Players"), std::to_string(sel.players)});
    for (const auto &f : facts)
    {
        g.text(Font::SemiBold, ts(24), px + 40, y, kSoft, Align::Left, f.first);
        g.text(Font::Regular, ts(24), px + 250, y, kWhite, Align::Left, fit(g, Font::Regular, ts(24), f.second, pw - 290));
        y += 38;
    }
    if (!sel.synopsis.empty())
    {
        y += 14;
        const int room = std::max(1, int((py + ph - 40 - y) / (ts(24) * 1.35f)));
        for (const std::string &line : wrap(g, Font::Regular, ts(24), sel.synopsis, pw - 80, std::size_t(room)))
        {
            g.text(Font::Regular, ts(24), px + 40, y, kSoft, Align::Left, line);
            y += ts(24) * 1.35f;
        }
    }
    else if (facts.empty())
        g.text(Font::Regular, ts(24), px + 40, y, kSoft, Align::Left,
               fit(g, Font::Regular, ts(24), tr("Turn the box round with the right stick."), pw - 80));
}

/* Moving through the library in the views that move differently: the shelf
 * goes by rows, the box turns with the right stick. True when it moved. */
bool App::update_view_nav(bool left, bool right, bool up, bool down, double dt)
{
    const int shown = lib_->shown();
    const int view = settings_->lib_view;
    bool moved = false;
    if (left && selected_ > 0)
    {
        --selected_;
        moved = true;
    }
    if (right && selected_ + 1 < shown)
    {
        ++selected_;
        moved = true;
    }
    if (view == 3)
    {
        if (up && selected_ >= kShelfCols)
        {
            selected_ -= kShelfCols;
            moved = true;
        }
        if (down && selected_ + kShelfCols < shown)
        {
            selected_ += kShelfCols;
            moved = true;
        }
        else if (down && selected_ / kShelfCols < (shown - 1) / kShelfCols)
        {
            selected_ = shown - 1; /* the last row is short */
            moved = true;
        }
        const float target = std::max(0.0f, float(selected_ / kShelfCols) - 0.0f);
        row_scroll_ = settings_->reduced_motion ? target : smooth(row_scroll_, target, dt, 12.0f);
    }
    if (view == 4)
    {
        /* The right stick turns the box; let go, and it comes back round
         * to its front after a while. */
        if (std::fabs(right_x_) > 0.15f)
        {
            view_yaw_ += right_x_ * float(dt) * 3.4f;
            view_idle_ = 0;
        }
        else
        {
            view_idle_ += dt;
            if (view_idle_ > 3.0)
            {
                const float front = std::round(view_yaw_ / (2.0f * kPi)) * 2.0f * kPi;
                view_yaw_ = smooth(view_yaw_, front, dt, 3.0f);
            }
        }
        if (moved)
        {
            view_yaw_ = 0;
            view_idle_ = 0;
        }
    }
    return moved;
}
} // namespace porpoise::ui
