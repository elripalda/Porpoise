/* Porpoise - the home launcher: a tiny payload a home screen tile hands to
 * the jailbreak's ELF loader (127.0.0.1:9021), which runs it as a process of
 * its own. An app can't start another app (the system answers "not
 * allowed"), so the tile asks this: once the tile has closed, it opens
 * Porpoise (PPSA99764) with the game (--rom).
 *
 * The tile writes its request over g_request's marker in its copy of this
 * program: the game on the first line, "exit-after-game" on the next when
 * wanted. What happened goes to /data/porpoise/home-art/last-launch.txt.
 *
 * The launch call and its parameters follow shsrv's "launch" command (John
 * Törnblom, ps5-payload-dev/shsrv, bundles/launch/launch.c, GPL-3.0-or-later).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct app_launch_ctx
{
    uint32_t structsize;
    uint32_t user_id;
    uint32_t app_opt;
    uint64_t crash_report;
    uint32_t check_flag;
} app_launch_ctx_t;

int sceUserServiceInitialize(void *);
int sceUserServiceGetForegroundUser(uint32_t *user_id);
int sceSystemServiceLaunchApp(const char *title_id, char **argv, app_launch_ctx_t *ctx);
int sceSystemServiceGetAppIdOfRunningBigApp(void);

/* Initialized, so the whole array is in the file: the room the tile fills.
 * volatile: what the tile wrote is read, not the marker seen here. */
__attribute__((used)) volatile char g_request[2048] = "PORPOISE-HOME-LAUNCH-V1";

static FILE *g_log;

static void note(const char *what, long value)
{
    if (g_log)
    {
        fprintf(g_log, "%s = %ld (0x%lx)\n", what, value, (unsigned long)value);
        fflush(g_log);
    }
}

int main(void)
{
    static char request[2048];
    for (unsigned i = 0; i < sizeof request - 1; ++i)
        request[i] = g_request[i];
    if (strncmp(request, "PORPOISE-HOME-LAUNCH-V1", 23) == 0 || request[0] != '/')
        return 1; /* no request written */
    g_log = fopen("/data/porpoise/home-art/last-launch.txt", "w");

    char *game = request;
    char *rest = strchr(request, '\n');
    int exit_after = 0;
    if (rest)
    {
        *rest++ = '\0';
        exit_after = strstr(rest, "exit-after-game") != NULL;
    }
    if (g_log)
        fprintf(g_log, "game: %s\n", game);

    /* The tile is the running app now; it closes itself after sending this.
     * Wait (up to 6 s) until it is gone, so Porpoise isn't asked to wait. */
    const int tile = sceSystemServiceGetAppIdOfRunningBigApp();
    note("running app (the tile)", tile);
    for (int i = 0; i < 60; ++i)
    {
        const int now = sceSystemServiceGetAppIdOfRunningBigApp();
        if (now != tile || now < 0)
            break;
        usleep(100 * 1000);
    }
    usleep(300 * 1000);

    note("user service", sceUserServiceInitialize(0));
    app_launch_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    note("foreground user result", sceUserServiceGetForegroundUser(&ctx.user_id));
    note("user", (long)ctx.user_id);

    char *argv[5] = {"PPSA99764", "--rom", game, NULL, NULL};
    if (exit_after)
        argv[3] = "--exit-after-game";
    int result = -1;
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        result = sceSystemServiceLaunchApp("PPSA99764", argv, &ctx);
        note("sceSystemServiceLaunchApp", result);
        if (result >= 0)
            break;
        usleep(500 * 1000); /* the tile may still be closing */
    }
    if (g_log)
        fclose(g_log);
    _exit(result >= 0 ? 0 : 2);
}
