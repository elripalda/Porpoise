/* Porpoise UI - Settings > Console > Wii System Data: the player's own Wii's
 * NAND backup (made with BootMii) into Dolphin's Wii folder, with the menus
 * held while it's read, and what came of it.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_app.hpp"

#include <algorithm>
#include <ctime>
#include <memory>

#include "porpoise_nand.hpp"
#include "title_threads.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;

namespace
{
struct NandRun
{
    std::shared_ptr<porpoise::nand::Progress> progress;
    std::string bin, keys, root, kept;
};

void *run_nand(void *arg)
{
    std::unique_ptr<NandRun> run(static_cast<NandRun *>(arg));
    porpoise::nand::import(run->bin, run->keys, run->root, run->kept, *run->progress);
    return nullptr;
}
} // namespace

void App::start_nand_import()
{
    if (nand_job_ || nand_bin_.empty())
        return;
    nand_job_ = std::make_shared<porpoise::nand::Progress>();
    /* What Porpoise had that the Wii's files replace: kept in a folder of
     * this import's own, by date. */
    char stamp[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y-%m-%d-%H%M%S", std::localtime(&now));
    auto *run = new NandRun{nand_job_, nand_bin_, nand_keys_, saves_dir_ + "/User/Wii",
                            saves_dir_ + "/User/Wii-before-NAND/" + stamp};
    pthread_t thread;
    if (create_title_thread(&thread, run_nand, run) == 0)
        pthread_detach(thread);
    else
        run_nand(run);
}

/* While it runs, the menus wait; when it ends, what happened. */
bool App::update_nand_job()
{
    if (!nand_job_)
        return false;
    const int state = nand_job_->state.load();
    if (state == 0)
        return true;
    const std::shared_ptr<porpoise::nand::Progress> done = nand_job_;
    nand_job_.reset();
    if (state == 1)
    {
        std::string message =
            trf("Your Wii's settings, Miis, channels and saves are Porpoise's now: {n} files.",
                {{"n", std::to_string(done->files)}});
        if (done->kept > 0)
            message += " " + trf("{n} of Porpoise's own files were in the way; they're kept in "
                                 "saves/User/Wii-before-NAND.",
                                 {{"n", std::to_string(done->kept)}});
        if (!done->certificates)
            message += " " + tr("Its online certificates weren't found (no IOS13 in this backup).");
        open_dialog(DialogKind::Info, tr("Wii system data imported"), message, "");
        sfx(Sound::LaunchGame);
    }
    else
    {
        std::string why;
        {
            std::lock_guard<std::mutex> lock(done->m);
            why = done->error;
        }
        open_dialog(DialogKind::Info, tr("The import didn't finish"), tr(why), "");
    }
    if (screen_ == Screen::Main && tab_ == Tab::Settings)
    {
        const int row = settings_row_;
        build_settings();
        settings_row_ = std::clamp(row, 0, int(rows_.size()) - 1);
    }
    return false;
}

#ifdef PORPOISE_HOST_PREVIEW
void App::preview_nand(int done, int total)
{
    nand_job_ = std::make_shared<porpoise::nand::Progress>();
    nand_job_->done.store(done);
    nand_job_->total.store(total);
}
#endif

void App::draw_nand_job()
{
    if (!nand_job_)
        return;
    Gfx &g = *g_;
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.7f), 1, 0);
    const float w = 860, h = 280, x = 960 - w * 0.5f, y = 400;
    g.panel(x, y, w, h, rgba(0x13256F, 0.94f), 0.65f, kR, rgba(0x6FAEFF), 2.0f, 10, 0.25f);
    g.text_mid(Font::Bold, ts(36), 960, y + 64, kWhite, Align::Center, tr("Importing your Wii's system data"));
    const int total = nand_job_->total.load(), done = std::min(nand_job_->done.load(), std::max(1, total));
    const float f = total > 0 ? float(done) / float(total) : 0.0f;
    const float bx = x + 70, bw = w - 140, by = y + 130;
    g.panel(bx, by, bw, 22, rgba(0x07102E, 0.8f), 1, 11, rgba(0x3D5AB0, 0.9f), 1.4f);
    if (f > 0)
        g.panel(bx, by, std::max(22.0f, bw * f), 22, rgba(0x5CD3FF, 0.95f), 1, 11);
    g.text_mid(Font::SemiBold, ts(24), 960, y + 200, kSoft, Align::Center,
               total > 0 ? trf("{a} of {n} files", {{"a", std::to_string(done)}, {"n", std::to_string(total)}})
                         : tr("Reading the backup\xE2\x80\xA6"));
    g.text_mid(Font::Regular, ts(21), 960, y + 240, rgba(0x8A96C8), Align::Center,
               tr("This takes a minute or two. Keep Porpoise open."));
}
} // namespace porpoise::ui
