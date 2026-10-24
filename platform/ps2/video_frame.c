/* PS2 reference-frame capture; no GS calls. See LICENSE for GPL-3.0. */
#include "platform/ps2/video_frame.h"

#include <string.h>

bool mipsnes2_ps2_video_frame_init(mipsnes2_ps2_video_frame *frame,
                                   uint32_t *pixels, size_t pixel_count,
                                   uint16_t visible_rows)
{
    if (frame == NULL || pixels == NULL ||
        pixel_count < MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS ||
        (visible_rows != 224U && visible_rows != 239U)) {
        return false;
    }
    (void)memset(frame, 0, sizeof(*frame));
    (void)memset(pixels, 0,
                 MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS * sizeof(*pixels));
    frame->pixels = pixels;
    frame->visible_rows = visible_rows;
    return true;
}

void mipsnes2_ps2_video_frame_line(void *context, uint16_t line_y,
                                   const uint16_t pixels[256])
{
    mipsnes2_ps2_video_frame *frame = context;
    unsigned x;
    size_t row;

    if (frame == NULL || frame->pixels == NULL || pixels == NULL ||
        line_y >= frame->visible_rows) {
        return;
    }
    row = (size_t)line_y * MIPSNES2_PS2_VIDEO_WIDTH;
    for (x = 0U; x < MIPSNES2_PS2_VIDEO_WIDTH; ++x) {
        uint16_t color = pixels[x];
        uint32_t red = (uint32_t)(color & 31U);
        uint32_t green = (uint32_t)((color >> 5) & 31U);
        uint32_t blue = (uint32_t)((color >> 10) & 31U);

        red = (red << 3) | (red >> 2);
        green = (green << 3) | (green >> 2);
        blue = (blue << 3) | (blue >> 2);
        frame->pixels[row + x] = red | (green << 8) | (blue << 16) |
                                 UINT32_C(0x80000000);
    }
    if (frame->received[line_y] == 0U) {
        frame->received[line_y] = 1U;
        ++frame->delivered_rows;
    }
}
