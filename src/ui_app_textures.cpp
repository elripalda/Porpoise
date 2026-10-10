/* Porpoise UI - Settings > Textures: the HD texture packs Porpoise finds, each
 * matched to the game it is for, with how many textures it holds. A pack is a
 * folder in <saves>/User/Load/Textures named with the game's ID (GALE01, or
 * its first three letters); Dolphin loads it when Custom Textures is on.
 * Counting a pack's files can take a while on a big one, so it happens on a
 * worker, and the rows fill in when it's done.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_app.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <strings.h>
#include <dirent.h>
#include <memory>
#include <mutex>
#include <sys/stat.h>

#include "title_threads.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
namespace
{
struct PackScan
{
    std::string root;
    std::vector<TexturePack> packs; /* names in, counts out */
    std::atomic<bool> done{false};
};

/* The pictures in a folder and below it (a cap keeps a vast one quick). */
long count_textures(const std::string &dir, int depth, long &budget)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return 0;
    long n = 0;
    while (dirent *e = readdir(d))
    {
        if (budget-- <= 0)
            break;
        const char *name = e->d_name;
        if (name[0] == '.')
            continue;
        const std::string path = dir + "/" + name;
        bool is_dir = e->d_type == DT_DIR;
        if (e->d_type == DT_UNKNOWN)
        {
            struct stat st{};
            is_dir = stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        }
        if (is_dir)
        {
            if (depth > 0)
                n += count_textures(path, depth - 1, budget);
            continue;
        }
        const char *dot = std::strrchr(name, '.');
        if (dot && (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".dds") == 0))
            ++n;
    }
    closedir(d);
    return n;
}

void *scan_packs(void *arg)
{
    auto *held = static_cast<std::shared_ptr<PackScan> *>(arg);
    std::shared_ptr<PackScan> scan = *held;
    delete held;
    for (TexturePack &p : scan->packs)
    {
        long budget = 400000;
        p.textures = count_textures(scan->root + "/" + p.folder, 8, budget);
        p.counted = true;
    }
    scan->done.store(true);
    return nullptr;
}

std::shared_ptr<PackScan> g_scan;
} // namespace

std::string App::texture_root() const
{
    return saves_dir_ + "/User/Load/Textures";
}

/* The packs' folders now, and a count of each started (or the last one's,
 * when the folders are the same). */
void App::refresh_texture_packs()
{
    std::vector<std::string> names;
    if (DIR *d = opendir(texture_root().c_str()))
    {
        while (dirent *e = readdir(d))
        {
            if (e->d_name[0] == '.')
                continue;
            const std::string path = texture_root() + "/" + e->d_name;
            struct stat st{};
            if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                names.push_back(e->d_name);
        }
        closedir(d);
    }
    std::sort(names.begin(), names.end());
    std::vector<std::string> had;
    for (const TexturePack &p : texture_packs_)
        had.push_back(p.folder);
    /* The same folders, counted lately (or being counted): as they are. */
    if (names == had && (texture_scanning_ || names.empty() || time_ - texture_scan_time_ < 30.0))
        return;
    if (names != had)
    {
        /* New or gone folders: counted afresh. The same ones keep showing
         * their counts while they're counted again. */
        texture_packs_.clear();
        for (const std::string &n : names)
            texture_packs_.push_back({n, 0, false});
    }
    if (names.empty())
        return;
    auto scan = std::make_shared<PackScan>();
    scan->root = texture_root();
    scan->packs = texture_packs_;
    g_scan = scan;
    texture_scanning_ = true;
    pthread_t thread;
    auto *arg = new std::shared_ptr<PackScan>(scan);
    if (create_title_thread(&thread, scan_packs, arg) == 0)
        pthread_detach(thread);
    else
        scan_packs(arg);
}

/* Once a frame in Settings: the counts, when the worker has them. */
void App::poll_texture_packs()
{
    if (!texture_scanning_ || !g_scan || !g_scan->done.load())
        return;
    texture_packs_ = g_scan->packs;
    g_scan.reset();
    texture_scanning_ = false;
    texture_scan_time_ = time_; /* counted now: not again for a while */
    /* The rows again (the selection kept), only where they show: a game's
     * settings, or Settings' tab. Elsewhere the next build has the counts. */
    const int row = settings_row_;
    if (screen_ == Screen::GameSettings && game_for_)
        build_game_settings();
    else if (screen_ == Screen::Main && tab_ == Tab::Settings && !map_in_game_)
        build_settings();
    else
        return;
    settings_row_ = std::clamp(row, 0, int(rows_.size()) - 1);
}

/* The game a pack is for: its ID, else the first game with those letters. */
const Game *App::game_for_pack(const std::string &folder) const
{
    if (!lib_)
        return nullptr;
    const Game *prefix = nullptr;
    for (const Game &g : lib_->games())
    {
        if (g.id.empty())
            continue;
        if (g.id == folder)
            return &g;
        if (!prefix && folder.size() >= 3 && folder.size() < 6 && g.id.compare(0, folder.size(), folder) == 0)
            prefix = &g;
    }
    return prefix;
}

void App::add_texture_rows(bool per_game)
{
    refresh_texture_packs();
    auto count_text = [&](const TexturePack &p) {
        if (!p.counted)
            return tr("Counting\xE2\x80\xA6");
        if (p.textures == 0)
            return tr("No textures");
        return plural((long long)p.textures, "1 texture", "{n} textures");
    };
    if (per_game)
    {
        if (!game_for_)
            return;
        /* This game's pack, if there is one. */
        SettingRow r;
        r.section = "Textures";
        r.label = tr("This Game's Pack");
        const TexturePack *mine = nullptr;
        for (const TexturePack &p : texture_packs_)
            if (!game_for_->id.empty() &&
                (p.folder == game_for_->id || (p.folder.size() >= 3 && p.folder.size() < 6 &&
                                               game_for_->id.compare(0, p.folder.size(), p.folder) == 0)))
                mine = &p;
        if (mine)
        {
            r.values = {count_text(*mine)};
            r.help = trf("Found in Load/Textures/{folder}. Custom Textures above turns it on or off for this game.",
                         {{"folder", mine->folder}});
        }
        else
        {
            r.values = {tr("None")};
            r.help = trf("No pack for this game yet. Put its folder, named {id}, in "
                         "/data/porpoise/saves/User/Load/Textures.",
                         {{"id", game_for_->id.empty() ? std::string("GALE01") : game_for_->id}});
        }
        rows_.push_back(r);
        return;
    }
    {
        SettingRow r;
        r.section = "Textures";
        r.label = tr("Pack Folder");
        r.values = {"Load/Textures"};
        r.help = tr("Each pack goes in /data/porpoise/saves/User/Load/Textures, in a folder named with the game's "
                    "ID (shown in the game's Details), like GALE01. A pack that names only the first three "
                    "letters (GAL) is for every version of the game.");
        rows_.push_back(r);
    }
    if (texture_packs_.empty())
    {
        SettingRow r;
        r.section = "Textures";
        r.label = tr("No Packs Found");
        r.values = {""};
        r.help = tr("Copy a pack's folder there with PS5 Upload or FTP, then come back here: it shows up with the "
                    "game it's for.");
        rows_.push_back(r);
        return;
    }
    /* The packs for your games first (by title), then the rest. */
    std::vector<const TexturePack *> order;
    for (const TexturePack &p : texture_packs_)
        order.push_back(&p);
    std::stable_sort(order.begin(), order.end(), [&](const TexturePack *a, const TexturePack *b) {
        const Game *ga = game_for_pack(a->folder), *gb = game_for_pack(b->folder);
        if ((ga != nullptr) != (gb != nullptr))
            return ga != nullptr;
        if (ga && gb)
            return (ga->db_title.empty() ? ga->title : ga->db_title) < (gb->db_title.empty() ? gb->title : gb->db_title);
        return a->folder < b->folder;
    });
    for (const TexturePack *pp : order)
    {
        const TexturePack &p = *pp;
        SettingRow r;
        r.section = "Textures";
        const Game *g = game_for_pack(p.folder);
        if (g)
        {
            r.label = g->db_title.empty() ? g->title : g->db_title;
            r.values = {count_text(p)};
            r.help = trf("Load/Textures/{folder}, for {game}. Turn it off for this game alone in the game's own "
                         "settings (Textures).",
                         {{"folder", p.folder}, {"game", r.label}});
        }
        else
        {
            r.label = p.folder;
            r.values = {tr("No Matching Game")};
            r.help = trf("No game in your library has the ID {folder}. The folder must be named with the game's ID, "
                         "like GALE01: check it against the game's Details.",
                         {{"folder", p.folder}});
        }
        rows_.push_back(r);
    }
}
} // namespace porpoise::ui
