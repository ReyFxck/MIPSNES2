/* PS2 GS presentation of one captured reference frame. GPL-3.0. */
#ifndef MIPSNES2_PS2_GS_PRESENT_H
#define MIPSNES2_PS2_GS_PRESENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One 256x256 RGBA32 image, in DMA-accessible EE memory. Initializes the
 * GS output and uploads directly to its framebuffer; no frame pacing loop.
 */
bool mipsnes2_ps2_gs_present(const uint32_t *pixels, size_t pixel_count);

#endif
