/* Porpoise - asking the HEN to free Porpoise from the app sandbox.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Porpoise needs to see the whole console: /data for its own folder and the
 * player's games, USB drives, and so on. Most setups start it that way
 * already (etaHEN or the launcher frees each homebrew app). Where it starts
 * inside the sandbox, /data is missing or empty and the folder browser shows
 * only a few folders; Porpoise then asks the HEN itself, the way PS5SX2 does
 * (ps5/coreorbis/orbis-shims/ProsperoHenJailbreak.cpp and main-boot.cpp in
 * PS5SX2, GPL-3.0-or-later; this is a port of its approach):
 *
 *   1. etaHEN / OnionHEN's request file: {"PID":<pid>} written to
 *      /download0/etahen_jailbreak (atomically, through a .tmp). The HEN
 *      consumes it and frees the process, if the title ID is on its app
 *      jailbreak list.
 *   2. Failing that, the legacy command servers on 127.0.0.1 (etaHEN 9028,
 *      the SharpProspero unjail daemon 9069): command 5, jailbreak this PID.
 *
 * Every step goes to trace.txt. Nothing here runs when /data is reachable. */
#include "porpoise_jailbreak.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <initializer_list>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "trace.hpp"

extern "C" int sceKernelUsleep(unsigned microseconds);

namespace porpoise::jailbreak
{
namespace
{
constexpr char kRequest[] = "/download0/etahen_jailbreak";
constexpr char kStaged[] = "/download0/etahen_jailbreak.tmp";

void note(const char *fmt, int a = 0, int b = 0, int c = 0)
{
    char line[200];
    std::snprintf(line, sizeof line, fmt, a, b, c);
    ps5::debug::mark(line);
}

/* The HEN's request file. True when it was consumed (and, ideally, we are root). */
bool request_file()
{
    const int pid = int(getpid());
    unlink(kStaged);
    unlink(kRequest);
    const int fd = open(kStaged, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (fd < 0)
    {
        note("jailbreak: request file can't be made (errno %d)", errno);
        return false;
    }
    fchmod(fd, 0666);
    char body[32];
    const int n = std::snprintf(body, sizeof body, "{\"PID\":%d}\n", pid);
    const bool written = write(fd, body, std::size_t(n)) == n && fsync(fd) == 0;
    close(fd);
    if (!written || rename(kStaged, kRequest) != 0)
    {
        unlink(kStaged);
        note("jailbreak: request file not published (errno %d)", errno);
        return false;
    }
    /* Up to 4 s for the HEN to take it, then up to 2 s for it to finish. */
    int polls = 0;
    while (access(kRequest, F_OK) == 0 && polls < 240)
    {
        sceKernelUsleep(16667);
        ++polls;
    }
    if (access(kRequest, F_OK) == 0)
    {
        unlink(kRequest);
        note("jailbreak: the HEN didn't take the request (is PPSA99764 on its app jailbreak list?)");
        return false;
    }
    int grace = 0;
    while (geteuid() != 0 && grace < 120)
    {
        sceKernelUsleep(16667);
        ++grace;
    }
    note("jailbreak: request taken after %d polls; uid now %d", polls, int(geteuid()));
    return true;
}

/* The legacy command servers: magic, command 5 (jailbreak), the PID. */
bool request_port()
{
    struct Command
    {
        int magic;
        int cmd;
        int pid;
        int ret;
        char msg1[0x500];
        char msg2[0x500];
    } cmd{};
    for (int port : {9028, 9069})
    {
        cmd = Command{};
        cmd.magic = int(0xDEADBEEF);
        cmd.cmd = 5;
        cmd.pid = int(getpid());
        cmd.ret = -1337;
        const int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            return false;
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(std::uint16_t(port));
        sa.sin_addr.s_addr = htonl(0x7F000001);
        if (connect(fd, reinterpret_cast<sockaddr *>(&sa), sizeof sa) != 0)
        {
            close(fd);
            note("jailbreak: nothing on port %d", port);
            continue;
        }
        const bool sent = send(fd, &cmd, sizeof cmd, 0) == ssize_t(sizeof cmd);
        int got = 0;
        while (sent && got < int(sizeof cmd))
        {
            const ssize_t r = recv(fd, reinterpret_cast<char *>(&cmd) + got, sizeof cmd - std::size_t(got), 0);
            if (r <= 0)
                break;
            got += int(r);
        }
        close(fd);
        note("jailbreak: port %d answered %d (%d bytes)", port, cmd.ret, got);
        if (sent && (cmd.ret == 0 || cmd.ret == -1337))
            return true;
    }
    return false;
}
} // namespace

bool data_reachable()
{
    struct stat st;
    if (stat("/data", &st) != 0 || !S_ISDIR(st.st_mode))
        return false;
    mkdir("/data/porpoise", 0777);
    const char probe[] = "/data/porpoise/.write-test";
    const int fd = open(probe, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    close(fd);
    unlink(probe);
    return true;
}

bool ensure()
{
    if (data_reachable())
        return true;
    note("jailbreak: /data isn't reachable (uid %d); asking the HEN", int(geteuid()));
    if (!request_file())
        request_port();
    const bool ok = data_reachable();
    note(ok ? "jailbreak: /data is reachable now" : "jailbreak: still sandboxed; using the app's own folder");
    return ok;
}
} // namespace porpoise::jailbreak
