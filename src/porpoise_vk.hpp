/* Porpoise - the screen: Vulkan on RADV, presented through VK_KHR_display.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <mutex>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "libretro.h"
#include "libretro_vulkan.h"

namespace porpoise::vk
{
struct Rect
{
    float x0, y0, x1, y1; /* pixels on the screen */
};

/* Instance, display surface and physical device. Call once at startup. */
bool open_display();
/* The device: through the core's negotiation interface when it has one
 * (Dolphin creates its own device with the features it needs), otherwise ours.
 * Then the swapchain and the presentation pipeline. */
bool open_device(const retro_hw_render_context_negotiation_interface_vulkan *negotiation);
/* The libretro hardware render interface handed to the core. */
retro_hw_render_interface_vulkan *render_interface();

unsigned screen_width();
unsigned screen_height();
double refresh_hz();

/* The core's frame, aspect-fit onto the screen. aspect <= 0 uses w / h. */
void present_core_frame(unsigned width, unsigned height, float aspect, bool sharp);
/* A frame with no core image (startup, messages). */
void present_clear(float r, float g, float b);

/* What the UI renderer needs to live on the current device. */
struct Context
{
    VkInstance instance;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue queue;
    std::uint32_t queue_family;
    VkRenderPass render_pass;
    unsigned slots;
    PFN_vkGetInstanceProcAddr get_instance_proc;
    PFN_vkGetDeviceProcAddr get_device_proc;
    std::mutex *queue_mutex;
};
Context context();
/* Drawn inside every presented frame's render pass, after the core's image. */
void set_overlay(void (*draw)(VkCommandBuffer cmd, unsigned slot, void *user), void *user);
unsigned current_slot();
/* Tear down the device (and its swapchain) so another can take the display. */
void close_device();

void close();
} // namespace porpoise::vk
