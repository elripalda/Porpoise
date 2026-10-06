/* Porpoise - PS5 notifications (see porpoise_notify.hpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The rich toast's JSON, the sounds, the channels, the pacing and the relay
 * through the ELF loader follow PS5SX2 (Spyros, ps5/coreorbis/orbis-shims/
 * ProsperoNotify.cpp, GPL-3.0-or-later), whose notes record what was seen on
 * a console: only logged toasts show; "psfx_trophy_toast" and
 * "psfx_platinum_trophy_toast" play the trophy sounds; a toast in a game's
 * first seconds loses its picture; the icon may be an https URL. The JSON
 * layout is the payload SDK's notify sample (LightningMods). */
#include "porpoise_notify.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <fcntl.h>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "title_threads.hpp"
#include "trace.hpp"

#if __has_include("notify_relay_elf.inc")
#include "notify_relay_elf.inc" /* kNotifyRelay, kNotifyRelaySize (tools/build-porpoise.sh) */
#define PORPOISE_NOTIFY_RELAY 1
#endif

extern "C"
{
    int sceKernelSendNotificationRequest(int device, void *request, std::size_t size, int blocking);
    int sceKernelLoadStartModule(const char *path, std::size_t args, const void *argp, std::uint32_t flags, void *opt,
                                 int *res);
    int sceKernelDlsym(int handle, const char *name, void **address);
}

namespace porpoise::notify
{
namespace
{
/* The kernel's text toast: 0xc30 bytes, the text from 0x2d. */
#pragma pack(push, 1)
struct KernelToast
{
    std::int32_t type, req_id, priority, msg_id, target_id, user_id, unk1, unk2, app_id, error_num, unk3;
    std::uint8_t use_icon_image_uri;
    char message[1024];
    char icon_uri[1024];
    char unk[1024];
    std::uint8_t pad[3];
};
#pragma pack(pop)
static_assert(sizeof(KernelToast) == 0xc30, "the kernel toast is 0xc30 bytes");

struct Toast
{
    bool rich = false;
    std::string message, sub, icon;
    Sound sound = Sound::Default;
    bool trophy = false;
};

struct Shared
{
    std::mutex lock;
    std::condition_variable wake, idle;
    std::deque<Toast> queue;
    bool started = false, sending = false;
    std::chrono::steady_clock::time_point hold_until{};
};
Shared &S()
{
    static Shared *const s = new Shared(); /* never freed: the worker waits on it for good */
    return *s;
}
constexpr std::size_t kMaxQueued = 24;
std::atomic<int> g_rich_state{-1};
constexpr auto kGap = std::chrono::milliseconds(6500); /* between rich toasts */

std::string quote(const std::string &s)
{
    std::string out = "\"";
    for (const unsigned char c : s)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += char(c);
        }
        else if (c == '\n')
            out += "\\n";
        else if (c < 0x20)
        {
            char esc[8];
            std::snprintf(esc, sizeof esc, "\\u%04x", c);
            out += esc;
        }
        else
            out += char(c);
    }
    return out + "\"";
}

std::string payload(const Toast &t)
{
    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    tm when{};
    gmtime_r(&ts.tv_sec, &when);
    char created[48];
    std::snprintf(created, sizeof created, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", when.tm_year + 1900, when.tm_mon + 1,
                  when.tm_mday, when.tm_hour, when.tm_min, when.tm_sec, int(ts.tv_nsec / 1000000));
    static unsigned count = 0;
    char id[16];
    std::snprintf(id, sizeof id, "%u", 100000000u + (unsigned(ts.tv_sec) % 800000u) * 1000u + (count++ % 1000u));
    const char *sound = t.sound == Sound::Trophy     ? "psfx_trophy_toast"
                        : t.sound == Sound::Platinum ? "psfx_platinum_trophy_toast"
                        : t.sound == Sound::Silent   ? "none"
                                                     : nullptr;
    std::string j = "{\"rawData\":{\"viewTemplateType\":\"InteractiveToastTemplateB\",\"channelType\":" +
                    quote(t.trophy ? "Trophies" : "ServiceFeedback") + ",\"useCaseId\":\"IDC\",";
    if (sound)
        j += "\"soundEffect\":" + quote(sound) + ",";
    j += "\"toastOverwriteType\":\"No\",\"isImmediate\":true,\"priority\":100,\"viewData\":{";
    if (!t.icon.empty())
        j += "\"icon\":{\"type\":\"Url\",\"parameters\":{\"url\":" + quote(t.icon) + "}},";
    j += "\"message\":{\"body\":" + quote(t.message) + "}";
    if (!t.sub.empty())
        j += ",\"subMessage\":{\"body\":" + quote(t.sub) + "}";
    j += "}},\"createdDateTime\":" + quote(created) + ",\"localNotificationId\":" + quote(id) + "}";
    return j;
}

int send_kernel(const std::string &text)
{
    KernelToast req;
    std::memset(&req, 0, sizeof req);
    std::snprintf(req.message, sizeof req.message, "%s", text.c_str());
    return sceKernelSendNotificationRequest(0, &req, sizeof req, 0);
}

/* libSceNotification in Porpoise's own process, when the system allows it. */
using SendFn = int (*)(int user, bool logged, const char *payload);
SendFn own_sender()
{
    static bool tried = false;
    static SendFn send = nullptr;
    if (tried)
        return send;
    tried = true;
    static const char *const dirs[] = {"/system/common/lib/", "/system/priv/lib/", "/system_ex/common_ex/lib/",
                                       "/system_ex/priv_ex/lib/"};
    for (const char *dir : dirs)
    {
        const std::string path = std::string(dir) + "libSceNotification.sprx";
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            continue;
        int res = 0;
        const int handle = sceKernelLoadStartModule(path.c_str(), 0, nullptr, 0, nullptr, &res);
        ps5::debug::mark_value(("notify: load " + path).c_str(), handle);
        if (handle < 0)
            continue;
        void *address = nullptr;
        if (sceKernelDlsym(handle, "sceNotificationSend", &address) == 0 && address)
            send = reinterpret_cast<SendFn>(address);
        break;
    }
    return send;
}

#ifdef PORPOISE_NOTIFY_RELAY
constexpr char kMarker[] = "PORPOISE-NOTIFY-RELAY-JSON-V1";
constexpr std::size_t kRoom = 16384; /* g_payload's size in tools/notify-relay/notify_relay.c */

std::size_t relay_offset()
{
    static const std::size_t offset = [] {
        const std::size_t len = sizeof kMarker - 1;
        for (std::size_t i = 0; i + kRoom <= kNotifyRelaySize; ++i)
        {
            if (std::memcmp(kNotifyRelay + i, kMarker, len) != 0)
                continue;
            std::size_t j = i + len;
            while (j < i + kRoom && kNotifyRelay[j] == 0)
                ++j;
            if (j == i + kRoom)
                return i;
        }
        return std::size_t(-1);
    }();
    return offset;
}

/* The payload with this JSON to the ELF loader; true once it ran. */
bool send_relay(const std::string &json)
{
    const std::size_t offset = relay_offset();
    if (offset == std::size_t(-1) || json.size() >= kRoom)
        return false;
    std::vector<std::uint8_t> elf(kNotifyRelay, kNotifyRelay + kNotifyRelaySize);
    std::memcpy(elf.data() + offset, json.data(), json.size());
    elf[offset + json.size()] = 0;
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9021);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int flags = fcntl(fd, F_GETFL, 0);
    bool ok = flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
    if (ok && connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof addr) != 0 && errno != EINPROGRESS)
        ok = false; /* nothing listening: no loader on this console */
    if (ok)
    {
        pollfd p{fd, POLLOUT, 0};
        int err = 0;
        socklen_t len = sizeof err;
        ok = poll(&p, 1, 500) == 1 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0;
    }
    if (ok)
    {
        fcntl(fd, F_SETFL, flags);
        for (std::size_t sent = 0; ok && sent < elf.size();)
        {
            const ssize_t n = send(fd, elf.data() + sent, elf.size() - sent, 0);
            ok = n > 0;
            if (ok)
                sent += std::size_t(n);
        }
    }
    if (ok)
    {
        shutdown(fd, SHUT_WR);
        /* The loader closes the connection when the payload has ended, which
         * keeps the toasts in order. */
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        for (;;)
        {
            const int left = int(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     deadline - std::chrono::steady_clock::now())
                                     .count());
            pollfd q{fd, POLLIN, 0};
            char buf[256];
            if (left <= 0 || poll(&q, 1, left) != 1 || recv(fd, buf, sizeof buf, 0) <= 0)
                break;
        }
    }
    close(fd);
    return ok;
}
#endif

void send(const Toast &t)
{
    if (!t.rich)
    {
        ps5::debug::mark_value("notify: plain", send_kernel(t.message));
        return;
    }
    const std::string json = payload(t);
    if (SendFn own = own_sender())
    {
        const int rc = own(0xFE, true, json.c_str());
        ps5::debug::mark_value("notify: rich", rc);
        if (rc == 0)
        {
            g_rich_state = 1;
            return;
        }
    }
#ifdef PORPOISE_NOTIFY_RELAY
    if (send_relay(json))
    {
        ps5::debug::mark("notify: rich, through the ELF loader");
        g_rich_state = 1;
        return;
    }
#endif
    g_rich_state = 0;
    ps5::debug::mark_value("notify: rich unavailable, plain", send_kernel(t.message + (t.sub.empty() ? "" : "\n" + t.sub)));
}

void *worker(void *)
{
    Shared &s = S();
    std::chrono::steady_clock::time_point last_rich{};
    for (;;)
    {
        Toast t;
        std::chrono::steady_clock::time_point hold{};
        {
            std::unique_lock<std::mutex> lock(s.lock);
            s.sending = false;
            s.idle.notify_all();
            s.wake.wait(lock, [&s] { return !s.queue.empty(); });
            t = std::move(s.queue.front());
            s.queue.pop_front();
            s.sending = true;
            hold = s.hold_until;
        }
        if (t.rich)
        {
            auto when = hold;
            if (last_rich.time_since_epoch().count() != 0 && last_rich + kGap > when)
                when = last_rich + kGap;
            if (when > std::chrono::steady_clock::now())
                std::this_thread::sleep_until(when);
        }
        send(t);
        if (t.rich)
            last_rich = std::chrono::steady_clock::now();
    }
    return nullptr;
}

/* Under s.lock. True when the worker is running. */
bool start_worker(Shared &s)
{
    if (!s.started)
    {
        pthread_t thread;
        if (create_title_thread(&thread, worker, nullptr) != 0)
            return false;
        pthread_detach(thread);
        s.started = true;
    }
    return true;
}

void queue(Toast t)
{
    Shared &s = S();
    std::lock_guard<std::mutex> lock(s.lock);
    if (!start_worker(s))
        return;
    if (s.queue.size() >= kMaxQueued)
        return;
    s.queue.push_back(std::move(t));
    s.wake.notify_one();
}
} // namespace

void rich(const std::string &message, const std::string &sub, const std::string &icon, Sound sound, bool trophy)
{
    Toast t;
    t.rich = true;
    t.message = message;
    t.sub = sub;
    t.icon = icon.rfind("https://", 0) == 0 || icon.rfind("/data/", 0) == 0 ? icon : std::string();
    t.sound = sound;
    t.trophy = trophy;
    queue(std::move(t));
}

void plain(const std::string &text)
{
    Toast t;
    t.message = text;
    queue(std::move(t));
}

void hold(int seconds)
{
    Shared &s = S();
    std::lock_guard<std::mutex> lock(s.lock);
    /* The worker starts here, on Porpoise's own thread as a game starts,
     * rather than from a core thread's first popup. */
    start_worker(s);
    s.hold_until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
}

int rich_state()
{
    return g_rich_state.load();
}

void flush(int max_ms)
{
    Shared &s = S();
    std::unique_lock<std::mutex> lock(s.lock);
    s.idle.wait_for(lock, std::chrono::milliseconds(max_ms), [&s] { return s.queue.empty() && !s.sending; });
}
} // namespace porpoise::notify
