/* Porpoise UI - a game's cheats and patches, as Dolphin ships them.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_cheats.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ui_i18n.hpp"

namespace porpoise::ui
{
namespace
{
void read_file(const std::string &path, std::vector<Cheat> &out)
{
    std::FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
        return;
    std::string section;
    char buf[512];
    auto find = [&](const std::string &kind, const std::string &name) -> Cheat * {
        for (Cheat &c : out)
            if (c.kind == kind && c.name == name)
                return &c;
        return nullptr;
    };
    while (std::fgets(buf, sizeof buf, f))
    {
        std::string line = buf;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        if (line[0] == '[')
        {
            const std::size_t end = line.find(']');
            section = end == std::string::npos ? "" : line.substr(1, end - 1);
            continue;
        }
        if (line[0] != '$' || line.size() < 2 || line.find('=') != std::string::npos)
            continue;
        for (const char *kind : {"OnFrame", "ActionReplay", "Gecko"})
        {
            const std::string k = kind;
            if (section == k)
            {
                if (!find(k, line))
                    out.push_back({k, line, false});
            }
            else if (section == k + "_Enabled")
            {
                if (Cheat *c = find(k, line))
                    c->default_on = true;
                else
                    out.push_back({k, line, true});
            }
        }
    }
    std::fclose(f);
}
} // namespace

std::vector<Cheat> cheats_for(const std::string &sys_dir, const std::string &game_id)
{
    std::vector<Cheat> out;
    if (game_id.size() < 3)
        return out;
    read_file(sys_dir + "/GameSettings/" + game_id.substr(0, 3) + ".ini", out);
    if (game_id.size() >= 4)
        read_file(sys_dir + "/GameSettings/" + game_id.substr(0, 4) + ".ini", out);
    if (game_id.size() == 6)
        read_file(sys_dir + "/GameSettings/" + game_id + ".ini", out);
    /* Patches first (widescreen, frame rate), then the cheats. */
    std::stable_sort(out.begin(), out.end(), [](const Cheat &a, const Cheat &b) {
        return (a.kind == "OnFrame") > (b.kind == "OnFrame");
    });
    return out;
}

std::string cheat_key(const Cheat &cheat, bool on)
{
    return "dolphin." + cheat.kind + (on ? "_Enabled." : "_Disabled.") + cheat.name;
}

std::string cheat_help(const Cheat &cheat)
{
    std::string lower = cheat.name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (cheat.kind == "OnFrame")
    {
        if (lower.find("widescreen") != std::string::npos || lower.find("16:9") != std::string::npos)
            return tr("A patch from Dolphin: the game drawn in 16:9. Use it with Aspect ratio on Force 16:9.");
        if (lower.find("60") != std::string::npos && lower.find("fps") != std::string::npos)
            return tr("A patch from Dolphin: the game runs at 60 frames a second. Needs a game that holds full "
                      "speed.");
        return cheat.default_on ? tr("A patch Dolphin turns on for this game by itself.")
                                : tr("A patch from Dolphin's list for this game.");
    }
    return cheat.kind == "Gecko" ? tr("A Gecko cheat from Dolphin's list. Turning one on turns this game's cheats on.")
                                 : tr("An Action Replay cheat from Dolphin's list. Turning one on turns this game's "
                                      "cheats on.");
}
} // namespace porpoise::ui
