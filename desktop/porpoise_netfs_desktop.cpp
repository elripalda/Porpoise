/* Porpoise on Windows: network shares are the PS5's (Windows opens a share's
 * folder by its own path, \\computer\share, like any other folder). Local
 * files only here, through the same calls.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_netfs.hpp"

#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace porpoise::netfs
{
const char *const kRoot = "/net";

bool is_net(std::string_view path)
{
    const std::size_t n = std::strlen(kRoot);
    return path.size() >= n && path.compare(0, n, kRoot) == 0 && (path.size() == n || path[n] == '/');
}

void set_shares(const std::vector<Share> &) {}
std::vector<Share> shares()
{
    return {};
}
std::string root_of(const Share &share)
{
    return std::string(kRoot) + "/" + share.name;
}
std::string unique_name(const Share &share, const std::vector<Share> &)
{
    return share.share;
}
bool load(const std::string &, std::vector<Share> &out)
{
    out.clear();
    return true;
}
bool save(const std::string &, const std::vector<Share> &)
{
    return false;
}

bool list(const std::string &dir, std::vector<Entry> &out)
{
    out.clear();
    if (is_net(dir))
        return false;
    DIR *d = opendir(dir.c_str());
    if (!d)
        return false;
    while (dirent *e = readdir(d))
    {
        if (!e->d_name[0] || e->d_name[0] == '.')
            continue;
        struct ::stat st;
        if (::stat((dir + "/" + e->d_name).c_str(), &st) != 0)
            continue;
        out.push_back({e->d_name, S_ISDIR(st.st_mode)});
    }
    closedir(d);
    return true;
}

bool stat(const std::string &path, bool *is_dir, std::uint64_t *size)
{
    struct ::stat st;
    if (is_net(path) || ::stat(path.c_str(), &st) != 0)
        return false;
    if (is_dir)
        *is_dir = S_ISDIR(st.st_mode);
    if (size)
        *size = S_ISDIR(st.st_mode) ? 0 : std::uint64_t(st.st_size);
    return true;
}

struct Reader::Impl
{
    std::FILE *f = nullptr;
    std::uint64_t size = 0;
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
        return false;
    impl_->f = std::fopen(path.c_str(), "rb");
    if (!impl_->f)
        return false;
    if (_fseeki64(impl_->f, 0, SEEK_END) == 0)
        impl_->size = std::uint64_t(_ftelli64(impl_->f));
    return true;
}
void Reader::close()
{
    if (impl_->f)
        std::fclose(impl_->f);
    impl_->f = nullptr;
    impl_->size = 0;
}
bool Reader::is_open() const
{
    return impl_->f != nullptr;
}
std::uint64_t Reader::size() const
{
    return impl_->size;
}
std::int64_t Reader::read_some(std::uint64_t offset, void *out, std::uint64_t n)
{
    if (!impl_->f || _fseeki64(impl_->f, std::int64_t(offset), SEEK_SET) != 0)
        return -1;
    const std::size_t got = std::fread(out, 1, std::size_t(n), impl_->f);
    return got == 0 && std::ferror(impl_->f) ? -1 : std::int64_t(got);
}
bool Reader::read(std::uint64_t offset, void *out, std::uint64_t n)
{
    if (offset > impl_->size || n > impl_->size - offset)
        return false;
    return read_some(offset, out, n) == std::int64_t(n);
}

Problem test(const Share &, std::string *detail)
{
    if (detail)
        *detail = "network shares are on the PS5";
    return Problem::Other;
}
Problem enumerate(const Share &, std::vector<std::string> &out, std::string *detail)
{
    out.clear();
    if (detail)
        *detail = "network shares are on the PS5";
    return Problem::Other;
}
std::vector<Found> discover()
{
    return {};
}
const retro_vfs_interface *vfs_interface()
{
    return nullptr;
}
void shutdown() {}
} // namespace porpoise::netfs
