/* Porpoise - the emulator core, hosted through the libretro API.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Dolphin is built as a libretro core (libretro/dolphin with Mihawk's PS5 port
 * patch). The API is a plugin interface, not a user interface: this file is the
 * whole host side of it, and nothing of RetroArch runs.
 *
 * The core is loaded with the title's own ELF loader (src/core_loader_ps5.cpp),
 * because this console refuses a title's dlopen, and its imports are bound to
 * the title's symbols (build/core_imports.inc). */
#include "porpoise_core.hpp"

#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "libretro.h"
#include "porpoise_audio.hpp"
#include "porpoise_pad.hpp"
#include "porpoise_vk.hpp"
#include "trace.hpp"

extern "C"
{
    void *ps5_core_dlopen(const char *path, int mode);
    void *ps5_core_dlsym(void *handle, const char *name);
    int ps5_core_dlclose(void *handle);
    char *ps5_core_dlerror();
}

namespace
{
using porpoise::core::Paths;

struct CoreApi
{
    void (*init)();
    void (*deinit)();
    void (*get_system_info)(retro_system_info *);
    void (*get_system_av_info)(retro_system_av_info *);
    void (*set_environment)(retro_environment_t);
    void (*set_video_refresh)(retro_video_refresh_t);
    void (*set_audio_sample)(retro_audio_sample_t);
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*set_input_poll)(retro_input_poll_t);
    void (*set_input_state)(retro_input_state_t);
    void (*set_controller_port_device)(unsigned, unsigned);
    bool (*load_game)(const retro_game_info *);
    void (*unload_game)();
    void (*run)();
};

struct Option
{
    std::string key;
    std::string default_value;
    std::string value;
    std::string description;
    std::vector<std::string> choices;
};

struct Host
{
    Paths paths;
    void *library = nullptr;
    CoreApi api{};
    std::vector<Option> options;
    bool options_updated = false;
    retro_hw_render_callback hw{};
    bool hw_requested = false;
    const retro_hw_render_context_negotiation_interface_vulkan *negotiation = nullptr;
    float aspect = 4.0f / 3.0f;
    double sample_rate = 32000.0;
    double fps = 60.0;
    std::atomic<unsigned long long> samples_in{0}; /* audio frames the core has pushed (any thread) */
    bool presented = false;
    unsigned last_width = 0, last_height = 0;
    bool sharp = false;
    bool have_frame = false;
    bool hold_input = false; /* after the menu: the game sees no buttons until all are let go */
    std::FILE *log = nullptr;
};

Host h;

/* ---- options --------------------------------------------------------------------------- */

std::string trim(const std::string &s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos)
        return {};
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

Option *find_option(const char *key)
{
    for (Option &o : h.options)
        if (o.key == key)
            return &o;
    return nullptr;
}

/* Porpoise's own choices, before the player's: things that suit a television
 * and a DualSense better than the core's desktop defaults. */
void apply_porpoise_defaults()
{
    /* Nothing is overridden yet; this is where Porpoise's video/controls
     * presets will land once the settings screens exist. */
}

void apply_user_overrides()
{
    std::FILE *f = std::fopen(h.paths.options, "r");
    if (!f)
        return;
    char line[512];
    int applied = 0;
    while (std::fgets(line, sizeof line, f))
    {
        std::string text = trim(line);
        if (text.empty() || text[0] == '#' || text[0] == ';')
            continue;
        const auto eq = text.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = trim(text.substr(0, eq));
        std::string value = trim(text.substr(eq + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);
        if (Option *o = find_option(key.c_str()))
        {
            o->value = value;
            ++applied;
        }
    }
    std::fclose(f);
    ps5::debug::mark_value("options: overrides applied from options.ini", applied);
}

void write_options_reference()
{
    mkdir("/app0/porpoise", 0777);
    std::FILE *f = std::fopen(h.paths.options_reference, "w");
    if (!f)
        return;
    std::fprintf(f, "# Every option the Dolphin core offers, written by Porpoise at each launch.\n"
                    "# To change one, copy its line to options.ini and edit the value.\n\n");
    for (const Option &o : h.options)
    {
        std::fprintf(f, "# %s\n# choices:", o.description.c_str());
        for (const std::string &c : o.choices)
            std::fprintf(f, " %s |", c.c_str());
        std::fprintf(f, "\n%s = %s\n\n", o.key.c_str(), o.default_value.c_str());
    }
    std::fclose(f);
}

void define_options_v2(const retro_core_options_v2 *opts)
{
    h.options.clear();
    if (!opts || !opts->definitions)
        return;
    for (const retro_core_option_v2_definition *d = opts->definitions; d->key; ++d)
    {
        Option o;
        o.key = d->key;
        o.description = d->desc ? d->desc : "";
        for (unsigned i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[i].value; ++i)
            o.choices.emplace_back(d->values[i].value);
        o.default_value = d->default_value ? d->default_value
                                           : (o.choices.empty() ? "" : o.choices.front());
        o.value = o.default_value;
        h.options.push_back(std::move(o));
    }
}

void define_options_v1(const retro_core_option_definition *defs)
{
    h.options.clear();
    for (const retro_core_option_definition *d = defs; d && d->key; ++d)
    {
        Option o;
        o.key = d->key;
        o.description = d->desc ? d->desc : "";
        for (unsigned i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[i].value; ++i)
            o.choices.emplace_back(d->values[i].value);
        o.default_value = d->default_value ? d->default_value
                                           : (o.choices.empty() ? "" : o.choices.front());
        o.value = o.default_value;
        h.options.push_back(std::move(o));
    }
}

void define_options_v0(const retro_variable *vars)
{
    /* "Description; first|second|third", the first choice being the default. */
    h.options.clear();
    for (const retro_variable *v = vars; v && v->key; ++v)
    {
        Option o;
        o.key = v->key;
        std::string spec = v->value ? v->value : "";
        const auto semi = spec.find(';');
        o.description = trim(spec.substr(0, semi));
        std::string list = semi == std::string::npos ? "" : trim(spec.substr(semi + 1));
        std::size_t start = 0;
        while (start <= list.size() && !list.empty())
        {
            const auto bar = list.find('|', start);
            o.choices.push_back(trim(list.substr(start, bar - start)));
            if (bar == std::string::npos)
                break;
            start = bar + 1;
        }
        o.default_value = o.choices.empty() ? "" : o.choices.front();
        o.value = o.default_value;
        h.options.push_back(std::move(o));
    }
}

void options_defined()
{
    apply_porpoise_defaults();
    apply_user_overrides();
    write_options_reference();
    h.options_updated = true;
    ps5::debug::mark_value("options: defined by the core", static_cast<long long>(h.options.size()));
}

/* ---- logging and perf ------------------------------------------------------------------ */

void core_log(enum retro_log_level level, const char *fmt, ...)
{
    if (!h.log)
        return;
    static const char *const names[] = {"debug", "info", "warn", "error"};
    std::fprintf(h.log, "[%s] ", names[level <= RETRO_LOG_ERROR ? level : RETRO_LOG_ERROR]);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(h.log, fmt, args);
    va_end(args);
}

retro_time_t perf_time_usec()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return retro_time_t(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}

uint64_t perf_cpu_features()
{
    /* Zen 2: SSE through AVX2. */
    return RETRO_SIMD_SSE | RETRO_SIMD_SSE2 | RETRO_SIMD_SSE3 | RETRO_SIMD_SSSE3 | RETRO_SIMD_SSE4 |
           RETRO_SIMD_SSE42 | RETRO_SIMD_AVX | RETRO_SIMD_AVX2 | RETRO_SIMD_POPCNT | RETRO_SIMD_CMOV;
}

retro_perf_tick_t perf_counter()
{
    return static_cast<retro_perf_tick_t>(perf_time_usec());
}

void perf_register(retro_perf_counter *counter)
{
    if (counter)
        counter->registered = true;
}

void perf_start(retro_perf_counter *counter)
{
    if (counter)
        counter->start = perf_counter();
}

void perf_stop(retro_perf_counter *counter)
{
    if (counter)
    {
        counter->total += perf_counter() - counter->start;
        counter->call_cnt++;
    }
}

void perf_log() {}

bool set_rumble(unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
    if (port != 0)
        return false;
    porpoise::pad::set_rumble(effect == RETRO_RUMBLE_STRONG, strength);
    return true;
}

/* ---- environment ----------------------------------------------------------------------- */

bool environment(unsigned cmd, void *data)
{
    /* Matched whole: several IDs carry RETRO_ENVIRONMENT_EXPERIMENTAL as part of
     * their value (the Vulkan handshake among them), so the bit must not be
     * stripped before the switch. */
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<const char **>(data) = h.paths.system;
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char **>(data) = h.paths.saves;
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
        *static_cast<const char **>(data) = h.paths.assets;
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback *>(data)->log = core_log;
        return true;
    case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
    {
        auto *perf = static_cast<retro_perf_callback *>(data);
        perf->get_time_usec = perf_time_usec;
        perf->get_cpu_features = perf_cpu_features;
        perf->get_perf_counter = perf_counter;
        perf->perf_register = perf_register;
        perf->perf_start = perf_start;
        perf->perf_stop = perf_stop;
        perf->perf_log = perf_log;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return true;
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
        *static_cast<unsigned *>(data) = RETRO_HW_CONTEXT_VULKAN;
        return true;
    case RETRO_ENVIRONMENT_SET_HW_RENDER:
    {
        auto *hw = static_cast<retro_hw_render_callback *>(data);
        if (hw->context_type != RETRO_HW_CONTEXT_VULKAN)
            return false;
        h.hw = *hw;
        h.hw_requested = true;
        ps5::debug::mark("core: asked for Vulkan");
        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
        h.negotiation =
            static_cast<const retro_hw_render_context_negotiation_interface_vulkan *>(data);
        return true;
    case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
    {
        retro_hw_render_interface_vulkan *iface = porpoise::vk::render_interface();
        *static_cast<const retro_hw_render_interface **>(data) =
            reinterpret_cast<const retro_hw_render_interface *>(iface);
        return iface != nullptr;
    }
    case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *static_cast<unsigned *>(data) = 2;
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        define_options_v2(static_cast<const retro_core_options_v2 *>(data));
        options_defined();
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        define_options_v1(static_cast<const retro_core_option_definition *>(data));
        options_defined();
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        define_options_v0(static_cast<const retro_variable *>(data));
        options_defined();
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE:
    {
        auto *var = static_cast<retro_variable *>(data);
        if (!var || !var->key)
            return false;
        Option *o = find_option(var->key);
        if (!o)
        {
            var->value = nullptr;
            return false;
        }
        var->value = o->value.c_str();
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool *>(data) = h.options_updated;
        h.options_updated = false;
        return true;
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
        static_cast<retro_rumble_interface *>(data)->set_rumble_state = set_rumble;
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    {
        const auto *geometry = static_cast<const retro_game_geometry *>(data);
        if (geometry->aspect_ratio > 0.0f)
            h.aspect = geometry->aspect_ratio;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    {
        const auto *av = static_cast<const retro_system_av_info *>(data);
        if (av->geometry.aspect_ratio > 0.0f)
            h.aspect = av->geometry.aspect_ratio;
        if (av->timing.sample_rate > 1000.0)
        {
            h.sample_rate = av->timing.sample_rate;
            porpoise::audio::set_source_rate(h.sample_rate);
        }
        h.fps = av->timing.fps;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
        *static_cast<float *>(data) = static_cast<float>(porpoise::vk::refresh_hz());
        return true;
    case RETRO_ENVIRONMENT_GET_JIT_CAPABLE:
        *static_cast<bool *>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
        *static_cast<bool *>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
        *static_cast<unsigned *>(data) = 1;
        return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE:
    {
        const auto *msg = static_cast<const retro_message *>(data);
        if (msg && msg->msg)
            core_log(RETRO_LOG_INFO, "message: %s\n", msg->msg);
        return true;
    }
    case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
    {
        const auto *msg = static_cast<const retro_message_ext *>(data);
        if (msg && msg->msg)
            core_log(RETRO_LOG_INFO, "message: %s\n", msg->msg);
        return true;
    }
    /* Accepted and not needed by Porpoise yet. */
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
        return true;
    /* Refused on purpose: the core then pushes audio itself each frame, paced
     * by the display (SET_AUDIO_CALLBACK, SET_FRAME_TIME_CALLBACK), reads files
     * with plain stdio (GET_VFS_INTERFACE), and leaves out the motion sensor and
     * microphone. EXEC_MEM_ALLOC is an iOS path; on PS5 the core takes JIT
     * memory from the platform layer directly. */
    default:
        return false;
    }
}

/* ---- frame callbacks ------------------------------------------------------------------- */

void video_refresh(const void *data, unsigned width, unsigned height, std::size_t)
{
    /* The frame is presented by the run loop after retro_run returns, once per
     * iteration; data == nullptr is a duplicate frame and keeps the last one. */
    if (data == RETRO_HW_FRAME_BUFFER_VALID && width && height)
    {
        h.last_width = width;
        h.last_height = height;
        h.have_frame = true;
    }
}

void audio_sample(int16_t left, int16_t right)
{
    const int16_t frame[2] = {left, right};
    ++h.samples_in;
    porpoise::audio::push(frame, 1);
}

std::size_t audio_batch(const int16_t *data, std::size_t frames)
{
    h.samples_in += frames;
    porpoise::audio::push(data, frames);
    return frames;
}

void input_poll()
{
    /* The pad is read once per frame by the run loop, before retro_run. */
}

int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    if (port != 0 || h.hold_input)
        return 0;
    const porpoise::pad::State &pad = porpoise::pad::state();
    switch (device)
    {
    case RETRO_DEVICE_JOYPAD:
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return static_cast<int16_t>(pad.joypad);
        return id < 16 && (pad.joypad & (1u << id)) ? 1 : 0;
    case RETRO_DEVICE_ANALOG:
        if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            return id == RETRO_DEVICE_ID_ANALOG_X ? pad.left_x : pad.left_y;
        if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            return id == RETRO_DEVICE_ID_ANALOG_X ? pad.right_x : pad.right_y;
        if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
        {
            if (id == RETRO_DEVICE_ID_JOYPAD_L2)
                return pad.l2;
            if (id == RETRO_DEVICE_ID_JOYPAD_R2)
                return pad.r2;
            return id < 16 && (pad.joypad & (1u << id)) ? 0x7fff : 0;
        }
        return 0;
    default:
        return 0;
    }
}

long long monotonic_ns()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<long long>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

/* Sleep most of the way, then spin the last stretch: a sleep can overshoot by
 * up to a millisecond, which over a 16.7 ms frame is visible judder. */
void sleep_until_ns(long long deadline)
{
    for (;;)
    {
        const long long left = deadline - monotonic_ns();
        if (left <= 0)
            return;
        if (left > 1500000)
        {
            const long long nap = left - 1000000;
            timespec ts{static_cast<time_t>(nap / 1000000000LL), static_cast<long>(nap % 1000000000LL)};
            nanosleep(&ts, nullptr);
        }
    }
}

/* ---- loading --------------------------------------------------------------------------- */

template <typename F> bool symbol(F &fn, const char *name)
{
    fn = reinterpret_cast<F>(ps5_core_dlsym(h.library, name));
    if (!fn)
        ps5::debug::mark(name);
    return fn != nullptr;
}

bool load_core()
{
    h.library = ps5_core_dlopen(h.paths.core, 0);
    if (!h.library)
    {
        const char *error = ps5_core_dlerror();
        ps5::debug::mark(error ? error : "core: load failed");
        return false;
    }
    bool ok = true;
    ok &= symbol(h.api.init, "retro_init");
    ok &= symbol(h.api.deinit, "retro_deinit");
    ok &= symbol(h.api.get_system_info, "retro_get_system_info");
    ok &= symbol(h.api.get_system_av_info, "retro_get_system_av_info");
    ok &= symbol(h.api.set_environment, "retro_set_environment");
    ok &= symbol(h.api.set_video_refresh, "retro_set_video_refresh");
    ok &= symbol(h.api.set_audio_sample, "retro_set_audio_sample");
    ok &= symbol(h.api.set_audio_sample_batch, "retro_set_audio_sample_batch");
    ok &= symbol(h.api.set_input_poll, "retro_set_input_poll");
    ok &= symbol(h.api.set_input_state, "retro_set_input_state");
    ok &= symbol(h.api.set_controller_port_device, "retro_set_controller_port_device");
    ok &= symbol(h.api.load_game, "retro_load_game");
    ok &= symbol(h.api.unload_game, "retro_unload_game");
    ok &= symbol(h.api.run, "retro_run");
    return ok;
}

/* The core is unmapped after each game (once its threads have ended), so the
 * next game starts with Dolphin's state fresh. */
void unload_core()
{
    if (h.library)
        ps5_core_dlclose(h.library);
    h.library = nullptr;
}

bool read_whole_file(const char *path, std::vector<unsigned char> &out)
{
    std::FILE *f = std::fopen(path, "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0)
    {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}
} // namespace

/* libretro-common's rtime_localtime, which the core imports from its host
 * (RetroArch provided it): a thread-safe localtime. */
extern "C" struct tm *rtime_localtime(const time_t *timep, struct tm *result)
{
    return localtime_r(timep, result);
}

namespace porpoise::core
{
void set_option(const char *key, const char *value)
{
    if (Option *o = find_option(key))
    {
        o->value = value;
        h.options_updated = true;
    }
}

void set_sharp(bool sharp)
{
    h.sharp = sharp;
}

Exit run_game(const char *game_path, const Paths &paths, const Hooks &hooks, const Playback &playback)
{
    /* A fresh start: the previous game's core state is gone with its core. */
    h.paths = paths;
    h.library = nullptr;
    h.api = CoreApi{};
    h.options.clear();
    h.options_updated = false;
    h.hw = retro_hw_render_callback{};
    h.hw_requested = false;
    h.negotiation = nullptr;
    h.aspect = 4.0f / 3.0f;
    h.sample_rate = 32000.0;
    h.fps = 60.0;
    h.presented = false;
    h.last_width = h.last_height = 0;
    h.hold_input = false;
    h.sharp = playback.sharp;
    h.have_frame = false;
    h.samples_in = 0;
    porpoise::audio::set_volume(playback.volume);
    porpoise::audio::set_muted(playback.muted);
    auto status = [&](const char *text) {
        if (hooks.status)
            hooks.status(text, hooks.user);
    };
    status("Loading Dolphin");
    mkdir("/app0/porpoise", 0777);
    mkdir(paths.saves, 0777);
    h.log = std::fopen(paths.log, "w");

    ps5::debug::mark(game_path);
    if (!load_core())
        return Exit::Failed;

    h.api.set_environment(environment);
    h.api.set_video_refresh(video_refresh);
    h.api.set_audio_sample(audio_sample);
    h.api.set_audio_sample_batch(audio_batch);
    h.api.set_input_poll(input_poll);
    h.api.set_input_state(input_state);
    h.api.init();
    ps5::debug::mark("core: retro_init done");

    retro_system_info system{};
    h.api.get_system_info(&system);
    {
        char line[256];
        std::snprintf(line, sizeof line, "core: %s %s, need_fullpath=%d",
                      system.library_name ? system.library_name : "?",
                      system.library_version ? system.library_version : "?",
                      system.need_fullpath ? 1 : 0);
        ps5::debug::mark(line);
    }

    status("Reading the disc");
    retro_game_info game{};
    game.path = game_path;
    std::vector<unsigned char> bytes;
    if (!system.need_fullpath)
    {
        if (!read_whole_file(game_path, bytes))
        {
            ps5::debug::mark("core: could not read the game file");
            h.api.deinit();
            unload_core();
            return Exit::Failed;
        }
        game.data = bytes.data();
        game.size = bytes.size();
    }
    if (!h.api.load_game(&game))
    {
        ps5::debug::mark("core: retro_load_game refused the game");
        h.api.deinit();
        unload_core();
        return Exit::Failed;
    }
    ps5::debug::mark("core: game loaded");
    h.api.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    retro_system_av_info av{};
    h.api.get_system_av_info(&av);
    if (av.geometry.aspect_ratio > 0.0f)
        h.aspect = av.geometry.aspect_ratio;
    else if (av.geometry.base_height)
        h.aspect = float(av.geometry.base_width) / float(av.geometry.base_height);
    h.sample_rate = av.timing.sample_rate > 1000.0 ? av.timing.sample_rate : 32000.0;
    h.fps = av.timing.fps;
    porpoise::audio::set_source_rate(h.sample_rate);
    {
        char line[200];
        std::snprintf(line, sizeof line, "core: %ux%u aspect %.3f, %.3f fps, audio %.0f Hz",
                      av.geometry.base_width, av.geometry.base_height, h.aspect, h.fps, h.sample_rate);
        ps5::debug::mark(line);
    }

    if (!h.hw_requested)
    {
        ps5::debug::mark("core: did not ask for Vulkan; Porpoise needs it");
        h.api.unload_game();
        h.api.deinit();
        unload_core();
        return Exit::Failed;
    }
    /* The display changes hands: the launcher's device goes, Dolphin makes its
     * own with the features it needs, and the launcher's renderer is rebuilt
     * on it to keep the launch screen up until the game's first picture. */
    if (hooks.device_closing)
        hooks.device_closing(hooks.user);
    porpoise::vk::close_device();
    if (!porpoise::vk::open_device(h.negotiation))
    {
        h.api.unload_game();
        h.api.deinit();
        unload_core();
        return Exit::Failed;
    }
    if (hooks.device_ready)
        hooks.device_ready(hooks.user);
    status("Preparing graphics");
    if (h.hw.context_reset)
        h.hw.context_reset();
    ps5::debug::mark("core: context_reset done; running");

    unsigned long long frames = 0;
    /* Pacing. Two clocks hold a game to its real speed:
     *
     * - The speakers. Dolphin makes its sound as it emulates, so the samples
     *   it has produced measure emulated time exactly. When more than ~60 ms
     *   are waiting to be played, the loop waits for the speakers, the way
     *   RetroArch's audio sync does. This holds whatever a retro_run covers.
     * - The display rate. The console's present does not wait for the display
     *   the way a desktop FIFO swapchain does, so each frame is also given
     *   1/fps of wall time from a running deadline; this keeps video smooth
     *   and covers the moments with no audio (loading). A stall longer than a
     *   few frames resets the deadline rather than racing to catch up.
     *
     * Dolphin reports 32 kHz audio before the game starts but its sound
     * stream runs at 48 kHz; the rate is asked for again once sound flows,
     * or the resampler would stretch the audio by half and overflow. */
    constexpr std::size_t kAudioHighWater = 2304 + 512; /* 48 kHz frames, ~59 ms */
    long long deadline = monotonic_ns();
    long long window_start = deadline;
    unsigned long long window_frames = 0, window_samples = 0;
    long long window_audio_wait_us = 0;
    double measured_fps = 0;
    long long fps_start = deadline;
    unsigned fps_frames = 0;
    bool rate_checked = false;
    Exit exit = Exit::Home;
    bool paused = false, combo_was = false;
    for (;;)
    {
        const porpoise::pad::State &pad = porpoise::pad::poll();
        /* Options + touch pad: the in-game menu, which pauses the game. */
        const bool combo = pad.ps_menu_combo;
        const bool combo_pressed = combo && !combo_was;
        combo_was = combo;
        if (h.hold_input && pad.buttons == 0)
            h.hold_input = false;
        if (!paused && combo_pressed && hooks.paused)
        {
            paused = true;
            porpoise::audio::flush();
            if (hooks.opened)
                hooks.opened(hooks.user);
            ps5::debug::mark("core: paused (menu)");
        }
        if (paused)
        {
            const int answer = hooks.paused(hooks.user);
            if (answer == kMenuLibrary || answer == kMenuHome)
            {
                exit = answer == kMenuLibrary ? Exit::Library : Exit::Home;
                ps5::debug::mark(answer == kMenuLibrary ? "core: back to the library" : "core: closing Porpoise");
                break;
            }
            if (answer == kMenuResume)
            {
                paused = false;
                h.hold_input = true;
                deadline = monotonic_ns();
                ps5::debug::mark("core: resumed");
            }
            /* The paused picture, with the menu over it. */
            if (hooks.frame)
                hooks.frame(h.have_frame, measured_fps, hooks.user);
            if (h.have_frame)
                porpoise::vk::present_core_frame(h.last_width, h.last_height, h.aspect, h.sharp);
            else
                porpoise::vk::present_clear(0, 0, 0);
            const long long period = static_cast<long long>(1e9 / 60.0);
            deadline += period;
            const long long now = monotonic_ns();
            if (now > deadline + 4 * period)
                deadline = now;
            else
                sleep_until_ns(deadline);
            continue;
        }
        h.api.run();

        if (!rate_checked && h.samples_in > 4096)
        {
            rate_checked = true;
            retro_system_av_info now_av{};
            h.api.get_system_av_info(&now_av);
            char line[160];
            std::snprintf(line, sizeof line, "core: sound is flowing; core now reports %.0f Hz (was %.0f), %.3f fps",
                          now_av.timing.sample_rate, h.sample_rate, now_av.timing.fps);
            ps5::debug::mark(line);
            if (h.log)
                std::fprintf(h.log, "[porpoise] %s\n", line);
            if (now_av.timing.sample_rate > 1000.0)
            {
                h.sample_rate = now_av.timing.sample_rate;
                porpoise::audio::set_source_rate(h.sample_rate);
            }
            if (now_av.timing.fps > 10.0 && now_av.timing.fps < 200.0)
                h.fps = now_av.timing.fps;
        }

        if (hooks.frame)
            hooks.frame(h.have_frame, measured_fps, hooks.user);
        if (h.have_frame)
            porpoise::vk::present_core_frame(h.last_width, h.last_height, h.aspect, h.sharp);
        else
            porpoise::vk::present_clear(0, 0, 0);

        /* The speakers first: never run ahead of what has been heard. */
        if (rate_checked)
            window_audio_wait_us += porpoise::audio::wait_below(kAudioHighWater, 40);

        /* Never above 60: a game's own rate (59.94 NTSC, 50 PAL) or less. */
        const double fps = h.fps > 10.0 && h.fps < 60.5 ? h.fps : 60.0;
        const long long period = static_cast<long long>(1e9 / fps);
        deadline += period;
        long long now = monotonic_ns();
        if (now > deadline + 4 * period)
            deadline = now; /* fell behind: start again from here */
        else
            sleep_until_ns(deadline);

        ++frames;
        ++window_frames;
        if (++fps_frames == 30)
        {
            const long long t = monotonic_ns();
            measured_fps = fps_frames * 1e9 / double(std::max(1LL, t - fps_start));
            fps_start = t;
            fps_frames = 0;
        }
        if (frames == 1)
            ps5::debug::mark("core: first frame run");
        now = monotonic_ns();
        if (now - window_start >= 10'000'000'000LL && h.log)
        {
            const double secs = (now - window_start) / 1e9;
            const unsigned long long samples = h.samples_in - window_samples;
            /* Emulated speed: the core's sound per wall second over its rate. */
            std::fprintf(h.log,
                         "[porpoise] %.2f runs/s (target %.3f), %.0f audio frames/s at %.0f Hz = %.0f%% speed, "
                         "%.0f per run, queue %zu, waited %.1f ms/s for the speakers\n",
                         window_frames / secs, fps, samples / secs, h.sample_rate,
                         100.0 * samples / secs / h.sample_rate, window_frames ? double(samples) / window_frames : 0.0,
                         porpoise::audio::queued(), window_audio_wait_us / 1000.0 / secs);
            std::fflush(h.log);
            window_start = now;
            window_frames = 0;
            window_samples = h.samples_in;
            window_audio_wait_us = 0;
        }
    }

    porpoise::audio::flush();
    porpoise::vk::close();
    /* Everything Porpoise made on the core's device goes before the core
     * takes the device down with it. */
    if (hooks.device_closing)
        hooks.device_closing(hooks.user);
    porpoise::vk::close_device();
    if (h.hw.context_destroy)
        h.hw.context_destroy();
    h.api.unload_game();
    h.api.deinit();
    unload_core();
    ps5::debug::mark("core: game closed");
    if (h.log)
        std::fclose(h.log);
    h.log = nullptr;
    h.hold_input = false;
    return exit;
}
} // namespace porpoise::core
