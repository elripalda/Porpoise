/* Porpoise - a home screen forwarder: a tiny app with its own tile that
 * opens Porpoise (PPSA99764) on one game. Porpoise writes one for a game
 * (its settings > Add to home screen): this program as its eboot.bin, a
 * forward.txt beside it naming the game, and the tile's art.
 *
 * forward.txt: the game's file on the first line; "exit-after-game" on a line
 * of its own closes Porpoise when that game is left (docs/FORWARDER.md).
 * What happened goes to forward-log.txt beside it (Porpoise's diagnostic test
 * reads it).
 *
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <cstdint>
#include <cstdio>
#include <cstring>

extern "C"
{
    int sceUserServiceInitialize(void *params);
    int sceUserServiceGetForegroundUser(int *user);
    int sceUserServiceGetInitialUser(int *user);
    int sceSystemServiceLaunchApp(const char *title_id, const char **argv, void *param);
    int sceSystemServiceLoadExec(const char *path, const char *const *argv);
    int sceSystemServiceHideSplashScreen();
    int sceLncUtilInitialize();
    int sceLncUtilLaunchApp(const char *title_id, const char **argv, void *param);
    int sceLncUtilGetAppId(const char *title_id);
    int sceLncUtilKillApp(int app_id);
    int sceKernelUsleep(unsigned microseconds);
}

namespace
{
constexpr const char *kPorpoise = "PPSA99764";

/* The launch's parameters, as the system's launcher takes them. */
struct LaunchParam
{
    std::uint32_t size;
    std::int32_t user;
    std::uint32_t options;
    std::uint64_t crash_report;
    std::uint64_t check_flag;
};

std::FILE *g_log = nullptr;

void note(const char *what, long long value)
{
    if (g_log)
    {
        std::fprintf(g_log, "%s = %lld (0x%llx)\n", what, value, static_cast<unsigned long long>(value));
        std::fflush(g_log);
    }
}

void trim(char *s)
{
    std::size_t n = std::strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
        s[--n] = '\0';
}
} // namespace

int main()
{
    g_log = std::fopen("/app0/forward-log.txt", "w");
    sceSystemServiceHideSplashScreen();

    static char game[1024];
    bool exit_after = false;
    if (std::FILE *f = std::fopen("/app0/forward.txt", "r"))
    {
        if (std::fgets(game, sizeof game, f))
            trim(game);
        char line[64];
        while (std::fgets(line, sizeof line, f))
        {
            trim(line);
            if (std::strcmp(line, "exit-after-game") == 0)
                exit_after = true;
        }
        std::fclose(f);
    }
    if (g_log)
        std::fprintf(g_log, "game: %s\n", game[0] ? game : "(none)");

    int user = -1;
    sceUserServiceInitialize(nullptr);
    note("foreground user result", sceUserServiceGetForegroundUser(&user));
    if (user < 0)
        note("initial user result", sceUserServiceGetInitialUser(&user));
    note("user", user);

    const char *argv[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    int argc = 0;
    if (game[0])
    {
        argv[argc++] = "--rom";
        argv[argc++] = game;
        if (exit_after)
            argv[argc++] = "--exit-after-game";
    }

    LaunchParam param{};
    param.size = sizeof param;
    param.user = user;

    /* Porpoise already open would only come to the front, without the game:
     * it is closed first (a tile is a choice to play that game). */
    note("launcher init", sceLncUtilInitialize());
    const int running = sceLncUtilGetAppId(kPorpoise);
    note("Porpoise's app id (running when positive)", running);
    if (running > 0 && running != -1)
    {
        note("closing Porpoise", sceLncUtilKillApp(running));
        sceKernelUsleep(1500 * 1000);
    }

    int result = sceSystemServiceLaunchApp(kPorpoise, argv, &param);
    note("sceSystemServiceLaunchApp", result);
    if (result < 0)
    {
        result = sceLncUtilLaunchApp(kPorpoise, argv, &param);
        note("sceLncUtilLaunchApp", result);
    }
    if (g_log)
        std::fclose(g_log);
    g_log = nullptr;
    sceKernelUsleep(500 * 1000);
    sceSystemServiceLoadExec("exit", nullptr);
    for (;;)
        sceKernelUsleep(100000);
}
