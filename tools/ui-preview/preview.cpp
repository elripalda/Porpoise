/* Porpoise UI preview: renders the launcher screens offscreen (any Vulkan
 * driver, e.g. lavapipe) and writes PNGs, to check the UI without a console.
 *
 *   preview <assets dir> <covers dir> <out dir>
 */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ui_app.hpp"
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
    Gfx gfx;
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

    auto render = [&](const char *name, auto &&draw) {
        gfx.begin(0, float(W), float(H), 12.0f, 0.0f, false);
        draw();
        vkResetCommandBuffer(cmd, 0);
        VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(cmd, &b);
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
    press(kCross); /* Game settings */
    press(kRight);
    settle();
    render("game-settings", [&] { ui.draw(12.0); });
    press(kCircle);
    press(kDown);  /* rail: Video */
    press(kRight);
    press(kRight); /* a change for this game */
    settle();
    render("game-settings-video", [&] { ui.draw(12.0); });
    press(kCircle);
    press(kCircle); /* back to details */
    press(kCircle); /* back to the library */
    settle();

    press(kTriangle);
    settle();
    render("sort", [&] { ui.draw(12.0); });
    press(kCircle);

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
    press(kCross); /* the browser: drives */
    press(kCross); /* the first one */
    press(kDown, 3);
    settle();
    render("browser", [&] { ui.draw(12.0); });
    press(kCircle, 2);
    settle();
    press(kCircle); /* to the rail */
    press(kDown);   /* Video */
    settle();
    render("settings-video", [&] { ui.draw(12.0); });
    press(kDown, 5); /* Interface */
    press(kRight);
    press(kDown, 2); /* Reset all settings */
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

    /* Controls, and the button-mapping screen. */
    {
        const std::uint32_t kUp = 1u << 0;
        press(kR1, 2);    /* Settings, from the library */
        press(kUp, 8);    /* the top of the rail */
        press(kDown, 4);  /* Controls */
        press(kRight);
        settle();
        render("settings-controls", [&] { ui.draw(12.0); });
        press(kDown);     /* Customize buttons */
        press(kCross);
        settle();
        render("mapping", [&] { ui.draw(12.0); });
        press(kDown);     /* B */
        press(kCross);    /* waiting for a button */
        for (int i = 0; i < 20; ++i)
            ui.update(none, 0.016);
        render("mapping-capture", [&] { ui.draw(12.0); });
        press(kCircle);   /* B on Circle */
        settle();
        render("mapping-custom", [&] { ui.draw(12.0); });
        press(kDown, 12); /* Use the PlayStation layout */
        press(kCross);
        settle();
        render("mapping-playstation", [&] { ui.draw(12.0); });
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
        render("ingame-menu", [&] {
            gfx.background();
            if (frame)
                gfx.image(frame, 0, 0, 1920, 1080, rgba(0xFFFFFF));
            ui.draw_game_menu(12.0);
        });
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
