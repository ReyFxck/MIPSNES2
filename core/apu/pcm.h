/* MIPSNES2 bounded platform-neutral PCM handoff. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_APU_PCM_H
#define MIPSNES2_APU_PCM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    MIPSNES2_PCM_SAMPLE_RATE = 32000,
    MIPSNES2_PCM_CHANNEL_COUNT = 2
};

/* One native S-DSP output frame. Samples are signed host-endian values; using
 * typed fields keeps byte serialization and device format outside core/. */
typedef struct {
    int16_t left;
    int16_t right;
} mipsnes2_pcm_frame;

/* Caller-owned, fixed-capacity FIFO. It is deliberately not thread-safe;
 * platform adapters must serialize producer/consumer access. Public counters
 * saturate at UINT64_MAX instead of wrapping.
 */
typedef struct {
    mipsnes2_pcm_frame *frames;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t count;
    uint64_t enqueued_frames;
    uint64_t consumed_frames;
    uint64_t dropped_frames;
    bool initialized;
} mipsnes2_pcm_buffer;

bool mipsnes2_pcm_buffer_init(mipsnes2_pcm_buffer *buffer,
                              mipsnes2_pcm_frame *frames,
                              size_t capacity);

/* Empty the FIFO and counters without modifying caller storage or capacity. */
void mipsnes2_pcm_buffer_reset(mipsnes2_pcm_buffer *buffer);

size_t mipsnes2_pcm_buffer_available(const mipsnes2_pcm_buffer *buffer);
size_t mipsnes2_pcm_buffer_free(const mipsnes2_pcm_buffer *buffer);

/* A full FIFO drops the new frame, increments dropped_frames and returns false.
 * Emulated time must continue; old queued frames are never overwritten. */
bool mipsnes2_pcm_buffer_push(mipsnes2_pcm_buffer *buffer,
                              mipsnes2_pcm_frame frame);

/* Copy up to max_frames in FIFO order. Empty input returns zero and does not
 * synthesize silence; concealment belongs to the platform audio adapter. */
size_t mipsnes2_pcm_buffer_read(mipsnes2_pcm_buffer *buffer,
                                mipsnes2_pcm_frame *frames,
                                size_t max_frames);

#endif
