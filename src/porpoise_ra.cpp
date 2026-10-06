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
        porpoise::notify::rich(t, trf("RetroAchievements: {done} of {total} unlocked",
                                      {{"done", std::to_string(a)}, {"total", std::to_string(b)}}),
                               pic, Sound::Default);
        break;
    case Unlocked:
        porpoise::notify::rich(t, x, pic, Sound::Trophy, true);
        break;
    case Completed:
        porpoise::notify::rich(trf("{game} completed", {{"game", t}}), tr("Every achievement unlocked"), pic,
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

void start_game(void *core_library)
{
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
        porpoise::notify::hold(8); /* a toast in the game's first seconds loses its picture */
}

void finish_game(void *core_library, int max_ms)
{
    auto pending = reinterpret_cast<PendingFn>(core_library ? ps5_core_dlsym(core_library, "porpoise_ra_pending") : nullptr);
    if (!pending)
        return;
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
    std::lock_guard<std::mutex> lock(g_session_lock);
    delete g_session;
    g_session = nullptr;
}
} // namespace porpoise::ra
