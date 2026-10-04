/* Porpoise UI - the game library.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A GameCube or Wii disc starts with its six-character ID (e.g. GABE01) and, at
 * 0x20, the game's title. ISO and GCM images hold that header at offset 0,
 * GameCube CISO after its 32 KiB block map, and Dolphin's WIA/RVZ keep a copy
 * of the first 0x80 bytes uncompressed at 0x58. GCZ and WBFS do not, so those
 * fall back to the file name. */
#include "ui_library.hpp"
#include "ui_i18n.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <sys/stat.h>

namespace porpoise::ui
{
namespace
{
std::uint32_t be32(const unsigned char *p)
{
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}

std::string lower(std::string s)
{
    for (char &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string extension(const std::string &name)
{
    const auto dot = name.rfind('.');
    return dot == std::string::npos ? std::string() : lower(name.substr(dot + 1));
}

bool is_game_file(const std::string &name)
{
    static const char *const known[] = {"iso", "gcm", "rvz", "ciso", "gcz", "wbfs", "wia"};
    const std::string ext = extension(name);
    for (const char *k : known)
        if (ext == k)
            return true;
    return false;
}

/* "My Game (USA).ciso" -> "My Game" */
std::string title_from_file(const std::string &file)
{
    std::string t = file;
    const auto dot = t.rfind('.');
    if (dot != std::string::npos)
        t = t.substr(0, dot);
    std::string out;
    int depth = 0;
    for (char c : t)
    {
        if (c == '(' || c == '[')
            ++depth;
        else if ((c == ')' || c == ']') && depth > 0)
            --depth;
        else if (depth == 0)
            out += (c == '_') ? ' ' : c;
    }
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    return out.empty() ? t : out;
}

std::string region_of(char code)
{
    switch (code)
    {
    case 'E': return "USA";
    case 'J': return "Japan";
    case 'K': return "Korea";
    case 'P': case 'X': case 'Y': return "Europe";
    case 'D': return "Germany";
    case 'F': return "France";
    case 'S': return "Spain";
    case 'I': return "Italy";
    case 'U': return "Australia";
    default: return "";
    }
}

void find_games(const std::string &dir, int depth, std::vector<std::string> &out)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return;
    std::vector<std::string> subdirs;
    while (dirent *entry = readdir(d))
    {
        const std::string name = entry->d_name;
        if (name.empty() || name[0] == '.')
            continue;
        const std::string path = dir + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            static const char *const skip[] = {"system", "cores", "info", "sce_sys", "sce_module", "licenses",
                                               "savefiles", "porpoise", "dolphin-emu", "homebrew", "Sys"};
            bool skipped = false;
            for (const char *k : skip)
                skipped |= name == k;
            if (!skipped)
                subdirs.push_back(path);
        }
        else if (is_game_file(name))
            out.push_back(path);
    }
    closedir(d);
    if (depth > 0)
        for (const std::string &sub : subdirs)
            find_games(sub, depth - 1, out);
}
} // namespace

int count_games(const std::string &dir, int depth)
{
    std::vector<std::string> files;
    find_games(dir, depth, files);
    return int(files.size());
}

bool read_disc_header(const std::string &path, Game &g)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    unsigned char head[0x400] = {};
    std::size_t got = std::fread(head, 1, sizeof head, f);
    long data = -1;
    std::size_t header_len = 0x400;
    if (got >= 4 && std::memcmp(head, "CISO", 4) == 0)
    {
        data = 0x8000;
        g.format = "CISO";
    }
    else if (got >= 4 && (std::memcmp(head, "RVZ\x01", 4) == 0 || std::memcmp(head, "WIA\x01", 4) == 0))
    {
        g.format = head[0] == 'R' ? "RVZ" : "WIA";
        std::memmove(head, head + 0x58, 0x80);
        std::memset(head + 0x80, 0, sizeof head - 0x80);
        header_len = 0x80;
        data = -2; /* already in hand */
    }
    else if (got >= 4 && be32(head) == 0xB10BC001)
    {
        g.format = "GCZ";
    }
    else if (got >= 4 && std::memcmp(head, "WBFS", 4) == 0)
    {
        g.format = "WBFS";
    }
    else
    {
        data = 0;
        const std::string ext = extension(g.file);
        g.format = ext == "gcm" ? "GCM" : "ISO";
    }
    if (data > 0)
    {
        std::fseek(f, data, SEEK_SET);
        got = std::fread(head, 1, sizeof head, f);
    }
    std::fclose(f);
    if (data == -1 || got < 0x60)
        return false;

    const bool gamecube = be32(head + 0x1C) == 0xC2339F3D;
    const bool wii = be32(head + 0x18) == 0x5D1C9EA3;
    if (!gamecube && !wii)
        return false;
    g.platform = wii ? "Wii" : "GameCube";
    std::string id;
    for (int i = 0; i < 6; ++i)
        id += std::isalnum(head[i]) ? char(head[i]) : '_';
    g.id = id;
    g.disc_number = head[6] <= 4 ? head[6] : 0; /* the disc number byte, 0 for the first */
    g.region = region_of(char(head[3]));

    std::string title;
    bool ascii = true;
    for (std::size_t i = 0x20; i < header_len && head[i]; ++i)
    {
        if (head[i] >= 0x80)
            ascii = false;
        title += char(head[i]);
    }
    while (!title.empty() && title.back() == ' ')
        title.pop_back();
    if (ascii && !title.empty())
        g.title = title;
    return true;
}

std::string relative_time(long long then, long long now)
{
    if (then <= 0)
        return tr("Not played yet");
    const long long d = now - then;
    if (d < 60 * 60)
        return tr("Played just now");
    if (d < 24 * 60 * 60)
        return tr("Played today");
    if (d < 2 * 24 * 60 * 60)
        return tr("Last played yesterday");
    if (d < 7 * 24 * 60 * 60)
        return trf("Last played {n} days ago", {{"n", std::to_string(d / (24 * 60 * 60))}});
    if (d < 30LL * 24 * 60 * 60)
        return trf("Last played {n} weeks ago", {{"n", std::to_string(d / (7 * 24 * 60 * 60))}});
    return tr("Last played a while ago");
}

std::string play_time_text(long long seconds)
{
    if (seconds < 60)
        return tr("Less than a minute");
    const long long m = seconds / 60, h = m / 60;
    if (h == 0)
        return trf("{m} min", {{"m", std::to_string(m)}});
    return trf("{h} h {m} min", {{"h", std::to_string(h)}, {"m", std::to_string(m % 60)}});
}

void Library::scan(const LibraryPaths &paths)
{
    paths_ = paths;
    games_.clear();
    std::vector<std::string> files;
    for (const std::string &root : paths.roots)
        find_games(root, 2, files);
    for (const std::string &root : paths.deep)
        find_games(root, 4, files);
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    for (const std::string &path : files)
    {
        Game g;
        g.path = path;
        g.file = path.substr(path.rfind('/') + 1);
        struct stat st;
        if (stat(path.c_str(), &st) == 0)
            g.bytes = std::uint64_t(st.st_size);
        read_disc_header(path, g);
        if (g.title.empty())
            g.title = title_from_file(g.file);
        if (g.platform.empty())
            g.platform = "GameCube";
        games_.push_back(std::move(g));
    }
    load_state();
    load_info();
    sort(sort_);
}

std::string Library::disc_path(const Game &g) const
{
    if (g.id.empty() || paths_.covers.empty())
        return "";
    const std::string p = paths_.covers + "/" + g.id + ".disc.png";
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? p : "";
}

std::string Library::back_path(const Game &g) const
{
    if (paths_.covers.empty())
        return "";
    for (const std::string &name : {g.id, g.file.substr(0, g.file.rfind('.'))})
    {
        if (name.empty())
            continue;
        const std::string p = paths_.covers + "/" + name + ".back.png";
        struct stat st;
        if (stat(p.c_str(), &st) == 0)
            return p;
    }
    return "";
}

namespace
{
std::string unescape_field(const std::string &s)
{
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            const char c = s[++i];
            out += c == 'n' ? '\n' : c == 't' ? '\t' : c;
        }
        else
            out += s[i];
    }
    return out;
}
} // namespace

void Library::load_info()
{
    if (paths_.info.empty())
        return;
    std::FILE *f = std::fopen(paths_.info.c_str(), "r");
    if (!f)
        return;
    std::string line;
    char buf[4096];
    while (std::fgets(buf, sizeof buf, f))
    {
        line += buf;
        if (line.empty() || line.back() != '\n')
            continue; /* a long line: keep reading */
        line.pop_back();
        std::vector<std::string> fields;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= line.size(); ++i)
            if (i == line.size() || line[i] == '\t')
            {
                fields.push_back(unescape_field(line.substr(start, i - start)));
                start = i + 1;
            }
        line.clear();
        if (fields.size() < 9)
            continue;
        for (Game &g : games_)
            if (g.id == fields[0])
            {
                g.db_title = fields[1];
                g.synopsis = fields[2];
                g.developer = fields[3];
                g.publisher = fields[4];
                g.released = fields[5];
                g.genre = fields[6];
                g.players = std::atoi(fields[7].c_str());
                g.rating = fields[8];
            }
    }
    std::fclose(f);
}

void Library::sort(Sort how)
{
    sort_ = how;
    std::stable_sort(games_.begin(), games_.end(), [how](const Game &a, const Game &b) {
        if (how == Sort::Recent && a.last_played != b.last_played)
            return a.last_played > b.last_played;
        if (how == Sort::MostPlayed && a.play_seconds != b.play_seconds)
            return a.play_seconds > b.play_seconds;
        if (how == Sort::Favourites && a.favourite != b.favourite)
            return a.favourite;
        return lower(a.title) < lower(b.title);
    });
}

std::string Library::cover_path(const Game &g) const
{
    const std::string stem = g.file.substr(0, g.file.rfind('.'));
    for (const std::string &name : {g.id, stem})
    {
        if (name.empty())
            continue;
        for (const char *ext : {".png", ".jpg", ".jpeg"})
        {
            const std::string p = paths_.covers + "/" + name + ext;
            struct stat st;
            if (stat(p.c_str(), &st) == 0)
                return p;
        }
    }
    return {};
}

void Library::mark_played(Game &g)
{
    g.last_played = static_cast<long long>(std::time(nullptr));
    selected_ = key_of(g);
    save();
}

void Library::add_play_time(Game &g, long long seconds)
{
    if (seconds <= 0)
        return;
    const long long total = g.play_seconds + seconds;
    const std::string key = key_of(g);
    for (Game &other : games_)
        if (key_of(other) == key)
            other.play_seconds = std::max(other.play_seconds, total);
    save();
}

void Library::toggle_favourite(Game &g)
{
    /* Every copy of the game (an ISO and an RVZ of the same disc) with it. */
    const bool on = !g.favourite;
    const std::string key = key_of(g);
    for (Game &other : games_)
        if (key_of(other) == key)
            other.favourite = on;
    save();
}

namespace
{
const char *sort_name(Library::Sort s)
{
    switch (s)
    {
    case Library::Sort::Recent: return "recent";
    case Library::Sort::MostPlayed: return "time";
    case Library::Sort::Favourites: return "favourites";
    default: return "title";
    }
}
} // namespace

void Library::load_state()
{
    kept_lines_.clear();
    std::FILE *f = std::fopen(paths_.state.c_str(), "r");
    if (!f)
        return;
    char line[600];
    while (std::fgets(line, sizeof line, f))
    {
        std::string s = line;
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
            s.pop_back();
        if (s.rfind("selected=", 0) == 0)
            selected_ = s.substr(9);
        else if (s.rfind("sort=", 0) == 0)
        {
            const std::string v = s.substr(5);
            sort_ = v == "recent" ? Sort::Recent : v == "time" ? Sort::MostPlayed
                                               : v == "favourites" ? Sort::Favourites : Sort::Title;
        }
        else if (s.rfind("played=", 0) == 0 || s.rfind("time=", 0) == 0 || s.rfind("fav=", 0) == 0)
        {
            /* played=<key> <unix time>, time=<key> <seconds>, fav=<key> */
            const auto eq = s.find('=');
            const bool fav = s[0] == 'f';
            const auto sp = fav ? std::string::npos : s.rfind(' ');
            if (!fav && sp == std::string::npos)
                continue;
            const std::string key = fav ? s.substr(eq + 1) : s.substr(eq + 1, sp - eq - 1);
            const long long n = fav ? 0 : std::atoll(s.c_str() + sp + 1);
            bool found = false;
            for (Game &g : games_)
                if (key_of(g) == key)
                {
                    found = true;
                    if (fav)
                        g.favourite = true;
                    else if (s[0] == 'p')
                        g.last_played = n;
                    else
                        g.play_seconds = n;
                }
            if (!found)
                kept_lines_.push_back(s);
        }
    }
    std::fclose(f);
}

void Library::save() const
{
    std::FILE *f = std::fopen(paths_.state.c_str(), "w");
    if (!f)
        return;
    std::fprintf(f, "selected=%s\nsort=%s\n", selected_.c_str(), sort_name(sort_));
    /* One line of each kind per key, however many copies of a game there are. */
    std::vector<std::string> done;
    for (const Game &g : games_)
    {
        const std::string key = key_of(g);
        if (std::find(done.begin(), done.end(), key) != done.end())
            continue;
        done.push_back(key);
        long long played = 0, seconds = 0;
        bool fav = false;
        for (const Game &o : games_)
            if (key_of(o) == key)
            {
                played = std::max(played, o.last_played);
                seconds = std::max(seconds, o.play_seconds);
                fav |= o.favourite;
            }
        if (played > 0)
            std::fprintf(f, "played=%s %lld\n", key.c_str(), played);
        if (seconds > 0)
            std::fprintf(f, "time=%s %lld\n", key.c_str(), seconds);
        if (fav)
            std::fprintf(f, "fav=%s\n", key.c_str());
    }
    for (const std::string &l : kept_lines_)
        std::fprintf(f, "%s\n", l.c_str());
    std::fclose(f);
}
} // namespace porpoise::ui
