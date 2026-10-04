/*
 * PS5 RetroArch - the present clock with ps5vk: the driver's own.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#if !defined(PS5_RETROARCH_RADV)
#include "present_clock.h"

/* ../PS5_Vulkan's driver/ps5vk_debug.h. */
uint64_t ps5vk_debug_now_ns(void);
uint64_t ps5vk_debug_last_present_ns(void);

uint64_t ps5_present_clock_now_ns(void)
{
    return ps5vk_debug_now_ns();
}

uint64_t ps5_present_clock_last_ns(void)
{
    return ps5vk_debug_last_present_ns();
}
#endif
