/* Porpoise - files the player keeps, written whole or not at all.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * A file rewritten in place ("w") is empty from the moment it is opened until
 * it is closed: a console that freezes then (and has to be unplugged), or a
 * full drive, leaves it empty or cut short. So the new contents go to
 * <path>.part, reach the disk, and only then take the old file's place, in
 * one step. Open with open_atomic, write as usual, then finish_atomic. */
#pragma once

#include <cstdio>
#include <string>
#include <vector>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace porpoise
{
inline std::FILE *open_atomic(const std::string &path)
{
    return std::fopen((path + ".part").c_str(), "w");
}

/* Closes f (from open_atomic(path)) and puts it in place of path. False, with
 * the old file left as it was, when any write failed. */
inline bool finish_atomic(std::FILE *f, const std::string &path)
{
    const std::string part = path + ".part";
    bool ok = std::fflush(f) == 0 && !std::ferror(f);
#ifndef _WIN32
    ok = ok && fsync(fileno(f)) == 0;
#endif
    ok = std::fclose(f) == 0 && ok;
    if (ok && std::rename(part.c_str(), path.c_str()) == 0)
        return true;
    std::remove(part.c_str());
    return false;
}

/* A big file (a save state) written whole: through <path>.part as above, in
 * large pieces. stdio hands the console's file system one small block per
 * call, which made an 83 MB state take five seconds. */
inline bool write_whole(const std::string &path, const void *data, std::size_t size)
{
    const std::string part = path + ".part";
#ifndef _WIN32
    const int fd = ::open(part.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
        return false;
    const auto *p = static_cast<const unsigned char *>(data);
    std::size_t done = 0;
    bool ok = true;
    while (ok && done < size)
    {
        const std::size_t piece = size - done < (16u << 20) ? size - done : (16u << 20);
        const ssize_t n = ::write(fd, p + done, piece);
        if (n <= 0)
            ok = false;
        else
            done += std::size_t(n);
    }
    ok = ok && fsync(fd) == 0;
    ok = ::close(fd) == 0 && ok;
#else
    std::FILE *f = std::fopen(part.c_str(), "wb");
    if (!f)
        return false;
    bool ok = std::fwrite(data, 1, size, f) == size && std::fflush(f) == 0;
    ok = std::fclose(f) == 0 && ok;
#endif
    if (ok && std::rename(part.c_str(), path.c_str()) == 0)
        return true;
    std::remove(part.c_str());
    return false;
}

/* A big file read whole, in large pieces (see write_whole). */
inline bool read_whole(const std::string &path, std::vector<unsigned char> &out)
{
#ifndef _WIN32
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return false;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0)
    {
        ::close(fd);
        return false;
    }
    out.resize(std::size_t(st.st_size));
    std::size_t done = 0;
    while (done < out.size())
    {
        const std::size_t piece = out.size() - done < (16u << 20) ? out.size() - done : (16u << 20);
        const ssize_t n = ::read(fd, out.data() + done, piece);
        if (n <= 0)
            break;
        done += std::size_t(n);
    }
    ::close(fd);
    return done == out.size();
#else
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size <= 0)
    {
        std::fclose(f);
        return false;
    }
    out.resize(std::size_t(size));
    const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
#endif
}
} // namespace porpoise
