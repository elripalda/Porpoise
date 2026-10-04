/*
 * PS5 RetroArch - an overflow heap in direct memory, for when the title's own
 * allocators refuse.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Two budgets are small on this console. SceLibcInternal's private heap, which
 * serves every allocation under 1 MiB, runs out long before the process does:
 * FBNeo's catalogue exhausted it (docs/PHASE_LOG.md), and PPSSPP's symbol map
 * made it refuse a 168-byte operator new with 178 MB of flexible memory still
 * free. And flexible memory itself, which anonymous mmap draws on for the large
 * allocations, is about 450 MB for the whole title. Direct memory is a separate
 * pool of several GiB (sceKernelGetDirectMemorySize).
 *
 * So src/memory_ps5.cpp keeps its routes and falls back to this heap when one
 * of them refuses: the payload SDK platform layer's title heap
 * (ps5platform/heap.h), dlmalloc in direct memory mapped CPU read-write in a
 * reserved range outside the GPU's window, as mspaces, one for each allocating
 * thread up to eight. With one locked mspace, as this heap had, threads
 * compiling shaders at once wait on each other's allocations: on the host, one
 * lock over every allocation doubled RADV's compile time for PPSSPP's
 * pipelines at eight threads (2026-09-28). A pointer is this heap's when the
 * platform heap owns it, which is how free and realloc route it back.
 */

#include <ps5platform/heap.h>

#include <stddef.h>

int ps5_overflow_owns(const void *pointer)
{
    return ps5_heap_owns(pointer);
}

void *ps5_overflow_malloc(size_t size)
{
    return ps5_heap_malloc(size ? size : 1);
}

void *ps5_overflow_calloc(size_t count, size_t size)
{
    return ps5_heap_calloc(count ? count : 1, size ? size : 1);
}

void *ps5_overflow_memalign(size_t alignment, size_t size)
{
    return ps5_heap_memalign(alignment, size ? size : 1);
}

void *ps5_overflow_realloc(void *pointer, size_t size)
{
    return ps5_heap_realloc(pointer, size);
}

size_t ps5_overflow_usable_size(const void *pointer)
{
    return ps5_heap_usable_size(pointer);
}

void ps5_overflow_free(void *pointer)
{
    ps5_heap_free(pointer);
}
