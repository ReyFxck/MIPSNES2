/* PS2 bounded PCM transport adapter. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PS2_AUDIO_ADAPTER_H
#define MIPSNES2_PS2_AUDIO_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/apu/pcm.h"

enum {
    MIPSNES2_PS2_AUDIO_OUTPUT_RATE = 48000
};

/* A transport accepts a prefix of the offered block. Returning less than
 * frame_count leaves an ordered tail for a later pump. Returning more than
 * frame_count violates the contract. The callback must not re-enter adapter
 * functions and must copy or consume the accepted prefix before returning. */
typedef size_t (*mipsnes2_ps2_audio_submit)(
    void *context, const mipsnes2_pcm_frame *frames, size_t frame_count);

typedef enum {
    MIPSNES2_PS2_AUDIO_PUMP_OK = 0,
    MIPSNES2_PS2_AUDIO_PUMP_STARVED,
    MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE,
    MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_CONTRACT_ERROR,
    MIPSNES2_PS2_AUDIO_PUMP_INVALID_ARGUMENT,
    MIPSNES2_PS2_AUDIO_PUMP_NOT_READY
} mipsnes2_ps2_audio_pump_result;

typedef struct {
    size_t generated_frames;
    size_t offered_frames;
    size_t accepted_frames;
    size_t pending_frames;
} mipsnes2_ps2_audio_pump_report;

typedef struct {
    mipsnes2_pcm_resampler resampler;
    mipsnes2_pcm_frame *staging;
    size_t staging_capacity;
    size_t pending_offset;
    size_t pending_frames;
    mipsnes2_ps2_audio_submit submit;
    void *submit_context;
    uint64_t generated_frames;
    uint64_t offered_frames;
    uint64_t accepted_frames;
    uint64_t submit_calls;
    uint64_t starvation_events;
    uint64_t backpressure_events;
    uint64_t contract_errors;
    bool initialized;
} mipsnes2_ps2_audio_adapter;

/* Borrows fixed staging storage and configures native 32 kHz PCM conversion to
 * the PS2 48 kHz boundary. No heap allocation or device API is used here. */
bool mipsnes2_ps2_audio_adapter_init(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_frame *staging,
    size_t staging_capacity, mipsnes2_ps2_audio_submit submit,
    void *submit_context);

/* Discards any retained output tail and counters while preserving configuration. */
void mipsnes2_ps2_audio_adapter_reset(
    mipsnes2_ps2_audio_adapter *adapter);

/* Performs at most one bounded transport call. A retained tail is always
 * retried before more source frames are drained. STARVED does not synthesize
 * silence or run the emulator; the caller owns device concealment and pacing. */
mipsnes2_ps2_audio_pump_result mipsnes2_ps2_audio_adapter_pump(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_buffer *source,
    mipsnes2_ps2_audio_pump_report *report);

#endif
