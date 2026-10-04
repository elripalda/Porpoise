/* Porpoise - screen borders: what fills the bars beside a 4:3 picture.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_borders.hpp"

#include <algorithm>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

namespace porpoise::borders
{
namespace
{
std::string g_assets = "/app0/assets";
std::string g_data = "/data/porpoise";

struct BuiltIn
{
    const char *name, *label;
};
constexpr BuiltIn kBuiltIn[] = {
    {"glass", "Porpoise glass"},
    {"midnight", "Midnight"},
    {"arcade", "Arcade cabinet"},
};

bool exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}

bool ends_with_png(const std::string &n)
{
    return n.size() > 4 && strcasecmp(n.c_str() + n.size() - 4, ".png") == 0;
}
} // namespace

void set_dirs(const std::string &asset_dir, const std::string &data_dir)
{
    g_assets = asset_dir;
    g_data = data_dir;
}

std::string user_dir()
{
    const std::string dir = g_data + "/borders";
    mkdir(dir.c_str(), 0777);
    return dir;
}

std::vector<Border> list()
{
    std::vector<Border> out;
    out.push_back({"", "None", true});
    for (const BuiltIn &b : kBuiltIn)
        out.push_back({b.name, b.label, true});
    std::vector<std::string> mine;
    if (DIR *d = opendir(user_dir().c_str()))
    {
        while (dirent *e = readdir(d))
        {
            const std::string n = e->d_name;
            if (n[0] != '.' && ends_with_png(n))
                mine.push_back(n.substr(0, n.size() - 4));
        }
        closedir(d);
    }
    std::sort(mine.begin(), mine.end(), [](const std::string &a, const std::string &b) {
        return strcasecmp(a.c_str(), b.c_str()) < 0;
    });
    for (const std::string &n : mine)
    {
        bool taken = false;
        for (Border &b : out)
            if (b.name == n)
            {
                taken = true; /* the player's own file replaces the built-in one */
                b.built_in = false;
                b.label = n;
            }
        if (!taken)
            out.push_back({n, n, false});
    }
    return out;
}

std::string path_of(const std::string &name)
{
    if (name.empty() || name.find('/') != std::string::npos || name.find("..") != std::string::npos)
        return "";
    const std::string mine = g_data + "/borders/" + name + ".png";
    if (exists(mine))
        return mine;
    if (exists(g_data + "/borders/" + name + ".PNG"))
        return g_data + "/borders/" + name + ".PNG";
    const std::string shipped = g_assets + "/borders/" + name + ".png";
    return exists(shipped) ? shipped : "";
}
} // namespace porpoise::borders
