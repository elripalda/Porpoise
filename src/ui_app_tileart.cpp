/* Porpoise UI - the tile art editor: a game's home screen tile, its icon and
 * the background behind it, seen as the home screen will show them, and made
 * as the player wants (porpoise_tile_art.hpp has the pictures).
 *
 * L1 / R1: the icon or the background. Up / Down: a choice, Left / Right:
 * change it (the picture, how it fits, what's behind it). The left stick moves
 * the picture, R2 / L2 zoom, Triangle puts it back. Cross on the picture:
 * one of the player's own. Square saves and makes the tile; Circle leaves.
 *
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <algorithm>
#include <cmath>
#include <functional>
#include <sys/stat.h>

#include "porpoise_pad.hpp"
#include "porpoise_tile_art.hpp"
#include "ui_app.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;
namespace art = porpoise::tileart;

namespace
{
/* The preview's sizes: the icon as the home screen's chosen tile, the
 * background a quarter of the screen's. */
constexpr int kIconPx = 300;
constexpr int kBgW = 768, kBgH = 432;

/* The pictures each part can be made from, in the order Left / Right steps. */
const std::vector<int> &sources_for(int part)
{
    static const std::vector<int> icon = {art::Cover, art::Back, art::Screenshot, art::TitleScreen, art::File};
    static const std::vector<int> bg = {art::Porpoise, art::Cover,       art::Back,
                                        art::Screenshot, art::TitleScreen, art::File};
    return part == 0 ? icon : bg;
}

const char *source_name(int source)
{
    switch (source)
    {
    case art::Cover:
        return "The Cover";
    case art::Back:
        return "The Back of the Box";
    case art::Screenshot:
        return "A Screenshot";
    case art::TitleScreen:
        return "The Title Screen";
    case art::File:
        return "Your Picture";
    default:
        return "Porpoise's";
    }
}

const char *behind_name(int behind)
{
    switch (behind)
    {
    case art::White:
        return "White";
    case art::Black:
        return "Black";
    case art::Pattern:
        return "Porpoise Pattern";
    default:
        return "The picture, blurred";
    }
}
} // namespace

std::string App::tile_key() const
{
    if (!game_for_)
        return "";
    if (!game_for_->id.empty())
        return game_for_->id;
    /* No disc ID (a homebrew program): its file's name, made safe. */
    std::string key = "file-" + std::to_string(std::hash<std::string>{}(game_for_->path) & 0xFFFFFFFFu);
    return key;
}

void App::open_tile_art()
{
    if (!game_for_)
        return;
    const auto keep_tex = art_.tex;
    art_ = TileArtEditor{};
    art_.tex[0] = keep_tex[0];
    art_.tex[1] = keep_tex[1];
    art_.spec = art::load(data_dir_, tile_key());
    open_screen(Screen::TileArt);
    sfx(Sound::DetailsFlip);
}

void App::close_tile_art()
{
    for (Texture *&t : art_.tex)
        if (t)
        {
            g_->free_texture(t);
            t = nullptr;
        }
    art_.src[0] = art_.src[1] = {};
    art_.src_from[0].clear();
    art_.src_from[1].clear();
    browse_images_ = false;
}

/* The preview made again for whichever part changed; a missing screenshot
 * or title screen asked of libretro. */
void App::tile_art_refresh()
{
    if (!game_for_)
        return;
    const std::string key = tile_key();
    for (int part = 0; part < 2; ++part)
    {
        if (!art_.dirty[part])
            continue;
        art_.dirty[part] = false;
        const art::Layer &l = part == 0 ? art_.spec.icon : art_.spec.bg;
        art_.shown[part] = false;
        if (l.source == art::Porpoise)
            continue;
        const std::string path = art::picture_path(l, data_dir_, key, lib_->cover_path(*game_for_));
        if (path.empty())
        {
            if ((l.source == art::Screenshot || l.source == art::TitleScreen) && art_.waiting < 0 &&
                art::download_state() != 1)
            {
                art::Download d;
                d.data_dir = data_dir_;
                d.game_id = key;
                d.game_path = game_for_->path;
                d.title = game_for_->title;
                d.wii = game_for_->platform == "Wii";
                d.source = l.source;
                art::download(d);
                art_.waiting = l.source;
            }
            continue;
        }
        if (art_.src_from[part] != path)
        {
            art::load_picture(path, art_.src[part], 1024);
            art_.src_from[part] = path;
        }
        art::Picture made;
        const int w = part == 0 ? kIconPx : kBgW, h = part == 0 ? kIconPx : kBgH;
        if (!art::compose(l, art_.src[part], w, h, part == 1, made))
            continue;
        if (!art_.tex[part])
            art_.tex[part] = g_->stream_texture(w, h);
        if (art_.tex[part])
        {
            g_->stream_update(art_.tex[part], made.px.data());
            art_.shown[part] = true;
        }
    }
}

App::Action App::update_tile_art(double dt)
{
    auto press = [&](std::uint32_t bit) { return ((raw_held_ & ~raw_prev_) & bit) != 0; };
    int &part = art_.part;
    art::Layer &l = part == 0 ? art_.spec.icon : art_.spec.bg;

    /* A download finished: its picture shows, or why it can't. */
    if (art_.waiting >= 0)
    {
        const int state = art::download_state(true);
        if (state >= 2)
        {
            const int what = art_.waiting;
            art_.waiting = -1;
            if (state == 2)
                art_.note.clear();
            else if (state == 3)
                art_.note = tr(what == art::TitleScreen ? "libretro has no title screen of this game"
                                                        : "libretro has no screenshot of this game");
            else
                art_.note = tr("It couldn't be downloaded. Is the console online?");
            art_.dirty[0] = art_.dirty[1] = true;
        }
    }

    if (press(BtnL1) || press(BtnR1))
    {
        part ^= 1;
        art_.note.clear();
        sfx(Sound::MovingTab);
    }
    const bool whole = l.fit == art::Whole;
    if (press(BtnUp) && art_.row > 0)
    {
        --art_.row;
        sfx(Sound::MenuScroll);
    }
    if (press(BtnDown) && art_.row < (whole ? 2 : 1))
    {
        ++art_.row;
        sfx(Sound::MenuScroll);
    }
    art_.row = std::min(art_.row, whole ? 2 : 1);
    const int step = press(BtnLeft) ? -1 : press(BtnRight) ? 1 : 0;
    if (step != 0)
    {
        if (art_.row == 0)
        {
            const auto &list = sources_for(part);
            auto at = std::find(list.begin(), list.end(), l.source);
            int i = at == list.end() ? 0 : int(at - list.begin());
            i = (i + step + int(list.size())) % int(list.size());
            l.source = list[std::size_t(i)];
            art_.note.clear();
        }
        else if (art_.row == 1)
        {
            l.fit = l.fit == art::Whole ? art::Fill : art::Whole;
            art::reset_position(l, part == 1);
        }
        else
            l.behind = (l.behind + step + art::kBehinds) % art::kBehinds;
        art_.dirty[part] = true;
        sfx(Sound::MenuScroll);
    }
    if (press(BtnCross) && art_.row == 0 && l.source != art::Porpoise)
    {
        /* One of the player's own: from Porpoise's folder for them, or a drive. */
        const std::string folder = data_dir_ + "/home-art";
        mkdir(folder.c_str(), 0777);
        browse_images_ = true;
        open_browser(folder);
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    if (press(BtnTriangle))
    {
        art::reset_position(l, part == 1);
        art_.dirty[part] = true;
        sfx(Sound::MenuScroll);
    }

    /* The stick moves the picture: whole, across the space; filling, the
     * part of it that shows. R2 / L2 zoom. */
    const float sx = std::fabs(stick_x_) > 0.15f ? stick_x_ : 0.0f;
    const float sy = std::fabs(stick_y_) > 0.15f ? stick_y_ : 0.0f;
    if (sx != 0 || sy != 0)
    {
        const float speed = 0.55f * float(dt);
        if (whole)
        {
            l.x = std::clamp(l.x + sx * speed, -0.3f, 1.3f);
            l.y = std::clamp(l.y + sy * speed, -0.3f, 1.3f);
        }
        else
        {
            l.x = std::clamp(l.x - sx * speed / l.zoom, 0.0f, 1.0f);
            l.y = std::clamp(l.y - sy * speed / l.zoom, 0.0f, 1.0f);
        }
        art_.dirty[part] = true;
    }
    const bool in = (raw_held_ & BtnR2) != 0, out = (raw_held_ & BtnL2) != 0;
    if (in != out)
    {
        const float k = std::exp((in ? 1.0f : -1.0f) * 0.9f * float(dt));
        l.zoom = whole ? std::clamp(l.zoom * k, 0.25f, 3.0f) : std::clamp(l.zoom * k, 1.0f, 5.0f);
        art_.dirty[part] = true;
    }

    if (press(BtnSquare))
    {
        if (!art::save(data_dir_, tile_key(), art_.spec))
        {
            open_dialog(DialogKind::Info, tr("The tile art couldn't be saved"),
                        trf("Writing {path} failed.", {{"path", data_dir_ + "/home-art"}}), "");
            return Action::None;
        }
        close_tile_art();
        open_screen(Screen::GameSettings);
        sfx(Sound::LaunchGame);
        make_forwarder();
        return Action::None;
    }
    if (press(BtnCircle))
    {
        close_tile_art();
        open_screen(Screen::GameSettings);
        sfx(Sound::MovingTab);
        return Action::None;
    }
    tile_art_refresh();
    return Action::None;
}

void App::draw_tile_art(double)
{
    Gfx &g = *g_;
    if (!game_for_)
        return;
    /* The panel sits where the settings' do, clear of the top bar. */
    const float x = 90, y = 136, w = 1740, h = 800;
    g.panel(x, y, w, h, rgba(0x0F1F63, 0.94f), 0.75f, kR, rgba(0x4C6FD8, 0.9f), 1.8f, 0, 0.12f);
    g.text_mid(Font::Bold, ts(44), x + 50, y + 58, kWhite, Align::Left, tr("Tile Art"));
    g.text_mid(Font::SemiBold, ts(26), x + 50, y + 104, kIcy, Align::Left,
               fit(g, Font::SemiBold, ts(26), game_for_->title, 1000));

    const art::Layer &l = art_.part == 0 ? art_.spec.icon : art_.spec.bg;

    /* The icon, large, and the home screen as it will look: the background,
     * and the tile on it at its own size among others. */
    const float px = x + 44, py = y + 146;
    const float isz = 300;
    const float bx = px + isz + 26, bw = 762, bh = bw * 9.0f / 16.0f;
    auto frame = [&](float fx, float fy, float fw, float fh, bool on, float radius) {
        g.panel(fx - 4, fy - 4, fw + 8, fh + 8, rgba(0x02040C, 0.9f), 1, radius + 4, on ? kCyan : rgba(0x3D4F9E, 0.8f),
                on ? 3.0f : 1.4f, on ? 10.0f : 0.0f);
    };
    frame(px, py, isz, isz, art_.part == 0, 34);
    if (art_.shown[0] && art_.tex[0])
        g.image(art_.tex[0], px, py, isz, isz, {}, 30);
    else
        g.panel(px, py, isz, isz, rgba(0x1D2F7A), 1, 30);
    g.text_mid(Font::SemiBold, ts(22), px + isz * 0.5f, py + isz + 36, art_.part == 0 ? kIcy : kLavender,
               Align::Center, tr("Icon"));
    frame(bx, py, bw, bh, art_.part == 1, 8);
    if (art_.shown[1] && art_.tex[1])
        g.image(art_.tex[1], bx, py, bw, bh, {}, 6);
    else
    {
        g.panel(bx, py, bw, bh, rgba(0x0B1640), 0.6f, 6);
        if (art_.spec.bg.source == art::Porpoise)
            g.text_mid(Font::SemiBold, ts(24), bx + bw * 0.5f, py + bh * 0.62f, kLavender, Align::Center,
                       tr("Porpoise's Own Background"));
    }
    /* The home screen's row: this tile chosen, others beside it. */
    const float tile = bh * 0.19f, tx = bx + bw * 0.035f, ty = py + bh * 0.07f, small = tile * 0.7f;
    for (int i = 0; i < 6; ++i)
        g.panel(tx + tile + bw * 0.02f + i * (small + bw * 0.012f), ty + (tile - small) * 0.5f, small, small,
                rgba(0xFFFFFF, 0.16f), 1, 8);
    g.panel(tx - 3, ty - 3, tile + 6, tile + 6, kWhite, 1, 13);
    if (art_.shown[0] && art_.tex[0])
        g.image(art_.tex[0], tx, ty, tile, tile, {}, 11);
    g.text_mid(Font::SemiBold, ts(18), tx, ty + tile + 22, kWhite, Align::Left,
               fit(g, Font::SemiBold, ts(18), game_for_->title, bw * 0.5f));
    g.text_mid(Font::SemiBold, ts(22), bx + bw * 0.5f, py + bh + 36, art_.part == 1 ? kIcy : kLavender,
               Align::Center, tr("Background, as the home screen shows it"));
    const float pw = bw + isz + 26, ph = bh + 36;

    /* What the preview can't show, and why. */
    std::string note = art_.note;
    if (note.empty() && !art_.shown[art_.part] && l.source != art::Porpoise)
    {
        if (art_.waiting == l.source)
            note = tr("Downloading from libretro\xE2\x80\xA6");
        else if (l.source == art::Cover)
            note = tr("This game has no cover yet");
        else if (l.source == art::Back)
            note = tr("GameTDB has no back of the box for this game");
        else if (l.source == art::File)
            note = tr("Press Cross to choose a picture");
    }
    if (!note.empty())
        g.text_mid(Font::SemiBold, ts(26), px + pw * 0.5f, py + ph + 70, rgba(0xFFD27A), Align::Center,
                   fit(g, Font::SemiBold, ts(26), note, pw));

    /* The choices. */
    const float rw = 476, rx = x + w - 44 - rw;
    const float tab_w = (rw - 12) * 0.5f;
    for (int i = 0; i < 2; ++i)
    {
        const bool on = art_.part == i;
        const float bx = rx + i * (tab_w + 12);
        g.panel(bx, py, tab_w, 64, on ? rgba(0x1D45B8, 0.95f) : rgba(0x0B1640, 0.7f), 0.8f, 32,
                on ? kIcy : rgba(0x3D4F9E, 0.8f), on ? 2.2f : 1.2f);
        g.text_mid(Font::Bold, ts(26), bx + tab_w * 0.5f + (i == 0 ? 14.0f : -14.0f), py + 32, on ? kWhite : kSoft,
                   Align::Center, tr(i == 0 ? "Icon" : "Background"));
        g.glyph(i == 0 ? Glyph::L1 : Glyph::R1, i == 0 ? bx + 34 : bx + tab_w - 34, py + 32, 30,
                on ? kWhite : kSoft);
    }

    const bool whole = l.fit == art::Whole;
    const std::string values[3] = {tr(source_name(l.source)),
                                   tr(whole ? "Whole" : "Fill (Cropped)"), tr(behind_name(l.behind))};
    const char *labels[3] = {"Picture", "Fit", "Behind It"};
    float ry = py + 100;
    for (int i = 0; i < 3; ++i)
    {
        const bool on = art_.row == i;
        const bool off = i == 2 && !whole;
        if (on)
            g.panel(rx, ry, rw, 112, rgba(0x1D45B8, 0.88f), 0.7f, kR, kIcy, 2.4f, 10, 0.18f);
        const Color label = off ? rgba(0x6F7BB8) : on ? kIcy : kLavender;
        g.text_mid(Font::SemiBold, ts(22), rx + 26, ry + 32, label, Align::Left, tr(labels[i]));
        const Color value = off ? rgba(0x6F7BB8) : kWhite;
        g.text_mid(Font::Bold, ts(28), rx + 26, ry + 76, value, Align::Left,
                   fit(g, Font::Bold, ts(28), off ? tr("Nothing: it fills the space") : values[i], rw - 110));
        if (on && !off)
        {
            g.glyph(Glyph::Arrow, rx + rw - 64, ry + 76, 18, kCyan, -kPi * 0.5f);
            g.glyph(Glyph::Arrow, rx + rw - 30, ry + 76, 18, kCyan, kPi * 0.5f);
        }
        ry += 124;
    }
    /* How to move it. */
    ry += 12;
    auto hint = [&](Glyph gl, const std::string &text) {
        g.glyph(gl, rx + 30, ry, 36, kSoft);
        g.text_mid(Font::Regular, ts(24), rx + 66, ry, kSoft, Align::Left, fit(g, Font::Regular, ts(24), text, rw - 80));
        ry += 50;
    };
    hint(Glyph::LStick, tr("Move the Picture"));
    hint(Glyph::R2, tr("Zoom In (L2: Out)"));
    if (art_.row == 0 && l.source != art::Porpoise)
        hint(Glyph::Cross, tr("Choose Your Own Picture"));

    draw_prompts({{Glyph::Circle, "Cancel"}, {Glyph::Triangle, "Reset Position"}},
                 {{Glyph::Square, "Save and Make the Tile"}}, "");
}
} // namespace porpoise::ui
