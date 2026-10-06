/* Porpoise - asking the HEN to free Porpoise from the app sandbox.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Porpoise needs to see the whole console: /data for its own folder and the
 * player's games, USB drives, and so on. Most setups start it that way
 * already (etaHEN or the launcher frees each homebrew app). Where it starts
 * inside the sandbox, /data is missing or empty and the folder browser shows
 * only a few folders; Porpoise then asks a jailbreak daemon itself. This is
 * the etaHEN jailbreak-on-demand API, which OnionHEN and standalone daemons
 * (a Lapy-style daemon) carry too: a process writes its PID to a request
 * file in its own sandbox and the daemon, watching every sandbox, bumps it.
 * Porpoise keeps asking for a short while, the way PS5SX2 does
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
 * Different daemons watch for different file names in /download0: etaHEN and
 * old Lapy want "etahen_jailbreak", OnionHEN also takes "onionhen_jailbreak",
 * and the Lapy owned-root daemon wants "elevate_proc" from a process that has
 * cloned its own credential first (prepare(), called before any thread). So
 * each round publishes all three names; whichever daemon is up takes its own.
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
#include <sys/time.h>
#include <unistd.h>

#include "trace.hpp"

extern "C" int sceKernelUsleep(unsigned microseconds);

namespace porpoise::jailbreak
{
bool data_reachable();

namespace
{
/* The request-file names daemons watch for, in /download0. */
const char *const kRequests[] = {
    "/download0/etahen_jailbreak",  /* etaHEN, OnionHEN, old Lapy */
    "/download0/onionhen_jailbreak", /* OnionHEN also takes this name */
    "/download0/elevate_proc",       /* Lapy owned-root daemon */
};
constexpr int kRequestCount = int(sizeof kRequests / sizeof kRequests[0]);

void note(const char *fmt, int a = 0, int b = 0, int c = 0)
{
    char line[200];
    std::snprintf(line, sizeof line, fmt, a, b, c);
    ps5::debug::mark(line);
}

/* Publishes the request file (the daemon, etaHEN / OnionHEN / a Lapy-style
 * daemon, watches each sandbox for it and bumps the process it names). Waits
 * a short while for it to be taken; true when the daemon took it (the file is
 * gone), so the caller can then wait for /data. One round: ensure() publishes
 * again and again, because the daemon may start a moment after Porpoise and
 * because the first bump can lose a timing race. */
/* Writes {"PID":n} to one request path atomically (through a .tmp). */
bool publish_one(const char *path, int pid)
{
    char staged[64];
    std::snprintf(staged, sizeof staged, "%s.tmp", path);
    unlink(staged);
    unlink(path);
    const int fd = open(staged, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
    if (fd < 0)
        return false;
    fchmod(fd, 0666);
    char body[32];
    const int n = std::snprintf(body, sizeof body, "{\"PID\":%d}\n", pid);
    const bool written = write(fd, body, std::size_t(n)) == n && fsync(fd) == 0;
    close(fd);
    if (!written || rename(staged, path) != 0)
    {
        unlink(staged);
        return false;
    }
    return true;
}

bool request_file()
{
    const int pid = int(getpid());
    int published = 0;
    for (int i = 0; i < kRequestCount; ++i)
        if (publish_one(kRequests[i], pid))
            ++published;
    if (published == 0)
    {
        note("jailbreak: no request file could be made (errno %d)", errno);
        return false;
    }
    /* Up to ~1.5 s for a daemon to take one of the files this round; a daemon
     * consumes (deletes) the name it watches. */
    bool taken = false;
    for (int polls = 0; polls < 90 && !taken; ++polls)
    {
        sceKernelUsleep(16667);
        for (int i = 0; i < kRequestCount; ++i)
            if (access(kRequests[i], F_OK) != 0)
            {
                taken = true;
                note("jailbreak: request taken after %d polls; uid now %d", polls, int(geteuid()));
                break;
            }
    }
    /* Leave nothing behind for the next round, whoever took what. */
    for (int i = 0; i < kRequestCount; ++i)
        unlink(kRequests[i]);
    return taken;
}

/* The legacy command servers: magic, command 5 (jailbreak), the PID. heard:
 * set when a server took the connection (a daemon is there); refused: set when
 * one answered in full and said no. */
bool request_port(bool &heard, bool &refused)
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
        /* A server that takes the connection but never answers mustn't hold
         * Porpoise's start: 2 s each way. */
        timeval limit{};
        limit.tv_sec = 2;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof limit);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &limit, sizeof limit);
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
        heard = true;
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
        if (sent && got == int(sizeof cmd) && cmd.ret == 0)
            return true;
        if (sent && got == int(sizeof cmd))
            refused = true;
    }
    return false;
}
} // namespace

void prepare()
{
    /* Give this process its own credential before any thread exists. The Lapy
     * owned-root daemon only frees a process that did this (and is still
     * single-threaded) first; for etaHEN and OnionHEN it does no harm. A
     * no-op when euid already equals the real uid. */
    if (seteuid(geteuid()) != 0)
        note("jailbreak: seteuid(self) failed (errno %d)", errno);
}

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
    /* A daemon may not be up the instant Porpoise starts (a cold launch from
     * the home screen, or the daemon still loading), and the first bump can
     * lose a timing race, so Porpoise keeps asking for a while rather than
     * once. Each round re-publishes the request file and tries the legacy
     * command ports; after any round that is taken, /data is given a moment
     * to open. The common case (a daemon is up and bumps Porpoise) returns in
     * the first round or two. With no daemon at all (no request taken, no
     * command port open) Porpoise stops after two rounds, about 3 s, and uses
     * its own folder: 2.0 kept asking for 30 s, which looked like a freeze. */
    bool heard = false;
    int refusals = 0;
    for (int round = 0; round < 10; ++round)
    {
        const bool taken = request_file();
        heard = heard || taken;
        bool refused = false;
        if (!taken && request_port(heard, refused))
            refused = false; /* one of them said yes */
        refusals += refused ? 1 : 0;
        if (!heard && round >= 1)
        {
            note("jailbreak: no jailbreak daemon answered in %d rounds", round + 1);
            break;
        }
        /* Up to ~1.5 s for /data to open after a daemon acts (not every one
         * makes the app root the same instant); a quick look when none did. */
        for (int grace = 0; grace < (heard ? 90 : 6); ++grace)
        {
            if (data_reachable())
            {
                note("jailbreak: /data is reachable now (round %d)", round);
                return true;
            }
            sceKernelUsleep(16667);
        }
        if (refusals >= 2)
        {
            /* A daemon that says no (Porpoise isn't on its list) keeps saying no. */
            note("jailbreak: the daemon refused %d times", refusals);
            break;
        }
    }
    note("jailbreak: still sandboxed; using the app's own folder");
    return false;
}
} // namespace porpoise::jailbreak
