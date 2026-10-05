/* Porpoise - Zstandard decoding for reading .rvz disc images (porpoise_disc):
 * zstd's own educational decoder (third_party/zstd-educational), compiled
 * here with its errors turned from exit() into a failed call.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Per thread: the two banner workers can decode at once, and a failure in
 * one must jump back to its own call, never to the other thread's. */
static _Thread_local jmp_buf *porpoise_zstd_jump;
#define ZDEC_NO_MESSAGE
#define exit(code) longjmp(*porpoise_zstd_jump, 1)
#include "../third_party/zstd-educational/zstd_decompress.c"
#undef exit

/* Decompresses whole frames; (size_t)-1 on corrupt data or a short buffer.
 * Safe from several threads at once (each has its own jump point). */
size_t porpoise_zstd_decompress(void *dst, size_t dst_len, const void *src, size_t src_len)
{
    jmp_buf jump;
    porpoise_zstd_jump = &jump;
    if (setjmp(jump))
        return (size_t)-1;
    return ZSTD_decompress(dst, dst_len, src, src_len);
}
