/* Synthetic PS2 texture-capture fixture. See LICENSE for GPL-3.0. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform/ps2/video_frame.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint32_t texture[MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS];
static uint16_t source[256];

int main(void)
{
    mipsnes2_ps2_video_frame frame;

    (void)memset(&frame, 0x5a, sizeof(frame));
    texture[0] = UINT32_C(0xdeadbeef);
    CHECK(!mipsnes2_ps2_video_frame_init(
        &frame, texture, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS - 1U, 224U));
    CHECK(frame.delivered_rows == UINT16_C(0x5a5a) &&
          texture[0] == UINT32_C(0xdeadbeef));
    CHECK(!mipsnes2_ps2_video_frame_init(
        &frame, texture, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS, 240U));
    CHECK(mipsnes2_ps2_video_frame_init(
        &frame, texture, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS, 224U));
    CHECK(texture[0] == 0U && texture[223U * 256U] == 0U &&
          texture[255U * 256U] == 0U);
    (void)memset(source, 0, sizeof(source));
    source[0] = UINT16_C(0x001f); /* Red 31. */
    source[1] = UINT16_C(0x03e0); /* Green 31. */
    source[2] = UINT16_C(0x7c00); /* Blue 31. */
    source[3] = UINT16_C(0x4210); /* All channels 16. */
    mipsnes2_ps2_video_frame_line(&frame, 223U, source);
    CHECK(frame.delivered_rows == 1U && frame.received[223] == 1U);
    CHECK(texture[223U * 256U] == UINT32_C(0x800000ff));
    CHECK(texture[223U * 256U + 1U] == UINT32_C(0x8000ff00));
    CHECK(texture[223U * 256U + 2U] == UINT32_C(0x80ff0000));
    CHECK(texture[223U * 256U + 3U] == UINT32_C(0x80848484));
    CHECK(texture[223U * 256U + 4U] == UINT32_C(0x80000000));
    CHECK(texture[224U * 256U] == 0U);
    mipsnes2_ps2_video_frame_line(&frame, 223U, source);
    mipsnes2_ps2_video_frame_line(&frame, 224U, source);
    CHECK(frame.delivered_rows == 1U);
    source[0] = 0U;
    CHECK(texture[223U * 256U] == UINT32_C(0x800000ff));
    CHECK(mipsnes2_ps2_video_frame_init(
        &frame, texture, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS, 239U));
    mipsnes2_ps2_video_frame_line(&frame, 238U, source);
    CHECK(frame.delivered_rows == 1U && texture[238U * 256U] ==
          UINT32_C(0x80000000));
    (void)puts("PS2 video-frame capture: OK");
    return 0;
}
