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
#ifndef _WIN32
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
} // namespace porpoise
