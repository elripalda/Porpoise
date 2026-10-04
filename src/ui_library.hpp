/* Porpoise UI - the game library: what is on the console, and what was played.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace porpoise::ui
{
struct Texture;

struct Game
{
    std::string path;
    std::string file;      /* file name */
    std::string id;        /* six-character disc ID, e.g. GABE01; empty if unreadable */
    std::string title;     /* from the disc header, else the file name */
    std::string format;    /* ISO, RVZ, CISO, ... */
    std::string platform;  /* GameCube or Wii */
    std::string region;    /* USA, Europe, Japan, ... */
    std::uint64_t bytes = 0;
    long long last_played = 0; /* unix time, 0 = never */
    long long play_seconds = 0; /* time played in Porpoise, all told */
    bool favourite = false;
    Texture *cover = nullptr;
    bool cover_tried = false;
    /* From GameTDB, when downloaded (Settings > Games > Download game info). */
    std::string synopsis, developer, publisher, released, genre, rating;
    std::string db_title; /* GameTDB's name for it */
    int players = 0;
    Texture *disc = nullptr; /* the disc's label art */
    bool disc_tried = false;
    Texture *back = nullptr; /* the back of the box */
    bool back_tried = false;
};

/* Game files in dir and up to depth folders below it. */
int count_games(const std::string &dir, int depth);

struct LibraryPaths
{
    std::vector<std::string> roots;   /* folders searched for games, two levels down */
    std::vector<std::string> deep;    /* folders the player chose, searched four levels down */
    std::string covers;               /* <covers>/<ID>.png|.jpg */
    std::string state;                /* small text file: last played, selection */
    std::string info;                 /* GameTDB details, one game per line (info.tsv) */
};

class Library
{
public:
    void scan(const LibraryPaths &paths);
    std::vector<Game> &games() { return games_; }
    const LibraryPaths &paths() const { return paths_; }

    enum class Sort
    {
        Title,
        Recent,
        MostPlayed,
        Favourites, /* favourites first, then by title */
    };
    void sort(Sort how);
    Sort sort_order() const { return sort_; }

    /* Selection and play history, kept across launches. */
    std::string selected_id() const { return selected_; }
    void set_selected(const std::string &key) { selected_ = key; }
    void mark_played(Game &g);
    void add_play_time(Game &g, long long seconds);
    void toggle_favourite(Game &g);
    void save() const;

    static std::string key_of(const Game &g) { return g.id.empty() ? g.file : g.id; }
    std::string cover_path(const Game &g) const;
    std::string disc_path(const Game &g) const; /* <covers>/<ID>.disc.png, or "" */
    std::string back_path(const Game &g) const; /* <covers>/<ID>.back.png, or "" */
    /* Reads info.tsv into the games (after a scan, or when it was downloaded). */
    void load_info();
    /* Another info.tsv (the descriptions in another language). */
    void set_info_path(const std::string &path) { paths_.info = path; }

private:
    void load_state();
    LibraryPaths paths_;
    std::vector<Game> games_;
    std::string selected_;
    Sort sort_ = Sort::Title;
    /* History of games not found in this search (a USB drive that's out):
     * kept, so it is there when they are back. */
    std::vector<std::string> kept_lines_;
};

/* Reads a disc image's header: ID, title, platform. Returns false if the
 * format keeps its header compressed (GCZ, WBFS). Public for the preview tool. */
bool read_disc_header(const std::string &path, Game &g);
std::string relative_time(long long then, long long now);
/* "3 h 20 min", "45 min", "Less than a minute" (translated). */
std::string play_time_text(long long seconds);
} // namespace porpoise::ui
