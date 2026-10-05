/* Porpoise - the screen: Vulkan on RADV, presented through VK_KHR_display.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What the console requires, learned from Mihawk's PS5 RetroArch (its patches
 * to gfx/common/vulkan_common.c, GPL-3.0-or-later):
 *  - the driver is linked, not loaded: vkGetInstanceProcAddr is RADV's entry,
 *    renamed by src/radv_icd_ps5.c;
 *  - there is no window system: the surface is a VK_KHR_display plane;
 *  - a swapchain may use only the usage bits its surface advertises, which on
 *    this console is colour attachment alone - so the core's frame is drawn as
 *    a textured quad, never blitted or copied into the swapchain.
 *
 * The libretro side follows libretro_vulkan.h: the core renders into its own
 * images, submits its own work under lock_queue(), and hands each finished
 * image over with set_image(), already in SHADER_READ_ONLY_OPTIMAL. One sync
 * slot per swapchain image; a slot's fence retires the core's use of the
 * matching image (wait_sync_index). */
#include "porpoise_paths.hpp"
#include "porpoise_vk.hpp"

/* Vulkan create-info structs are written {VK_STRUCTURE_TYPE_...}: the rest is
 * zero, which is what is meant. */
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>
#include <time.h>

#include "trace.hpp"

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,
                                                                         const char *name);
#ifdef PORPOISE_DESKTOP
/* A window on the desktop (desktop/porpoise_platform.cpp): SDL makes its
 * surface, and the window's size is the swapchain's. */
#include <SDL3/SDL_vulkan.h>
#include "porpoise_platform.hpp"
#else
/* VideoOut (libSceVideoOut, linked for the driver's WSI): V-Sync. */
extern "C" int sceVideoOutGetFlipStatus(int handle, std::uint64_t status[16]);
/* The link wraps sceVideoOutOpen (tools/build-porpoise.sh, --wrap): the
 * driver's WSI opens the display through here, and Porpoise keeps the handle
 * to wait on its vblank (V-Sync). */
extern "C" int __real_sceVideoOutOpen(int user, int bus, int index, const void *parameter);
extern "C" int __wrap_sceVideoOutOpen(int user, int bus, int index, const void *parameter);
extern "C" int sceVideoOutWaitVblank(int handle);
#endif

namespace
{
#include "porpoise_quad_frag.inc"
#include "porpoise_quad_vert.inc"

#define PORPOISE_VK_INSTANCE_FUNCS(X)                                                            \
    X(vkEnumeratePhysicalDevices)                                                                \
    X(vkGetPhysicalDeviceProperties)                                                             \
    X(vkGetPhysicalDeviceQueueFamilyProperties)                                                  \
    X(vkGetPhysicalDeviceDisplayPropertiesKHR)                                                   \
    X(vkGetPhysicalDeviceDisplayPlanePropertiesKHR)                                              \
    X(vkGetDisplayPlaneSupportedDisplaysKHR)                                                     \
    X(vkGetDisplayModePropertiesKHR)                                                             \
    X(vkCreateDisplayPlaneSurfaceKHR)                                                            \
    X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)                                                 \
    X(vkGetPhysicalDeviceSurfaceFormatsKHR)                                                      \
    X(vkGetPhysicalDeviceSurfaceSupportKHR)                                                      \
    X(vkCreateDevice)                                                                            \
    X(vkGetDeviceProcAddr)                                                                       \
    X(vkDestroySurfaceKHR)                                                                       \
    X(vkGetPhysicalDeviceMemoryProperties)                                                       \
    X(vkDestroyInstance)

#define PORPOISE_VK_DEVICE_FUNCS(X)                                                              \
    X(vkGetDeviceQueue)                                                                          \
    X(vkCreateSwapchainKHR)                                                                      \
    X(vkGetSwapchainImagesKHR)                                                                   \
    X(vkAcquireNextImageKHR)                                                                     \
    X(vkQueuePresentKHR)                                                                         \
    X(vkCreateImageView)                                                                         \
    X(vkCreateRenderPass)                                                                        \
    X(vkCreateFramebuffer)                                                                       \
    X(vkCreateShaderModule)                                                                      \
    X(vkDestroyShaderModule)                                                                     \
    X(vkCreatePipelineLayout)                                                                    \
    X(vkCreateGraphicsPipelines)                                                                 \
    X(vkCreateDescriptorSetLayout)                                                               \
    X(vkCreateDescriptorPool)                                                                    \
    X(vkAllocateDescriptorSets)                                                                  \
    X(vkUpdateDescriptorSets)                                                                    \
    X(vkCreateSampler)                                                                           \
    X(vkCreateCommandPool)                                                                       \
    X(vkAllocateCommandBuffers)                                                                  \
    X(vkBeginCommandBuffer)                                                                      \
    X(vkEndCommandBuffer)                                                                        \
    X(vkCmdBeginRenderPass)                                                                      \
    X(vkCmdEndRenderPass)                                                                        \
    X(vkCmdBindPipeline)                                                                         \
    X(vkCmdBindDescriptorSets)                                                                   \
    X(vkCmdPushConstants)                                                                        \
    X(vkCmdSetViewport)                                                                          \
    X(vkCmdSetScissor)                                                                           \
    X(vkCmdDraw)                                                                                 \
    X(vkCreateFence)                                                                             \
    X(vkWaitForFences)                                                                           \
    X(vkResetFences)                                                                             \
    X(vkCreateSemaphore)                                                                         \
    X(vkQueueSubmit)                                                                             \
    X(vkDeviceWaitIdle)                                                                          \
    X(vkDestroyDevice)                                                                           \
    X(vkDestroySwapchainKHR)                                                                     \
    X(vkDestroyFramebuffer)                                                                      \
    X(vkDestroyImageView)                                                                        \
    X(vkDestroyRenderPass)                                                                       \
    X(vkDestroyPipeline)                                                                         \
    X(vkDestroyPipelineLayout)                                                                   \
    X(vkDestroyDescriptorSetLayout)                                                              \
    X(vkDestroyDescriptorPool)                                                                   \
    X(vkDestroySampler)                                                                          \
    X(vkDestroyCommandPool)                                                                      \
    X(vkDestroyFence)                                                                            \
    X(vkDestroySemaphore)                                                                        \
    X(vkCreateBuffer)                                                                            \
    X(vkDestroyBuffer)                                                                           \
    X(vkGetBufferMemoryRequirements)                                                             \
    X(vkAllocateMemory)                                                                          \
    X(vkFreeMemory)                                                                              \
    X(vkBindBufferMemory)                                                                        \
    X(vkMapMemory)                                                                               \
    X(vkUnmapMemory)                                                                             \
    X(vkCmdPipelineBarrier)                                                                      \
    X(vkCmdCopyImageToBuffer)                                                                    \
    X(vkFreeCommandBuffers)

#define DECLARE(name) PFN_##name name = nullptr;
PORPOISE_VK_INSTANCE_FUNCS(DECLARE)
PORPOISE_VK_DEVICE_FUNCS(DECLARE)
#undef DECLARE

constexpr unsigned max_slots = 3;

struct Push
{
    float rect[4];
    float uv[4];
    float color[4];
    float params[4]; /* filter, strength 0..1, time, - (shaders/quad.frag) */
    float size[4];   /* picture texels w, h; on screen w, h */
};

struct State
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkExtent2D extent{1920, 1080};
    double refresh_hz = 60.0;
    long long last_wait_ns = 0; /* the last present's wait for a free image */
    float picture[4] = {0, 0, 1920, 1080}; /* the last game picture's place, design space */

    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family = 0;
    std::mutex queue_mutex;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    std::uint32_t image_count = 0;
    VkImage images[8] = {};
    VkImageView views[8] = {};
    VkFramebuffer framebuffers[8] = {};
    VkSemaphore render_done[8] = {};

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkSampler sampler_linear = VK_NULL_HANDLE;
    VkSampler sampler_nearest = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

    unsigned slots = 2;
    unsigned index = 0;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer commands[max_slots] = {};
    VkFence fences[max_slots] = {};
    VkSemaphore acquired[max_slots] = {};
    VkDescriptorSet sets[max_slots] = {};

    /* What the core handed over for the frame being built. */
    retro_vulkan_image core_image{};
    bool have_core_image = false;
    VkSemaphore core_waits[4] = {};
    std::uint32_t core_wait_count = 0;
    VkCommandBuffer core_commands[8] = {};
    std::uint32_t core_command_count = 0;
    VkSemaphore core_signal = VK_NULL_HANDLE;

    retro_hw_render_interface_vulkan iface{};
    bool device_is_ours = false;
    void (*overlay)(VkCommandBuffer, unsigned, void *) = nullptr;
    void (*prepass)(VkCommandBuffer, void *) = nullptr; /* before the render pass: texture copies */
    void *prepass_user = nullptr;
    void *overlay_user = nullptr;
};

State s;

#define CHECK(call, what)                                                                       \
    do                                                                                           \
    {                                                                                            \
        const VkResult result_ = (call);                                                         \
        if (result_ != VK_SUCCESS)                                                               \
        {                                                                                        \
            ps5::debug::mark_value("vk: " what " failed", result_);                              \
            return false;                                                                        \
        }                                                                                        \
    } while (0)

/* ---- libretro_vulkan.h callbacks ------------------------------------------------------- */

void cb_set_image(void *, const retro_vulkan_image *image, std::uint32_t num_semaphores,
                  const VkSemaphore *semaphores, std::uint32_t)
{
    if (image)
    {
        s.core_image = *image;
        s.have_core_image = true;
    }
    s.core_wait_count = std::min<std::uint32_t>(num_semaphores, 4);
    for (std::uint32_t i = 0; i < s.core_wait_count; ++i)
        s.core_waits[i] = semaphores[i];
}

std::uint32_t cb_get_sync_index(void *)
{
    return s.index;
}

std::uint32_t cb_get_sync_index_mask(void *)
{
    return (1u << s.slots) - 1u;
}

void cb_set_command_buffers(void *, std::uint32_t count, const VkCommandBuffer *commands)
{
    s.core_command_count = std::min<std::uint32_t>(count, 8);
    for (std::uint32_t i = 0; i < s.core_command_count; ++i)
        s.core_commands[i] = commands[i];
}

void cb_wait_sync_index(void *)
{
    (void)vkWaitForFences(s.device, 1, &s.fences[s.index], VK_TRUE, UINT64_MAX);
}

void cb_lock_queue(void *)
{
    s.queue_mutex.lock();
}

void cb_unlock_queue(void *)
{
    s.queue_mutex.unlock();
}

void cb_set_signal_semaphore(void *, VkSemaphore semaphore)
{
    s.core_signal = semaphore;
}

/* ---- setup ----------------------------------------------------------------------------- */

template <typename F> bool load_instance(F &fn, const char *name)
{
    fn = reinterpret_cast<F>(vkGetInstanceProcAddr(s.instance, name));
    if (!fn)
        ps5::debug::mark(name);
    return fn != nullptr;
}

template <typename F> bool load_device(F &fn, const char *name)
{
    fn = reinterpret_cast<F>(vkGetDeviceProcAddr(s.device, name));
    if (!fn)
        ps5::debug::mark(name);
    return fn != nullptr;
}

#ifdef PORPOISE_DESKTOP
bool pick_display()
{
    SDL_Window *window = porpoise::platform::window();
    if (!window || !SDL_Vulkan_CreateSurface(window, s.instance, nullptr, &s.surface))
    {
        ps5::debug::mark((std::string("vk: no window surface: ") + SDL_GetError()).c_str());
        return false;
    }
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    s.extent = {std::uint32_t(std::max(w, 1)), std::uint32_t(std::max(h, 1))};
    s.refresh_hz = porpoise::platform::display_hz();
    char line[128];
    std::snprintf(line, sizeof line, "vk: window %ux%u, display at %.3f Hz", s.extent.width, s.extent.height,
                  s.refresh_hz);
    ps5::debug::mark(line);
    return true;
}
#else
bool pick_display()
{
    std::uint32_t count = 0;
    CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(s.gpu, &count, nullptr), "display count");
    if (count == 0)
    {
        ps5::debug::mark("vk: no display");
        return false;
    }
    VkDisplayPropertiesKHR displays[4];
    count = std::min<std::uint32_t>(count, 4);
    CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(s.gpu, &count, displays), "displays");
    const VkDisplayKHR display = displays[0].display;

    std::uint32_t mode_count = 0;
    CHECK(vkGetDisplayModePropertiesKHR(s.gpu, display, &mode_count, nullptr), "mode count");
    VkDisplayModePropertiesKHR modes[64];
    mode_count = std::min<std::uint32_t>(mode_count, 64);
    CHECK(vkGetDisplayModePropertiesKHR(s.gpu, display, &mode_count, modes), "modes");
    if (mode_count == 0)
        return false;

    /* The size Settings > Video > Output resolution asks for (1080p unless
     * the player chose more: /app0/porpoise/output.txt, written when it
     * changes, read here before the settings themselves can be) at the
     * refresh closest to 60 Hz: the emulator is paced by the display, one
     * frame per vblank, so a 120 Hz mode would run games at double speed.
     * Otherwise the largest mode, again closest to 60 Hz. (PS5 Mesa before
     * dc82d01 offered only 3840x2160.) */
    unsigned wanted = 1080;
    if (std::FILE *f = std::fopen(PORPOISE_APP "/porpoise/output.txt", "r"))
    {
        unsigned h = 0;
        if (std::fscanf(f, "%u", &h) == 1 && (h == 1080 || h == 1440 || h == 2160))
            wanted = h;
        std::fclose(f);
    }
    int best = -1;
    auto better = [&](const VkDisplayModePropertiesKHR &m, const VkDisplayModePropertiesKHR &b) {
        const auto &a = m.parameters, &c = b.parameters;
        const bool m_want = a.visibleRegion.height == wanted;
        const bool b_want = c.visibleRegion.height == wanted;
        if (m_want != b_want)
            return m_want;
        const std::uint64_t area_m = std::uint64_t(a.visibleRegion.width) * a.visibleRegion.height;
        const std::uint64_t area_b = std::uint64_t(c.visibleRegion.width) * c.visibleRegion.height;
        if (!m_want && area_m != area_b)
            return area_m > area_b;
        return std::abs(int(a.refreshRate) - 60000) < std::abs(int(c.refreshRate) - 60000);
    };
    for (std::uint32_t i = 0; i < mode_count; ++i)
        if (best < 0 || better(modes[i], modes[best]))
            best = int(i);
    const VkDisplayModePropertiesKHR &mode = modes[best];
    s.extent = mode.parameters.visibleRegion;
    s.refresh_hz = mode.parameters.refreshRate / 1000.0;
    {
        char line[128];
        std::snprintf(line, sizeof line, "vk: display mode %ux%u @ %.3f Hz (%u modes)",
                      s.extent.width, s.extent.height, s.refresh_hz, mode_count);
        ps5::debug::mark(line);
    }

    /* A plane that can show this display. */
    std::uint32_t plane_count = 0;
    CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(s.gpu, &plane_count, nullptr), "planes");
    std::uint32_t plane = UINT32_MAX;
    for (std::uint32_t i = 0; i < plane_count && plane == UINT32_MAX; ++i)
    {
        std::uint32_t supported = 0;
        if (vkGetDisplayPlaneSupportedDisplaysKHR(s.gpu, i, &supported, nullptr) != VK_SUCCESS)
            continue;
        VkDisplayKHR list[8];
        supported = std::min<std::uint32_t>(supported, 8);
        if (vkGetDisplayPlaneSupportedDisplaysKHR(s.gpu, i, &supported, list) != VK_SUCCESS)
            continue;
        for (std::uint32_t j = 0; j < supported; ++j)
            if (list[j] == display)
                plane = i;
    }
    if (plane == UINT32_MAX)
        plane = 0;

    VkDisplaySurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR};
    info.displayMode = mode.displayMode;
    info.planeIndex = plane;
    info.planeStackIndex = 0;
    info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    info.globalAlpha = 1.0f;
    info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    info.imageExtent = s.extent;
    CHECK(vkCreateDisplayPlaneSurfaceKHR(s.instance, &info, nullptr, &s.surface), "display surface");
    return true;
}
#endif

bool own_device()
{
    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(s.gpu, &family_count, nullptr);
    VkQueueFamilyProperties families[16];
    family_count = std::min<std::uint32_t>(family_count, 16);
    vkGetPhysicalDeviceQueueFamilyProperties(s.gpu, &family_count, families);
    s.queue_family = 0;
    for (std::uint32_t i = 0; i < family_count; ++i)
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            s.queue_family = i;
            break;
        }
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = s.queue_family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = extensions;
    CHECK(vkCreateDevice(s.gpu, &info, nullptr, &s.device), "own device");
    return true;
}

bool create_swapchain()
{
    VkSurfaceCapabilitiesKHR caps{};
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s.gpu, s.surface, &caps), "surface caps");
    std::uint32_t format_count = 0;
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(s.gpu, s.surface, &format_count, nullptr), "formats");
    VkSurfaceFormatKHR formats[32];
    format_count = std::min<std::uint32_t>(format_count, 32);
    CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(s.gpu, s.surface, &format_count, formats), "formats");
    VkSurfaceFormatKHR chosen = formats[0];
    for (std::uint32_t i = 0; i < format_count; ++i)
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || formats[i].format == VK_FORMAT_R8G8B8A8_UNORM)
        {
            chosen = formats[i];
            break;
        }
    s.format = chosen.format;
    if (caps.currentExtent.width != UINT32_MAX)
        s.extent = caps.currentExtent;

    std::uint32_t wanted = std::max(2u, caps.minImageCount);
    if (caps.maxImageCount)
        wanted = std::min(wanted, caps.maxImageCount);

    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = s.surface;
    info.minImageCount = wanted;
    info.imageFormat = chosen.format;
    info.imageColorSpace = chosen.colorSpace;
    info.imageExtent = s.extent;
    info.imageArrayLayers = 1;
    /* Only what the surface advertises: colour attachment, on this console. */
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT & caps.supportedUsageFlags;
    if (!info.imageUsage)
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    if (!(caps.supportedTransforms & info.preTransform))
        info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & info.compositeAlpha))
        info.compositeAlpha = VkCompositeAlphaFlagBitsKHR(caps.supportedCompositeAlpha &
                                                          -caps.supportedCompositeAlpha);
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR; /* vsync: one emulated frame per vblank */
    info.clipped = VK_TRUE;
    CHECK(vkCreateSwapchainKHR(s.device, &info, nullptr, &s.swapchain), "swapchain");

    s.image_count = 0;
    CHECK(vkGetSwapchainImagesKHR(s.device, s.swapchain, &s.image_count, nullptr), "image count");
    s.image_count = std::min<std::uint32_t>(s.image_count, 8);
    CHECK(vkGetSwapchainImagesKHR(s.device, s.swapchain, &s.image_count, s.images), "images");
    s.slots = std::clamp<unsigned>(s.image_count, 2, max_slots);
    {
        char line[128];
        std::snprintf(line, sizeof line, "vk: swapchain %ux%u format %d, %u images, %u sync slots",
                      s.extent.width, s.extent.height, int(s.format), s.image_count, s.slots);
        ps5::debug::mark(line);
    }
    return true;
}

bool create_pipeline()
{
    VkAttachmentDescription colour{};
    colour.format = s.format;
    colour.samples = VK_SAMPLE_COUNT_1_BIT;
    colour.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colour.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colour.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colour.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colour.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colour.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = 1;
    rp.pAttachments = &colour;
    rp.subpassCount = 1;
    rp.pSubpasses = &subpass;
    rp.dependencyCount = 1;
    rp.pDependencies = &dependency;
    CHECK(vkCreateRenderPass(s.device, &rp, nullptr, &s.render_pass), "render pass");

    for (std::uint32_t i = 0; i < s.image_count; ++i)
    {
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = s.images[i];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = s.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        CHECK(vkCreateImageView(s.device, &view, nullptr, &s.views[i]), "swapchain view");
        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb.renderPass = s.render_pass;
        fb.attachmentCount = 1;
        fb.pAttachments = &s.views[i];
        fb.width = s.extent.width;
        fb.height = s.extent.height;
        fb.layers = 1;
        CHECK(vkCreateFramebuffer(s.device, &fb, nullptr, &s.framebuffers[i]), "framebuffer");
        VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        CHECK(vkCreateSemaphore(s.device, &sem, nullptr, &s.render_done[i]), "semaphore");
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = 1;
    dsl.pBindings = &binding;
    CHECK(vkCreateDescriptorSetLayout(s.device, &dsl, nullptr, &s.set_layout), "set layout");

    VkPushConstantRange range{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Push)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &s.set_layout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &range;
    CHECK(vkCreatePipelineLayout(s.device, &pl, nullptr, &s.pipeline_layout), "pipeline layout");

    VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm.codeSize = sizeof(porpoise_quad_vert);
    sm.pCode = porpoise_quad_vert;
    CHECK(vkCreateShaderModule(s.device, &sm, nullptr, &vert), "vertex shader");
    sm.codeSize = sizeof(porpoise_quad_frag);
    sm.pCode = porpoise_quad_frag;
    CHECK(vkCreateShaderModule(s.device, &sm, nullptr, &frag), "fragment shader");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE; /* straight alpha, for the UI layers drawn later */
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;
    const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamic_states;
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = s.pipeline_layout;
    gp.renderPass = s.render_pass;
    CHECK(vkCreateGraphicsPipelines(s.device, VK_NULL_HANDLE, 1, &gp, nullptr, &s.pipeline), "pipeline");
    vkDestroyShaderModule(s.device, vert, nullptr);
    vkDestroyShaderModule(s.device, frag, nullptr);

    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW =
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.maxLod = 0.0f;
    CHECK(vkCreateSampler(s.device, &sampler, nullptr, &s.sampler_linear), "linear sampler");
    sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
    CHECK(vkCreateSampler(s.device, &sampler, nullptr, &s.sampler_nearest), "nearest sampler");

    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_slots};
    VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dp.maxSets = max_slots;
    dp.poolSizeCount = 1;
    dp.pPoolSizes = &size;
    CHECK(vkCreateDescriptorPool(s.device, &dp, nullptr, &s.descriptor_pool), "descriptor pool");
    VkDescriptorSetLayout layouts[max_slots] = {s.set_layout, s.set_layout, s.set_layout};
    VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ds.descriptorPool = s.descriptor_pool;
    ds.descriptorSetCount = s.slots;
    ds.pSetLayouts = layouts;
    CHECK(vkAllocateDescriptorSets(s.device, &ds, s.sets), "descriptor sets");

    VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cp.queueFamilyIndex = s.queue_family;
    CHECK(vkCreateCommandPool(s.device, &cp, nullptr, &s.command_pool), "command pool");
    VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ca.commandPool = s.command_pool;
    ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ca.commandBufferCount = s.slots;
    CHECK(vkAllocateCommandBuffers(s.device, &ca, s.commands), "command buffers");
    for (unsigned i = 0; i < s.slots; ++i)
    {
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        CHECK(vkCreateFence(s.device, &fence, nullptr, &s.fences[i]), "fence");
        VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        CHECK(vkCreateSemaphore(s.device, &sem, nullptr, &s.acquired[i]), "semaphore");
    }
    return true;
}

/* Frames come from retro_run's thread (Dolphin runs its GPU loop there), but a
 * lock keeps two presents from interleaving if that ever changes. */
std::mutex present_mutex;

#ifdef PORPOISE_DESKTOP
/* The window changed size (or the swapchain went out of date): a new
 * swapchain the window's size, its views and framebuffers. The render pass,
 * pipeline and the per-slot sync stay; the slot count the core was told stays
 * too. */
bool g_swapchain_stale = false;

bool recreate_swapchain()
{
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(porpoise::platform::window(), &w, &h);
    if (w <= 0 || h <= 0)
        return false; /* minimised: nothing to show */
    {
        std::lock_guard<std::mutex> queue_lock(s.queue_mutex);
        (void)vkDeviceWaitIdle(s.device);
    }
    for (std::uint32_t i = 0; i < s.image_count; ++i)
    {
        if (s.framebuffers[i])
            vkDestroyFramebuffer(s.device, s.framebuffers[i], nullptr);
        if (s.views[i])
            vkDestroyImageView(s.device, s.views[i], nullptr);
        if (s.render_done[i])
            vkDestroySemaphore(s.device, s.render_done[i], nullptr);
        s.framebuffers[i] = VK_NULL_HANDLE;
        s.views[i] = VK_NULL_HANDLE;
        s.render_done[i] = VK_NULL_HANDLE;
    }
    vkDestroySwapchainKHR(s.device, s.swapchain, nullptr);
    s.swapchain = VK_NULL_HANDLE;
    s.extent = {std::uint32_t(w), std::uint32_t(h)};
    const unsigned slots = s.slots;
    if (!create_swapchain())
        return false;
    s.slots = slots;
    for (std::uint32_t i = 0; i < s.image_count; ++i)
    {
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = s.images[i];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = s.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        CHECK(vkCreateImageView(s.device, &view, nullptr, &s.views[i]), "swapchain view");
        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb.renderPass = s.render_pass;
        fb.attachmentCount = 1;
        fb.pAttachments = &s.views[i];
        fb.width = s.extent.width;
        fb.height = s.extent.height;
        fb.layers = 1;
        CHECK(vkCreateFramebuffer(s.device, &fb, nullptr, &s.framebuffers[i]), "framebuffer");
        VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        CHECK(vkCreateSemaphore(s.device, &sem, nullptr, &s.render_done[i]), "semaphore");
    }
    return true;
}
#endif

/* One presented frame: optionally the core's image as a quad, over a clear. */
void present(const float clear[3], const Push *quad, VkImageView view, bool sharp)
{
    std::lock_guard<std::mutex> present_lock(present_mutex);
    const unsigned slot = s.index;
    /* How long the display kept this present waiting: the frame pacer's sign
     * that the swapchain is holding the loop to the vblank. */
    (void)vkWaitForFences(s.device, 1, &s.fences[slot], VK_TRUE, UINT64_MAX);
#ifdef PORPOISE_DESKTOP
    if (porpoise::platform::take_resized() || g_swapchain_stale)
        g_swapchain_stale = !recreate_swapchain();
    if (g_swapchain_stale)
        return;
#endif

    /* Only the acquire: the fence above waits for the GPU, the acquire for
     * the display to flip an image away. */
    timespec wait_start{}, wait_end{};
    clock_gettime(CLOCK_MONOTONIC, &wait_start);
    std::uint32_t image = 0;
    const VkResult acquired = vkAcquireNextImageKHR(s.device, s.swapchain, UINT64_MAX,
                                                    s.acquired[slot], VK_NULL_HANDLE, &image);
    clock_gettime(CLOCK_MONOTONIC, &wait_end);
    s.last_wait_ns = (wait_end.tv_sec - wait_start.tv_sec) * 1000000000LL + (wait_end.tv_nsec - wait_start.tv_nsec);
#ifdef PORPOISE_DESKTOP
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR || acquired == VK_SUBOPTIMAL_KHR)
        g_swapchain_stale = true;
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR)
        return;
#endif
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
    {
        static bool reported = false;
        if (!reported)
            ps5::debug::mark_value("vk: acquire failed", acquired);
        reported = true;
        return;
    }
    (void)vkResetFences(s.device, 1, &s.fences[slot]);

    if (quad && view)
    {
        VkDescriptorImageInfo info{};
        info.sampler = sharp ? s.sampler_nearest : s.sampler_linear;
        info.imageView = view;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = s.sets[slot];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &info;
        vkUpdateDescriptorSets(s.device, 1, &write, 0, nullptr);
    }

    VkCommandBuffer cmd = s.commands[slot];
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    (void)vkBeginCommandBuffer(cmd, &begin);
    if (s.prepass)
        s.prepass(cmd, s.prepass_user);
    VkClearValue clear_value{};
    clear_value.color.float32[0] = clear[0];
    clear_value.color.float32[1] = clear[1];
    clear_value.color.float32[2] = clear[2];
    clear_value.color.float32[3] = 1.0f;
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = s.render_pass;
    rp.framebuffer = s.framebuffers[image];
    rp.renderArea.extent = s.extent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear_value;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    if (quad && view)
    {
        VkViewport viewport{0, 0, float(s.extent.width), float(s.extent.height), 0, 1};
        VkRect2D scissor{{0, 0}, s.extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s.pipeline_layout, 0, 1,
                                &s.sets[slot], 0, nullptr);
        vkCmdPushConstants(cmd, s.pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(Push), quad);
        vkCmdDraw(cmd, 4, 1, 0, 0);
    }
    if (s.overlay)
        s.overlay(cmd, slot, s.overlay_user);
    vkCmdEndRenderPass(cmd);
    (void)vkEndCommandBuffer(cmd);

    VkSemaphore waits[5];
    VkPipelineStageFlags stages[5];
    std::uint32_t wait_count = 0;
    waits[wait_count] = s.acquired[slot];
    stages[wait_count++] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    for (std::uint32_t i = 0; i < s.core_wait_count && wait_count < 5; ++i)
    {
        waits[wait_count] = s.core_waits[i];
        stages[wait_count++] = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    s.core_wait_count = 0;

    VkCommandBuffer buffers[9];
    std::uint32_t buffer_count = 0;
    for (std::uint32_t i = 0; i < s.core_command_count; ++i)
        buffers[buffer_count++] = s.core_commands[i];
    s.core_command_count = 0;
    buffers[buffer_count++] = cmd;

    VkSemaphore signals[2] = {s.render_done[image], VK_NULL_HANDLE};
    std::uint32_t signal_count = 1;
    if (s.core_signal)
    {
        signals[signal_count++] = s.core_signal;
        s.core_signal = VK_NULL_HANDLE;
    }

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = wait_count;
    submit.pWaitSemaphores = waits;
    submit.pWaitDstStageMask = stages;
    submit.commandBufferCount = buffer_count;
    submit.pCommandBuffers = buffers;
    submit.signalSemaphoreCount = signal_count;
    submit.pSignalSemaphores = signals;

    VkPresentInfoKHR present_info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &s.render_done[image];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &s.swapchain;
    present_info.pImageIndices = &image;
    {
        std::lock_guard<std::mutex> lock(s.queue_mutex);
        const VkResult submitted = vkQueueSubmit(s.queue, 1, &submit, s.fences[slot]);
        if (submitted != VK_SUCCESS)
        {
            static bool reported = false;
            if (!reported)
                ps5::debug::mark_value("vk: submit failed", submitted);
            reported = true;
        }
        (void)vkQueuePresentKHR(s.queue, &present_info);
    }
    s.index = (s.index + 1) % s.slots;
}
} // namespace

namespace porpoise::vk
{
bool open_display()
{
    PFN_vkCreateInstance create_instance =
        reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!create_instance)
    {
        ps5::debug::mark("vk: no vkCreateInstance from the driver");
        return false;
    }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Porpoise";
    app.applicationVersion = 1;
    app.pEngineName = "Porpoise";
    app.apiVersion = VK_API_VERSION_1_1;
#ifdef PORPOISE_DESKTOP
    /* The window system's own surface extensions, as SDL names them. */
    Uint32 extension_count = 0;
    const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extension_count);
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = extension_count;
    info.ppEnabledExtensionNames = extensions;
#else
    const char *extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME};
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = 2;
    info.ppEnabledExtensionNames = extensions;
#endif
    CHECK(create_instance(&info, nullptr, &s.instance), "instance");

    bool ok = true;
#ifdef PORPOISE_DESKTOP
    /* No display extension on a desktop: those are left out. */
#define LOAD(name)                                                                                           \
    {                                                                                                        \
        const bool got_ = load_instance(name, #name);                                                         \
        ok &= got_ || std::strstr(#name, "Display") != nullptr;                                               \
    }
#else
#define LOAD(name) ok &= load_instance(name, #name);
#endif
    PORPOISE_VK_INSTANCE_FUNCS(LOAD)
#undef LOAD
    if (!ok)
        return false;

#ifdef PORPOISE_DESKTOP
    /* A desktop may have several GPUs (and a software one): the discrete
     * one first, then integrated, then anything. */
    std::uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(s.instance, &count, nullptr);
    VkPhysicalDevice gpus[16];
    count = std::min<std::uint32_t>(count, 16);
    if (count)
        result = vkEnumeratePhysicalDevices(s.instance, &count, gpus);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || count == 0)
    {
        ps5::debug::mark_value("vk: no physical device", result);
        return false;
    }
    auto rank = [](VkPhysicalDeviceType type) {
        switch (type)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 0;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 1;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 2;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return 4;
        default: return 3;
        }
    };
    int best_rank = 99;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(gpus[i], &p);
        if (rank(p.deviceType) < best_rank)
        {
            best_rank = rank(p.deviceType);
            s.gpu = gpus[i];
        }
    }
#else
    std::uint32_t count = 1;
    VkResult result = vkEnumeratePhysicalDevices(s.instance, &count, &s.gpu);
    if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || count == 0)
    {
        ps5::debug::mark_value("vk: no physical device", result);
        return false;
    }
#endif
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(s.gpu, &props);
    ps5::debug::mark(props.deviceName);
    return pick_display();
}

/* The GPU's memory types, once, into the trace: whether the CPU has cached
 * memory to read the GPU's results from decides how fast Dolphin's EFB copies
 * to RAM (and save-state pictures) are. */
void log_memory_types()
{
    static bool done = false;
    if (done || !s.gpu)
        return;
    done = true;
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(s.gpu, &props);
    for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i)
    {
        const VkMemoryPropertyFlags f = props.memoryTypes[i].propertyFlags;
        char line[160];
        std::snprintf(line, sizeof line, "vk: memory type %u heap %u:%s%s%s%s", i, props.memoryTypes[i].heapIndex,
                      (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? " device-local" : "",
                      (f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ? " host-visible" : "",
                      (f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? " coherent" : "",
                      (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? " CACHED" : "");
        ps5::debug::mark(line);
    }
}

bool open_device(const retro_hw_render_context_negotiation_interface_vulkan *negotiation)
{
    log_memory_types();
    if (negotiation && negotiation->interface_type == RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN &&
        negotiation->create_device)
    {
        retro_vulkan_context context{};
        const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        if (!negotiation->create_device(&context, s.instance, s.gpu, s.surface, vkGetInstanceProcAddr,
                                        extensions, 1, nullptr, 0, nullptr))
        {
            ps5::debug::mark("vk: the core's create_device failed");
            return false;
        }
        s.gpu = context.gpu;
        s.device = context.device;
        s.queue = context.queue;
        s.queue_family = context.queue_family_index;
        s.device_is_ours = false;
        ps5::debug::mark_value("vk: device from the core, queue family", s.queue_family);
    }
    else
    {
        if (!own_device())
            return false;
        s.device_is_ours = true;
        ps5::debug::mark("vk: device created by Porpoise");
    }

    bool ok = true;
#define LOAD(name) ok &= load_device(name, #name);
    PORPOISE_VK_DEVICE_FUNCS(LOAD)
#undef LOAD
    if (!ok)
        return false;
    if (!s.queue)
        vkGetDeviceQueue(s.device, s.queue_family, 0, &s.queue);

    VkBool32 supported = VK_FALSE;
    (void)vkGetPhysicalDeviceSurfaceSupportKHR(s.gpu, s.queue_family, s.surface, &supported);
    ps5::debug::mark_value("vk: queue can present", supported);

    if (!create_swapchain() || !create_pipeline())
        return false;

    retro_hw_render_interface_vulkan &i = s.iface;
    i.interface_type = RETRO_HW_RENDER_INTERFACE_VULKAN;
    i.interface_version = RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION;
    i.handle = &s;
    i.instance = s.instance;
    i.gpu = s.gpu;
    i.device = s.device;
    i.get_device_proc_addr = vkGetDeviceProcAddr;
    i.get_instance_proc_addr = vkGetInstanceProcAddr;
    i.queue = s.queue;
    i.queue_index = s.queue_family;
    i.set_image = cb_set_image;
    i.get_sync_index = cb_get_sync_index;
    i.get_sync_index_mask = cb_get_sync_index_mask;
    i.set_command_buffers = cb_set_command_buffers;
    i.wait_sync_index = cb_wait_sync_index;
    i.lock_queue = cb_lock_queue;
    i.unlock_queue = cb_unlock_queue;
    i.set_signal_semaphore = cb_set_signal_semaphore;
    return true;
}

retro_hw_render_interface_vulkan *render_interface()
{
    return s.device ? &s.iface : nullptr;
}

unsigned screen_width()
{
    return s.extent.width;
}

unsigned screen_height()
{
    return s.extent.height;
}

bool capture_picture(unsigned width, unsigned height, std::vector<std::uint8_t> &rgba)
{
    /* A copy of the game's last picture, for a save state's thumbnail. The
     * game is paused when this runs, so its image holds still. */
    if (!s.device || !s.have_core_image || width == 0 || height == 0)
        return false;
    const VkImageViewCreateInfo &info = s.core_image.create_info;
    const VkFormat format = info.format;
    const bool bgra = format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
    const bool rgba8 = format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB;
    if (!bgra && !rgba8)
    {
        ps5::debug::mark_value("vk: capture: picture format not 8-bit RGBA", int(format));
        return false;
    }
    const VkDeviceSize bytes = VkDeviceSize(width) * height * 4;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    bool ok = false;
    std::lock_guard<std::mutex> lock(s.queue_mutex);
    (void)vkDeviceWaitIdle(s.device);
    do
    {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = bytes;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(s.device, &bi, nullptr, &buffer) != VK_SUCCESS)
            break;
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(s.device, buffer, &req);
        VkPhysicalDeviceMemoryProperties props{};
        vkGetPhysicalDeviceMemoryProperties(s.gpu, &props);
        /* Memory the CPU reads quickly (cached), else any it can read. */
        std::uint32_t type = UINT32_MAX;
        const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (const VkMemoryPropertyFlags flags : {want | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, want})
        {
            for (std::uint32_t i = 0; i < props.memoryTypeCount && type == UINT32_MAX; ++i)
                if ((req.memoryTypeBits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
                    type = i;
            if (type != UINT32_MAX)
                break;
        }
        if (type == UINT32_MAX)
            break;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = type;
        if (vkAllocateMemory(s.device, &ai, nullptr, &memory) != VK_SUCCESS ||
            vkBindBufferMemory(s.device, buffer, memory, 0) != VK_SUCCESS)
            break;
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ca.commandPool = s.command_pool;
        ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ca.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(s.device, &ca, &cmd) != VK_SUCCESS)
            break;
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);
        VkImageMemoryBarrier to{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        to.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        to.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        to.oldLayout = s.core_image.image_layout;
        to.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to.srcQueueFamilyIndex = to.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to.image = info.image;
        to.subresourceRange = info.subresourceRange;
        to.subresourceRange.levelCount = 1;
        to.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &to);
        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.mipLevel = info.subresourceRange.baseMipLevel;
        copy.imageSubresource.baseArrayLayer = info.subresourceRange.baseArrayLayer;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(cmd, info.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
        VkImageMemoryBarrier back = to;
        back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        back.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        back.newLayout = s.core_image.image_layout;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0,
                             nullptr, 1, &back);
        vkEndCommandBuffer(cmd);
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (vkCreateFence(s.device, &fi, nullptr, &fence) != VK_SUCCESS)
            break;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        if (vkQueueSubmit(s.queue, 1, &submit, fence) != VK_SUCCESS)
            break;
        (void)vkWaitForFences(s.device, 1, &fence, VK_TRUE, UINT64_MAX);
        void *mapped = nullptr;
        if (vkMapMemory(s.device, memory, 0, bytes, 0, &mapped) != VK_SUCCESS)
            break;
        /* One straight copy out of the mapping (uncached memory reads slowly
         * byte by byte), then the channels put in order in place. */
        rgba.resize(std::size_t(bytes));
        std::memcpy(rgba.data(), mapped, std::size_t(bytes));
        for (std::size_t i = 0; i < std::size_t(width) * height; ++i)
        {
            if (bgra)
                std::swap(rgba[i * 4 + 0], rgba[i * 4 + 2]);
            rgba[i * 4 + 3] = 255;
        }
        vkUnmapMemory(s.device, memory);
        ok = true;
    } while (false);
    if (fence)
        vkDestroyFence(s.device, fence, nullptr);
    if (cmd)
        vkFreeCommandBuffers(s.device, s.command_pool, 1, &cmd);
    if (buffer)
        vkDestroyBuffer(s.device, buffer, nullptr);
    if (memory)
        vkFreeMemory(s.device, memory, nullptr);
    return ok;
}

void picture_rect(float out[4])
{
    for (int i = 0; i < 4; ++i)
        out[i] = s.picture[i];
}

double display_hz()
{
    return s.refresh_hz;
}

double last_present_wait_ms()
{
    return s.last_wait_ns / 1e6;
}

/* V-Sync. The driver's VideoOut output (opened by its swapchain, PS5_Mesa's
 * wsi_common_videoout.c) is the one the TV's vblanks come from; Porpoise
 * finds its handle by asking each small handle for its flip status (an
 * output this process has not opened answers with an error) and waits on its
 * vblank itself. The driver's own acquire never held Porpoise's loop to the
 * vblank on the console (1.5's logs: "held 0 of 120 presents"), so frames
 * went out on Porpoise's own timer instead of the display's. */
namespace
{
int g_videoout = -1;
int g_videoout_tries = 0;
/* The handle the driver's WSI got from sceVideoOutOpen (below). */
std::atomic<int> g_videoout_opened{-1};
} // namespace

#ifdef PORPOISE_DESKTOP
/* A desktop's V-Sync is the swapchain's (FIFO); there's no vblank to wait on
 * besides it. */
void note_videoout(int) {}
bool vblank_ready()
{
    return false;
}
bool wait_vblank()
{
    return false;
}
#else
void note_videoout(int handle)
{
    g_videoout_opened.store(handle);
    g_videoout_tries = 0; /* a new handle: look again */
    g_videoout = -1;
}

bool vblank_ready()
{
    if (g_videoout >= 0)
        return true;
    if (!s.swapchain || g_videoout_tries >= 8)
        return false;
    ++g_videoout_tries;
    /* VideoOut's handles are large numbers (0x4e100100 and up, as PS5_Vulkan
     * recorded), not small ones: the driver's own sceVideoOutOpen call is
     * caught on its way (__wrap_sceVideoOutOpen) and its answer used. */
    const int opened = g_videoout_opened.load();
    for (int handle : {opened})
    {
        if (handle < 0)
            continue;
        std::uint64_t status[16] = {};
        if (sceVideoOutGetFlipStatus(handle, status) == 0)
        {
            g_videoout = handle;
            char line[256];
            std::snprintf(line, sizeof line,
                          "vk: VideoOut output %d for V-Sync; flip status %llx %llx %llx %llx %llx %llx %llx %llx",
                          handle, (unsigned long long)status[0], (unsigned long long)status[1],
                          (unsigned long long)status[2], (unsigned long long)status[3],
                          (unsigned long long)status[4], (unsigned long long)status[5],
                          (unsigned long long)status[6], (unsigned long long)status[7]);
            ps5::debug::mark(line);
            return true;
        }
    }
    if (g_videoout_tries == 8)
    {
        char line[128];
        std::snprintf(line, sizeof line, "vk: VideoOut output %#x didn't answer; V-Sync uses Porpoise's timer",
                      unsigned(opened));
        ps5::debug::mark(line);
    }
    return false;
}

bool wait_vblank()
{
    return g_videoout >= 0 && sceVideoOutWaitVblank(g_videoout) == 0;
}
#endif

double refresh_hz()
{
    /* Porpoise never runs faster than 60 frames a second, whatever the
     * display mode: GameCube and Wii games are 50/60 Hz, and the launcher has
     * no use for more. */
    return s.refresh_hz > 60.5 ? 60.0 : s.refresh_hz;
}

int g_colour_filter = 0;

void set_game_colour_filter(int mode)
{
    g_colour_filter = mode;
}

void present_core_frame(unsigned width, unsigned height, float aspect, int filter, float strength)
{
    const bool sharp = filter == 1;
    static const float black[3] = {0, 0, 0};
    if (!s.device)
        return;
    if (!s.have_core_image || width == 0 || height == 0)
    {
        present(black, nullptr, VK_NULL_HANDLE, false);
        return;
    }
    if (aspect <= 0.0f)
        aspect = float(width) / float(height);
    /* Aspect-fit into the screen, centred. */
    const float sw = float(s.extent.width), sh = float(s.extent.height);
    float w = sw, h = sw / aspect;
    if (h > sh)
    {
        h = sh;
        w = sh * aspect;
    }
    const float x0 = (sw - w) * 0.5f, y0 = (sh - h) * 0.5f;
    Push quad{};
    quad.rect[0] = x0 / sw * 2.0f - 1.0f;
    quad.rect[1] = y0 / sh * 2.0f - 1.0f;
    quad.rect[2] = (x0 + w) / sw * 2.0f - 1.0f;
    quad.rect[3] = (y0 + h) / sh * 2.0f - 1.0f;
    quad.uv[0] = 0.0f;
    quad.uv[1] = 0.0f;
    quad.uv[2] = 1.0f;
    quad.uv[3] = 1.0f;
    quad.color[0] = quad.color[1] = quad.color[2] = quad.color[3] = 1.0f;
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    quad.params[0] = float(filter);
    quad.params[1] = strength;
    quad.params[2] = float(now.tv_sec % 3600) + float(now.tv_nsec) * 1e-9f;
    quad.params[3] = float(g_colour_filter);
    quad.size[0] = float(width);
    quad.size[1] = float(height);
    quad.size[2] = w;
    quad.size[3] = h;
    /* Where the picture sits, in the launcher's 1920x1080 design space, for
     * what is drawn around it (borders). */
    s.picture[0] = x0 / sw * 1920.0f;
    s.picture[1] = y0 / sh * 1080.0f;
    s.picture[2] = w / sw * 1920.0f;
    s.picture[3] = h / sh * 1080.0f;
    present(black, &quad, s.core_image.image_view, sharp);
}

void present_clear(float r, float g, float b)
{
    if (!s.device)
        return;
    const float colour[3] = {r, g, b};
    present(colour, nullptr, VK_NULL_HANDLE, false);
}

void set_prepass(void (*record)(VkCommandBuffer, void *), void *user)
{
    s.prepass = record;
    s.prepass_user = user;
}

void set_overlay(void (*draw)(VkCommandBuffer, unsigned, void *), void *user)
{
    s.overlay = draw;
    s.overlay_user = user;
}

unsigned current_slot()
{
    return s.index;
}

Context context()
{
    Context c;
    c.instance = s.instance;
    c.gpu = s.gpu;
    c.device = s.device;
    c.queue = s.queue;
    c.queue_family = s.queue_family;
    c.render_pass = s.render_pass;
    c.slots = s.slots;
    c.get_instance_proc = vkGetInstanceProcAddr;
    c.get_device_proc = vkGetDeviceProcAddr;
    c.queue_mutex = &s.queue_mutex;
    return c;
}

/* Everything on the current device goes, and the device too when Porpoise
 * made it; the instance and the display surface stay for the next device. */
void close_device()
{
    if (!s.device)
        return;
    (void)vkDeviceWaitIdle(s.device);
    for (std::uint32_t i = 0; i < s.image_count; ++i)
    {
        if (s.framebuffers[i])
            vkDestroyFramebuffer(s.device, s.framebuffers[i], nullptr);
        if (s.views[i])
            vkDestroyImageView(s.device, s.views[i], nullptr);
        if (s.render_done[i])
            vkDestroySemaphore(s.device, s.render_done[i], nullptr);
        s.framebuffers[i] = VK_NULL_HANDLE;
        s.views[i] = VK_NULL_HANDLE;
        s.render_done[i] = VK_NULL_HANDLE;
    }
    for (unsigned i = 0; i < max_slots; ++i)
    {
        if (s.fences[i])
            vkDestroyFence(s.device, s.fences[i], nullptr);
        if (s.acquired[i])
            vkDestroySemaphore(s.device, s.acquired[i], nullptr);
        s.fences[i] = VK_NULL_HANDLE;
        s.acquired[i] = VK_NULL_HANDLE;
        s.commands[i] = VK_NULL_HANDLE;
        s.sets[i] = VK_NULL_HANDLE;
    }
    if (s.swapchain)
        vkDestroySwapchainKHR(s.device, s.swapchain, nullptr);
    if (s.command_pool)
        vkDestroyCommandPool(s.device, s.command_pool, nullptr);
    if (s.descriptor_pool)
        vkDestroyDescriptorPool(s.device, s.descriptor_pool, nullptr);
    if (s.sampler_linear)
        vkDestroySampler(s.device, s.sampler_linear, nullptr);
    if (s.sampler_nearest)
        vkDestroySampler(s.device, s.sampler_nearest, nullptr);
    if (s.pipeline)
        vkDestroyPipeline(s.device, s.pipeline, nullptr);
    if (s.pipeline_layout)
        vkDestroyPipelineLayout(s.device, s.pipeline_layout, nullptr);
    if (s.set_layout)
        vkDestroyDescriptorSetLayout(s.device, s.set_layout, nullptr);
    if (s.render_pass)
        vkDestroyRenderPass(s.device, s.render_pass, nullptr);
    if (s.device_is_ours)
        vkDestroyDevice(s.device, nullptr);
    s.swapchain = VK_NULL_HANDLE;
    s.command_pool = VK_NULL_HANDLE;
    s.descriptor_pool = VK_NULL_HANDLE;
    s.sampler_linear = s.sampler_nearest = VK_NULL_HANDLE;
    s.pipeline = VK_NULL_HANDLE;
    s.pipeline_layout = VK_NULL_HANDLE;
    s.set_layout = VK_NULL_HANDLE;
    s.render_pass = VK_NULL_HANDLE;
    s.device = VK_NULL_HANDLE;
    s.queue = VK_NULL_HANDLE;
    s.image_count = 0;
    s.index = 0;
    s.have_core_image = false;
    s.core_wait_count = 0;
    s.core_command_count = 0;
    s.core_signal = VK_NULL_HANDLE;
    s.device_is_ours = false;
    ps5::debug::mark("vk: device closed");
}

void close()
{
    if (s.device)
        (void)vkDeviceWaitIdle(s.device);
}
} // namespace porpoise::vk

/* The driver's WSI opening VideoOut: the handle is kept for V-Sync. */
#ifndef PORPOISE_DESKTOP
extern "C" int __wrap_sceVideoOutOpen(int user, int bus, int index, const void *parameter)
{
    const int handle = __real_sceVideoOutOpen(user, bus, index, parameter);
    if (handle >= 0)
        porpoise::vk::note_videoout(handle);
    return handle;
}
#endif
