/* Porpoise - games on a network share (SMB), read with libsmb2.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_netfs.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "libretro.h"

extern "C"
{
#include <smb2/smb2.h>
#include <smb2/libsmb2.h>
#include <smb2/libsmb2-raw.h>
#include <smb2/libsmb2-share-enum.h>
}

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

/* One connection a share, one request on it at a time. */
struct Conn
{
    Share cfg;
    std::mutex m;
    smb2_context *ctx = nullptr;
    unsigned generation = 0; /* a new one each time it connects; files opened before reopen */
    std::uint32_t max_read = 0;
    long long failed_at = 0; /* when connecting last failed: not tried again for a moment */
    std::string error;

    /* Closes the connection without a goodbye to the server: on a dead
     * connection that would wait out the timeout again. The server lets go of
     * the session when the socket closes. */
    void drop()
    {
        if (ctx)
        {
            smb2_destroy_context(ctx);
            ctx = nullptr;
        }
    }
    ~Conn() { drop(); }
};

std::mutex g_mutex; /* the share list */
std::vector<Share> g_shares;
std::map<std::string, std::shared_ptr<Conn>> g_conns;

bool same_target(const Share &a, const Share &b)
{
    return a.host == b.host && a.share == b.share && a.folder == b.folder && a.user == b.user &&
           a.password == b.password;
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

/* The server answered with an error about the request itself (a missing
 * file, no permission): no point reconnecting. A failure with no answer (a
 * socket error) or one that says the session or handle is gone is the
 * connection's. */
bool request_error(smb2_context *ctx)
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

std::string server_of(const Share &s)
{
    std::string host = s.host;
    while (!host.empty() && (host.back() == ' ' || host.back() == '/' || host.back() == '\\'))
        host.pop_back();
    while (!host.empty() && (host.front() == ' ' || host.front() == '/' || host.front() == '\\'))
        host.erase(host.begin());
    if (host.rfind("smb:", 0) == 0)
    {
        host.erase(0, 4);
        while (!host.empty() && host.front() == '/')
            host.erase(host.begin());
    }
    return host;
}

/* The last request's error: the server's own status (STATUS_LOGON_FAILURE)
 * when it sent one, which libsmb2's message can hide behind the socket
 * closing, else that message. */
std::string last_error(smb2_context *ctx)
{
    const int status = smb2_get_nterror(ctx);
    if (status != 0)
        return nterror_to_str(std::uint32_t(status));
    const char *e = smb2_get_error(ctx);
    return e ? e : "";
}

/* -errno for a request that failed. */
int last_errno(smb2_context *ctx)
{
    const int status = smb2_get_nterror(ctx);
    if (status != 0)
    {
        const int e = nterror_to_errno(std::uint32_t(status));
        return e > 0 ? -e : -EIO;
    }
    return -EIO;
}

/* A new context, connected to share (IPC$ to list the shares). */
smb2_context *open_context(const Share &s, const std::string &share, std::string &error)
{
    smb2_context *ctx = smb2_init_context();
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
    const int rc = smb2_connect_share(ctx, server.c_str(), share.c_str(), guest ? nullptr : user.c_str());
    if (rc < 0)
    {
        error = last_error(ctx);
        if (error.empty())
            error = std::strerror(-rc);
        smb2_destroy_context(ctx);
        return nullptr;
    }
    return ctx;
}

/* Connected, or false (with c.error). Holds c.m. */
bool ensure(Conn &c)
{
    if (c.ctx)
        return true;
    if (c.failed_at && now_ms() - c.failed_at < 3000)
        return false;
    std::string error;
    c.ctx = open_context(c.cfg, c.cfg.share, error);
    if (!c.ctx)
    {
        c.failed_at = now_ms();
        if (error != c.error)
            note("can't connect to " + c.cfg.name + " (" + server_of(c.cfg) + "/" + c.cfg.share + "): " + error);
        c.error = error;
        return false;
    }
    c.failed_at = 0;
    c.error.clear();
    ++c.generation;
    c.max_read = smb2_get_max_read_size(c.ctx);
    if (c.max_read == 0 || c.max_read > (8u << 20))
        c.max_read = 1u << 20;
    note("connected to " + c.cfg.name + " (" + server_of(c.cfg) + "/" + c.cfg.share + "), reads up to " +
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
        smb2_set_error(c.ctx, ""); /* no status left from an earlier request */
        const int rc = op(c.ctx);
        if (rc >= 0 || request_error(c.ctx))
            return rc;
        note("lost the connection to " + c.cfg.name + ": " + last_error(c.ctx));
        c.drop();
        c.failed_at = 0;
    }
    return -EIO;
}

bool hidden(const char *name)
{
    return !name || !name[0] || name[0] == '.';
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
    smb2fh *fh = nullptr;
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
        if (fh && conn->ctx && generation == conn->generation)
            smb2_close(conn->ctx, fh);
    }

    /* Opened on the current connection. Holds conn->m. */
    int reopen(smb2_context *ctx)
    {
        if (fh && generation == conn->generation)
            return 0;
        fh = smb2_open(ctx, inside.c_str(), O_RDONLY);
        if (!fh)
            return last_errno(ctx); /* a missing file is a file error; anything else may be the connection */
        generation = conn->generation;
        return 0;
    }

    bool open(std::shared_ptr<Conn> c, const std::string &path_inside)
    {
        conn = std::move(c);
        inside = path_inside;
        std::lock_guard<std::mutex> lock(conn->m);
        smb2_stat_64 st{};
        const int rc = run(*conn, [&](smb2_context *ctx) {
            const int r = reopen(ctx);
            if (r < 0)
                return r;
            return smb2_fstat(ctx, fh, &st);
        });
        if (rc < 0)
        {
            if (fh && conn->ctx && generation == conn->generation)
                smb2_close(conn->ctx, fh);
            fh = nullptr;
            return false;
        }
        if (st.smb2_type == SMB2_TYPE_DIRECTORY)
        {
            smb2_close(conn->ctx, fh);
            fh = nullptr;
            return false;
        }
        size = st.smb2_size;
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
            const int rc = run(*conn, [&](smb2_context *ctx) {
                const int r = reopen(ctx);
                if (r < 0)
                    return r;
                got = smb2_pread(ctx, fh, out + done, want, offset + done);
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
    const int rc = run(*c, [&](smb2_context *ctx) {
        out.clear();
        smb2dir *d = smb2_opendir(ctx, inside.c_str());
        if (!d)
            return last_errno(ctx);
        while (smb2dirent *ent = smb2_readdir(ctx, d))
        {
            if (hidden(ent->name))
                continue;
            out.push_back({ent->name, ent->st.smb2_type == SMB2_TYPE_DIRECTORY});
        }
        smb2_closedir(ctx, d);
        return 0;
    });
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
    smb2_stat_64 st{};
    const int rc = run(*c, [&](smb2_context *ctx) {
        const int r = smb2_stat(ctx, inside.c_str(), &st);
        return r;
    });
    if (rc < 0)
        return false;
    if (is_dir)
        *is_dir = st.smb2_type == SMB2_TYPE_DIRECTORY;
    if (size)
        *size = st.smb2_type == SMB2_TYPE_DIRECTORY ? 0 : st.smb2_size;
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
    if (has("ACCESS_DENIED"))
        return Problem::Denied;
    if (has("NOT_FOUND") || has("NOT_A_DIRECTORY") || has("PATH_INVALID"))
        return Problem::NoFolder;
    if (has("resolve") || has("Invalid address"))
        return Problem::UnknownName;
    if (has("connect failed") || has("Timeout") || has("timed out") || has("POLLHUP") || has("socket"))
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
    std::string base = share.share.empty() ? server_of(share) : share.share;
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
        fields.resize(6);
        Share s{fields[0], fields[1], fields[2], fields[3], fields[4], fields[5]};
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
    std::fputs("# Porpoise's network shares: name, computer, share, folder, username, password\n", f);
    for (const Share &s : list)
        std::fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\n", escape(s.name).c_str(), escape(s.host).c_str(),
                     escape(s.share).c_str(), escape(s.folder).c_str(), escape(s.user).c_str(),
                     escape(s.password).c_str());
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
    smb2_context *ctx = open_context(share, share.share, error);
    if (!ctx)
    {
        note("test of " + server_of(share) + "/" + share.share + " failed: " + error);
        if (detail)
            *detail = error;
        return classify(error);
    }
    std::string folder = share.folder;
    while (!folder.empty() && (folder.front() == '/' || folder.front() == '\\'))
        folder.erase(folder.begin());
    while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\'))
        folder.pop_back();
    smb2_set_error(ctx, "");
    smb2dir *d = smb2_opendir(ctx, folder.c_str());
    Problem result = Problem::None;
    if (!d)
    {
        error = last_error(ctx);
        note("test of " + server_of(share) + "/" + share.share + ", its folder: " + error);
        if (detail)
            *detail = error;
        result = classify(error);
        if (result == Problem::Other || result == Problem::Unreachable)
            result = Problem::NoFolder;
    }
    else
        smb2_closedir(ctx, d);
    smb2_disconnect_share(ctx);
    smb2_destroy_context(ctx);
    return result;
}

Problem enumerate(const Share &server, std::vector<std::string> &out, std::string *detail)
{
    out.clear();
    if (server_of(server).empty())
        return Problem::NoComputer;
    std::string raw;
    smb2_context *ctx = open_context(server, "IPC$", raw);
    if (!ctx)
    {
        note("listing the shares of " + server_of(server) + " failed: " + raw);
        if (detail)
            *detail = raw;
        return classify(raw);
    }
    smb2_share_enum_reply *reply = smb2_share_enum_sync(ctx, SMB2_SHARE_INFO_1);
    if (!reply)
    {
        raw = last_error(ctx);
        note("listing the shares of " + server_of(server) + " failed: " + raw);
        if (detail)
            *detail = raw;
        smb2_disconnect_share(ctx);
        smb2_destroy_context(ctx);
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
    smb2_free_data(ctx, reply);
    smb2_disconnect_share(ctx);
    smb2_destroy_context(ctx);
    std::sort(out.begin(), out.end());
    return Problem::None;
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

/* ---- name lookups for libsmb2 on the PS5 ------------------------------------------------- */
#ifdef __PROSPERO__
/* The console's libc has no getaddrinfo in the modules Porpoise loads:
 * libsmb2 is built to call these instead. An address as it is, a name through
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
            rc = sceNetResolverStartNtoa(rid, node, &addr, 0, 0, 0);
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
    sin->sin_port = htons(std::uint16_t(service ? std::atoi(service) : 445));
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
}
#endif
