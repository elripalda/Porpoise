/* Porpoise on Windows: no PS5 notifications (RetroAchievements is PS5-only
 * for now; the Windows core is built without it).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_notify.hpp"

namespace porpoise::notify
{
void rich(const std::string &, const std::string &, const std::string &, Sound, bool) {}
void plain(const std::string &) {}
void hold(int) {}
void flush(int) {}
} // namespace porpoise::notify
