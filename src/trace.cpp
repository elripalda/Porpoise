/*
 * PS5 RetroArch - the startup trace. See src/trace.hpp for why it exists.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "trace.hpp"
#include "porpoise_paths.hpp"

#include <atomic>
#include <cstdio>
#include <unistd.h>

namespace ps5::debug
{
namespace
{
/* Inside the title's own folder, which the console mounts at /app0 and which the
 * trace files written by an earlier build proved is writable. */
constexpr const char *trace_path = PORPOISE_APP "/trace.txt";
/* The first lines go to the disk itself, not only the console's cache: a
 * console that freezes as Porpoise starts has to be unplugged, and a file
 * still in the cache is lost with the power (2.1.1's first test left no
 * trace at all). Past them, the cache is enough. */
constexpr int kSyncedLines = 120;
std::atomic<int> g_lines{0};

void write(const char *line) noexcept
{
    std::FILE *file = std::fopen(trace_path, "a");
    if (file == nullptr)
        return;
    std::fputs(line, file);
    std::fputc('\n', file);
    std::fflush(file);
    if (g_lines.fetch_add(1, std::memory_order_relaxed) < kSyncedLines)
        fsync(fileno(file));
    std::fclose(file);
}
} // namespace

void mark(const char *step) noexcept
{
    write(step);
}

void mark_value(const char *step, long long value) noexcept
{
    char line[192];
    std::snprintf(line, sizeof(line), "%s = %lld", step, value);
    write(line);
}

void mark_init(const char *step, bool have_video, int width, int height) noexcept
{
    char line[192];
    std::snprintf(line, sizeof(line), "%s (video=%s width=%d height=%d)", step,
                  have_video ? "present" : "NULL", width, height);
    write(line);
}
} // namespace ps5::debug

/* The C door src/input_ps5.cpp writes through; see the note in trace.hpp. */
extern "C" void ps5_input_trace(const char *line) noexcept
{
    ps5::debug::mark(line);
}
