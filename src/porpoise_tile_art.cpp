/* Porpoise - a home screen tile's art (porpoise_tile_art.hpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_tile_art.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <sys/stat.h>

#include "porpoise_atomic.hpp"
#include "porpoise_http.hpp"
#include "porpoise_image.hpp"
#include "porpoise_paths.hpp"
#include "title_threads.hpp"
#include "trace.hpp"

namespace porpoise::tileart
{
namespace
{
std::string g_asset_dir = PORPOISE_APP "/assets";

bool exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && st.st_size > 0;
}

std::string first_of(const std::string &base)
{
    for (const char *ext : {".png", ".jpg", ".jpeg", ".PNG", ".JPG"})
        if (exists(base + ext))
            return base + ext;
    return "";
}

std::string art_dir(const std::string &data_dir)
{
    return data_dir + "/home-art";
}

std::uint8_t *at(Picture &p, int x, int y)
{
    return &p.px[(std::size_t(y) * p.w + x) * 4];
}
const std::uint8_t *at(const Picture &p, int x, int y)
{
    return &p.px[(std::size_t(y) * p.w + x) * 4];
}

Picture solid(int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    Picture out;
    out.w = w;
    out.h = h;
    out.px.resize(std::size_t(w) * h * 4);
    for (std::size_t i = 0; i < out.px.size(); i += 4)
    {
        out.px[i] = r;
        out.px[i + 1] = g;
        out.px[i + 2] = b;
        out.px[i + 3] = 255;
    }
    return out;
}

/* The part of src from (x0, y0), cw x ch, made w x h. */
Picture region(const Picture &src, int x0, int y0, int cw, int ch, int w, int h)
{
    cw = std::clamp(cw, 1, src.w);
    ch = std::clamp(ch, 1, src.h);
    x0 = std::clamp(x0, 0, src.w - cw);
    y0 = std::clamp(y0, 0, src.h - ch);
    Picture crop;
    crop.w = cw;
    crop.h = ch;
    crop.px.resize(std::size_t(cw) * ch * 4);
    for (int y = 0; y < ch; ++y)
        std::memcpy(at(crop, 0, y), at(src, x0, y0 + y), std::size_t(cw) * 4);
    return scaled(crop, w, h);
}

/* The part of src with the w:h shape, from its middle (filling, not fitting). */
Picture filled(const Picture &src, int w, int h)
{
    const float want = float(w) / float(h), have = float(src.w) / float(src.h);
    int cw = src.w, ch = src.h;
    if (have > want)
        cw = std::max(1, int(src.h * want));
    else
        ch = std::max(1, int(src.w / want));
    return region(src, (src.w - cw) / 2, (src.h - ch) / 2, cw, ch, w, h);
}

/* A soft, dark copy to sit behind: blurred at a small size, then grown. */
Picture blurred(const Picture &src, int w, int h, float darken)
{
    Picture small = filled(src, std::max(8, w / 24), std::max(8, h / 24));
    for (int pass = 0; pass < 3; ++pass)
    {
        Picture b = small;
        for (int y = 0; y < small.h; ++y)
            for (int x = 0; x < small.w; ++x)
            {
                unsigned sum[3] = {0, 0, 0}, n = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        const int xx = std::clamp(x + dx, 0, small.w - 1), yy = std::clamp(y + dy, 0, small.h - 1);
                        for (int c = 0; c < 3; ++c)
                            sum[c] += at(small, xx, yy)[c];
                        ++n;
                    }
                for (int c = 0; c < 3; ++c)
                    at(b, x, y)[c] = std::uint8_t(sum[c] / n);
            }
        small = b;
    }
    Picture out = scaled(small, w, h);
    for (std::size_t i = 0; i < out.px.size(); i += 4)
    {
        for (int c = 0; c < 3; ++c)
            out.px[i + c] = std::uint8_t(out.px[i + c] * darken);
        out.px[i + 3] = 255;
    }
    return out;
}

/* Porpoise's dolphin, as a mask (its alpha), loaded once. */
const Picture &dolphin()
{
    static Picture mask;
    static std::string from;
    if (from != g_asset_dir)
    {
        from = g_asset_dir;
        mask = {};
        load_picture(g_asset_dir + "/brand/dolphin-mask.png", mask);
    }
    return mask;
}

/* Rows of little dolphins, Porpoise's blues. */
Picture pattern(int w, int h)
{
    Picture out = solid(w, h, 13, 30, 92);
    const Picture &mask = dolphin();
    if (mask.empty())
        return out;
    const int size = std::max(12, h / 7);
    const Picture m = scaled(mask, size, size);
    const int gap_x = int(size * 1.7f), gap_y = int(size * 1.35f);
    const float light[3] = {34, 74, 172};
    for (int row = -1; row * gap_y < h + size; ++row)
        for (int col = -1; col * gap_x < w + size; ++col)
        {
            const int ox = col * gap_x + (row & 1 ? gap_x / 2 : 0), oy = row * gap_y;
            for (int y = 0; y < size; ++y)
                for (int x = 0; x < size; ++x)
                {
                    const int dx = ox + x, dy = oy + y;
                    if (dx < 0 || dy < 0 || dx >= w || dy >= h)
                        continue;
                    const float a = at(m, x, y)[3] / 255.0f;
                    if (a <= 0)
                        continue;
                    std::uint8_t *d = at(out, dx, dy);
                    for (int c = 0; c < 3; ++c)
                        d[c] = std::uint8_t(d[c] * (1 - a) + light[c] * a);
                }
        }
    return out;
}

/* src at w x h with its centre at (cx, cy), over dst, with a soft shadow. */
void place(Picture &dst, const Picture &src, int w, int h, int cx, int cy)
{
    w = std::max(1, w);
    h = std::max(1, h);
    const Picture p = scaled(src, w, h);
    const int x0 = cx - w / 2, y0 = cy - h / 2;
    const int shadow = std::max(4, h / 30);
    for (int y = -shadow; y < h + shadow * 2; ++y)
    {
        const int dy = y0 + y + shadow / 2;
        if (dy < 0 || dy >= dst.h)
            continue;
        for (int x = -shadow; x < w + shadow; ++x)
        {
            const int dx = x0 + x;
            if (dx < 0 || dx >= dst.w)
                continue;
            const float ox = std::max({0.0f, float(-x), float(x - w + 1)}) / shadow;
            const float oy = std::max({0.0f, float(-y), float(y - h + 1)}) / shadow;
            const float a = 0.55f * std::max(0.0f, 1.0f - std::sqrt(ox * ox + oy * oy));
            std::uint8_t *d = at(dst, dx, dy);
            for (int c = 0; c < 3; ++c)
                d[c] = std::uint8_t(d[c] * (1.0f - a));
        }
    }
    for (int y = 0; y < h; ++y)
    {
        const int dy = y0 + y;
        if (dy < 0 || dy >= dst.h)
            continue;
        for (int x = 0; x < w; ++x)
        {
            const int dx = x0 + x;
            if (dx < 0 || dx >= dst.w)
                continue;
            const std::uint8_t *s = at(p, x, y);
            std::uint8_t *d = at(dst, dx, dy);
            const unsigned a = s[3];
            for (int c = 0; c < 3; ++c)
                d[c] = std::uint8_t((s[c] * a + d[c] * (255 - a)) / 255);
        }
    }
}

/* ---- the download ------------------------------------------------------- */

struct Fetch
{
    std::atomic<int> state{0};
    Download request;
};
Fetch g_fetch;

/* libretro's file names: these characters become underscores. */
std::string libretro_name(std::string s)
{
    for (char &c : s)
        if (std::strchr("&*/:`<>?\\|\"", c))
            c = '_';
    return s;
}

std::string url_part(const std::string &s)
{
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c : s)
    {
        if (std::isalnum(c) || std::strchr("-_.~()',!", c))
            out += char(c);
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

/* The names libretro may know the game by: its file's (most sets are named
 * as libretro's lists are), then its title with the disc's region. */
std::vector<std::string> names_for(const Download &d)
{
    std::vector<std::string> names;
    std::string stem = d.game_path.substr(d.game_path.rfind('/') + 1);
    for (int i = 0; i < 2; ++i)
    {
        const std::size_t dot = stem.rfind('.');
        if (dot == std::string::npos)
            break;
        std::string ext = stem.substr(dot + 1);
        for (char &c : ext)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        if (i == 1 && ext != "nkit")
            break;
        stem.erase(dot);
    }
    if (!stem.empty())
        names.push_back(stem);
    if (!d.title.empty())
    {
        const char region = d.game_id.size() > 3 ? d.game_id[3] : 'E';
        const char *place = region == 'P' || region == 'D' || region == 'F' || region == 'S' || region == 'I'
                                ? "Europe"
                            : region == 'J' ? "Japan"
                                            : "USA";
        std::string title = d.title;
        for (std::size_t at = title.find(": "); at != std::string::npos; at = title.find(": ", at))
            title.replace(at, 2, " - ");
        const std::string named = title + " (" + place + ")";
        if (std::find(names.begin(), names.end(), named) == names.end())
            names.push_back(named);
    }
    return names;
}

void *fetch_worker(void *)
{
    const Download d = g_fetch.request;
    const char *folder = d.source == TitleScreen ? "Named_Titles" : "Named_Snaps";
    const char *repo = d.wii ? "Nintendo_-_Wii" : "Nintendo_-_GameCube";
    const char *system = d.wii ? "Nintendo%20-%20Wii" : "Nintendo%20-%20GameCube";
    porpoise::http::Session http("porpoise-tile-art");
    int state = 4;
    if (http.init())
    {
        state = 3;
        std::vector<std::uint8_t> data;
        for (const std::string &name : names_for(d))
        {
            const std::string file = url_part(libretro_name(name)) + ".png";
            const std::string urls[2] = {
                std::string("https://raw.githubusercontent.com/libretro-thumbnails/") + repo + "/master/" + folder +
                    "/" + file,
                std::string("https://thumbnails.libretro.com/") + system + "/" + folder + "/" + file,
            };
            bool found = false;
            for (const std::string &url : urls)
            {
                data.clear();
                const int status = http.get(url, data, {}, std::size_t(8) << 20);
                ps5::debug::mark(("tile art: " + url + " -> " + std::to_string(status)).c_str());
                if (status == 200 && data.size() > 8 && std::memcmp(data.data(), "\x89PNG", 4) == 0)
                {
                    const std::string to = download_path(d.data_dir, d.game_id, d.source);
                    mkdir(art_dir(d.data_dir).c_str(), 0777);
                    state = porpoise::write_whole(to, data.data(), data.size()) ? 2 : 4;
                    found = true;
                    break;
                }
                if (status < 0)
                {
                    state = 4;
                    break;
                }
            }
            if (found || state == 4)
                break;
        }
    }
    g_fetch.state = state;
    return nullptr;
}
} // namespace

Spec defaults()
{
    Spec s;
    reset_position(s.icon, false);
    reset_position(s.bg, true);
    return s;
}

void reset_position(Layer &layer, bool background)
{
    layer.x = 0.5f;
    layer.y = 0.5f;
    layer.zoom = 1.0f;
    /* The background's cover sat right of the middle, a little smaller. */
    if (background && layer.fit == Whole)
    {
        layer.x = 0.703f;
        layer.zoom = 0.79f;
    }
}

Spec load(const std::string &data_dir, const std::string &game_id)
{
    Spec s = defaults();
    std::FILE *f = std::fopen((art_dir(data_dir) + "/" + game_id + ".tile").c_str(), "r");
    if (!f)
    {
        /* Before the editor: an own picture under the old names. */
        const std::string icon = first_of(art_dir(data_dir) + "/" + game_id + "-icon");
        const std::string bg = first_of(art_dir(data_dir) + "/" + game_id + "-background");
        if (!icon.empty())
        {
            s.icon.source = File;
            s.icon.file = icon;
            s.icon.fit = Fill;
            reset_position(s.icon, false);
        }
        if (!bg.empty())
        {
            s.bg.source = File;
            s.bg.file = bg;
            s.bg.fit = Fill;
            reset_position(s.bg, true);
        }
        return s;
    }
    char line[2048];
    while (std::fgets(line, sizeof line, f))
    {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r'))
            l.pop_back();
        const std::size_t eq = l.find('=');
        if (eq == std::string::npos || l.size() < 4)
            continue;
        const std::string key = l.substr(0, eq), value = l.substr(eq + 1);
        Layer *layer = key.rfind("icon.", 0) == 0 ? &s.icon : key.rfind("bg.", 0) == 0 ? &s.bg : nullptr;
        if (!layer)
            continue;
        const std::string field = key.substr(key.find('.') + 1);
        const float number = float(std::atof(value.c_str()));
        if (field == "source")
            layer->source = std::clamp(int(number), 0, kSources - 1);
        else if (field == "file")
            layer->file = value;
        else if (field == "fit")
            layer->fit = std::clamp(int(number), 0, 1);
        else if (field == "behind")
            layer->behind = std::clamp(int(number), 0, kBehinds - 1);
        else if (field == "zoom")
            layer->zoom = std::clamp(number, 0.2f, 6.0f);
        else if (field == "x")
            layer->x = std::clamp(number, -0.5f, 1.5f);
        else if (field == "y")
            layer->y = std::clamp(number, -0.5f, 1.5f);
    }
    std::fclose(f);
    if (s.icon.source == Porpoise)
        s.icon.source = Cover;
    return s;
}

bool save(const std::string &data_dir, const std::string &game_id, const Spec &spec)
{
    mkdir(art_dir(data_dir).c_str(), 0777);
    const std::string path = art_dir(data_dir) + "/" + game_id + ".tile";
    std::FILE *f = porpoise::open_atomic(path);
    if (!f)
        return false;
    const Layer *layers[2] = {&spec.icon, &spec.bg};
    const char *names[2] = {"icon", "bg"};
    for (int i = 0; i < 2; ++i)
    {
        const Layer &l = *layers[i];
        std::fprintf(f, "%s.source=%d\n%s.file=%s\n%s.fit=%d\n%s.behind=%d\n%s.zoom=%.4f\n%s.x=%.4f\n%s.y=%.4f\n",
                     names[i], l.source, names[i], l.file.c_str(), names[i], l.fit, names[i], l.behind, names[i],
                     double(l.zoom), names[i], double(l.x), names[i], double(l.y));
    }
    return porpoise::finish_atomic(f, path);
}

bool load_picture(const std::string &path, Picture &out, int max_side)
{
    out = {};
    if (path.empty() || !porpoise::image::load_rgba(path, out.px, out.w, out.h) || out.w <= 0 || out.h <= 0)
    {
        out = {};
        return false;
    }
    if (max_side > 0 && std::max(out.w, out.h) > max_side)
    {
        const float k = float(max_side) / float(std::max(out.w, out.h));
        out = scaled(out, std::max(1, int(out.w * k)), std::max(1, int(out.h * k)));
    }
    return true;
}

/* Scaled to w x h: each output pixel the average of the source pixels under
 * it (smooth when shrinking), or the nearest four mixed (when growing). */
Picture scaled(const Picture &src, int w, int h)
{
    Picture out;
    out.w = w;
    out.h = h;
    out.px.assign(std::size_t(w) * h * 4, 0);
    const float sx = float(src.w) / float(w), sy = float(src.h) / float(h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            std::uint8_t *o = at(out, x, y);
            if (sx > 1.0f || sy > 1.0f)
            {
                const int x0 = int(x * sx), x1 = std::max(x0 + 1, std::min(src.w, int((x + 1) * sx)));
                const int y0 = int(y * sy), y1 = std::max(y0 + 1, std::min(src.h, int((y + 1) * sy)));
                const int step_x = std::max(1, (x1 - x0) / 4), step_y = std::max(1, (y1 - y0) / 4);
                unsigned sum[4] = {0, 0, 0, 0}, n = 0;
                for (int yy = y0; yy < y1; yy += step_y)
                    for (int xx = x0; xx < x1; xx += step_x)
                    {
                        const std::uint8_t *s = at(src, std::min(xx, src.w - 1), std::min(yy, src.h - 1));
                        for (int c = 0; c < 4; ++c)
                            sum[c] += s[c];
                        ++n;
                    }
                for (int c = 0; c < 4; ++c)
                    o[c] = std::uint8_t(sum[c] / n);
            }
            else
            {
                const float fx = std::max(0.0f, (x + 0.5f) * sx - 0.5f), fy = std::max(0.0f, (y + 0.5f) * sy - 0.5f);
                const int ix = std::min(int(fx), src.w - 1), iy = std::min(int(fy), src.h - 1);
                const int jx = std::min(ix + 1, src.w - 1), jy = std::min(iy + 1, src.h - 1);
                const float ax = fx - ix, ay = fy - iy;
                for (int c = 0; c < 4; ++c)
                {
                    const float top = at(src, ix, iy)[c] * (1 - ax) + at(src, jx, iy)[c] * ax;
                    const float bottom = at(src, ix, jy)[c] * (1 - ax) + at(src, jx, jy)[c] * ax;
                    o[c] = std::uint8_t(std::lround(top * (1 - ay) + bottom * ay));
                }
            }
        }
    return out;
}

std::string download_path(const std::string &data_dir, const std::string &game_id, int source)
{
    return art_dir(data_dir) + "/" + game_id + (source == TitleScreen ? ".title.png" : ".snap.png");
}

std::string picture_path(const Layer &layer, const std::string &data_dir, const std::string &game_id,
                         const std::string &cover_path)
{
    switch (layer.source)
    {
    case Cover:
        return exists(cover_path) ? cover_path : "";
    case Back:
    {
        /* Kept beside the cover by the covers worker: <ID>.back.png. */
        const std::size_t slash = cover_path.rfind('/');
        if (slash == std::string::npos)
            return "";
        const std::string back = cover_path.substr(0, slash + 1) + game_id + ".back.png";
        return exists(back) ? back : "";
    }
    case Screenshot:
    case TitleScreen:
    {
        const std::string p = download_path(data_dir, game_id, layer.source);
        return exists(p) ? p : "";
    }
    case File:
        return exists(layer.file) ? layer.file : "";
    default:
        return "";
    }
}

bool compose(const Layer &layer, const Picture &src, int w, int h, bool background, Picture &out)
{
    if (src.empty() || w <= 0 || h <= 0)
        return false;
    if (layer.fit == Fill)
    {
        const float zoom = std::max(1.0f, layer.zoom);
        const float k = std::max(float(w) / src.w, float(h) / src.h) * zoom;
        const float cw = std::min(float(src.w), w / k), ch = std::min(float(src.h), h / k);
        const float cx = std::clamp(layer.x * src.w, cw * 0.5f, src.w - cw * 0.5f);
        const float cy = std::clamp(layer.y * src.h, ch * 0.5f, src.h - ch * 0.5f);
        Picture base = solid(w, h, 0, 0, 0);
        const Picture part = region(src, int(std::lround(cx - cw * 0.5f)), int(std::lround(cy - ch * 0.5f)),
                                    int(std::lround(cw)), int(std::lround(ch)), w, h);
        /* Over black, for a picture with see-through parts. */
        for (std::size_t i = 0; i < part.px.size(); i += 4)
            for (int c = 0; c < 3; ++c)
                base.px[i + c] = std::uint8_t(part.px[i + c] * part.px[i + 3] / 255);
        out = std::move(base);
        return true;
    }
    switch (layer.behind)
    {
    case White:
        out = solid(w, h, 242, 242, 244);
        break;
    case Black:
        out = solid(w, h, 8, 8, 12);
        break;
    case Pattern:
        out = pattern(w, h);
        break;
    default:
        out = blurred(src, w, h, background ? 0.45f : 0.55f);
        break;
    }
    const float k = std::min(float(w) / src.w, float(h) / src.h) * 0.88f * std::clamp(layer.zoom, 0.2f, 6.0f);
    place(out, src, int(src.w * k), int(src.h * k), int(layer.x * w), int(layer.y * h));
    return true;
}

void set_asset_dir(const std::string &dir)
{
    g_asset_dir = dir;
}

void download(const Download &request)
{
    if (g_fetch.state.load() == 1)
        return;
    g_fetch.request = request;
    g_fetch.state = 1;
    pthread_t thread;
    if (create_title_thread(&thread, fetch_worker, nullptr) == 0)
        pthread_detach(thread);
    else
        g_fetch.state = 4;
}

int download_state(bool take)
{
    const int s = g_fetch.state.load();
    if (take && s >= 2)
        g_fetch.state = 0;
    return s;
}
} // namespace porpoise::tileart
