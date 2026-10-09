/* Porpoise - name lookups for the emulator's network (porpoise_dns.hpp).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_dns.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef PORPOISE_NETFS_HOST_TEST
namespace
{
void note(const std::string &line)
{
    std::fprintf(stderr, "dns: %s\n", line.c_str());
}
} // namespace
#else
#include "trace.hpp"
namespace
{
void note(const std::string &line)
{
    ps5::debug::mark(("dns: " + line).c_str());
}
} // namespace
#endif

#if defined(__PROSPERO__) && !defined(PORPOISE_NETFS_HOST_TEST)
extern "C"
{
int sceNetInit(void);
int sceNetPoolCreate(const char *name, int size, int flags);
int sceNetPoolDestroy(int memid);
int sceNetResolverCreate(const char *name, int memid, int flags);
int sceNetResolverStartNtoa(int rid, const char *hostname, struct in_addr *addr, int timeout, int retry,
                            int flags);
int sceNetResolverDestroy(int rid);
}
#endif

namespace porpoise::dns
{
namespace
{
std::mutex g_lock;
std::string g_server; /* "" the console's own */
struct Known
{
    std::uint32_t address = 0;
    bool found = false;
    double until = 0;
};
std::map<std::string, Known> g_known;

double now_s()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

long long now_ms()
{
    return (long long)(now_s() * 1000.0);
}

std::string text_of(std::uint32_t address)
{
    char text[INET_ADDRSTRLEN] = "?";
    in_addr a{};
    a.s_addr = address;
    inet_ntop(AF_INET, &a, text, sizeof text);
    return text;
}

/* What a DNS server said. */
enum class Said
{
    Address,  /* here it is */
    No,       /* it answered: no such name, refused, or no address */
    Nothing,  /* no answer at all */
};

/* One question to a DNS server over UDP: the name's first IPv4 address
 * (following a CNAME within the answer). */
Said ask(const std::string &server, const std::string &name, std::uint32_t &out, std::string &why)
{
    in_addr to_addr{};
    if (inet_pton(AF_INET, server.c_str(), &to_addr) != 1)
    {
        why = "not an address: " + server;
        return Said::Nothing;
    }
    /* The question: a header, the name as labels, type A, class IN. */
    unsigned char q[512];
    std::size_t n = 0;
    const std::uint16_t id = std::uint16_t(std::rand() ^ (now_ms() & 0xFFFF));
    const unsigned char header[12] = {std::uint8_t(id >> 8), std::uint8_t(id), 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0};
    std::memcpy(q, header, sizeof header);
    n = sizeof header;
    std::size_t start = 0;
    while (start <= name.size())
    {
        std::size_t dot = name.find('.', start);
        if (dot == std::string::npos)
            dot = name.size();
        const std::size_t len = dot - start;
        if (len == 0 && dot == name.size())
            break; /* a trailing dot */
        if (len == 0 || len > 63 || n + len + 1 > sizeof q - 6)
        {
            why = "a name DNS can't carry";
            return Said::No;
        }
        q[n++] = std::uint8_t(len);
        std::memcpy(q + n, name.data() + start, len);
        n += len;
        start = dot + 1;
    }
    q[n++] = 0;
    q[n++] = 0;
    q[n++] = 1; /* A */
    q[n++] = 0;
    q[n++] = 1; /* IN */

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
    {
        why = std::string("socket: ") + std::strerror(errno);
        return Said::Nothing;
    }
    sockaddr_in sin{};
#ifdef __FreeBSD__
    sin.sin_len = sizeof sin;
#endif
    sin.sin_family = AF_INET;
    sin.sin_port = htons(53);
    sin.sin_addr = to_addr;
    bool found = false, answered = false;
    why = "no answer";
    /* Two tries, 0.8 s each: the game waits while its name is looked up. */
    for (int attempt = 0; attempt < 2 && !found; ++attempt)
    {
        if (sendto(fd, q, n, 0, reinterpret_cast<sockaddr *>(&sin), sizeof sin) != ssize_t(n))
        {
            why = std::string("send: ") + std::strerror(errno);
            break;
        }
        const long long deadline = now_ms() + 800;
        while (!found)
        {
            const long long left = deadline - now_ms();
            if (left <= 0)
                break;
            pollfd p{fd, POLLIN, 0};
            if (poll(&p, 1, int(left)) <= 0)
                break;
            unsigned char r[1500];
            const ssize_t got = recv(fd, r, sizeof r, 0);
            if (got < 12 || r[0] != q[0] || r[1] != q[1] || !(r[2] & 0x80))
                continue; /* not the answer to this question */
            answered = true;
            const int rcode = r[3] & 0x0F;
            if (rcode != 0)
            {
                why = rcode == 3 ? "no such name" : "the server refused (" + std::to_string(rcode) + ")";
                attempt = 2;
                break;
            }
            const int qd = (r[4] << 8) | r[5], an = (r[6] << 8) | r[7];
            std::size_t at = 12;
            auto skip_name = [&]() {
                while (at < std::size_t(got))
                {
                    const unsigned char len = r[at];
                    if (len == 0)
                    {
                        ++at;
                        return true;
                    }
                    if ((len & 0xC0) == 0xC0)
                    {
                        at += 2;
                        return at <= std::size_t(got);
                    }
                    at += 1 + len;
                }
                return false;
            };
            bool ok = true;
            for (int i = 0; i < qd && ok; ++i)
                ok = skip_name() && (at += 4) <= std::size_t(got);
            for (int i = 0; i < an && ok && !found; ++i)
            {
                ok = skip_name() && at + 10 <= std::size_t(got);
                if (!ok)
                    break;
                const int type = (r[at] << 8) | r[at + 1];
                const int klass = (r[at + 2] << 8) | r[at + 3];
                const std::size_t length = std::size_t((r[at + 8] << 8) | r[at + 9]);
                at += 10;
                if (at + length > std::size_t(got))
                    break;
                if (type == 1 && klass == 1 && length == 4)
                {
                    std::memcpy(&out, r + at, 4);
                    found = true;
                }
                at += length;
            }
            if (!found)
            {
                why = "no address in the answer";
                attempt = 2;
                break;
            }
        }
    }
    close(fd);
    return found ? Said::Address : answered ? Said::No : Said::Nothing;
}

/* The console's own resolver (its DNS settings). */
bool ask_console(const std::string &name, std::uint32_t &out)
{
#if defined(__PROSPERO__) && !defined(PORPOISE_NETFS_HOST_TEST)
    (void)sceNetInit();
    const int pool = sceNetPoolCreate("porpoise-dns", 4 * 1024, 0);
    if (pool < 0)
        return false;
    const int rid = sceNetResolverCreate("porpoise-dns", pool, 0);
    int rc = -1;
    in_addr addr{};
    if (rid >= 0)
    {
        /* 1.5 s and one more try (microseconds): the game waits meanwhile. */
        rc = sceNetResolverStartNtoa(rid, name.c_str(), &addr, 1500 * 1000, 1, 0);
        sceNetResolverDestroy(rid);
    }
    sceNetPoolDestroy(pool);
    if (rc < 0)
        return false;
    out = addr.s_addr;
    return true;
#else
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo *ai = nullptr;
    if (getaddrinfo(name.c_str(), nullptr, &hints, &ai) != 0 || !ai)
        return false;
    out = reinterpret_cast<sockaddr_in *>(ai->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(ai);
    return true;
#endif
}
} // namespace

void set_server(const std::string &address)
{
    std::lock_guard<std::mutex> lock(g_lock);
    const std::string next = is_address(address) ? address : std::string();
    if (next != g_server)
    {
        g_server = next;
        g_known.clear();
        note(next.empty() ? "the console's own DNS" : "DNS server " + next);
    }
}

std::string server()
{
    std::lock_guard<std::mutex> lock(g_lock);
    return g_server;
}

bool is_address(const std::string &text)
{
    in_addr a{};
    return !text.empty() && inet_pton(AF_INET, text.c_str(), &a) == 1;
}

bool lookup(const std::string &name, std::uint32_t &address)
{
    if (name.empty() || name.size() > 253)
        return false;
    in_addr a{};
    if (inet_pton(AF_INET, name.c_str(), &a) == 1)
    {
        address = a.s_addr;
        return true;
    }
    std::string key = name;
    for (char &c : key)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    std::string server_now;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        server_now = g_server;
        auto it = g_known.find(key);
        if (it != g_known.end() && it->second.until > now_s())
        {
            if (it->second.found)
                address = it->second.address;
            return it->second.found;
        }
    }
    /* Asked without the lock: it can take a few seconds. */
    const long long started = now_ms();
    std::uint32_t found = 0;
    bool ok = false;
    std::string why, via;
    bool ask_the_console = server_now.empty();
    if (!server_now.empty())
    {
        const Said said = ask(server_now, key, found, why);
        ok = said == Said::Address;
        via = server_now;
        /* Its "no" stands (a custom server may keep a game from a host on
         * purpose); only silence goes on to the console's own. */
        if (said == Said::Nothing)
        {
            note(key + ": " + server_now + " didn't answer (" + why + "), asking the console's");
            ask_the_console = true;
        }
        else if (said == Said::No)
            via = server_now + " (" + why + ")";
    }
    if (ask_the_console)
    {
        ok = ask_console(key, found);
        via = "the console";
    }
    note(key + (ok ? " -> " + text_of(found) : std::string(" not found")) + " via " + via + " in " +
         std::to_string(now_ms() - started) + " ms");
    {
        std::lock_guard<std::mutex> lock(g_lock);
        if (server_now == g_server)
            /* An answer for five minutes; nothing found asked again after a minute. */
            g_known[key] = Known{found, ok, now_s() + (ok ? 300.0 : 60.0)};
    }
    if (ok)
        address = found;
    return ok;
}
} // namespace porpoise::dns

/* ---- what the emulator core imports (tools/core-imports.py) ----------------------------------------- */
extern "C"
{
/* gethostbyname: one address, in a per-thread result as libc keeps its own. */
struct hostent *porpoise_core_gethostbyname(const char *name)
{
    struct Result
    {
        hostent h;
        char name[256];
        char *aliases[1];
        char *addresses[2];
        in_addr address;
    };
    static thread_local Result r;
    std::uint32_t address = 0;
    if (!name || !porpoise::dns::lookup(name, address))
        return nullptr; /* (h_errno isn't the console libc's to set) */
    std::memset(&r, 0, sizeof r);
    std::snprintf(r.name, sizeof r.name, "%s", name);
    r.address.s_addr = address;
    r.addresses[0] = reinterpret_cast<char *>(&r.address);
    r.h.h_name = r.name;
    r.h.h_aliases = r.aliases;
    r.h.h_addrtype = AF_INET;
    r.h.h_length = 4;
    r.h.h_addr_list = r.addresses;
    return &r.h;
}

/* getaddrinfo: IPv4, one entry per socket type asked for (stream and datagram
 * when the hints don't say). */
int porpoise_core_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
                              struct addrinfo **res)
{
    if (!res)
        return EAI_FAIL;
    *res = nullptr;
    if (hints && hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET)
        return EAI_FAMILY;
    if (!node && !service)
        return EAI_NONAME;
    int port = 0;
    if (service && *service)
    {
        char *end = nullptr;
        const long p = std::strtol(service, &end, 10);
        if (*end || p < 0 || p > 65535)
            return EAI_SERVICE; /* only numbers: there's no services database */
        port = int(p);
    }
    std::uint32_t address = 0;
    if (!node)
        address = hints && (hints->ai_flags & AI_PASSIVE) ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    else if (hints && (hints->ai_flags & AI_NUMERICHOST))
    {
        in_addr a{};
        if (inet_pton(AF_INET, node, &a) != 1)
            return EAI_NONAME;
        address = a.s_addr;
    }
    else if (!porpoise::dns::lookup(node, address))
        return EAI_NONAME;

    const int asked = hints ? hints->ai_socktype : 0;
    const int types[2] = {asked ? asked : SOCK_STREAM, asked ? 0 : SOCK_DGRAM};
    addrinfo *first = nullptr, **next = &first;
    for (int type : types)
    {
        if (!type)
            continue;
        auto *sin = static_cast<sockaddr_in *>(std::calloc(1, sizeof(sockaddr_in)));
        auto *ai = static_cast<addrinfo *>(std::calloc(1, sizeof(addrinfo)));
        if (!sin || !ai)
        {
            std::free(sin);
            std::free(ai);
            while (first)
            {
                addrinfo *after = first->ai_next;
                std::free(first->ai_addr);
                std::free(first->ai_canonname);
                std::free(first);
                first = after;
            }
            return EAI_MEMORY;
        }
#ifdef __FreeBSD__
        sin->sin_len = sizeof(sockaddr_in);
#endif
        sin->sin_family = AF_INET;
        sin->sin_port = htons(std::uint16_t(port));
        sin->sin_addr.s_addr = address;
        ai->ai_family = AF_INET;
        ai->ai_socktype = type;
        ai->ai_protocol = asked && hints->ai_protocol ? hints->ai_protocol
                          : type == SOCK_DGRAM ? IPPROTO_UDP
                          : type == SOCK_STREAM ? IPPROTO_TCP
                                                : 0;
        ai->ai_addrlen = sizeof(sockaddr_in);
        ai->ai_addr = reinterpret_cast<sockaddr *>(sin);
        if (!first && node && hints && (hints->ai_flags & AI_CANONNAME))
        {
            const std::size_t len = std::strlen(node);
            ai->ai_canonname = static_cast<char *>(std::malloc(len + 1));
            if (ai->ai_canonname)
                std::memcpy(ai->ai_canonname, node, len + 1);
        }
        *next = ai;
        next = &ai->ai_next;
    }
    *res = first;
    return 0;
}

void porpoise_core_freeaddrinfo(struct addrinfo *ai)
{
    while (ai)
    {
        addrinfo *after = ai->ai_next;
        std::free(ai->ai_addr);
        std::free(ai->ai_canonname);
        std::free(ai);
        ai = after;
    }
}

/* getnameinfo: numbers only (no reverse lookups). */
int porpoise_core_getnameinfo(const struct sockaddr *sa, socklen_t length, char *host, size_t host_size,
                              char *service, size_t service_size, int)
{
    if (!sa || sa->sa_family != AF_INET || length < socklen_t(sizeof(sockaddr_in)))
        return EAI_FAMILY;
    const auto *sin = reinterpret_cast<const sockaddr_in *>(sa);
    if (host && host_size && !inet_ntop(AF_INET, &sin->sin_addr, host, socklen_t(host_size)))
        return EAI_OVERFLOW;
    if (service && service_size)
        std::snprintf(service, service_size, "%u", unsigned(ntohs(sin->sin_port)));
    return 0;
}
}
