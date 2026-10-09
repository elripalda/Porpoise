/* Porpoise - games on a network share: SMB (a Windows, macOS or Linux shared
 * folder, or a NAS), read with libsmb2, or NFS (a NAS or a Linux machine),
 * read with libnfs.
 *
 * A share the player adds appears as /net/<name>/... : the library searches
 * it like a folder, the launcher reads disc headers and banners from it, and
 * Dolphin reads the game through the libretro VFS, which Porpoise answers for
 * those paths only (every other file the core opens is its own, as before).
 * Read-only: nothing is ever written to the share.
 *
 * The functions below take any path, local or /net/, so the launcher's code
 * reads both the same way.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct retro_vfs_interface;

namespace porpoise::netfs
{
/* One shared folder the player added. */
struct Share
{
    std::string name;     /* the name Porpoise shows; the path is /net/<name> */
    std::string host;     /* the computer: an address (192.168.1.20) or a name */
    std::string share;    /* the shared folder on it ("Games"), or the NFS export ("/volume1/games") */
    std::string folder;   /* a folder inside the share to start from, or empty */
    std::string user;     /* empty: as a guest (SMB) */
    std::string password; /* typed by the player on the console (SMB) */
    std::string protocol; /* "nfs" for NFS (share is then the export path); else SMB */
};

/* "/net" */
extern const char *const kRoot;

/* Whether a path is on a network share (/net/...). */
bool is_net(std::string_view path);

/* The shares to answer for; connections to shares that changed or went
 * away are closed. Safe from any thread. */
void set_shares(const std::vector<Share> &shares);
std::vector<Share> shares();
/* "/net/<name>" for a share. */
std::string root_of(const Share &share);
/* A name for a new share from its host and share, unique among existing. */
std::string unique_name(const Share &share, const std::vector<Share> &existing);

/* The saved list: one share a line. false when the file can't be read
 * (no file yet: true and an empty list). */
bool load(const std::string &file, std::vector<Share> &out);
bool save(const std::string &file, const std::vector<Share> &shares);

/* ---- reading, local or network ---------------------------------------------------------- */

struct Entry
{
    std::string name;
    bool is_dir = false;
};

/* A folder's entries (".", ".." and hidden names left out). */
bool list(const std::string &dir, std::vector<Entry> &out);

/* Whether a path exists; is_dir and size when it does. */
bool stat(const std::string &path, bool *is_dir = nullptr, std::uint64_t *size = nullptr);

/* A file opened for reading at any offset. */
class Reader
{
public:
    Reader();
    ~Reader();
    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;

    bool open(const std::string &path);
    void close();
    bool is_open() const;
    std::uint64_t size() const;
    /* Exactly n bytes at offset, or false. */
    bool read(std::uint64_t offset, void *out, std::uint64_t n);
    /* Up to n bytes at offset: how many, or -1 on an error. */
    std::int64_t read_some(std::uint64_t offset, void *out, std::uint64_t n);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/* ---- the share itself ------------------------------------------------------------------- */

/* What went wrong reaching a share, for the player (the UI words it). */
enum class Problem
{
    None,
    NoComputer,  /* no address or name entered */
    NoShare,     /* no shared folder entered */
    UnknownName, /* no computer by that name */
    Unreachable, /* nothing answered: wrong address, the computer off, sharing off */
    SignIn,      /* the username or password wasn't accepted */
    NoSuchShare, /* no shared folder by that name */
    Denied,      /* not allowed in */
    NoFolder,    /* the folder inside isn't there */
    NoExports,   /* an NFS server that won't list its exports */
    Other,       /* detail says what */
};

/* Connects to the share and reads its folder. detail: the server's or the
 * library's own words, for the log and for Other. */
Problem test(const Share &share, std::string *detail = nullptr);

/* The shared folders on a computer (for picking one); a Problem when the
 * list can't be had. */
Problem enumerate(const Share &server, std::vector<std::string> &out, std::string *detail = nullptr);

/* A computer on the console's own network that shares files. */
struct Found
{
    std::string address; /* "192.168.1.20" */
    std::string name;    /* its Windows (NetBIOS) name when it says, else empty */
    bool smb = false;    /* answers on SMB's port (445) */
    bool nfs = false;    /* answers on NFS's port (2049) */
};

/* The computers on the console's network (the 254 addresses around its own)
 * that answer on SMB's or NFS's port, with their names where they give them.
 * Takes a couple of seconds; any thread. */
std::vector<Found> discover();

/* The libretro VFS (v4) for the core: /net paths only. */
const retro_vfs_interface *vfs_interface();

/* Closes every connection (when Porpoise exits). */
void shutdown();
} // namespace porpoise::netfs
