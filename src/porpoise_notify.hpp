/* Porpoise - PS5 notifications: plain ones, and rich ones that look and sound
 * like the console's own trophies (RetroAchievements' unlocks).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Callers only queue: a worker thread sends, one toast at a time, spaced so
 * the system shows each picture. A rich toast is posted by libSceNotification;
 * when Porpoise's process may not load it (recent firmware), a small payload
 * carries it to the jailbreak's ELF loader (127.0.0.1:9021); without either,
 * the text goes out as a plain notification. */
#pragma once

#include <string>

namespace porpoise::notify
{
enum class Sound
{
    Default,  /* the system's usual notification sound */
    Trophy,   /* the trophy ding */
    Platinum, /* the platinum trophy sound */
    Silent,
};

/* message and sub: two lines. icon: an https URL or a PNG under /data (empty
 * for none). trophy: in the Trophies channel, so it follows the player's
 * trophy notification settings, as a real trophy does. */
void rich(const std::string &message, const std::string &sub, const std::string &icon, Sound sound,
          bool trophy = false);
/* Text only. */
void plain(const std::string &text);
/* Toasts queued from now on wait `seconds` first (a game starting: the
 * system drops the picture of a toast sent in a game's first seconds). */
void hold(int seconds);
/* Waits up to max_ms for the queue to be sent (before Porpoise closes). */
void flush(int max_ms);
/* Whether rich toasts reach the screen: -1 not tried yet, 1 yes (libSceNotification
 * or the ELF loader), 0 no (only plain notifications on this console). */
int rich_state();
} // namespace porpoise::notify
