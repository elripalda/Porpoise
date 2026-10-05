/* Porpoise UI - a Wii disc's own tile and banner on the Revolution look:
 * played by porpoise_banner's workers a frame at a time, streamed into a
 * texture as each frame comes (the cover stands in until the first, and for
 * discs without one).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cmath>

#include "ui_app.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;

namespace
{
bool wii_disc(const Game &g)
{
    return g.platform == "Wii" && g.id.size() == 6;
}

/* The language group a banner shows for the menus' language. */
const char *banner_language(Language l)
{
    switch (l)
    {
    case Language::Spanish:
    case Language::SpanishLatinAmerica: return "SPA";
    case Language::French: return "FRA";
    case Language::Portuguese:
    case Language::PortugueseBrazil: return "POR";
    case Language::Italian: return "ITA";
    case Language::Japanese: return "JPN";
    case Language::German: return "GER";
    case Language::Dutch: return "NED";
    default: return "ENG";
    }
}

std::string key_of(const Game &g, bool big)
{
    return g.id + (big ? "/banner" : "/icon");
}
} // namespace

/* Once a frame: the language (the banners' and the menus' font for Japanese,
 * Chinese or Korean), the jingle that was waiting, and textures no longer
 * drawn given back. */
void App::pump_banners()
{
    static const std::string names = language_names();
    g_->set_cjk_font(cjk_font(), names);
    const std::string lang = banner_language(language());
    if (lang != banner_lang_)
    {
        banner_lang_ = lang;
        porpoise::banner::set_language(lang);
    }
    if (!jingle_for_.empty())
    {
        std::vector<std::int16_t> pcm;
        if (time_ - rd_open_time_ > 8.0)
            jingle_for_.clear(); /* too late to start it now */
        else if (porpoise::banner::take_jingle(jingle_for_, pcm))
        {
            if (jingle_)
                jingle_(pcm.data(), pcm.size() / 2);
            jingle_for_.clear();
        }
    }
    for (auto it = banner_tex_.begin(); it != banner_tex_.end();)
        if (time_ - it->second.used > 2.0)
        {
            if (it->second.tex)
                g_->free_texture(it->second.tex);
            it = banner_tex_.erase(it);
        }
        else
            ++it;
}

/* The device went away (a game ran on its own): its textures with it. */
void App::forget_banners()
{
    banner_tex_.clear();
}

/* The disc's tile or banner as it plays now, when there is a frame of it. */
App::BannerTex *App::banner_texture(const Game &game, bool big)
{
    if (!wii_disc(game) || no_banner_.count(game.id))
        return nullptr;
    if (!banner_checked_.count(game.id))
    {
        banner_checked_.insert(game.id);
        if (porpoise::banner::known_none(game.id))
        {
            no_banner_.insert(game.id);
            return nullptr;
        }
    }
    const auto kind = big ? porpoise::banner::Kind::Banner : porpoise::banner::Kind::Icon;
    porpoise::banner::want(game.id, game.path, kind);
    if (porpoise::banner::failed(game.id, kind))
    {
        no_banner_.insert(game.id);
        return nullptr;
    }
    BannerTex &b = banner_tex_[key_of(game, big)];
    b.used = time_;
    int w = 0, h = 0;
    if (porpoise::banner::frame(game.id, kind, banner_px_, w, h, b.serial))
    {
        if (!b.tex || b.w != w || b.h != h)
        {
            if (b.tex)
                g_->free_texture(b.tex);
            b.tex = g_->stream_texture(w, h);
            b.w = w;
            b.h = h;
        }
        g_->stream_update(b.tex, banner_px_.data());
    }
    return b.tex ? &b : nullptr;
}

bool App::has_banner(const Game &game) const
{
    const auto it = banner_tex_.find(key_of(game, true));
    return wii_disc(game) && it != banner_tex_.end() && it->second.tex;
}

/* The opened tile: its banner plays from the start, its jingle with it. */
void App::opened_tile(const Game &game)
{
    rd_open_time_ = time_;
    if (jingle_)
        jingle_(nullptr, 0);
    jingle_for_.clear();
    if (!wii_disc(game) || no_banner_.count(game.id))
        return;
    porpoise::banner::want(game.id, game.path, porpoise::banner::Kind::Banner, true);
    jingle_for_ = game.id;
}

void App::draw_frames(const BannerTex &b, float x, float y, float w, float h, float fade, float radius, bool fill)
{
    Gfx &g = *g_;
    if (!b.tex || b.w <= 0 || b.h <= 0)
        return;
    const float aspect = float(b.w) / float(b.h), want = w / h;
    if (fill)
    {
        /* Fill the shape, cropping the picture's excess (a tile). */
        float uv[4] = {0, 0, 1, 1};
        if (want > aspect)
        {
            const float k = aspect / want;
            uv[1] = 0.5f - k * 0.5f;
            uv[3] = 0.5f + k * 0.5f;
        }
        else
        {
            const float k = want / aspect;
            uv[0] = 0.5f - k * 0.5f;
            uv[2] = 0.5f + k * 0.5f;
        }
        g.image_part(b.tex, x, y, w, h, uv, rgba(0xFFFFFF, fade), radius);
        return;
    }
    /* The banner: as tall as the frame. A console showed its lowest quarter
     * under its buttons, so banners keep that part plain: it is left out.
     * Its own picture, soft and wide, fills the sides. */
    constexpr float kShown = 0.78f;
    const float shown = aspect / kShown;
    const float bw = std::min(w, h * shown), bh = bw / shown;
    if (bw < w - 1)
    {
        const float k = shown / want;
        const float uv[4] = {0, kShown * (0.5f - k * 0.5f), 1, kShown * (0.5f + k * 0.5f)};
        g.image_part(b.tex, x, y, w, h, uv, rgba(0xFFFFFF, 0.45f * fade), radius);
        g.panel(x, y, w, h, rgba(0xFFFFFF, 0.35f * fade), 1, radius);
    }
    const float uv[4] = {0, 0, 1, kShown};
    g.image_part(b.tex, x + (w - bw) * 0.5f, y + (h - bh) * 0.5f, bw, bh, uv, rgba(0xFFFFFF, fade),
                 bw < w - 1 ? 0.0f : radius);
}

bool App::draw_banner(Game &game, float x, float y, float w, float h, double time, float fade, bool big)
{
    (void)time;
    BannerTex *b = banner_texture(game, big);
    if (!b && big)
    {
        /* Until the banner's first frame: the tile's own picture, large. */
        const auto ic = banner_tex_.find(key_of(game, false));
        if (ic != banner_tex_.end() && ic->second.tex)
        {
            ic->second.used = time_;
            draw_frames(ic->second, x, y, w, h, fade, rev::kTileR - 8, false);
            return true;
        }
    }
    if (!b)
        return false;
    draw_frames(*b, x, y, w, h, fade, big ? rev::kTileR - 8 : rev::kTileR - 6, !big);
    return true;
}
} // namespace porpoise::ui
