/* Porpoise UI - a network share's panel: the computer, the shared folder and
 * how to sign in, typed with the controller; a test of the connection, and
 * the computer's shared folders to pick from. Settings > Games opens it.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_pad.hpp"
#include "ui_app.hpp"

#include <mutex>

#include "title_threads.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

/* A test or a search, on its own thread: the panel looks for its result each
 * frame, and a panel closed meanwhile just lets it finish. */
struct App::ShareJob
{
    enum Kind
    {
        Test,
        Find,
    };
    Kind kind = Test;
    netfs::Share share;
    std::mutex m;
    bool done = false;
    netfs::Problem problem = netfs::Problem::None;
    std::string detail;
    std::vector<std::string> names;
};

namespace
{
constexpr int kFields = 5, kButtonsRow = 5;
enum Button
{
    ButtonFind,
    ButtonTest,
    ButtonSave,
    ButtonRemove,
};

} // namespace

void *App::run_share_job(void *arg)
{
    auto *held = static_cast<std::shared_ptr<ShareJob> *>(arg);
    std::shared_ptr<ShareJob> job = *held;
    delete held;
    std::string detail;
    std::vector<std::string> names;
    const netfs::Problem problem = job->kind == ShareJob::Find ? netfs::enumerate(job->share, names, &detail)
                                                               : netfs::test(job->share, &detail);
    std::lock_guard<std::mutex> lock(job->m);
    job->problem = problem;
    job->detail = detail;
    job->names = std::move(names);
    job->done = true;
    return nullptr;
}

namespace
{
/* A problem in the player's words. */
std::string problem_text(netfs::Problem problem, const netfs::Share &s, const std::string &detail)
{
    using netfs::Problem;
    switch (problem)
    {
    case Problem::None:
        return "";
    case Problem::NoComputer:
        return tr("Enter the computer's address or name first.");
    case Problem::NoShare:
        return tr("Enter the shared folder's name, or look for the computer's shared folders.");
    case Problem::UnknownName:
        return trf("Couldn't find a computer called {computer}. Try its address instead (like 192.168.1.20).",
                   {{"computer", s.host}});
    case Problem::Unreachable:
        return trf("Couldn't reach {computer}. Check the address, that the computer is on, and that it shares "
                   "files on the network.",
                   {{"computer", s.host}});
    case Problem::SignIn:
        return tr("The computer didn't accept the username or password.");
    case Problem::NoSuchShare:
        return trf("{computer} has no shared folder called {name}.", {{"computer", s.host}, {"name", s.share}});
    case Problem::Denied:
        return tr("The computer didn't let Porpoise in. Check the folder's sharing permissions, or sign in with a "
                  "username and password.");
    case Problem::NoFolder:
        return trf("There's no folder {folder} in the share.", {{"folder", s.folder}});
    case Problem::Other:
        break;
    }
    return trf("Couldn't connect: {reason}", {{"reason", detail}});
}

std::string &field_text(netfs::Share &s, int field)
{
    switch (field)
    {
    case 0:
        return s.host;
    case 1:
        return s.share;
    case 2:
        return s.folder;
    case 3:
        return s.user;
    default:
        return s.password;
    }
}

std::size_t field_limit(int field)
{
    return field == 0 ? 128 : field == 1 ? 80 : field == 2 ? 200 : field == 3 ? 64 : 128;
}
} // namespace

void App::open_share(int index)
{
    share_ = SharePanel{};
    share_.open = true;
    share_.index = index;
    const std::vector<netfs::Share> list = netfs::shares();
    if (index >= 0 && index < int(list.size()))
        share_.share = list[std::size_t(index)];
    else
        share_.index = -1;
    sfx(Sound::DetailsFlip);
}

void App::close_share()
{
    share_.share.password.assign(share_.share.password.size(), '\0');
    share_ = SharePanel{};
    sfx(Sound::DetailsFlip);
}

App::Action App::update_share(bool up, bool down, bool left, bool right)
{
    SharePanel &p = share_;
    bool busy = false;
    if (p.job)
    {
        std::lock_guard<std::mutex> lock(p.job->m);
        if (p.job->done)
        {
            if (p.job->kind == ShareJob::Find)
            {
                const bool ok = p.job->problem == netfs::Problem::None;
                if (ok && !p.job->names.empty())
                {
                    p.found = p.job->names;
                    if (std::find(p.found.begin(), p.found.end(), p.share.share) == p.found.end())
                        p.share.share = p.found.front();
                    p.message = p.found.size() == 1
                                    ? trf("Found the shared folder {name}.", {{"name", p.found.front()}})
                                    : trf("Found {n} shared folders. Left and right on Shared Folder pick one.",
                                          {{"n", std::to_string(p.found.size())}});
                    p.message_ok = true;
                }
                else if (ok)
                {
                    p.message = tr("The computer answered, but shares no folders Porpoise can see.");
                    p.message_ok = false;
                }
                else
                {
                    p.message = problem_text(p.job->problem, p.job->share, p.job->detail);
                    p.message_ok = false;
                }
            }
            else
            {
                p.message_ok = p.job->problem == netfs::Problem::None;
                p.message = p.message_ok ? tr("Connected. Porpoise can read this folder.")
                                         : problem_text(p.job->problem, p.job->share, p.job->detail);
            }
            sfx(p.message_ok ? Sound::LaunchGame : Sound::MovingTab);
            p.job.reset();
        }
        else
            busy = true;
    }

    if (p.typing)
    {
        const int field = p.row;
        if (keyboard_update(p.kb, field_text(p.share, field), field_limit(field), field != 0, up, down, left, right))
        {
            p.typing = false;
            /* The next field still empty, or the buttons. */
            p.row = field < 1 && p.share.share.empty() ? 1 : kButtonsRow;
            if (p.row == kButtonsRow)
                p.button = p.share.share.empty() ? ButtonFind : ButtonSave;
        }
        return Action::None;
    }

    if (pressed(BtnCircle))
    {
        close_share();
        return Action::None;
    }
    if (up && p.row > 0)
    {
        --p.row;
        sfx(Sound::MenuScroll);
    }
    if (down && p.row < kButtonsRow)
    {
        ++p.row;
        sfx(Sound::MenuScroll);
    }
    const int buttons = p.index >= 0 ? 4 : 3;
    if (p.row == kButtonsRow && (left || right))
    {
        p.button = (p.button + (left ? buttons - 1 : 1)) % buttons;
        sfx(Sound::MenuScroll);
    }
    if (p.row == 1 && (left || right) && !p.found.empty())
    {
        /* The shared folders found: pick one. */
        auto it = std::find(p.found.begin(), p.found.end(), p.share.share);
        int i = it == p.found.end() ? -1 : int(it - p.found.begin());
        const int n = int(p.found.size());
        i = left ? (i <= 0 ? n - 1 : i - 1) : (i + 1) % n;
        p.share.share = p.found[std::size_t(i)];
        sfx(Sound::MenuScroll);
    }
    if (!pressed(BtnCross))
        return Action::None;

    if (p.row < kFields)
    {
        p.typing = true;
        p.kb = Keyboard{};
        p.message.clear();
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    if (busy)
        return Action::None;
    const bool has_host = !p.share.host.empty();
    const bool ready = has_host && !p.share.share.empty();
    switch (Button(p.button))
    {
    case ButtonFind:
    case ButtonTest:
    {
        if (p.button == ButtonFind ? !has_host : !ready)
        {
            p.message = problem_text(has_host ? netfs::Problem::NoShare : netfs::Problem::NoComputer, p.share, "");
            p.message_ok = false;
            sfx(Sound::MovingTab);
            return Action::None;
        }
        auto job = std::make_shared<ShareJob>();
        job->kind = p.button == ButtonFind ? ShareJob::Find : ShareJob::Test;
        job->share = p.share;
        auto *held = new std::shared_ptr<ShareJob>(job);
        pthread_t thread;
        if (create_title_thread(&thread, run_share_job, held) == 0)
            pthread_detach(thread);
        else
            run_share_job(held); /* no thread: here, as it would have */
        p.job = job;
        p.message.clear();
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    case ButtonSave:
    {
        if (!ready)
        {
            p.message = problem_text(has_host ? netfs::Problem::NoShare : netfs::Problem::NoComputer, p.share, "");
            p.message_ok = false;
            sfx(Sound::MovingTab);
            return Action::None;
        }
        std::vector<netfs::Share> list = netfs::shares();
        netfs::Share s = p.share;
        if (p.index >= 0 && p.index < int(list.size()))
        {
            s.name = list[std::size_t(p.index)].name;
            list[std::size_t(p.index)] = s;
        }
        else
        {
            s.name = netfs::unique_name(s, list);
            list.push_back(s);
        }
        if (!netfs::save(shares_file(), list))
        {
            p.message = tr("Porpoise couldn't save the share. Is its folder full or read-only?");
            p.message_ok = false;
            sfx(Sound::MovingTab);
            return Action::None;
        }
        netfs::set_shares(list);
        close_share();
        sfx(Sound::LaunchGame);
        build_settings();
        return Action::Rescan;
    }
    case ButtonRemove:
    {
        std::vector<netfs::Share> list = netfs::shares();
        if (p.index < 0 || p.index >= int(list.size()))
            return Action::None;
        list.erase(list.begin() + p.index);
        if (!netfs::save(shares_file(), list))
        {
            p.message = tr("Porpoise couldn't save the share. Is its folder full or read-only?");
            p.message_ok = false;
            sfx(Sound::MovingTab);
            return Action::None;
        }
        netfs::set_shares(list);
        close_share();
        build_settings();
        return Action::Rescan;
    }
    }
    return Action::None;
}

void App::draw_share()
{
    if (!share_.open)
        return;
    SharePanel &p = share_;
    Gfx &g = *g_;
    p.anim = settings_ && settings_->reduced_motion ? 1.0f : std::min(1.0f, p.anim + 1.0f / 10.0f);
    const float t = ease_out(p.anim);
    bool busy = false;
    if (p.job)
    {
        std::lock_guard<std::mutex> lock(p.job->m);
        busy = !p.job->done;
    }
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.62f * t), 1, 0);
    g.set_layer(0, 24.0f * (1.0f - t), t);

    const float w = 1180, x = 960 - w * 0.5f, pad = 56;
    const float text_w = w - pad * 2;
    std::vector<std::string> about;
    if (!p.typing)
        about = wrap(g, Font::Regular, ts(26),
                     tr("Games from a shared folder on a computer or NAS on your home network (SMB). Porpoise only "
                        "reads from it. A wired connection works best for big games."),
                     text_w, 3);
    std::string note_text = busy ? (p.job && p.job->kind == ShareJob::Find ? tr("Looking for shared folders\xE2\x80\xA6")
                                                                           : tr("Connecting\xE2\x80\xA6"))
                                 : p.message;
    const auto note = note_text.empty() ? std::vector<std::string>{} : wrap(g, Font::SemiBold, ts(26), note_text, text_w, 3);
    constexpr float kFieldH = 80;
    const int fields_shown = p.typing ? 1 : kFields;
    const float h = 124 + float(about.size()) * 34 + (p.typing ? 0.0f : 20.0f) + float(fields_shown) * kFieldH +
                    (p.typing ? 10.0f : 110.0f) + float(note.size()) * 34 + 26;
    const float y = p.typing ? 34.0f : 540 - h * 0.5f - 20;

    Glass face;
    face.tint = rgba(0x16348F, 0.92f);
    face.rim = rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.4f;
    face.glow = 14;
    face.phase = 0.4f;
    glass_block(g, 960, y + h * 0.5f, w, h, 18, 0, 0, 42, face);

    float ly = y + 66;
    g.text_mid(Font::Bold, ts(40), x + pad, ly, kWhite, Align::Left,
               p.index >= 0 ? p.share.name : tr("Add a Network Share"));
    ly += 62;
    for (const std::string &l : about)
    {
        g.text_mid(Font::Regular, ts(26), x + pad, ly, kSoft, Align::Left, l);
        ly += 34;
    }
    ly += p.typing ? 0.0f : 20.0f;

    static const char *const kLabels[kFields] = {"Computer", "Shared Folder", "Folder Inside", "Username",
                                                 "Password"};
    static const char *const kHints[kFields] = {"Its address, like 192.168.1.20, or its name",
                                                "The shared folder's name", "Optional: a folder inside it",
                                                "Optional: empty to connect as a guest", "Optional"};
    for (int field = 0; field < kFields; ++field)
    {
        if (p.typing && field != p.row)
            continue;
        const bool focus = p.row == field;
        const bool typing_here = p.typing && focus;
        const std::string &text = field_text(p.share, field);
        g.text_mid(Font::SemiBold, ts(28), x + pad, ly + 36, focus ? kWhite : kSoft, Align::Left,
                   fit(g, Font::SemiBold, ts(28), tr(kLabels[field]), 300));
        const float fx = x + pad + 320, fw = w - pad * 2 - 320, fh = 64;
        g.panel(fx, ly + 4, fw, fh, rgba(0x07102E, typing_here ? 0.85f : 0.6f), 1, kR,
                focus ? kIcy : rgba(0x3D5AB0, 0.8f), focus ? 2.2f : 1.4f, typing_here ? 8 : 0,
                typing_here ? 0.2f : 0.0f);
        std::string shown = text;
        if (field == 4)
        {
            shown.clear();
            for (std::size_t i = 0; i < text.size(); ++i)
                shown += "\xE2\x80\xA2"; /* a dot each */
        }
        float tx = fx + 22;
        const bool picks = field == 1 && !p.found.empty() && !p.typing;
        const float arrows = picks ? 60.0f : 0.0f;
        if (!shown.empty())
            tx += g.text_mid(Font::SemiBold, ts(28), tx, ly + 36, kWhite, Align::Left,
                             fit(g, Font::SemiBold, ts(28), shown, fw - 60 - arrows));
        else if (!typing_here)
            g.text_mid(Font::Regular, ts(26), tx, ly + 36, rgba(0x8A96C8), Align::Left,
                       fit(g, Font::Regular, ts(26), tr(kHints[field]), fw - 60 - arrows));
        if (picks)
        {
            /* Left and right pick another. */
            const Color c = focus ? kCyan : rgba(0x8A96C8);
            g.glyph(Glyph::Arrow, fx + fw - 52, ly + 36, 18, c, -kPi * 0.5f);
            g.glyph(Glyph::Arrow, fx + fw - 22, ly + 36, 18, c, kPi * 0.5f);
        }
        if (typing_here && std::fmod(time_, 1.0) < 0.6)
            g.panel(tx + 3, ly + 18, 3, 36, kIcy, 1, 0);
        ly += kFieldH;
    }

    if (!p.typing)
    {
        ly += 14;
        const bool editing = p.index >= 0;
        const int buttons = editing ? 4 : 3;
        static const char *const kButtons[4] = {"Find Shared Folders", "Test", "Save", "Remove"};
        const float gap = 18;
        const float bw = (text_w - gap * float(buttons - 1)) / float(buttons);
        for (int b = 0; b < buttons; ++b)
        {
            const bool on = p.row == kButtonsRow && p.button == b;
            const bool danger = b == ButtonRemove;
            const bool dim = busy; /* every button waits for the test or search */
            const float bx = x + pad + float(b) * (bw + gap), bh = 62, by = ly + 30;
            g.panel(bx, by - bh * 0.5f, bw, bh,
                    on ? (danger ? rgba(0xB0305A, 0.92f) : rgba(0x1F63F0, 0.92f)) : rgba(0x07102E, dim ? 0.3f : 0.5f),
                    0.7f, kR, on ? (danger ? kDanger : kIcy) : rgba(0x3D5AB0, 0.8f), on ? 2.2f : 1.4f, on ? 8 : 0,
                    on ? 0.25f : 0.0f);
            g.text_mid(Font::Bold, ts(28), bx + bw * 0.5f, by, on ? kWhite : (dim ? rgba(0x8A96C8) : kSoft),
                       Align::Center, fit(g, Font::Bold, ts(28), title_case(tr(kButtons[b])), bw - 20));
        }
        ly += 96;
    }
    for (const std::string &l : note)
    {
        g.text_mid(Font::SemiBold, ts(26), x + pad, ly, busy ? kIcy : p.message_ok ? rgba(0x7CF0A6) : kDanger,
                   Align::Left, l);
        ly += 34;
    }

    if (p.typing)
        draw_keyboard(p.kb, y + h + 44, face);
    g.set_layer();
    if (dialog_.open)
        return;
    drawing_dialog_ = true;
    if (p.typing)
        draw_keyboard_prompts();
    else if (p.row == 1 && !p.found.empty())
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Type"}}, {{Glyph::Circle, "Cancel"}}, "");
    else if (p.row < kFields)
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Type"}}, {{Glyph::Circle, "Cancel"}}, "");
    else
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Select"}}, {{Glyph::Circle, "Cancel"}}, "");
    drawing_dialog_ = false;
}
} // namespace porpoise::ui
