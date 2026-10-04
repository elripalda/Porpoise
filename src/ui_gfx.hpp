/* Porpoise UI - the renderer: quads, covers in perspective, SDF text, shader shapes.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Everything is laid out in a 1920x1080 design space and scaled to the
 * screen. The renderer is device-agnostic: it can live on Porpoise's own
 * Vulkan device (the launcher) and be rebuilt on the core's device (the
 * launch screen, overlays) - init() and shutdown() are the whole lifecycle. */
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace porpoise::ui
{
struct Color
{
    float r = 1, g = 1, b = 1, a = 1;
};
Color rgba(std::uint32_t hex, float alpha = 1.0f);

struct Texture
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    int width = 0, height = 0;
};

enum class Font
{
    Regular,
    SemiBold,
    Bold,
    ExtraBold,
};

enum class Align
{
    Left,
    Center,
    Right,
};

enum class Glyph
{
    Cross = 0,
    Circle = 1,
    Square = 2,
    Triangle = 3,
    Arrow = 4,   /* filled triangle, rotated */
    Pointer = 5, /* outlined triangle pointing up */
    DPad = 6,
    Star = 7,        /* filled: a favourite */
    StarOutline = 8, /* outlined */
    /* The DualSense's other controls, drawn from the button icons. */
    L1 = 10,
    R1 = 11,
    L2 = 12,
    R2 = 13,
    Options = 14,
    TouchPad = 15,
    Create = 16,
    L3 = 17,
    R3 = 18,
    LStick = 19,
    RStick = 20,
};

/* The button icons in assets/ui/buttons.png (Zacksly's "PS5 Button Icons and
 * Controls", CC BY 3.0, drawn by tools/make-controller-art.py), in its order. */
enum class Icon
{
    Cross, Circle, Square, Triangle,
    CrossSolid, CircleSolid, SquareSolid, TriangleSolid,
    L1, R1, L2, R2,
    Options, Create, TouchPad, DPad,
    DPadUp, DPadDown, DPadLeft, DPadRight,
    LStick, RStick, L3, R3,
    LStickAll, RStickAll,
    Count
};

struct GfxInit
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family = 0;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    unsigned slots = 2;
    PFN_vkGetInstanceProcAddr get_instance_proc = nullptr;
    PFN_vkGetDeviceProcAddr get_device_proc = nullptr;
    std::mutex *queue_mutex = nullptr; /* held around this renderer's own submits */
    std::string asset_dir;             /* fonts/ and brand/ live here */
};

/* How a piece of glass looks (Gfx::glass). */
struct Glass
{
    Color tint;          /* body */
    Color rim;           /* rim and glow */
    float radius = 14;
    float rim_w = 2;
    float glow = 0;
    float phase = 0;     /* where the room's reflections fall on it, e.g. its x / 1920 */
    float light = 1;     /* brightness of a side slice */
    int face = 0;        /* 0 front, 1 side slice, 2 gloss over a picture */
    float fade = 1;      /* the whole piece's opacity */
};

/* A screen-space corner after projection: what the vertex shader receives. */
struct Corner
{
    float x, y, w; /* design-space position and the perspective w */
};

class Gfx
{
public:
    bool init(const GfxInit &init);
    void shutdown();
    bool ready() const { return device_ != VK_NULL_HANDLE; }

    /* Textures. RGBA8, straight alpha. */
    Texture *texture_rgba(const std::uint8_t *pixels, int width, int height);
    /* PNG / JPG, halved until its longer side fits max_side. */
    Texture *texture_file(const std::string &path, int max_side = 1024);
    void free_texture(Texture *texture);
    Texture *brand_mask() const { return brand_mask_; }
    const std::string &asset_dir() const { return init_.asset_dir; }

    /* A frame: begin, draw, then record() into the render pass that is open
     * on cmd. Target size is the framebuffer in pixels. */
    void begin(unsigned slot, float target_w, float target_h, float time, float dim, bool reduced_motion);
    void record(VkCommandBuffer cmd);

    /* Everything drawn after this is moved by (dx, dy) design pixels and faded:
     * how a screen slides in. begin() resets it. */
    void set_layer(float dx = 0, float dy = 0, float fade = 1)
    {
        layer_dx_ = dx;
        layer_dy_ = dy;
        layer_fade_ = fade;
    }

    /* The light look (Revolution): colours given to the drawing calls below
     * are turned light for dark and dark for light, keeping their hue, so
     * the screens drawn for the dark look read on white. Pictures are left
     * alone. begin() turns it off; screens drawn for the light look keep it
     * off. */
    void set_tone(bool light) { tone_ = light; }
    bool toned() const { return tone_; }
    Color tone(Color c) const;

    /* Drawing, in design-space pixels (1920x1080). */
    void background();
    void panel(float x, float y, float w, float h, Color fill, float bottom_mul, float radius,
               Color border = {}, float border_w = 0, float glow = 0, float sheen = 0);
    void image(Texture *t, float x, float y, float w, float h, Color tint = {}, float radius = 0);
    void blob(float cx, float cy, float w, float h, Color c);
    void glyph(Glyph g, float cx, float cy, float size, Color c, float rotation = 0);
    /* A button icon in a size x size cell (the icons carry their own margin:
     * a face button's ring is about two thirds of the cell). */
    void icon(Icon i, float cx, float cy, float size, Color c);
    bool has_icons() const { return icons_ != nullptr; }
    /* Part of a texture: uv is u0 v0 u1 v1. */
    void image_part(Texture *t, float x, float y, float w, float h, const float uv[4], Color tint = {});
    /* A textured quad from four projected corners (top-left, top-right,
     * bottom-right, bottom-left); shape_w/h is the quad's own size before
     * projection, for rounded corners. reflection fades it out downward. */
    void quad3d(Texture *t, const Corner c[4], float shape_w, float shape_h, Color tint, float radius,
                bool reflection, bool flip_v, const float *uv_rect = nullptr /* u0 v0 u1 v1 */);
    void panel3d(const Corner c[4], float shape_w, float shape_h, float margin, Color fill, float bottom_mul,
                 float radius, Color border, float border_w, float glow, float sheen);
    /* Glass from four projected corners; margin is room around the shape for the glow. */
    void glass(const Corner c[4], float shape_w, float shape_h, float margin, const Glass &g);

    /* Text. y is the top of the line box; returns the advance width. */
    float text(Font f, float size, float x, float y, Color c, Align a, const std::string &s,
               float spacing = 0, float weight = 0);
    float measure(Font f, float size, const std::string &s, float spacing = 0) const;
    /* Text whose capital letters are centred on cy: what labels inside
     * buttons, tabs and rows need to look centred. */
    float text_mid(Font f, float size, float x, float cy, Color c, Align a, const std::string &s,
                   float spacing = 0, float weight = 0);
    float line_height(Font f, float size) const;

    float design_w() const { return 1920.0f; }
    float design_h() const { return 1080.0f; }

private:
    struct Vertex
    {
        float pos[4];
        float uv[2];
        float local[2];
        float color[4];
        float p0[4];
        float p1[4];
        float p2[4];
        float bcolor[4];
    };
    struct Batch
    {
        VkDescriptorSet set;
        std::uint32_t first, count;
    };
    struct GlyphInfo
    {
        bool present = false;
        float u0, v0, u1, v1;   /* atlas UVs */
        float xoff, yoff, w, h; /* at base size, pixels */
        float advance;
        bool cjk = false; /* in the Japanese font's atlas */
    };
    struct FontData
    {
        std::vector<unsigned char> ttf;
        float ascent = 0, descent = 0, line_gap = 0; /* at base size */
        float cap = 0;                               /* capital height at base size */
        GlyphInfo glyphs[128];
        /* Beyond ASCII: accented letters for Spanish, French and Portuguese,
         * and typographic marks; sorted by code point. */
        std::vector<std::pair<std::uint32_t, GlyphInfo>> extra;
        void *info = nullptr; /* stbtt_fontinfo */
        float scale = 1;
    };

    bool load_functions();
    bool create_pipeline();
    bool build_fonts();
    const GlyphInfo *find(const FontData &f, std::uint32_t cp) const;
    void push(Texture *t, const Vertex v[4]);
    void corners_flat(float x, float y, float w, float h, float out[4][4]) const;
    void to_clip(const Corner &c, float out[4]) const;
    std::uint32_t memory_type(std::uint32_t bits, VkMemoryPropertyFlags flags) const;
    bool make_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buffer, VkDeviceMemory &memory,
                     void **mapped);
    Texture *upload(const std::uint8_t *pixels, int width, int height);
    void destroy_texture(Texture *t);

    GfxInit init_{};
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkCommandPool upload_pool_ = VK_NULL_HANDLE;
    std::vector<VkBuffer> vbufs_;
    std::vector<VkDeviceMemory> vmems_;
    std::vector<void *> vmaps_;
    std::size_t vcapacity_ = 0; /* vertices per slot */

    bool tone_ = false;
    Texture *white_ = nullptr;
    Texture *atlas_ = nullptr;
    Texture *icons_ = nullptr;
    Texture *brand_mask_ = nullptr;
    std::vector<Texture *> textures_;
    std::vector<std::pair<Texture *, std::uint64_t>> graveyard_; /* freed, waiting for the GPU */
    std::uint64_t frame_no_ = 0;
    FontData fonts_[4];
    std::vector<std::uint8_t> atlas_pixels_; /* kept across device changes */
    /* Japanese: one weight of Noto Sans JP, subset to the characters the menus
     * use, in an atlas of its own; any font falls back to it. */
    FontData cjk_;
    Texture *cjk_atlas_ = nullptr;
    std::vector<std::uint8_t> cjk_pixels_;
    bool fonts_built_ = false;

    unsigned slot_ = 0;
    float target_w_ = 1920, target_h_ = 1080;
    float time_ = 0, dim_ = 0;
    float layer_dx_ = 0, layer_dy_ = 0, layer_fade_ = 1;
    bool reduced_motion_ = false;
    std::vector<Vertex> vertices_;
    std::vector<Batch> batches_;
    bool overflowed_ = false;

#define PORPOISE_UI_VK_FUNCS(X)                                                                  \
    X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkAllocateMemory)    \
    X(vkFreeMemory) X(vkBindBufferMemory) X(vkMapMemory) X(vkUnmapMemory) X(vkCreateImage)       \
    X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) X(vkCreateImageView)  \
    X(vkDestroyImageView) X(vkCreateSampler) X(vkDestroySampler) X(vkCreateDescriptorSetLayout) \
    X(vkDestroyDescriptorSetLayout) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool)        \
    X(vkAllocateDescriptorSets) X(vkFreeDescriptorSets) X(vkUpdateDescriptorSets)               \
    X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) X(vkCreateShaderModule)                \
    X(vkDestroyShaderModule) X(vkCreateGraphicsPipelines) X(vkDestroyPipeline)                  \
    X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers)                  \
    X(vkFreeCommandBuffers) X(vkBeginCommandBuffer) X(vkEndCommandBuffer)                        \
    X(vkCmdPipelineBarrier) X(vkCmdCopyBufferToImage) X(vkQueueSubmit) X(vkQueueWaitIdle)       \
    X(vkCmdBindPipeline) X(vkCmdBindVertexBuffers) X(vkCmdBindDescriptorSets)                   \
    X(vkCmdPushConstants) X(vkCmdSetViewport) X(vkCmdSetScissor) X(vkCmdDraw)                    \
    X(vkDeviceWaitIdle)
#define PORPOISE_UI_DECLARE(name) PFN_##name name##_ = nullptr;
    PORPOISE_UI_VK_FUNCS(PORPOISE_UI_DECLARE)
#undef PORPOISE_UI_DECLARE
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties_ = nullptr;
};
} // namespace porpoise::ui
