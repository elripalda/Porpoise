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
    /* A WAD or an app says what it is (WiiWare, Virtual Console, ...). */
    std::string meta = (g.kind.empty() ? g.platform : tr(g.kind)) + "   \xE2\x80\xA2   " +
                       relative_time(g.last_played, (long long)std::time(nullptr));
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
    const int view = starcube() ? (settings_->sc_games == 0 ? 2 : 0) : settings_->lib_view;
    const int disc_keep = starcube() ? keep : 5;
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
        const bool near_disc = view == 2 && std::abs(i - selected_) <= disc_keep;
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

/* A game case: black plastic, with the printed paper insert under its clear
 * sleeve - the cover on the front, the spine down the left, the back behind -
 * and plastic on the open side, top and bottom. A face is drawn when its
 * corners, as projected, turn clockwise (it faces you), so at any angle each
 * shows cleanly and none bleeds through another. */
void App::draw_box3d(Game &game, float cx, float cy, float w, float h, float depth, float yaw, float alpha)
{
    Gfx &g = *g_;
    const bool toned = g.toned();
    g.set_tone(false); /* the case is black in every theme */
    const float hw = w * 0.5f, hh = h * 0.5f;
    struct P
    {
        float x, y, z;
    };
    auto corner = [&](P p) { return project(cx, cy, p.x, p.y, yaw, p.z); };
    auto seen = [](const Corner q[4]) {
        float area = 0;
        for (int i = 0; i < 4; ++i)
        {
            const Corner &a = q[i], &b = q[(i + 1) % 4];
            area += a.x * b.y - b.x * a.y;
        }
        return area > 1.0f;
    };
    /* A face's corners (top left, top right, bottom right, bottom left as
     * seen from outside), and a rectangle inset into it by (iu, iv). */
    auto face = [&](const P f[4], float iu, float iv, Corner out[4]) {
        const P &tl = f[0], &tr = f[1], &bl = f[3];
        const P du{tr.x - tl.x, tr.y - tl.y, tr.z - tl.z}, dv{bl.x - tl.x, bl.y - tl.y, bl.z - tl.z};
        const float lu = std::sqrt(du.x * du.x + du.y * du.y + du.z * du.z);
        const float lv = std::sqrt(dv.x * dv.x + dv.y * dv.y + dv.z * dv.z);
        const float u0 = lu > 0 ? iu / lu : 0, v0 = lv > 0 ? iv / lv : 0;
        const float uv[4][2] = {{u0, v0}, {1 - u0, v0}, {1 - u0, 1 - v0}, {u0, 1 - v0}};
        for (int i = 0; i < 4; ++i)
            out[i] = corner(P{tl.x + du.x * uv[i][0] + dv.x * uv[i][1], tl.y + du.y * uv[i][0] + dv.y * uv[i][1],
                              tl.z + du.z * uv[i][0] + dv.z * uv[i][1]});
    };
    const Color black = Color{0.035f, 0.038f, 0.045f, alpha};
    const Color edge_light = Color{0.16f, 0.17f, 0.19f, alpha};

    /* Its shadow on the floor. */
    g.blob(cx, cy + hh + 26, w * 1.3f, 60, rgba(0x000000, 0.5f * alpha));

    const P front[4] = {{-hw, -hh, 0}, {hw, -hh, 0}, {hw, hh, 0}, {-hw, hh, 0}};
    const P back[4] = {{hw, -hh, depth}, {-hw, -hh, depth}, {-hw, hh, depth}, {hw, hh, depth}};
    const P spine[4] = {{-hw, -hh, depth}, {-hw, -hh, 0}, {-hw, hh, 0}, {-hw, hh, depth}};
    const P open_side[4] = {{hw, -hh, 0}, {hw, -hh, depth}, {hw, hh, depth}, {hw, hh, 0}};
    const P top[4] = {{-hw, -hh, depth}, {hw, -hh, depth}, {hw, -hh, 0}, {-hw, -hh, 0}};
    const P bottom[4] = {{-hw, hh, 0}, {hw, hh, 0}, {hw, hh, depth}, {-hw, hh, depth}};
    const float paper = 7; /* the plastic showing round the insert */

    Corner q[4];
    for (const P *f : {top, bottom})
    {
        for (int i = 0; i < 4; ++i)
            q[i] = corner(f[i]);
        if (seen(q))
            g.quad3d(nullptr, q, w, depth, black, 2, false, false);
    }
    for (int i = 0; i < 4; ++i)
        q[i] = corner(open_side[i]);
    if (seen(q))
    {
        /* The open side: plastic, with the ridge of its clasp catching the light. */
        g.quad3d(nullptr, q, depth, h, black, 3, false, false);
        Corner r[4];
        face(open_side, depth * 0.40f, h * 0.30f, r);
        g.quad3d(nullptr, r, depth * 0.2f, h * 0.4f, edge_light, 1, false, false);
    }
    for (int i = 0; i < 4; ++i)
        q[i] = corner(spine[i]);
    if (seen(q))
    {
        g.quad3d(nullptr, q, depth, h, black, 2, false, false);
        Corner in[4];
        face(spine, 3, paper, in);
        if (Texture *art = spine_of(game))
            g.quad3d(art, in, depth - 6, h - paper * 2, with_alpha(kWhite, alpha), 1, false, false);
        else if (Texture *cover = cover_of(game))
        {
            const float uv[4] = {0.0f, 0.0f, 0.06f, 1.0f};
            g.quad3d(cover, in, depth - 6, h - paper * 2, Color{0.62f, 0.64f, 0.70f, alpha}, 1, false, false, uv);
        }
        gloss_over(g, q, depth, h, 2, cx / 1920.0f + yaw * 0.1f, alpha * 0.6f);
    }
    for (int i = 0; i < 4; ++i)
        q[i] = corner(back[i]);
    if (seen(q))
    {
        g.quad3d(nullptr, q, w, h, black, 6, false, false);
        Corner in[4];
        face(back, paper, paper, in);
        if (Texture *art = back_of(game))
        {
            float uv[4];
            cover_uv(art, w - paper * 2, h - paper * 2, uv);
            g.quad3d(art, in, w - paper * 2, h - paper * 2, with_alpha(kWhite, alpha), 3, false, false, uv);
        }
        else if (Texture *mark = logo_ ? logo_ : g.brand_mask())
        {
            const float lw = w * 0.42f, lh = lw * float(mark->height) / float(std::max(1, mark->width));
            Corner l[4] = {corner(P{lw * 0.5f, -lh * 0.5f, depth}), corner(P{-lw * 0.5f, -lh * 0.5f, depth}),
                           corner(P{-lw * 0.5f, lh * 0.5f, depth}), corner(P{lw * 0.5f, lh * 0.5f, depth})};
            g.quad3d(mark, l, lw, lh, with_alpha(logo_ ? kWhite : kCyan, 0.85f * alpha), 0, false, false);
        }
        gloss_over(g, q, w, h, 6, cx / 1920.0f + yaw * 0.1f, alpha);
    }
    for (int i = 0; i < 4; ++i)
        q[i] = corner(front[i]);
    if (seen(q))
    {
        g.quad3d(nullptr, q, w, h, black, 6, false, false);
        Corner in[4];
        face(front, paper, paper, in);
        if (Texture *cover = cover_of(game))
        {
            float uv[4];
            cover_uv(cover, w - paper * 2, h - paper * 2, uv);
            g.quad3d(cover, in, w - paper * 2, h - paper * 2, with_alpha(kWhite, alpha), 3, false, false, uv);
        }
        else
        {
            g.quad3d(nullptr, in, w - paper * 2, h - paper * 2, rgba(0x16328F, alpha), 3, false, false);
            draw_mark(cx, cy - h * 0.12f, w * 0.42f, with_alpha(rgba(0x6FD8FF), 0.9f * alpha));
        }
        gloss_over(g, q, w, h, 6, cx / 1920.0f + yaw * 0.1f, alpha);
    }
    g.set_tone(toned);
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

/* ---- list ------------------------------------------------------------------------------- */

/* Your games by name, down the right; the chosen one's box on the left. */
void App::draw_list_view(double time)
{
    Gfx &g = *g_;
    auto &games = lib_->games();
    const int shown = lib_->shown();
    if (shown == 0)
        return;
    Game &sel = games[std::size_t(selected_)];
    const float sway = settings_->reduced_motion ? 0.0f : std::sin(float(time) * 0.6f) * 0.08f;
    draw_tile(&sel, 470, kCy + 30, kTileW * 1.05f, kTileH * 1.05f, 0.18f + sway, 1.0f, true, false);
    g.text(Font::Bold, ts(40), 470, 840, kWhite, Align::Center, fit(g, Font::Bold, ts(40), sel.title, 640));
    g.text(Font::SemiBold, ts(24), 470, 900, kLavender, Align::Center, game_meta(sel));

    const float lx = 860, ly = 250, lw = 980, row_h = 76;
    const int visible = 9;
    /* The list slides so the chosen row stays near the middle. */
    const float top = std::clamp(scroll_ - 3.5f, 0.0f, std::max(0.0f, float(shown - visible)));
    g.panel(lx, ly - 14, lw, row_h * visible + 28, rgba(0x0F1F63, 0.55f), 0.75f, kR, rgba(0x4C6FD8, 0.8f), 1.6f, 0, 0.1f);
    for (int i = std::max(0, int(top) - 1); i < std::min(shown, int(top) + visible + 1); ++i)
    {
        const float y = ly + (float(i) - top) * row_h;
        if (y < ly - row_h * 0.5f || y > ly + row_h * (visible - 0.5f))
            continue;
        const float edge = std::clamp(std::min(y - (ly - row_h * 0.5f), ly + row_h * (visible - 0.5f) - y) / row_h, 0.0f, 1.0f);
        Game &game = games[std::size_t(i)];
        const bool on = i == selected_;
        if (on)
            g.panel(lx + 12, y, lw - 24, row_h - 8, rgba(0x1F63F0, 0.9f), 0.62f, kR, rgba(0x7FD9FF), 1.6f, 6, 0.3f);
        if (Texture *cover = cover_of(game))
            g.image(cover, lx + 28, y + 4, 44, 60, with_alpha(kWhite, edge), 4);
        else
            g.panel(lx + 28, y + 4, 44, 60, with_alpha(kTileFill, 0.55f * edge), 0.6f, 4, with_alpha(kEdge, 0.6f * edge),
                    1.2f);
        g.text_mid(on ? Font::Bold : Font::SemiBold, ts(27), lx + 92, y + (row_h - 8) * 0.5f, with_alpha(kWhite, edge),
                   Align::Left, fit(g, Font::Bold, ts(27), game.title, lw - 380));
        if (game.favourite)
            g.glyph(Glyph::Star, lx + lw - 250, y + (row_h - 8) * 0.5f, 26, rgba(0xFFD45C, edge));
        g.text_mid(Font::Regular, ts(22), lx + lw - 36, y + (row_h - 8) * 0.5f, with_alpha(on ? kWhite : kLavender, edge),
                   Align::Right, game.kind.empty() ? game.platform : tr(game.kind));
    }
}

/* ---- stack ------------------------------------------------------------------------------ */

/* A deck: the chosen box in front, the next ones behind it, rising away; the
 * ones gone by drop forward out of sight. */
void App::draw_stack(double time)
{
    auto &games = lib_->games();
    const int shown = lib_->shown();
    std::vector<int> order;
    for (int i = 0; i < shown; ++i)
    {
        const float k = float(i) - scroll_;
        if (k > -1.2f && k < 6.0f)
            order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) { return a > b; }); /* far first */
    for (int i : order)
    {
        const float k = float(i) - scroll_;
        float x = kCx, y = kCy - 10, s = 1.0f, yaw = -0.12f, alpha = 1.0f;
        if (k >= 0)
        {
            x += k * 92.0f;
            y -= k * 26.0f;
            s = 1.0f - k * 0.075f;
            yaw = -0.12f - k * 0.04f;
            alpha = std::clamp(1.0f - (k - 3.5f) * 0.5f, 0.0f, 1.0f) * (1.0f - k * 0.08f);
        }
        else
        {
            /* Going: it tips forward and drops. */
            const float a = -k;
            y += a * 420.0f;
            x -= a * 120.0f;
            yaw = -0.12f - a * 0.6f;
            alpha = std::clamp(1.0f - a * 1.2f, 0.0f, 1.0f);
        }
        const bool focused = i == selected_ && std::fabs(k) < 0.5f;
        const float sway = focused && !settings_->reduced_motion ? std::sin(float(time) * 0.9f) * 0.05f : 0.0f;
        draw_tile(&games[std::size_t(i)], x, y, kTileW * s, kTileH * s, yaw + sway, alpha, focused, false);
    }
}

/* ---- helix ------------------------------------------------------------------------------ */

/* The boxes climb round a column, turning to bring the chosen one round. */
void App::draw_helix(double time)
{
    Gfx &g = *g_;
    (void)time;
    auto &games = lib_->games();
    const int shown = lib_->shown();
    const float step = 0.85f, radius = 560.0f, rise = 118.0f;
    /* The column. */
    g.panel(kCx - 5, 200, 10, 560, rgba(0x5CD3FF, 0.14f), 1, 6, rgba(0x8BD9FF, 0.25f), 1.0f, 8);
    struct Item
    {
        int i;
        float a, z;
    };
    std::vector<Item> items;
    for (int i = 0; i < shown; ++i)
    {
        const float k = float(i) - scroll_;
        if (std::fabs(k) < 4.0f)
            items.push_back({i, k * step, radius * (1.0f - std::cos(k * step))});
    }
    std::sort(items.begin(), items.end(), [](const Item &x, const Item &y) { return x.z > y.z; });
    for (const Item &it : items)
    {
        const float k = float(it.i) - scroll_;
        const float s = kFocal / (kFocal + it.z);
        const float x = kCx + radius * std::sin(it.a) * s;
        const float y = kCy - 10 - k * rise * s;
        /* Fade before the top bar or the title below. */
        const float room = std::clamp(std::min(y - 250.0f, 700.0f - y) / 90.0f + 1.0f, 0.0f, 1.0f);
        const float alpha = room * std::clamp((4.0f - std::fabs(k)) / 1.2f, 0.0f, 1.0f) * (it.z > radius ? 0.5f : 1.0f);
        const bool focused = it.i == selected_ && std::fabs(k) < 0.5f;
        draw_tile(&games[std::size_t(it.i)], x, y, kTileW * 0.66f * s, kTileH * 0.66f * s, it.a, alpha, focused, false);
    }
}

/* ---- a glass cube ---------------------------------------------------------------------- */

/* A cube of glass turned every way, each face it shows lit by where it faces;
 * a picture on its front face. lit: a brighter rim (chosen). */
void App::draw_glass_cube(float cx, float cy, float size, float yaw, float pitch, float roll, Color tint, Color rim,
                          Texture *front, float alpha, bool lit)
{
    Gfx &g = *g_;
    const float h = size * 0.5f;
    const V3 faces[6][4] = {
        {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h}}, /* front */
        {{h, -h, h}, {-h, -h, h}, {-h, h, h}, {h, h, h}},     /* back */
        {{-h, -h, h}, {-h, -h, -h}, {-h, h, -h}, {-h, h, h}}, /* left */
        {{h, -h, -h}, {h, -h, h}, {h, h, h}, {h, h, -h}},     /* right */
        {{-h, -h, h}, {h, -h, h}, {h, -h, -h}, {-h, -h, -h}}, /* top */
        {{-h, h, -h}, {h, h, -h}, {h, h, h}, {-h, h, h}},     /* bottom */
    };
    const float light[6] = {1.0f, 0.55f, 0.70f, 0.80f, 1.15f, 0.50f};
    for (int f = 0; f < 6; ++f)
    {
        Corner q[4];
        for (int i = 0; i < 4; ++i)
            q[i] = seen3(cx, cy, turn3(faces[f][i], yaw, pitch, roll));
        if (!facing_you(q))
            continue;
        Glass face;
        face.tint = Color{tint.r * light[f], tint.g * light[f], tint.b * light[f], tint.a};
        face.rim = rim;
        face.radius = size * 0.10f;
        face.rim_w = lit ? 2.6f : 1.6f;
        face.phase = cx / 1920.0f + float(f) * 0.13f;
        face.fade = alpha;
        g.glass(q, size, size, 0, face);
        if (f == 0 && front)
        {
            Corner p[4];
            const float in = h * 0.70f;
            const V3 pic[4] = {{-in, -in, -h}, {in, -in, -h}, {in, in, -h}, {-in, in, -h}};
            for (int i = 0; i < 4; ++i)
                p[i] = seen3(cx, cy, turn3(pic[i], yaw, pitch, roll));
            g.quad3d(front, p, in * 2, in * 2, with_alpha(kWhite, alpha), 4, false, false);
        }
    }
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
    if (view == 5)
    {
        /* The list: up and down too. */
        if (up && selected_ > 0)
        {
            --selected_;
            moved = true;
        }
        if (down && selected_ + 1 < shown)
        {
            ++selected_;
            moved = true;
        }
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
