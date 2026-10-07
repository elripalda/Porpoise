/* Porpoise - Dolphin's built-in graphics mods, as a few settings per game.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_gfxmods.hpp"

#include "porpoise_atomic.hpp"

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace porpoise::gfxmods
{
namespace
{
struct GameMods
{
    const char *id; /* the game ID's first three letters, or all six */
    bool bloom, dof, hud;
    const char *bloom_off, *bloom_blur; /* the game's own bloom mods */
    const char *extra, *extra_title;    /* the game's own mod */
};
constexpr GameMods kGames[] = {
#include "porpoise_gfxmods_table.inc"
};

/* Dolphin's "All Games" mods (Sys/Load/GraphicMods). */
const char *const kBloom[4] = {"", "All Games Bloom Removal/metadata.json", "All Games Blurred Bloom/metadata.json",
                               "All Games Native Resolution Bloom/metadata.json"};
const char *const kDof[4] = {"", "All Games DOF Removal/metadata.json", "All Games Blurred DOF/metadata.json",
                             "All Games Native Resolution DOF/metadata.json"};
const char *const kHud = "All Games HUD Removal/metadata.json";

const GameMods *find(const std::string &game_id)
{
    for (const GameMods &g : kGames)
    {
        const std::string id = g.id;
        if (!id.empty() && game_id.compare(0, id.size(), id) == 0)
            return &g;
    }
    return nullptr;
}

std::vector<std::string> chosen(const std::string &game_id, const Choice &c)
{
    std::vector<std::string> on;
    const GameMods *g = find(game_id);
    if (!g)
        return on;
    const bool own_bloom = g->bloom_off[0] != '\0';
    if (own_bloom && c.bloom == 1)
        on.push_back(g->bloom_off);
    else if (own_bloom && c.bloom == 2 && g->bloom_blur[0])
        on.push_back(g->bloom_blur);
    else if (!own_bloom && g->bloom && c.bloom > 0 && c.bloom < 4)
        on.push_back(kBloom[c.bloom]);
    if (g->dof && c.dof > 0 && c.dof < 4)
        on.push_back(kDof[c.dof]);
    if (g->hud && c.hud)
        on.push_back(kHud);
    if (g->extra[0] && c.extra)
        on.push_back(g->extra);
    return on;
}
} // namespace

Offer offer(const std::string &game_id)
{
    Offer o;
    if (const GameMods *g = find(game_id))
    {
        o.own_bloom = g->bloom_off[0] != '\0';
        o.bloom = g->bloom || o.own_bloom;
        o.dof = g->dof;
        o.hud = g->hud;
        o.extra_title = g->extra_title;
    }
    return o;
}

bool wanted(const std::string &game_id, const Choice &choice)
{
    return !chosen(game_id, choice).empty();
}

void write_profile(const std::string &config_dir, const std::string &game_id, const Choice &choice)
{
    if (game_id.size() < 3)
        return;
    const std::string dir = config_dir + "/GraphicMods";
    const std::string path = dir + "/" + game_id + ".json";
    const std::vector<std::string> on = chosen(game_id, choice);
    if (on.empty())
        return; /* Dolphin's graphics mods stay off for the game, so its list isn't read */
    mkdir(config_dir.c_str(), 0777);
    mkdir(dir.c_str(), 0777);
    std::FILE *f = porpoise::open_atomic(path);
    if (!f)
        return;
    std::fprintf(f, "{\n  \"mods\": [\n");
    for (std::size_t i = 0; i < on.size(); ++i)
        std::fprintf(f, "    {\"source\": \"system\", \"path\": \"%s\", \"enabled\": true, \"weight\": 0}%s\n",
                     on[i].c_str(), i + 1 < on.size() ? "," : "");
    std::fprintf(f, "  ]\n}\n");
    porpoise::finish_atomic(f, path);
}
} // namespace porpoise::gfxmods
