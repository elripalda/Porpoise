/* Porpoise - where the app and the player's things live.
 *
 * On the PS5 the app is /app0 and the player's things /data/porpoise. On a
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
#define PORPOISE_APP "/app0"
#define PORPOISE_DATA "/data/porpoise"
#endif
