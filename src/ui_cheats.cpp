/* Porpoise UI - a game's cheats and patches, as Dolphin ships them.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_cheats.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ui_i18n.hpp"
#include "ui_widescreen.hpp"

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

/* The player's own files for a game, most specific first. */
std::vector<std::string> own_files(const std::string &own_dir, const std::string &game_id)
{
    std::vector<std::string> out;
    if (own_dir.empty() || game_id.size() < 3)
        return out;
    for (std::size_t len : {std::size_t(6), std::size_t(4), std::size_t(3)})
        if (game_id.size() >= len)
            for (const char *ext : {".ini", ".txt"})
            {
                const std::string path = own_dir + "/" + game_id.substr(0, len) + ext;
                if (std::FILE *f = std::fopen(path.c_str(), "r"))
                {
                    std::fclose(f);
                    out.push_back(path);
                }
            }
    return out;
}

std::vector<Cheat> cheats_for(const std::string &sys_dir, const std::string &game_id, const std::string &own_dir,
                              bool wii)
{
    std::vector<Cheat> out;
    if (game_id.size() < 3)
        return out;
    read_file(sys_dir + "/GameSettings/" + game_id.substr(0, 3) + ".ini", out);
    if (game_id.size() >= 4)
        read_file(sys_dir + "/GameSettings/" + game_id.substr(0, 4) + ".ini", out);
    if (game_id.size() == 6)
        read_file(sys_dir + "/GameSettings/" + game_id + ".ini", out);
    /* Codes Dolphin keeps per disc revision (<ID>r1.ini...): listed by name,
     * once; Dolphin loads the running revision's lines. */
    const bool revisions = game_id.size() == 6 && widescreen::has_revisions(sys_dir, game_id);
    if (revisions)
        for (int r = 0; r < 10; ++r)
            read_file(sys_dir + "/GameSettings/" + game_id + "r" + std::to_string(r) + ".ini", out);
    /* Patches first (widescreen, frame rate), then the cheats. */
    std::stable_sort(out.begin(), out.end(), [](const Cheat &a, const Cheat &b) {
        return (a.kind == "OnFrame") > (b.kind == "OnFrame");
    });
    /* The widescreen collection's codes for the game, then the player's own. */
    if (game_id.size() == 6)
    {
        std::vector<Cheat> pack = widescreen::pack_codes(game_id);
        if (revisions)
            for (Cheat &c : pack)
                c.default_on = false; /* made for one revision of the disc: off unless turned on */
        bool pack_widescreen = false;
        for (const Cheat &c : pack)
            pack_widescreen |= c.default_on;
        if (!wii && !pack_widescreen && !widescreen::has_native(game_id))
            for (Cheat &c : out)
                if (!c.default_on && widescreen::is_widescreen_code(c.name))
                {
                    c.default_on = true; /* the one Widescreen turns on (ui_widescreen add_codes) */
                    break;
                }
        out.insert(out.end(), pack.begin(), pack.end());
    }
    /* The player's own after Dolphin's: on unless turned off. One with the
     * name of a code already listed is that code, now the player's (their
     * file defines it, and turns it on). */
    std::vector<Cheat> mine;
    for (const std::string &path : own_files(own_dir, game_id))
        read_file(path, mine);
    for (Cheat &m : mine)
    {
        Cheat *same = nullptr;
        for (Cheat &c : out)
            if (c.kind == m.kind && c.name == m.name)
                same = &c;
        Cheat &c = same ? *same : (out.push_back(m), out.back());
        c.own = true;
        c.pack = false;
        c.default_on = true;
    }
    return out;
}

bool add_own_cheats(const std::string &own_dir, const std::string &game_id, const std::string &ini_path,
                    bool (*off)(const Cheat &cheat, const void *user), const void *user)
{
    const std::vector<std::string> files = own_files(own_dir, game_id);
    if (files.empty())
        return false;
    /* Each code: its section, its $name and its lines, as the files have them. */
    struct Code
    {
        std::string kind, name;
        std::vector<std::string> lines;
    };
    std::vector<Code> codes;
    for (const std::string &path : files)
    {
        std::FILE *f = std::fopen(path.c_str(), "r");
        if (!f)
            continue;
        std::string section;
        char buf[512];
        bool skipping = false;
        while (std::fgets(buf, sizeof buf, f))
        {
            std::string line = buf;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            if (line.empty())
                continue;
            if (line[0] == '[')
            {
                const std::size_t end = line.find(']');
                section = end == std::string::npos ? "" : line.substr(1, end - 1);
                skipping = true;
                continue;
            }
            if (section != "OnFrame" && section != "ActionReplay" && section != "Gecko")
                continue;
            if (line[0] == '$')
            {
                skipping = false;
                for (const Code &c : codes)
                    skipping |= c.kind == section && c.name == line; /* a repeat (a later, less specific file) */
                if (!skipping)
                    codes.push_back({section, line, {}});
            }
            else if (!skipping && !codes.empty())
                codes.back().lines.push_back(line);
        }
        std::fclose(f);
    }
    if (codes.empty())
        return false;
    bool exists = false;
    if (std::FILE *probe = std::fopen(ini_path.c_str(), "r"))
    {
        exists = true;
        std::fclose(probe);
    }
    std::FILE *f = std::fopen(ini_path.c_str(), "a");
    if (!f)
        return false;
    if (!exists) /* Porpoise's mark first, as Settings::write_dolphin_game_ini writes it */
        std::fprintf(f, "# Written by Porpoise from this game's settings; changes here are replaced.\n");
    bool cheats = false;
    std::fprintf(f, "\n# The player's own codes (Porpoise's cheats folder)\n");
    for (const char *kind : {"OnFrame", "ActionReplay", "Gecko"})
    {
        bool any = false;
        for (const Code &c : codes)
            if (c.kind == kind)
            {
                if (!any)
                    std::fprintf(f, "\n[%s]\n", kind);
                any = true;
                std::fprintf(f, "%s\n", c.name.c_str());
                for (const std::string &l : c.lines)
                    std::fprintf(f, "%s\n", l.c_str());
            }
        bool header = false;
        for (const Code &c : codes)
            if (c.kind == kind)
            {
                Cheat cheat;
                cheat.kind = c.kind;
                cheat.name = c.name;
                cheat.default_on = cheat.own = true;
                if (off && off(cheat, user))
                    continue;
                if (!header)
                    std::fprintf(f, "\n[%s_Enabled]\n", kind);
                header = true;
                std::fprintf(f, "%s\n", list_name(c.kind, c.name).c_str());
                cheats |= c.kind != "OnFrame";
            }
    }
    std::fclose(f);
    return cheats;
}

std::string list_name(const std::string &kind, const std::string &name)
{
    if (kind != "Gecko")
        return name;
    std::string n = name.substr(0, name.find('['));
    while (!n.empty() && (n.back() == ' ' || n.back() == '\t'))
        n.pop_back();
    /* Dolphin also trims the start, after the '$'. */
    std::size_t i = n.empty() || n[0] != '$' ? 0 : 1;
    while (i < n.size() && (n[i] == ' ' || n[i] == '\t'))
        n.erase(i, 1);
    return n.size() > 1 ? n : name;
}

std::string cheat_key(const Cheat &cheat, bool on)
{
    return "dolphin." + cheat.kind + (on ? "_Enabled." : "_Disabled.") + cheat.name;
}

std::string cheat_help(const Cheat &cheat)
{
    std::string lower = cheat.name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (cheat.own)
        return tr("Your own code, from Porpoise's cheats folder. On unless you turn it off here.");
    if (cheat.pack)
        return cheat.default_on
                   ? tr("Widescreen code from Warped Polygon's collection. On when Widescreen is Auto or On.")
                   : tr("A code from Warped Polygon's widescreen collection. Off unless you turn it on.");
    /* The widescreen code Widescreen turns on for the game (ui_widescreen). */
    if (cheat.default_on && widescreen::is_widescreen_code(cheat.name))
        return tr("A widescreen code from Dolphin's list. On when Widescreen is Auto or On, with the picture at 16:9.");
    if (cheat.kind == "OnFrame")
    {
        if (lower.find("widescreen") != std::string::npos || lower.find("16:9") != std::string::npos)
            return tr("A patch from Dolphin: the game drawn in 16:9. Use it with Aspect Ratio on Force 16:9.");
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
