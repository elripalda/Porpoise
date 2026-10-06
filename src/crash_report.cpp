/*
 * PS5 RetroArch - one line about a fatal signal, before the console's own report.
 *
 * Copyright (C) 2026 Mihawk
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The console's log says that a title died and where it faulted, but not what
 * called what: a call through a null pointer reports fault address 0 and nothing
 * else. This handler appends the instruction pointer, the stack pointer and the
 * return address on top of the stack to /app0/trace.txt, with the runtime address
 * of this handler, so build/title.map turns each into "object + offset" (subtract
 * the handler's slide) and a core's load base in the loader's "ready" line turns
 * an address inside a core into an offset in that core. It then flushes stdio, so
 * the buffered trace and RetroArch log lines that led up to the fault are kept,
 * and lets the signal take its default course. Nothing here runs unless a fatal
 * signal arrives.
 */

#include "porpoise_paths.hpp"
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include <fcntl.h>
#include <pthread.h>
#include <pthread_np.h>
#include <ucontext.h>
#include <unistd.h>

#include <ps5platform/context.h>
#include <ps5platform/heap.h>

extern "C" int sceKernelAvailableFlexibleMemorySize(std::size_t *size);

namespace
{
void report(int signal, siginfo_t *info, void *context_pointer)
{
    /* stderr is buffered (src/main.cpp): what it holds -- an assertion's
     * message above all -- is written out first, unless the crashing thread
     * holds the stream, where flushing would deadlock instead of reporting. */
    if (ftrylockfile(stderr) == 0)
    {
        std::fflush(stderr);
        funlockfile(stderr);
    }
    const ucontext_t *context = static_cast<const ucontext_t *>(context_pointer);
    /* The SDK fork's ucontext_t is the console's layout (ps5platform/context.h
     * refuses any other), so the registers are the header's own fields. */
    const std::uintptr_t rip = static_cast<std::uintptr_t>(context->uc_mcontext.mc_rip);
    const std::uintptr_t rsp = static_cast<std::uintptr_t>(context->uc_mcontext.mc_rsp);
    /* After a call through a bad pointer the return address is on top of the
     * stack; otherwise the word is only a hint, and is read only when rsp looks
     * like a stack address. */
    std::uintptr_t top = 0;
    if (rsp >= 4096 && (rsp & 7) == 0)
        top = *reinterpret_cast<const std::uintptr_t *>(rsp);
    std::size_t flexible = 0;
    sceKernelAvailableFlexibleMemorySize(&flexible);
    char line[256];
    const int length = std::snprintf(
        line, sizeof(line),
        "crash: signal %d fault=0x%016llx rip=0x%016llx rsp=0x%016llx [rsp]=0x%016llx "
        "handler=0x%016llx flexible_free=%zu\n",
        signal, static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(info->si_addr)),
        static_cast<unsigned long long>(rip), static_cast<unsigned long long>(rsp),
        static_cast<unsigned long long>(top),
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&report)), flexible);
    const int fd = open(PORPOISE_APP "/trace.txt", O_WRONLY | O_APPEND);
    if (fd >= 0 && length > 0)
        (void)!write(fd, line, static_cast<std::size_t>(length));
    /* Sixteen more stack words: with frame pointers omitted, the callers'
     * return addresses are among them, and the map tells code from data. */
    if (fd >= 0 && top != 0)
    {
        (void)!write(fd, "crash: stack", 12);
        for (int i = 1; i <= 16; ++i)
        {
            char word[24];
            const int n = std::snprintf(
                word, sizeof(word), " %llx",
                static_cast<unsigned long long>(reinterpret_cast<const std::uintptr_t *>(rsp)[i]));
            (void)!write(fd, word, static_cast<std::size_t>(n));
        }
        (void)!write(fd, "\n", 1);
    }
    /* The return addresses above the fault: every word from rsp to the top of
     * the thread's stack (32 KiB at most) that points into the title's code or
     * a core's. Without frame pointers this is how a crash ending in abort()
     * (an uncaught exception, which terminates before unwinding) shows where
     * it began; the loader's base turns a core's into offsets. */
    if (fd >= 0 && rsp >= 4096 && (rsp & 7) == 0)
    {
        pthread_attr_t attributes;
        void *stack = nullptr;
        std::size_t stack_size = 0;
        if (pthread_attr_init(&attributes) == 0)
        {
            if (pthread_attr_get_np(pthread_self(), &attributes) == 0)
                pthread_attr_getstack(&attributes, &stack, &stack_size);
            pthread_attr_destroy(&attributes);
        }
        const std::uintptr_t stack_top = reinterpret_cast<std::uintptr_t>(stack) + stack_size;
        if (stack && rsp < stack_top)
        {
            const std::uintptr_t end = stack_top - rsp > 32768 ? rsp + 32768 : stack_top;
            (void)!write(fd, "crash: frames", 13);
            int found = 0;
            for (std::uintptr_t at = rsp; at + 8 <= end && found < 96; at += 8)
            {
                const std::uintptr_t word = *reinterpret_cast<const std::uintptr_t *>(at);
                const bool title = word >= 0x400000 && word < 0x1100000;
                const bool core = word >= 0x400000000ull && word < 0x500000000ull;
                if (!title && !core)
                    continue;
                char text[24];
                const int n = std::snprintf(text, sizeof(text), " %llx",
                                            static_cast<unsigned long long>(word));
                (void)!write(fd, text, static_cast<std::size_t>(n));
                ++found;
            }
            (void)!write(fd, "\n", 1);
        }
    }
    /* Every general register, then 64 bytes around each one that points into
     * the title heap: a bad pointer read from memory shows beside the good
     * ones it was stored with. A dump that faults ends the report there, after
     * the lines above are written. */
    if (fd >= 0)
    {
        const mcontext_t &mc = context->uc_mcontext;
        const std::uintptr_t registers[] = {
            std::uintptr_t(mc.mc_rax), std::uintptr_t(mc.mc_rbx), std::uintptr_t(mc.mc_rcx),
            std::uintptr_t(mc.mc_rdx), std::uintptr_t(mc.mc_rsi), std::uintptr_t(mc.mc_rdi),
            std::uintptr_t(mc.mc_rbp), std::uintptr_t(mc.mc_r8),  std::uintptr_t(mc.mc_r9),
            std::uintptr_t(mc.mc_r10), std::uintptr_t(mc.mc_r11), std::uintptr_t(mc.mc_r12),
            std::uintptr_t(mc.mc_r13), std::uintptr_t(mc.mc_r14), std::uintptr_t(mc.mc_r15)};
        static const char *const names[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "r8",
                                            "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
        (void)!write(fd, "crash: registers", 16);
        for (std::size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); ++i)
        {
            char word[32];
            const int n = std::snprintf(word, sizeof(word), " %s=%llx", names[i],
                                        static_cast<unsigned long long>(registers[i]));
            (void)!write(fd, word, static_cast<std::size_t>(n));
        }
        (void)!write(fd, "\n", 1);
        for (std::size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); ++i)
        {
            const std::uintptr_t at = (registers[i] & ~std::uintptr_t(7)) - 32;
            if (!ps5_heap_owns(reinterpret_cast<const void *>(at)) ||
                !ps5_heap_owns(reinterpret_cast<const void *>(at + 63)))
                continue;
            char head[48];
            const int n = std::snprintf(head, sizeof(head), "crash: near %s %llx:", names[i],
                                        static_cast<unsigned long long>(at));
            (void)!write(fd, head, static_cast<std::size_t>(n));
            for (int w = 0; w < 8; ++w)
            {
                char word[24];
                const int m = std::snprintf(word, sizeof(word), " %llx",
                                            static_cast<unsigned long long>(
                                                reinterpret_cast<const std::uintptr_t *>(at)[w]));
                (void)!write(fd, word, static_cast<std::size_t>(m));
            }
            (void)!write(fd, "\n", 1);
        }
    }
    if (fd >= 0)
        close(fd);
    /* Not async-signal-safe, and worth the risk: the process is ending anyway,
     * and without it the lines just before the fault stay in their buffers. */
    std::fflush(nullptr);
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}
} // namespace

extern "C" void ps5_crash_report_install()
{
    struct sigaction action;
    std::memset(&action, 0, sizeof(action));
    action.sa_sigaction = report;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&action.sa_mask);
    for (const int signal : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
        sigaction(signal, &action, nullptr);
}
