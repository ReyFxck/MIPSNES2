/* Test-only RFAuds2 public surface. See LICENSE for GPL-3.0 terms. */
#ifndef TEST_RFAUDS2_H
#define TEST_RFAUDS2_H

#include <stdint.h>

typedef int16_t s16;
typedef uint32_t u32;

#define RFAUDS2_OUTPUT_RATE 48000u
#define RFAUDS2_ASYNC_MAX_FRAMES 960u

int rfauds2_submit_s16_async(const s16 *interleaved_stereo, u32 frames);
int rfauds2_submit_poll(u32 *accepted_frames);

#endif
