/* PS2 GS presentation of captured reference frames. GPL-3.0. */
#ifndef MIPSNES2_PS2_GS_PRESENT_H
#define MIPSNES2_PS2_GS_PRESENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *packet;
    int framebuffer_address;
    bool initialized;
} mipsnes2_ps2_gs_presenter;

/* The caller supplies zero-initialized state. The presenter owns one packet
 * and one 256x256 RGBA32 GS framebuffer until end().
 */
bool mipsnes2_ps2_gs_presenter_init(
    mipsnes2_ps2_gs_presenter *presenter);

/* Upload one 256x256 RGBA32 image from 16-byte-aligned EE memory. The packet
 * and framebuffer are reused; this function waits for DMA completion and
 * vertical sync before returning.
 */
bool mipsnes2_ps2_gs_presenter_present(
    mipsnes2_ps2_gs_presenter *presenter,
    const uint32_t *pixels, size_t pixel_count);

void mipsnes2_ps2_gs_presenter_end(
    mipsnes2_ps2_gs_presenter *presenter);

#endif
