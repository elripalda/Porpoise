/* Porpoise - RetroAchievements (see porpoise_ra.hpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The account panel and the trophy-style popups follow PS5SX2 1.9 (Spyros,
 * with Gabriel Fonseca's groundwork): softcore only, a token instead of the
 * password, unlocks as PS5 toasts with the badge and the trophy sound. */
#include "porpoise_ra.hpp"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include "porpoise_http.hpp"
#include "porpoise_notify.hpp"
#include "title_threads.hpp"
#include "trace.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

extern "C" void *ps5_core_dlsym(void *handle, const char *name);

namespace porpoise::ra
{
namespace
{
using porpoise::ui::tr;
using porpoise::ui::trf;

constexpr const char *kServer = "https://retroachievements.org/dorequest.php";

std::mutex g_lock;
std::string g_path; /* <data>/retroachievements.ini */
Account g_account;
std::string g_token;
bool g_login_done = false;
std::string g_agent;

/* The game's connection, made at its first request and closed with it. */
std::mutex g_session_lock;
porpoise::http::Session *g_session = nullptr;
std::atomic<bool> g_unreachable_said{false}, g_error_said{false};

/* The game's list, kept for the library, and its badges. */
std::string g_lists;           /* <data>/achievements */
std::string g_disc_id;         /* the game being played */
std::atomic<void *> g_core{nullptr};
std::atomic<bool> g_dirty{false};
std::chrono::steady_clock::time_point g_refreshed{};
std::mutex g_toast_lock;
std::vector<GameToast> g_toasts; /* drawn over the game, oldest first */
std::mutex g_badge_lock;
std::vector<std::pair<std::string, std::string>> g_badges; /* url, file: still to fetch */
bool g_badge_worker = false;

const std::string &agent()
{
    if (g_agent.empty())
        g_agent = std::string("Porpoise/") + porpoise::ui::look::kVersion + " (PS5) rcheevos/12.2";
    return g_agent;
}

std::string url_encode(const std::string &s)
{
    std::string out;
    for (const unsigned char c : s)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += char(c);
        else
        {
            char hex[4];
            std::snprintf(hex, sizeof hex, "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

/* A field of a flat JSON answer: a string's text, or a number/true/false as written. */
std::string json_field(const std::string &json, const char *key)
{
    const std::string want = std::string("\"") + key + "\"";
    std::size_t at = json.find(want);
    if (at == std::string::npos)
        return "";
    at = json.find(':', at + want.size());
    if (at == std::string::npos)
        return "";
    ++at;
    while (at < json.size() && std::isspace(static_cast<unsigned char>(json[at])))
        ++at;
    if (at >= json.size())
        return "";
    if (json[at] != '"')
    {
        std::size_t end = at;
        while (end < json.size() && json[end] != ',' && json[end] != '}' && !std::isspace(static_cast<unsigned char>(json[end])))
            ++end;
        return json.substr(at, end - at);
    }
    std::string out;
    for (std::size_t i = at + 1; i < json.size(); ++i)
    {
        const char c = json[i];
        if (c == '"')
            break;
        if (c == '\\' && i + 1 < json.size())
        {
            const char e = json[++i];
            if (e == 'n')
                out += '\n';
            else if (e == 'u' && i + 4 < json.size())
            {
                const unsigned cp = unsigned(std::strtoul(json.substr(i + 1, 4).c_str(), nullptr, 16));
                i += 4;
                if (cp < 0x80)
                    out += char(cp);
                else if (cp < 0x800)
                {
                    out += char(0xC0 | (cp >> 6));
                    out += char(0x80 | (cp & 0x3F));
                }
                else
                {
                    out += char(0xE0 | (cp >> 12));
                    out += char(0x80 | ((cp >> 6) & 0x3F));
                    out += char(0x80 | (cp & 0x3F));
                }
            }
            else
                out += e;
            continue;
        }
        out += c;
    }
    return out;
}

/* Under g_lock. */
void save_locked()
{
    if (g_path.empty())
        return;
    if (!g_account.signed_in)
    {
        std::remove(g_path.c_str());
        return;
    }
    const std::string tmp = g_path + ".tmp";
    if (std::FILE *f = std::fopen(tmp.c_str(), "w"))
    {
        std::fprintf(f, "# RetroAchievements: the account Porpoise signs in with (a token, not the password).\n");
        std::fprintf(f, "user=%s\ntoken=%s\npoints=%d\n", g_account.user.c_str(), g_token.c_str(), g_account.points);
        const bool ok = std::fclose(f) == 0;
        chmod(tmp.c_str(), 0600);
        if (ok)
            std::rename(tmp.c_str(), g_path.c_str());
        else
            std::remove(tmp.c_str());
    }
}

struct Login
{
    std::string user, password;
};

void *login_worker(void *arg)
{
    Login *login = static_cast<Login *>(arg);
    std::string form = "r=login2&u=" + url_encode(login->user) + "&p=" + url_encode(login->password);
    std::fill(login->password.begin(), login->password.end(), '\0');
    std::vector<std::uint8_t> body;
    int status = -1;
    {
        porpoise::http::Session http("porpoise-ra", agent().c_str());
        if (http.init())
            status = http.request(kServer, &form, body);
        http.term();
    }
    std::fill(form.begin(), form.end(), '\0');
    const std::string json(body.begin(), body.end());
    std::lock_guard<std::mutex> lock(g_lock);
    g_account.busy = false;
    g_login_done = true;
    const std::string token = json_field(json, "Token");
    if (status > 0 && json_field(json, "Success") == "true" && !token.empty())
    {
        const std::string user = json_field(json, "User");
        g_account.signed_in = true;
        g_account.user = user.empty() ? login->user : user;
        g_account.points = std::atoi(json_field(json, "SoftcoreScore").c_str());
        g_account.message.clear();
        g_token = token;
        save_locked();
        ps5::debug::mark("ra: signed in");
    }
    else if (status <= 0)
        g_account.message = "Couldn't reach RetroAchievements. Check the PS5's internet connection.";
    else if (json_field(json, "Code") == "invalid_credentials" || status == 401)
        g_account.message = "Wrong username or password.";
    else
    {
        /* Never shown with the server's own words: they could repeat what was sent. */
        g_account.message = "RetroAchievements didn't accept the sign-in. Try again later.";
        ps5::debug::mark_value("ra: sign-in refused, status", status);
    }
    delete login;
    return nullptr;
}

/* ---- the core's side ------------------------------------------------------------------------ */

int core_request(const char *url, const char *post_data, void *ctx, void (*sink)(void *, const char *, std::size_t))
{
    std::lock_guard<std::mutex> lock(g_session_lock);
    if (!g_session)
    {
        g_session = new porpoise::http::Session("porpoise-ra", agent().c_str());
        if (!g_session->init())
        {
            delete g_session; /* lets go of the network for the next try */
            g_session = nullptr;
            return -1;
        }
    }
    const std::string form = post_data ? post_data : "";
    std::vector<std::uint8_t> body;
    const int status = g_session->request(url, post_data ? &form : nullptr, body);
    if (status < 0)
    {
        /* A dropped connection: a fresh session for the retry. */
        delete g_session;
        g_session = nullptr;
        return -1;
    }
    if (!body.empty())
        sink(ctx, reinterpret_cast<const char *>(body.data()), body.size());
    return status;
}

/* ---- the game's list and its badges --------------------------------------------------------- */

using ListFn = int (*)(char *buf, int size);

bool list_text(std::string &out)
{
    void *core = g_core.load();
    auto list = reinterpret_cast<ListFn>(core ? ps5_core_dlsym(core, "porpoise_ra_list") : nullptr);
    if (!list)
        return false;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        const int need = list(nullptr, 0);
        if (need <= 1)
            return false;
        std::vector<char> buf(std::size_t(need) + 4096); /* room if it grows meanwhile */
        const int got = list(buf.data(), int(buf.size()));
        if (got > 0 && got <= int(buf.size()))
        {
            out.assign(buf.data(), std::size_t(got - 1));
            return true;
        }
    }
    return false;
}

std::string kept_path(const std::string &disc_id)
{
    std::string safe = disc_id;
    for (char &c : safe)
        if (!std::isalnum(static_cast<unsigned char>(c)))
            c = '_';
    return g_lists + "/" + safe + ".txt";
}

void *badge_worker(void *)
{
    for (;;)
    {
        std::pair<std::string, std::string> job;
        {
            std::lock_guard<std::mutex> lock(g_badge_lock);
            if (g_badges.empty() || !g_core.load())
            {
                g_badges.clear();
                g_badge_worker = false;
                return nullptr;
            }
            job = g_badges.back();
            g_badges.pop_back();
        }
        struct stat st;
        if (stat(job.second.c_str(), &st) == 0 && st.st_size > 0)
            continue;
        std::vector<std::uint8_t> png;
        int status = -1;
        {
            /* The game's connection (one at a time across Porpoise). */
            std::lock_guard<std::mutex> lock(g_session_lock);
            if (!g_core.load())
                continue; /* the game has gone: end_game closed its connection */
            if (!g_session)
            {
                g_session = new porpoise::http::Session("porpoise-ra", agent().c_str());
                if (!g_session->init())
                {
                    delete g_session;
                    g_session = nullptr;
                }
            }
            if (g_session)
                status = g_session->request(job.first, nullptr, png, std::size_t(1) << 20);
        }
        if (status != 200 || png.size() < 8 || png[1] != 'P' || png[2] != 'N' || png[3] != 'G')
            continue; /* tried again with the next list */
        const std::string tmp = job.second + ".part";
        if (std::FILE *f = std::fopen(tmp.c_str(), "wb"))
        {
            const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
            if (std::fclose(f) == 0 && ok)
                std::rename(tmp.c_str(), job.second.c_str());
            else
                std::remove(tmp.c_str());
        }
    }
}

/* Keeps the running game's list for the library and fetches badges it lacks. */
void refresh_now()
{
    std::string text;
    porpoise::ui::AchievementSet set;
    if (!list_text(text) || !porpoise::ui::parse_achievements(text, set))
        return;
    mkdir(g_lists.c_str(), 0777);
    if (!g_disc_id.empty())
    {
        const std::string path = kept_path(g_disc_id), tmp = path + ".part";
        if (std::FILE *f = std::fopen(tmp.c_str(), "w"))
        {
            const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
            if (std::fclose(f) == 0 && ok)
                std::rename(tmp.c_str(), path.c_str());
            else
                std::remove(tmp.c_str());
        }
    }
    const std::string dir = g_lists + "/badges";
    mkdir(dir.c_str(), 0777);
    std::vector<std::pair<std::string, std::string>> missing;
    auto want = [&](const std::string &url) {
        const std::string name = porpoise::ui::badge_file(url);
        if (name.empty() || url.rfind("https://", 0) != 0)
            return;
        struct stat st;
        const std::string path = dir + "/" + name;
        if (stat(path.c_str(), &st) != 0 || st.st_size <= 0)
            missing.emplace_back(url, path);
    };
    /* Fetched last to first: the game's badge and the list's top come first. */
    for (auto it = set.list.rbegin(); it != set.list.rend(); ++it)
        want(it->badge_url);
    want(set.badge_url);
    if (missing.empty())
        return;
    std::lock_guard<std::mutex> lock(g_badge_lock);
    g_badges = std::move(missing);
    if (!g_badge_worker)
    {
        pthread_t thread;
        g_badge_worker = create_title_thread(&thread, badge_worker, nullptr) == 0;
        if (g_badge_worker)
            pthread_detach(thread);
    }
}

std::string badge_path(const std::string &url)
{
    const std::string name = porpoise::ui::badge_file(url);
    return name.empty() ? std::string() : g_lists + "/badges/" + name;
}

/* A popup: the console's own when its rich notifications work, else drawn by
 * Porpoise over the game (main.cpp, draw_game_toast). */
void popup(const std::string &caption, const std::string &title, const std::string &text, const std::string &image,
           porpoise::notify::Sound sound, bool trophy)
{
    if (porpoise::notify::rich_state() != 0)
    {
        porpoise::notify::rich(title, text, image, sound, trophy);
        return;
    }
    std::lock_guard<std::mutex> lock(g_toast_lock);
    if (g_toasts.size() < 16)
        g_toasts.push_back({caption, title, text, badge_path(image), trophy});
}

enum Kind
{
    LoginOk = 1,
    LoginFailed = 2,
    GameLoaded = 3,
    Unlocked = 4,
    Completed = 5,
    NoAchievements = 6,
    ServerError = 7,
};

void core_event(int kind, const char *title, const char *text, const char *image, int a, int b)
{
    using porpoise::notify::Sound;
    const std::string t = title ? title : "", x = text ? text : "", pic = image ? image : "";
    switch (kind)
    {
    case LoginOk: {
        std::lock_guard<std::mutex> lock(g_lock);
        if (g_account.signed_in && (g_account.points != a || (!t.empty() && g_account.user != t)))
        {
            g_account.points = a;
            if (!t.empty())
                g_account.user = t;
            save_locked();
        }
        break;
    }
    case LoginFailed:
        if (a == -34 /* RC_INVALID_CREDENTIALS */ || a == -35 /* RC_EXPIRED_TOKEN */ || a == -28 /* RC_LOGIN_REQUIRED */)
        {
            {
                std::lock_guard<std::mutex> lock(g_lock);
                g_account.signed_in = false;
                g_token.clear();
                g_account.message = "RetroAchievements signed you out. Sign in again.";
                save_locked();
            }
            porpoise::notify::rich(tr("RetroAchievements signed you out"),
                                   tr("Sign in again in your library: L1 + Square."), "", Sound::Default);
        }
        else if (!g_unreachable_said.exchange(true))
            porpoise::notify::rich(tr("Couldn't reach RetroAchievements"),
                                   tr("Achievements are off for this game. Check the PS5's internet connection."), "",
                                   Sound::Default);
        break;
    case GameLoaded:
        g_dirty = true;
        popup("RetroAchievements", t,
              trf("RetroAchievements: {done} of {total} unlocked", {{"done", std::to_string(a)}, {"total", std::to_string(b)}}),
              pic, Sound::Default, false);
        break;
    case Unlocked:
        g_dirty = true;
        popup(tr("Achievement unlocked"), t, x, pic, Sound::Trophy, true);
        break;
    case Completed:
        g_dirty = true;
        popup(tr("Game completed"), trf("{game} completed", {{"game", t}}), tr("Every achievement unlocked"), pic,
              Sound::Platinum, true);
        break;
    case NoAchievements:
        porpoise::notify::rich(tr("No RetroAchievements for this game"),
                               tr("RetroAchievements doesn't know this game, or this version of it."), "",
                               Sound::Silent);
        break;
    case ServerError:
        if (!g_error_said.exchange(true))
            porpoise::notify::rich(tr("RetroAchievements"), x.empty() ? tr("The server had a problem.") : x, "",
                                   Sound::Default);
        break;
    default:
        break;
    }
}

struct CoreSetup
{
    unsigned version;
    const char *username;
    const char *token;
    int (*request)(const char *, const char *, void *, void (*)(void *, const char *, std::size_t));
    void (*event)(int, const char *, const char *, const char *, int, int);
};
using SetupFn = int (*)(const CoreSetup *);
using PendingFn = int (*)();
} // namespace

void init(const std::string &data_dir)
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_path = data_dir + "/retroachievements.ini";
    g_lists = data_dir + "/achievements";
    g_account = Account{};
    g_token.clear();
    if (std::FILE *f = std::fopen(g_path.c_str(), "r"))
    {
        char line[1024];
        while (std::fgets(line, sizeof line, f))
        {
            std::string s = line;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                s.pop_back();
            const std::size_t eq = s.find('=');
            if (s.empty() || s[0] == '#' || eq == std::string::npos)
                continue;
            const std::string key = s.substr(0, eq), value = s.substr(eq + 1);
            if (key == "user")
                g_account.user = value;
            else if (key == "token")
                g_token = value;
            else if (key == "points")
                g_account.points = std::atoi(value.c_str());
        }
        std::fclose(f);
        chmod(g_path.c_str(), 0600);
    }
    g_account.signed_in = !g_account.user.empty() && !g_token.empty();
}

Account account()
{
    std::lock_guard<std::mutex> lock(g_lock);
    return g_account;
}

bool begin_login(const std::string &user, const std::string &password)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (g_account.busy || user.empty() || password.empty())
        return false;
    Login *login = new Login{user, password};
    pthread_t thread;
    if (create_title_thread(&thread, login_worker, login) != 0)
    {
        delete login;
        g_account.message = "Couldn't start signing in. Try again.";
        return false;
    }
    pthread_detach(thread);
    g_account.busy = true;
    g_account.message.clear();
    return true;
}

bool take_login_done()
{
    std::lock_guard<std::mutex> lock(g_lock);
    const bool done = g_login_done;
    g_login_done = false;
    return done;
}

void logout()
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (g_account.busy)
        return;
    g_account = Account{};
    g_token.clear();
    save_locked();
    ps5::debug::mark("ra: signed out");
}

bool take_game_toast(GameToast &out)
{
    std::lock_guard<std::mutex> lock(g_toast_lock);
    if (g_toasts.empty())
        return false;
    out = g_toasts.front();
    g_toasts.erase(g_toasts.begin());
    return true;
}

void prepare_game(const std::string &disc_id)
{
    g_disc_id = disc_id;
}

void pump()
{
    if (!g_core.load() || !g_dirty.load())
        return;
    const auto now = std::chrono::steady_clock::now();
    if (now - g_refreshed < std::chrono::milliseconds(1500))
        return;
    g_refreshed = now;
    g_dirty = false;
    refresh_now();
}

bool live_list(porpoise::ui::AchievementSet &out)
{
    std::string text;
    return list_text(text) && porpoise::ui::parse_achievements(text, out);
}

bool kept_list(const std::string &disc_id, porpoise::ui::AchievementSet &out)
{
    out = porpoise::ui::AchievementSet{};
    if (disc_id.empty() || g_lists.empty())
        return false;
    std::FILE *f = std::fopen(kept_path(disc_id).c_str(), "r");
    if (!f)
        return false;
    std::string text;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        text.append(buf, n);
    std::fclose(f);
    return porpoise::ui::parse_achievements(text, out);
}

void start_game(void *core_library)
{
    g_core = nullptr;
    g_dirty = false;
    {
        std::lock_guard<std::mutex> lock(g_toast_lock);
        g_toasts.clear();
    }
    auto setup = reinterpret_cast<SetupFn>(core_library ? ps5_core_dlsym(core_library, "porpoise_ra_setup") : nullptr);
    if (!setup)
        return;
    g_unreachable_said = false;
    g_error_said = false;
    std::string user, token;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        if (g_account.signed_in)
        {
            user = g_account.user;
            token = g_token;
        }
    }
    CoreSetup s{1, user.c_str(), token.c_str(), core_request, core_event};
    const int on = setup(user.empty() ? nullptr : &s);
    ps5::debug::mark_value("ra: achievements this game", on);
    if (on)
    {
        g_core = core_library;
        porpoise::notify::hold(8); /* a toast in the game's first seconds loses its picture */
    }
}

void finish_game(void *core_library, int max_ms)
{
    auto pending = reinterpret_cast<PendingFn>(core_library ? ps5_core_dlsym(core_library, "porpoise_ra_pending") : nullptr);
    if (!pending)
        return;
    if (g_core.load())
        refresh_now(); /* the list as the game leaves it, for the library */
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(max_ms);
    int left = pending();
    if (left > 0)
        ps5::debug::mark_value("ra: waiting for requests still being sent", left);
    while (left > 0 && std::chrono::steady_clock::now() < until)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        left = pending();
    }
    if (left > 0)
        ps5::debug::mark_value("ra: closing with requests not sent", left);
}

void end_game()
{
    g_core = nullptr; /* the badge worker stops after the badge in hand */
    {
        std::lock_guard<std::mutex> lock(g_badge_lock);
        g_badges.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_toast_lock);
        g_toasts.clear();
    }
    std::lock_guard<std::mutex> lock(g_session_lock);
    delete g_session;
    g_session = nullptr;
}
} // namespace porpoise::ra
