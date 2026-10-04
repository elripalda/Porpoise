/* PS5 RetroArch - the RADV archive's entry point under the name the title uses.
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * RADV exports the ICD's vk_icdGetInstanceProcAddr; the title resolves every
 * Vulkan command through vkGetInstanceProcAddr, as ps5vk's archive names it.
 * The title also keeps its present clock here (src/present_clock.h): the
 * vkQueuePresentKHR it is handed, directly or through vkGetDeviceProcAddr,
 * records when each present returns. */
#if defined(PS5_RETROARCH_RADV)
#include <stdatomic.h>
#include <string.h>
#include <time.h>

#include "gfx/include/vulkan/vulkan.h"
#include "present_clock.h"

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance,
                                                                   const char *name);

static _Atomic uint64_t last_present_ns;
static PFN_vkQueuePresentKHR driver_present;
static PFN_vkGetDeviceProcAddr driver_device_proc;

uint64_t ps5_present_clock_now_ns(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
}

uint64_t ps5_present_clock_last_ns(void)
{
    return atomic_load_explicit(&last_present_ns, memory_order_relaxed);
}

static VKAPI_ATTR VkResult VKAPI_CALL present(VkQueue queue, const VkPresentInfoKHR *info)
{
    const VkResult result = driver_present(queue, info);
    atomic_store_explicit(&last_present_ns, ps5_present_clock_now_ns(), memory_order_relaxed);
    return result;
}

/* The driver's command, or the title's in its place. */
static PFN_vkVoidFunction command(PFN_vkVoidFunction driver, const char *name)
{
    if (driver && strcmp(name, "vkQueuePresentKHR") == 0)
    {
        driver_present = (PFN_vkQueuePresentKHR)driver;
        return (PFN_vkVoidFunction)present;
    }
    return driver;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL device_proc(VkDevice device, const char *name)
{
    return command(driver_device_proc(device, name), name);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,
                                                               const char *name)
{
    PFN_vkVoidFunction driver = vk_icdGetInstanceProcAddr(instance, name);
    if (driver && strcmp(name, "vkGetDeviceProcAddr") == 0)
    {
        driver_device_proc = (PFN_vkGetDeviceProcAddr)driver;
        return (PFN_vkVoidFunction)device_proc;
    }
    return command(driver, name);
}
#endif
