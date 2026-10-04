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
#include "porpoise_covers.hpp"
#include "porpoise_pacer.hpp"
#include "porpoise_pad.hpp"
#include "porpoise_sound.hpp"
#include "porpoise_states.hpp"
#include "porpoise_vk.hpp"
#include "title_threads.hpp"
#include "trace.hpp"
#include "ui_app.hpp"
#include "ui_gfx.hpp"
#include "ui_i18n.hpp"
#include "ui_library.hpp"
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

void choose_data_dir()
{
    mkdir("/data/porpoise", 0777);
    const std::string probe = "/data/porpoise/.write-test";
    if (std::FILE *f = std::fopen(probe.c_str(), "w"))
    {
        std::fputs("ok\n", f);
        std::fclose(f);
        std::remove(probe.c_str());
        g_data = "/data/porpoise";
    }
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

/* Where games are looked for: the usual folders and drives (unless the player
 * turned that off) and every folder the player added in Settings. */
porpoise::ui::LibraryPaths library_paths()
{
    porpoise::ui::LibraryPaths paths;
    paths.roots = {"/app0/content", "/data/porpoise/games"};
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
    paths.info = g_data + "/info.tsv";
    return paths;
}

void fetch_covers()
{
    porpoise::covers::Request request;
    request.dir = g_data + "/covers";
    request.covers = g_settings.download_covers;
    request.discs = g_settings.download_info;
    if (g_settings.download_info)
        request.info_path = g_data + "/info.tsv";
    if (!request.covers && !request.discs && request.info_path.empty())
        return;
    for (const porpoise::ui::Game &game : g_library.games())
        if (!game.id.empty())
            request.ids.push_back(game.id);
    porpoise::covers::start(request);
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
            g_border = g_gfx.texture_file(path);
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
    else if (key == "button_layout")
        porpoise::pad::set_mapping(g_play.mapping());
    else if (key == "fast_forward")
        porpoise::core::set_fast_forward(g_app.menu_fast_forward());
    else if (key == "border" || key == "fps_overlay")
        ; /* drawn by launch_frame */
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
        const bool save = request.kind == porpoise::ui::App::MenuRequest::Save;
        const bool ok = save ? porpoise::states::save(game, request.slot) : porpoise::states::load(game, request.slot);
        ps5::debug::mark_value(save ? "main: state saved to slot" : "main: state loaded from slot",
                               ok ? request.slot + 1 : -(request.slot + 1));
        g_app.menu_state_done(request.kind, request.slot, ok);
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
        const char *text = g_app.menu_fast_forward() >= 4 ? "\xE2\x96\xB6\xE2\x96\xB6 4x" : "\xE2\x96\xB6\xE2\x96\xB6 2x";
        g_gfx.panel(1884 - 150, 30, 150, 50, porpoise::ui::rgba(0x0A1236, 0.72f), 0.9f, 14,
                    porpoise::ui::rgba(0xFFC85C, 0.9f), 1.6f);
        g_gfx.text_mid(porpoise::ui::Font::Bold, 28, 1884 - 75, 55, porpoise::ui::rgba(0xFFE7B0),
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
    g_app.init(&g_gfx, &g_library, &g_settings, g_settings_path, g_options_path, g_saves_path);
    g_app.set_sound_hook(play_sound);
    porpoise::sound::load("/app0/assets");
    porpoise::states::set_data_dir(g_data);
    porpoise::borders::set_dirs("/app0/assets", g_data);
    apply_settings();
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
                g_library.load_info();
            {
                int phase = 0, done = 0, total = 0;
                std::string note;
                if (porpoise::covers::progress(phase, done, total))
                    note = phase == 1 ? porpoise::ui::tr("Getting game info")
                                      : porpoise::ui::trf(phase == 2 ? "Getting disc art {done} of {total}"
                                                                     : "Getting covers {done} of {total}",
                                                          {{"done", std::to_string(done)}, {"total", std::to_string(total)}});
                g_app.set_note(note);
            }
            const auto action = g_app.update(in, dt);
            if (action == porpoise::ui::App::Action::SettingsChanged)
            {
                apply_settings();
                fetch_covers(); /* in case covers were just turned on */
            }
            if (action == porpoise::ui::App::Action::Rescan)
                rescan_library();
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
        /* The game's own settings, on top of the global ones. */
        g_play = g_settings;
        if (g_play.load(g_app.game_settings_path(*launch), true))
            ps5::debug::mark("main: the game has its own settings");
        g_play.write_core_options(g_options_path);
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
        const std::string start_state = g_app.take_launch_state();
        playback.load_state = start_state.empty() ? nullptr : start_state.c_str();
        porpoise::core::Paths core_paths;
        core_paths.saves = g_saves_path.c_str();
        core_paths.options = g_options_path.c_str();
        core_paths.options_reference = g_options_reference.c_str();
        core_paths.log = g_core_log.c_str();
        const porpoise::core::Exit exit = porpoise::core::run_game(launch->path.c_str(), core_paths, hooks, playback);
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
