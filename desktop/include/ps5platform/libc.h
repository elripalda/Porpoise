/* Porpoise on a desktop: the PS5 SDK's thread-affinity calls, which mean
 * nothing here (Windows schedules the threads itself). */
#pragma once
#include <pthread.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline int ps5_pthread_setaffinity_np(pthread_t thread, size_t size, const void *mask)
{
    (void)thread, (void)size, (void)mask;
    return -1;
}
static inline int ps5_pthread_getaffinity_np(pthread_t thread, size_t size, void *mask)
{
    (void)thread, (void)size, (void)mask;
    return -1;
}
#ifdef __cplusplus
}
#endif
