/* Porpoise - asking the HEN to free Porpoise from the app sandbox.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>

namespace porpoise::jailbreak
{
/* Whether /data is there and Porpoise can write in /data/porpoise. */
bool data_reachable();
/* The console's own /data is there (freed), even if Porpoise can't write
 * /data/porpoise in it. */
bool data_visible();
/* Called once at the very top of main(), before any thread is made: gives
 * this process its own credential (seteuid to its own uid). The Lapy owned-
 * root daemon only frees a process that did this first and is still single-
 * threaded; harmless for etaHEN and OnionHEN. */
void prepare();
/* When /data isn't reachable: asks whichever daemon is running (etaHEN,
 * OnionHEN or a Lapy-style one) to free this process (a few seconds at most).
 * True when /data is reachable afterwards, or at least visible (data_visible:
 * Porpoise's folder then stays in /app0). */
bool ensure();
/* The daemon freed Porpoise by making the console's root its own, where
 * Porpoise's folder (/app0) isn't, and Porpoise put the sandbox back as its
 * root to keep running (ensure() then says whether /data is reachable). */
bool root_put_back();

/* "Stay in the sandbox" (Settings > Games): Porpoise doesn't ask a daemon to
 * free it. A file in /app0/porpoise, since it is read before Porpoise knows
 * where its folder is. */
bool stay_wanted();
void set_stay(bool stay);
/* While ensure() asks a daemon, a note sits in /app0/porpoise saying how far
 * the start got; it goes once Porpoise has been up a few seconds (survived).
 * Still there at the next start: freeing Porpoise closed it, and the stage it
 * reached ("" when the last start was fine). */
std::string closed_last();
void forget_closed();
/* How far this start got, while an attempt is in flight (else nothing). */
void stage(const char *what);
void survived();
} // namespace porpoise::jailbreak
