/* Porpoise - name lookups for the emulator's network (a Wii game going
 * online: Wiimmfi, a custom server, WiiConnect24 through WiiLink).
 *
 * Dolphin asks the system to turn a server's name into an address
 * (gethostbyname, getaddrinfo). The console's libc has neither in the
 * modules a title loads, so the core imports Porpoise's (tools/core-imports.py):
 * an address as it is; a name through the DNS server the player set (as a
 * Wii's own network settings take one, for a custom server), or else the
 * console's own resolver.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <cstdint>
#include <string>

namespace porpoise::dns
{
/* The DNS server to ask ("192.168.1.2"), or "" for the console's own.
 * Forgets what was looked up before. Safe from any thread. */
void set_server(const std::string &address);
std::string server();
/* Whether text is a plain IPv4 address. */
bool is_address(const std::string &text);
/* A name's IPv4 address (network order) through the server set, or the
 * console's resolver; false when neither knows it. Answers are kept a while. */
bool lookup(const std::string &name, std::uint32_t &address);
} // namespace porpoise::dns
