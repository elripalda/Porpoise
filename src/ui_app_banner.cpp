/* Porpoise UI - a Wii disc's own tile and banner on the Revolution look.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_app.hpp"

namespace porpoise::ui
{
bool App::has_banner(const Game &) const
{
    return false;
}

bool App::draw_banner(Game &, float, float, float, float, double, float, bool)
{
    return false;
}

void App::forget_banners()
{
}
} // namespace porpoise::ui
