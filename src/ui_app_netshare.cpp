/* Porpoise UI - a network share's panel: the computers on the network that
 * share files (looked for when it opens, picked with a press), SMB or NFS,
 * the computer (a number pad for its address), the shared folder or export
 * (looked for on the computer, picked with left and right) and, for SMB, how
 * to sign in; a test of the connection. Settings > Games opens it.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_pad.hpp"
#include "ui_app.hpp"

#include <cstdlib>
#include <mutex>

#include "title_threads.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

/* A test, a search for shared folders or a look around the network, on its
 * own thread: the panel looks for its result each frame, and a panel closed
 * meanwhile just lets it finish. */
struct App::ShareJob
{
    enum Kind
    {
        Test,
        Find,
        Scan,
    };
    Kind kind = Test;
    netfs::Share share;
    std::mutex m;
    bool done = false;
    netfs::Problem problem = netfs::Problem::None;
    std::string detail;
    std::vector<std::string> names;
    std::vector<netfs::Found> computers;
};

namespace
{
enum ShareRow
{
    RowNetwork, /* the computers found */
    RowType,    /* SMB or NFS */
    RowComputer,
    RowFolder, /* the shared folder (or export) and any folder inside it */
    RowUser,
    RowPassword,
    RowButtons,
};
enum Button
{
    ButtonTest,
    ButtonSave,
    ButtonRemove,
};

/* The preview looks at the network only when asked (PREVIEW_SHARE_LIVE). */
bool live()
{
#ifdef PORPOISE_HOST_PREVIEW
    static const bool on = std::getenv("PREVIEW_SHARE_LIVE") != nullptr;
    return on;
#else
    return true;
#endif
}

bool is_nfs(const netfs::Share &s)
{
    return s.protocol == "nfs";
}

/* The rows shown: NFS has no sign-in. */
std::vector<int> rows_of(const netfs::Share &s)
{
    if (is_nfs(s))
        return {RowNetwork, RowType, RowComputer, RowFolder, RowButtons};
    return {RowNetwork, RowType, RowComputer, RowFolder, RowUser, RowPassword, RowButtons};
}

bool same_text(const std::string &a, const std::string &b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
            return false;
    return true;
}

/* The computer found that a host names, or null. */
const netfs::Found *online(const std::vector<netfs::Found> &computers, const std::string &host)
{
    if (host.empty())
        return nullptr;
    for (const netfs::Found &f : computers)
        if (f.address == host || (!f.name.empty() && same_text(f.name, host)))
            return &f;
    return nullptr;
}

/* An address (digits and dots) or nothing yet: the number pad. */
bool looks_numeric(const std::string &s)
{
    for (char c : s)
        if (!(c == '.' || (c >= '0' && c <= '9')))
            return false;
    return true;
}

/* The path as shown: the share and the folder inside it. */
std::string path_of(const netfs::Share &s)
{
    std::string folder = s.folder;
    while (!folder.empty() && (folder.front() == '/' || folder.front() == '\\'))
        folder.erase(folder.begin());
    if (folder.empty())
        return s.share;
    if (is_nfs(s))
        return s.share + (!s.share.empty() && s.share.back() == '/' ? "" : "/") + folder;
    std::replace(folder.begin(), folder.end(), '/', '\\');
    return s.share + "\\" + folder;
}

/* The share with the path split into the shared folder and the folder inside
 * it: for SMB at the first slash; for NFS after the longest export found that
 * it starts with (an export can have slashes of its own), else all of it. */
netfs::Share split(const netfs::Share &base, const std::string &path, const std::vector<std::string> &exports)
{
    netfs::Share s = base;
    std::string p = path;
    while (!p.empty() && (p.back() == ' ' || p.back() == '/' || p.back() == '\\'))
        p.pop_back();
    while (!p.empty() && p.front() == ' ')
        p.erase(p.begin());
    s.share.clear();
    s.folder.clear();
    if (is_nfs(s))
    {
        std::replace(p.begin(), p.end(), '\\', '/');
        std::size_t best = 0;
        for (const std::string &e0 : exports)
        {
            std::string e = e0;
            while (e.size() > 1 && e.back() == '/')
                e.pop_back();
            if (e.size() > best && p.compare(0, e.size(), e) == 0 && (p.size() == e.size() || p[e.size()] == '/'))
                best = e.size();
        }
        s.share = best ? p.substr(0, best) : p;
        if (best && best < p.size())
            s.folder = p.substr(best + 1);
        return s;
    }
    while (!p.empty() && (p.front() == '/' || p.front() == '\\'))
        p.erase(p.begin());
    const std::size_t cut = p.find_first_of("/\\");
    s.share = p.substr(0, cut);
    if (cut != std::string::npos)
    {
        s.folder = p.substr(cut + 1);
        std::replace(s.folder.begin(), s.folder.end(), '\\', '/');
    }
    return s;
}

/* The share the path names, without the folder inside it. */
std::string head_of(const netfs::Share &s, const std::string &path, const std::vector<std::string> &found)
{
    return split(s, path, found).share;
}

/* Whether a share found is the one named: SMB's names in any case, NFS's
 * exports with or without a slash at the end. */
bool same_share(const netfs::Share &s, const std::string &found, const std::string &head)
{
    if (!is_nfs(s))
        return same_text(found, head);
    auto trim = [](std::string e) {
        while (e.size() > 1 && e.back() == '/')
            e.pop_back();
        return e;
    };
    return trim(found) == trim(head);
}

/* The found share the path names, or -1. */
int find_share(const netfs::Share &s, const std::vector<std::string> &found, const std::string &path)
{
    const std::string head = head_of(s, path, found);
    for (std::size_t i = 0; i < found.size(); ++i)
        if (same_share(s, found[i], head))
            return int(i);
    return -1;
}
} // namespace

void *App::run_share_job(void *arg)
{
    auto *held = static_cast<std::shared_ptr<ShareJob> *>(arg);
    std::shared_ptr<ShareJob> job = *held;
    delete held;
    std::string detail;
    std::vector<std::string> names;
    std::vector<netfs::Found> computers;
    netfs::Problem problem = netfs::Problem::None;
    switch (job->kind)
    {
    case ShareJob::Find:
        problem = netfs::enumerate(job->share, names, &detail);
        break;
    case ShareJob::Test:
        problem = netfs::test(job->share, &detail);
        break;
    case ShareJob::Scan:
        computers = netfs::discover();
        break;
    }
    std::lock_guard<std::mutex> lock(job->m);
    job->problem = problem;
    job->detail = detail;
    job->names = std::move(names);
    job->computers = std::move(computers);
    job->done = true;
    return nullptr;
}

namespace
{
/* A problem in the player's words. */
std::string problem_text(netfs::Problem problem, const netfs::Share &s, const std::string &detail)
{
    using netfs::Problem;
    const bool nfs = is_nfs(s);
    switch (problem)
    {
    case Problem::None:
        return "";
    case Problem::NoComputer:
        return tr("Enter the computer's address or name first.");
    case Problem::NoShare:
        return nfs ? tr("Enter the export's path, like /volume1/games.")
                   : tr("Enter the shared folder's name, or look for the computer's shared folders.");
    case Problem::UnknownName:
        return trf("Couldn't find a computer called {computer}. Try its address instead (like 192.168.1.20).",
                   {{"computer", s.host}});
    case Problem::Unreachable:
        return nfs ? trf("Couldn't reach {computer}. Check the address, that the computer is on, and that NFS is "
                         "turned on for it.",
                         {{"computer", s.host}})
                   : trf("Couldn't reach {computer}. Check the address, that the computer is on, and that it shares "
                         "files on the network.",
                         {{"computer", s.host}});
    case Problem::SignIn:
        return tr("The computer didn't accept the username or password.");
    case Problem::NoSuchShare:
        return nfs ? trf("{computer} has no export called {name}.", {{"computer", s.host}, {"name", s.share}})
                   : trf("{computer} has no shared folder called {name}.", {{"computer", s.host}, {"name", s.share}});
    case Problem::Denied:
        return nfs ? tr("The computer didn't let Porpoise in. Allow this PS5's address in the export's settings. "
                        "Some NAS boxes also need connections from non-privileged ports allowed (\"insecure\").")
                   : tr("The computer didn't let Porpoise in. Check the folder's sharing permissions, or sign in "
                        "with a username and password.");
    case Problem::NoFolder:
        return trf("There's no folder {folder} in the share.", {{"folder", s.folder}});
    case Problem::NoExports:
        return tr("The computer didn't list its exports. Type the export's path, like /volume1/games.");
    case Problem::Other:
        break;
    }
    return trf("Couldn't connect: {reason}", {{"reason", detail}});
}

/* The text a row types into. */
std::string &field_text(netfs::Share &s, std::string &path, int row)
{
    switch (row)
    {
    case RowComputer:
        return s.host;
    case RowFolder:
        return path;
    case RowUser:
        return s.user;
    default:
        return s.password;
    }
}

std::size_t field_limit(int row)
{
    return row == RowComputer ? 128 : row == RowFolder ? 260 : row == RowUser ? 64 : 128;
}
} // namespace

void App::start_share_job(std::shared_ptr<ShareJob> job)
{
    auto *held = new std::shared_ptr<ShareJob>(std::move(job));
    pthread_t thread;
    if (create_title_thread(&thread, run_share_job, held) == 0)
        pthread_detach(thread);
    else
        run_share_job(held); /* no thread: here, as it would have */
}

#ifdef PORPOISE_HOST_PREVIEW
void App::preview_share_busy(bool find)
{
    share_.job = std::make_shared<ShareJob>();
    share_.job->kind = find ? ShareJob::Find : ShareJob::Test;
    share_.job_started = time_;
}

bool App::preview_share_waiting()
{
    for (const auto &j : {share_.job, share_.scan})
        if (j)
            return true;
    return false;
}

std::string App::preview_share_state()
{
    const SharePanel &p = share_;
    std::string s = "open=" + std::to_string(p.open) + " row=" + std::to_string(p.row) + " host=" + p.share.host +
                    " type=" + (is_nfs(p.share) ? "nfs" : "smb") + " path=" + p.path + " found=[";
    for (const std::string &f : p.found)
        s += f + ",";
    s += "] computers=[";
    for (const netfs::Found &f : p.computers)
        s += f.address + "/" + f.name + (f.smb ? "/smb" : "") + (f.nfs ? "/nfs" : "") + ",";
    s += "] msg=" + p.message;
    return s;
}

void App::preview_share_network(const std::vector<netfs::Found> &computers, bool scanning, bool nfs, int card)
{
    share_.computers = computers;
    share_.scanned = !scanning;
    if (scanning)
    {
        share_.scan = std::make_shared<ShareJob>();
        share_.scan->kind = ShareJob::Scan;
    }
    share_.share.protocol = nfs ? "nfs" : "";
    share_.slide = nfs ? 1.0f : 0.0f;
    share_.card = card;
}
#endif

void App::start_share_scan()
{
    SharePanel &p = share_;
    auto job = std::make_shared<ShareJob>();
    job->kind = ShareJob::Scan;
    p.scan = job;
    p.scanned = false;
    if (live())
        start_share_job(job);
}

void App::start_share_find()
{
    SharePanel &p = share_;
    auto job = std::make_shared<ShareJob>();
    job->kind = ShareJob::Find;
    job->share = p.share;
    p.find_key = p.share.protocol + "\n" + p.share.host + "\n" + p.share.user + "\n" + p.share.password;
    p.job = job;
    p.job_started = time_;
    p.message.clear();
    start_share_job(job);
}

void App::open_share(int index)
{
    share_ = SharePanel{};
    SharePanel &p = share_;
    p.open = true;
    p.index = index;
    const std::vector<netfs::Share> list = netfs::shares();
    if (index >= 0 && index < int(list.size()))
    {
        p.share = list[std::size_t(index)];
        p.saved = p.share;
        p.saved.password.clear();
        p.path = path_of(p.share);
        p.row = RowComputer;
    }
    else
    {
        p.index = -1;
        p.row = RowNetwork;
    }
    p.slide = is_nfs(p.share) ? 1.0f : 0.0f;
    p.button = p.index >= 0 ? ButtonTest : ButtonSave;
    start_share_scan();
    sfx(Sound::DetailsFlip);
}

void App::close_share()
{
    share_.share.password.assign(share_.share.password.size(), '\0');
    share_.find_key.assign(share_.find_key.size(), '\0');
    share_ = SharePanel{};
    sfx(Sound::DetailsFlip);
}

App::Action App::update_share(bool up, bool down, bool left, bool right)
{
    SharePanel &p = share_;
    const std::vector<std::string> no_exports;

    /* The look around the network, done. */
    if (p.scan)
    {
        const std::shared_ptr<ShareJob> held = p.scan;
        std::lock_guard<std::mutex> lock(held->m);
        if (held->done)
        {
            p.computers = held->computers;
            p.scanned = true;
            p.card = std::min(p.card, int(p.computers.size()));
            p.scan.reset();
        }
    }

    bool busy = false;
    if (p.job)
    {
        /* Held here too: p.job may let go of it below, inside the lock. */
        const std::shared_ptr<ShareJob> held = p.job;
        std::lock_guard<std::mutex> lock(held->m);
        if (held->done)
        {
            const bool current = held->share.host == p.share.host && held->share.protocol == p.share.protocol;
            if (held->kind == ShareJob::Find && !current)
            {
                /* The computer or type changed meanwhile: looked for again below. */
                p.find_key.clear();
            }
            else if (held->kind == ShareJob::Find)
            {
                const bool ok = held->problem == netfs::Problem::None;
                const bool nfs = is_nfs(p.share);
                if (ok && !held->names.empty())
                {
                    p.found = held->names;
                    /* Picked for the player only when there's nothing yet: a
                     * share typed or saved stays (it may be one the computer
                     * doesn't list, like Games$). */
                    const bool typing_it = p.typing && p.row == RowFolder;
                    if (!typing_it && head_of(p.share, p.path, p.found).empty())
                        p.path = p.found.front();
                    p.message = p.found.size() == 1
                                    ? (nfs ? trf("Found the export {name}.", {{"name", p.found.front()}})
                                           : trf("Found the shared folder {name}.", {{"name", p.found.front()}}))
                                    : (nfs ? trf("Found {n} exports. Left and right pick one.",
                                                 {{"n", std::to_string(p.found.size())}})
                                           : trf("Found {n} shared folders. Left and right pick one.",
                                                 {{"n", std::to_string(p.found.size())}}));
                    p.message_ok = true;
                    sfx(Sound::LaunchGame);
                }
                else if (ok)
                {
                    p.message = nfs ? tr("The computer answered, but has no exports Porpoise can see.")
                                    : tr("The computer answered, but shares no folders Porpoise can see.");
                    p.message_ok = false;
                    sfx(Sound::MovingTab);
                }
                else
                {
                    p.message = problem_text(held->problem, held->share, held->detail);
                    if (held->problem == netfs::Problem::SignIn || held->problem == netfs::Problem::Denied)
                    {
                        if (!nfs && p.share.user.empty())
                        {
                            /* Windows mostly: no guests. Sign in, then they're looked for again. */
                            p.message = tr("This computer wants a username and password. Enter them below and "
                                           "Porpoise looks for its shared folders again.");
                            if (!p.typing)
                                p.row = RowUser;
                        }
                    }
                    p.message_ok = false;
                    sfx(Sound::MovingTab);
                }
            }
            else
            {
                p.message_ok = held->problem == netfs::Problem::None;
                p.message = p.message_ok ? tr("Connected. Porpoise can read this folder.")
                                         : problem_text(held->problem, held->share, held->detail);
                sfx(p.message_ok ? Sound::LaunchGame : Sound::MovingTab);
            }
            p.job.reset();
        }
        else if (time_ - p.job_started > 45.0)
        {
            /* No answer for this long: stop waiting (the try ends by itself
             * on its own thread). */
            p.message = problem_text(netfs::Problem::Unreachable, held->share, "");
            p.message_ok = false;
            sfx(Sound::MovingTab);
            p.job.reset();
        }
        else
            busy = true;
    }

    if (p.typing)
    {
        const int row = p.row;
        const bool spaces = row != RowComputer;
        if (keyboard_update(p.kb, field_text(p.share, p.path, row), field_limit(row), spaces, up, down, left, right))
        {
            p.typing = false;
            if (row == RowComputer)
            {
                /* Another computer: its folders are looked for (below). */
                p.found.clear();
                p.other_found.clear();
                p.find_key.clear();
                p.row = p.share.host.empty() ? RowComputer : RowFolder;
            }
            else if (row == RowUser)
                p.row = RowPassword;
            else if (row == RowPassword && p.found.empty())
                p.row = RowFolder;
            else
            {
                p.row = RowButtons;
                p.button = ButtonSave;
            }
        }
        return Action::None;
    }

    /* The computer's shared folders, looked for by themselves once there's
     * a computer (and again when the sign-in changes, once it's entered:
     * not a try with half of it). */
    if (live() && !busy && p.found.empty() && !p.share.host.empty() && p.row != RowUser && p.row != RowPassword)
    {
        const std::string key = p.share.protocol + "\n" + p.share.host + "\n" + p.share.user + "\n" + p.share.password;
        if (key != p.find_key)
        {
            start_share_find();
            busy = true;
        }
    }

    if (pressed(BtnCircle))
    {
        close_share();
        return Action::None;
    }
    const std::vector<int> rows = rows_of(p.share);
    auto at = std::find(rows.begin(), rows.end(), p.row);
    if (at == rows.end())
        at = rows.begin();
    if (up && at != rows.begin())
    {
        p.row = *(at - 1);
        sfx(Sound::MenuScroll);
    }
    if (down && at + 1 != rows.end())
    {
        p.row = *(at + 1);
        sfx(Sound::MenuScroll);
    }
    if (up || down)
        return Action::None;

    auto switch_type = [&](bool nfs) {
        if (is_nfs(p.share) == nfs)
            return;
        /* Each type keeps its own folder and what was found, for coming back. */
        std::swap(p.path, p.other_path);
        std::swap(p.found, p.other_found);
        p.share.protocol = nfs ? "nfs" : "";
        p.message.clear();
        sfx(Sound::MovingTab);
    };

    switch (p.row)
    {
    case RowNetwork:
    {
        /* The computers, then Look Again (looking still: nothing to press). */
        const int cards = int(p.computers.size()) + (p.scanned || !p.computers.empty() ? 1 : 0);
        if ((left || right) && cards > 0)
        {
            p.card = std::clamp(p.card + (left ? -1 : 1), 0, cards - 1);
            sfx(Sound::MenuScroll);
        }
        if (!pressed(BtnCross) || cards == 0)
            return Action::None;
        if (p.card >= int(p.computers.size()))
        {
            if (!p.scanned)
                return Action::None;
            start_share_scan();
            p.card = 0;
            sfx(Sound::DetailsFlip);
            return Action::None;
        }
        const netfs::Found &f = p.computers[std::size_t(p.card)];
        const bool nfs = f.nfs && (!f.smb || is_nfs(p.share));
        if (online(p.computers, p.share.host) != &f)
        {
            /* Another computer (the same one, by its name or address, keeps
             * what's typed). */
            p.share.host = f.address;
            p.found.clear();
            p.other_found.clear();
            p.path.clear();
            p.other_path.clear();
            p.find_key.clear();
        }
        switch_type(nfs);
        p.row = RowFolder;
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    case RowType:
        if (left || right || pressed(BtnCross))
            switch_type(left ? false : right ? true : !is_nfs(p.share));
        return Action::None;
    case RowFolder:
        if ((left || right) && !p.found.empty())
        {
            /* The shared folders found: pick one. */
            int i = find_share(p.share, p.found, p.path);
            const int n = int(p.found.size());
            i = left ? (i <= 0 ? n - 1 : i - 1) : (i + 1) % n;
            p.path = p.found[std::size_t(i)];
            sfx(Sound::MenuScroll);
        }
        if (pressed(BtnTriangle) && !busy && !p.share.host.empty())
        {
            /* Look again. */
            p.found.clear();
            start_share_find();
            sfx(Sound::DetailsFlip);
            return Action::None;
        }
        break;
    case RowButtons:
    {
        const int buttons = p.index >= 0 ? 3 : 2;
        if (left || right)
        {
            p.button = (p.button + (left ? buttons - 1 : 1)) % buttons;
            sfx(Sound::MenuScroll);
        }
        break;
    }
    default:
        break;
    }
    if (!pressed(BtnCross))
        return Action::None;

    if (p.row != RowButtons)
    {
        p.typing = true;
        p.kb = Keyboard{};
        p.kb.numeric = p.row == RowComputer && looks_numeric(p.share.host);
        if (p.kb.numeric)
        {
            p.kb.kr = 0;
            p.kb.kc = 0;
        }
        p.message.clear();
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    /* A search going on holds up only Test (Save and Remove don't need it). */
    bool finding = false;
    if (p.job)
    {
        std::lock_guard<std::mutex> lock(p.job->m);
        finding = p.job->kind == ShareJob::Find;
    }
    if (busy && (p.button == ButtonTest || !finding))
        return Action::None;
    netfs::Share whole = split(p.share, p.path, is_nfs(p.share) ? p.found : no_exports);
    if (p.index >= 0 && p.share.protocol == p.saved.protocol && p.path == path_of(p.saved))
    {
        /* The folder as saved: split as it was (an NFS export the computer
         * doesn't list would otherwise take the folder inside it along). */
        whole.share = p.saved.share;
        whole.folder = p.saved.folder;
    }
    const bool has_host = !whole.host.empty();
    const bool ready = has_host && !whole.share.empty();
    auto not_ready = [&] {
        p.message = problem_text(has_host ? netfs::Problem::NoShare : netfs::Problem::NoComputer, whole, "");
        p.message_ok = false;
        p.row = has_host ? RowFolder : RowComputer;
        sfx(Sound::MovingTab);
    };
    switch (Button(p.button))
    {
    case ButtonTest:
    {
        if (!ready)
        {
            not_ready();
            return Action::None;
        }
        auto job = std::make_shared<ShareJob>();
        job->kind = ShareJob::Test;
        job->share = whole;
        p.job = job;
        p.job_started = time_;
        p.message.clear();
        start_share_job(job);
        sfx(Sound::DetailsFlip);
        return Action::None;
    }
    case ButtonSave:
    {
        if (!ready)
        {
            not_ready();
            return Action::None;
        }
        std::vector<netfs::Share> list = netfs::shares();
        netfs::Share s = whole;
        if (is_nfs(s))
        {
            s.user.clear();
            s.password.clear();
        }
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

namespace
{
/* Three dots bouncing in turn: working. */
void draw_dots(Gfx &g, float x, float y, double time, bool still, Color c)
{
    for (int i = 0; i < 3; ++i)
    {
        const float wave = still ? 0.0f : std::max(0.0f, std::sin(float(time) * 7.0f - float(i) * 0.9f));
        const float cx = x + 9 + float(i) * 24, cy = y - 9.0f * wave;
        g.panel(cx - 8, cy - 8, 16, 16, with_alpha(c, 0.4f + 0.6f * (still ? 1.0f : wave)), 1, 8);
    }
}

/* A small screen on a stand: a computer. */
void draw_computer(Gfx &g, float cx, float cy, Color c)
{
    g.panel(cx - 22, cy - 18, 44, 30, Color{0, 0, 0, 0}, 1, 4, c, 3);
    g.panel(cx - 3, cy + 12, 6, 7, c, 1, 0);
    g.panel(cx - 13, cy + 19, 26, 4, c, 1, 2);
}

/* A small rounded tag: "SMB", "NFS". */
float draw_tag(Gfx &g, float x, float cy, float size, const std::string &text, Color fill, Color ink)
{
    const float tw = g.measure(Font::Bold, size, text) + 18;
    g.panel(x, cy - 14, tw, 28, fill, 1, 14);
    g.text_mid(Font::Bold, size, x + tw * 0.5f, cy, ink, Align::Center, text);
    return tw;
}
} // namespace

void App::draw_share()
{
    if (!share_.open)
        return;
    SharePanel &p = share_;
    Gfx &g = *g_;
    const bool still = settings_ && settings_->reduced_motion;
    p.anim = still ? 1.0f : std::min(1.0f, p.anim + 1.0f / 10.0f);
    const float t = ease_out(p.anim);
    const bool nfs = is_nfs(p.share);
    p.slide = still ? (nfs ? 1.0f : 0.0f) : smooth(p.slide, nfs ? 1.0f : 0.0f, 1.0 / 60.0, 16.0f);
    bool busy = false, finding = false, scanning = false;
    if (p.job)
    {
        std::lock_guard<std::mutex> lock(p.job->m);
        busy = !p.job->done;
        finding = busy && p.job->kind == ShareJob::Find;
    }
    if (p.scan)
    {
        std::lock_guard<std::mutex> lock(p.scan->m);
        scanning = !p.scan->done;
    }
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.62f * t), 1, 0);
    g.set_layer(0, 24.0f * (1.0f - t), t);

    const float w = 1180, x = 960 - w * 0.5f, pad = 56;
    const float text_w = w - pad * 2;
    const Color hint = rgba(0x8A96C8);
    const Color good = rgba(0x7CF0A6);
    std::string note_text = finding ? (nfs ? tr("Looking for exports\xE2\x80\xA6") : tr("Looking for shared folders\xE2\x80\xA6"))
                            : busy  ? tr("Connecting\xE2\x80\xA6")
                                    : p.message;
    const auto note = note_text.empty() ? std::vector<std::string>{} : wrap(g, Font::SemiBold, ts(26), note_text, text_w, 3);

    constexpr float kFieldH = 80, kCardsH = 172, kTypeH = 88;
    const std::vector<int> rows = rows_of(p.share);
    float h = 124;
    if (p.typing)
        h += kFieldH + 10;
    else
        for (int r : rows)
            h += r == RowNetwork ? kCardsH : r == RowType ? kTypeH : r == RowButtons ? 110 : kFieldH;
    h += float(std::max<std::size_t>(note.size(), 1)) * 34 + 26;
    const float y = p.typing ? 34.0f : std::max(24.0f, 540 - h * 0.5f - 20);

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
               fit(g, Font::Bold, ts(40), p.index >= 0 ? p.share.name : tr("Add a Network Share"), text_w - 220));
    if (!p.typing)
        g.text_mid(Font::Regular, ts(24), x + w - pad, ly, hint, Align::Right,
                   fit(g, Font::Regular, ts(24), tr("Porpoise only reads from it."), 360));
    ly += 58;

    const float label_w = 300, fx = x + pad + 320, fw = w - pad * 2 - 320;
    auto label = [&](float ry, const std::string &text, bool focus) {
        g.text_mid(Font::SemiBold, ts(28), x + pad, ry, focus ? kWhite : kSoft, Align::Left,
                   fit(g, Font::SemiBold, ts(28), text, label_w));
    };

    for (int row : rows)
    {
        if (p.typing && row != p.row)
            continue;
        const bool focus = p.row == row;
        if (row == RowNetwork)
        {
            /* The computers that answered, as cards; Look Again after them. */
            label(ly + 22, tr("On Your Network"), focus);
            constexpr int kVisible = 4;
            const float cy = ly + 52, ch = 104, gap = 16, cw = (text_w - gap * float(kVisible - 1)) / float(kVisible);
            const int n = int(p.computers.size());
            const int cards = n + 1; /* Look Again, or the dots while looking */
            if (n == 0 && p.scanned)
            {
                /* Nothing answered: Look Again, and what to do. */
                const bool on = focus;
                g.panel(x + pad, cy, cw, ch, on ? rgba(0x1F63F0, 0.92f) : rgba(0x07102E, 0.55f), 1, kR,
                        on ? kIcy : rgba(0x3D5AB0, 0.8f), on ? 2.2f : 1.2f, on ? 8 : 0, on ? 0.25f : 0.0f);
                g.text_mid(Font::Bold, ts(26), x + pad + cw * 0.5f, cy + ch * 0.5f, on ? kWhite : kSoft,
                           Align::Center, fit(g, Font::Bold, ts(26), tr("Look Again"), cw - 24));
                const auto lines = wrap(g, Font::Regular, ts(24),
                                        tr("No computers answered. Check that file sharing is on, or enter the "
                                           "computer's address below."),
                                        text_w - cw - 32, 3);
                float ty = cy + ch * 0.5f - float(lines.size() - 1) * 15.0f;
                for (const std::string &l : lines)
                {
                    g.text_mid(Font::Regular, ts(24), x + pad + cw + 32, ty, kSoft, Align::Left, l);
                    ty += 30;
                }
            }
            else if (n == 0)
            {
                g.panel(x + pad, cy, text_w, ch, rgba(0x07102E, 0.45f), 1, kR, rgba(0x3D5AB0, 0.6f), 1.2f);
                draw_dots(g, x + pad + 30, cy + ch * 0.5f, time_, still, kIcy);
                g.text_mid(Font::SemiBold, ts(26), x + pad + 114, cy + ch * 0.5f, kIcy, Align::Left,
                           fit(g, Font::SemiBold, ts(26), tr("Looking for computers that share files"), text_w - 140));
            }
            else
            {
                /* Four at a time, moved along to keep the one in focus in
                 * view; a mark at an end with more past it. */
                const int first0 = int(p.cards_x);
                const int first = std::clamp(p.card < first0 ? p.card : p.card >= first0 + kVisible ? p.card - kVisible + 1 : first0,
                                             0, std::max(0, cards - kVisible));
                p.cards_x = float(first);
                if (first > 0)
                    g.glyph(Glyph::Arrow, x + pad - 24, cy + ch * 0.5f, 16, hint, -kPi * 0.5f);
                if (first + kVisible < cards)
                    g.glyph(Glyph::Arrow, x + w - pad + 24, cy + ch * 0.5f, 16, hint, kPi * 0.5f);
                for (int i = first; i < cards && i < first + kVisible; ++i)
                {
                    const float cx = x + pad + float(i - first) * (cw + gap);
                    const bool on = focus && p.card == i;
                    const bool chosen = i < n && online(p.computers, p.share.host) == &p.computers[std::size_t(i)];
                    g.panel(cx, cy, cw, ch, on ? rgba(0x1F63F0, 0.92f) : rgba(0x07102E, 0.55f), 1, kR,
                            on ? kIcy : chosen ? good : rgba(0x3D5AB0, 0.8f), on || chosen ? 2.2f : 1.2f, on ? 8 : 0,
                            on ? 0.25f : 0.0f);
                    if (i == n)
                    {
                        /* Look again (or still looking). */
                        if (scanning)
                            draw_dots(g, cx + cw * 0.5f - 33, cy + ch * 0.5f, time_, still, on ? kWhite : kIcy);
                        else
                            g.text_mid(Font::Bold, ts(26), cx + cw * 0.5f, cy + ch * 0.5f, on ? kWhite : kSoft,
                                       Align::Center, fit(g, Font::Bold, ts(26), tr("Look Again"), cw - 24));
                        continue;
                    }
                    const netfs::Found &f = p.computers[std::size_t(i)];
                    draw_computer(g, cx + 38, cy + 36, on ? kWhite : chosen ? good : kIcy);
                    const float tx = cx + 74, tw = cw - 86;
                    const std::string title = f.name.empty() ? f.address : f.name;
                    g.text_mid(Font::Bold, ts(26), tx, cy + 28, kWhite, Align::Left, fit(g, Font::Bold, ts(26), title, tw));
                    if (!f.name.empty())
                        g.text_mid(Font::Regular, ts(21), tx, cy + 56, on ? kWhite : kSoft, Align::Left,
                                   fit(g, Font::Regular, ts(21), f.address, tw));
                    float bx = cx + 18;
                    const Color tag_fill = on ? rgba(0xFFFFFF, 0.92f) : rgba(0x3D5AB0, 0.9f);
                    const Color tag_ink = on ? rgba(0x1F63F0) : kWhite;
                    if (f.smb)
                        bx += draw_tag(g, bx, cy + ch - 22, ts(18), "SMB", tag_fill, tag_ink) + 8;
                    if (f.nfs)
                        draw_tag(g, bx, cy + ch - 22, ts(18), "NFS", tag_fill, tag_ink);
                }
            }
            ly += kCardsH;
            continue;
        }
        if (row == RowType)
        {
            /* SMB or NFS: two halves, the highlight sliding between them. */
            label(ly + 40, tr("Share Type"), focus);
            const float sy = ly + 6, sh = 68, half = fw * 0.5f;
            g.panel(fx, sy, fw, sh, rgba(0x07102E, 0.6f), 1, kR, focus ? kIcy : rgba(0x3D5AB0, 0.8f),
                    focus ? 2.2f : 1.4f);
            const float hx = fx + 5 + p.slide * half;
            g.panel(hx, sy + 5, half - 10, sh - 10, focus ? rgba(0x1F63F0, 0.95f) : rgba(0x3D5AB0, 0.75f), 1,
                    std::max(2.0f, kR - 4), focus ? kIcy : Color{0, 0, 0, 0}, focus ? 1.6f : 0.0f, focus ? 6 : 0,
                    focus ? 0.2f : 0.0f);
            static const char *const kNames[2] = {"SMB", "NFS"};
            static const char *const kFor[2] = {"Windows, Mac, most NAS", "Linux, NAS"};
            for (int k = 0; k < 2; ++k)
            {
                const bool sel = (k == 1) == nfs;
                const float cx = fx + half * float(k) + half * 0.5f;
                const std::string name = kNames[k];
                const std::string sub = tr(kFor[k]);
                const float nw = g.measure(Font::Bold, ts(28), name);
                const std::string sub_fit = fit(g, Font::Regular, ts(22), sub, half - nw - 64);
                const float sw = g.measure(Font::Regular, ts(22), sub_fit);
                const float left = cx - (nw + 14 + sw) * 0.5f;
                g.text_mid(Font::Bold, ts(28), left, sy + sh * 0.5f, sel ? kWhite : kSoft, Align::Left, name);
                g.text_mid(Font::Regular, ts(22), left + nw + 14, sy + sh * 0.5f, sel ? kWhite : hint, Align::Left,
                           sub_fit);
            }
            ly += kTypeH;
            continue;
        }
        if (row == RowButtons)
        {
            const bool editing = p.index >= 0;
            const int buttons = editing ? 3 : 2;
            static const char *const kButtons[3] = {"Test", "Save", "Remove"};
            const float gap = 18;
            const float bw = (text_w - gap * float(buttons - 1)) / float(buttons);
            for (int b = 0; b < buttons; ++b)
            {
                const bool on = focus && p.button == b;
                const bool danger = b == ButtonRemove;
                const bool dim = busy && (b == ButtonTest || !finding); /* waiting for the test or search */
                const float bx = x + pad + float(b) * (bw + gap), bh = 62, by = ly + 44;
                g.panel(bx, by - bh * 0.5f, bw, bh,
                        on ? (danger ? rgba(0xB0305A, 0.92f) : rgba(0x1F63F0, 0.92f))
                           : rgba(0x07102E, dim ? 0.3f : 0.5f),
                        0.7f, kR, on ? (danger ? kDanger : kIcy) : rgba(0x3D5AB0, 0.8f), on ? 2.2f : 1.4f, on ? 8 : 0,
                        on ? 0.25f : 0.0f);
                g.text_mid(Font::Bold, ts(28), bx + bw * 0.5f, by, on ? kWhite : (dim ? hint : kSoft), Align::Center,
                           fit(g, Font::Bold, ts(28), title_case(tr(kButtons[b])), bw - 20));
            }
            ly += 110;
            continue;
        }

        /* A field. */
        const bool typing_here = p.typing && focus;
        const std::string &text = field_text(p.share, p.path, row);
        const char *name = row == RowComputer ? "Computer"
                           : row == RowFolder ? (nfs ? "Export" : "Shared Folder")
                           : row == RowUser   ? "Username"
                                              : "Password";
        const char *hint_text = row == RowComputer ? "Its address, like 192.168.1.20, or its name"
                                : row == RowFolder ? (nfs ? "Its path, like /volume1/games"
                                                          : "Its name, and a folder inside it if you like")
                                : row == RowUser   ? "Optional: empty to connect as a guest"
                                                   : "Optional";
        label(ly + 36, tr(name), focus);
        const float fh = 64;
        g.panel(fx, ly + 4, fw, fh, rgba(0x07102E, typing_here ? 0.85f : 0.6f), 1, kR,
                focus ? kIcy : rgba(0x3D5AB0, 0.8f), focus ? 2.2f : 1.4f, typing_here ? 8 : 0,
                typing_here ? 0.2f : 0.0f);
        std::string shown = text;
        if (row == RowPassword)
        {
            shown.clear();
            for (std::size_t i = 0; i < text.size(); ++i)
                shown += "\xE2\x80\xA2"; /* a dot each */
        }
        float tx = fx + 22;
        const bool picks = row == RowFolder && !p.found.empty() && !p.typing;
        const bool looking = row == RowFolder && finding && !p.typing;
        /* On the right: arrows to pick a shared folder, dots while they're
         * looked for, or that the computer is on the network. */
        float right_w = 0;
        const netfs::Found *on_net = row == RowComputer && !p.typing ? online(p.computers, p.share.host) : nullptr;
        std::string online_text;
        if (picks)
            right_w = 70;
        else if (looking)
            right_w = 90;
        else if (on_net)
        {
            online_text = tr("On the network");
            right_w = std::min(260.0f, g.measure(Font::SemiBold, ts(22), online_text) + 46);
        }
        if (!shown.empty())
            tx += g.text_mid(Font::SemiBold, ts(28), tx, ly + 36, kWhite, Align::Left,
                             fit(g, Font::SemiBold, ts(28), shown, fw - 60 - right_w));
        else if (!typing_here)
            g.text_mid(Font::Regular, ts(26), tx, ly + 36, hint, Align::Left,
                       fit(g, Font::Regular, ts(26), tr(hint_text), fw - 60 - right_w));
        if (picks)
        {
            const Color c = focus ? kCyan : hint;
            g.glyph(Glyph::Arrow, fx + fw - 52, ly + 36, 18, c, -kPi * 0.5f);
            g.glyph(Glyph::Arrow, fx + fw - 22, ly + 36, 18, c, kPi * 0.5f);
        }
        else if (looking)
            draw_dots(g, fx + fw - 92, ly + 36, time_, still, kIcy);
        else if (on_net)
        {
            g.panel(fx + fw - right_w, ly + 29, 14, 14, good, 1, 7);
            g.text_mid(Font::SemiBold, ts(22), fx + fw - right_w + 24, ly + 36, good, Align::Left,
                       fit(g, Font::SemiBold, ts(22), online_text, right_w - 40));
        }
        if (typing_here && std::fmod(time_, 1.0) < 0.6)
            g.panel(tx + 3, ly + 18, 3, 36, kIcy, 1, 0);
        ly += kFieldH;
    }

    if (busy && !note.empty())
    {
        /* Working: the dots, then what it's doing (its own "..." left off,
         * the dots say it). */
        draw_dots(g, x + pad, ly, time_, still, kIcy);
        std::string text = note_text;
        const std::string ellipsis = "\xE2\x80\xA6";
        if (text.size() >= ellipsis.size() && text.compare(text.size() - ellipsis.size(), ellipsis.size(), ellipsis) == 0)
            text.resize(text.size() - ellipsis.size());
        g.text_mid(Font::SemiBold, ts(26), x + pad + 84, ly, kIcy, Align::Left,
                   fit(g, Font::SemiBold, ts(26), text, text_w - 84));
    }
    else
        for (const std::string &l : note)
        {
            g.text_mid(Font::SemiBold, ts(26), x + pad, ly, p.message_ok ? good : kDanger, Align::Left, l);
            ly += 34;
        }

    if (p.typing)
        draw_keyboard(p.kb, y + h + 44, face);
    g.set_layer();
    if (dialog_.open)
        return;
    drawing_dialog_ = true;
    if (p.typing)
        draw_keyboard_prompts(p.kb.numeric);
    else if (p.row == RowNetwork)
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Select"}}, {{Glyph::Circle, "Cancel"}}, "");
    else if (p.row == RowType)
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Switch"}}, {{Glyph::Circle, "Cancel"}}, "");
    else if (p.row == RowFolder && !p.share.host.empty())
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Type"}, {Glyph::Triangle, "Look Again"}},
                     {{Glyph::Circle, "Cancel"}}, "");
    else if (p.row != RowButtons)
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Type"}}, {{Glyph::Circle, "Cancel"}}, "");
    else
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Select"}}, {{Glyph::Circle, "Cancel"}}, "");
    drawing_dialog_ = false;
}
} // namespace porpoise::ui
