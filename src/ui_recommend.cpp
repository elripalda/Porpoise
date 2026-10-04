/* Porpoise UI - recommended settings for a game, by its disc ID.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_recommend.hpp"

#include <algorithm>
#include <cstdio>
#include <map>

namespace porpoise::ui::recommend
{
namespace
{
std::string g_feed_path, g_settings_dir;
bool g_loaded = false;
std::map<std::string, Pick> g_picks;

std::string trim(std::string s)
{
    const char *ws = " \t\r\n";
    s.erase(0, s.find_first_not_of(ws));
    const auto end = s.find_last_not_of(ws);
    s.erase(end == std::string::npos ? 0 : end + 1);
    return s;
}

std::string upper(std::string s)
{
    for (char &c : s)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
    return s;
}

void load_feed()
{
    g_loaded = true;
    g_picks.clear();
    std::FILE *f = std::fopen(g_feed_path.c_str(), "r");
    if (!f)
        return;
    char line[1024];
    std::string id;
    while (std::fgets(line, sizeof line, f))
    {
        const std::string s = trim(line);
        if (s.empty() || s[0] == '#' || s[0] == ';')
            continue;
        if (s.front() == '[' && s.back() == ']')
        {
            id = upper(trim(s.substr(1, s.size() - 2)));
            continue;
        }
        const auto eq = s.find('=');
        if (id.empty() || eq == std::string::npos)
            continue;
        const std::string k = trim(s.substr(0, eq)), v = trim(s.substr(eq + 1));
        if (k == "note")
            g_picks[id].note = v;
        else if (!k.empty())
            g_picks[id].values.push_back({k, v});
    }
    std::fclose(f);
}

/* Dolphin's keys, said plainly. Unknown ones are shown as Dolphin names them. */
struct Known
{
    const char *key, *label;
    const char *on, *off; /* how True / False read; nullptr: show the number */
};
constexpr Known kKnown[] = {
    {"SafeTextureCacheColorSamples", "Texture cache accuracy", nullptr, nullptr},
    {"ImmediateXFBEnable", "Show frames immediately (XFB)", "On", "Off"},
    {"EFBToTextureEnable", "EFB copies", "To texture only", "To RAM too (accurate)"},
    {"XFBToTextureEnable", "XFB copies", "To texture only", "To RAM too (accurate)"},
    {"CPUThread", "Dual core", "On", "Off"},
    {"EnableJIT", "CPU recompiler", "On", "Off"},
    {"DSPHLE", "Audio emulation", "Fast (HLE)", "Accurate (LLE)"},
    {"ForceTextureFiltering", "Texture filtering", nullptr, nullptr},
    {"EFBAccessEnable", "CPU access to the EFB", "On", "Off"},
    {"EFBEmulateFormatChanges", "Emulate EFB format changes", "On", "Off"},
    {"DeferEFBCopies", "Defer EFB copies", "On", "Off"},
    {"MMU", "Full memory management (MMU)", "On", "Off"},
    {"VertexRounding", "Vertex rounding", "On", "Off"},
    {"FPRF", "Floating-point result flags", "On", "Off"},
    {"SyncOnSkipIdle", "Sync on idle skipping", "On", "Off"},
    {"CPUCull", "Cull vertices on the CPU", "On", "Off"},
    {"FastTextureSampling", "Fast texture sampling", "On", "Off"},
    {"ArbitraryMipmapDetection", "Arbitrary mipmap detection", "On", "Off"},
    {"MemoryCardSize", "Memory card size", nullptr, nullptr},
    {"BBoxEnable", "Bounding box", "On", "Off"},
    {"MSAA", "Anti-aliasing", nullptr, nullptr},
    {"DisableICache", "Instruction cache", "Off", "On"},
    {"VISkip", "Skip VI interrupts", "On", "Off"},
    {"FastDepthCalc", "Fast depth calculation", "On", "Off"},
    {"EarlyXFBOutput", "Early XFB output", "On", "Off"},
    {"PerfQueriesEnable", "Performance queries", "On", "Off"},
    {"ProgressiveScan", "Progressive scan", "On", "Off"},
    {"AccurateNaNs", "Accurate NaNs", "On", "Off"},
    {"LowDCBZHack", "Low DCBZ hack", "On", "Off"},
    {"OverclockEnable", "CPU overclock", "On", "Off"},
};
/* Settings that change nothing a player sees on PS5. */
constexpr const char *kSkip[] = {"StereoConvergence", "StereoEFBMonoDepth", "SuggestedAspectRatio", "HSPDevice",
                                 "RealWiiRemoteRepeatReports", "WidescreenHeuristicWidescreenRatio",
                                 "WidescreenHeuristicStandardRatio", "WidescreenHeuristicAspectRatioSlop"};

bool is_true(const std::string &v)
{
    return v == "True" || v == "true" || v == "1";
}

void read_fixes(const std::string &path, std::vector<Fix> &out)
{
    std::FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
        return;
    char line[1024];
    std::string section, comment;
    while (std::fgets(line, sizeof line, f))
    {
        const std::string s = trim(line);
        if (s.empty())
        {
            comment.clear();
            continue;
        }
        if (s[0] == '#')
        {
            const std::string c = trim(s.substr(1));
            /* Skip the file's own boilerplate. */
            if (c.rfind("Values set here", 0) == 0 || c.rfind("Add ", 0) == 0 || c.find(" - ") != std::string::npos)
                continue;
            comment += (comment.empty() ? "" : " ") + c;
            continue;
        }
        if (s.front() == '[')
        {
            section = s;
            comment.clear();
            continue;
        }
        /* Patches and cheats are lists of codes, not settings. */
        if (section == "[OnFrame]" || section == "[ActionReplay]" || section == "[Gecko]" ||
            section.find("Enabled]") != std::string::npos || s[0] == '$' || s[0] == '*')
            continue;
        const auto eq = s.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string k = trim(s.substr(0, eq)), v = trim(s.substr(eq + 1));
        bool skip = false;
        for (const char *x : kSkip)
            skip |= k == x;
        if (skip)
        {
            comment.clear();
            continue;
        }
        Fix fix;
        fix.label = k;
        fix.value = v;
        fix.why = comment;
        for (const Known &kn : kKnown)
            if (k == kn.key)
            {
                fix.label = kn.label;
                if (kn.on)
                    fix.value = is_true(v) ? kn.on : kn.off;
                else if (k == "SafeTextureCacheColorSamples")
                    fix.value = v == "0" ? "Safe" : v == "512" ? "Middle" : "Fast";
            }
        /* A later file for the same setting (the region's) wins. */
        for (auto it = out.begin(); it != out.end(); ++it)
            if (it->label == fix.label)
            {
                out.erase(it);
                break;
            }
        out.push_back(fix);
        comment.clear();
    }
    std::fclose(f);
}
} // namespace

void set_paths(const std::string &feed, const std::string &game_settings)
{
    g_feed_path = feed;
    g_settings_dir = game_settings;
    g_loaded = false;
}

void feed_changed()
{
    g_loaded = false;
}

std::vector<Fix> dolphin_fixes(const std::string &id)
{
    std::vector<Fix> out;
    if (id.size() < 3 || g_settings_dir.empty())
        return out;
    /* As Dolphin reads them: every region's file, then this region's. */
    read_fixes(g_settings_dir + "/" + id.substr(0, 3) + ".ini", out);
    if (id.size() >= 6)
        read_fixes(g_settings_dir + "/" + id.substr(0, 6) + ".ini", out);
    return out;
}

bool pick_for(const std::string &id, Pick &out)
{
    if (!g_loaded)
        load_feed();
    const std::string key = upper(id);
    for (const std::string &k : {key.substr(0, std::min<std::size_t>(6, key.size())),
                                 key.substr(0, std::min<std::size_t>(3, key.size()))})
    {
        const auto it = g_picks.find(k);
        if (!k.empty() && it != g_picks.end() && !it->second.values.empty())
        {
            out = it->second;
            return true;
        }
    }
    return false;
}
} // namespace porpoise::ui::recommend
