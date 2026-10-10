/* Porpoise UI - a game's cheats and patches, as Dolphin ships them.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_cheats.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "ui_i18n.hpp"
#include "ui_widescreen.hpp"

namespace porpoise::ui
{
namespace
{
/* "0123ABCD 4567EF01": a line of a Gecko or Action Replay code. */
bool code_line(const std::string &l)
{
    if (l.size() < 17 || l[8] != ' ')
        return false;
    for (int i = 0; i < 17; ++i)
        if (i != 8 && !std::isxdigit(static_cast<unsigned char>(l[std::size_t(i)])))
            return false;
    return true;
}

bool ends_with(const std::string &s, const char *tail)
{
    const std::size_t n = std::strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

/* A .txt of bare code lines (as WiiLink WFC hands out a game's code): one
 * Gecko code, named after its file. */
std::string bare_name(const std::string &path)
{
    return "$Codes from " + path.substr(path.rfind('/') + 1);
}

void read_file(const std::string &path, std::vector<Cheat> &out)
{
    std::FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
        return;
    const bool bare = ends_with(path, ".txt");
    std::string section = bare ? "Gecko" : "";
    bool named = false;
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
        if (bare && !named && section == "Gecko" && code_line(line))
        {
            /* Code lines before any name: the file's own code. */
            named = true;
            if (!find("Gecko", bare_name(path)))
                out.push_back({"Gecko", bare_name(path), false});
            continue;
        }
        if (line[0] != '$' || line.size() < 2 || line.find('=') != std::string::npos)
            continue;
        named = true;
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
        const bool bare = ends_with(path, ".txt");
        std::string section = bare ? "Gecko" : "";
        char buf[512];
        /* A .txt's lines before its code (a title, a note) belong to nothing. */
        bool skipping = bare, file_named = false;
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
                skipping = true;
                continue;
            }
            if (section != "OnFrame" && section != "ActionReplay" && section != "Gecko")
                continue;
            if (line[0] == '$')
            {
                file_named = true;
                skipping = false;
                for (const Code &c : codes)
                    skipping |= c.kind == section && c.name == line; /* a repeat (a later, less specific file) */
                if (!skipping)
                    codes.push_back({section, line, {}});
            }
            else if (bare && !file_named && section == "Gecko" && code_line(line))
            {
                file_named = true;
                /* Code lines before any name: the file's own code. */
                const std::string name = bare_name(path);
                bool have = false;
                for (const Code &c : codes)
                    have |= c.kind == "Gecko" && c.name == name;
                if (!have)
                    codes.push_back({"Gecko", name, {line}});
                skipping = have;
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
bool add_online_code(const std::string &ini_path)
{
    /* Once, as the game starts (it writes a blr over its own first word):
     * every "https" in the game's memory from 0x80003000 on loses its 's'
     * (the rest of the string moves up a byte), so the login server is asked
     * over plain HTTP. */
    static const char *const kLines[] = {
        "C0000000 0000000E", "3C004E80 60000020", "900F0000 3D808000", "618C3000 3C00017F",
        "6000CFFC 7C0903A6", "3D607474 616B7073", "800C0000 7C005800", "40A20034 394C0003",
        "392C0002 7D455378", "38600000 8C050001", "2C000000 38630001", "4082FFF4 8C0A0001",
        "9C090001 3463FFFF", "4082FFF4 398C0001", "4200FFC0 4E800020",
    };
    const char *const name = "$Porpoise: custom server (https to http)";
    bool exists = false;
    if (std::FILE *probe = std::fopen(ini_path.c_str(), "r"))
    {
        exists = true;
        std::fclose(probe);
    }
    std::FILE *f = std::fopen(ini_path.c_str(), "a");
    if (!f)
        return false;
    if (!exists)
        std::fprintf(f, "# Written by Porpoise from this game's settings; changes here are replaced.\n");
    std::fprintf(f, "\n# Online: a custom server (Settings > Online)\n[Gecko]\n%s\n", name);
    for (const char *l : kLines)
        std::fprintf(f, "%s\n", l);
    std::fprintf(f, "\n[Gecko_Enabled]\n%s\n", name);
    return std::fclose(f) == 0;
}
} // namespace porpoise::ui
