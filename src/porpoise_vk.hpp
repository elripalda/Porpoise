/* Porpoise - the screen: Vulkan on RADV, presented through VK_KHR_display.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <vector>

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
/* The display mode's own rate (may be above 60). */
double display_hz();
/* How long the last present waited for the display to free an image. */
double last_present_wait_ms();
/* The TV's refresh, waited for directly (VideoOut's vblank), for V-Sync.
 * vblank_ready(): the display's VideoOut output has been found (it opens
 * with the first swapchain). wait_vblank(): blocks until the next vblank;
 * false when it could not. */
bool vblank_ready();
/* The display's VideoOut handle, from the driver's sceVideoOutOpen (V-Sync). */
void note_videoout(int handle);
bool wait_vblank();

/* The core's frame, aspect-fit onto the screen. aspect <= 0 uses w / h. */
/* filter: 0 smooth, 1 sharp, 2 sharpen, 3 CRT, 4 arcade CRT, 5 VHS; strength 0..1. */
void present_core_frame(unsigned width, unsigned height, float aspect, int filter, float strength);
/* Accessibility's colour filter on the game's picture (0 off; shaders/quad.frag). */
void set_game_colour_filter(int mode);
/* Enhancements: saturation and contrast (1 as it is), warmth (0 as it is),
 * bloom 0..1, on the game's picture. */
void set_picture_adjust(float saturation, float contrast, float warmth, float bloom);
/* A copy of the game's last picture (width x height, RGBA), for thumbnails. */
bool capture_picture(unsigned width, unsigned height, std::vector<std::uint8_t> &rgba);
/* Where the last game picture was drawn, as x, y, w, h in 1920x1080 design space. */
void picture_rect(float out[4]);
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
/* Called in each frame's command buffer before its render pass (copies into
 * textures that change every frame). */
void set_prepass(void (*record)(VkCommandBuffer cmd, void *user), void *user);
unsigned current_slot();
/* Tear down the device (and its swapchain) so another can take the display. */
void close_device();

void close();
} // namespace porpoise::vk
