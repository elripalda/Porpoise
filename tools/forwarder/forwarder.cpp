/* Porpoise - a home screen forwarder: a tiny app with its own tile that
 * opens Porpoise (PPSA99764) on one game. Porpoise writes one for a game
 * (its settings > Home screen > Add to home screen): this program as its
 * eboot.bin, a forward.txt beside it naming the game, home-launcher.elf, and
 * the tile's art.
 *
 * An app may not start another app itself, so this one hands the home
 * launcher (tools/home-launcher) to the jailbreak's ELF loader on
 * 127.0.0.1:9021 with the game written into it, and closes; the launcher,
 * a process of the jailbreak's, opens Porpoise once this tile has gone.
 * It uses only what Porpoise itself already uses, so it always starts.
 *
 * forward.txt: the game's file on the first line; "exit-after-game" on a line
 * of its own closes Porpoise when that game is left. What happened goes to
 * forward-log.txt beside it.
 *
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C"
{
    int sceSystemServiceLoadExec(const char *path, const char *const *argv);
    int sceSystemServiceHideSplashScreen();
    int sceKernelUsleep(unsigned microseconds);
}

namespace
{
std::FILE *g_log = nullptr;

void note(const char *what, long long value)
{
    if (g_log)
    {
        std::fprintf(g_log, "%s = %lld\n", what, value);
        std::fflush(g_log);
    }
}

[[noreturn]] void leave()
{
    if (g_log)
        std::fclose(g_log);
    g_log = nullptr;
    sceSystemServiceLoadExec("exit", nullptr);
    for (;;)
        sceKernelUsleep(100000);
}
} // namespace

int main()
{
    g_log = std::fopen("/app0/forward-log.txt", "w");
    sceSystemServiceHideSplashScreen();

    /* The request: forward.txt as it is (the game, then its flags). */
    static char request[2048];
    std::size_t request_size = 0;
    if (std::FILE *f = std::fopen("/app0/forward.txt", "rb"))
    {
        request_size = std::fread(request, 1, sizeof request - 1, f);
        std::fclose(f);
    }
    request[request_size] = '\0';
    if (g_log)
        std::fprintf(g_log, "request: %s\n", request_size ? request : "(none)");
    if (request_size == 0 || request[0] != '/')
        leave();

    /* The launcher, with the request written over its marker. */
    static unsigned char elf[256 * 1024];
    std::size_t elf_size = 0;
    if (std::FILE *f = std::fopen("/app0/home-launcher.elf", "rb"))
    {
        elf_size = std::fread(elf, 1, sizeof elf, f);
        std::fclose(f);
    }
    note("launcher bytes", static_cast<long long>(elf_size));
    static const char kMarker[] = "PORPOISE-HOME-LAUNCH-V1";
    unsigned char *at = nullptr;
    for (std::size_t i = 0; elf_size > sizeof kMarker && i + sizeof kMarker < elf_size && !at; ++i)
        if (std::memcmp(elf + i, kMarker, sizeof kMarker - 1) == 0)
            at = elf + i;
    if (!at || static_cast<std::size_t>(elf + elf_size - at) < 2048)
    {
        note("no room for the request in the launcher", 0);
        leave();
    }
    std::memset(at, 0, 2048);
    std::memcpy(at, request, request_size < 2047 ? request_size : 2047);

    /* To the jailbreak's ELF loader. */
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9021);
    addr.sin_addr.s_addr = htonl(0x7F000001);
    const int connected = fd >= 0 ? connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr) : -1;
    note("connected to the ELF loader (0 = yes)", connected);
    if (connected == 0)
    {
        std::size_t sent = 0;
        while (sent < elf_size)
        {
            const ssize_t n = send(fd, elf + sent, elf_size - sent, 0);
            if (n <= 0)
                break;
            sent += std::size_t(n);
        }
        note("launcher sent, bytes", static_cast<long long>(sent));
    }
    if (fd >= 0)
        close(fd);
    sceKernelUsleep(300 * 1000);
    leave(); /* the launcher opens Porpoise once this tile has gone */
}
