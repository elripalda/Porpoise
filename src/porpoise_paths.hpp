/* Porpoise - where the app and the player's things live.
 *
 * On the PS5 the app is /app0 and the player's things /data/porpoise. The
 * app's own files are found from the working directory, which main() sets to
 * /app0 before anything else: a jailbreak that frees Porpoise from the
 * sandbox (the Lapy daemon, LegacyJB) moves its root to the console's own,
 * where there is no /app0, but leaves the working directory where it was.
 * 2.1.2: with "/app0" in every path, a freed Porpoise found none of its
 * files and closed at once. On a
 * desktop (PORPOISE_DESKTOP: Windows), Porpoise runs from its own folder
 * (the working directory is set to it at start): the app's files beside the
 * program, the player's in its data folder.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#ifdef PORPOISE_DESKTOP
#define PORPOISE_APP "."
#define PORPOISE_DATA "./data"
#else
#define PORPOISE_APP "." /* /app0, as the working directory (above) */
#define PORPOISE_DATA "/data/porpoise"
#endif
