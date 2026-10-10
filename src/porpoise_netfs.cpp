/* Porpoise - games on a network share (SMB), read with libsmb2.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_netfs.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>

#include "libretro.h"

extern "C"
{
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/libsmb2-share-enum.h>
#include <nfsc/libnfs.h>
#include <nfsc/libnfs-raw-mount.h>
}

namespace
{
/* The server name libsmb2 last saw, on this thread (porpoise_netfs_target_name). */
thread_local std::string t_target_name;
} // namespace

#ifdef PORPOISE_NETFS_HOST_TEST
#include <cstdio>
namespace
{
void note(const std::string &line)
{
    std::fprintf(stderr, "netfs: %s\n", line.c_str());
}
} // namespace
#else
#include "trace.hpp"
namespace
{
void note(const std::string &line)
{
    ps5::debug::mark(("netfs: " + line).c_str());
}
} // namespace
#endif

namespace porpoise::netfs
{
const char *const kRoot = "/net";

namespace
{
using u64 = std::uint64_t;
using s64 = std::int64_t;

long long now_ms()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool is_nfs(const Share &s)
{
    return s.protocol == "nfs";
}

/* What a file or folder is. */
struct Info
{
    bool is_dir = false;
    u64 size = 0;
};

/* A connected client of one share, SMB or NFS. Every call holds the share's
 * Conn::m: one request at a time. Paths are inside the share ("GameCube/x.iso"). */
struct Session
{
    virtual ~Session() = default;
    virtual std::uint32_t max_read() const = 0;
    /* -errno on failure. */
    virtual int open(const std::string &path, void **fh) = 0;
    virtual int fstat(void *fh, Info &info) = 0;
    /* Bytes read, or -errno. */
    virtual int pread(void *fh, std::uint8_t *buf, std::uint32_t n, u64 offset) = 0;
    virtual void close(void *fh) = 0;
    virtual int list(const std::string &dir, std::vector<Entry> &out) = 0;
    virtual int stat(const std::string &path, Info &info) = 0;
    /* Before a request: no error left from an earlier one. */
    virtual void clear_error() {}
    /* After a failed request: the server answered about the request itself
     * (a missing file, no permission), so the connection is fine. */
    virtual bool request_error(int rc) = 0;
    virtual std::string last_error() = 0;
};

/* One connection a share, one request on it at a time. */
struct Conn
{
    Share cfg;
    std::mutex m;
    std::unique_ptr<Session> session;
    unsigned generation = 0; /* a new one each time it connects; files opened before reopen */
    std::uint32_t max_read = 0;
    long long failed_at = 0; /* when connecting last failed: not tried again for a moment */
    std::string error;

    /* Closes the connection without a goodbye to the server: on a dead
     * connection that would wait out the timeout again. The server lets go of
     * the session when the socket closes. */
    void drop() { session.reset(); }
    ~Conn() { drop(); }
};

std::mutex g_mutex; /* the share list */
std::vector<Share> g_shares;
std::map<std::string, std::shared_ptr<Conn>> g_conns;

bool same_target(const Share &a, const Share &b)
{
    return a.host == b.host && a.share == b.share && a.folder == b.folder && a.user == b.user &&
           a.password == b.password && a.protocol == b.protocol;
}

/* "/net/<name>/<rest>": the share's connection and the path inside the
 * share (with its starting folder), or nullptr. */
std::shared_ptr<Conn> resolve(std::string_view path, std::string *inside, std::string *name = nullptr)
{
    if (!is_net(path))
        return nullptr;
    std::string_view rest = path.substr(std::strlen(kRoot));
    while (!rest.empty() && rest.front() == '/')
        rest.remove_prefix(1);
    const std::size_t slash = rest.find('/');
    const std::string share_name(rest.substr(0, slash));
    std::string_view tail = slash == std::string_view::npos ? std::string_view() : rest.substr(slash + 1);
    if (name)
        *name = share_name;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_conns.find(share_name);
    if (it == g_conns.end())
        return nullptr;
    if (inside)
    {
        std::string p = it->second->cfg.folder;
        while (!p.empty() && (p.front() == '/' || p.front() == '\\'))
            p.erase(p.begin());
        while (!p.empty() && (p.back() == '/' || p.back() == '\\'))
            p.pop_back();
        while (!tail.empty() && tail.front() == '/')
            tail.remove_prefix(1);
        while (!tail.empty() && tail.back() == '/')
            tail.remove_suffix(1);
        if (!tail.empty())
            p += (p.empty() ? "" : "/") + std::string(tail);
        *inside = p;
    }
    return it->second;
}

std::string server_of(const Share &s)
{
    std::string host = s.host;
    while (!host.empty() && (host.back() == ' ' || host.back() == '/' || host.back() == '\\'))
        host.pop_back();
    while (!host.empty() && (host.front() == ' ' || host.front() == '/' || host.front() == '\\'))
        host.erase(host.begin());
    for (const char *scheme : {"smb:", "nfs:"})
        if (host.rfind(scheme, 0) == 0)
        {
            host.erase(0, 4);
            while (!host.empty() && host.front() == '/')
                host.erase(host.begin());
        }
    return host;
}

/* A share for the log: "192.168.1.20:/volume1/games" or "192.168.1.20/Games". */
std::string where(const Share &s);

/* The export path an NFS server is asked for: "/volume1/games". */
std::string export_of(const Share &s)
{
    std::string e = s.share;
    std::replace(e.begin(), e.end(), '\\', '/');
    while (!e.empty() && e.back() == '/')
        e.pop_back();
    if (e.empty() || e.front() != '/')
        e.insert(e.begin(), '/');
    return e;
}

std::string where(const Share &s)
{
    return s.protocol == "nfs" ? server_of(s) + ":" + export_of(s) : server_of(s) + "/" + s.share;
}

#ifdef __PROSPERO__
} // namespace
} // namespace porpoise::netfs
/* The console's name lookup (below): what libsmb2 is built to call. */
extern "C" int porpoise_netfs_getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **);
extern "C" void porpoise_netfs_freeaddrinfo(struct addrinfo *);
namespace porpoise::netfs
{
namespace
{
int lookup(const char *node, const char *service, addrinfo **res)
{
    return porpoise_netfs_getaddrinfo(node, service, nullptr, res);
}
void lookup_free(addrinfo *ai)
{
    porpoise_netfs_freeaddrinfo(ai);
}
#else
int lookup(const char *node, const char *service, addrinfo **res)
{
    return getaddrinfo(node, service, nullptr, res);
}
void lookup_free(addrinfo *ai)
{
    freeaddrinfo(ai);
}
#endif

/* A plain TCP connection to the computer's SMB (445) or NFS (2049) port first, step by step in
 * the log, with its own deadline: says what the network does (no answer,
 * refused, no route) before the client library tries, and fails fast when it can't. */
bool probe(const std::string &server, int port, std::string &error)
{
    std::string host = server;
    const std::size_t colon = host.rfind(':');
    if (colon != std::string::npos && host.find(':') == colon)
    {
        port = std::atoi(host.c_str() + colon + 1);
        host.resize(colon);
    }
    addrinfo *ai = nullptr;
    const std::string service = std::to_string(port);
    const int gai = lookup(host.c_str(), service.c_str(), &ai);
    if (gai != 0 || !ai)
    {
        error = "Invalid address: can't resolve " + host;
        note("probe: no address for " + host + " (" + std::to_string(gai) + ")");
        return false;
    }
    char text[64] = "?";
    if (ai->ai_family == AF_INET)
        inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(ai->ai_addr)->sin_addr, text, sizeof text);
    const int fd = socket(ai->ai_family, SOCK_STREAM, 0);
    if (fd < 0)
    {
        error = std::string("socket failed: ") + std::strerror(errno);
        note("probe: " + error);
        lookup_free(ai);
        return false;
    }
    const int flags = fcntl(fd, F_GETFL, 0);
    const int set = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    int nbio_ioctl = 0, nbio_opt = 0;
#ifdef __PROSPERO__
    {
        /* As libsmb2 is built to (lib/socket.c): fcntl alone doesn't make a
         * socket non-blocking on the console. */
        int one = 1;
        nbio_ioctl = ioctl(fd, FIONBIO, &one);
        nbio_opt = setsockopt(fd, SOL_SOCKET, 0x1200, &one, sizeof one);
    }
#endif
    const long long started = now_ms();
    const int rc = connect(fd, ai->ai_addr, socklen_t(ai->ai_addrlen));
    const int connect_errno = rc == 0 ? 0 : errno;
    lookup_free(ai);
    note("probe: " + std::string(text) + ":" + service + " fd " + std::to_string(fd) + ", O_NONBLOCK " +
         (flags >= 0 && set == 0 && (fcntl(fd, F_GETFL, 0) & O_NONBLOCK) ? "yes" : "no") + ", FIONBIO " +
         std::to_string(nbio_ioctl) + ", SO_NBIO " + std::to_string(nbio_opt) + ", connect " +
         std::to_string(rc) + " (" + (rc == 0 ? "done" : std::strerror(connect_errno)) + ") after " +
         std::to_string(now_ms() - started) + " ms, clock " + std::to_string((long long)std::time(nullptr)));
    bool ok = rc == 0;
    if (!ok && connect_errno == EINPROGRESS)
    {
        pollfd p{fd, POLLOUT, 0};
        const int pr = poll(&p, 1, 5000);
        int err = 0;
        socklen_t len = sizeof err;
        const int gs = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        ok = pr == 1 && gs == 0 && err == 0;
        note("probe: waited " + std::to_string(now_ms() - started) + " ms, poll " + std::to_string(pr) +
             " revents " + std::to_string(p.revents) + ", socket error " + std::to_string(err) + " (" +
             (err ? std::strerror(err) : "none") + "), clock " + std::to_string((long long)std::time(nullptr)));
        if (!ok)
            error = pr == 0 ? "connect failed: no answer in 5 s (Timeout)"
                            : std::string("connect failed: ") + std::strerror(err ? err : errno);
    }
    else if (!ok)
        error = std::string("connect failed: ") + std::strerror(connect_errno);
    if (ok)
    {
        /* Non-blocking for real? A read with nothing sent yet (the server
         * waits for the client to speak first) returns at once if so; a
         * blocking one waits out the half-second limit set here. */
        timeval limit{0, 500 * 1000};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof limit);
        char byte = 0;
        const long long before = now_ms();
        const ssize_t got = recv(fd, &byte, 1, 0);
        const int recv_errno = got < 0 ? errno : 0;
        note("probe: an empty read returned " + std::to_string(got) + " (" +
             (got < 0 ? std::strerror(recv_errno) : "data") + ") after " + std::to_string(now_ms() - before) +
             " ms: the socket is " + (now_ms() - before < 250 ? "non-blocking" : "BLOCKING"));
    }
    close(fd);
    return ok;
}

bool hidden(const char *name)
{
    return !name || !name[0] || name[0] == '.';
}

/* ---- SMB (libsmb2) ------------------------------------------------------------------------- */

/* The last request's error: the server's own status (STATUS_LOGON_FAILURE)
 * when it sent one, which libsmb2's message can hide behind the socket
 * closing, else that message. */
std::string smb_error(smb2_context *ctx)
{
    const int status = smb2_get_nterror(ctx);
    if (status != 0)
        return nterror_to_str(std::uint32_t(status));
    const char *e = smb2_get_error(ctx);
    std::string out = e ? e : "";
    while (!out.empty() && (out.back() == '\n' || out.back() == ' '))
        out.pop_back();
    return out;
}

/* -errno for a request that failed. */
int smb_errno(smb2_context *ctx)
{
    const int status = smb2_get_nterror(ctx);
    if (status != 0)
    {
        const int e = nterror_to_errno(std::uint32_t(status));
        return e > 0 ? -e : -EIO;
    }
    return -EIO;
}

struct SmbSession : Session
{
    smb2_context *ctx = nullptr;
    std::uint32_t most = 0;

    ~SmbSession() override
    {
        if (ctx)
            smb2_destroy_context(ctx);
    }
    std::uint32_t max_read() const override { return most; }
    int open(const std::string &path, void **fh) override
    {
        smb2fh *f = smb2_open(ctx, path.c_str(), O_RDONLY);
        if (!f)
            return smb_errno(ctx);
        *fh = f;
        return 0;
    }
    int fstat(void *fh, Info &info) override
    {
        smb2_stat_64 st{};
        const int rc = smb2_fstat(ctx, static_cast<smb2fh *>(fh), &st);
        if (rc < 0)
            return rc;
        info.is_dir = st.smb2_type == SMB2_TYPE_DIRECTORY;
        info.size = st.smb2_size;
        return 0;
    }
    int pread(void *fh, std::uint8_t *buf, std::uint32_t n, u64 offset) override
    {
        return smb2_pread(ctx, static_cast<smb2fh *>(fh), buf, n, offset);
    }
    void close(void *fh) override { smb2_close(ctx, static_cast<smb2fh *>(fh)); }
    int list(const std::string &dir, std::vector<Entry> &out) override
    {
        out.clear();
        smb2dir *d = smb2_opendir(ctx, dir.c_str());
        if (!d)
            return smb_errno(ctx);
        while (smb2dirent *ent = smb2_readdir(ctx, d))
            if (!hidden(ent->name))
                out.push_back({ent->name, ent->st.smb2_type == SMB2_TYPE_DIRECTORY});
        smb2_closedir(ctx, d);
        return 0;
    }
    int stat(const std::string &path, Info &info) override
    {
        smb2_stat_64 st{};
        const int rc = smb2_stat(ctx, path.c_str(), &st);
        if (rc < 0)
            return rc;
        info.is_dir = st.smb2_type == SMB2_TYPE_DIRECTORY;
        info.size = info.is_dir ? 0 : st.smb2_size;
        return 0;
    }
    void clear_error() override { smb2_set_error(ctx, ""); }
    bool request_error(int) override
    {
        const std::uint32_t status = std::uint32_t(smb2_get_nterror(ctx));
        if (status == 0)
            return false;
        switch (status)
        {
        case 0xC0000203: /* STATUS_USER_SESSION_DELETED */
        case 0xC00000C9: /* STATUS_NETWORK_NAME_DELETED */
        case 0xC000035C: /* STATUS_NETWORK_SESSION_EXPIRED */
        case 0xC0000008: /* STATUS_INVALID_HANDLE */
        case 0xC0000128: /* STATUS_FILE_CLOSED */
        case 0xC000020C: /* STATUS_CONNECTION_DISCONNECTED */
        case 0xC000013B: /* STATUS_LOCAL_DISCONNECT */
        case 0xC000013C: /* STATUS_REMOTE_DISCONNECT */
        case 0xC00000B5: /* STATUS_IO_TIMEOUT: no answer in time (libsmb2's own, for a silent server) */
        case 0xC0000120: /* STATUS_CANCELLED */
            return false;
        default:
            return true;
        }
    }
    std::string last_error() override { return smb_error(ctx); }
};

/* Connected to share (IPC$ to list the shares), or nullptr and why. */
std::unique_ptr<SmbSession> smb_connect(const Share &s, const std::string &share, std::string &error)
{
    auto session = std::make_unique<SmbSession>();
    smb2_context *ctx = session->ctx = smb2_init_context();
    if (!ctx)
    {
        error = "out of memory";
        return nullptr;
    }
    smb2_set_security_mode(ctx, SMB2_NEGOTIATE_SIGNING_ENABLED);
    smb2_set_version(ctx, SMB2_VERSION_ANY);
    smb2_set_timeout(ctx, 10);
    const bool guest = s.user.empty();
    smb2_set_user(ctx, guest ? "Guest" : s.user.c_str());
    if (!guest)
        smb2_set_password(ctx, s.password.c_str());
    std::string user = s.user;
    const std::size_t sep = user.find_first_of("\\/");
    if (!guest && sep != std::string::npos)
    {
        /* DOMAIN\user */
        smb2_set_domain(ctx, user.substr(0, sep).c_str());
        user = user.substr(sep + 1);
        smb2_set_user(ctx, user.c_str());
        smb2_set_password(ctx, s.password.c_str());
    }
    const std::string server = server_of(s);
    if (!probe(server, 445, error))
        return nullptr;
    note("connecting to " + server + "/" + share + (guest ? " as a guest" : " with a username"));
    const long long started = now_ms();
    const int rc = smb2_connect_share(ctx, server.c_str(), share.c_str(), guest ? nullptr : user.c_str());
    if (rc < 0)
    {
        error = smb_error(ctx);
        if (error.empty())
            error = std::strerror(-rc);
        note("connecting to " + server + "/" + share + " failed after " + std::to_string(now_ms() - started) +
             " ms: " + error);
        return nullptr;
    }
    note("connected to " + server + "/" + share + " in " + std::to_string(now_ms() - started) + " ms");
    session->most = smb2_get_max_read_size(ctx);
    return session;
}

/* ---- NFS (libnfs) ------------------------------------------------------------------------- */

std::string nfs_path(const std::string &inside)
{
    return "/" + inside;
}

struct NfsSession : Session
{
    nfs_context *nfs = nullptr;
    std::uint32_t most = 0;

    ~NfsSession() override
    {
        if (nfs)
            nfs_destroy_context(nfs);
    }
    std::uint32_t max_read() const override { return most; }
    int open(const std::string &path, void **fh) override
    {
        nfsfh *f = nullptr;
        const int rc = nfs_open(nfs, nfs_path(path).c_str(), O_RDONLY, &f);
        if (rc < 0 || !f)
            return rc < 0 ? rc : -EIO;
        *fh = f;
        return 0;
    }
    int fstat(void *fh, Info &info) override
    {
        nfs_stat_64 st{};
        const int rc = nfs_fstat64(nfs, static_cast<nfsfh *>(fh), &st);
        if (rc < 0)
            return rc;
        info.is_dir = S_ISDIR(st.nfs_mode);
        info.size = st.nfs_size;
        return 0;
    }
    int pread(void *fh, std::uint8_t *buf, std::uint32_t n, u64 offset) override
    {
        return nfs_pread(nfs, static_cast<nfsfh *>(fh), buf, n, offset);
    }
    void close(void *fh) override { nfs_close(nfs, static_cast<nfsfh *>(fh)); }
    int list(const std::string &dir, std::vector<Entry> &out) override
    {
        out.clear();
        nfsdir *d = nullptr;
        const int rc = nfs_opendir(nfs, nfs_path(dir).c_str(), &d);
        if (rc < 0 || !d)
            return rc < 0 ? rc : -EIO;
        while (nfsdirent *ent = nfs_readdir(nfs, d))
            if (!hidden(ent->name))
                out.push_back({ent->name, S_ISDIR(ent->mode) || ent->type == 2 /* NF3DIR */});
        nfs_closedir(nfs, d);
        return 0;
    }
    int stat(const std::string &path, Info &info) override
    {
        nfs_stat_64 st{};
        const int rc = nfs_stat64(nfs, nfs_path(path).c_str(), &st);
        if (rc < 0)
            return rc;
        info.is_dir = S_ISDIR(st.nfs_mode);
        info.size = info.is_dir ? 0 : st.nfs_size;
        return 0;
    }
    bool request_error(int rc) override
    {
        const int e = rc < 0 ? -rc : rc;
        return e == ENOENT || e == EACCES || e == EPERM || e == ENOTDIR || e == EISDIR || e == EINVAL ||
               e == ENAMETOOLONG;
    }
    std::string last_error() override
    {
        const char *e = nfs_get_error(nfs);
        return e ? e : "";
    }
};

/* Mounted (NFS version 3, else 4), or nullptr and why. */
std::unique_ptr<NfsSession> nfs_connect(const Share &s, std::string &error)
{
    const std::string server = server_of(s), exp = export_of(s);
    if (!probe(server, 2049, error))
        return nullptr;
    std::string why[2];
    for (int v = 0; v < 2; ++v)
    {
        auto session = std::make_unique<NfsSession>();
        nfs_context *nfs = session->nfs = nfs_init_context();
        if (!nfs)
        {
            error = "out of memory";
            return nullptr;
        }
        nfs_set_timeout(nfs, 10000);
        nfs_set_autoreconnect(nfs, 0); /* netfs reconnects, and gives up */
        nfs_set_version(nfs, v == 0 ? 3 : 4); /* NFS_V3, NFS_V4 (libnfs-raw-nfs.h, libnfs-raw-nfs4.h) */
        note("mounting " + server + ":" + exp + " (NFS " + (v == 0 ? "3" : "4") + ")");
        const long long started = now_ms();
        const int rc = nfs_mount(nfs, server.c_str(), exp.c_str());
        if (rc == 0)
        {
            note("mounted " + server + ":" + exp + " in " + std::to_string(now_ms() - started) + " ms");
            session->most = std::uint32_t(nfs_get_readmax(nfs));
            return session;
        }
        why[v] = session->last_error();
        if (why[v].empty())
            why[v] = std::strerror(-rc);
        note("mounting " + server + ":" + exp + " (NFS " + (v == 0 ? "3" : "4") + ") failed after " +
             std::to_string(now_ms() - started) + " ms: " + why[v]);
        /* Version 4 only when version 3's mount service didn't answer at all
         * (a version-4-only server). A server that answered version 3 is a
         * version 3 one: a version 4 request can bring one down (WinNFSd). */
        if (v == 0 && !(why[0].find("portmap") != std::string::npos || why[0].find("connect") != std::string::npos ||
                        why[0].find("Timeout") != std::string::npos))
            break;
    }
    /* Version 3's reason, unless it couldn't reach the mount service at all
     * (a version-4-only server) or version 4 says the path isn't there (a
     * version 3 server refuses a path it doesn't export as "access denied"):
     * then 4's says more. */
    const bool v3_unreachable = why[0].find("portmap") != std::string::npos ||
                                why[0].find("connect") != std::string::npos ||
                                why[0].find("Timeout") != std::string::npos;
    const bool v4_missing = why[1].find("NOENT") != std::string::npos;
    error = v3_unreachable || v4_missing ? why[1] : why[0];
    return nullptr;
}

/* ---- either ------------------------------------------------------------------------------- */

std::unique_ptr<Session> connect_session(const Share &s, std::string &error)
{
    if (is_nfs(s))
        return nfs_connect(s, error);
    return smb_connect(s, s.share, error);
}

/* Connected, or false (with c.error). Holds c.m. */
bool ensure(Conn &c)
{
    if (c.session)
        return true;
    if (c.failed_at && now_ms() - c.failed_at < 3000)
        return false;
    std::string error;
    c.session = connect_session(c.cfg, error);
    if (!c.session)
    {
        c.failed_at = now_ms();
        if (error != c.error)
            note("can't connect to " + c.cfg.name + " (" + where(c.cfg) + "): " + error);
        c.error = error;
        return false;
    }
    c.failed_at = 0;
    c.error.clear();
    ++c.generation;
    c.max_read = c.session->max_read();
    if (c.max_read == 0 || c.max_read > (8u << 20))
        c.max_read = 1u << 20;
    note("connected to " + c.cfg.name + " (" + where(c.cfg) + "), reads up to " +
         std::to_string(c.max_read / 1024) + " KiB");
    return true;
}

/* Runs op (an int: >= 0 done, -errno failed) on the share, connecting first
 * and, when the connection has dropped, once more on a new one. Holds c.m. */
template <class Op> int run(Conn &c, Op op)
{
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        if (!ensure(c))
            return -ENOTCONN;
        c.session->clear_error(); /* no status left from an earlier request */
        const int rc = op(*c.session);
        if (rc >= 0 || c.session->request_error(rc))
            return rc;
        note("lost the connection to " + c.cfg.name + ": " + c.session->last_error());
        c.drop();
        c.failed_at = 0;
    }
    return -EIO;
}

/* ---- files ------------------------------------------------------------------------------ */

/* Small reads fetch more than asked, to read ahead: 64 KiB at first (a
 * header read in the library), twice as much each time the reads go on from
 * where the last fetch ended, up to 1 MiB (a game streaming its disc). */
constexpr std::uint32_t kFirstAhead = 64u << 10;
constexpr std::uint32_t kChunk = 1u << 20;

struct NetFile
{
    std::shared_ptr<Conn> conn;
    std::string inside;
    void *fh = nullptr;
    unsigned generation = 0;
    u64 size = 0;
    std::vector<std::uint8_t> cache;
    u64 cache_at = 0;
    std::uint32_t ahead = kFirstAhead;

    ~NetFile()
    {
        if (!conn)
            return;
        std::lock_guard<std::mutex> lock(conn->m);
        if (fh && conn->session && generation == conn->generation)
            conn->session->close(fh);
    }

    /* Opened on the current connection. Holds conn->m. */
    int reopen(Session &s)
    {
        if (fh && generation == conn->generation)
            return 0;
        fh = nullptr;
        const int rc = s.open(inside, &fh);
        if (rc < 0)
        {
            fh = nullptr;
            return rc; /* a missing file is a request error; anything else may be the connection */
        }
        generation = conn->generation;
        return 0;
    }

    bool open(std::shared_ptr<Conn> c, const std::string &path_inside)
    {
        conn = std::move(c);
        inside = path_inside;
        std::lock_guard<std::mutex> lock(conn->m);
        Info info;
        const int rc = run(*conn, [&](Session &s) {
            const int r = reopen(s);
            if (r < 0)
                return r;
            return s.fstat(fh, info);
        });
        if (rc < 0 || info.is_dir)
        {
            if (fh && conn->session && generation == conn->generation)
                conn->session->close(fh);
            fh = nullptr;
            return false;
        }
        size = info.size;
        return true;
    }

    /* Straight from the share. Holds conn->m. */
    s64 fetch(u64 offset, std::uint8_t *out, u64 n)
    {
        u64 done = 0;
        while (done < n)
        {
            const std::uint32_t want = std::uint32_t(std::min<u64>(n - done, conn->max_read ? conn->max_read : kChunk));
            int got = 0;
            const int rc = run(*conn, [&](Session &s) {
                const int r = reopen(s);
                if (r < 0)
                    return r;
                got = s.pread(fh, out + done, want, offset + done);
                return got;
            });
            if (rc < 0)
                return done ? s64(done) : -1;
            if (got == 0)
                break;
            done += u64(got);
        }
        return s64(done);
    }
    s64 read(u64 offset, void *dst, u64 n)
    {
        if (offset >= size)
            return 0;
        n = std::min(n, size - offset);
        auto *out = static_cast<std::uint8_t *>(dst);
        std::lock_guard<std::mutex> lock(conn->m);
        u64 done = 0;
        while (done < n)
        {
            const u64 at = offset + done;
            if (!cache.empty() && at >= cache_at && at < cache_at + cache.size())
            {
                const u64 take = std::min<u64>(n - done, cache_at + cache.size() - at);
                std::memcpy(out + done, cache.data() + (at - cache_at), take);
                done += take;
                continue;
            }
            const u64 left = n - done;
            if (left >= kChunk)
            {
                /* A big read: straight in, whole reads at a time. */
                const s64 got = fetch(at, out + done, left);
                if (got <= 0)
                    return done ? s64(done) : got;
                done += u64(got);
                continue;
            }
            /* A small one: a chunk into the cache, read ahead. */
            const bool onward = !cache.empty() && at == cache_at + cache.size();
            ahead = onward ? std::min(ahead * 2, kChunk) : kFirstAhead;
            const u64 chunk = std::min<u64>(std::max<u64>(ahead, left), size - at);
            cache.resize(chunk);
            const s64 got = fetch(at, cache.data(), chunk);
            if (got <= 0)
            {
                cache.clear();
                return done ? s64(done) : got;
            }
            cache.resize(std::size_t(got));
            cache_at = at;
        }
        return s64(done);
    }
};

/* ---- folders ---------------------------------------------------------------------------- */

bool list_net(const std::string &dir, std::vector<Entry> &out)
{
    out.clear();
    std::string inside;
    std::string name;
    if (is_net(dir))
    {
        std::string_view rest = std::string_view(dir).substr(std::strlen(kRoot));
        while (!rest.empty() && rest.front() == '/')
            rest.remove_prefix(1);
        if (rest.empty())
        {
            /* /net itself: the shares. */
            std::lock_guard<std::mutex> lock(g_mutex);
            for (const Share &s : g_shares)
                out.push_back({s.name, true});
            return true;
        }
    }
    const std::shared_ptr<Conn> c = resolve(dir, &inside, &name);
    if (!c)
        return false;
    std::lock_guard<std::mutex> lock(c->m);
    const int rc = run(*c, [&](Session &s) { return s.list(inside, out); });
    return rc >= 0;
}

bool stat_net(const std::string &path, bool *is_dir, u64 *size)
{
    std::string_view rest = std::string_view(path).substr(std::strlen(kRoot));
    while (!rest.empty() && rest.front() == '/')
        rest.remove_prefix(1);
    while (!rest.empty() && rest.back() == '/')
        rest.remove_suffix(1);
    std::string inside;
    const std::shared_ptr<Conn> c = rest.empty() ? nullptr : resolve(path, &inside);
    if (rest.empty() || (c && rest.find('/') == std::string_view::npos))
    {
        /* /net, or a share's own top: folders without asking the share. */
        if (!rest.empty() && !c)
            return false;
        if (is_dir)
            *is_dir = true;
        if (size)
            *size = 0;
        return true;
    }
    if (!c)
        return false;
    std::lock_guard<std::mutex> lock(c->m);
    Info info;
    const int rc = run(*c, [&](Session &s) { return s.stat(inside, info); });
    if (rc < 0)
        return false;
    if (is_dir)
        *is_dir = info.is_dir;
    if (size)
        *size = info.size;
    return true;
}

/* ---- the VFS ---------------------------------------------------------------------------- */
} // namespace
} // namespace porpoise::netfs

struct retro_vfs_file_handle
{
    std::string path;
    porpoise::netfs::NetFile file;
    std::int64_t pos = 0;
};

struct retro_vfs_dir_handle
{
    std::vector<porpoise::netfs::Entry> entries;
    std::size_t next = 0;
    bool started = false;
};

namespace porpoise::netfs
{
namespace
{
const char *RETRO_CALLCONV vfs_get_path(retro_vfs_file_handle *h)
{
    return h ? h->path.c_str() : nullptr;
}

retro_vfs_file_handle *RETRO_CALLCONV vfs_open(const char *path, unsigned mode, unsigned)
{
    if (!path || !is_net(path))
        return nullptr;
    if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
    {
        note(std::string("refused writing ") + path + " (network shares are read-only)");
        return nullptr;
    }
    std::string inside;
    std::shared_ptr<Conn> c = resolve(path, &inside);
    if (!c)
        return nullptr;
    auto h = std::make_unique<retro_vfs_file_handle>();
    h->path = path;
    if (!h->file.open(std::move(c), inside))
        return nullptr;
    return h.release();
}

int RETRO_CALLCONV vfs_close(retro_vfs_file_handle *h)
{
    delete h;
    return 0;
}

int64_t RETRO_CALLCONV vfs_size(retro_vfs_file_handle *h)
{
    return h ? int64_t(h->file.size) : -1;
}

int64_t RETRO_CALLCONV vfs_tell(retro_vfs_file_handle *h)
{
    return h ? h->pos : -1;
}

int64_t RETRO_CALLCONV vfs_seek(retro_vfs_file_handle *h, int64_t offset, int whence)
{
    if (!h)
        return -1;
    int64_t base = whence == RETRO_VFS_SEEK_POSITION_START     ? 0
                   : whence == RETRO_VFS_SEEK_POSITION_CURRENT ? h->pos
                   : whence == RETRO_VFS_SEEK_POSITION_END     ? int64_t(h->file.size)
                                                               : -1;
    if (base < 0 || base + offset < 0)
        return -1;
    h->pos = base + offset;
    return 0; /* libretro.h: 0 on success */
}

int64_t RETRO_CALLCONV vfs_read(retro_vfs_file_handle *h, void *s, uint64_t len)
{
    if (!h || h->pos < 0)
        return -1;
    const s64 got = h->file.read(u64(h->pos), s, len);
    if (got > 0)
        h->pos += got;
    return got;
}

int64_t RETRO_CALLCONV vfs_write(retro_vfs_file_handle *, const void *, uint64_t)
{
    return -1;
}

int RETRO_CALLCONV vfs_flush(retro_vfs_file_handle *)
{
    return 0;
}

int RETRO_CALLCONV vfs_remove(const char *)
{
    return -1;
}

int RETRO_CALLCONV vfs_rename(const char *, const char *)
{
    return -1;
}

int64_t RETRO_CALLCONV vfs_truncate(retro_vfs_file_handle *, int64_t)
{
    return -1;
}

int RETRO_CALLCONV vfs_stat_64(const char *path, int64_t *size)
{
    bool dir = false;
    u64 n = 0;
    if (!path || !is_net(path) || !netfs::stat(path, &dir, &n))
        return 0;
    if (size)
        *size = int64_t(n);
    return RETRO_VFS_STAT_IS_VALID | (dir ? RETRO_VFS_STAT_IS_DIRECTORY : 0);
}

int RETRO_CALLCONV vfs_stat(const char *path, int32_t *size)
{
    int64_t n = 0;
    const int flags = vfs_stat_64(path, &n);
    if (size)
        *size = int32_t(std::min<int64_t>(n, INT32_MAX));
    return flags;
}

int RETRO_CALLCONV vfs_mkdir(const char *)
{
    return -1;
}

retro_vfs_dir_handle *RETRO_CALLCONV vfs_opendir(const char *dir, bool)
{
    if (!dir || !is_net(dir))
        return nullptr;
    auto h = std::make_unique<retro_vfs_dir_handle>();
    if (!list_net(dir, h->entries))
        return nullptr;
    return h.release();
}

bool RETRO_CALLCONV vfs_readdir(retro_vfs_dir_handle *h)
{
    if (!h)
        return false;
    if (h->started)
        ++h->next;
    h->started = true;
    return h->next < h->entries.size();
}

const char *RETRO_CALLCONV vfs_dirent_get_name(retro_vfs_dir_handle *h)
{
    return h && h->started && h->next < h->entries.size() ? h->entries[h->next].name.c_str() : nullptr;
}

bool RETRO_CALLCONV vfs_dirent_is_dir(retro_vfs_dir_handle *h)
{
    return h && h->started && h->next < h->entries.size() && h->entries[h->next].is_dir;
}

int RETRO_CALLCONV vfs_closedir(retro_vfs_dir_handle *h)
{
    delete h;
    return 0;
}

retro_vfs_interface g_vfs = {
    vfs_get_path, vfs_open,  vfs_close,  vfs_size,    vfs_tell,    vfs_seek,
    vfs_read,     vfs_write, vfs_flush,  vfs_remove,  vfs_rename,  vfs_truncate,
    vfs_stat,     vfs_mkdir, vfs_opendir, vfs_readdir, vfs_dirent_get_name, vfs_dirent_is_dir,
    vfs_closedir, vfs_stat_64,
};

/* ---- the saved list --------------------------------------------------------------------- */

std::string escape(const std::string &s)
{
    std::string out;
    for (char ch : s)
    {
        if (ch == '\\')
            out += "\\\\";
        else if (ch == '\t')
            out += "\\t";
        else if (ch == '\n')
            out += "\\n";
        else if (ch == '\r')
            out += "\\r";
        else
            out += ch;
    }
    return out;
}

std::string unescape(const std::string &s)
{
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            const char n = s[++i];
            out += n == 't' ? '\t' : n == 'n' ? '\n' : n == 'r' ? '\r' : n;
        }
        else
            out += s[i];
    }
    return out;
}

/* What went wrong, from libsmb2's message or the server's status. */
Problem classify(const std::string &error)
{
    auto has = [&](const char *k) { return error.find(k) != std::string::npos; };
    if (has("LOGON_FAILURE") || has("WRONG_PASSWORD") || has("ACCOUNT_") || has("PASSWORD_EXPIRED"))
        return Problem::SignIn;
    if (has("BAD_NETWORK_NAME"))
        return Problem::NoSuchShare;
    if (has("ACCESS_DENIED") || has("ERR_ACCES") || has("ERR_PERM") || has("Permission denied") ||
        has("AUTH_ERROR") || has("AUTH_TOOWEAK") || has("AUTH_BADCRED"))
        return Problem::Denied;
    if (has("NOT_FOUND") || has("NOT_A_DIRECTORY") || has("PATH_INVALID") || has("ERR_NOENT") ||
        has("ERR_NOTDIR") || has("No such file"))
        return Problem::NoFolder;
    if (has("resolve") || has("Invalid address"))
        return Problem::UnknownName;
    if (has("connect failed") || has("Timeout") || has("timed out") || has("TIMEOUT") || has("POLLHUP") ||
        has("socket") || has("connect()") || has("Failed to connect") || has("portmap") || has("RPC ERROR"))
        return Problem::Unreachable;
    return Problem::Other;
}
} // namespace

/* ---- the API ---------------------------------------------------------------------------- */

bool is_net(std::string_view path)
{
    const std::size_t n = std::strlen(kRoot);
    return path.size() >= n && path.compare(0, n, kRoot) == 0 && (path.size() == n || path[n] == '/');
}

void set_shares(const std::vector<Share> &shares)
{
    /* Connections that go: let go of here, closed by whoever holds one last
     * (a search or a game reading through it), never waited for here. */
    std::vector<std::shared_ptr<Conn>> closing;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_shares = shares;
        std::map<std::string, std::shared_ptr<Conn>> next;
        for (const Share &s : shares)
        {
            auto it = g_conns.find(s.name);
            if (it != g_conns.end() && same_target(it->second->cfg, s))
                next[s.name] = it->second;
            else
            {
                auto c = std::make_shared<Conn>();
                c->cfg = s;
                next[s.name] = c;
            }
        }
        for (auto &kv : g_conns)
            if (!next.count(kv.first) || next[kv.first] != kv.second)
                closing.push_back(kv.second);
        g_conns.swap(next);
    }
    closing.clear();
}

std::vector<Share> shares()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_shares;
}

std::string root_of(const Share &share)
{
    return std::string(kRoot) + "/" + share.name;
}

std::string unique_name(const Share &share, const std::vector<Share> &existing)
{
    std::string base = share.share;
    std::replace(base.begin(), base.end(), '\\', '/');
    while (!base.empty() && base.back() == '/')
        base.pop_back();
    if (base.find('/') != std::string::npos)
        base = base.substr(base.rfind('/') + 1); /* an NFS export: its last folder */
    if (base.empty())
        base = server_of(share);
    std::string clean;
    for (char ch : base)
        clean += (ch == '/' || ch == '\\' || ch == ':' || ch == '\t' || ch == '\n') ? '-' : ch;
    if (clean.empty())
        clean = "Share";
    std::string name = clean;
    for (int n = 2;; ++n)
    {
        const bool taken = std::any_of(existing.begin(), existing.end(), [&](const Share &s) { return s.name == name; });
        if (!taken)
            return name;
        name = clean + " " + std::to_string(n);
    }
}

bool load(const std::string &file, std::vector<Share> &out)
{
    out.clear();
    std::FILE *f = std::fopen(file.c_str(), "r");
    if (!f)
        return errno == ENOENT;
    char line[4096];
    while (std::fgets(line, sizeof line, f))
    {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r'))
            l.pop_back();
        if (l.empty() || l[0] == '#')
            continue;
        std::vector<std::string> fields;
        std::size_t at = 0;
        for (;;)
        {
            const std::size_t tab = l.find('\t', at);
            fields.push_back(unescape(l.substr(at, tab == std::string::npos ? std::string::npos : tab - at)));
            if (tab == std::string::npos)
                break;
            at = tab + 1;
        }
        if (fields.size() < 3 || fields[0].empty())
            continue;
        fields.resize(7);
        Share s{fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6] == "nfs" ? "nfs" : ""};
        out.push_back(std::move(s));
    }
    std::fclose(f);
    return true;
}

bool save(const std::string &file, const std::vector<Share> &list)
{
    const std::string tmp = file + ".tmp";
    std::FILE *f = std::fopen(tmp.c_str(), "w");
    if (!f)
        return false;
    std::fputs("# Porpoise's network shares: name, computer, share (or NFS export), folder, username, password, "
               "nfs for an NFS share\n",
               f);
    for (const Share &s : list)
        std::fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", escape(s.name).c_str(), escape(s.host).c_str(),
                     escape(s.share).c_str(), escape(s.folder).c_str(), escape(s.user).c_str(),
                     escape(s.password).c_str(), is_nfs(s) ? "nfs" : "smb");
    const bool ok = std::fflush(f) == 0;
    std::fclose(f);
    if (!ok || std::rename(tmp.c_str(), file.c_str()) != 0)
    {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool list(const std::string &dir, std::vector<Entry> &out)
{
    if (is_net(dir))
        return list_net(dir, out);
    out.clear();
    DIR *d = opendir(dir.c_str());
    if (!d)
        return false;
    while (dirent *e = readdir(d))
    {
        if (hidden(e->d_name))
            continue;
        bool is_dir = false;
#ifdef DT_DIR
        if (e->d_type == DT_DIR)
            is_dir = true;
        else if (e->d_type == DT_REG)
            is_dir = false;
        else
#endif
        {
            struct ::stat st;
            if (::stat((dir + "/" + e->d_name).c_str(), &st) != 0)
                continue;
            is_dir = S_ISDIR(st.st_mode);
        }
        out.push_back({e->d_name, is_dir});
    }
    closedir(d);
    return true;
}

bool stat(const std::string &path, bool *is_dir, std::uint64_t *size)
{
    if (is_net(path))
        return stat_net(path, is_dir, size);
    struct ::stat st;
    if (::stat(path.c_str(), &st) != 0)
        return false;
    if (is_dir)
        *is_dir = S_ISDIR(st.st_mode);
    if (size)
        *size = S_ISDIR(st.st_mode) ? 0 : std::uint64_t(st.st_size);
    return true;
}

struct Reader::Impl
{
    std::FILE *local = nullptr;
    std::unique_ptr<NetFile> net;
    u64 size = 0;
};

Reader::Reader() : impl_(std::make_unique<Impl>()) {}
Reader::~Reader()
{
    close();
}

bool Reader::open(const std::string &path)
{
    close();
    if (is_net(path))
    {
        std::string inside;
        std::shared_ptr<Conn> c = resolve(path, &inside);
        if (!c)
            return false;
        auto f = std::make_unique<NetFile>();
        if (!f->open(std::move(c), inside))
            return false;
        impl_->size = f->size;
        impl_->net = std::move(f);
        return true;
    }
    impl_->local = std::fopen(path.c_str(), "rb");
    if (!impl_->local)
        return false;
    if (fseeko(impl_->local, 0, SEEK_END) == 0)
        impl_->size = u64(ftello(impl_->local));
    return true;
}

void Reader::close()
{
    if (impl_->local)
        std::fclose(impl_->local);
    impl_->local = nullptr;
    impl_->net.reset();
    impl_->size = 0;
}

bool Reader::is_open() const
{
    return impl_->local || impl_->net;
}

std::uint64_t Reader::size() const
{
    return impl_->size;
}

std::int64_t Reader::read_some(std::uint64_t offset, void *out, std::uint64_t n)
{
    if (impl_->net)
        return impl_->net->read(offset, out, n);
    if (!impl_->local || fseeko(impl_->local, off_t(offset), SEEK_SET) != 0)
        return -1;
    const std::size_t got = std::fread(out, 1, std::size_t(n), impl_->local);
    return got == 0 && std::ferror(impl_->local) ? -1 : s64(got);
}

bool Reader::read(std::uint64_t offset, void *out, std::uint64_t n)
{
    if (offset > impl_->size || n > impl_->size - offset)
        return false;
    return read_some(offset, out, n) == s64(n);
}

Problem test(const Share &share, std::string *detail)
{
    if (server_of(share).empty())
        return Problem::NoComputer;
    if (share.share.empty())
        return Problem::NoShare;
    std::string error;
    std::unique_ptr<Session> session = connect_session(share, error);
    if (!session)
    {
        note("test of " + where(share) + " failed: " + error);
        if (detail)
            *detail = error;
        const Problem p = classify(error);
        if (is_nfs(share) && (p == Problem::Denied || p == Problem::NoFolder))
        {
            /* A version 3 server refuses a path it doesn't export as "access
             * denied" (WinNFSd): when its list hasn't got it, that's what it
             * is, and the list says which to use. */
            std::vector<std::string> exports;
            if (enumerate(share, exports) == Problem::None && !exports.empty())
            {
                auto plain = [](std::string e) {
                    std::replace(e.begin(), e.end(), '\\', '/');
                    while (e.size() > 1 && e.back() == '/')
                        e.pop_back();
                    return e; /* case and all: a Linux or NAS server tells /Games from /games */
                };
                const std::string wanted = plain(export_of(share));
                bool listed = false;
                std::string names;
                for (const std::string &e : exports)
                {
                    listed |= plain(e) == wanted;
                    names += (names.empty() ? "" : ", ") + e;
                }
                if (!listed)
                {
                    note(where(share) + " isn't among its exports: " + names);
                    if (detail)
                        *detail = names;
                    return Problem::NoSuchShare;
                }
            }
        }
        /* A mount refused for a path the server doesn't export. */
        return is_nfs(share) && p == Problem::NoFolder ? Problem::NoSuchShare : p;
    }
    std::string folder = share.folder;
    std::replace(folder.begin(), folder.end(), '\\', '/');
    while (!folder.empty() && folder.front() == '/')
        folder.erase(folder.begin());
    while (!folder.empty() && folder.back() == '/')
        folder.pop_back();
    session->clear_error();
    std::vector<Entry> entries;
    Problem result = Problem::None;
    if (session->list(folder, entries) < 0)
    {
        error = session->last_error();
        note("test of " + where(share) + ", its folder: " + error);
        if (detail)
            *detail = error;
        result = classify(error);
        if (result == Problem::Other || result == Problem::Unreachable || result == Problem::NoSuchShare)
            result = Problem::NoFolder;
    }
    else
        note("test of " + where(share) + ": " + std::to_string(entries.size()) +
             " entries at the top");
    return result; /* closed without a goodbye: a server slow to answer it would hold the panel */
}

Problem enumerate(const Share &server, std::vector<std::string> &out, std::string *detail)
{
    out.clear();
    if (server_of(server).empty())
        return Problem::NoComputer;
    if (is_nfs(server))
    {
        /* The server's exports, from its mount service. */
        std::string raw;
        if (!probe(server_of(server), 2049, raw))
        {
            if (detail)
                *detail = raw;
            return classify(raw);
        }
        note("asking " + server_of(server) + " for its NFS exports");
        exportnode *list = mount_getexports_timeout(server_of(server).c_str(), 10000);
        if (!list)
        {
            raw = "the server didn't list its exports (no mount service, or it isn't allowed)";
            note("listing the exports of " + server_of(server) + " failed");
            if (detail)
                *detail = raw;
            return Problem::NoExports;
        }
        for (exportnode *e = list; e; e = e->ex_next)
            if (e->ex_dir && e->ex_dir[0])
                out.push_back(e->ex_dir);
        mount_free_export_list(list);
        note(server_of(server) + " exports " + std::to_string(out.size()) + " folders");
        std::sort(out.begin(), out.end());
        return Problem::None;
    }
    std::string raw;
    std::unique_ptr<SmbSession> session = smb_connect(server, "IPC$", raw);
    if (!session)
    {
        note("listing the shares of " + server_of(server) + " failed: " + raw);
        if (detail)
            *detail = raw;
        return classify(raw);
    }
    note("asking " + server_of(server) + " for its shared folders");
    smb2_share_enum_reply *reply = smb2_share_enum_sync(session->ctx, SMB2_SHARE_INFO_1);
    if (!reply)
    {
        raw = session->last_error();
        note("listing the shares of " + server_of(server) + " failed: " + raw);
        if (detail)
            *detail = raw;
        return classify(raw);
    }
    for (std::uint32_t i = 0; i < reply->entries_read; ++i)
    {
        const smb2_share_info_1 &info = reply->share_info.info_1[i];
        if ((info.type & 3) != SMB2_SHARE_TYPE_DISKTREE || (info.type & SMB2_SHARE_TYPE_HIDDEN) || !info.netname)
            continue;
        const std::string name = info.netname;
        if (name.empty() || name.back() == '$')
            continue;
        out.push_back(name);
    }
    smb2_free_data(session->ctx, reply);
    note(server_of(server) + " shares " + std::to_string(out.size()) + " folders Porpoise can use");
    std::sort(out.begin(), out.end());
    return Problem::None;
}

/* ---- finding computers --------------------------------------------------------------- */

namespace
{
/* Non-blocking, on the console too (fcntl's O_NONBLOCK doesn't take there). */
void make_nonblocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#ifdef __PROSPERO__
    int one = 1;
    setsockopt(fd, SOL_SOCKET, 0x1200, &one, sizeof one); /* SO_NBIO */
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
}

/* The console's own IPv4 address: a UDP socket "connected" toward a
 * documentation address (nothing is sent) says which one it would use. */
bool own_address(in_addr &out)
{
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return false;
    sockaddr_in to{};
#ifdef __PROSPERO__
    to.sin_len = sizeof to;
#endif
    to.sin_family = AF_INET;
    to.sin_port = htons(9);
    inet_pton(AF_INET, "192.0.2.1", &to.sin_addr);
    sockaddr_in me{};
    socklen_t len = sizeof me;
    const bool ok = connect(fd, reinterpret_cast<sockaddr *>(&to), sizeof to) == 0 &&
                    getsockname(fd, reinterpret_cast<sockaddr *>(&me), &len) == 0 && me.sin_addr.s_addr != 0;
    close(fd);
    if (ok)
        out = me.sin_addr;
    return ok;
}

/* Which of these addresses answer on which of these ports: non-blocking
 * connects, a batch at a time, each batch given up to wait_ms. open[host][port]. */
std::vector<std::vector<bool>> answering(const std::vector<in_addr> &hosts, const std::vector<int> &ports,
                                         int wait_ms)
{
    std::vector<std::vector<bool>> open(hosts.size(), std::vector<bool>(ports.size(), false));
    std::vector<std::pair<std::size_t, std::size_t>> tries; /* host, port */
    for (std::size_t h = 0; h < hosts.size(); ++h)
        for (std::size_t p = 0; p < ports.size(); ++p)
            tries.push_back({h, p});
    constexpr std::size_t kBatch = 128;
    for (std::size_t first = 0; first < tries.size(); first += kBatch)
    {
        const std::size_t last = std::min(tries.size(), first + kBatch);
        std::vector<pollfd> fds;
        std::vector<std::size_t> which;
        for (std::size_t t = first; t < last; ++t)
        {
            const int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0)
                continue;
            make_nonblocking(fd);
            sockaddr_in to{};
#ifdef __PROSPERO__
            to.sin_len = sizeof to;
#endif
            to.sin_family = AF_INET;
            to.sin_port = htons(std::uint16_t(ports[tries[t].second]));
            to.sin_addr = hosts[tries[t].first];
            const int rc = connect(fd, reinterpret_cast<sockaddr *>(&to), sizeof to);
            if (rc == 0)
            {
                open[tries[t].first][tries[t].second] = true;
                close(fd);
                continue;
            }
            if (errno != EINPROGRESS)
            {
                close(fd);
                continue;
            }
            fds.push_back({fd, POLLOUT, 0});
            which.push_back(t);
        }
        const long long deadline = now_ms() + wait_ms;
        std::size_t left = fds.size();
        while (left > 0)
        {
            const long long now = now_ms();
            if (now >= deadline)
                break;
            if (poll(fds.data(), nfds_t(fds.size()), int(deadline - now)) <= 0)
                break;
            for (std::size_t k = 0; k < fds.size(); ++k)
            {
                if (fds[k].fd < 0 || fds[k].revents == 0)
                    continue;
                int err = 0;
                socklen_t len = sizeof err;
                if (getsockopt(fds[k].fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0 &&
                    (fds[k].revents & POLLOUT))
                    open[tries[which[k]].first][tries[which[k]].second] = true;
                close(fds[k].fd);
                fds[k].fd = -1; /* poll skips it */
                --left;
            }
        }
        for (pollfd &p : fds)
            if (p.fd >= 0)
                close(p.fd);
    }
    return open;
}

/* NetBIOS names (a Windows computer's, or Samba's): a node status query to
 * each address on port 137 at once, the answers taken for wait_ms. */
std::vector<std::string> netbios_names(const std::vector<in_addr> &hosts, int wait_ms)
{
    std::vector<std::string> names(hosts.size());
    if (hosts.empty())
        return names;
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return names;
    make_nonblocking(fd);
    /* A node status request for "*": the name half-ASCII encoded (CK + 30 A's). */
    std::uint8_t query[50] = {0x50, 0x4F, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 'C', 'K'};
    for (int i = 15; i < 45; ++i)
        query[i] = 'A';
    query[45] = 0;
    query[46] = 0x00;
    query[47] = 0x21; /* NBSTAT */
    query[48] = 0x00;
    query[49] = 0x01; /* IN */
    for (const in_addr &h : hosts)
    {
        sockaddr_in to{};
#ifdef __PROSPERO__
        to.sin_len = sizeof to;
#endif
        to.sin_family = AF_INET;
        to.sin_port = htons(137);
        to.sin_addr = h;
        sendto(fd, query, sizeof query, 0, reinterpret_cast<sockaddr *>(&to), sizeof to);
    }
    const long long deadline = now_ms() + wait_ms;
    std::size_t got = 0;
    while (got < hosts.size())
    {
        const long long now = now_ms();
        if (now >= deadline)
            break;
        pollfd p{fd, POLLIN, 0};
        if (poll(&p, 1, int(deadline - now)) <= 0)
            break;
        std::uint8_t reply[1024];
        sockaddr_in from{};
        socklen_t len = sizeof from;
        const ssize_t n = recvfrom(fd, reply, sizeof reply, 0, reinterpret_cast<sockaddr *>(&from), &len);
        /* Header 12, the name 34, type, class, TTL, length 10, then a count
         * and 18 bytes a name: 15 characters, the suffix, two flag bytes. */
        constexpr std::size_t kNames = 12 + 34 + 10;
        if (n < ssize_t(kNames + 1))
            continue;
        const int count = reply[kNames];
        std::string name;
        for (int i = 0; i < count && kNames + 1 + std::size_t(i + 1) * 18 <= std::size_t(n); ++i)
        {
            const std::uint8_t *e = reply + kNames + 1 + i * 18;
            const bool group = (e[16] & 0x80) != 0;
            if (e[15] != 0x00 || group) /* the workstation name: suffix 0, unique */
                continue;
            std::string nb(reinterpret_cast<const char *>(e), 15);
            while (!nb.empty() && (nb.back() == ' ' || nb.back() == 0))
                nb.pop_back();
            name = nb;
            break;
        }
        for (std::size_t i = 0; i < hosts.size(); ++i)
            if (hosts[i].s_addr == from.sin_addr.s_addr && names[i].empty() && !name.empty())
            {
                names[i] = name;
                ++got;
            }
    }
    close(fd);
    return names;
}
} // namespace

std::vector<Found> discover()
{
    std::vector<Found> out;
    in_addr me{};
    if (!own_address(me))
    {
        note("discover: no network address of our own");
        return out;
    }
#ifdef PORPOISE_NETFS_HOST_TEST
    if (std::getenv("NETFS_TEST_LOOPBACK"))
        inet_pton(AF_INET, "127.0.0.200", &me); /* the tests' servers are on 127.0.0.1 */
#endif
    const std::uint32_t own = ntohl(me.s_addr);
    const std::uint32_t net = own & 0xFFFFFF00u; /* the 254 addresses around it (a home network) */
    std::vector<in_addr> hosts;
    std::uint32_t last_host = 254;
#ifdef PORPOISE_NETFS_HOST_TEST
    if (std::getenv("NETFS_TEST_LOOPBACK"))
        last_host = 3; /* every 127.0.0.x is this machine: three are enough */
#endif
    for (std::uint32_t h = 1; h <= last_host; ++h)
    {
        const std::uint32_t a = net | h;
        if (a == own)
            continue;
        in_addr ia{};
        ia.s_addr = htonl(a);
        hosts.push_back(ia);
    }
    const long long started = now_ms();
    const auto open = answering(hosts, {445, 2049}, 600);
    std::vector<in_addr> named;
    for (std::size_t i = 0; i < hosts.size(); ++i)
        if (open[i][0] || open[i][1])
        {
            char text[INET_ADDRSTRLEN] = "";
            inet_ntop(AF_INET, &hosts[i], text, sizeof text);
            out.push_back({text, "", open[i][0], open[i][1]});
            named.push_back(hosts[i]);
        }
    const std::vector<std::string> names = netbios_names(named, 600);
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        out[i].name = names[i];
        if (out[i].name.empty() && out[i].smb)
        {
            /* No NetBIOS answer (Windows's firewall often keeps it out): the
             * name the server gives as it starts a sign-in, as a guest to IPC$. */
            smb2_context *ctx = smb2_init_context();
            if (ctx)
            {
                t_target_name.clear();
                smb2_set_timeout(ctx, 3);
                smb2_set_security_mode(ctx, SMB2_NEGOTIATE_SIGNING_ENABLED);
                smb2_set_user(ctx, "Guest");
                smb2_connect_share(ctx, out[i].address.c_str(), "IPC$", nullptr);
                smb2_destroy_context(ctx);
                out[i].name = t_target_name;
                t_target_name.clear();
            }
        }
    }
    char own_text[INET_ADDRSTRLEN] = "";
    inet_ntop(AF_INET, &me, own_text, sizeof own_text);
    std::string seen;
    for (const Found &f : out)
        seen += " " + f.address + (f.name.empty() ? "" : " (" + f.name + ")") + (f.smb ? " smb" : "") +
                (f.nfs ? " nfs" : "");
    note("discover: from " + std::string(own_text) + ", " + std::to_string(out.size()) + " sharing in " +
         std::to_string(now_ms() - started) + " ms:" + seen);
    return out;
}

const retro_vfs_interface *vfs_interface()
{
    return &g_vfs;
}

void shutdown()
{
    set_shares({});
}
} // namespace porpoise::netfs

/* ---- name lookups for libsmb2 and libnfs on the PS5 ------------------------------------------------- */
#ifdef __PROSPERO__
/* The console's libc has no getaddrinfo in the modules Porpoise loads:
 * libsmb2 and libnfs are built to call these instead. An address as it is, a name through
 * the console's own resolver (DNS). */
extern "C"
{
int sceNetInit(void);
int sceNetPoolCreate(const char *name, int size, int flags);
int sceNetPoolDestroy(int memid);
int sceNetResolverCreate(const char *name, int memid, int flags);
int sceNetResolverStartNtoa(int rid, const char *hostname, struct in_addr *addr, int timeout, int retry,
                            int flags);
int sceNetResolverDestroy(int rid);

int porpoise_netfs_getaddrinfo(const char *node, const char *service, const struct addrinfo *,
                               struct addrinfo **res)
{
    if (!node || !res)
        return EAI_NONAME;
    in_addr addr{};
    if (inet_pton(AF_INET, node, &addr) != 1)
    {
        (void)sceNetInit();
        const int pool = sceNetPoolCreate("porpoise-smb", 4 * 1024, 0);
        if (pool < 0)
            return EAI_FAIL;
        const int rid = sceNetResolverCreate("porpoise-smb", pool, 0);
        int rc = -1;
        if (rid >= 0)
        {
            /* 2 s a try, 3 tries (microseconds), never left waiting. */
            rc = sceNetResolverStartNtoa(rid, node, &addr, 2 * 1000 * 1000, 2, 0);
            sceNetResolverDestroy(rid);
        }
        sceNetPoolDestroy(pool);
        if (rc < 0)
            return EAI_NONAME;
    }
    auto *sin = static_cast<sockaddr_in *>(std::calloc(1, sizeof(sockaddr_in)));
    auto *ai = static_cast<addrinfo *>(std::calloc(1, sizeof(addrinfo)));
    if (!sin || !ai)
    {
        std::free(sin);
        std::free(ai);
        return EAI_MEMORY;
    }
    sin->sin_len = sizeof(sockaddr_in);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(std::uint16_t(service ? std::atoi(service) : 0)); /* no service: the caller sets the port */
    sin->sin_addr = addr;
    ai->ai_family = AF_INET;
    ai->ai_socktype = SOCK_STREAM;
    ai->ai_protocol = IPPROTO_TCP;
    ai->ai_addrlen = sizeof(sockaddr_in);
    ai->ai_addr = reinterpret_cast<sockaddr *>(sin);
    *res = ai;
    return 0;
}

void porpoise_netfs_freeaddrinfo(struct addrinfo *ai)
{
    while (ai)
    {
        addrinfo *next = ai->ai_next;
        std::free(ai->ai_addr);
        std::free(ai);
        ai = next;
    }
}

/* No login name on the console: libsmb2 then uses "Guest". */
int porpoise_netfs_getlogin_r(char *, size_t)
{
    return -1;
}

int porpoise_netfs_gethostname(char *name, size_t len)
{
    if (!name || len == 0)
        return -1;
    std::strncpy(name, "PS5", len);
    name[len - 1] = 0;
    return 0;
}

/* FreeBSD's stdio macros read __isthreaded; the locked calls are always right. */
int porpoise_netfs_isthreaded = 1;

/* libnfs's: TCP is protocol 6; no port is a "well-known service" it must
 * avoid binding to (it only looks so as to skip them); numeric names only. */
struct protoent *porpoise_netfs_getprotobyname(const char *name)
{
    static char tcp_name[] = "tcp";
    static char *aliases[] = {nullptr};
    static protoent tcp{tcp_name, aliases, IPPROTO_TCP};
    return name && std::strcmp(name, "tcp") == 0 ? &tcp : nullptr;
}

struct servent *porpoise_netfs_getservbyport(int, const char *)
{
    return nullptr;
}

int porpoise_netfs_getnameinfo(const struct sockaddr *sa, socklen_t, char *host, size_t hostlen, char *serv,
                               size_t servlen, int)
{
    if (!sa || sa->sa_family != AF_INET)
        return EAI_FAMILY;
    const auto *sin = reinterpret_cast<const sockaddr_in *>(sa);
    if (host && hostlen && !inet_ntop(AF_INET, &sin->sin_addr, host, socklen_t(hostlen)))
        return EAI_FAIL;
    if (serv && servlen)
        std::snprintf(serv, servlen, "%u", unsigned(ntohs(sin->sin_port)));
    return 0;
}
}
#endif

/* libsmb2's NTLM challenge names the server (lib/ntlmssp.c): kept for the
 * thread that asked (discover). */
extern "C" void porpoise_netfs_target_name(const char *name)
{
    t_target_name = name ? name : "";
}

/* libsmb2's sync wait, in the log only when it's slow: after two seconds
 * without the answer, then every ten rounds. (A busy transfer goes round
 * often in no time; that isn't worth a line.) */
extern "C" void porpoise_netfs_wait_note(int round, long waited, int fd, int events, int poll_rc, int revents)
{
    if (waited >= 2 && round % 10 == 0)
        note("wait: round " + std::to_string(round) + ", " + std::to_string(waited) + " s, fd " +
             std::to_string(fd) + " events " + std::to_string(events) + ", poll " + std::to_string(poll_rc) +
             " revents " + std::to_string(revents));
}
