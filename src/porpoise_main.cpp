/* Porpoise - the title's entry point: the launcher, then the game.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Startup keeps what Mihawk's PS5 RetroArch learned the console needs
 * (its src/main.cpp, GPL-3.0-or-later): a trace file, since a title's stderr
 * reaches nothing; the buffered log and its flusher thread; the terminate
 * handler that names an exception before abort; the crash report; core thread
 * tracking; and leaving through the shell rather than exit().
 *
 * Then: the launcher (src/ui_*.cpp) on Porpoise's own Vulkan device; on Play,
 * the launch screen, the hand-over of the display to Dolphin's device, and the
 * game (src/porpoise_core.cpp). */
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cxxabi.h>
#include <dirent.h>
#include <exception>
#include <pthread.h>
#include <string>
#include <sys/stat.h>
#include <time.h>
#include <typeinfo>
#include <unistd.h>

#include "../build/title_build_identity.h"
#include "memory_diagnostics.hpp"
#include "porpoise_audio.hpp"
#include "porpoise_borders.hpp"
#include "porpoise_core.hpp"
#include "porpoise_banner.hpp"
#include "porpoise_covers.hpp"
#include "porpoise_jailbreak.hpp"
#include "porpoise_pacer.hpp"
#include "porpoise_aim.hpp"
#include "porpoise_pad.hpp"
#include "porpoise_sound.hpp"
#include "porpoise_states.hpp"
#include "porpoise_update.hpp"
#include "porpoise_vk.hpp"
#include "title_threads.hpp"
#include "trace.hpp"
#include "ui_app.hpp"
#include "ui_gfx.hpp"
#include "ui_i18n.hpp"
#include "ui_library.hpp"
#include "ui_recommend.hpp"
#include "ui_setups.hpp"
#include "ui_settings.hpp"

extern "C"
{
    int sceSystemServiceHideSplashScreen();
    int sceSystemServiceParamGetInt(int param, int *value);
    int sceSystemServiceLoadExec(const char *, const char *const *);
    void ps5_vulkan_profile_init();
    void ps5_crash_report_install();
    void ps5_core_threads_start();
    void ps5_sampler_start();
    void ps5_open_permissions();
    void ps5_memory_report(const char *, size_t, int);
}

namespace
{
/* Where the player's things live: /data/porpoise, outside the app folder, so
 * reinstalling Porpoise keeps games, saves, settings and covers. If the
 * sandbox will not let Porpoise write there, the app's own folder is used. */
std::string g_data = "/app0/porpoise";
std::string g_settings_path, g_options_path, g_saves_path, g_options_reference, g_core_log;

bool g_sandboxed = false; /* /data out of reach even after asking the HEN */

/* Copies a file or a whole folder; what's already at `to` is left alone. */
void copy_tree(const std::string &from, const std::string &to, int depth = 0)
{
    struct stat st;
    if (depth > 8 || stat(from.c_str(), &st) != 0)
        return;
    if (S_ISDIR(st.st_mode))
    {
        mkdir(to.c_str(), 0777);
        if (DIR *d = opendir(from.c_str()))
        {
            while (dirent *e = readdir(d))
                if (std::strcmp(e->d_name, ".") != 0 && std::strcmp(e->d_name, "..") != 0)
                    copy_tree(from + "/" + e->d_name, to + "/" + e->d_name, depth + 1);
            closedir(d);
        }
        return;
    }
    if (stat(to.c_str(), &st) == 0)
        return;
    std::FILE *in = std::fopen(from.c_str(), "rb");
    if (!in)
        return;
    const std::string part = to + ".part";
    std::FILE *out = std::fopen(part.c_str(), "wb");
    bool ok = out != nullptr;
    char buf[65536];
    std::size_t n;
    while (ok && (n = std::fread(buf, 1, sizeof buf, in)) > 0)
        ok = std::fwrite(buf, 1, n, out) == n;
    std::fclose(in);
    if (out)
        ok = std::fclose(out) == 0 && ok;
    if (!ok || std::rename(part.c_str(), to.c_str()) != 0)
        std::remove(part.c_str());
}

/* A player whose Porpoise 1.0 couldn't reach /data kept everything in the
 * app's own folder. Now that /data is open, their settings, memory cards,
 * save states and the rest come along once (games stay where they are and are
 * still found there; covers download again). */
void bring_over_app_folder_data()
{
    const std::string old_dir = "/app0/porpoise";
    struct stat st;
    if (stat((old_dir + "/settings.ini").c_str(), &st) != 0 || stat((g_data + "/settings.ini").c_str(), &st) == 0)
        return;
    ps5::debug::mark("main: bringing the player's things over from the app folder");
    for (const char *item : {"settings.ini", "library.txt", "game-settings", "states", "setups", "lang", "saves/User/GC",
                             "saves/User/Wii", "saves/User/Config", "saves/User/GameSettings"})
    {
        const std::string rel = item;
        if (rel.rfind("saves/User/", 0) == 0)
        {
            mkdir((g_data + "/saves").c_str(), 0777);
            mkdir((g_data + "/saves/User").c_str(), 0777);
        }
        copy_tree(old_dir + "/" + rel, g_data + "/" + rel);
    }
}

void choose_data_dir()
{
    if (porpoise::jailbreak::ensure())
    {
        g_data = "/data/porpoise";
        bring_over_app_folder_data();
    }
    else
        g_sandboxed = true;
    mkdir(g_data.c_str(), 0777);
    mkdir((g_data + "/covers").c_str(), 0777);
    mkdir((g_data + "/games").c_str(), 0777);
    mkdir((g_data + "/saves").c_str(), 0777);
    g_settings_path = g_data + "/settings.ini";
    g_options_path = g_data + "/options.ini";
    g_saves_path = g_data + "/saves";
    g_options_reference = g_data + "/options-reference.txt";
    g_core_log = "/app0/porpoise/core.log";
    ps5::debug::mark(("main: player data in " + g_data).c_str());
    /* Test files the Dolphin core still reads: worth knowing about when a
     * setting doesn't seem to take. */
    struct stat st;
    if (stat("/app0/dolphin-options.txt", &st) == 0)
        ps5::debug::mark("main: /app0/dolphin-options.txt is there: its options win over Porpoise's");
    if (stat("/app0/dolphin-debug.txt", &st) == 0)
        ps5::debug::mark("main: /app0/dolphin-debug.txt is there: the shader cache is off while it is");
}

porpoise::ui::Gfx g_gfx;
porpoise::ui::Library g_library;
porpoise::Settings g_settings;
porpoise::Settings g_play; /* what the game being played uses: g_settings + its own */
porpoise::ui::Game *g_playing = nullptr;
bool g_menu_open = false;
porpoise::ui::App g_app;
double g_time = 0;

void *log_flusher(void *)
{
    const timespec interval = {0, 250 * 1000 * 1000};
    for (;;)
    {
        nanosleep(&interval, nullptr);
        std::fflush(nullptr);
    }
    return nullptr;
}

void start_log_flusher()
{
    pthread_t thread;
    if (create_title_thread(&thread, log_flusher, nullptr) == 0)
        pthread_detach(thread);
}

void on_terminate()
{
    const std::type_info *type = abi::__cxa_current_exception_type();
    if (!type)
    {
        ps5::debug::mark("terminate: called with no active exception");
        std::fflush(stderr);
        std::abort();
    }
    int status = 0;
    char *pretty = abi::__cxa_demangle(type->name(), nullptr, nullptr, &status);
    char line[256];
    std::snprintf(line, sizeof line, "terminate: exception type=%s",
                  (status == 0 && pretty) ? pretty : type->name());
    std::free(pretty);
    ps5::debug::mark(line);
    std::fflush(stderr);
    std::abort();
}

[[noreturn]] void leave(int status)
{
    ps5::debug::mark_value("main: leaving through the shell, status", status);
    porpoise::audio::close();
    porpoise::pad::close();
    ps5::memory::finish();
    std::fflush(nullptr);
    const int result = sceSystemServiceLoadExec("exit", nullptr);
    ps5::debug::mark_value("main: LoadExec result", result);
    for (;;)
        usleep(100000); /* the shell ends the process asynchronously */
}

long long now_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<long long>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

/* The launcher's frame pacing (porpoise_pacer.hpp): one frame per vblank
 * when the display holds the loop to it, its own clock otherwise. */
porpoise::pacer::Pacer g_pacer;

/* Before each frame's render pass: the pixels of textures that changed
 * (a Wii disc's banner playing) go into them. */
void prepass(VkCommandBuffer cmd, void *)
{
    if (g_gfx.ready())
        g_gfx.record_uploads(cmd);
}

/* The UI is recorded into every presented frame, after the core's picture. */
void overlay(VkCommandBuffer cmd, unsigned, void *)
{
    if (g_gfx.ready())
        g_gfx.record(cmd);
}

bool start_gfx()
{
    const porpoise::vk::Context c = porpoise::vk::context();
    porpoise::ui::GfxInit init;
    init.instance = c.instance;
    init.gpu = c.gpu;
    init.device = c.device;
    init.queue = c.queue;
    init.queue_family = c.queue_family;
    init.render_pass = c.render_pass;
    init.slots = c.slots;
    init.get_instance_proc = c.get_instance_proc;
    init.get_device_proc = c.get_device_proc;
    init.queue_mutex = c.queue_mutex;
    init.asset_dir = "/app0/assets";
    const long long t0 = now_ns();
    const bool ok = g_gfx.init(init);
    ps5::debug::mark_value(ok ? "ui: renderer ready, ms" : "ui: renderer FAILED, ms", (now_ns() - t0) / 1000000);
    return ok;
}

void begin_ui_frame(float dim)
{
    g_gfx.begin(porpoise::vk::current_slot(), float(porpoise::vk::screen_width()),
                float(porpoise::vk::screen_height()), float(g_time), dim, g_settings.reduced_motion);
}

/* GameTDB's descriptions in the menus' language: Spanish, French, Portuguese
 * and Italian have their own table; English and Japanese use the English one
 * (the Japanese font holds only the menus' own characters). */
const char *info_lang()
{
    switch (porpoise::ui::language())
    {
    case porpoise::ui::Language::Spanish:
    case porpoise::ui::Language::SpanishLatinAmerica: return "ES";
    case porpoise::ui::Language::French: return "FR";
    case porpoise::ui::Language::Portuguese:
    case porpoise::ui::Language::PortugueseBrazil: return "PT";
    case porpoise::ui::Language::Italian: return "IT";
    case porpoise::ui::Language::German: return "DE";
    case porpoise::ui::Language::Dutch: return "NL";
    default: return "EN";
    }
}

std::string info_path();

/* The table the library shows: the language's, or English until it is here. */
std::string shown_info_path()
{
    const std::string own = info_path();
    struct stat st;
    return stat(own.c_str(), &st) == 0 ? own : g_data + "/info.tsv";
}

std::string info_path()
{
    const std::string lang = info_lang();
    if (lang == "EN")
        return g_data + "/info.tsv";
    std::string lower = lang;
    for (char &c : lower)
        c = char(c - 'A' + 'a');
    return g_data + "/info-" + lower + ".tsv";
}

/* Where games are looked for: the usual folders and drives (unless the player
 * turned that off) and every folder the player added in Settings. */
porpoise::ui::LibraryPaths library_paths()
{
    porpoise::ui::LibraryPaths paths;
    paths.roots = {"/app0/content", g_data + "/games"};
    if (g_data != "/app0/porpoise")
        paths.roots.push_back("/app0/porpoise/games"); /* where a sandboxed 1.0 kept them */
    if (g_settings.auto_search)
    {
        for (const char *dir : {"/data/games", "/data/GameCube", "/data/gamecube", "/data/Wii", "/data/wii",
                                "/data/roms", "/data/ROMs", "/data/iso", "/data/ISO"})
            paths.roots.push_back(dir);
        for (int i = 0; i < 8; ++i)
            paths.roots.push_back("/mnt/usb" + std::to_string(i));
        paths.roots.push_back("/mnt/ext0");
        paths.roots.push_back("/mnt/ext1");
    }
    paths.deep = g_settings.folders;
    paths.covers = g_data + "/covers";
    paths.state = g_data + "/library.txt";
    paths.info = shown_info_path();
    return paths;
}

/* One key = value in an INI file's [section], kept with everything else in it. */
/* The Dolphin graphics settings a game's own files change, and the core
 * options that carry the same thing. The core copies every one of its options
 * straight into the running video config whenever any option changes (as the
 * in-game menu does), which would undo the game's own values for the rest of
 * the session; so for each game these options are given the game's values. */
struct GameOption
{
    const char *section, *key, *option;
};
constexpr GameOption kGameOptions[] = {
    {"Video_Hacks", "EFBToTextureEnable", "dolphin_efb_to_texture"},
    {"Video_Hacks", "XFBToTextureEnable", "dolphin_xfb_to_texture_enable"},
    {"Video_Hacks", "EFBAccessEnable", "dolphin_efb_access_enable"},
    {"Video_Hacks", "EFBAccessDeferInvalidation", "dolphin_efb_access_defer_invalidation"},
    {"Video_Hacks", "BBoxEnable", "dolphin_bbox_enabled"},
    {"Video_Hacks", "DisableCopyToVRAM", "dolphin_efb_to_vram"},
    {"Video_Hacks", "DeferEFBCopies", "dolphin_defer_efb_copies"},
    {"Video_Hacks", "ImmediateXFBEnable", "dolphin_immediate_xfb"},
    {"Video_Hacks", "EFBScaledCopy", "dolphin_efb_scaled_copy"},
    {"Video_Hacks", "EFBEmulateFormatChanges", "dolphin_efb_emulate_format_changes"},
    {"Video_Hacks", "VertexRounding", "dolphin_vertex_rounding"},
    {"Video_Hacks", "VISkip", "dolphin_vi_skip"},
    {"Video_Hacks", "FastTextureSampling", "dolphin_fast_texture_sampling"},
    {"Video_Settings", "SafeTextureCacheColorSamples", "dolphin_texture_cache_accuracy"},
};

/* The game's values for kGameOptions, as Dolphin layers its files: Sys then
 * User, each <first letter>, <first three>, <ID>. */
std::vector<std::pair<std::string, std::string>> game_dolphin_options(const std::string &id)
{
    std::vector<std::pair<std::string, std::string>> out;
    if (id.size() != 6)
        return out;
    std::vector<std::string> files;
    for (const std::string &dir : {std::string("/app0/system/dolphin-emu/Sys/GameSettings"),
                                   g_saves_path + "/User/GameSettings"})
        for (const std::string &name : {id.substr(0, 1), id.substr(0, 3), id})
            files.push_back(dir + "/" + name + ".ini");
    for (const std::string &path : files)
    {
        std::FILE *f = std::fopen(path.c_str(), "r");
        if (!f)
            continue;
        char raw[512];
        std::string section;
        while (std::fgets(raw, sizeof raw, f))
        {
            std::string line = raw;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            if (!line.empty() && line[0] == '[')
            {
                section = line.substr(1, line.find(']') == std::string::npos ? std::string::npos : line.find(']') - 1);
                continue;
            }
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos || line[0] == '#' || line[0] == ';')
                continue;
            std::string key = line.substr(0, eq), value = line.substr(eq + 1);
            while (!key.empty() && key.back() == ' ')
                key.pop_back();
            while (!value.empty() && value.front() == ' ')
                value.erase(0, 1);
            for (const GameOption &g : kGameOptions)
                if (section == g.section && key == g.key)
                {
                    std::string v;
                    if (key == "SafeTextureCacheColorSamples")
                    {
                        const long n = std::strtol(value.c_str(), nullptr, 10);
                        v = n <= 0 ? "0" : n > 128 ? "512" : "128";
                    }
                    else
                        v = (value == "True" || value == "true" || value == "1") ? "enabled" : "disabled";
                    bool replaced = false;
                    for (auto &kv : out)
                        if (kv.first == g.option)
                        {
                            kv.second = v;
                            replaced = true;
                        }
                    if (!replaced)
                        out.emplace_back(g.option, v);
                }
        }
        std::fclose(f);
    }
    return out;
}

void set_ini_value(const std::string &path, const std::string &section, const std::string &key,
                   const std::string &value)
{
    std::vector<std::string> lines;
    if (std::FILE *f = std::fopen(path.c_str(), "r"))
    {
        char buf[1024];
        while (std::fgets(buf, sizeof buf, f))
        {
            std::string l = buf;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r'))
                l.pop_back();
            lines.push_back(l);
        }
        std::fclose(f);
    }
    auto trim = [](std::string s) {
        const auto a = s.find_first_not_of(" \t");
        const auto b = s.find_last_not_of(" \t");
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    };
    const std::string header = "[" + section + "]";
    std::size_t sec = lines.size(), end = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (trim(lines[i]) == header)
        {
            sec = i;
            end = lines.size();
            for (std::size_t j = i + 1; j < lines.size(); ++j)
                if (!trim(lines[j]).empty() && trim(lines[j])[0] == '[')
                {
                    end = j;
                    break;
                }
            break;
        }
    const std::string entry = key + " = " + value;
    if (sec == lines.size())
    {
        lines.push_back(header);
        lines.push_back(entry);
    }
    else
    {
        bool done = false;
        for (std::size_t i = sec + 1; i < end && !done; ++i)
        {
            const auto eq = lines[i].find('=');
            if (eq != std::string::npos && trim(lines[i].substr(0, eq)) == key)
            {
                lines[i] = entry;
                done = true;
            }
        }
        if (!done)
            lines.insert(lines.begin() + std::ptrdiff_t(end), entry);
    }
    const std::string dir = path.substr(0, path.rfind('/'));
    mkdir(dir.substr(0, dir.rfind('/')).c_str(), 0777);
    mkdir(dir.c_str(), 0777);
    if (std::FILE *f = std::fopen(path.c_str(), "w"))
    {
        for (const std::string &l : lines)
            std::fprintf(f, "%s\n", l.c_str());
        std::fclose(f);
    }
}

/* The newest release, as the last check wrote it: GitHub's own answer. */
porpoise::update::Release g_release;
void read_latest_release()
{
    porpoise::update::Release r;
    if (porpoise::update::read_cached(g_data + "/latest-release.json", r))
    {
        g_release = r;
        g_app.set_latest_release(r.tag, r.page, r.size, r.build);
    }
}

/* Where an update goes: the folder Porpoise was installed to, when it's the
 * usual one and holds this same build (the app folder may be served from a
 * copy of it), else the app folder itself. */
std::string install_dir()
{
    auto read_all = [](const std::string &path) {
        std::string text;
        if (std::FILE *f = std::fopen(path.c_str(), "rb"))
        {
            char buf[16384];
            std::size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
                text.append(buf, n);
            std::fclose(f);
        }
        return text;
    };
    const std::string home = "/data/homebrew/PPSA99764";
    const std::string mine = read_all("/app0/manifest.sha256");
    const bool same = !mine.empty() && mine == read_all(home + "/manifest.sha256");
    ps5::debug::mark(("main: the update goes to " + (same ? home : std::string("/app0"))).c_str());
    return same ? home : "/app0";
}

bool g_covers_again = false; /* a request came while a run was going: ask again once it ends */
bool g_covers_force = false; /* the player asked for covers and info now (Sort & filter) */

void fetch_covers(bool force = false)
{
    g_covers_force |= force;
    /* Not while the updater is online: one download at a time, and it goes first. */
    const porpoise::update::Phase updating = porpoise::update::progress().phase;
    if (updating == porpoise::update::Phase::Checking || updating == porpoise::update::Phase::Downloading ||
        updating == porpoise::update::Phase::Installing || updating == porpoise::update::Phase::Finishing)
        return;
    porpoise::covers::Request request;
    request.dir = g_data + "/covers";
    /* Asked for, it gets both, whatever the settings say about doing it by itself. */
    const bool covers = g_settings.download_covers || g_covers_force, info = g_settings.download_info || g_covers_force;
    request.force = g_covers_force;
    request.covers = covers;
    request.discs = info;
    if (info)
    {
        request.info_path = info_path();
        request.info_lang = info_lang();
    }
    /* Recommended settings and the update check ride along, once a day,
     * when the player lets Porpoise go online for covers or game info. */
    if (covers || info)
    {
        request.feed_path = g_data + "/recommended.ini";
        request.release_path = g_data + "/latest-release.json";
    }
    for (const porpoise::ui::Game &game : g_library.games())
        if (!game.id.empty())
            request.ids.push_back(game.id);
    /* Games added while a run is going (the daily checks at start, often)
     * used to wait for the next start of Porpoise: now they follow it. */
    g_covers_again = !porpoise::covers::start(request);
    if (g_covers_again && g_covers_force)
        porpoise::covers::stop(); /* the run going now ends early, and this one follows it */
    if (!g_covers_again)
        g_covers_force = false;
}

void rescan_library()
{
    g_app.release_covers();
    g_library.scan(library_paths());
    g_app.library_changed();
    ps5::debug::mark_value("main: games after a new search", static_cast<long long>(g_library.games().size()));
    fetch_covers();
}

void apply_settings()
{
    porpoise::sound::set_music(g_settings.menu_music, g_settings.music_volume / 10.0f);
    porpoise::sound::set_effects(g_settings.menu_sounds, g_settings.sounds_volume / 10.0f);
    porpoise::pad::set_mapping(g_settings.mapping());
    porpoise::pad::set_rumble_enabled(g_settings.rumble);
}

/* ---- the launch, as seen by the core host -------------------------------------------- */

void launch_status(const char *text, void *)
{
    ps5::debug::mark(text);
    g_app.set_launch_status(text, -1);
    if (!g_gfx.ready())
        return;
    begin_ui_frame(0.6f);
    g_app.draw_launch(g_time);
    porpoise::vk::present_clear(0, 0, 0);
}

/* The border beside a 4:3 picture, on whichever device is drawing. */
porpoise::ui::Texture *g_border = nullptr;
std::string g_border_loaded; /* its name; "" none */
bool g_border_tried = false;

void forget_border()
{
    g_border = nullptr; /* its device is going, and the texture with it */
    g_border_tried = false;
}

void draw_border()
{
    if (g_play.border.empty() || !g_gfx.ready())
        return;
    if (!g_border_tried || g_border_loaded != g_play.border)
    {
        if (g_border && g_border_tried)
            g_gfx.free_texture(g_border);
        g_border = nullptr;
        g_border_tried = true;
        g_border_loaded = g_play.border;
        const std::string path = porpoise::borders::path_of(g_play.border);
        if (!path.empty())
            g_border = g_gfx.texture_file(path, 2048); /* full screen: full size */
        if (!g_border)
            ps5::debug::mark(("main: border not found: " + g_play.border).c_str());
    }
    if (!g_border)
        return;
    /* Only for a picture about 4:3 that leaves bars at the sides. */
    float rect[4];
    porpoise::vk::picture_rect(rect);
    const float h = rect[3] > 1 ? rect[3] : 1;
    const float aspect = rect[2] / h;
    if (aspect < 1.15f || aspect > 1.55f || rect[3] < 1000.0f)
        return;
    g_gfx.image(g_border, 0, 0, 1920, 1080, porpoise::ui::rgba(0xFFFFFF));
}

void launch_device_closing(void *)
{
    forget_border();
    g_app.forget_textures();
    g_gfx.shutdown();
}

void launch_device_ready(void *)
{
    start_gfx();
}

double g_game_rate = 48000.0; /* the game's audio rate, kept while the menu's sounds play */

void play_sound(porpoise::ui::Sound s)
{
    porpoise::sound::play(static_cast<porpoise::sound::Effect>(int(s)));
}

void menu_opened(void *)
{
    g_menu_open = true;
    g_game_rate = porpoise::audio::source_rate();
    g_app.open_game_menu(g_playing, &g_play);
}

int menu_paused(void *)
{
    const porpoise::pad::State &pad = porpoise::pad::state();
    porpoise::ui::Input in;
    in.held = pad.buttons;
    in.stick_x = pad.left_x / 32768.0f;
    in.stick_y = pad.left_y / 32768.0f;
    const int answer = g_app.update_game_menu(in, 1.0 / 60.0);
    /* Settings take effect right away. */
    const std::string key = g_app.take_menu_change();
    if (key == "volume")
        porpoise::audio::set_volume(g_play.volume / 10.0f);
    else if (key == "screen_filter" || key == "filter_strength")
        porpoise::core::set_picture(g_play.screen_filter, g_play.filter_strength / 10.0f);
    else if (key == "setup")
    {
        /* A whole setup: the picture and every Dolphin option. */
        porpoise::core::set_picture(g_play.screen_filter, g_play.filter_strength / 10.0f);
        for (const auto &[k, v] : g_play.core_options())
            porpoise::core::set_option(k.c_str(), v.c_str());
    }
    else if (key == "button_layout")
        porpoise::pad::set_mapping(g_play.mapping());
    else if (key == "fast_forward")
        porpoise::core::set_fast_forward(g_app.menu_fast_forward());
    else if (key == "border" || key == "fps_overlay" || key == "motion_readout")
        ; /* drawn by launch_frame */
    else if (key.rfind("wii_", 0) == 0)
    {
        /* The Wii Remote: the pad and the core's ports; the pointer's source
         * is also a Dolphin option. */
        porpoise::core::set_wii(g_play.wii_config(true));
        if (key == "wii_pointer" || key == "wii_setup")
            for (const auto &[k, v] : g_play.core_options())
                porpoise::core::set_option(k.c_str(), v.c_str());
    }
    else if (!key.empty())
    {
        /* A Dolphin option: hand the core every value (it applies what it can
         * while running; the rest at the next start). */
        for (const auto &[k, v] : g_play.core_options())
            porpoise::core::set_option(k.c_str(), v.c_str());
        if (key == "rumble")
            porpoise::pad::set_rumble_enabled(g_play.rumble);
    }
    /* Save states, asked for in the menu, done here on the core's thread. */
    const porpoise::ui::App::MenuRequest request = g_app.take_menu_request();
    if (request.kind != porpoise::ui::App::MenuRequest::None && g_playing)
    {
        const std::string game = porpoise::ui::Library::key_of(*g_playing);
        if (request.kind == porpoise::ui::App::MenuRequest::Save)
        {
            /* Taken now; written in the background (reported below). */
            if (!porpoise::states::save(game, request.slot))
                g_app.menu_state_done(request.kind, request.slot, false);
        }
        else
        {
            const bool ok = porpoise::states::load(game, request.slot);
            ps5::debug::mark_value("main: state loaded from slot", ok ? request.slot + 1 : -(request.slot + 1));
            g_app.menu_state_done(request.kind, request.slot, ok);
        }
    }
    {
        int slot = 0;
        bool ok = false;
        if (porpoise::states::take_finished(slot, ok))
        {
            ps5::debug::mark_value("main: state saved to slot", ok ? slot + 1 : -(slot + 1));
            g_app.menu_state_done(porpoise::ui::App::MenuRequest::Save, slot, ok);
        }
    }
    porpoise::sound::pump(); /* the menu's own sounds, while the game is still */
    if (answer != porpoise::core::kMenuStay)
    {
        g_menu_open = false;
        porpoise::audio::flush();
        porpoise::audio::set_source_rate(g_game_rate);
    }
    return answer;
}

/* Testing the Wii Remote: what player 1's controller feels, and where the
 * pointer is, over the game. */
/* A Wii game with the gyro pointer: how to centre it, for the first seconds,
 * and a short "centred" each time it is. */
double g_wii_hint_from = -1;      /* g_time the game started, or -1 */
unsigned g_wii_centrings = 0;
double g_wii_centred_at = -100;

void draw_wii_hint()
{
    using namespace porpoise::ui;
    const porpoise::pad::WiiConfig wii = porpoise::pad::wii();
    const bool held = wii.controller != porpoise::pad::WiiSideways && wii.controller != porpoise::pad::WiiClassic;
    if (!wii.active || !held || wii.pointer != porpoise::pad::PointerGyro)
        return;
    const porpoise::pad::Motion m = porpoise::pad::snapshot(0).motion;
    if (m.centrings != g_wii_centrings)
    {
        g_wii_centrings = m.centrings;
        g_wii_centred_at = g_time;
    }
    const bool left = porpoise::pad::pose_left_hand(m.pose);
    std::string text;
    float a = 0;
    if (g_time - g_wii_centred_at < 1.4)
    {
        text = tr("Centred");
        a = float(std::min(1.0, (1.4 - (g_time - g_wii_centred_at)) * 3.0));
    }
    else if (g_wii_hint_from >= 0 && g_time >= g_wii_hint_from && g_time - g_wii_hint_from < 10.0 && m.centrings == 0)
    {
        text = tr(left ? "Hold the controller as you'll play, point at the middle of the screen, and hold L1"
                       : "Hold the controller as you'll play, point at the middle of the screen, and hold R1");
        a = float(std::min(1.0, (10.0 - (g_time - g_wii_hint_from)) * 2.0));
    }
    if (text.empty() || a <= 0)
        return;
    const float w = g_gfx.measure(Font::SemiBold, 26, text) + 70, h = 58, x = 960 - w * 0.5f, y = 1080 - 120;
    g_gfx.panel(x, y, w, h, rgba(0x0A1236, 0.82f * a), 0.9f, h * 0.5f, rgba(0x6BE3A8, 0.9f * a), 1.8f);
    g_gfx.text_mid(Font::SemiBold, 26, 960, y + h * 0.5f, rgba(0xF4F7FF, a), Align::Center, text);
}

void draw_motion_readout()
{
    using namespace porpoise::ui;
    const porpoise::pad::State p = porpoise::pad::snapshot(0);
    const porpoise::pad::Motion &m = p.motion;
    const float x = 36, y = 100, w = 560, h = 398;
    g_gfx.panel(x, y, w, h, rgba(0x0A1236, 0.8f), 0.9f, 14, rgba(0x5CD3FF, 0.9f), 1.6f);
    char line[160];
    float ty = y + 34;
    auto row = [&](const char *text) {
        g_gfx.text_mid(Font::SemiBold, 22, x + 22, ty, rgba(0xF4F7FF), Align::Left, text);
        ty += 34;
    };
    std::snprintf(line, sizeof line, "Motion %s, %u readings", m.valid ? "on" : "none", m.samples);
    row(line);
    std::snprintf(line, sizeof line, "accel g   %+.2f %+.2f %+.2f", m.raw_accel[0], m.raw_accel[1], m.raw_accel[2]);
    row(line);
    std::snprintf(line, sizeof line, "gyro r/s  %+.2f %+.2f %+.2f", m.raw_gyro[0], m.raw_gyro[1], m.raw_gyro[2]);
    row(line);
    std::snprintf(line, sizeof line, "remote a  %+.2f %+.2f %+.2f", m.accel[0], m.accel[1], m.accel[2]);
    row(line);
    std::snprintf(line, sizeof line, "remote g  %+.2f %+.2f %+.2f", m.gyro[0], m.gyro[1], m.gyro[2]);
    row(line);
    static const char *const kPose[] = {"flat", "upright R (trigger)", "upright L (trigger)", "upright R (facing)",
                                        "upright L (facing)"};
    const porpoise::pad::Motion m2 = porpoise::pad::snapshot(1).motion;
    std::snprintf(line, sizeof line, "hold      %s%s%s", kPose[std::clamp(m.pose, 0, 4)], m2.valid ? "  / 2: " : "",
                  m2.valid ? kPose[std::clamp(m2.pose, 0, 4)] : "");
    row(line);
    std::snprintf(line, sizeof line, "pointer   %+.2f %+.2f  roll %+.0f", m.aim_x, m.aim_y, m.roll * 57.29578f);
    row(line);
    std::snprintf(line, sizeof line, "touch     %s %d, %d", m.touching ? "yes" : "no ", m.touch_x, m.touch_y);
    row(line);
    std::snprintf(line, sizeof line, "shake %s   buttons %04x", m.shaking ? "YES" : "no ", unsigned(p.joypad));
    row(line);
    /* What the game is told the remote's camera sees: the sensor bar's lights. */
    porpoise::aim::Dot dots[2];
    porpoise::aim::sensor_bar(m.aim_x, m.aim_y, m.roll, dots);
    if (g_play.wii_pointer != 2)
        std::snprintf(line, sizeof line, "camera    %4d,%3d  %4d,%3d", dots[0].visible ? int(dots[0].x * 1023) : -1,
                      dots[0].visible ? int(dots[0].y * 767) : -1, dots[1].visible ? int(dots[1].x * 1023) : -1,
                      dots[1].visible ? int(dots[1].y * 767) : -1);
    else
        std::snprintf(line, sizeof line, "camera    (right stick aims)");
    row(line);
    /* Where Porpoise points: a ring over the picture. */
    const float px = 960 + m.pointer_x * 940, py = 540 + m.pointer_y * 520;
    g_gfx.panel(px - 16, py - 16, 32, 32, rgba(0xFFFFFF, 0.0f), 0.0f, 16, rgba(0xFFC85C, 0.95f), 3.0f);
    g_gfx.panel(px - 3, py - 3, 6, 6, rgba(0xFFC85C), 1, 3);
}

void launch_frame(bool core_frame, double fps, void *)
{
    g_time += 1.0 / 60.0;
    if (!g_gfx.ready())
        return;
    begin_ui_frame(core_frame ? 0.0f : 0.6f);
    if (!core_frame)
    {
        g_app.draw_launch(g_time);
        return;
    }
    draw_border();
    if (g_app.menu_fast_forward() > 1 && !g_menu_open)
    {
        /* Fast forward is on: say so, top right. */
        const char *text = g_app.menu_fast_forward() >= 4 ? "4x" : "2x";
        g_gfx.panel(1884 - 150, 30, 150, 50, porpoise::ui::rgba(0x0A1236, 0.72f), 0.9f, 14,
                    porpoise::ui::rgba(0xFFC85C, 0.9f), 1.6f);
        const float kHalfPi = 1.5707963f;
        g_gfx.glyph(porpoise::ui::Glyph::Arrow, 1884 - 116, 55, 20, porpoise::ui::rgba(0xFFE7B0), kHalfPi);
        g_gfx.glyph(porpoise::ui::Glyph::Arrow, 1884 - 100, 55, 20, porpoise::ui::rgba(0xFFE7B0), kHalfPi);
        g_gfx.text_mid(porpoise::ui::Font::Bold, 28, 1884 - 52, 55, porpoise::ui::rgba(0xFFE7B0),
                       porpoise::ui::Align::Center, text);
    }
    if (g_play.fps_overlay && !g_menu_open)
    {
        char text[32];
        std::snprintf(text, sizeof text, "%.0f FPS", fps);
        g_gfx.panel(36, 30, 150, 50, porpoise::ui::rgba(0x0A1236, 0.72f), 0.9f, 14,
                    porpoise::ui::rgba(0x5CD3FF, 0.9f), 1.6f);
        g_gfx.text_mid(porpoise::ui::Font::Bold, 28, 111, 55, porpoise::ui::rgba(0xF4F7FF),
                   porpoise::ui::Align::Center, text);
    }
    if (g_play.motion_readout && g_play.developer && !g_menu_open)
        draw_motion_readout();
    if (!g_menu_open)
        draw_wii_hint();
    if (g_menu_open)
        g_app.draw_game_menu(g_time);
}
} // namespace

int main()
{
    ps5::debug::mark("Porpoise: main() entered");
    ps5_open_permissions();
    if (std::freopen("/app0/trace.txt", "a", stderr))
    {
        static char stderr_buffer[64 * 1024];
        std::setvbuf(stderr, stderr_buffer, _IOFBF, sizeof stderr_buffer);
        start_log_flusher();
    }
    std::set_terminate(on_terminate);
    ps5::debug::mark(PS5_RETROARCH_BUILD_ID);
    ps5::memory::init("/app0/memory-diagnostics.log", PS5_RETROARCH_BUILD_ID);
    ps5_vulkan_profile_init();
    ps5_crash_report_install();
    ps5_memory_report("startup", 0, 0);
    ps5_core_threads_start();
    ps5_sampler_start();

    mkdir("/app0/content", 0777);
    mkdir("/app0/savefiles", 0777);
    mkdir("/app0/system", 0777);
    mkdir("/app0/porpoise", 0777);
    mkdir("/app0/porpoise/covers", 0777);

    ps5::debug::mark_value("main: splash hidden", sceSystemServiceHideSplashScreen());

    if (!porpoise::vk::open_display())
    {
        ps5::debug::mark("main: no display; nothing can be shown");
        leave(1);
    }
    porpoise::pad::open();
    porpoise::audio::open();

    choose_data_dir();
    g_settings.load(g_settings_path);
    {
        /* Games' own settings from 1.0, brought up to date once. */
        bool changed = false;
        if (DIR *d = opendir((g_data + "/game-settings").c_str()))
        {
            while (dirent *e = readdir(d))
            {
                const std::string n = e->d_name;
                if (n.size() > 4 && n.compare(n.size() - 4, 4, ".ini") == 0)
                    changed |= porpoise::Settings::migrate_game_file(g_data + "/game-settings/" + n, g_settings);
            }
            closedir(d);
        }
        if (changed)
            g_settings.save(g_settings_path);
    }
    g_settings.write_core_options(g_options_path);
    apply_settings();
    {
        /* The menus speak the PS5's language unless Settings says otherwise. */
        int language = 1;
        const int rc = sceSystemServiceParamGetInt(1 /* language */, &language);
        ps5::debug::mark_value("main: system language", rc == 0 ? language : -1);
        porpoise::ui::set_system_language(rc == 0 ? language : 1);
        mkdir((g_data + "/lang").c_str(), 0777);
        porpoise::ui::apply_language(g_settings.ui_language, g_data + "/lang");
    }

    g_library.scan(library_paths());
    ps5::debug::mark_value("main: games in the library", static_cast<long long>(g_library.games().size()));

    /* The launcher runs on Porpoise's own device. */
    if (!porpoise::vk::open_device(nullptr) || !start_gfx())
        leave(1);
    porpoise::vk::set_overlay(overlay, nullptr);
    porpoise::vk::set_prepass(prepass, nullptr);
    g_app.init(&g_gfx, &g_library, &g_settings, g_settings_path, g_options_path, g_saves_path);
    g_app.set_sound_hook(play_sound);
    g_app.set_jingle_hook([](const std::int16_t *frames, std::size_t count) {
        porpoise::sound::play_jingle(frames, count);
    });
    porpoise::banner::set_cache_dir(g_data + "/banners");
    porpoise::sound::load("/app0/assets");
    porpoise::states::set_data_dir(g_data);
    porpoise::borders::set_dirs("/app0/assets", g_data);
    porpoise::ui::setups::set_dir(g_data);
    porpoise::ui::recommend::set_paths(g_data + "/recommended.ini", "/app0/system/dolphin-emu/Sys/GameSettings",
                                       "/app0/assets/recommended.ini");
    read_latest_release();
    apply_settings();
    if (g_sandboxed)
        g_app.show_message(porpoise::ui::tr("Porpoise can't reach /data"),
                           porpoise::ui::tr("The console started Porpoise inside the app sandbox, so it can't see "
                                            "/data or USB drives, and no jailbreak daemon freed it. Make sure one is "
                                            "running before you open Porpoise: etaHEN or OnionHEN with PPSA99764 on "
                                            "its app jailbreak list, or a standalone daemon such as Lapy. Then open "
                                            "Porpoise again (if it still fails the first time, try once more). Until "
                                            "then, games go in /app0/porpoise/games."));
    porpoise::sound::fade_music(1.0f, 2.5f);
    fetch_covers();

    const double hz = porpoise::vk::refresh_hz();
    g_pacer.start(hz, "launcher");
    for (;;)
    {
        porpoise::ui::Game *launch = nullptr;
        for (;;)
        {
            const porpoise::pad::State &pad = porpoise::pad::poll();
            porpoise::ui::Input in;
            in.held = pad.buttons;
            in.stick_x = pad.left_x / 32768.0f;
            in.stick_y = pad.left_y / 32768.0f;
            in.right_x = pad.right_x / 32768.0f;
            in.right_y = pad.right_y / 32768.0f;
            const double dt = 1.0 / hz;
            g_time += dt;
            std::string cover_id;
            while (porpoise::covers::take_ready(cover_id))
                g_app.cover_arrived(cover_id);
            if (porpoise::covers::take_info_ready())
            {
                g_library.set_info_path(shown_info_path());
                g_library.load_info();
            }
            if (porpoise::covers::take_feed_ready())
                porpoise::ui::recommend::feed_changed();
            if (porpoise::covers::take_release_ready())
                read_latest_release();
            {
                /* The updater: checked, downloading, installing, done. */
                const porpoise::update::Progress up = porpoise::update::progress();
                if (up.phase == porpoise::update::Phase::Checked)
                    read_latest_release();
                g_app.set_update_progress(int(up.phase), up.done, up.total, up.error);
                if (up.phase == porpoise::update::Phase::Checked || up.phase == porpoise::update::Phase::Failed)
                {
                    porpoise::update::acknowledge();
                    fetch_covers(); /* the covers paused for it */
                }
            }
            {
                int phase = 0, done = 0, total = 0;
                std::string note;
                if (porpoise::covers::progress(phase, done, total) && phase < 4)
                    note = phase == 1 ? porpoise::ui::tr("Getting game info")
                                      : porpoise::ui::trf(phase == 2 ? "Getting disc art {done} of {total}"
                                                                     : "Getting covers {done} of {total}",
                                                          {{"done", std::to_string(done)}, {"total", std::to_string(total)}});
                g_app.set_note(note);
            }
            if (g_covers_again && !porpoise::covers::busy())
                fetch_covers();
            const auto action = g_app.update(in, dt);
            if (action == porpoise::ui::App::Action::SettingsChanged)
            {
                apply_settings();
                if (g_library.paths().info != shown_info_path())
                {
                    /* The menus changed language: descriptions in it too. */
                    g_library.set_info_path(shown_info_path());
                    g_library.load_info();
                }
                fetch_covers(); /* in case covers were just turned on */
            }
            if (action == porpoise::ui::App::Action::Rescan)
                rescan_library();
            if (action == porpoise::ui::App::Action::FetchCovers)
                fetch_covers(true);
            if (action == porpoise::ui::App::Action::CheckUpdate)
            {
                porpoise::covers::stop(); /* one download at a time: let the check go first */
                porpoise::update::start_check(g_data + "/latest-release.json");
            }
            if (action == porpoise::ui::App::Action::InstallUpdate)
            {
                porpoise::covers::stop();
                ps5::debug::mark(("main: updating to " + g_release.tag).c_str());
                porpoise::update::start_install(g_release, install_dir());
            }
            if (action == porpoise::ui::App::Action::Quit)
            {
                ps5::debug::mark("main: closing after the update");
                leave(0);
            }
            if (action == porpoise::ui::App::Action::Launch && g_app.launch_game())
            {
                launch = g_app.launch_game();
                break;
            }
            begin_ui_frame(0.0f);
            g_app.draw(g_time);
            porpoise::vk::present_clear(0, 0, 0);
            porpoise::sound::pump();
            g_pacer.frame_done();
        }

        /* The launch: the screen dims into the launch tile for a moment, then
         * the game starts behind it. */
        ps5::debug::mark(("main: launching " + launch->path).c_str());
        porpoise::covers::stop();
        porpoise::banner::pause(true); /* the disc is the game's now */
        porpoise::sound::play_jingle(nullptr, 0);
        /* The game's own settings, on top of the global ones. */
        g_play = g_settings;
        if (g_play.load(g_app.game_settings_path(*launch), true))
            ps5::debug::mark("main: the game has its own settings");
        g_play.write_core_options(g_options_path);
        /* Fast save states: Dolphin leaves its GPU texture cache out of them
         * (Dolphin.ini's base layer, read when the core starts). */
        set_ini_value(g_saves_path + "/User/Config/GFX.ini", "Settings", "SaveTextureCacheToState",
                      g_play.fast_states ? "False" : "True");
        /* Shaders compile on four background threads, not Dolphin's one: the
         * game spends less time on the slow ubershaders and stutters less the
         * first time it shows something. */
        set_ini_value(g_saves_path + "/User/Config/GFX.ini", "Settings", "ShaderCompilerThreads", "4");
        if (launch->id.size() == 6)
        {
            /* The game's Dolphin settings (recommended ones turned on or off),
             * where Dolphin reads them: over its own per-game fixes. */
            const std::string dir = g_saves_path + "/User/GameSettings";
            mkdir((g_saves_path + "/User").c_str(), 0777);
            mkdir(dir.c_str(), 0777);
            if (!g_play.write_dolphin_game_ini(dir + "/" + launch->id + ".ini"))
                ps5::debug::mark("main: the game's Dolphin settings file is the player's own; left as it is");
            for (const auto &[k, v] : g_play.dolphin)
                ps5::debug::mark(("main: Dolphin setting for this game: " + k + " = " + v).c_str());
            /* The same values as core options, so a change in the in-game menu
             * keeps them (see kGameOptions). */
            const auto pinned = game_dolphin_options(launch->id);
            if (!pinned.empty())
                if (std::FILE *f = std::fopen(g_options_path.c_str(), "a"))
                {
                    std::fprintf(f, "# Set by Porpoise for this game only (its Dolphin settings):\n");
                    for (const auto &[k, v] : pinned)
                    {
                        std::fprintf(f, "%s = %s\n", k.c_str(), v.c_str());
                        ps5::debug::mark(("main: core option for this game: " + k + " = " + v).c_str());
                    }
                    std::fclose(f);
                }
        }
        porpoise::pad::set_mapping(g_play.mapping());
        porpoise::pad::set_rumble_enabled(g_play.rumble);
        g_app.begin_launch(launch);
        /* The music fades as the screen dims; the Play sound finishes. */
        porpoise::sound::fade_music(0.0f, 0.5f);
        for (int frame = 0; frame < 36; ++frame)
        {
            g_time += 1.0 / hz;
            const float t = std::min(1.0f, float(frame + 1) / 24.0f);
            begin_ui_frame(0.6f * t);
            g_app.draw_launch(g_time);
            porpoise::vk::present_clear(0, 0, 0);
            porpoise::sound::pump();
            g_pacer.frame_done();
        }

        g_playing = launch;
        g_menu_open = false;
        porpoise::core::Hooks hooks;
        hooks.status = launch_status;
        hooks.device_closing = launch_device_closing;
        hooks.device_ready = launch_device_ready;
        hooks.frame = launch_frame;
        hooks.opened = menu_opened;
        hooks.paused = menu_paused;
        porpoise::core::Playback playback;
        playback.volume = g_play.volume / 10.0f;
        playback.muted = g_play.muted;
        playback.filter = g_play.screen_filter;
        playback.strength = g_play.filter_strength / 10.0f;
        /* A Wii game: the DualSense as a Wii Remote; a test build logs it. */
        playback.wii = g_play.wii_config(launch->platform == "Wii");
        g_wii_hint_from = g_time + 2.0; /* once the game is up */
        g_wii_centrings = 0;
        const std::string debug_dir = g_data + "/debug";
        if (g_settings.debug_logs)
        {
            playback.debug_dir = debug_dir.c_str();
            playback.motion_log = g_settings.developer && g_settings.motion_logs;
            mkdir(debug_dir.c_str(), 0777);
            if (std::FILE *f = std::fopen((debug_dir + "/README.txt").c_str(), "w"))
            {
                std::fputs("Porpoise's debug folder (turn it off in Settings > System > Debug logs).\n\n"
                           "WiimoteNew.ini  The Wii Remote set-up Dolphin was given for the last Wii game.\n\n"
                           "Send it with /data/homebrew/PPSA99764/trace.txt and "
                           "/data/homebrew/PPSA99764/porpoise/core.log when you report a problem.\n",
                           f);
                std::fclose(f);
            }
        }
        if (playback.wii.active)
            ps5::debug::mark_value("main: Wii game; Wii controller", playback.wii.controller);
        const std::string start_state = g_app.take_launch_state();
        playback.load_state = start_state.empty() ? nullptr : start_state.c_str();
        porpoise::core::Paths core_paths;
        core_paths.saves = g_saves_path.c_str();
        core_paths.options = g_options_path.c_str();
        core_paths.options_reference = g_options_reference.c_str();
        core_paths.log = g_core_log.c_str();
        porpoise::core::set_fast_forward(1);
        const long long played_from = now_ns();
        const porpoise::core::Exit exit = porpoise::core::run_game(launch->path.c_str(), core_paths, hooks, playback);
        /* Play time: the whole visit, loading included, as consoles count it. */
        if (exit != porpoise::core::Exit::Failed)
            g_library.add_play_time(*launch, (now_ns() - played_from) / 1000000000LL);
        porpoise::states::wait(); /* a save still being written */
        g_playing = nullptr;
        g_menu_open = false;
        if (exit == porpoise::core::Exit::Home)
            leave(0);

        /* Back to the library: Porpoise's own device again (a game that
         * failed early never took it away). */
        if (!g_gfx.ready())
        {
            if (!porpoise::vk::open_device(nullptr) || !start_gfx())
                leave(exit == porpoise::core::Exit::Failed ? 2 : 1);
        }
        porpoise::vk::set_overlay(overlay, nullptr);
        porpoise::audio::flush();
        porpoise::audio::set_volume(1.0f);
        porpoise::audio::set_muted(false);
        g_settings.write_core_options(g_options_path);
        apply_settings();
        porpoise::banner::pause(false);
        g_app.return_from_game();
        porpoise::sound::fade_music(1.0f, 2.5f);
        if (exit == porpoise::core::Exit::Failed)
            g_app.show_message(porpoise::ui::tr("This game didn't start"),
                               porpoise::ui::tr("Dolphin could not start it. The file may be damaged or in a format "
                                                "Porpoise can't read yet. Details are in porpoise/core.log."));
        ps5::debug::mark("main: back in the library");
        fetch_covers();
        g_pacer.start(hz, "launcher");
    }
}

/* The SDK's _start calls this when main returns. Leaving through the shell is
 * what returns to the home screen; kernel exit(0) raises SIGSYS for an app. */
extern "C" void catchReturnFromMain(int status)
{
    leave(status);
}
