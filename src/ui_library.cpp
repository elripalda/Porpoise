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

#include <map>

#include "porpoise_atomic.hpp"
#include "porpoise_disc.hpp"
#include "porpoise_netfs.hpp"
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
    /* Discs, WADs (WiiWare, Virtual Console, channels) and GameCube demo
     * discs (.tgc). Homebrew (.dol, .elf) is found only beside its meta.xml
     * (find_games): an .elf alone could be anything, a PS5 payload too. */
    static const char *const known[] = {"iso", "gcm", "rvz", "ciso", "gcz", "wbfs", "wia", "wad", "tgc"};
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

long long now_ms()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Folders never searched: Porpoise's and the console's own, a PC's system
 * folders on a drive, and PS4 / PS5 game folders (a title ID: CUSA12345). */
bool skipped_folder(const std::string &name)
{
    static const char *const skip[] = {"system", "cores", "info", "sce_sys", "sce_module", "licenses",
                                       "savefiles", "porpoise", "dolphin-emu", "homebrew", "Sys"};
    for (const char *k : skip)
        if (name == k)
            return true;
    const std::string low = lower(name);
    for (const char *k : {"system volume information", "$recycle.bin", "recycler", "lost.dir", "found.000"})
        if (low == k)
            return true;
    if (name.size() >= 9)
    {
        const std::string head = name.substr(0, 4);
        bool digits = true;
        for (std::size_t i = 4; i < 9; ++i)
            digits &= std::isdigit((unsigned char)name[i]) != 0;
        if (digits && (name.size() == 9 || name[9] == '-' || name[9] == '_' || name[9] == ' ') &&
            (head == "CUSA" || head == "PPSA" || head == "PCSA" || head == "PCSE" || head == "PLAS" ||
             head == "NPXS"))
            return true;
    }
    return false;
}

/* deadline: 0 for none, else the clock (now_ms) at which a search gives up. */
void find_games(const std::string &dir, int depth, std::vector<std::string> &out, long long deadline = 0,
                bool *cut = nullptr)
{
    if (deadline && now_ms() > deadline)
    {
        if (cut)
            *cut = true;
        return;
    }
    /* The folder listing says what most entries are; a stat (slow on a big
     * exFAT drive) only for those it doesn't. A network share's (/net/) the
     * same way. */
    std::vector<porpoise::netfs::Entry> entries;
    if (!porpoise::netfs::list(dir, entries))
        return;
    std::vector<std::string> subdirs;
    bool has_meta = false;
    std::vector<std::string> apps; /* boot.dol / .elf: a Homebrew Channel app when meta.xml is beside it */
    for (const porpoise::netfs::Entry &entry : entries)
    {
        const std::string &name = entry.name;
        const std::string path = dir + "/" + name;
        const bool is_dir = entry.is_dir, is_file = !entry.is_dir;
        if (is_dir)
        {
            if (!skipped_folder(name))
                subdirs.push_back(path);
        }
        else if (!is_file)
            continue;
        else if (is_game_file(name))
            out.push_back(path);
        else if (extension(name) == "dol" || extension(name) == "elf")
            apps.push_back(path);
        else if (name == "meta.xml")
            has_meta = true;
    }
    if (has_meta)
        out.insert(out.end(), apps.begin(), apps.end());
    if (depth > 0)
        for (const std::string &sub : subdirs)
            find_games(sub, depth - 1, out, deadline, cut);
}
} // namespace

bool is_game_name(const std::string &name)
{
    return is_game_file(name);
}

int count_games(const std::string &dir, int depth)
{
    std::vector<std::string> files;
    find_games(dir, depth, files);
    return int(files.size());
}

bool read_disc_header(const std::string &path, Game &g)
{
    porpoise::netfs::Reader f; /* a file here or on a network share */
    if (!f.open(path))
        return false;
    unsigned char head[0x400] = {};
    const std::int64_t first = f.read_some(0, head, sizeof head);
    std::size_t got = first > 0 ? std::size_t(first) : 0;
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
    else if (got >= 4 && head[0] == 0x01 && head[1] == 0xC0 && head[2] == 0x0B && head[3] == 0xB1)
    {
        /* GCZ (its magic is little-endian) and WBFS keep the header inside:
         * the disc reader opens them. Without this they showed as GameCube
         * games named after the file, Wii games without their controls. */
        g.format = "GCZ";
        data = -2;
    }
    else if (got >= 4 && std::memcmp(head, "WBFS", 4) == 0)
    {
        g.format = "WBFS";
        data = -2;
    }
    else
    {
        data = 0;
        const std::string ext = extension(g.file);
        g.format = ext == "gcm" ? "GCM" : "ISO";
    }
    if (data > 0)
    {
        const std::int64_t more = f.read_some(std::uint64_t(data), head, sizeof head);
        got = more > 0 ? std::size_t(more) : 0;
    }
    f.close();
    if (g.format == "GCZ" || g.format == "WBFS")
    {
        std::string error;
        std::memset(head, 0, sizeof head);
        if (!porpoise::disc::read_header(path, head, error))
            return false;
        got = 0x100;
        header_len = 0x100;
    }
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

/* What a WAD is, by its title ID: a system channel, Virtual Console (by the
 * console it brings back), WiiWare, or another channel. */
std::string wad_kind(std::uint64_t title_id)
{
    const std::uint32_t high = std::uint32_t(title_id >> 32);
    const char first = char((title_id >> 24) & 0xFF);
    if (high == 0x00010002 || high == 0x00010008 || first == 'H')
        return "Channel";
    switch (first)
    {
    case 'F': case 'J': case 'N': case 'L': case 'M': case 'P': case 'Q': case 'E': case 'C': case 'X':
        return "Virtual Console";
    case 'W':
        return "WiiWare";
    default:
        return "Channel";
    }
}

bool read_wad_header(const std::string &path, Game &g)
{
    porpoise::disc::WadInfo info;
    std::string error;
    g.format = "WAD";
    g.platform = "Wii";
    g.kind = "Channel";
    if (!porpoise::disc::read_wad(path, info, nullptr, error))
    {
        /* The Wii Menu's WAD has no banner; name it so it can be found. */
        if (info.title_id == 0x0000000100000002ull)
            g.title = "Wii Menu";
        return false;
    }
    g.id = info.id;
    g.kind = wad_kind(info.title_id);
    if (info.id.size() >= 4)
        g.region = region_of(info.id[3]);
    /* Its English name, else its Japanese one; the WAD's own languages. */
    g.title = !info.names[1].empty() ? info.names[1] : info.names[0];
    return true;
}

/* A Homebrew Channel app: its name from meta.xml, its icon.png as its cover. */
bool read_app_meta(const std::string &path, Game &g)
{
    g.kind = "Homebrew";
    g.platform = "Wii";
    g.format = extension(g.file) == "elf" ? "ELF" : "DOL";
    const std::string dir = path.substr(0, path.rfind('/'));
    porpoise::netfs::Reader f; /* here or on a network share */
    if (!f.open(dir + "/meta.xml"))
        return false;
    std::string xml(8192, '\0');
    const std::int64_t got = f.read_some(0, &xml[0], xml.size());
    xml.resize(got > 0 ? std::size_t(got) : 0);
    f.close();
    const auto a = xml.find("<name>"), b = xml.find("</name>");
    if (a != std::string::npos && b != std::string::npos && b > a + 6 && b - a < 200)
        g.title = xml.substr(a + 6, b - a - 6);
    /* Without an ID, the app is known by its folder's name. */
    g.app_dir = dir;
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
    if (d < 14 * 24 * 60 * 60) /* "2 weeks" from 14 days: never "1 weeks" */
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

namespace
{
/* One game file as the library knows it: its header (or WAD, or app) read. */
Game game_from_file(const std::string &path)
{
    Game g;
    g.path = path;
    g.file = path.substr(path.rfind('/') + 1);
    std::uint64_t bytes = 0;
    if (porpoise::netfs::stat(path, nullptr, &bytes))
        g.bytes = bytes;
    const std::string ext = extension(g.file);
    if (ext == "wad")
        read_wad_header(path, g);
    else if (ext == "dol" || ext == "elf")
        read_app_meta(path, g);
    else
        read_disc_header(path, g);
    if (ext == "tgc")
        g.format = "TGC";
    if (g.title.empty())
        g.title = title_from_file(g.file);
    if (g.platform.empty())
        g.platform = "GameCube";
    return g;
}
} // namespace


std::vector<std::string> find_game_files(const std::vector<std::string> &roots, int limit_ms,
                                         std::vector<std::string> *cut)
{
    std::vector<std::string> files;
    /* Every place searched goes four folders deep, the usual ones and the
     * drives as much as the folders the player adds, so games sorted into
     * subfolders on a USB drive (games/Wii/Series/...) are found by themselves. */
    for (const std::string &root : roots)
    {
        bool was_cut = false;
        find_games(root, 4, files, limit_ms > 0 ? now_ms() + limit_ms : 0, &was_cut);
        if (was_cut && cut)
            cut->push_back(root);
    }
    return files;
}

void Library::scan(const LibraryPaths &paths)
{
    std::vector<std::string> roots = paths.roots;
    roots.insert(roots.end(), paths.deep.begin(), paths.deep.end());
    scan_files(paths, find_game_files(roots, 0, nullptr));
}

std::vector<Game> read_games(std::vector<std::string> files)
{
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    std::vector<Game> games;
    games.reserve(files.size());
    for (const std::string &path : files)
        games.push_back(game_from_file(path));
    return games;
}

void Library::scan_files(const LibraryPaths &paths, std::vector<std::string> files)
{
    scan_games(paths, read_games(std::move(files)));
}

void Library::scan_games(const LibraryPaths &paths, std::vector<Game> games)
{
    paths_ = paths;
    games_ = std::move(games);
    load_state();
    load_info();
    sort(sort_);
}

Game *Library::open_file(const std::string &path)
{
    const bool net = porpoise::netfs::is_net(path);
    bool is_dir = true;
    struct stat wanted;
    if (net ? !porpoise::netfs::stat(path, &is_dir) || is_dir
            : stat(path.c_str(), &wanted) != 0 || !S_ISREG(wanted.st_mode))
        return nullptr;
    /* The same file as one the search found, however its path is spelled (a
     * share's by its path). */
    for (Game &g : games_)
    {
        struct stat st;
        if (g.path == path || (!net && !porpoise::netfs::is_net(g.path) && stat(g.path.c_str(), &st) == 0 &&
                               st.st_dev == wanted.st_dev && st.st_ino == wanted.st_ino))
            return &g;
    }
    /* Outside every folder searched: in the library until the next search,
     * with its play history from library.txt like any other game. */
    games_.push_back(game_from_file(path));
    load_state();
    load_info();
    sort(sort_);
    for (Game &g : games_)
        if (g.path == path)
            return &g;
    return nullptr;
}

std::string Library::disc_path(const Game &g) const
{
    if (g.id.empty() || paths_.covers.empty())
        return "";
    const std::string p = paths_.covers + "/" + g.id + ".disc.png";
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? p : "";
}

std::string Library::spine_path(const Game &g) const
{
    std::string p = back_path(g);
    if (p.empty())
        return "";
    p = p.substr(0, p.size() - 9) + ".spine.png";
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
    const auto split = std::stable_partition(games_.begin(), games_.end(), [this](const Game &g) { return shows(g); });
    shown_ = int(split - games_.begin());
}

std::string Library::cover_path(const Game &g) const
{
    if (!g.app_dir.empty())
    {
        /* A homebrew app's own icon. */
        const std::string p = g.app_dir + "/icon.png";
        struct stat st;
        if (stat(p.c_str(), &st) == 0)
            return p;
    }
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
        else if (s.rfind("show=", 0) == 0)
            show_ = s == "show=wii"        ? Show::Wii
                    : s == "show=gamecube" ? Show::GameCube
                    : s == "show=channels" ? Show::Channels
                                           : Show::All;
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
    /* Written whole or not at all (porpoise_atomic.hpp): play time and
     * favourites survive a console unplugged as a game starts. */
    std::FILE *f = open_atomic(paths_.state);
    if (!f)
        return;
    std::fprintf(f, "selected=%s\nsort=%s\n", selected_.c_str(), sort_name(sort_));
    if (show_ != Show::All)
        std::fprintf(f, "show=%s\n", show_ == Show::Wii ? "wii" : show_ == Show::Channels ? "channels" : "gamecube");
    /* One line of each kind per key, however many copies of a game there are:
     * each key once, in the library's order, with its copies merged. */
    struct Merged
    {
        long long played = 0, seconds = 0;
        bool fav = false;
    };
    std::vector<std::string> order;
    std::map<std::string, Merged> merged;
    for (const Game &g : games_)
    {
        const std::string key = key_of(g);
        auto [it, fresh] = merged.try_emplace(key);
        if (fresh)
            order.push_back(key);
        it->second.played = std::max(it->second.played, g.last_played);
        it->second.seconds = std::max(it->second.seconds, g.play_seconds);
        it->second.fav |= g.favourite;
    }
    for (const std::string &key : order)
    {
        const Merged &m = merged[key];
        if (m.played > 0)
            std::fprintf(f, "played=%s %lld\n", key.c_str(), m.played);
        if (m.seconds > 0)
            std::fprintf(f, "time=%s %lld\n", key.c_str(), m.seconds);
        if (m.fav)
            std::fprintf(f, "fav=%s\n", key.c_str());
    }
    for (const std::string &l : kept_lines_)
        std::fprintf(f, "%s\n", l.c_str());
    finish_atomic(f, paths_.state);
}
} // namespace porpoise::ui
