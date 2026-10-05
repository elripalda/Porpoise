/* Porpoise UI - the renderer.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Text is drawn from signed-distance-field glyphs built at startup from the
 * Nunito font (SIL Open Font License) with stb_truetype; images are decoded
 * with stb_image (both public domain / MIT, Sean Barrett). */
#include "ui_gfx.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unordered_map>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wextra"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb/stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb/stb_image.h"
#pragma clang diagnostic pop
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

namespace
{
#include "ui_frag.inc"
#include "ui_vert.inc"

constexpr float kDesignW = 1920.0f;
constexpr float kDesignH = 1080.0f;
constexpr float kBase = 64.0f;  /* SDF glyph base size, px */
constexpr int kPad = 8;         /* SDF padding, px */
constexpr int kAtlas = 2048;

enum Kind
{
    K_IMAGE = 0,
    K_PANEL = 1,
    K_TEXT = 2,
    K_BACKGROUND = 3,
    K_GLYPH = 4,
    K_COVER = 5,
    K_BLOB = 6,
    K_REFLECTION = 7,
    K_GLASS = 8,
};

bool read_file(const std::string &path, std::vector<unsigned char> &out)
{
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0)
    {
        std::fclose(f);
        return false;
    }
    out.resize(std::size_t(n));
    bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

/* Halves an RGBA picture until its longer side fits max_side (a player's own
 * 4K cover): memory stays small. The result is in out. */
void fit_side(const unsigned char *pixels, int &w, int &h, int max_side, std::vector<unsigned char> &out)
{
    std::vector<unsigned char> small;
    const unsigned char *src = pixels;
    while (std::max(w, h) > max_side && w >= 2 && h >= 2)
    {
        const int nw = w / 2, nh = h / 2;
        std::vector<unsigned char> half(std::size_t(nw) * std::size_t(nh) * 4);
        for (int y = 0; y < nh; ++y)
            for (int x = 0; x < nw; ++x)
                for (int c = 0; c < 4; ++c)
                {
                    const std::size_t a = (std::size_t(y * 2) * std::size_t(w) + std::size_t(x * 2)) * 4 + c;
                    const std::size_t b = a + std::size_t(w) * 4;
                    half[(std::size_t(y) * std::size_t(nw) + std::size_t(x)) * 4 + c] =
                        static_cast<unsigned char>((src[a] + src[a + 4] + src[b] + src[b + 4] + 2) / 4);
                }
        small.swap(half);
        src = small.data();
        w = nw;
        h = nh;
    }
    if (src == pixels)
        out.assign(pixels, pixels + std::size_t(w) * std::size_t(h) * 4);
    else
        out.swap(small);
}

/* ---- pictures decoded off the render thread (Gfx::texture_file_async) ----
 *
 * One worker reads and decodes; the newest request goes first, so the covers
 * coming into view while the library scrolls are the ones that arrive. A
 * request nobody has asked for again in a second is dropped, decoded or not. */
struct Load
{
    enum State
    {
        Queued,
        Busy,
        Done,
        Failed,
    } state = Queued;
    int max_side = 1024;
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;
    std::uint64_t wanted = 0; /* the frame it was last asked for */
};

struct Loader
{
    std::mutex lock;
    std::condition_variable wake;
    std::unordered_map<std::string, Load> loads;
    std::vector<std::string> queue; /* the newest last, taken first */
    std::atomic<std::uint64_t> frame{0};
    bool started = false;
};

Loader &loader()
{
    static Loader *l = new Loader(); /* the worker outlives everything: never freed */
    return *l;
}

constexpr std::uint64_t kLoadForgotten = 60; /* frames without a request: dropped */

void *load_work(void *)
{
    Loader &L = loader();
    for (;;)
    {
        std::string path;
        int max_side = 1024;
        {
            std::unique_lock<std::mutex> g(L.lock);
            L.wake.wait(g, [&] { return !L.queue.empty(); });
            path = std::move(L.queue.back());
            L.queue.pop_back();
            auto it = L.loads.find(path);
            if (it == L.loads.end() || it->second.state != Load::Queued)
                continue;
            if (L.frame.load() - it->second.wanted > kLoadForgotten)
            {
                L.loads.erase(it); /* scrolled past before its turn */
                continue;
            }
            it->second.state = Load::Busy;
            max_side = it->second.max_side;
        }
        std::vector<unsigned char> bytes, rgba;
        int w = 0, h = 0, n = 0;
        bool ok = false;
        if (read_file(path, bytes))
            if (unsigned char *pixels = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &n, 4))
            {
                fit_side(pixels, w, h, max_side, rgba);
                stbi_image_free(pixels);
                ok = w > 0 && h > 0;
            }
        std::lock_guard<std::mutex> g(L.lock);
        auto it = L.loads.find(path);
        if (it == L.loads.end())
            continue;
        it->second.state = ok ? Load::Done : Load::Failed;
        it->second.rgba.swap(rgba);
        it->second.w = w;
        it->second.h = h;
    }
    return nullptr;
}

void start_loader_locked(Loader &L)
{
    if (L.started)
        return;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 1u << 20);
    pthread_t t;
    L.started = pthread_create(&t, &attr, load_work, nullptr) == 0;
    pthread_attr_destroy(&attr);
    if (L.started)
        pthread_detach(t);
}

std::uint32_t next_codepoint(const std::string &s, std::size_t &i)
{
    const unsigned char c = static_cast<unsigned char>(s[i++]);
    if (c < 0x80)
        return c;
    int extra = (c >= 0xf0) ? 3 : (c >= 0xe0) ? 2 : (c >= 0xc0) ? 1 : 0;
    std::uint32_t cp = c & (0x3f >> extra);
    for (int k = 0; k < extra && i < s.size(); ++k)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3f);
    return cp;
}
} // namespace

namespace porpoise::ui
{
Color rgba(std::uint32_t hex, float alpha)
{
    return Color{((hex >> 16) & 0xff) / 255.0f, ((hex >> 8) & 0xff) / 255.0f, (hex & 0xff) / 255.0f, alpha};
}

bool Gfx::load_functions()
{
    bool ok = true;
#define LOAD(name)                                                                               \
    name##_ = reinterpret_cast<PFN_##name>(init_.get_device_proc(init_.device, #name));         \
    ok &= name##_ != nullptr;
    PORPOISE_UI_VK_FUNCS(LOAD)
#undef LOAD
    vkGetPhysicalDeviceMemoryProperties_ = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        init_.get_instance_proc(init_.instance, "vkGetPhysicalDeviceMemoryProperties"));
    return ok && vkGetPhysicalDeviceMemoryProperties_;
}

std::uint32_t Gfx::memory_type(std::uint32_t bits, VkMemoryPropertyFlags flags) const
{
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties_(init_.gpu, &props);
    for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    return UINT32_MAX;
}

bool Gfx::make_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buffer, VkDeviceMemory &memory,
                      void **mapped)
{
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    if (vkCreateBuffer_(device_, &info, nullptr, &buffer) != VK_SUCCESS)
        return false;
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements_(device_, buffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex =
        memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (alloc.memoryTypeIndex == UINT32_MAX || vkAllocateMemory_(device_, &alloc, nullptr, &memory) != VK_SUCCESS)
        return false;
    vkBindBufferMemory_(device_, buffer, memory, 0);
    if (mapped && vkMapMemory_(device_, memory, 0, VK_WHOLE_SIZE, 0, mapped) != VK_SUCCESS)
        return false;
    return true;
}

bool Gfx::create_pipeline()
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dsl.bindingCount = 1;
    dsl.pBindings = &binding;
    if (vkCreateDescriptorSetLayout_(device_, &dsl, nullptr, &set_layout_) != VK_SUCCESS)
        return false;

    VkPushConstantRange range{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 32};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &set_layout_;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &range;
    if (vkCreatePipelineLayout_(device_, &pl, nullptr, &layout_) != VK_SUCCESS)
        return false;

    VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm.codeSize = sizeof(porpoise_ui_vert);
    sm.pCode = porpoise_ui_vert;
    if (vkCreateShaderModule_(device_, &sm, nullptr, &vert) != VK_SUCCESS)
        return false;
    sm.codeSize = sizeof(porpoise_ui_frag);
    sm.pCode = porpoise_ui_frag;
    if (vkCreateShaderModule_(device_, &sm, nullptr, &frag) != VK_SUCCESS)
        return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription vb{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[8] = {
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, pos)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, local)},
        {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color)},
        {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, p0)},
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, p1)},
        {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, p2)},
        {7, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, bcolor)},
    };
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &vb;
    vi.vertexAttributeDescriptionCount = 8;
    vi.pVertexAttributeDescriptions = attrs;
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
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
    const VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dyn_states;
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
    gp.layout = layout_;
    gp.renderPass = init_.render_pass;
    const VkResult r = vkCreateGraphicsPipelines_(device_, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline_);
    vkDestroyShaderModule_(device_, vert, nullptr);
    vkDestroyShaderModule_(device_, frag, nullptr);
    if (r != VK_SUCCESS)
        return false;

    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler_(device_, &sampler, nullptr, &sampler_) != VK_SUCCESS)
        return false;

    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024};
    VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dp.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dp.maxSets = 1024;
    dp.poolSizeCount = 1;
    dp.pPoolSizes = &size;
    if (vkCreateDescriptorPool_(device_, &dp, nullptr, &pool_) != VK_SUCCESS)
        return false;

    VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cp.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cp.queueFamilyIndex = init_.queue_family;
    if (vkCreateCommandPool_(device_, &cp, nullptr, &upload_pool_) != VK_SUCCESS)
        return false;

    vcapacity_ = 60000; /* 10000 quads a frame */
    vbufs_.resize(init_.slots);
    vmems_.resize(init_.slots);
    vmaps_.resize(init_.slots);
    for (unsigned i = 0; i < init_.slots; ++i)
        if (!make_buffer(vcapacity_ * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vbufs_[i], vmems_[i],
                         &vmaps_[i]))
            return false;
    return true;
}

Texture *Gfx::upload(const std::uint8_t *pixels, int width, int height)
{
    if (!device_ || width <= 0 || height <= 0)
        return nullptr;
    auto *t = new Texture();
    t->width = width;
    t->height = height;

    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {std::uint32_t(width), std::uint32_t(height), 1};
    info.mipLevels = 1; /* single level: what the console's driver is proven with */
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage_(device_, &info, nullptr, &t->image) != VK_SUCCESS)
    {
        delete t;
        return nullptr;
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements_(device_, t->image, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (alloc.memoryTypeIndex == UINT32_MAX)
        alloc.memoryTypeIndex = memory_type(req.memoryTypeBits, 0);
    if (vkAllocateMemory_(device_, &alloc, nullptr, &t->memory) != VK_SUCCESS)
    {
        vkDestroyImage_(device_, t->image, nullptr);
        delete t;
        return nullptr;
    }
    vkBindImageMemory_(device_, t->image, t->memory, 0);

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory staging_mem = VK_NULL_HANDLE;
    void *mapped = nullptr;
    const VkDeviceSize bytes = VkDeviceSize(width) * height * 4;
    if (!make_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging, staging_mem, &mapped))
    {
        free_texture(t);
        return nullptr;
    }
    std::memcpy(mapped, pixels, std::size_t(bytes));

    VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ca.commandPool = upload_pool_;
    ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ca.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers_(device_, &ca, &cmd);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer_(cmd, &begin);
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = t->image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier_(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                          nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = info.extent;
    vkCmdCopyBufferToImage_(cmd, staging, t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier_(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                          0, nullptr, 1, &barrier);
    vkEndCommandBuffer_(cmd);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    if (init_.queue_mutex)
        init_.queue_mutex->lock();
    vkQueueSubmit_(init_.queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle_(init_.queue);
    if (init_.queue_mutex)
        init_.queue_mutex->unlock();
    vkFreeCommandBuffers_(device_, upload_pool_, 1, &cmd);
    vkUnmapMemory_(device_, staging_mem);
    vkDestroyBuffer_(device_, staging, nullptr);
    vkFreeMemory_(device_, staging_mem, nullptr);

    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = t->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R8G8B8A8_UNORM;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView_(device_, &view, nullptr, &t->view);

    VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ds.descriptorPool = pool_;
    ds.descriptorSetCount = 1;
    ds.pSetLayouts = &set_layout_;
    vkAllocateDescriptorSets_(device_, &ds, &t->set);
    VkDescriptorImageInfo image_info{sampler_, t->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = t->set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;
    vkUpdateDescriptorSets_(device_, 1, &write, 0, nullptr);
    textures_.push_back(t);
    return t;
}

Texture *Gfx::texture_rgba(const std::uint8_t *pixels, int width, int height)
{
    /* Save icons and the like, often dozens at once (a memory card opening):
     * copied in with the frame's commands, not one GPU wait each. */
    return upload_later(pixels, width, height);
}

Texture *Gfx::texture_file(const std::string &path, int max_side)
{
    std::vector<unsigned char> bytes;
    if (!read_file(path, bytes))
        return nullptr;
    int w = 0, h = 0, n = 0;
    unsigned char *pixels = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &n, 4);
    if (!pixels)
        return nullptr;
    /* Big pictures (a player's own 4K cover) are halved until they fit
     * max_side (1024 unless the picture fills the screen): memory stays small. */
    std::vector<unsigned char> fitted;
    fit_side(pixels, w, h, max_side, fitted);
    stbi_image_free(pixels);
    return upload(fitted.data(), w, h);
}

Texture *Gfx::texture_file_async(const std::string &path, bool *pending, int max_side)
{
    *pending = false;
    if (!async_loads_)
        return texture_file(path, max_side);
    if (!device_ || path.empty())
        return nullptr;
    Loader &L = loader();
    std::vector<unsigned char> rgba;
    int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> g(L.lock);
        auto it = L.loads.find(path);
        if (it == L.loads.end())
        {
            Load &load = L.loads[path];
            load.max_side = max_side;
            load.wanted = frame_no_;
            L.queue.push_back(path);
            start_loader_locked(L);
            L.wake.notify_one();
            *pending = true;
            return nullptr;
        }
        Load &load = it->second;
        load.wanted = frame_no_;
        if (load.state == Load::Failed)
        {
            L.loads.erase(it);
            return nullptr;
        }
        if (load.state != Load::Done || uploads_left_ <= 0)
        {
            *pending = true;
            return nullptr;
        }
        rgba.swap(load.rgba);
        w = load.w;
        h = load.h;
        L.loads.erase(it);
    }
    --uploads_left_;
    Texture *t = upload_later(rgba.data(), w, h);
    if (!t)
        return nullptr;
    return t;
}

/* Like upload(), but the copy is recorded into this frame's own command
 * buffer (record_uploads, before the render pass) instead of being submitted
 * and waited on: no stall. */
Texture *Gfx::upload_later(const std::uint8_t *pixels, int width, int height)
{
    if (!device_ || width <= 0 || height <= 0)
        return nullptr;
    auto *t = new Texture();
    t->width = width;
    t->height = height;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {std::uint32_t(width), std::uint32_t(height), 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage_(device_, &info, nullptr, &t->image) != VK_SUCCESS)
    {
        delete t;
        return nullptr;
    }
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements_(device_, t->image, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (alloc.memoryTypeIndex == UINT32_MAX)
        alloc.memoryTypeIndex = memory_type(req.memoryTypeBits, 0);
    if (vkAllocateMemory_(device_, &alloc, nullptr, &t->memory) != VK_SUCCESS)
    {
        destroy_texture(t);
        return nullptr;
    }
    vkBindImageMemory_(device_, t->image, t->memory, 0);
    const VkDeviceSize bytes = VkDeviceSize(width) * height * 4;
    if (!make_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, t->staging[0], t->staging_memory[0],
                     &t->staging_mapped[0]))
    {
        destroy_texture(t);
        return nullptr;
    }
    std::memcpy(t->staging_mapped[0], pixels, std::size_t(bytes));
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = t->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R8G8B8A8_UNORM;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView_(device_, &view, nullptr, &t->view) != VK_SUCCESS)
    {
        destroy_texture(t);
        return nullptr;
    }
    VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ds.descriptorPool = pool_;
    ds.descriptorSetCount = 1;
    ds.pSetLayouts = &set_layout_;
    if (vkAllocateDescriptorSets_(device_, &ds, &t->set) != VK_SUCCESS)
    {
        t->set = VK_NULL_HANDLE;
        destroy_texture(t);
        return nullptr;
    }
    VkDescriptorImageInfo image_info{sampler_, t->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = t->set;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;
    vkUpdateDescriptorSets_(device_, 1, &write, 0, nullptr);
    t->fresh = true;
    t->pending = 0;
    textures_.push_back(t);
    return t;
}

void Gfx::free_texture(Texture *t)
{
    if (!t || !device_)
        return;
    /* A frame still on the GPU may sample it: it is destroyed once every
     * frame slot has come round again (destroy_texture, from begin()). */
    textures_.erase(std::remove(textures_.begin(), textures_.end(), t), textures_.end());
    graveyard_.push_back({t, frame_no_});
}

Texture *Gfx::stream_texture(int width, int height)
{
    std::vector<std::uint8_t> clear(std::size_t(width) * height * 4, 0);
    Texture *t = upload(clear.data(), width, height);
    if (!t)
        return nullptr;
    const VkDeviceSize bytes = VkDeviceSize(width) * height * 4;
    for (int i = 0; i < 2; ++i)
        if (!make_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, t->staging[i], t->staging_memory[i],
                         &t->staging_mapped[i]))
        {
            free_texture(t);
            return nullptr;
        }
    return t;
}

void Gfx::stream_update(Texture *t, const std::uint8_t *pixels)
{
    if (!t || !t->staging_mapped[0] || !pixels)
        return;
    const int slot = int(slot_ % 2);
    std::memcpy(t->staging_mapped[slot], pixels, std::size_t(t->width) * t->height * 4);
    t->pending = slot;
}

void Gfx::record_uploads(VkCommandBuffer cmd)
{
    for (Texture *t : textures_)
    {
        if (t->pending < 0 || !t->staging[t->pending])
            continue;
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = t->image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.oldLayout = t->fresh ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = t->fresh ? 0 : VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier_(cmd, t->fresh ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {std::uint32_t(t->width), std::uint32_t(t->height), 1};
        vkCmdCopyBufferToImage_(cmd, t->staging[t->pending], t->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier_(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                              nullptr, 0, nullptr, 1, &barrier);
        t->pending = -1;
        if (t->fresh)
        {
            /* Its one copy is recorded: the staging buffer goes once this
             * frame is done with it. */
            auto *spent = new Texture();
            spent->staging[0] = t->staging[0];
            spent->staging_memory[0] = t->staging_memory[0];
            spent->staging_mapped[0] = t->staging_mapped[0];
            t->staging[0] = VK_NULL_HANDLE;
            t->staging_memory[0] = VK_NULL_HANDLE;
            t->staging_mapped[0] = nullptr;
            t->fresh = false;
            graveyard_.push_back({spent, frame_no_});
        }
    }
}

void Gfx::destroy_texture(Texture *t)
{
    for (int i = 0; i < 2; ++i)
    {
        if (t->staging_mapped[i])
            vkUnmapMemory_(device_, t->staging_memory[i]);
        if (t->staging[i])
            vkDestroyBuffer_(device_, t->staging[i], nullptr);
        if (t->staging_memory[i])
            vkFreeMemory_(device_, t->staging_memory[i], nullptr);
    }
    if (t->set)
        vkFreeDescriptorSets_(device_, pool_, 1, &t->set);
    if (t->view)
        vkDestroyImageView_(device_, t->view, nullptr);
    if (t->image)
        vkDestroyImage_(device_, t->image, nullptr);
    if (t->memory)
        vkFreeMemory_(device_, t->memory, nullptr);
    delete t;
}

bool Gfx::build_fonts()
{
    if (fonts_built_)
    {
        atlas_ = upload(atlas_pixels_.data(), kAtlas, kAtlas);
        if (!cjk_pixels_.empty())
            cjk_atlas_ = upload(cjk_pixels_.data(), kAtlas, kAtlas);
        return atlas_ != nullptr;
    }
    static const char *const files[4] = {"Nunito-Regular.ttf", "Nunito-SemiBold.ttf", "Nunito-Bold.ttf",
                                         "Nunito-ExtraBold.ttf"};
    std::vector<std::uint8_t> atlas(std::size_t(kAtlas) * kAtlas * 4, 0);
    for (std::size_t i = 0; i < atlas.size(); i += 4)
        atlas[i] = atlas[i + 1] = atlas[i + 2] = 255;
    int pen_x = 1, pen_y = 1, row_h = 0;
    const unsigned char onedge = 128;
    const float dist_scale = 128.0f / kPad;

    std::vector<std::uint8_t> *target = &atlas;
    auto bake = [&](FontData &fd, std::uint32_t cp, GlyphInfo &g) {
        auto *info = static_cast<stbtt_fontinfo *>(fd.info);
        int w = 0, h = 0, xoff = 0, yoff = 0;
        unsigned char *sdf = stbtt_GetCodepointSDF(info, fd.scale, int(cp), kPad, onedge, dist_scale, &w, &h,
                                                    &xoff, &yoff);
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(info, int(cp), &advance, &lsb);
        g.advance = advance * fd.scale;
        g.present = true;
        if (!sdf)
        {
            g.w = g.h = 0;
            return;
        }
        if (pen_x + w + 1 >= kAtlas)
        {
            pen_x = 1;
            pen_y += row_h + 1;
            row_h = 0;
        }
        if (pen_y + h + 1 >= kAtlas)
        {
            stbtt_FreeSDF(sdf, nullptr);
            g.present = false;
            return;
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                (*target)[(std::size_t(pen_y + y) * kAtlas + std::size_t(pen_x + x)) * 4 + 3] = sdf[y * w + x];
        g.u0 = float(pen_x) / kAtlas;
        g.v0 = float(pen_y) / kAtlas;
        g.u1 = float(pen_x + w) / kAtlas;
        g.v1 = float(pen_y + h) / kAtlas;
        g.xoff = float(xoff);
        g.yoff = float(yoff);
        g.w = float(w);
        g.h = float(h);
        pen_x += w + 1;
        row_h = std::max(row_h, h);
        stbtt_FreeSDF(sdf, nullptr);
    };

    /* The fonts themselves and their measurements: quick. */
    for (int f = 0; f < 4; ++f)
    {
        FontData &fd = fonts_[f];
        if (!read_file(init_.asset_dir + "/fonts/" + files[f], fd.ttf))
            return false;
        auto *info = new stbtt_fontinfo();
        if (!stbtt_InitFont(info, fd.ttf.data(), stbtt_GetFontOffsetForIndex(fd.ttf.data(), 0)))
            return false;
        fd.info = info;
        fd.scale = stbtt_ScaleForPixelHeight(info, kBase);
        int ascent = 0, descent = 0, gap = 0;
        stbtt_GetFontVMetrics(info, &ascent, &descent, &gap);
        fd.ascent = ascent * fd.scale;
        fd.descent = descent * fd.scale;
        fd.line_gap = gap * fd.scale;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        if (stbtt_GetCodepointBox(info, 'H', &x0, &y0, &x1, &y1))
            fd.cap = float(y1) * fd.scale;
        else
            fd.cap = fd.ascent * 0.7f;
    }
    if (read_file(init_.asset_dir + "/fonts/NotoSansJP-Porpoise.ttf", cjk_.ttf))
    {
        auto *info = new stbtt_fontinfo();
        if (stbtt_InitFont(info, cjk_.ttf.data(), stbtt_GetFontOffsetForIndex(cjk_.ttf.data(), 0)))
        {
            cjk_.info = info;
            /* The same em as Nunito, a touch smaller: kana and kanji fill their em. */
            const float nunito_em = fonts_[0].scale * 1000.0f;
            cjk_.scale = stbtt_ScaleForMappingEmToPixels(info, nunito_em * 0.92f);
        }
        else
        {
            delete info;
            cjk_.ttf.clear();
        }
    }

    /* The glyphs' distance fields take seconds to bake on the console, so
     * they are kept in the cache folder, named for everything they depend on,
     * and read back at the next start. */
    std::string cache;
    if (!init_.cache_dir.empty())
    {
        std::uint64_t key = 1469598103934665603ULL;
        auto mix = [&](const void *data, std::size_t n) {
            const auto *b = static_cast<const unsigned char *>(data);
            for (std::size_t i = 0; i < n; ++i)
                key = (key ^ b[i]) * 1099511628211ULL;
        };
        const char version[] = "porpoise-atlas-1";
        mix(version, sizeof version);
        const int dims[4] = {kAtlas, int(kBase), kPad, int(sizeof(GlyphInfo))};
        mix(dims, sizeof dims);
        for (const FontData &fd : fonts_)
            mix(fd.ttf.data(), fd.ttf.size());
        mix(cjk_.ttf.data(), cjk_.ttf.size());
        char name[64];
        std::snprintf(name, sizeof name, "/text-atlas-%016llx.bin", static_cast<unsigned long long>(key));
        cache = init_.cache_dir + name;
        std::vector<std::uint8_t> cjk;
        if (load_atlas_cache(cache, atlas, cjk))
        {
            atlas_ = upload(atlas.data(), kAtlas, kAtlas);
            atlas_pixels_ = std::move(atlas);
            if (!cjk.empty())
            {
                cjk_atlas_ = upload(cjk.data(), kAtlas, kAtlas);
                cjk_pixels_ = std::move(cjk);
            }
            std::fprintf(stderr, "[gfx] text atlases read from %s\n", cache.c_str());
            fonts_built_ = true;
            return atlas_ != nullptr;
        }
    }

    for (int f = 0; f < 4; ++f)
    {
        FontData &fd = fonts_[f];
        for (std::uint32_t cp = 32; cp < 127; ++cp)
            bake(fd, cp, fd.glyphs[cp]);
        /* Latin-1 letters and marks, Polish and Dutch letters, Russian's
         * Cyrillic, plus the typographic ones Porpoise uses. */
        fd.extra.clear();
        std::vector<std::uint32_t> cps;
        for (std::uint32_t cp = 0xA1; cp <= 0xFF; ++cp)
            cps.push_back(cp);
        for (std::uint32_t cp = 0x410; cp <= 0x44F; ++cp)
            cps.push_back(cp);
        for (std::uint32_t cp : {0x104u, 0x105u, 0x106u, 0x107u, 0x118u, 0x119u, 0x132u, 0x133u, 0x141u, 0x142u,
                                 0x143u, 0x144u, 0x152u, 0x153u, 0x15Au, 0x15Bu, 0x179u, 0x17Au, 0x17Bu, 0x17Cu,
                                 0x401u, 0x451u, 0x2013u, 0x2014u, 0x2018u, 0x2019u, 0x201Au, 0x201Cu, 0x201Du,
                                 0x201Eu, 0x2022u, 0x2026u, 0x20ACu, 0x2116u})
            cps.push_back(cp);
        std::sort(cps.begin(), cps.end()); /* the lookup is a binary search */
        auto *finfo = static_cast<stbtt_fontinfo *>(fd.info);
        for (std::uint32_t cp : cps)
        {
            if (!stbtt_FindGlyphIndex(finfo, int(cp)))
                continue;
            GlyphInfo g;
            bake(fd, cp, g);
            if (g.present)
                fd.extra.push_back({cp, g});
        }
    }
    atlas_ = upload(atlas.data(), kAtlas, kAtlas);
    atlas_pixels_ = std::move(atlas);
    std::fprintf(stderr, "[gfx] text atlas filled to row %d of %d\n", pen_y + row_h, kAtlas);

    /* Japanese, when its font is there: every character the subset holds. */
    if (cjk_.info)
    {
        auto *info = static_cast<stbtt_fontinfo *>(cjk_.info);
        {
            cjk_.extra.clear();
            std::vector<std::uint8_t> cjk(std::size_t(kAtlas) * kAtlas * 4, 0);
            for (std::size_t i = 0; i < cjk.size(); i += 4)
                cjk[i] = cjk[i + 1] = cjk[i + 2] = 255;
            target = &cjk;
            pen_x = pen_y = 1;
            row_h = 0;
            static const std::uint32_t ranges[][2] = {{0x2190, 0x27FF}, {0x2E80, 0x9FFF}, {0xF900, 0xFFEF}};
            for (const auto &r : ranges)
                for (std::uint32_t cp = r[0]; cp <= r[1]; ++cp)
                {
                    if (!stbtt_FindGlyphIndex(info, int(cp)))
                        continue;
                    GlyphInfo g;
                    bake(cjk_, cp, g);
                    g.cjk = true;
                    if (g.present)
                        cjk_.extra.push_back({cp, g});
                }
            std::fprintf(stderr, "[gfx] Japanese atlas: %zu glyphs, filled to row %d of %d\n", cjk_.extra.size(),
                         pen_y + row_h, kAtlas);
            cjk_atlas_ = upload(cjk.data(), kAtlas, kAtlas);
            cjk_pixels_ = std::move(cjk);
        }
    }
    fonts_built_ = true;
    if (!cache.empty() && atlas_)
        save_atlas_cache(cache);
    return atlas_ != nullptr;
}

/* The cache file: a header, each font's glyphs, then the atlases' coverage
 * (one byte a pixel; the colour is always white). */
namespace
{
struct AtlasHeader
{
    char magic[8];
    std::uint32_t glyph_size, atlas, fonts, cjk;
};
} // namespace

bool Gfx::load_atlas_cache(const std::string &path, std::vector<std::uint8_t> &atlas, std::vector<std::uint8_t> &cjk)
{
    std::vector<unsigned char> bytes;
    if (!read_file(path, bytes))
        return false;
    std::size_t at = 0;
    auto take = [&](void *out, std::size_t n) {
        if (at + n > bytes.size())
            return false;
        std::memcpy(out, bytes.data() + at, n);
        at += n;
        return true;
    };
    AtlasHeader h{};
    if (!take(&h, sizeof h) || std::memcmp(h.magic, "PPATLAS1", 8) != 0 || h.glyph_size != sizeof(GlyphInfo) ||
        h.atlas != std::uint32_t(kAtlas) || h.fonts != 4 || h.cjk != (cjk_.info ? 1u : 0u))
        return false;
    FontData *all[5] = {&fonts_[0], &fonts_[1], &fonts_[2], &fonts_[3], &cjk_};
    for (int f = 0; f < 4 + int(h.cjk); ++f)
    {
        FontData &fd = *all[f];
        if (f < 4 && !take(fd.glyphs, sizeof fd.glyphs))
            return false;
        std::uint32_t n = 0;
        if (!take(&n, sizeof n) || n > 65536)
            return false;
        fd.extra.resize(n);
        for (auto &e : fd.extra)
            if (!take(&e.first, sizeof e.first) || !take(&e.second, sizeof e.second))
                return false;
    }
    const std::size_t pixels = std::size_t(kAtlas) * kAtlas;
    auto expand = [&](std::vector<std::uint8_t> &out) {
        if (at + pixels > bytes.size())
            return false;
        out.assign(pixels * 4, 255);
        for (std::size_t i = 0; i < pixels; ++i)
            out[i * 4 + 3] = bytes[at + i];
        at += pixels;
        return true;
    };
    if (!expand(atlas))
        return false;
    if (h.cjk && !expand(cjk))
        return false;
    return true;
}

void Gfx::save_atlas_cache(const std::string &path) const
{
    const std::string staged = path + ".tmp";
    std::FILE *f = std::fopen(staged.c_str(), "wb");
    if (!f)
        return;
    const bool with_cjk = !cjk_pixels_.empty() && cjk_.info;
    AtlasHeader h{};
    std::memcpy(h.magic, "PPATLAS1", 8);
    h.glyph_size = sizeof(GlyphInfo);
    h.atlas = std::uint32_t(kAtlas);
    h.fonts = 4;
    h.cjk = with_cjk ? 1u : 0u;
    bool ok = std::fwrite(&h, sizeof h, 1, f) == 1;
    const FontData *all[5] = {&fonts_[0], &fonts_[1], &fonts_[2], &fonts_[3], &cjk_};
    for (int i = 0; ok && i < 4 + int(h.cjk); ++i)
    {
        const FontData &fd = *all[i];
        if (i < 4)
            ok &= std::fwrite(fd.glyphs, sizeof fd.glyphs, 1, f) == 1;
        const std::uint32_t n = std::uint32_t(fd.extra.size());
        ok &= std::fwrite(&n, sizeof n, 1, f) == 1;
        for (const auto &e : fd.extra)
            ok &= std::fwrite(&e.first, sizeof e.first, 1, f) == 1 && std::fwrite(&e.second, sizeof e.second, 1, f) == 1;
    }
    std::vector<std::uint8_t> alpha(std::size_t(kAtlas) * kAtlas);
    auto squeeze = [&](const std::vector<std::uint8_t> &rgba) {
        for (std::size_t i = 0; i < alpha.size(); ++i)
            alpha[i] = rgba[i * 4 + 3];
        return std::fwrite(alpha.data(), 1, alpha.size(), f) == alpha.size();
    };
    ok &= atlas_pixels_.size() == alpha.size() * 4 && squeeze(atlas_pixels_);
    if (ok && with_cjk)
        ok &= cjk_pixels_.size() == alpha.size() * 4 && squeeze(cjk_pixels_);
    ok &= std::fclose(f) == 0;
    if (!ok || std::rename(staged.c_str(), path.c_str()) != 0)
        std::remove(staged.c_str());
}

bool Gfx::init(const GfxInit &init)
{
    init_ = init;
    device_ = init.device;
    if (!load_functions() || !create_pipeline())
    {
        shutdown();
        return false;
    }
    const std::uint8_t white[4] = {255, 255, 255, 255};
    white_ = upload(white, 1, 1);
    if (!white_ || !build_fonts())
    {
        shutdown();
        return false;
    }
    brand_mask_ = texture_file(init_.asset_dir + "/brand/dolphin-mask.png");
    icons_ = texture_file(init_.asset_dir + "/ui/buttons.png");
    vertices_.reserve(vcapacity_);
    return true;
}

void Gfx::shutdown()
{
    if (!device_)
        return;
    if (vkDeviceWaitIdle_)
        vkDeviceWaitIdle_(device_);
    for (Texture *t : textures_)
        destroy_texture(t);
    textures_.clear();
    for (auto &dead : graveyard_)
        destroy_texture(dead.first);
    graveyard_.clear();
    white_ = atlas_ = brand_mask_ = icons_ = cjk_atlas_ = nullptr;
    for (std::size_t i = 0; i < vbufs_.size(); ++i)
    {
        if (vmaps_[i])
            vkUnmapMemory_(device_, vmems_[i]);
        if (vbufs_[i])
            vkDestroyBuffer_(device_, vbufs_[i], nullptr);
        if (vmems_[i])
            vkFreeMemory_(device_, vmems_[i], nullptr);
    }
    vbufs_.clear();
    vmems_.clear();
    vmaps_.clear();
    if (upload_pool_)
        vkDestroyCommandPool_(device_, upload_pool_, nullptr);
    if (pool_)
        vkDestroyDescriptorPool_(device_, pool_, nullptr);
    if (sampler_)
        vkDestroySampler_(device_, sampler_, nullptr);
    if (pipeline_)
        vkDestroyPipeline_(device_, pipeline_, nullptr);
    if (layout_)
        vkDestroyPipelineLayout_(device_, layout_, nullptr);
    if (set_layout_)
        vkDestroyDescriptorSetLayout_(device_, set_layout_, nullptr);
    upload_pool_ = VK_NULL_HANDLE;
    pool_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    pipeline_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    set_layout_ = VK_NULL_HANDLE;
    /* The fonts' metrics and baked atlas stay in memory (fonts_built_), so a
     * renderer rebuilt on another device only uploads the atlas again. */
    device_ = VK_NULL_HANDLE;
}

/* ---- frame ------------------------------------------------------------------------------- */

void Gfx::begin(unsigned slot, float target_w, float target_h, float time, float dim, bool reduced_motion)
{
    tone_ = false;
    slot_ = slot % std::max<unsigned>(init_.slots, 1);
    target_w_ = target_w;
    target_h_ = target_h;
    time_ = time;
    dim_ = dim;
    reduced_motion_ = reduced_motion;
    layer_dx_ = layer_dy_ = 0;
    layer_fade_ = 1;
    ++frame_no_;
    uploads_left_ = 2;
    if (async_loads_)
    {
        Loader &L = loader();
        L.frame.store(frame_no_);
        if (frame_no_ % 30 == 0)
        {
            /* Decoded pictures nobody came back for give their memory back. */
            std::lock_guard<std::mutex> g(L.lock);
            for (auto it = L.loads.begin(); it != L.loads.end();)
                if (it->second.state != Load::Busy && frame_no_ - it->second.wanted > kLoadForgotten)
                    it = L.loads.erase(it);
                else
                    ++it;
        }
    }
    const std::uint64_t safe = std::uint64_t(std::max<unsigned>(init_.slots, 1)) + 1;
    for (std::size_t i = 0; i < graveyard_.size();)
        if (frame_no_ - graveyard_[i].second > safe)
        {
            destroy_texture(graveyard_[i].first);
            graveyard_[i] = graveyard_.back();
            graveyard_.pop_back();
        }
        else
            ++i;
    vertices_.clear();
    batches_.clear();
    overflowed_ = false;
}

void Gfx::to_clip(const Corner &c, float out[4]) const
{
    out[0] = (c.x / kDesignW * 2.0f - 1.0f) * c.w;
    out[1] = (c.y / kDesignH * 2.0f - 1.0f) * c.w;
    out[2] = 0.0f;
    out[3] = c.w;
}

void Gfx::corners_flat(float x, float y, float w, float h, float out[4][4]) const
{
    const Corner c[4] = {{x, y, 1}, {x + w, y, 1}, {x + w, y + h, 1}, {x, y + h, 1}};
    for (int i = 0; i < 4; ++i)
        to_clip(c[i], out[i]);
}

void Gfx::push(Texture *t, const Vertex v[4])
{
    if (vertices_.size() + 6 > vcapacity_)
    {
        overflowed_ = true;
        return;
    }
    VkDescriptorSet set = (t ? t : white_)->set;
    if (batches_.empty() || batches_.back().set != set)
        batches_.push_back({set, std::uint32_t(vertices_.size()), 0});
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    const bool layered = layer_dx_ != 0 || layer_dy_ != 0 || layer_fade_ < 1;
    for (int i : order)
    {
        vertices_.push_back(v[i]);
        if (!layered || int(v[i].p0[0] + 0.5f) == K_BACKGROUND)
            continue;
        Vertex &o = vertices_.back();
        o.pos[0] += layer_dx_ * 2.0f / kDesignW * o.pos[3];
        o.pos[1] += layer_dy_ * 2.0f / kDesignH * o.pos[3];
        if (int(o.p0[0] + 0.5f) == K_GLASS)
            o.p2[3] *= layer_fade_;
        else
        {
            o.color[3] *= layer_fade_;
            o.bcolor[3] *= layer_fade_;
        }
    }
    batches_.back().count += 6;
}

namespace
{
void fill(float *dst, std::initializer_list<float> values)
{
    int i = 0;
    for (float v : values)
        dst[i++] = v;
}
} // namespace

Color Gfx::tone(Color c) const
{
    if (!tone_)
        return c;
    /* RGB -> HSL, lightness turned over (dark navy glass -> near white, white
     * text -> slate ink, cyan -> a deeper blue), saturation eased as it gets
     * light so panels stay pale. */
    const float mx = std::max(c.r, std::max(c.g, c.b)), mn = std::min(c.r, std::min(c.g, c.b));
    float h = 0, sat = 0;
    const float l = (mx + mn) * 0.5f, d = mx - mn;
    if (d > 1e-5f)
    {
        sat = l > 0.5f ? d / (2.0f - mx - mn) : d / (mx + mn);
        if (mx == c.r)
            h = (c.g - c.b) / d + (c.g < c.b ? 6.0f : 0.0f);
        else if (mx == c.g)
            h = (c.b - c.r) / d + 2.0f;
        else
            h = (c.r - c.g) / d + 4.0f;
        h /= 6.0f;
    }
    const float l2 = 0.22f + 0.76f * std::pow(std::max(0.0f, 1.0f - l), l < 0.5f ? 0.45f : 0.7f);
    /* Light labels become slate ink, dark glass near-white, and the colours
     * in between (the accents) keep most of their colour. */
    const float s2 = l > 0.82f ? sat * 0.35f : l < 0.30f ? sat * 0.22f : l < 0.42f ? sat * 0.6f : sat * 0.9f;
    auto hue = [](float p, float q, float t) {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0f / 6) return p + (q - p) * 6 * t;
        if (t < 0.5f) return q;
        if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
        return p;
    };
    Color o;
    o.a = c.a;
    if (s2 <= 1e-5f)
        o.r = o.g = o.b = l2;
    else
    {
        const float q = l2 < 0.5f ? l2 * (1 + s2) : l2 + s2 - l2 * s2, p = 2 * l2 - q;
        o.r = hue(p, q, h + 1.0f / 3);
        o.g = hue(p, q, h);
        o.b = hue(p, q, h - 1.0f / 3);
    }
    return o;
}

void Gfx::background()
{
    Vertex v[4]{};
    float pos[4][4];
    corners_flat(0, 0, kDesignW, kDesignH, pos);
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        std::memcpy(v[i].pos, pos[i], sizeof pos[i]);
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {1, 1, 1, 1});
        fill(v[i].p0, {float(K_BACKGROUND), 0, 0, 0});
        fill(v[i].p1, {target_w_, target_h_, target_w_, target_h_});
    }
    push(nullptr, v);
}

void Gfx::panel(float x, float y, float w, float h, Color fill_c, float bottom_mul, float radius, Color border,
                float border_w, float glow, float sheen)
{
    if (tone_)
        bottom_mul = 1.0f - (1.0f - bottom_mul) * 0.25f; /* no dark floor under light glass */
    fill_c = tone(fill_c);
    border = tone(border);
    const float px = target_w_ / kDesignW; /* design px -> screen px */
    const float margin = glow > 0 ? glow * 3.0f : 1.0f;
    Vertex v[4]{};
    float pos[4][4];
    corners_flat(x - margin, y - margin, w + margin * 2, h + margin * 2, pos);
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        std::memcpy(v[i].pos, pos[i], sizeof pos[i]);
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {fill_c.r, fill_c.g, fill_c.b, fill_c.a});
        fill(v[i].p0, {float(K_PANEL), radius * px, border_w * px, glow * px});
        fill(v[i].p1, {(w + margin * 2) * px, (h + margin * 2) * px, w * px, h * px});
        fill(v[i].p2, {bottom_mul, sheen, 0, 0});
        fill(v[i].bcolor, {border.r, border.g, border.b, border.a});
    }
    push(nullptr, v);
}

void Gfx::image(Texture *t, float x, float y, float w, float h, Color tint, float radius)
{
    const float px = target_w_ / kDesignW;
    Vertex v[4]{};
    float pos[4][4];
    corners_flat(x, y, w, h, pos);
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        std::memcpy(v[i].pos, pos[i], sizeof pos[i]);
        fill(v[i].uv, {local[i][0], local[i][1]});
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {tint.r, tint.g, tint.b, tint.a});
        fill(v[i].p0, {float(radius > 0 ? K_COVER : K_IMAGE), radius * px, 0, 0});
        fill(v[i].p1, {w * px, h * px, w * px, h * px});
    }
    push(t, v);
}

void Gfx::blob(float cx, float cy, float w, float h, Color c)
{
    c = tone(c);
    const float px = target_w_ / kDesignW;
    Vertex v[4]{};
    float pos[4][4];
    corners_flat(cx - w * 0.5f, cy - h * 0.5f, w, h, pos);
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        std::memcpy(v[i].pos, pos[i], sizeof pos[i]);
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {c.r, c.g, c.b, c.a});
        fill(v[i].p0, {float(K_BLOB), 0, 0, 0});
        fill(v[i].p1, {w * px, h * px, w * px, h * px});
    }
    push(nullptr, v);
}

void Gfx::image_part(Texture *t, float x, float y, float w, float h, const float uv[4], Color tint, float radius)
{
    const float px = target_w_ / kDesignW;
    Vertex v[4]{};
    float pos[4][4];
    corners_flat(x, y, w, h, pos);
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        std::memcpy(v[i].pos, pos[i], sizeof pos[i]);
        fill(v[i].uv, {uv[0] + (uv[2] - uv[0]) * local[i][0], uv[1] + (uv[3] - uv[1]) * local[i][1]});
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {tint.r, tint.g, tint.b, tint.a});
        fill(v[i].p0, {float(radius > 0 ? K_COVER : K_IMAGE), radius * px, 0, 0});
        fill(v[i].p1, {w * px, h * px, w * px, h * px});
    }
    push(t, v);
}

void Gfx::icon(Icon i, float cx, float cy, float size, Color c)
{
    c = tone(c);
    constexpr int kCols = 8, kRows = 4;
    const int n = int(i);
    if (!icons_ || n < 0 || n >= int(Icon::Count))
        return;
    /* Half a texel in from the cell's edge, so neighbours never bleed in. */
    const float cu = 1.0f / kCols, cv = 1.0f / kRows;
    const float hu = 0.5f / float(icons_->width), hv = 0.5f / float(icons_->height);
    const float u0 = float(n % kCols) * cu, v0 = float(n / kCols) * cv;
    const float uv[4] = {u0 + hu, v0 + hv, u0 + cu - hu, v0 + cv - hv};
    image_part(icons_, cx - size * 0.5f, cy - size * 0.5f, size, size, uv, c);
}

void Gfx::glyph(Glyph g, float cx, float cy, float size, Color c, float rotation)
{
    /* The DualSense's buttons come from the icon atlas when it is there; a
     * face button's ring is drawn the size the old glyph was. */
    if (icons_)
    {
        Icon ic = Icon::Count;
        float scale = 1.5f;
        switch (g)
        {
        case Glyph::Cross: ic = Icon::Cross; break;
        case Glyph::Circle: ic = Icon::Circle; break;
        case Glyph::Square: ic = Icon::Square; break;
        case Glyph::Triangle: ic = Icon::Triangle; break;
        case Glyph::DPad: ic = Icon::DPad; scale = 1.3f; break;
        case Glyph::L1: ic = Icon::L1; scale = 1.4f; break;
        case Glyph::R1: ic = Icon::R1; scale = 1.4f; break;
        case Glyph::L2: ic = Icon::L2; scale = 1.4f; break;
        case Glyph::R2: ic = Icon::R2; scale = 1.4f; break;
        case Glyph::Options: ic = Icon::Options; scale = 1.4f; break;
        case Glyph::TouchPad: ic = Icon::TouchPad; scale = 1.4f; break;
        case Glyph::Create: ic = Icon::Create; scale = 1.4f; break;
        case Glyph::L3: ic = Icon::L3; scale = 1.4f; break;
        case Glyph::R3: ic = Icon::R3; scale = 1.4f; break;
        case Glyph::LStick: ic = Icon::LStick; break;
        case Glyph::RStick: ic = Icon::RStick; break;
        default: break;
        }
        if (ic != Icon::Count)
        {
            icon(ic, cx, cy, size * scale, c);
            return;
        }
    }
    if (int(g) >= int(Glyph::L1))
        g = Glyph::Cross; /* no atlas: any button will do */
    c = tone(c);
    const float px = target_w_ / kDesignW;
    Vertex v[4]{};
    float pos[4][4];
    corners_flat(cx - size * 0.5f, cy - size * 0.5f, size, size, pos);
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        std::memcpy(v[i].pos, pos[i], sizeof pos[i]);
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {c.r, c.g, c.b, c.a});
        fill(v[i].p0, {float(K_GLYPH), 0, 0, 0});
        fill(v[i].p1, {size * px, size * px, size * px, size * px});
        fill(v[i].p2, {0, 0, float(int(g)), rotation});
    }
    push(nullptr, v);
}

void Gfx::quad3d(Texture *t, const Corner c[4], float shape_w, float shape_h, Color tint, float radius,
                 bool reflection, bool flip_v, const float *uv_rect)
{
    if (!t)
        tint = tone(tint); /* a plain shape, not a picture */
    const float u0 = uv_rect ? uv_rect[0] : 0.0f, v0 = uv_rect ? uv_rect[1] : 0.0f;
    const float u1 = uv_rect ? uv_rect[2] : 1.0f, v1 = uv_rect ? uv_rect[3] : 1.0f;
    const float px = target_w_ / kDesignW;
    Vertex v[4]{};
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        to_clip(c[i], v[i].pos);
        const float ly = flip_v ? 1.0f - local[i][1] : local[i][1];
        fill(v[i].uv, {u0 + (u1 - u0) * local[i][0], v0 + (v1 - v0) * ly});
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {tint.r, tint.g, tint.b, tint.a});
        fill(v[i].p0, {float(reflection ? K_REFLECTION : K_COVER), radius * px, 0, 0});
        fill(v[i].p1, {shape_w * px, shape_h * px, shape_w * px, shape_h * px});
    }
    push(t, v);
}

void Gfx::panel3d(const Corner c[4], float shape_w, float shape_h, float margin, Color fill_c, float bottom_mul,
                  float radius, Color border, float border_w, float glow, float sheen)
{
    fill_c = tone(fill_c);
    border = tone(border);
    const float px = target_w_ / kDesignW;
    Vertex v[4]{};
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        to_clip(c[i], v[i].pos);
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {fill_c.r, fill_c.g, fill_c.b, fill_c.a});
        fill(v[i].p0, {float(K_PANEL), radius * px, border_w * px, glow * px});
        fill(v[i].p1, {(shape_w + margin * 2) * px, (shape_h + margin * 2) * px, shape_w * px, shape_h * px});
        fill(v[i].p2, {bottom_mul, sheen, 0, 0});
        fill(v[i].bcolor, {border.r, border.g, border.b, border.a});
    }
    push(nullptr, v);
}

void Gfx::glass(const Corner c[4], float shape_w, float shape_h, float margin, const Glass &g0)
{
    if (tone_)
    {
        /* The light look: glass is white and flat, rimmed; its thickness and
         * reflections are left out. */
        if (g0.face != 0)
            return;
        Color fill = rgba(0xFBFCFD, std::min(1.0f, std::max(g0.tint.a, 0.85f)) * g0.fade);
        Color rim = tone(g0.rim);
        rim.a *= g0.fade;
        const bool was = tone_;
        tone_ = false;
        panel3d(c, shape_w, shape_h, margin, fill, 0.97f, g0.radius, rim, std::max(2.0f, g0.rim_w), g0.glow * 0.6f, 0.35f);
        tone_ = was;
        return;
    }
    Glass g = g0;
    const float px = target_w_ / kDesignW;
    Vertex v[4]{};
    const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i)
    {
        to_clip(c[i], v[i].pos);
        fill(v[i].local, {local[i][0], local[i][1]});
        fill(v[i].color, {g.tint.r, g.tint.g, g.tint.b, g.tint.a});
        fill(v[i].p0, {float(K_GLASS), g.radius * px, g.rim_w * px, g.glow * px});
        fill(v[i].p1, {(shape_w + margin * 2) * px, (shape_h + margin * 2) * px, shape_w * px, shape_h * px});
        fill(v[i].p2, {g.phase, g.light, float(g.face), g.fade});
        fill(v[i].bcolor, {g.rim.r, g.rim.g, g.rim.b, g.rim.a});
    }
    push(nullptr, v);
}

const Gfx::GlyphInfo *Gfx::find(const FontData &f, std::uint32_t cp) const
{
    if (cp < 128)
        return f.glyphs[cp].present ? &f.glyphs[cp] : nullptr;
    const auto it = std::lower_bound(f.extra.begin(), f.extra.end(), cp,
                                     [](const std::pair<std::uint32_t, GlyphInfo> &e, std::uint32_t c) { return e.first < c; });
    if (it != f.extra.end() && it->first == cp)
        return &it->second;
    if (cjk_atlas_ && cp >= 0x2000)
    {
        const auto jt = std::lower_bound(cjk_.extra.begin(), cjk_.extra.end(), cp,
                                         [](const std::pair<std::uint32_t, GlyphInfo> &e, std::uint32_t c) {
                                             return e.first < c;
                                         });
        if (jt != cjk_.extra.end() && jt->first == cp)
            return &jt->second;
    }
    return f.glyphs['?'].present ? &f.glyphs['?'] : nullptr;
}

float Gfx::measure(Font font, float size, const std::string &s, float spacing) const
{
    const FontData &f = fonts_[int(font)];
    const float k = size / kBase;
    float w = 0;
    std::size_t i = 0;
    std::uint32_t prev = 0;
    while (i < s.size())
    {
        const std::uint32_t cp = next_codepoint(s, i);
        const GlyphInfo *g = find(f, cp);
        if (!g)
            continue;
        if (prev && f.info && !g->cjk)
            w += stbtt_GetCodepointKernAdvance(static_cast<stbtt_fontinfo *>(f.info), int(prev), int(cp)) *
                 f.scale * k;
        w += g->advance * k + spacing;
        prev = cp;
    }
    return s.empty() ? 0 : w - spacing;
}

float Gfx::text_mid(Font font, float size, float x, float cy, Color c, Align a, const std::string &s,
                    float spacing, float weight)
{
    const FontData &f = fonts_[int(font)];
    const float k = size / kBase;
    /* text() takes the top of the line box: baseline = top + ascent. */
    const float top = cy + f.cap * k * 0.5f - f.ascent * k;
    return text(font, size, x, top, c, a, s, spacing, weight);
}

float Gfx::line_height(Font font, float size) const
{
    const FontData &f = fonts_[int(font)];
    return (f.ascent - f.descent) * size / kBase;
}

float Gfx::text(Font font, float size, float x, float y, Color c, Align a, const std::string &s, float spacing,
                float weight)
{
    c = tone(c);
    const FontData &f = fonts_[int(font)];
    const float k = size / kBase;
    const float width = measure(font, size, s, spacing);
    if (a == Align::Center)
        x -= width * 0.5f;
    else if (a == Align::Right)
        x -= width;
    const float baseline = y + f.ascent * k;
    const float px = target_w_ / kDesignW;
    std::size_t i = 0;
    std::uint32_t prev = 0;
    float pen = x;
    while (i < s.size())
    {
        const std::uint32_t cp = next_codepoint(s, i);
        const GlyphInfo *g = find(f, cp);
        if (!g)
            continue;
        if (prev && f.info && !g->cjk)
            pen += stbtt_GetCodepointKernAdvance(static_cast<stbtt_fontinfo *>(f.info), int(prev), int(cp)) *
                   f.scale * k;
        if (g->w > 0)
        {
            const float gx = pen + g->xoff * k, gy = baseline + g->yoff * k;
            const float gw = g->w * k, gh = g->h * k;
            Vertex v[4]{};
            float pos[4][4];
            corners_flat(gx, gy, gw, gh, pos);
            const float uv[4][2] = {{g->u0, g->v0}, {g->u1, g->v0}, {g->u1, g->v1}, {g->u0, g->v1}};
            const float local[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (int n = 0; n < 4; ++n)
            {
                std::memcpy(v[n].pos, pos[n], sizeof pos[n]);
                fill(v[n].uv, {uv[n][0], uv[n][1]});
                fill(v[n].local, {local[n][0], local[n][1]});
                fill(v[n].color, {c.r, c.g, c.b, c.a});
                /* Japanese has one weight: bolder fonts thicken it. */
                const float w8 = g->cjk ? weight + 0.03f * float(int(font)) : weight;
                fill(v[n].p0, {float(K_TEXT), w8, 0, 0});
                fill(v[n].p1, {gw * px, gh * px, gw * px, gh * px});
            }
            push(g->cjk ? cjk_atlas_ : atlas_, v);
        }
        pen += g->advance * k + spacing;
        prev = cp;
    }
    return width;
}

void Gfx::record(VkCommandBuffer cmd)
{
    if (!device_ || vertices_.empty())
        return;
    std::memcpy(vmaps_[slot_], vertices_.data(), vertices_.size() * sizeof(Vertex));
    VkViewport viewport{0, 0, target_w_, target_h_, 0, 1};
    VkRect2D scissor{{0, 0}, {std::uint32_t(target_w_), std::uint32_t(target_h_)}};
    vkCmdSetViewport_(cmd, 0, 1, &viewport);
    vkCmdSetScissor_(cmd, 0, 1, &scissor);
    vkCmdBindPipeline_(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers_(cmd, 0, 1, &vbufs_[slot_], &offset);
    const float push_data[8] = {target_w_, target_h_, time_, dim_, reduced_motion_ ? 1.0f : 0.0f, 0, 0, 0};
    vkCmdPushConstants_(cmd, layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push_data, push_data);
    for (const Batch &b : batches_)
    {
        vkCmdBindDescriptorSets_(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &b.set, 0, nullptr);
        vkCmdDraw_(cmd, b.count, 1, b.first, 0);
    }
}
} // namespace porpoise::ui
