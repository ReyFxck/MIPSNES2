/* PS2 reference-frame capture; no GS calls. See LICENSE for GPL-3.0. */
#ifndef MIPSNES2_PS2_VIDEO_FRAME_H
#define MIPSNES2_PS2_VIDEO_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MIPSNES2_PS2_VIDEO_WIDTH = 256,
    MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT = 256,
    MIPSNES2_PS2_VIDEO_MAX_VISIBLE = 239,
    MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS = 256 * 256
};

typedef struct {
    uint32_t *pixels; /* Caller-owned RGBA32 texture, row stride 256. */
    uint16_t visible_rows;
    uint16_t delivered_rows;
    uint8_t received[MIPSNES2_PS2_VIDEO_MAX_VISIBLE];
} mipsnes2_ps2_video_frame;

/* Prepares a zeroed 256x256 texture for 224 or 239 visible lines.
 * Invalid arguments leave frame and pixels unchanged.
 */
bool mipsnes2_ps2_video_frame_init(mipsnes2_ps2_video_frame *frame,
                                   uint32_t *pixels, size_t pixel_count,
                                   uint16_t visible_rows);
/* Callback compatible with mipsnes2_video_line_sink. A source line must
 * contain 256 nominal BGR555 pixels. RGBA32 uses R in bits 0..7, G in
 * 8..15, B in 16..23, and GS opaque alpha 0x80 in 24..31.
 */
void mipsnes2_ps2_video_frame_line(void *context, uint16_t line_y,
                                   const uint16_t pixels[256]);

#endif
