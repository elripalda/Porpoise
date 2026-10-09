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
std::string g_feed_path, g_settings_dir, g_bundled_path;
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

void read_feed(const std::string &path)
{
    std::FILE *f = std::fopen(path.c_str(), "r");
    if (!f)
        return;
    char line[1024];
    std::string id;
    std::map<std::string, Pick> here;
    while (std::fgets(line, sizeof line, f))
    {
        const std::string s = trim(line);
        if (s.empty() || s[0] == '#' || s[0] == ';')
            continue;
        if (s.front() == '[' && s.back() == ']')
        {
            id = upper(trim(s.substr(1, s.size() - 2)));
            here[id];
            continue;
        }
        const auto eq = s.find('=');
        if (id.empty() || eq == std::string::npos)
            continue;
        const std::string k = trim(s.substr(0, eq)), v = trim(s.substr(eq + 1));
        if (k == "note")
            here[id].note = v;
        else if (!k.empty())
            here[id].values.push_back({k, v});
    }
    std::fclose(f);
    for (auto &kv : here)
        g_picks[kv.first] = kv.second; /* a later file wins, game by game */
}

void load_feed()
{
    g_loaded = true;
    g_picks.clear();
    if (!g_bundled_path.empty())
        read_feed(g_bundled_path);
    read_feed(g_feed_path);
}

/* Dolphin's keys, said plainly. Unknown ones are shown as Dolphin names them. */
struct Known
{
    const char *key, *label;
    const char *on, *off; /* how True / False read; nullptr: show the number */
};
constexpr Known kKnown[] = {
    {"SafeTextureCacheColorSamples", "Texture Cache Accuracy", nullptr, nullptr},
    {"ImmediateXFBEnable", "Show Frames Immediately (XFB)", "On", "Off"},
    {"EFBToTextureEnable", "EFB Copies", "To Texture Only", "To RAM Too (Accurate)"},
    {"XFBToTextureEnable", "XFB Copies", "To Texture Only", "To RAM Too (Accurate)"},
    {"CPUThread", "Dual Core", "On", "Off"},
    {"EnableJIT", "CPU Recompiler", "On", "Off"},
    {"DSPHLE", "Audio Emulation", "Fast (HLE)", "Accurate (LLE)"},
    {"ForceTextureFiltering", "Texture Filtering", nullptr, nullptr},
    {"EFBAccessEnable", "CPU Access to the EFB", "On", "Off"},
    {"EFBEmulateFormatChanges", "Emulate EFB Format Changes", "On", "Off"},
    {"DeferEFBCopies", "Defer EFB Copies", "On", "Off"},
    {"MMU", "Full Memory Management (MMU)", "On", "Off"},
    {"VertexRounding", "Vertex Rounding", "On", "Off"},
    {"FPRF", "Floating-Point Result Flags", "On", "Off"},
    {"SyncOnSkipIdle", "Sync on Idle Skipping", "On", "Off"},
    {"CPUCull", "Cull Vertices on the CPU", "On", "Off"},
    {"FastTextureSampling", "Fast Texture Sampling", "On", "Off"},
    {"ArbitraryMipmapDetection", "Arbitrary Mipmap Detection", "On", "Off"},
    {"MemoryCardSize", "Memory Card Size", nullptr, nullptr},
    {"BBoxEnable", "Bounding Box", "On", "Off"},
    {"MSAA", "Anti-Aliasing", nullptr, nullptr},
    {"DisableICache", "Instruction Cache", "Off", "On"},
    {"VISkip", "Skip VI Interrupts", "On", "Off"},
    {"FastDepthCalc", "Fast Depth Calculation", "On", "Off"},
    {"EarlyXFBOutput", "Early XFB Output", "On", "Off"},
    {"PerfQueriesEnable", "Performance Queries", "On", "Off"},
    {"ProgressiveScan", "Progressive Scan", "On", "Off"},
    {"AccurateNaNs", "Accurate NaNs", "On", "Off"},
    {"LowDCBZHack", "Low DCBZ Hack", "On", "Off"},
    {"OverclockEnable", "CPU Overclock", "On", "Off"},
    {"EnableGPUTextureDecoding", "GPU Texture Decoding", "On", "Off"},
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
        fix.section = section.size() > 2 ? section.substr(1, section.size() - 2) : section;
        fix.key = k;
        fix.raw = v;
        if (v == "True" || v == "False" || v == "true" || v == "false")
        {
            fix.override_key = "dolphin." + fix.section + "." + k;
            fix.off_value = is_true(v) ? "False" : "True";
        }
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

void set_paths(const std::string &feed, const std::string &game_settings, const std::string &bundled)
{
    g_feed_path = feed;
    g_settings_dir = game_settings;
    g_bundled_path = bundled;
    g_loaded = false;
}

void describe_dolphin(const std::string &settings_key, const std::string &value, std::string &label,
                      std::string &value_text)
{
    const std::size_t dot = settings_key.rfind('.');
    const std::string k = dot == std::string::npos ? settings_key : settings_key.substr(dot + 1);
    label = k;
    value_text = value;
    for (const Known &kn : kKnown)
        if (k == kn.key)
        {
            label = kn.label;
            if (kn.on)
                value_text = is_true(value) ? kn.on : kn.off;
        }
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
