/* Test-only RFAuds2 public surface. See LICENSE for GPL-3.0 terms. */
#ifndef TEST_RFAUDS2_H
#define TEST_RFAUDS2_H

#include <stdint.h>

typedef int16_t s16;
typedef uint32_t u32;

#define RFAUDS2_OUTPUT_RATE 48000u
#define RFAUDS2_OUTPUT_BLOCK_FRAMES 512u
#define RFAUDS2_RING_CAPACITY_FRAMES 4096u
#define RFAUDS2_VOLUME_MAX 0x3fffu
#define RFAUDS2_ASYNC_MAX_FRAMES 960u
#define RFAUDS2_ERROR_INVALID_ARGUMENT (-1)

typedef struct {
    u32 queued_frames;
    u32 capacity_frames;
    u32 underruns;
    u32 overruns;
    u32 latency_ms;
    u32 volume;
    u32 started;
    u32 paused;
    u32 min_queued_frames;
    u32 max_queued_frames;
    u32 refill_count;
    u32 silent_frames;
    u32 missed_refills;
} rfauds2_stats;

int rfauds2_init(const void *irx, u32 irx_size);
int rfauds2_submit_s16_async(const s16 *interleaved_stereo, u32 frames);
int rfauds2_submit_poll(u32 *accepted_frames);
int rfauds2_start(void);
int rfauds2_stop(void);
int rfauds2_flush(void);
int rfauds2_set_volume(u32 volume);
int rfauds2_set_latency_ms(u32 latency_ms);
int rfauds2_get_stats(rfauds2_stats *stats);
int rfauds2_reset_stats(void);
int rfauds2_shutdown(void);

#endif
