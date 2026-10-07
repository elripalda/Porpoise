/* Porpoise - asking the HEN to free Porpoise from the app sandbox.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

namespace porpoise::jailbreak
{
/* Whether /data is there and Porpoise can write in /data/porpoise. */
bool data_reachable();
/* Called once at the very top of main(), before any thread is made: gives
 * this process its own credential (seteuid to its own uid). The Lapy owned-
 * root daemon only frees a process that did this first and is still single-
 * threaded; harmless for etaHEN and OnionHEN. */
void prepare();
/* When /data isn't reachable: asks whichever daemon is running (etaHEN,
 * OnionHEN or a Lapy-style one) to free this process (a few seconds at most).
 * True when /data is reachable afterwards. */
bool ensure();
/* The daemon freed Porpoise by making the console's root its own, where
 * Porpoise's folder (/app0) isn't, and Porpoise put the sandbox back as its
 * root to keep running (ensure() then says whether /data is reachable). */
bool root_put_back();
} // namespace porpoise::jailbreak
