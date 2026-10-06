/* Porpoise UI - widescreen per game (see ui_widescreen.hpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The codes are Warped Polygon's GameCube widescreen collection, cleaned by
 * tools/make-widescreen.py into assets/widescreen/codes.ini and native.txt. */
#include "ui_widescreen.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <set>

#include "ui_i18n.hpp"

namespace porpoise::ui::widescreen
{
namespace
{
struct Code
{
    std::string kind, name;
    std::vector<std::string> lines;
    bool enabled = false; /* a widescreen code: on with the plan (tools/make-widescreen.py chose) */
};

std::mutex g_lock;
std::string g_dir;
bool g_loaded = false;
std::map<std::string, std::vector<Code>> g_codes;
std::set<std::string> g_native;
std::set<std::string> g_needs_hack;
std::map<std::string, bool> g_revisions; /* game ID: Dolphin has <ID>rN.ini files */

std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;
    return s.substr(i);
}

/* Under g_lock. */
void load_locked()
{
    if (g_loaded)
        return;
    g_loaded = true;
    if (std::FILE *f = std::fopen((g_dir + "/codes.ini").c_str(), "r"))
    {
        char buf[512];
        std::string game, section;
        while (std::fgets(buf, sizeof buf, f))
        {
            const std::string line = trim(buf);
            if (line.empty() || line[0] == '#')
                continue;
            if (line.rfind("[[", 0) == 0)
            {
                game = line.substr(2, line.find("]]") == std::string::npos ? 0 : line.find("]]") - 2);
                section.clear();
                continue;
            }
            if (line[0] == '[')
            {
                section = line.substr(1, line.find(']') == std::string::npos ? 0 : line.find(']') - 1);
                continue;
            }
            if (game.empty() || section.empty())
                continue;
            if (section == "Porpoise")
            {
                if (line.rfind("widescreen_hack", 0) == 0 && line.find("True") != std::string::npos)
                    g_needs_hack.insert(game);
                continue;
            }
            std::vector<Code> &codes = g_codes[game];
            const bool list = section.size() > 8 && section.compare(section.size() - 8, 8, "_Enabled") == 0;
            if (list)
            {
                const std::string kind = section.substr(0, section.size() - 8);
                for (Code &c : codes)
                    if (c.kind == kind && c.name == line)
                        c.enabled = true;
            }
            else if (line[0] == '$')
                codes.push_back({section, line, {}, false});
            else if (!codes.empty() && codes.back().kind == section)
                codes.back().lines.push_back(line);
        }
        std::fclose(f);
    }
    if (std::FILE *f = std::fopen((g_dir + "/native.txt").c_str(), "r"))
    {
        char buf[64];
        while (std::fgets(buf, sizeof buf, f))
        {
            const std::string id = trim(buf);
            if (id.size() == 6 && id[0] != '#')
                g_native.insert(id);
        }
        std::fclose(f);
    }
}

std::vector<Code> codes_of(const std::string &game_id)
{
    std::lock_guard<std::mutex> lock(g_lock);
    load_locked();
    const auto it = g_codes.find(game_id);
    return it == g_codes.end() ? std::vector<Code>{} : it->second;
}

bool native(const std::string &game_id)
{
    std::lock_guard<std::mutex> lock(g_lock);
    load_locked();
    return g_native.count(game_id) > 0;
}

/* The first widescreen code in Dolphin's file for one revision of the disc
 * (<ID>r<N>.ini): its kind and name, or empty. */
std::pair<std::string, std::string> revision_code(const std::string &sys_dir, const std::string &game_id, int revision)
{
    if (revision < 0 || revision > 9 || sys_dir.empty())
        return {};
    std::FILE *f = std::fopen((sys_dir + "/GameSettings/" + game_id + "r" + std::to_string(revision) + ".ini").c_str(), "r");
    if (!f)
        return {};
    std::pair<std::string, std::string> found;
    std::string section;
    char buf[512];
    while (found.second.empty() && std::fgets(buf, sizeof buf, f))
    {
        const std::string line = trim(buf);
        if (line.empty() || line[0] == '#')
            continue;
        if (line[0] == '[')
        {
            section = line.substr(1, line.find(']') == std::string::npos ? 0 : line.find(']') - 1);
            continue;
        }
        if (line[0] == '$' && (section == "OnFrame" || section == "ActionReplay" || section == "Gecko") &&
            is_widescreen_code(line))
            found = {section, line};
    }
    std::fclose(f);
    return found;
}

/* Dolphin's own widescreen codes for the game (its Sys/GameSettings). */
std::vector<Cheat> dolphin_widescreen(const std::string &game_id, const std::string &sys_dir)
{
    std::vector<Cheat> out;
    if (sys_dir.empty())
        return out;
    for (const Cheat &c : cheats_for(sys_dir, game_id))
        if (!c.own && !c.pack && is_widescreen_code(c.name))
            out.push_back(c);
    return out;
}
} // namespace

bool has_revisions(const std::string &sys_dir, const std::string &game_id)
{
    if (game_id.size() != 6 || sys_dir.empty())
        return false;
    std::lock_guard<std::mutex> lock(g_lock);
    const auto it = g_revisions.find(game_id);
    if (it != g_revisions.end())
        return it->second;
    bool any = false;
    for (int r = 0; r < 10 && !any; ++r)
        if (std::FILE *f = std::fopen((sys_dir + "/GameSettings/" + game_id + "r" + std::to_string(r) + ".ini").c_str(), "r"))
        {
            std::fclose(f);
            any = true;
        }
    g_revisions[game_id] = any;
    return any;
}

void set_dir(const std::string &dir)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (dir == g_dir)
        return;
    g_dir = dir;
    g_loaded = false;
    g_codes.clear();
    g_native.clear();
    g_needs_hack.clear();
}

bool is_widescreen_code(const std::string &name)
{
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return lower.find("widescreen") != std::string::npos || lower.find("16:9") != std::string::npos ||
           lower.find("16x9") != std::string::npos;
}

bool code_needs_hack(const std::string &game_id)
{
    std::lock_guard<std::mutex> lock(g_lock);
    load_locked();
    return g_needs_hack.count(game_id) > 0;
}

bool has_native(const std::string &game_id)
{
    return native(game_id);
}

Kind kind_of(const std::string &game_id, const std::string &sys_dir, bool wii, int revision)
{
    if (wii)
        return Kind::Native; /* the Wii's own 16:9 setting */
    if (game_id.size() != 6)
        return Kind::None;
    if (has_revisions(sys_dir, game_id))
    {
        /* Codes per revision: the running one's, when it has one (a revision
         * not known yet: whether any has one). */
        const bool code = revision >= 0 ? !revision_code(sys_dir, game_id, revision).second.empty()
                                        : !dolphin_widescreen(game_id, sys_dir).empty();
        if (code)
            return Kind::Patch;
        return native(game_id) ? Kind::Native : Kind::None;
    }
    for (const Code &c : codes_of(game_id))
        if (c.enabled)
            return Kind::Patch;
    /* Its own 16:9 option before Dolphin's codes: those may be fixes of
     * another kind for a game that needs none. */
    if (native(game_id))
        return Kind::Native;
    return dolphin_widescreen(game_id, sys_dir).empty() ? Kind::None : Kind::Patch;
}

Plan plan_for(int mode, Kind kind, bool needs_hack)
{
    if (mode == ModeOff)
        return Plan::Standard;
    if (kind == Kind::Patch)
        return needs_hack ? Plan::PatchHack : Plan::Patch;
    if (kind == Kind::Native)
        return Plan::Native;
    return mode == ModeOn ? Plan::Hack : Plan::Standard;
}

std::vector<Cheat> pack_codes(const std::string &game_id)
{
    std::vector<Cheat> out;
    for (const Code &c : codes_of(game_id))
    {
        Cheat cheat;
        cheat.kind = c.kind;
        cheat.name = c.name;
        cheat.default_on = c.enabled;
        cheat.pack = true;
        out.push_back(cheat);
    }
    return out;
}

bool add_codes(const std::string &game_id, const std::string &sys_dir, const std::string &ini_path, Plan plan,
               bool (*off)(const Cheat &, const void *), const void *user, bool &widescreen_on, int revision)
{
    widescreen_on = false;
    const std::vector<Code> codes = codes_of(game_id);
    /* Which to turn on: the collection's widescreen codes, or else Dolphin's. */
    std::vector<std::pair<std::string, std::string>> on; /* kind, name */
    auto turned_off = [&](const std::string &kind, const std::string &name) {
        Cheat c;
        c.kind = kind;
        c.name = name;
        c.default_on = true;
        return off && off(c, user);
    };
    bool pack_widescreen = false;
    const bool revisions = has_revisions(sys_dir, game_id);
    for (const Code &c : codes)
        if (c.enabled && !revisions) /* a collection code is made for one revision of the disc */
        {
            pack_widescreen = true;
            if ((plan == Plan::Patch || plan == Plan::PatchHack) && !turned_off(c.kind, c.name))
                on.emplace_back(c.kind, c.name);
        }
    if (revisions && (plan == Plan::Patch || plan == Plan::PatchHack))
    {
        /* The running revision's own code (Dolphin loads its file), or none:
         * then the game stays 4:3 (widescreen_on false). */
        const auto rc = revision_code(sys_dir, game_id, revision);
        if (!rc.second.empty() && !turned_off(rc.first, rc.second))
            on.emplace_back(rc.first, rc.second);
    }
    else if (plan == Plan::Patch && !pack_widescreen)
    {
        const std::vector<Cheat> own = dolphin_widescreen(game_id, sys_dir);
        if (!own.empty() && !turned_off(own.front().kind, own.front().name))
            on.emplace_back(own.front().kind, own.front().name);
    }
    if (codes.empty() && on.empty())
        return false;
    widescreen_on = !on.empty();
    bool exists = false;
    if (std::FILE *probe = std::fopen(ini_path.c_str(), "r"))
    {
        exists = true;
        std::fclose(probe);
    }
    std::FILE *f = std::fopen(ini_path.c_str(), "a");
    if (!f)
    {
        widescreen_on = false;
        return false;
    }
    if (!exists) /* Porpoise's mark first, as Settings::write_dolphin_game_ini writes it */
        std::fprintf(f, "# Written by Porpoise from this game's settings; changes here are replaced.\n");
    std::fprintf(f, "\n# Widescreen codes (Warped Polygon's collection)\n");
    bool cheats = false;
    for (const char *kind : {"OnFrame", "ActionReplay", "Gecko"})
    {
        bool header = false;
        for (const Code &c : codes)
            if (c.kind == kind)
            {
                if (!header)
                    std::fprintf(f, "\n[%s]\n", kind);
                header = true;
                std::fprintf(f, "%s\n", c.name.c_str());
                for (const std::string &l : c.lines)
                    std::fprintf(f, "%s\n", l.c_str());
            }
        header = false;
        for (const auto &[k, name] : on)
            if (k == kind)
            {
                if (!header)
                    std::fprintf(f, "\n[%s_Enabled]\n", kind);
                header = true;
                std::fprintf(f, "%s\n", list_name(k, name).c_str());
                cheats |= k != "OnFrame";
            }
    }
    std::fclose(f);
    return cheats;
}

std::string about(Kind kind)
{
    switch (kind)
    {
    case Kind::Patch:
        return tr("This game has a widescreen code: Auto and On draw it in 16:9 the way the game itself would, with "
                  "nothing popping in at the edges.");
    case Kind::Native:
        return tr("This game has a 16:9 option of its own: turn it on in the game's options, and the picture "
                  "follows.");
    case Kind::None:
    default:
        return tr("This game has no widescreen code. On uses Dolphin's emulated widescreen hack, which can glitch: "
                  "things at the edges of the screen may pop in and out or disappear.");
    }
}
} // namespace porpoise::ui::widescreen
