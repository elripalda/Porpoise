/* Porpoise UI preview: renders the launcher screens offscreen (any Vulkan
 * driver, e.g. lavapipe) and writes PNGs, to check the UI without a console.
 *
 *   preview <assets dir> <covers dir> <out dir>
 */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <unistd.h>

#include <sys/stat.h>

#include "porpoise_borders.hpp"
#include "porpoise_states.hpp"
#include "porpoise_banner.hpp"
#include "ui_recommend.hpp"
#include "ui_setups.hpp"
#include "ui_app.hpp"
#include "ui_widescreen.hpp"
#include "ui_i18n.hpp"
#include "ui_gfx.hpp"
#include "ui_library.hpp"
#include "ui_settings.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char *);

using namespace porpoise::ui;

#define GI(name) auto name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name))
#define GD(name) auto name = reinterpret_cast<PFN_##name>(gdpa(device, #name))

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::fprintf(stderr, "usage: preview <assets> <covers> <out>\n");
        return 2;
    }
    const std::string assets = argv[1], covers = argv[2], out = argv[3];
    const std::string saves = argc > 4 ? argv[4] : out;
    const unsigned W = 1920, H = 1080;

    VkInstance instance = VK_NULL_HANDLE;
    auto vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    if (vkCreateInstance(&ici, nullptr, &instance) != VK_SUCCESS)
        return 1;
    GI(vkEnumeratePhysicalDevices);
    GI(vkCreateDevice);
    GI(vkGetDeviceProcAddr);
    GI(vkGetPhysicalDeviceMemoryProperties);
    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    std::uint32_t n = 1;
    vkEnumeratePhysicalDevices(instance, &n, &gpu);
    const float prio = 1;
    VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    q.queueCount = 1;
    q.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &q;
    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(gpu, &dci, nullptr, &device) != VK_SUCCESS)
        return 1;
    PFN_vkGetDeviceProcAddr gdpa = vkGetDeviceProcAddr;
    GD(vkGetDeviceQueue);
    GD(vkCreateImage);
    GD(vkGetImageMemoryRequirements);
    GD(vkAllocateMemory);
    GD(vkBindImageMemory);
    GD(vkCreateImageView);
    GD(vkCreateRenderPass);
    GD(vkCreateFramebuffer);
    GD(vkCreateCommandPool);
    GD(vkAllocateCommandBuffers);
    GD(vkBeginCommandBuffer);
    GD(vkEndCommandBuffer);
    GD(vkCmdBeginRenderPass);
    GD(vkCmdEndRenderPass);
    GD(vkCmdCopyImageToBuffer);
    GD(vkQueueSubmit);
    GD(vkQueueWaitIdle);
    GD(vkCreateBuffer);
    GD(vkGetBufferMemoryRequirements);
    GD(vkBindBufferMemory);
    GD(vkMapMemory);
    GD(vkResetCommandBuffer);
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, 0, 0, &queue);

    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &mp);
    auto memtype = [&](std::uint32_t bits, VkMemoryPropertyFlags f) {
        for (std::uint32_t i = 0; i < mp.memoryTypeCount; ++i)
            if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & f) == f)
                return i;
        return 0u;
    };

    const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent = {W, H, 1};
    ii.mipLevels = ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage image;
    vkCreateImage(device, &ii, nullptr, &image);
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memtype(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkDeviceMemory imem;
    vkAllocateMemory(device, &ai, nullptr, &imem);
    vkBindImageMemory(device, image, imem, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view;
    vkCreateImageView(device, &vi, nullptr, &view);

    VkAttachmentDescription att{};
    att.format = format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkRenderPassCreateInfo rpi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpi.attachmentCount = 1;
    rpi.pAttachments = &att;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sub;
    VkRenderPass rp;
    vkCreateRenderPass(device, &rpi, nullptr, &rp);
    VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbi.renderPass = rp;
    fbi.attachmentCount = 1;
    fbi.pAttachments = &view;
    fbi.width = W;
    fbi.height = H;
    fbi.layers = 1;
    VkFramebuffer fb;
    vkCreateFramebuffer(device, &fbi, nullptr, &fb);

    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = VkDeviceSize(W) * H * 4;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer readback;
    vkCreateBuffer(device, &bi, nullptr, &readback);
    vkGetBufferMemoryRequirements(device, readback, &req);
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memtype(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory rmem;
    vkAllocateMemory(device, &ai, nullptr, &rmem);
    vkBindBufferMemory(device, readback, rmem, 0);
    void *mapped = nullptr;
    vkMapMemory(device, rmem, 0, VK_WHOLE_SIZE, 0, &mapped);

    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool;
    vkCreateCommandPool(device, &cpi, nullptr, &pool);
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = pool;
    cai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cai, &cmd);

    GfxInit gi;
    gi.instance = instance;
    gi.gpu = gpu;
    gi.device = device;
    gi.queue = queue;
    gi.queue_family = 0;
    gi.render_pass = rp;
    gi.slots = 1;
    gi.get_instance_proc = vkGetInstanceProcAddr;
    gi.get_device_proc = gdpa;
    gi.asset_dir = assets;
    gi.cjk_also = porpoise::ui::language_names();
    if (const char *cache = std::getenv("PREVIEW_CACHE"))
        gi.cache_dir = cache; /* the baked text atlases, as Porpoise keeps them */
    Gfx gfx;
    /* Covers decode on a worker thread in Porpoise; here, in place, so every
     * scene shows them (PREVIEW_ASYNC=1 to see the worker's way). */
    const bool async = std::getenv("PREVIEW_ASYNC") != nullptr;
    gfx.set_async_loads(async);
    if (!gfx.init(gi))
    {
        std::fprintf(stderr, "gfx init failed\n");
        return 1;
    }

    /* A sample library with invented games (covers generated by the preview script). */
    Library lib;
    LibraryPaths lp;
    lp.covers = covers;
    lp.state = out + "/library-state.txt";
    lib.scan(lp);
    const char *titles[][3] = {
        {"PRVW01", "Island Adventure", "GameCube"}, {"PRVW02", "Comet Racers", "GameCube"},
        {"PRVW03", "Haunted Manor", "GameCube"},    {"PRVW04", "Sunny Shores", "GameCube"},
        {"PRVW05", "Sky Sail", "GameCube"},         {"PRVW06", "Mech Hunter", "GameCube"},
        {"PRVW07", "Kart Party", "GameCube"},       {"PRVW08", "Garden Critters", "GameCube"},
        {"NOCOVR", "A Game Without Cover Art", "GameCube"},
        {"PRVW09", "Tiny Tennis", "Wii"},           {"PRVW10", "Bowling Night", "Wii"},
        {"PRVW11", "Puzzle Lab", "GameCube"},       {"PRVW12", "Star Pilots", "Wii"},
        {"PRVW13", "Rhythm Farm", "Wii"},
    };
    for (auto &t : titles)
    {
        Game g;
        g.id = t[0];
        g.title = t[1];
        g.platform = t[2];
        g.file = std::string(t[1]) + ".rvz";
        g.format = "RVZ";
        g.region = "USA";
        g.bytes = 1400ull << 20;
        lib.games().push_back(g);
    }
    lib.sort(lib.sort_order()); /* as a scan leaves it: Sort & filter's count too */
    /* Two Wii games are a disc with a real tile and banner (made by
     * tools/ui-preview from an open-source channel's layout). */
    for (Game &g : lib.games())
    {
        if (g.title == "Bowling Night" || g.title == "Star Pilots")
            g.path = covers + "/../discs/banner-test.rvz";
        if (g.title == "Tiny Tennis")
            g.path = covers + "/../discs/banner-test.iso";
    }
    std::system(("rm -rf '" + out + "/banners'").c_str());
    porpoise::banner::set_cache_dir(out + "/banners");
    lib.games()[3].last_played = (long long)std::time(nullptr) - 30 * 3600;
    for (Game &g : lib.games())
        if (g.id == "PRVW03")
        {
            g.developer = "Lantern Works";
            g.publisher = "Porpoise Preview Co.";
            g.released = "October 31, 2003";
            g.genre = "Adventure, Puzzle";
            g.players = 1;
            g.rating = "ESRB T";
            g.synopsis = "A storm strands you at the gates of an old manor where every room hides a puzzle and every "
                         "portrait seems to follow you. Gather the scattered keys, calm sixty-four restless ghosts and "
                         "find out who still lives on the top floor before the clock strikes midnight.";
        }
    lib.sort(Library::Sort::Title);

    porpoise::Settings settings;
    App ui;
    ui.init(&gfx, &lib, &settings, out + "/settings.ini", out + "/options.ini", saves);
    ui.set_sys_dir(std::string(getenv("HOME")) + "/porpoise-release/build/cores/stage/system/dolphin-emu/Sys");
    /* Two save states for the 4th game, with a cover for their pictures. */
    porpoise::states::set_data_dir(out);
    porpoise::borders::set_dirs(assets, out);
    porpoise::ui::setups::set_dir(out);
    porpoise::ui::setups::save(0, settings, "Kart Party");
    {
        /* A Dolphin fix and a Porpoise pick for the preview's games (PRV...). */
        mkdir((out + "/gs").c_str(), 0777);
        if (std::FILE *f = std::fopen((out + "/gs/PRV.ini").c_str(), "w"))
        {
            std::fputs("# PRVW01 - Preview\n[Video_Hacks]\n# Fixes the entrance videos.\nEFBToTextureEnable = False\n"
                       "[Video_Settings]\nSafeTextureCacheColorSamples = 0\n", f);
            std::fclose(f);
        }
        if (std::FILE *f = std::fopen((out + "/recommended.ini").c_str(), "w"))
        {
            std::fputs("[PRV]\nnote = Smoother on PS5 with these.\nresolution = 2\n"
                       "dolphin.Video_Hacks.EFBToTextureEnable = True\n"
                       "dolphin.Video_Settings.EnableGPUTextureDecoding = True\n", f);
            std::fclose(f);
        }
        porpoise::ui::recommend::set_paths(out + "/recommended.ini", out + "/gs");
    }
    if (lib.games().size() > 3)
    {
        const std::string dir = out + "/states/" + Library::key_of(lib.games()[3]);
        mkdir((out + "/states").c_str(), 0777);
        mkdir(dir.c_str(), 0777);
        for (int slot : {1, 3})
        {
            if (std::FILE *f = std::fopen((dir + "/slot" + std::to_string(slot) + ".state").c_str(), "wb"))
            {
                std::fputs("preview", f);
                std::fclose(f);
            }
            const std::string from = covers + (slot == 1 ? "/PRVW07.png" : "/PRVW03.png");
            if (std::FILE *in = std::fopen(from.c_str(), "rb"))
            {
                if (std::FILE *o = std::fopen((dir + "/slot" + std::to_string(slot) + ".png").c_str(), "wb"))
                {
                    char buf[8192];
                    std::size_t n;
                    while ((n = std::fread(buf, 1, sizeof buf, in)) > 0)
                        std::fwrite(buf, 1, n, o);
                    std::fclose(o);
                }
                std::fclose(in);
            }
        }
    }

    auto render = [&](const char *name, auto &&draw) {
        gfx.begin(0, float(W), float(H), 12.0f, 0.0f, false);
        draw();
        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cmd, &b);
        gfx.record_uploads(cmd); /* streamed textures (Wii banners) */
        VkClearValue clear{};
        VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rbi.renderPass = rp;
        rbi.framebuffer = fb;
        rbi.renderArea.extent = {W, H};
        rbi.clearValueCount = 1;
        rbi.pClearValues = &clear;
        vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
        gfx.record(cmd);
        vkCmdEndRenderPass(cmd);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {W, H, 1};
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &copy);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        auto *px = static_cast<unsigned char *>(mapped);
        for (std::size_t i = 0; i < std::size_t(W) * H; ++i)
            px[i * 4 + 3] = 255;
        const std::string path = out + "/" + name + ".png";
        stbi_write_png(path.c_str(), int(W), int(H), 4, px, int(W * 4));
        std::printf("wrote %s\n", path.c_str());
    };

    /* Library, with the 4th game selected (settle the slide first). */
    Input none;
    Input right;
    right.held = 1u << 3;
    for (int i = 0; i < 3; ++i)
    {
        ui.update(right, 0.016);
        ui.update(none, 0.016);
    }
    for (int i = 0; i < 200; ++i)
        ui.update(none, 0.016);
    render("library", [&] { ui.draw(12.0); });
    /* The sandbox help, as porpoise_main shows it: it must fit its dialog. */
    ui.show_message(porpoise::ui::tr("Porpoise can't reach /data"),
                    porpoise::ui::tr("The console started Porpoise inside the app sandbox, so it can't see "
                                     "/data or USB drives, and no jailbreak daemon freed it. A daemon that "
                                     "frees Porpoise must be running before you open it:\n"
                                     "\xE2\x80\xA2 etaHEN: turn on Legacy Command Server in etaHEN's Toolbox "
                                     "settings (etaHEN's built-in app list can't be edited to add Porpoise).\n"
                                     "\xE2\x80\xA2 OnionHEN: add PPSA99764 to exact_title_ids in "
                                     "/data/OnionHEN/config.ini.\n"
                                     "\xE2\x80\xA2 Or run a standalone daemon such as Lapy.\n"
                                     "Then open Porpoise again; if a launch still lands here, try once more. "
                                     "Until then, games go in /app0/porpoise/games."));
    for (int i = 0; i < 30; ++i)
        ui.update(none, 0.016);
    render("sandbox-help", [&] { ui.draw(12.0); });
    {
        Input in;
        in.held = 1u << 4; /* Cross closes it */
        ui.update(in, 0.016);
        ui.update(none, 0.016);
    }

    /* A button press: down for a frame, then up; then let animations settle. */
    auto press = [&](std::uint32_t bit, int times = 1) {
        for (int i = 0; i < times; ++i)
        {
            Input in;
            in.held = bit;
            ui.update(in, 0.016);
            ui.update(none, 0.016);
        }
    };
    auto settle = [&] {
        for (int i = 0; i < 90; ++i)
            ui.update(none, 0.016);
    };
    const std::uint32_t kDown = 1u << 1, kRight = 1u << 3, kCross = 1u << 4, kCircle = 1u << 5,
                        kSquare = 1u << 6, kTriangle = 1u << 7, kR1 = 1u << 9;

    /* PREVIEW_27=1: 2.7's Apply: Settings with changes waiting, the question
     * on leaving, the restart question; the in-game menu the same way; the
     * cover's glide into the launch screen; then stop. */
    if (std::getenv("PREVIEW_27"))
    {
        if (const char *lang = std::getenv("PREVIEW_PLANG"))
            porpoise::ui::apply_language(std::atoi(lang), "");
        /* The glide: the library, then the entrance at a few moments. */
        settle();
        render("v27-glide-0", [&] { ui.draw(12.0); });
        porpoise::ui::Game *sel = nullptr;
        for (auto &gm : lib.games())
            if (gm.cover && !sel)
                sel = &gm;
        bool begun = false;
        for (double at : {0.08, 0.2, 0.32, 0.45, 0.7})
        {
            char name[32];
            std::snprintf(name, sizeof name, "v27-glide-%03d", int(at * 1000));
            render(name, [&] {
                if (!begun)
                {
                    begun = true;
                    ui.begin_launch(sel ? sel : &lib.games()[0]);
                    ui.preview_launch_start(12.0);
                }
                if (ui.launch_intro(12.0 + at) < 1.0f)
                    ui.draw(12.0 + at);
                ui.draw_launch(12.0 + at);
            });
        }
        ui.return_from_game();
        settle();
        /* Settings: two changes waiting. */
        press(kR1, 2);
        settle();
        press(kRight); /* into Games */
        press(kRight); /* Find games automatically: off */
        press(kDown);
        press(kRight); /* Download covers: off */
        settle();
        render("v27-settings-pending", [&] { ui.draw(12.0); });
        press(kCircle); /* to the sections */
        press(kCircle); /* leaving: Apply or Discard? */
        settle();
        render("v27-settings-ask", [&] { ui.draw(12.0); });
        press(kCircle); /* keep editing */
        settle();
        press(kSquare); /* Apply */
        settle();
        render("v27-settings-applied", [&] { ui.draw(12.0); });
        press(kDown, 1); /* Video */
        press(kRight);
        press(kRight); /* Output resolution */
        press(kSquare);
        settle();
        render("v27-settings-restart", [&] { ui.draw(12.0); });
        press(kCircle);
        /* In a game: a change waiting, then the question on Resume. */
        porpoise::ui::Game &g0 = lib.games()[0];
        Texture *frame = gfx.texture_file(covers + "/PRVW07.png");
        auto shot = [&](const char *name) {
            render(name, [&] {
                gfx.background();
                if (frame)
                    gfx.image(frame, 0, 0, 1920, 1080, rgba(0xFFFFFF));
                ui.draw_game_menu(12.0);
            });
        };
        auto tap = [&](std::uint32_t b, int times = 1) {
            for (int t = 0; t < times; ++t)
            {
                Input in;
                in.held = b;
                ui.update_game_menu(in, 0.016);
                for (int i = 0; i < 12; ++i)
                    ui.update_game_menu(none, 0.016);
            }
        };
        porpoise::Settings play = settings;
        ui.open_game_menu(&g0, &play);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        tap(kR1, 2); /* Graphics */
        tap(kRight); /* Shader compilation */
        shot("v27-ingame-pending");
        tap(kCircle);
        shot("v27-ingame-ask");
        tap(kCross); /* Apply: the shaders take effect at the next start */
        shot("v27-ingame-restart");
        /* A game's settings: its home screen tile. */
        ui.return_from_game();
        settle();
        ui.preview_game_settings(3);
        settle();
        press(kDown, 2); /* Home screen (after Recommended) */
        press(kRight);
        settle();
        render("v27-home-screen", [&] { ui.draw(12.0); });
        press(kDown, 3);
        settle();
        render("v27-home-screen-add", [&] { ui.draw(12.0); });
        return 0;
    }
    /* PREVIEW_TILE=1: the tile art editor: the cover whole over its blur, the
     * background's pattern, a screenshot filling it, the picture picker. */
    if (std::getenv("PREVIEW_TILE"))
    {
        namespace art = porpoise::tileart;
        art::set_asset_dir(argv[1]);
        if (const char *lang = std::getenv("PREVIEW_PLANG"))
            porpoise::ui::apply_language(std::atoi(lang), "");
        settle();
        ui.preview_game_settings(3);
        settle();
        press(kDown, 2); /* Home screen */
        press(kRight);
        settle();
        press(kCross); /* Tile art: Edit */
        settle();
        render("tile-1-icon", [&] { ui.draw(12.0); });
        art::Spec d = art::defaults();
        art::Layer l = d.bg;
        l.behind = art::Pattern;
        l.x = 0.5f;
        ui.preview_tile_art(1, 2, l);
        settle();
        render("tile-2-bg-pattern", [&] { ui.draw(12.0); });
        const char *snap = std::getenv("PREVIEW_SNAP");
        l = d.bg;
        l.source = art::File;
        l.file = snap ? snap : "";
        l.fit = art::Fill;
        art::reset_position(l, true);
        ui.preview_tile_art(1, 1, l);
        art::Layer i = d.icon;
        i.fit = art::Fill;
        i.source = art::File;
        i.file = l.file;
        i.zoom = 1.5f;
        ui.preview_tile_art(0, 1, i);
        settle();
        render("tile-3-fill", [&] { ui.draw(12.0); });
        ui.preview_tile_art(1, 0, l);
        press(kCross); /* the picture picker */
        settle();
        render("tile-4-picker", [&] { ui.draw(12.0); });
        return 0;
    }
    /* PREVIEW_ACH=1: a game's achievements on Details, its page, and the
     * in-game menu's Achievements tab; then stop. */
    if (std::getenv("PREVIEW_ACH"))
    {
        static porpoise::ui::AchievementSet fake;
        fake.game_id = 11;
        fake.title = "Garden Critters";
        fake.badge_url = "https://media.retroachievements.org/Images/000001.png";
        const char *titles[] = {"First Steps", "Bug Collector", "Night Owl", "Speed Runner", "Perfectionist",
                                "Secret Garden", "Rainy Day", "Full House", "Long Haul", "Last Bloom"};
        const char *descs[] = {"Finish the first garden.", "Catch 50 critters in one day.",
                               "Play through a whole night without resting.", "Clear the meadow in under 3 minutes.",
                               "Get a perfect score on every garden.", "Find the hidden gate behind the old oak.",
                               "Catch a critter in the rain.", "Fill every jar on the shelf.",
                               "Play for 10 hours.", "See the last flower bloom."};
        mkdir((out + "/achievements").c_str(), 0777);
        mkdir((out + "/achievements/badges").c_str(), 0777);
        for (int i = 0; i < 10; ++i)
        {
            porpoise::ui::Achievement a;
            a.id = unsigned(100 + i);
            a.points = (i % 4 + 1) * 5;
            a.unlocked = i < 4;
            a.unlock_time = a.unlocked ? 1759700000LL - i * 86400 : 0;
            a.type = i == 5 ? 1 : 0;
            a.rarity = 42.5f - float(i) * 3.7f;
            a.progress = i == 6 ? "3/10" : "";
            a.title = titles[i];
            a.description = descs[i];
            a.badge_url = "https://media.retroachievements.org/Badge/" + std::to_string(5000 + i) + ".png";
            fake.list.push_back(a);
            const std::string from = covers + "/PRVW0" + std::to_string(1 + i % 8) + ".png";
            const std::string to = out + "/achievements/badges/" + porpoise::ui::badge_file(a.badge_url);
            std::system(("cp '" + from + "' '" + to + "'").c_str());
        }
        std::system(("cp '" + covers + "/PRVW02.png' '" + out + "/achievements/badges/000001.png'").c_str());
        fake.total = 10;
        fake.unlocked = 4;
        fake.points_total = 125;
        fake.points = 50;
        ui.set_achievement_source([](const std::string &, bool) { return fake; });
        settle();
        press(kSquare); /* Details */
        settle();
        render("ach-details", [&] { ui.draw(12.0); });
        press(kSquare); /* the achievements page */
        settle();
        render("ach-page", [&] { ui.draw(12.0); });
        press(kDown, 5);
        settle();
        render("ach-page-locked", [&] { ui.draw(12.0); });
        press(kCircle);
        press(kCircle);
        settle();
        porpoise::Settings play = settings;
        ui.open_game_menu(&lib.games()[0], &play);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        Input l1;
        l1.held = 1u << 8; /* L1: round to the last tab */
        ui.update_game_menu(l1, 0.016);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        Texture *frame = gfx.texture_file(covers + "/PRVW07.png");
        render("ach-ingame", [&] {
            gfx.background();
            if (frame)
                gfx.image(frame, 0, 0, 1920, 1080, rgba(0xFFFFFF));
            ui.draw_game_menu(12.0);
        });
        return 0;
    }
    /* PREVIEW_PATCHES=1: the in-game menu's Patches tab for a game with codes
     * (Super Mario Sunshine's ID, Dolphin's own lists and the widescreen
     * collection), scrolled, and the Video tab's Widescreen row; then stop. */
    if (std::getenv("PREVIEW_PATCHES"))
    {
        const char *root = std::getenv("PREVIEW_ROOT");
        const std::string base = root ? root : "/root/porpoise-release";
        ui.set_sys_dir(base + "/.deps/dolphin-src/Data/Sys");
        porpoise::ui::widescreen::set_dir(base + "/assets/widescreen");
        if (const char *lang = std::getenv("PREVIEW_PLANG"))
            porpoise::ui::apply_language(std::atoi(lang), "");
        porpoise::ui::Game &g0 = lib.games()[0];
        g0.id = std::getenv("PREVIEW_GAMEID") ? std::getenv("PREVIEW_GAMEID") : "GMSE01";
        g0.platform = "GameCube";
        porpoise::Settings play = settings;
        play.ws_plan = 1;
        ui.open_game_menu(&g0, &play);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        Texture *frame = gfx.texture_file(covers + "/PRVW07.png");
        auto shot = [&](const char *name) {
            render(name, [&] {
                gfx.background();
                if (frame)
                    gfx.image(frame, 0, 0, 1920, 1080, rgba(0xFFFFFF));
                ui.draw_game_menu(12.0);
            });
        };
        auto tap = [&](std::uint32_t b, int times = 1) {
            for (int t = 0; t < times; ++t)
            {
                Input in;
                in.held = b;
                ui.update_game_menu(in, 0.016);
                for (int i = 0; i < 12; ++i)
                    ui.update_game_menu(none, 0.016);
            }
        };
        tap(kR1); /* Video */
        tap(kDown);
        shot("patches-video");
        tap(kR1, 2); /* Audio */
        shot("patches-audio");
        tap(kDown, 2);
        tap(kRight); /* Responsive */
        shot("patches-audio-preset");
        tap(kR1, 2); /* Patches */
        shot("patches-tab");
        tap(kDown, 2);
        shot("patches-code");
        tap(kDown, 9);
        shot("patches-scrolled");
        return 0;
    }
    /* PREVIEW_25=1: 2.5's rows: a game's graphics mods (Super Mario Sunshine),
     * the Controls tab's extras, a Wii game with the GameCube controller, and
     * Settings > Controls; then stop. */
    if (std::getenv("PREVIEW_25"))
    {
        if (const char *lang = std::getenv("PREVIEW_PLANG"))
            porpoise::ui::apply_language(std::atoi(lang), "");
        porpoise::ui::Game &g0 = lib.games()[0];
        Texture *frame = gfx.texture_file(covers + "/PRVW07.png");
        auto shot = [&](const char *name) {
            render(name, [&] {
                gfx.background();
                if (frame)
                    gfx.image(frame, 0, 0, 1920, 1080, rgba(0xFFFFFF));
                ui.draw_game_menu(12.0);
            });
        };
        auto tap = [&](std::uint32_t b, int times = 1) {
            for (int t = 0; t < times; ++t)
            {
                Input in;
                in.held = b;
                ui.update_game_menu(in, 0.016);
                for (int i = 0; i < 12; ++i)
                    ui.update_game_menu(none, 0.016);
            }
        };
        g0.id = "GMSE01";
        g0.platform = "GameCube";
        porpoise::Settings play = settings;
        play.ws_plan = 1;
        ui.open_game_menu(&g0, &play);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        tap(kR1, 2); /* Graphics */
        tap(kDown, 12);
        shot("v25-graphics-sunshine");
        tap(kR1, 2); /* Controls */
        tap(kDown, 4);
        shot("v25-controls-gamecube");
        g0.id = "RSBE01";
        g0.platform = "Wii";
        porpoise::Settings wii_play = settings;
        wii_play.wii_controller = porpoise::pad::WiiGameCube;
        ui.open_game_menu(&g0, &wii_play);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        tap(kR1, 4); /* Controls */
        shot("v25-controls-brawl-gamecube");
        g0.id = "SX3P01";
        porpoise::Settings tower = settings;
        ui.open_game_menu(&g0, &tower);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        tap(kR1, 2); /* Graphics */
        tap(kDown, 12);
        shot("v25-graphics-pandoras-tower");
        return 0;
    }
    /* PREVIEW_ACCOUNT=1: the RetroAchievements panel signed out, typing, busy
     * with an error, and signed in; then stop. */
    if (std::getenv("PREVIEW_ACCOUNT"))
    {
        static porpoise::ui::App::RaState ra;
        ui.set_ra([] { return ra; }, [](const std::string &, const std::string &) { return true; }, [] {});
        settle();
        ui.preview_account(false, 0, 1, 0);
        ui.preview_account(false, 2, 1, 0);
        settle();
        render("account-out", [&] { ui.draw(12.0); });
        ui.preview_account(true, 1, 2, 3);
        settle();
        render("account-typing", [&] { ui.draw(12.0); });
        ra.message = "Wrong username or password.";
        ui.preview_account(false, 2, 1, 0);
        settle();
        render("account-error", [&] { ui.draw(12.0); });
        ra.message.clear();
        ra.signed_in = true;
        ra.user = "Ripalda";
        ra.points = 1250;
        ui.preview_account(false, 0, 1, 0);
        settle();
        render("account-in", [&] { ui.draw(12.0); });
        return 0;
    }
    /* PREVIEW_WIIMAP=1: Customize buttons on each controller's tab; then stop. */
    if (std::getenv("PREVIEW_WIIMAP"))
    {
        settle();
        for (int kind = 0; kind < 5; ++kind)
        {
            ui.preview_mapping(kind, kind == 0 ? 1 : 2);
            settle();
            render(("wiimap-" + std::to_string(kind)).c_str(), [&] { ui.draw(12.0); });
        }
        settings.wii_buttons[0][0] = 2, settings.wii_buttons[0][2] = 0; /* Cross and Square swapped */
        ui.preview_mapping(1, 0);
        settle();
        render("wiimap-custom", [&] { ui.draw(12.0); });
        return 0;
    }
    /* PREVIEW_ABOUT=1: Settings > About on the Discord row and on the
     * creator's row; then stop. */
    if (std::getenv("PREVIEW_ABOUT"))
    {
        settle();
        press(kR1, 2);
        settle();
        press(kDown, 9); /* About, the last section */
        press(kRight);
        press(kDown, 3); /* Discord */
        settle();
        render("about-discord", [&] { ui.draw(12.0); });
        press(kDown, 3); /* Created by */
        settle();
        render("about-creator", [&] { ui.draw(12.0); });
        return 0;
    }
    /* PREVIEW_WELCOME=1: the first start's theme picker on each look, the
     * welcome after it and Settings > Interface's last rows; then stop. */
    if (std::getenv("PREVIEW_WELCOME"))
    {
        settings.ui_theme = 0;
        ui.language_changed();
        settle();
        ui.start_welcome();
        settle();
        render("welcome-0", [&] { ui.draw(12.0); });
        press(kRight);
        settle();
        while (gfx.theme_fonts_busy())
        {
            settle();
            usleep(10000);
        }
        settle();
        render("welcome-1", [&] { ui.draw(12.0); });
        press(kRight);
        settle();
        render("welcome-2", [&] { ui.draw(12.0); });
        press(1u << 2); /* left */
        press(1u << 2);
        settle();
        press(kCross);
        settle();
        render("welcome-done", [&] { ui.draw(12.0); });
        return 0;
    }
    /* PREVIEW_STARCUBE=palette: Star Cube's home turned to each edge, then
     * each page and a game's page; then stop. */
    if (const char *sc = std::getenv("PREVIEW_STARCUBE"))
    {
        settings.ui_theme = 14;
        settings.ui_palette = std::atoi(sc);
        if (const char *e = std::getenv("PREVIEW_SCGAMES"))
            settings.sc_games = std::atoi(e);
        if (const char *e = std::getenv("PREVIEW_SCLANG"))
        {
            settings.ui_language = std::atoi(e);
            porpoise::ui::apply_language(settings.ui_language);
        }
        ui.language_changed();
        settle();
        while (gfx.theme_fonts_busy() || gfx.cjk_busy())
        {
            settle();
            usleep(10000);
        }
        const char *names[4] = {"games", "calendar", "cards", "settings"};
        for (int face = 0; face < 4; ++face)
        {
            ui.preview_starcube(face, false);
            settle();
            render((std::string("sc-home-") + names[face]).c_str(), [&] { ui.draw(12.0); });
        }
        for (int face = 0; face < 4; ++face)
        {
            ui.preview_starcube(face, true);
            for (int wait = 0; wait < 40; ++wait)
            {
                settle();
                usleep(10000);
            }
            if (face == 3)
            {
                press(1u << 0, 12);
                press(kDown, 7); /* Interface */
                press(kRight);
                settle();
            }
            render((std::string("sc-page-") + names[face]).c_str(), [&] { ui.draw(12.0); });
        }
        ui.preview_starcube(0, true);
        settle();
        press(kRight);
        press(kCross);
        settle();
        render("sc-page-game", [&] { ui.draw(12.0); });
        /* The in-game menu in a few themes: it takes the theme's colours and font. */
        for (int theme : {14, 12, 11, 5})
        {
            settings.ui_theme = theme;
            settle();
            while (gfx.theme_fonts_busy())
            {
                settle();
                usleep(10000);
            }
            porpoise::Settings play = settings;
            ui.open_game_menu(&lib.games()[3], &play);
            for (int i = 0; i < 40; ++i)
                ui.update_game_menu(none, 0.016);
            Texture *frame = gfx.texture_file(covers + "/PRVW07.png");
            render(("sc-ingame-" + std::to_string(theme)).c_str(), [&] {
                gfx.background();
                if (frame)
                    gfx.image(frame, 0, 0, 1920, 1080, rgba(0xFFFFFF));
                ui.draw_game_menu(12.0);
            });
            Input c;
            c.held = kCircle;
            ui.update_game_menu(c, 0.016);
            for (int i = 0; i < 40; ++i)
                ui.update_game_menu(none, 0.016);
        }
        return 0;
    }
    /* PREVIEW_LOOKS=theme:colours:view[:memcards],...: the library (and, with
     * PREVIEW_PAGES=1, Settings > Interface and Accessibility and Memory
     * Cards) in each, then stop. */
    if (const char *looks = std::getenv("PREVIEW_LOOKS"); looks && *looks)
    {
        const bool pages = std::getenv("PREVIEW_PAGES") != nullptr;
        for (const char *at = looks; *at;)
        {
            int v[4] = {0, 0, 0, 0}, n = 0;
            while (*at && *at != ',')
            {
                if (*at >= '0' && *at <= '9' && n < 4)
                    v[n] = v[n] * 10 + (*at - '0');
                else if (*at == ':')
                    ++n;
                ++at;
            }
            if (*at == ',')
                ++at;
            settings.ui_theme = v[0];
            settings.ui_palette = v[1];
            settings.lib_view = v[2];
            settings.mc_view = v[3];
            if (const char *e = std::getenv("PREVIEW_FONT"))
                settings.ui_font = std::atoi(e);
            if (const char *e = std::getenv("PREVIEW_HC"))
                settings.high_contrast = std::atoi(e) != 0;
            if (const char *e = std::getenv("PREVIEW_FILTER"))
                settings.colour_filter = std::atoi(e);
            if (const char *e = std::getenv("PREVIEW_TEXT"))
                settings.text_size = std::atoi(e);
            ui.language_changed();
            settle();
            while (gfx.theme_fonts_busy() || gfx.cjk_busy()) /* made on workers */
            {
                settle();
                usleep(10000);
            }
            if (const char *e = std::getenv("PREVIEW_TURN")) /* the right stick held for this many frames */
                for (int f = 0, frames = std::atoi(e); f < frames; ++f)
                {
                    Input turn;
                    turn.right_x = 1.0f;
                    ui.update(turn, 0.016);
                }
            char tag[48];
            std::snprintf(tag, sizeof tag, "look-%d-%d-%d-%d", v[0], v[1], v[2], v[3]);
            render((std::string(tag) + "-library").c_str(), [&] { ui.draw(12.0); });
            if (!pages)
                continue;
            press(kR1, 2);
            press(1u << 0, 12);
            press(kDown, 7); /* Interface */
            press(kRight);
            settle();
            render((std::string(tag) + "-interface").c_str(), [&] { ui.draw(12.0); });
            press(kCircle);
            press(kDown); /* Accessibility */
            press(kRight);
            settle();
            render((std::string(tag) + "-access").c_str(), [&] { ui.draw(12.0); });
            press(kCircle);
            press(kR1, 2); /* to Memory Cards */
            for (int wait = 0; wait < 60; ++wait) /* the cards are read on a worker */
            {
                settle();
                usleep(20000);
            }
            render((std::string(tag) + "-cards").c_str(), [&] { ui.draw(12.0); });
            press(kR1, 2); /* back to the library */
            settle();
        }
        gfx.shutdown();
        return 0;
    }

    /* PREVIEW_LANGS=13,16: only these languages' library, a long message (line
     * breaking) and Settings > Interface, then stop. */
    if (const char *langs = std::getenv("PREVIEW_LANGS"); langs && *langs)
    {
        static const char *const tags[] = {"",    "en-",    "es-",   "fr-", "pt-", "it-",
                                           "ja-", "es419-", "ptbr-", "de-", "nl-", "pl-",
                                           "ru-", "zhs-",   "zht-",  "ko-", "tr-"};
        for (const char *at = langs; *at;)
        {
            const int lang = std::atoi(at);
            while (*at && *at != ',')
                ++at;
            if (*at == ',')
                ++at;
            if (lang < 1 || lang > 16)
                continue;
            const std::string tag = tags[lang];
            settings.ui_language = lang;
            porpoise::ui::apply_language(lang);
            ui.language_changed();
            settle();
            while (gfx.cjk_busy()) /* the language's CJK atlas, made on a worker */
                settle();
            if (const char *intro = std::getenv("PREVIEW_INTRO")) /* the menus' entrance, part way */
            {
                const float a = float(std::atof(intro));
                ui.set_intro(a, 28.0f * (1.0f - a));
                render((tag + "entrance").c_str(), [&] { ui.draw(12.0); });
                ui.set_intro(1, 0);
            }
            render((tag + "library").c_str(), [&] { ui.draw(12.0); });
            ui.show_message(
                porpoise::ui::tr("Porpoise can't reach /data"),
                porpoise::ui::tr(
                    "The console started Porpoise inside the app sandbox, so it can't see "
                    "/data or USB drives, and no jailbreak daemon freed it. A daemon that "
                    "frees Porpoise must be running before you open it:\n"
                    "\xE2\x80\xA2 etaHEN: turn on Legacy Command Server in etaHEN's Toolbox "
                    "settings (etaHEN's built-in app list can't be edited to add Porpoise).\n"
                    "\xE2\x80\xA2 OnionHEN: add PPSA99764 to exact_title_ids in "
                    "/data/OnionHEN/config.ini.\n"
                    "\xE2\x80\xA2 Or run a standalone daemon such as Lapy.\n"
                    "Then open Porpoise again; if a launch still lands here, try once more. "
                    "Until then, games go in /app0/porpoise/games."));
            settle();
            render((tag + "message").c_str(), [&] { ui.draw(12.0); });
            press(kCross);
            press(kR1, 2);
            press(1u << 0, 9);
            press(kDown, 7); /* Interface */
            press(kRight);
            settle();
            render((tag + "settings-interface").c_str(), [&] { ui.draw(12.0); });
            press(kCircle);
            press(kR1);
            settle();
        }
        gfx.shutdown();
        return 0;
    }

    if (async)
    {
        render("async-first", [&] { ui.draw(12.0); });
        for (int i = 0; i < 40; ++i)
        {
            usleep(15000);
            render("async-later", [&] { ui.draw(12.0); });
            ui.update(none, 0.016);
        }
    }
    press(kSquare);
    for (int i = 0; i < 9; ++i)
        ui.update(none, 0.016);
    render("details-flip", [&] { ui.draw(12.0); });
    settle();
    render("details", [&] { ui.draw(12.0); });
    press(kTriangle);
    settle();
    render("details-back", [&] { ui.draw(12.0); });
    {
        Input turn;
        turn.right_x = 0.6f;
        for (int i = 0; i < 60; ++i)
            ui.update(turn, 0.016);
        render("details-turn", [&] { ui.draw(12.0); });
    }
    press(kTriangle);
    settle();
    {
        /* R2: the next game, swiping in; L2 back again. */
        const std::uint32_t kL2 = 1u << 10, kR2 = 1u << 11;
        Input in;
        in.held = kR2;
        ui.update(in, 0.016);
        ui.update(none, 0.016);
        for (int i = 0; i < 7; ++i)
            ui.update(none, 0.016);
        render("details-swipe", [&] { ui.draw(12.0); });
        settle();
        render("details-next", [&] { ui.draw(12.0); });
        press(kL2);
        settle();
    }
    press(kDown);
    press(kCross); /* Save states */
    settle();
    render("details-states", [&] { ui.draw(12.0); });
    press(kCircle);
    settle();
    press(kDown);
    press(kCross); /* Game settings */
    press(kRight);
    settle();
    render("game-settings", [&] { ui.draw(12.0); });
    press(kCircle);   /* to the rail */
    press(kDown);     /* Recommended */
    press(kRight);
    settle();
    render("game-settings-recommended", [&] { ui.draw(12.0); });
    press(kDown);     /* the first pick */
    press(kCross);    /* off */
    settle();
    render("game-settings-recommended-toggled", [&] { ui.draw(12.0); });
    press(kCircle);
    press(1u << 0);   /* back up to This game */
    press(kCircle);
    press(kDown);  /* rail: Video */
    press(kRight);
    press(kRight); /* a change for this game */
    settle();
    render("game-settings-video", [&] { ui.draw(12.0); });
    press(kCircle);
    press(kCircle); /* back to details */
    {
        /* A game Dolphin lists codes for: the Cheats and patches section. */
        Game *g = nullptr;
        for (Game &x : lib.games())
            if (x.title == "Garden Critters")
                g = &x;
        if (g)
        {
            const std::string was = g->id;
            g->id = "GMSE01";
            press(kSquare); /* its details */
            settle();
            press(kDown, 2);
            settle();
            render("cheats-step0", [&] { ui.draw(12.0); });
            press(kCross); /* Game settings, again */
            settle();
            render("cheats-step1", [&] { ui.draw(12.0); });
            press(kDown, 12);
            press(kRight);
            settle();
            render("game-settings-cheats", [&] { ui.draw(12.0); });
            press(kDown, 2);
            press(kCross);
            settle();
            render("game-settings-cheats-toggled", [&] { ui.draw(12.0); });
            press(kCross);
            press(kCircle);
            press(kCircle);
            settle();
            g->id = was;
        }
    }
    press(kCircle); /* back to the library */
    settle();

    press(kTriangle);
    settle();
    render("sort", [&] { ui.draw(12.0); });
    press(kDown); /* Show */
    press(kRight); /* GameCube */
    settle();
    render("sort-gamecube", [&] { ui.draw(12.0); });
    press(kCircle);
    settle();
    render("library-gamecube", [&] { ui.draw(12.0); });
    press(kTriangle);
    press(kDown);
    press(kRight); /* Wii */
    press(kCircle);
    settle();
    render("library-wii", [&] { ui.draw(12.0); });
    press(kTriangle);
    press(kDown);
    press(kRight); /* All */
    press(kDown); /* Get covers and info now */
    settle();
    render("sort-covers", [&] { ui.draw(12.0); });
    press(kCircle);
    settle();

    /* The Revolution look: the home screen, pointed at and with the D-pad. */
    {
        settings.ui_theme = 1;
        settle();
        /* Wii banners play on worker threads: draw a while, in real time. */
        auto warm = [&](double seconds) {
            const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
            while (std::chrono::steady_clock::now() < until)
            {
                gfx.begin(0, float(W), float(H), 12.0f, 0.0f, false);
                ui.update(none, 0.016);
                ui.draw(12.0);
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
            }
        };
        warm(2.5);
        ui.preview_pointer(1010, 360);
        settle();
        render("home-pointer", [&] { ui.draw(12.0); });
        ui.preview_pointer(1300, 1010); /* over nothing: the open hand */
        settle();
        render("home-pointer-open", [&] { ui.draw(12.0); });
        ui.preview_pointer(-1, -1);
        press(kDown);
        settle();
        render("home-dpad", [&] { ui.draw(12.0); });
        ui.preview_pointer(156, 1000);
        settle();
        render("home-settings-button", [&] { ui.draw(12.0); });
        ui.preview_pointer(-1, -1);
        ui.preview_home_page(1);
        settle();
        render("home-page-2", [&] { ui.draw(12.0); });
        press(kTriangle);
        settle();
        render("home-sort", [&] { ui.draw(12.0); });
        press(kCircle);
        ui.preview_home_page(0);
        press(1u << 0, 3);
        press(1u << 2, 3);
        press(kRight, 1); /* a Wii game */
        settle();
        press(kCross); /* its tile opens: it grows to fill the screen */
        for (int i = 0; i < 6; ++i)
            ui.update(none, 0.016);
        render("rev-zoom-1", [&] { ui.draw(12.0); });
        for (int i = 0; i < 8; ++i)
            ui.update(none, 0.016);
        render("rev-zoom-2", [&] { ui.draw(12.0); });
        for (int i = 0; i < 8; ++i)
            ui.update(none, 0.016);
        render("rev-zoom-3", [&] { ui.draw(12.0); });
        warm(3.0);
        render("rev-details-wii", [&] { ui.draw(12.0); });
        press(1u << 2); /* Wii controls, to the left of Start */
        settle();
        render("rev-details-controls", [&] { ui.draw(12.0); });
        press(kCross); /* the setup for this game */
        settle();
        render("wii-setup-game", [&] { ui.draw(12.0); });
        press(kDown, 2); /* Before this game */
        settle();
        render("wii-setup-game-ask", [&] { ui.draw(12.0); });
        press(kCircle);
        settle();
        press(kDown); /* the chips */
        settle();
        render("rev-details-chips", [&] { ui.draw(12.0); });
        ui.preview_pointer(1858, 466);
        settle();
        render("rev-details-pointer", [&] { ui.draw(12.0); });
        ui.preview_pointer(-1, -1);
        press(kCircle);
        settle();
        press(1u << 0, 3);
        press(1u << 2, 3);
        press(kRight, 3); /* a GameCube game */
        press(kCross);
        settle();
        render("rev-details-gc", [&] { ui.draw(12.0); });
        press(kCircle);
        settle();
        press(kR1); /* Memory Cards, in white */
        settle();
        render("rev-memory-cards", [&] { ui.draw(12.0); });
        press(1u << 10); /* L2: the Wii saves */
        settle();
        press(kRight);
        settle();
        render("rev-wii-saves", [&] { ui.draw(12.0); });
        settings.ui_theme = 0;
        settle();
        render("wii-saves", [&] { ui.draw(12.0); });
        settings.ui_theme = 1;
        press(1u << 10);
        settle();
        press(kR1); /* Settings */
        settle();
        press(kDown, 3);
        press(kRight);
        settle();
        render("rev-settings", [&] { ui.draw(12.0); });
        press(kCircle);
        press(kR1); /* back to the tiles */
        settle();
        {
            porpoise::Settings play = settings;
            play.ui_theme = 1;
            ui.open_game_menu(&lib.games()[3], &play);
            for (int i = 0; i < 40; ++i)
                ui.update_game_menu(none, 0.016);
            Texture *frame = gfx.texture_file(covers + "/PRVW07.png");
            render("rev-ingame-menu", [&] {
                gfx.background();
                if (frame)
                    gfx.image(frame, 0, 0, 1920, 1080, rgba(0xFFFFFF));
                ui.draw_game_menu(12.0);
            });
            Input c;
            c.held = kCircle;
            ui.update_game_menu(c, 0.016);
            for (int i = 0; i < 40; ++i)
                ui.update_game_menu(none, 0.016);
        }
        settings.ui_layout = 1;
        settle();
        render("rev-coverflow", [&] { ui.draw(12.0); });
        settings.ui_layout = 0;
        settle();
        ui.preview_pointer(1100, 470);
        settle();
        render("home-pointer-tile", [&] { ui.draw(12.0); });
        ui.preview_pointer(-1, -1);
        settings.ui_theme = 0;
        settle();
        /* Check my setup, as at the first start. */
        ui.show_setup_check(true);
        settle();
        render("setup-check", [&] { ui.draw(12.0); });
        press(kCircle);
        settle();
        /* Quick resume: a saved spot for every game, then Play asks first. */
        settings.quick_resume = true;
        for (const Game &gm : lib.games())
        {
            const std::string resume = porpoise::states::resume_path(Library::key_of(gm));
            const std::string dir = resume.substr(0, resume.rfind('/'));
            mkdir((out + "/states").c_str(), 0777);
            mkdir(dir.c_str(), 0777);
            if (std::FILE *f = std::fopen(resume.c_str(), "wb"))
            {
                std::fputs("state", f);
                std::fclose(f);
            }
            std::FILE *in = std::fopen((covers + "/PRVW07.png").c_str(), "rb");
            std::FILE *png = std::fopen((resume.substr(0, resume.size() - 6) + ".png").c_str(), "wb");
            if (in && png)
            {
                char buf[4096];
                std::size_t n;
                while ((n = std::fread(buf, 1, sizeof buf, in)) > 0)
                    std::fwrite(buf, 1, n, png);
            }
            if (in)
                std::fclose(in);
            if (png)
                std::fclose(png);
        }
        press(kCross);
        settle();
        render("resume-ask", [&] { ui.draw(12.0); });
        press(kCircle);
        settle();
        settings.quick_resume = false;
        /* Developer options (Cross three times on the creator's name, in About). */
        settings.developer = true;
        ui.language_changed();
        press(kR1, 2);
        press(1u << 0, 12);
        press(kDown, 7);
        press(kRight);
        settle();
        render("settings-developer", [&] { ui.draw(12.0); });
        press(kCircle);
        /* Every section, top and bottom, for a look at the new rows. */
        for (int sec = 0; sec < 10; ++sec)
        {
            press(1u << 0, 12);
            press(kDown, sec);
            press(kRight);
            settle();
            const std::string top = "sec-" + std::to_string(sec);
            render(top.c_str(), [&] { ui.draw(12.0); });
            press(kDown, 30);
            settle();
            const std::string end = top + "-end";
            render(end.c_str(), [&] { ui.draw(12.0); });
            press(1u << 0, sec == 9 ? 24 : 9);
            if (sec == 9)
            {
                ui.set_versions({"v2.0-beta.1", "v1.5.1", "v1.5", "v1.1"}, {0, 0, 0, 0}, {true, false, false, false},
                                {44u << 20, 42u << 20, 42u << 20, 40u << 20});
                press(kDown, 7);
            }
            settle();
            const std::string mid = top + "-mid";
            render(mid.c_str(), [&] { ui.draw(12.0); });
            press(kCircle);
        }
        press(kCircle);
        settings.developer = false;
        ui.language_changed();
        settle();
    }
    if (const char *only = std::getenv("PREVIEW_ONLY"); only && *only) /* just the scenes above */
    {
        gfx.shutdown();
        return 0;
    }

    press(kR1);
    settle();
    render("memory-cards", [&] { ui.draw(12.0); }); /* scans the cards */
    press(kRight);
    settle();
    render("memory-cards", [&] { ui.draw(12.0); });
    press(kRight, 4);
    settle();
    render("memory-cards-b", [&] { ui.draw(12.0); });
    press(kTriangle);
    settle();
    render("memcard-delete", [&] { ui.draw(12.0); });
    press(kCircle);

    press(kR1);
    settle();
    render("settings", [&] { ui.draw(12.0); });
    {
        /* mid-slide, for the motion */
        Input in;
        in.held = 1u << 8; /* L1 */
        ui.update(in, 0.016);
        ui.update(none, 0.016);
        for (int i = 0; i < 8; ++i)
            ui.update(none, 0.016);
        render("tab-slide", [&] { ui.draw(12.0); });
        settle();
        press(kR1);
        settle();
    }
    press(kRight);
    press(kDown, 3); /* Add a game folder */
    settle();
    render("settings-games", [&] { ui.draw(12.0); });
    press(kCross); /* the browser: the whole console, on /data */
    settle();
    render("browser-root", [&] { ui.draw(12.0); });
    press(kCross); /* /data */
    settle();
    render("browser", [&] { ui.draw(12.0); });
    press(kDown); /* games */
    press(kCross);
    settle();
    render("browser-games", [&] { ui.draw(12.0); });
    press(kCross); /* GameCube */
    press(kDown);
    settle();
    render("browser-files", [&] { ui.draw(12.0); });
    press(kCross); /* copy this one? */
    settle();
    render("browser-copy", [&] { ui.draw(12.0); });
    press(kCircle);
    press(kTriangle); /* drives and shortcuts */
    settle();
    render("browser-drives", [&] { ui.draw(12.0); });
    press(kTriangle); /* back to the whole console */
    press(kCircle);   /* out */
    settle();
    press(kCircle); /* to the rail */
    press(kDown);   /* Video */
    settle();
    render("settings-video", [&] { ui.draw(12.0); });
    press(kDown, 5); /* Interface */
    press(kRight);
    press(kDown, 3); /* Reset all settings */
    press(kCross);
    settle();
    render("dialog", [&] { ui.draw(12.0); });
    press(kCircle);
    press(kCircle);
    press(kDown); /* About */
    press(kRight);
    press(kDown, 2);
    settle();
    render("settings-about", [&] { ui.draw(12.0); });
    /* A newer release: the About row offers it, then the update runs. */
    ui.set_latest_release("v1.2", "https://github.com/elripalda/Porpoise/releases/tag/v1.2", 41u << 20);
    press(1u << 0, 1); /* up to the update row */
    settle();
    render("settings-about-update", [&] { ui.draw(12.0); });
    press(kCross);
    settle();
    render("update-confirm", [&] { ui.draw(12.0); });
    press(kRight);
    press(kCross); /* Install */
    ui.set_update_progress(2, 18u << 20, 41u << 20, "");
    settle();
    render("update-downloading", [&] { ui.draw(12.0); });
    ui.set_update_progress(3, 1630, 2709, "");
    render("update-installing", [&] { ui.draw(12.0); });
    ui.set_update_progress(5, 0, 0, "The download doesn't match its SHA-256. Nothing was changed.");
    settle();
    render("update-failed", [&] { ui.draw(12.0); });
    press(kCross);
    ui.set_update_progress(0, 0, 0, "");
    ui.set_update_progress(4, 2709, 2709, "");
    render("update-done", [&] { ui.draw(12.0); });
    ui.set_update_progress(0, 0, 0, "");
    ui.set_latest_release("v1.1", "", 0);
    settle();
    press(kCircle);
    press(kCircle); /* back to the library */
    settle();

    /* Controls, and the button-mapping screen. */
    {
        const std::uint32_t kUp = 1u << 0;
        press(kR1, 2);    /* Settings, from the library */
        press(kUp, 8);    /* the top of the rail */
        press(kDown, 4);  /* Controls */
        press(kRight);
        settle();
        render("settings-controls", [&] { ui.draw(12.0); });
        press(kCircle);
        press(kDown); /* Wii Remote */
        press(kRight);
        settle();
        render("settings-wii-remote", [&] { ui.draw(12.0); });
        press(kCross); /* Wii Remote setup */
        settle();
        press(kSquare); /* Advanced */
        for (int i = 0; i < 8; ++i)
        {
            ui.preview_setup_step(i);
            const std::string shot_name = "wii-setup-" + std::to_string(i);
            render(shot_name.c_str(), [&] { ui.draw(12.0); });
        }
        ui.preview_setup_step(0);
        press(kCircle);
        settle();
        press(kDown, 2); /* How to hold it */
        press(kCross);
        settle();
        for (int i = 0; i < 5; ++i)
        {
            const std::string shot_name = "wii-guide-" + std::to_string(i);
            render(shot_name.c_str(), [&] { ui.draw(12.0); });
            press(kRight);
            settle();
        }
        press(kCircle);
        settle();
        press(kCircle);
        press(1u << 0); /* back to Controls */
        press(kRight);
        settle();
        press(kDown);     /* Customize buttons */
        press(kCross);
        settle();
        render("mapping", [&] { ui.draw(12.0); });
        press(kDown, 2);  /* B */
        press(kCross);    /* waiting for a button */
        for (int i = 0; i < 20; ++i)
            ui.update(none, 0.016);
        render("mapping-capture", [&] { ui.draw(12.0); });
        press(kCircle);   /* B on Circle */
        settle();
        render("mapping-custom", [&] { ui.draw(12.0); });
        press(kDown, 12); /* Start over from the PlayStation layout */
        press(kCross);
        settle();
        render("mapping-playstation", [&] { ui.draw(12.0); });
        press(kR1); /* the Wii Remote + Nunchuk's buttons */
        settle();
        render("mapping-wii", [&] { ui.draw(12.0); });
        press(kDown, 2);  /* the 1 button */
        press(kCross);
        for (int i = 0; i < 20; ++i)
            ui.update(none, 0.016);
        press(kSquare);   /* 1 on Square */
        settle();
        render("mapping-wii-custom", [&] { ui.draw(12.0); });
        press(kR1, 3);    /* the Classic Controller */
        settle();
        render("mapping-classic", [&] { ui.draw(12.0); });
        press(kCircle);
        settle();
        render("settings-controls-after", [&] { ui.draw(12.0); });
        press(kCircle);
        press(kCircle); /* back to the library */
        settle();
    }

    /* The in-game menu, over a stand-in for the game's picture. */
    {
        porpoise::Settings play = settings;
        ui.open_game_menu(&lib.games()[3], &play);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        Input dn;
        dn.held = 1u << 1;
        ui.update_game_menu(dn, 0.016);
        ui.update_game_menu(none, 0.016);
        for (int i = 0; i < 20; ++i)
            ui.update_game_menu(none, 0.016);
        Texture *frame = gfx.texture_file(covers + "/PRVW07.png");
        auto shot = [&](const char *name) {
            for (int i = 0; i < 20; ++i)
                ui.update_game_menu(none, 0.016);
            render(name, [&] {
                gfx.background();
                if (frame)
                    gfx.image(frame, 0, 0, 1920, 1080, rgba(0xFFFFFF));
                ui.draw_game_menu(12.0);
            });
        };
        auto menu_press = [&](std::uint32_t bit, int times = 1) {
            for (int i = 0; i < times; ++i)
            {
                Input in;
                in.held = bit;
                ui.update_game_menu(in, 0.016);
                ui.update_game_menu(none, 0.016);
            }
        };
        shot("ingame-menu");
        menu_press(1u << 4);    /* Save state...: the slot picker */
        shot("ingame-save-pick");
        menu_press(1u << 4);    /* slot 1 has a state: asks first */
        shot("ingame-save-replace");
        menu_press(1u << 5);    /* back */
        menu_press(1u << 1);    /* Load state... */
        menu_press(1u << 4);
        shot("ingame-menu-load");
        menu_press(1u << 4);    /* load it: busy until the host answers */
        shot("ingame-loading");
        ui.take_menu_request();
        ui.menu_state_done(porpoise::ui::App::MenuRequest::Load, 0, false);
        menu_press(1u << 5);
        menu_press(1u << 0, 2); /* back to Resume */
        menu_press(1u << 9); /* R1: Video */
        menu_press(1u << 1, 5); /* Screen filter */
        menu_press(1u << 3, 3); /* CRT */
        shot("ingame-video");
        menu_press(1u << 9); /* Graphics */
        menu_press(1u << 1, 9); /* Use a setup */
        shot("ingame-graphics");
        menu_press(1u << 0, 9);
        menu_press(1u << 9); /* Controls */
        shot("ingame-controls");
        {
            /* The same tab for a Wii game: the Wii Remote's rows. */
            const std::string was = lib.games()[3].platform;
            lib.games()[3].platform = "Wii";
            for (int i = 0; i < 4; ++i)
                menu_press(1u << 1);
            shot("ingame-controls-wii");
            /* Row 3 Wii controller, row 6 Grip: the drawing for each way of holding it. */
            menu_press(1u << 0);    /* Wii controller */
            menu_press(1u << 3);    /* Remote */
            menu_press(1u << 1, 3); /* Grip */
            shot("wii-remote-both-hands");
            menu_press(1u << 3);
            shot("wii-remote-upright-right");
            menu_press(1u << 3);
            shot("wii-remote-upright-left");
            menu_press(1u << 3);    /* back to both hands */
            menu_press(1u << 0, 3);
            menu_press(1u << 3, 3); /* Two controllers */
            shot("wii-two-controllers");
            menu_press(1u << 3, 1); /* Remote + Nunchuk */
            for (int i = 0; i < 10; ++i)
                menu_press(1u << 1);
            shot("ingame-controls-wii-2");
            for (int i = 0; i < 14; ++i)
                menu_press(1u << 0);
            lib.games()[3].platform = was;
        }
        menu_press(1u << 1);
        menu_press(1u << 4); /* Customize buttons, over the game */
        shot("ingame-mapping");
        menu_press(1u << 5);
    }

    /* Spanish. */
    porpoise::ui::apply_language(2);
    ui.language_changed();
    press(kCircle, 3);
    settle();
    render("es-library", [&] { ui.draw(12.0); });
    press(kSquare);
    settle();
    render("es-details", [&] { ui.draw(12.0); });
    press(kCircle);
    press(kR1);
    settle();
    render("es-memory-cards", [&] { ui.draw(12.0); });
    press(kR1);
    press(kDown); /* Video */
    settle();
    render("es-settings", [&] { ui.draw(12.0); });
    {
        porpoise::Settings play = settings;
        ui.open_game_menu(&lib.games()[3], &play);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        render("es-ingame-menu", [&] {
            gfx.background();
            ui.draw_game_menu(12.0);
        });
    }
    /* The other languages: Japanese, Italian, German, Russian, Polish, Latin
     * American Spanish, Brazilian Portuguese, Dutch. */
    for (int lang : {6, 5, 9, 12, 11, 7, 8, 10})
    {
        static const char *const tags[] = {"",    "en-", "es-", "fr-",  "pt-",  "it-", "ja-", "es419-", "ptbr-",
                                           "de-", "nl-", "pl-", "ru-", "zhs-", "zht-", "ko-", "tr-"};
        const std::string tag = tags[lang];
        settings.ui_language = lang;
        porpoise::ui::apply_language(lang);
        ui.language_changed();
        press(kCircle, 3);
        settle();
        while (gfx.cjk_busy())
            settle();
        render((tag + "library").c_str(), [&] { ui.draw(12.0); });
        press(kSquare);
        settle();
        render((tag + "details").c_str(), [&] { ui.draw(12.0); });
        press(kCircle);
        press(kR1, 2);
        press(1u << 0, 9);
        press(kDown, 6); /* Interface */
        press(kRight);
        settle();
        render((tag + "settings-interface").c_str(), [&] { ui.draw(12.0); });
        press(kCircle);
        press(kR1);
        settle();
        porpoise::Settings play = settings;
        ui.open_game_menu(&lib.games()[3], &play);
        for (int i = 0; i < 40; ++i)
            ui.update_game_menu(none, 0.016);
        render((tag + "ingame-menu").c_str(), [&] {
            gfx.background();
            ui.draw_game_menu(12.0);
        });
    }
    settings.ui_language = 0;
    porpoise::ui::apply_language(1);
    ui.language_changed();

    ui.begin_launch(&lib.games()[3]);
    ui.set_launch_status("Preparing graphics", -1);
    render("launch", [&] { ui.draw_launch(12.4); });
    ui.begin_launch(&lib.games()[8]);
    ui.set_launch_status("Reading disc", -1);
    render("launch-nocover", [&] { ui.draw_launch(12.4); });

    Library empty;
    LibraryPaths ep;
    ep.state = out + "/empty-state.txt";
    empty.scan(ep);
    App ui2;
    ui2.init(&gfx, &empty, &settings, out + "/settings.ini", out + "/options.ini", out);
    render("empty", [&] { ui2.draw(12.0); });
    gfx.shutdown();
    return 0;
}
