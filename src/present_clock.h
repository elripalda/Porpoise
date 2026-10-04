/*
 * PS5 RetroArch - when the title last presented a frame, for the stall sampler.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * With ps5vk these are the driver's own present clock (its ps5vk_debug.h);
 * with RADV the title keeps it, around vkQueuePresentKHR (src/radv_icd_ps5.c).
 */
#ifndef PS5_RETROARCH_PRESENT_CLOCK_H
#define PS5_RETROARCH_PRESENT_CLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* The monotonic clock the times below are on, in nanoseconds. */
    uint64_t ps5_present_clock_now_ns(void);
    /* When the latest present returned; 0 before the first. */
    uint64_t ps5_present_clock_last_ns(void);

#ifdef __cplusplus
}
#endif

#endif
