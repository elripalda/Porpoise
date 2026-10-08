/*
 * PS5 RetroArch - an opt-in sampling profiler for the thread that runs frames.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why this exists. A frame that stalls for a tenth of a second or for twenty
 * seconds shows up in ../PS5_Vulkan's hitch report as "app" time: the driver
 * knows the frame spent it outside every driver call, not where. This samples
 * where: a thread interrupts the main thread (the one RetroArch runs the core
 * and the driver on) about every millisecond with a signal, whose handler
 * records the interrupted instruction pointer. A sample taken while no frame
 * has been presented for over 50 ms (src/present_clock.h) counts as a stall
 * sample; the flag file's "stall-ms N" line sets the threshold, since one just
 * above a game's own frame period is what separates its late frames from its
 * ordinary ones.
 *
 * Every ten seconds the window's stall samples are summarised as their most
 * frequent addresses, one line each, which tools resolve against the title's
 * map and the loaded core's base. Nothing is written per frame.
 *
 * Testing only: enabled by /app0/ps5-sampler.txt, which is never shipped.
 */

#include "porpoise_paths.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <poll.h>
#include <pthread.h>
#include <pthread_np.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/select.h>
#include <ucontext.h>
#include <unistd.h>

#include <ps5platform/context.h>
#include "present_clock.h"
#include "title_threads.hpp"

namespace
{
constexpr std::uint32_t kRingSize = 1u << 16;
/* The main thread is thread 0; core threads (src/core_threads_ps5.cpp) follow. */
constexpr unsigned kMaxThreads = 64;
/* Per sample: the flags word (bit 0: a stall), rip, the return address on top
 * of the stack and the frame-pointer chain's return addresses. */
constexpr unsigned kFrames = 10;
/* A sample counts toward a stall when no frame was presented for this long:
 * 50 ms, or the flag file's "stall-ms N" line. */
std::uint64_t g_stall_ns = 50ull * 1000 * 1000;
constexpr std::uint64_t kWindowNs = 10ull * 1000 * 1000 * 1000;
constexpr int kSampleSignal = SIGUSR2;
constexpr unsigned kTop = 60;

pthread_t g_threads[kMaxThreads];
std::atomic<unsigned> g_thread_count{0};
std::atomic<bool> g_running{false};
/* The core threads to sample, by start routine: the flag file's hex addresses,
 * one a line. Interrupting every thread every few milliseconds stopped PPSSPP's
 * emulation (a wait it does not retry), so only the named ones are. */
constexpr unsigned kMaxStarts = 16;
std::uint64_t g_starts[kMaxStarts];
unsigned g_start_count = 0;
/* The flag file's "all-threads" line: every core thread, up to kMaxThreads,
 * for a core whose threads start from code it generates (RPCS3's trampolines
 * move from run to run) and that tolerates the interruptions. */
bool g_all_threads = false;
/* The flag file's "leaves" line: for each thread busy in 100 samples or more,
 * the window's most frequent busy instruction addresses (16-byte buckets),
 * a flat profile the tools fold into functions; "leaves N" prints up to N of
 * them (60 by default), for a thread whose time spreads over many functions. */
bool g_leaves = false;
unsigned g_leaf_top = 60;
std::atomic<bool> g_stall{false};
std::atomic<std::uint32_t> g_head{0};
std::uint64_t g_ring[kRingSize][kFrames + 1];

/* The stack a walk may read: from rsp up to a bound no thread stack exceeds. */
/* Each sampled thread's stack top, 0 when unknown: a walk or a scan past it
 * faulted on a core thread near the top of its stack (2026-09-29, RPCS3 with
 * every thread sampled). */
std::uint64_t g_stack_top[kMaxThreads];

extern "C" int sceKernelGetCurrentCpu(void);

void on_sample(int, siginfo_t *, void *context_pointer)
{
    const auto *context = static_cast<const ucontext_t *>(context_pointer);
    const mcontext_t &registers = context->uc_mcontext;
    const std::uint32_t at = g_head.fetch_add(1, std::memory_order_relaxed);
    std::uint64_t *const sample = g_ring[at & (kRingSize - 1)];
    const std::uint64_t rsp = static_cast<std::uint64_t>(registers.mc_rsp);
    unsigned thread = 0;
    const pthread_t self = pthread_self();
    const unsigned count = g_thread_count.load(std::memory_order_acquire);
    for (unsigned index = 0; index < count; ++index)
        if (pthread_equal(g_threads[index], self))
            thread = index;
    /* Bits 16-23: the processor the thread was on (which threads share a core). */
    const int cpu = sceKernelGetCurrentCpu();
    sample[0] = (g_stall.load(std::memory_order_relaxed) ? 1u : 0u) | (std::uint64_t{thread} << 8) |
                (std::uint64_t(cpu >= 0 && cpu < 255 ? cpu : 255) << 16);
    /* Only a stack top the interrupted rsp lies under is trusted; without one
     * the sample keeps its leaf and [rsp] alone. */
    const std::uint64_t known = g_stack_top[thread];
    const std::uint64_t top = known > rsp && known - rsp < 8ull * 1024 * 1024 ? known : rsp;
    sample[1] = static_cast<std::uint64_t>(registers.mc_rip);
    sample[2] = (rsp & 7) == 0 && rsp != 0 ? *reinterpret_cast<const std::uint64_t *>(rsp) : 0;
    std::uint64_t frame = static_cast<std::uint64_t>(registers.mc_rbp);
    const std::uint64_t rip = static_cast<std::uint64_t>(registers.mc_rip);
    if (rip >= 0x800000000ull && rip < 0x800200000ull && (rsp & 7) == 0 && rsp != 0)
    {
        /* Inside libkernel, whose callers in the system libc keep no frame
         * chain: scan the stack instead, keeping the words that point into
         * the title (below 0x10000000), a loaded core (0x400000000 up) or the
         * system libraries, in order. A scan, so a stale word can appear; the
         * chain line says which kind it is (a leading 1). */
        const auto *const words = reinterpret_cast<const std::uint64_t *>(rsp);
        unsigned kept = 3;
        sample[3] = 1;
        for (unsigned word = 0; word < 256 && kept < kFrames && rsp + (word + 1) * 8 <= top; ++word)
        {
            const std::uint64_t value = words[word];
            const bool title = value >= 0x400000ull && value < 0x10000000ull;
            const bool core = value >= 0x400000000ull && value < 0x480000000ull;
            const bool system = value >= 0x800000000ull && value < 0x800200000ull;
            if (title || core || system)
                sample[++kept] = value;
        }
        for (unsigned depth = kept + 1; depth <= kFrames; ++depth)
            sample[depth] = 0;
        return;
    }
    for (unsigned depth = 3; depth <= kFrames; ++depth)
    {
        if ((frame & 7) != 0 || frame < rsp || frame + 16 > top)
        {
            sample[depth] = 0;
            continue;
        }
        const auto *const pair = reinterpret_cast<const std::uint64_t *>(frame);
        sample[depth] = pair[1];
        frame = pair[0] > frame ? pair[0] : 0;
    }
}

/* libkernel and libc, where a blocked thread sits; the first frame outside
 * them is the one that asked to wait. */
bool system_address(std::uint64_t address)
{
    return address >= 0x800000000ull && address < 0x800200000ull;
}

struct Count
{
    std::uint64_t rip;
    std::uint64_t leaf;
    std::uint32_t samples;
    std::uint32_t thread;
    std::uint32_t example;
};

Count g_counts[4096];

/* Folds one window's stall samples into address counts and prints the most
 * frequent. Linear probing in a fixed table: the sampler allocates nothing. */
constexpr int kHotMax = 48;
pthread_mutex_t g_hot_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long long g_hot_rips[kHotMax];
unsigned g_hot_counts[kHotMax];
int g_hot_n = 0;

void report(std::uint32_t from, std::uint32_t to, std::uint32_t stall_samples)
{
    std::memset(g_counts, 0, sizeof(g_counts));
    constexpr std::uint32_t slots = sizeof(g_counts) / sizeof(g_counts[0]);
    for (std::uint32_t at = from; at != to; ++at)
    {
        const std::uint64_t *const sample = g_ring[at & (kRingSize - 1)];
        if ((sample[0] & 1u) == 0)
            continue;
        const std::uint64_t leaf = sample[1] & ~std::uint64_t{0xff};
        std::uint64_t rip = sample[1];
        for (unsigned depth = 1; depth <= kFrames && system_address(rip); ++depth)
            if (sample[depth] != 0 && !system_address(sample[depth]))
                rip = sample[depth];
        const auto thread = static_cast<std::uint32_t>((sample[0] >> 8) & 0xff);
        std::uint32_t slot =
            static_cast<std::uint32_t>(((rip >> 4) ^ (leaf >> 8) ^ (std::uint64_t{thread} << 40)) *
                                       2654435761u) %
            slots;
        for (std::uint32_t probe = 0; probe < slots; ++probe, slot = (slot + 1) % slots)
        {
            if (g_counts[slot].samples == 0 ||
                (g_counts[slot].rip == rip && g_counts[slot].leaf == leaf &&
                 g_counts[slot].thread == thread))
            {
                g_counts[slot].rip = rip;
                g_counts[slot].leaf = leaf;
                g_counts[slot].example = at;
                g_counts[slot].thread = thread;
                ++g_counts[slot].samples;
                break;
            }
        }
    }
    std::fprintf(stderr, "sampler: window samples=%u stall=%u\n", to - from, stall_samples);
    /* Per thread: its samples, those outside libkernel and libc (busy, not
     * blocked), and its most frequent busy group with one chain. A busy thread
     * spreads over many addresses and never reaches the most frequent groups,
     * which blocked threads fill (RPCS3's RSX thread, 2026-09-29). */
    static std::uint32_t totals[kMaxThreads], busy[kMaxThreads];
    std::memset(totals, 0, sizeof(totals));
    std::memset(busy, 0, sizeof(busy));
    for (std::uint32_t at = from; at != to; ++at)
    {
        const std::uint64_t *const sample = g_ring[at & (kRingSize - 1)];
        if ((sample[0] & 1u) == 0)
            continue;
        const auto thread = static_cast<std::uint32_t>((sample[0] >> 8) & 0xff);
        if (thread >= kMaxThreads)
            continue;
        ++totals[thread];
        busy[thread] += system_address(sample[1]) ? 0u : 1u;
    }
    for (std::uint32_t thread = 0; thread < kMaxThreads; ++thread)
    {
        if (busy[thread] * 10u < totals[thread] || busy[thread] == 0)
            continue;
        std::fprintf(stderr, "sampler: thread=%u samples=%u busy=%u\n", thread, totals[thread],
                     busy[thread]);
        {
            /* Which processors its busy samples ran on. */
            std::uint32_t cpus[256] = {};
            for (std::uint32_t at = from; at != to; ++at)
            {
                const std::uint64_t *const sample = g_ring[at & (kRingSize - 1)];
                if ((sample[0] & 1u) != 0 && ((sample[0] >> 8) & 0xff) == thread && !system_address(sample[1]))
                    ++cpus[(sample[0] >> 16) & 0xff];
            }
            char line[512];
            int used = std::snprintf(line, sizeof line, "sampler:   cpus thread=%u", thread);
            for (unsigned cpu = 0; cpu < 256 && used > 0 && used < 480; ++cpu)
                if (cpus[cpu] != 0)
                    used += std::snprintf(line + used, sizeof line - used, " %u:%u", cpu, cpus[cpu]);
            std::fprintf(stderr, "%s\n", line);
        }
        /* Its most frequent groups, blocked ones too (where a busy thread
         * waits), each with one chain. */
        for (unsigned rank = 0; rank < 6; ++rank)
        {
            std::uint32_t best = slots;
            for (std::uint32_t slot = 0; slot < slots; ++slot)
                if (g_counts[slot].samples != 0 && g_counts[slot].thread == thread &&
                    (best == slots || g_counts[slot].samples > g_counts[best].samples))
                    best = slot;
            if (best == slots)
                break;
            const std::uint64_t *const chain = g_ring[g_counts[best].example & (kRingSize - 1)];
            char line[512];
            int used = std::snprintf(line, sizeof(line), "sampler:   group thread=%u n=%u chain",
                                     thread, g_counts[best].samples);
            for (unsigned depth = 1; depth <= kFrames && used > 0 && used < 480; ++depth)
                used += std::snprintf(line + used, sizeof(line) - used, " %llx",
                                      static_cast<unsigned long long>(chain[depth]));
            std::fprintf(stderr, "%s\n", line);
            g_counts[best].samples = 0;
        }
    }
    for (unsigned rank = 0; rank < kTop; ++rank)
    {
        std::uint32_t best = slots;
        for (std::uint32_t slot = 0; slot < slots; ++slot)
            if (g_counts[slot].samples != 0 &&
                (best == slots || g_counts[slot].samples > g_counts[best].samples))
                best = slot;
        if (best == slots)
            break;
        std::fprintf(stderr, "sampler: stall thread=%u caller=0x%016llx leaf=0x%016llx n=%u\n",
                     g_counts[best].thread, static_cast<unsigned long long>(g_counts[best].rip),
                     static_cast<unsigned long long>(g_counts[best].leaf), g_counts[best].samples);
        /* The first ten groups carry one whole sampled chain. */
        if (rank < 10)
        {
            const std::uint64_t *const chain = g_ring[g_counts[best].example & (kRingSize - 1)];
            char line[512];
            int used = std::snprintf(line, sizeof(line), "sampler:   chain");
            for (unsigned depth = 1; depth <= kFrames && used > 0 && used < 480; ++depth)
                used += std::snprintf(line + used, sizeof(line) - used, " %llx",
                                      static_cast<unsigned long long>(chain[depth]));
            std::fprintf(stderr, "%s\n", line);
        }
        g_counts[best].samples = 0;
    }
    if (!g_leaves)
        return;
    /* The busiest thread's hottest places are kept for ps5_sampler_hot (the
     * emulated CPU's, which Porpoise has the core tell as the game's code). */
    std::uint32_t busiest = kMaxThreads;
    for (std::uint32_t thread = 0; thread < kMaxThreads; ++thread)
        if (busy[thread] >= 100 && (busiest == kMaxThreads || busy[thread] > busy[busiest]))
            busiest = thread;
    unsigned long long hot_rips[kHotMax];
    unsigned hot_counts[kHotMax];
    int hot_n = 0;
    for (std::uint32_t thread = 0; thread < kMaxThreads; ++thread)
    {
        if (busy[thread] < 100)
            continue;
        std::memset(g_counts, 0, sizeof(g_counts));
        for (std::uint32_t at = from; at != to; ++at)
        {
            const std::uint64_t *const sample = g_ring[at & (kRingSize - 1)];
            if ((sample[0] & 1u) == 0 || ((sample[0] >> 8) & 0xff) != thread ||
                system_address(sample[1]))
                continue;
            const std::uint64_t bucket = sample[1] >> 4;
            std::uint32_t slot = static_cast<std::uint32_t>(bucket * 2654435761u) % slots;
            for (std::uint32_t probe = 0; probe < slots; ++probe, slot = (slot + 1) % slots)
            {
                if (g_counts[slot].samples == 0 || g_counts[slot].rip == bucket)
                {
                    g_counts[slot].rip = bucket;
                    ++g_counts[slot].samples;
                    break;
                }
            }
        }
        for (unsigned rank = 0; rank < g_leaf_top; ++rank)
        {
            std::uint32_t best = slots;
            for (std::uint32_t slot = 0; slot < slots; ++slot)
                if (g_counts[slot].samples != 0 &&
                    (best == slots || g_counts[slot].samples > g_counts[best].samples))
                    best = slot;
            if (best == slots)
                break;
            std::fprintf(stderr, "sampler: leaf thread=%u rip=0x%llx n=%u\n", thread,
                         static_cast<unsigned long long>(g_counts[best].rip << 4),
                         g_counts[best].samples);
            if (thread == busiest && hot_n < kHotMax)
            {
                hot_rips[hot_n] = static_cast<unsigned long long>(g_counts[best].rip << 4);
                hot_counts[hot_n] = g_counts[best].samples;
                ++hot_n;
            }
            g_counts[best].samples = 0;
        }
    }
    if (hot_n > 0)
    {
        pthread_mutex_lock(&g_hot_mutex);
        std::memcpy(g_hot_rips, hot_rips, sizeof hot_rips);
        std::memcpy(g_hot_counts, hot_counts, sizeof hot_counts);
        g_hot_n = hot_n;
        pthread_mutex_unlock(&g_hot_mutex);
    }
}

void *sampler(void *)
{
    const timespec interval = {0, 2 * 1000 * 1000};
    std::uint64_t window_start = ps5_present_clock_now_ns();
    std::uint32_t window_from = g_head.load();
    std::uint32_t stall_samples = 0;
    for (;;)
    {
        nanosleep(&interval, nullptr);
        const std::uint64_t now = ps5_present_clock_now_ns();
        const std::uint64_t last = ps5_present_clock_last_ns();
        const bool stall = last != 0 && now > last && now - last > g_stall_ns;
        g_stall.store(stall, std::memory_order_relaxed);
        stall_samples += stall ? 1u : 0u;
        const unsigned count = g_thread_count.load(std::memory_order_acquire);
        for (unsigned index = 0; index < count; ++index)
            pthread_kill(g_threads[index], kSampleSignal);
        if (now - window_start >= kWindowNs)
        {
            const std::uint32_t to = g_head.load();
            /* A window longer than the ring keeps only its last samples. */
            const std::uint32_t from = to - window_from > kRingSize ? to - kRingSize : window_from;
            if (stall_samples != 0)
                report(from, to, stall_samples);
            window_start = now;
            window_from = to;
            stall_samples = 0;
        }
    }
    return nullptr;
}
} // namespace

/* The rest of the waiting functions by their exported names, declared under
 * assembler names so no header's declaration of them is contradicted. */
extern "C" void sampler_export_pthread_yield() __asm__("pthread_yield");
extern "C" void sampler_export_sceKernelUsleep() __asm__("sceKernelUsleep");
extern "C" void sampler_export_sceKernelNanosleep() __asm__("sceKernelNanosleep");
extern "C" void
sampler_export_pthread_cond_reltimedwait_np() __asm__("pthread_cond_reltimedwait_np");
extern "C" void sampler_export_pthread_mutex_timedlock() __asm__("pthread_mutex_timedlock");
extern "C" void sampler_export_pthread_mutex_trylock() __asm__("pthread_mutex_trylock");
extern "C" void sampler_export_pthread_spin_lock() __asm__("pthread_spin_lock");
extern "C" void sampler_export_umtx_op() __asm__("_umtx_op");
extern "C" void sampler_export_sceKernelWaitEventFlag() __asm__("sceKernelWaitEventFlag");
extern "C" void sampler_export_sceKernelWaitSema() __asm__("sceKernelWaitSema");
extern "C" void sampler_export_pthread_barrier_wait() __asm__("pthread_barrier_wait");
extern "C" void sampler_export_sceKernelWaitEqueue() __asm__("sceKernelWaitEqueue");
extern "C" void sampler_export_kevent() __asm__("kevent");
extern "C" void sampler_export_sem_post() __asm__("sem_post");
extern "C" void sampler_export_sceKernelGetProcessTime() __asm__("sceKernelGetProcessTime");

/* Where the console's waiting functions are, once: a blocked thread's samples
 * sit inside one of them, and its caller may leave no frame chain (JIT code),
 * so the nearest export at or below a sampled address names the wait. Only
 * functions the title imports anyway, by their exported names. */
static void report_exports()
{
    const struct
    {
        const char *name;
        const void *address;
    } exports[] = {
        {"nanosleep", reinterpret_cast<const void *>(&nanosleep)},
        {"usleep", reinterpret_cast<const void *>(&usleep)},
        {"sched_yield", reinterpret_cast<const void *>(&sched_yield)},
        {"pthread_cond_wait", reinterpret_cast<const void *>(&pthread_cond_wait)},
        {"pthread_cond_timedwait", reinterpret_cast<const void *>(&pthread_cond_timedwait)},
        {"pthread_cond_signal", reinterpret_cast<const void *>(&pthread_cond_signal)},
        {"pthread_cond_broadcast", reinterpret_cast<const void *>(&pthread_cond_broadcast)},
        {"pthread_mutex_lock", reinterpret_cast<const void *>(&pthread_mutex_lock)},
        {"pthread_mutex_unlock", reinterpret_cast<const void *>(&pthread_mutex_unlock)},
        {"pthread_join", reinterpret_cast<const void *>(&pthread_join)},
        {"pthread_rwlock_rdlock", reinterpret_cast<const void *>(&pthread_rwlock_rdlock)},
        {"pthread_rwlock_wrlock", reinterpret_cast<const void *>(&pthread_rwlock_wrlock)},
        {"sem_wait", reinterpret_cast<const void *>(&sem_wait)},
        {"sem_timedwait", reinterpret_cast<const void *>(&sem_timedwait)},
        {"clock_gettime", reinterpret_cast<const void *>(&clock_gettime)},
        {"poll", reinterpret_cast<const void *>(&poll)},
        {"select", reinterpret_cast<const void *>(&select)},
        {"read", reinterpret_cast<const void *>(&read)},
        {"write", reinterpret_cast<const void *>(&write)},
        {"pthread_yield", reinterpret_cast<const void *>(&sampler_export_pthread_yield)},
        {"sceKernelUsleep", reinterpret_cast<const void *>(&sampler_export_sceKernelUsleep)},
        {"sceKernelNanosleep", reinterpret_cast<const void *>(&sampler_export_sceKernelNanosleep)},
        {"pthread_cond_reltimedwait_np",
         reinterpret_cast<const void *>(&sampler_export_pthread_cond_reltimedwait_np)},
        {"pthread_mutex_timedlock",
         reinterpret_cast<const void *>(&sampler_export_pthread_mutex_timedlock)},
        {"pthread_mutex_trylock",
         reinterpret_cast<const void *>(&sampler_export_pthread_mutex_trylock)},
        {"pthread_spin_lock", reinterpret_cast<const void *>(&sampler_export_pthread_spin_lock)},
        {"_umtx_op", reinterpret_cast<const void *>(&sampler_export_umtx_op)},
        {"sceKernelWaitEventFlag",
         reinterpret_cast<const void *>(&sampler_export_sceKernelWaitEventFlag)},
        {"sceKernelWaitSema", reinterpret_cast<const void *>(&sampler_export_sceKernelWaitSema)},
        {"pthread_barrier_wait",
         reinterpret_cast<const void *>(&sampler_export_pthread_barrier_wait)},
        {"sceKernelWaitEqueue",
         reinterpret_cast<const void *>(&sampler_export_sceKernelWaitEqueue)},
        {"kevent", reinterpret_cast<const void *>(&sampler_export_kevent)},
        {"sem_post", reinterpret_cast<const void *>(&sampler_export_sem_post)},
        {"sceKernelGetProcessTime",
         reinterpret_cast<const void *>(&sampler_export_sceKernelGetProcessTime)},
    };
    for (const auto &entry : exports)
        std::fprintf(stderr, "sampler: export %s=%p\n", entry.name, entry.address);
}

/* The top of a thread's stack, or 0. */
static std::uint64_t stack_top(pthread_t thread)
{
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0)
        return 0;
    void *base = nullptr;
    size_t size = 0;
    std::uint64_t top = 0;
    if (pthread_attr_get_np(thread, &attributes) == 0 &&
        pthread_attr_getstack(&attributes, &base, &size) == 0 && base != nullptr)
        top = reinterpret_cast<std::uint64_t>(base) + size;
    pthread_attr_destroy(&attributes);
    return top;
}

/* Starts the sampler on the calling thread when /app0/ps5-sampler.txt exists. */
extern "C" void ps5_sampler_start()
{
    std::FILE *const flag = std::fopen(PORPOISE_APP "/ps5-sampler.txt", "rb");
    if (flag == nullptr)
        return;
    char line[64];
    while (g_start_count < kMaxStarts && std::fgets(line, sizeof(line), flag) != nullptr)
    {
        if (std::strncmp(line, "all-threads", 11) == 0)
        {
            g_all_threads = true;
            continue;
        }
        if (std::strncmp(line, "leaves", 6) == 0)
        {
            g_leaves = true;
            const unsigned long top = std::strtoul(line + 6, nullptr, 10);
            if (top != 0)
                g_leaf_top = top < 4096 ? static_cast<unsigned>(top) : 4096u;
            continue;
        }
        if (std::strncmp(line, "stall-ms", 8) == 0)
        {
            /* 0 counts every sample, for a core that stalls while RetroArch
             * keeps presenting. */
            g_stall_ns = std::strtoull(line + 8, nullptr, 10) * 1000ull * 1000ull;
            continue;
        }
        char *end = nullptr;
        const unsigned long long start = std::strtoull(line, &end, 16);
        if (end != line && start != 0)
            g_starts[g_start_count++] = start;
    }
    std::fclose(flag);
    report_exports();
    g_threads[0] = pthread_self();
    g_stack_top[0] = stack_top(g_threads[0]);
    g_thread_count.store(1, std::memory_order_release);
    g_running.store(true, std::memory_order_release);
    struct sigaction action;
    std::memset(&action, 0, sizeof(action));
    action.sa_sigaction = on_sample;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    if (sigaction(kSampleSignal, &action, nullptr) != 0)
    {
        std::fputs("sampler: the sample signal could not be installed\n", stderr);
        return;
    }
    pthread_t thread;
    if (create_title_thread(&thread, sampler, nullptr) == 0)
    {
        pthread_detach(thread);
        std::fputs("sampler: sampling the main thread and core threads every 2 ms\n", stderr);
    }
}

/* A core thread to sample too (src/core_threads_ps5.cpp); its number is its
 * order of creation after the main thread's 0. */
extern "C" void ps5_sampler_add_thread(pthread_t thread, const void *start)
{
    if (!g_running.load(std::memory_order_acquire))
        return;
    bool named = g_all_threads;
    for (unsigned at = 0; at < g_start_count; ++at)
        named = named || g_starts[at] == reinterpret_cast<std::uintptr_t>(start);
    if (!named)
    {
        /* Where each core thread starts, once, so a run can name the ones to
         * sample in the next: the addresses depend on the core build and where
         * the loader put it. */
        std::fprintf(stderr, "sampler: core thread not sampled, start=%p\n", start);
        return;
    }
    const unsigned index = g_thread_count.load(std::memory_order_relaxed);
    if (index >= kMaxThreads)
        return;
    g_stack_top[index] = stack_top(thread);
    g_threads[index] = thread;
    g_thread_count.store(index + 1, std::memory_order_release);
    std::fprintf(stderr, "sampler: thread %u added, start=%p stack top=0x%llx\n", index, start,
                 static_cast<unsigned long long>(g_stack_top[index]));
}

/* The busiest thread's hottest places in the last report window (0 when the
 * sampler isn't running): for the core to tell as the game's own code. */
extern "C" int ps5_sampler_hot(unsigned long long *rips, unsigned *counts, int max)
{
    pthread_mutex_lock(&g_hot_mutex);
    const int n = g_hot_n < max ? g_hot_n : max;
    for (int i = 0; i < n; ++i)
    {
        rips[i] = g_hot_rips[i];
        counts[i] = g_hot_counts[i];
    }
    pthread_mutex_unlock(&g_hot_mutex);
    return n;
}
