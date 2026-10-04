/*
 * PS5 RetroArch - AGC driver link stub.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host-link declarations for system libSceAgcDriver; never packaged or executed.
 * Under tooling/ for the reason tooling/ps5-stubs/agc_link_stub.c gives.
 */

#include <stdint.h>

uint32_t sceAgcDriverGetWaitRenderingPacketSizeInDwords(void) { return 0; }

/* Added by this project: the submission half of the AGC flip path, mirroring
 * ../PS5_Vulkan's vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c. */
int32_t sceAgcDriverSubmitDcb(void *description) { (void)description; return -1; }

/* ../PS5_Vulkan's draw path waits for the previous frame's rendering to finish
 * before it writes the next command buffer; signature from that project's
 * agc_driver_canary_link_stub.c. Host-link only. */
uint32_t sceAgcDriverWaitUntilSafeForRendering(uint32_t **command, uint32_t packet_size,
                                               uint32_t reserved, uint32_t handle,
                                               int buffer_index)
{
    (void)command; (void)packet_size; (void)reserved; (void)handle; (void)buffer_index;
    return 0;
}

/* RADV (PS5_VULKAN_DRIVER=radv) sets the tessellation factor ring and the HS
 * off-chip parameters through AGC; signatures from ../PS5_Vulkan's
 * agc_driver_canary_link_stub.c and ps5platform/agc.h. Host-link only. */
int32_t sceAgcDriverSetTFRing(uintptr_t address, uint32_t size)
{
    (void)address; (void)size;
    return -1;
}

int32_t sceAgcDriverGetTFRing(uintptr_t *address, uint32_t *size)
{
    (void)address; (void)size;
    return -1;
}

int32_t sceAgcDriverSetHsOffchipParam(uint32_t granularity, uint32_t buffering)
{
    (void)granularity; (void)buffering;
    return -1;
}

int32_t sceAgcDriverGetHsOffchipParam(uint16_t *granularity, uint16_t *buffering)
{
    (void)granularity; (void)buffering;
    return -1;
}
