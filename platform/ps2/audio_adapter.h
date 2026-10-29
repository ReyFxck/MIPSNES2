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

/* Launch owns a copy of the offered block before returning zero. A negative
 * result means no request is pending. Poll returns zero while pending, one
 * with an accepted prefix on completion, or a negative completed error.
 * Both callbacks must be serialized and must not re-enter the adapter. */
typedef int (*mipsnes2_ps2_audio_launch)(
    void *context, const mipsnes2_pcm_frame *frames, size_t frame_count);
typedef int (*mipsnes2_ps2_audio_poll)(
    void *context, size_t *accepted_frames);

typedef enum {
    MIPSNES2_PS2_AUDIO_PUMP_OK = 0,
    MIPSNES2_PS2_AUDIO_PUMP_STARVED,
    MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED,
    MIPSNES2_PS2_AUDIO_PUMP_PENDING,
    MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE,
    MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_ERROR,
    MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_CONTRACT_ERROR,
    MIPSNES2_PS2_AUDIO_PUMP_INVALID_ARGUMENT,
    MIPSNES2_PS2_AUDIO_PUMP_NOT_READY
} mipsnes2_ps2_audio_pump_result;

typedef struct {
    size_t generated_frames;
    size_t offered_frames;
    size_t accepted_frames;
    size_t pending_frames;
    int transport_result;
} mipsnes2_ps2_audio_pump_report;

typedef struct {
    mipsnes2_pcm_resampler resampler;
    mipsnes2_pcm_frame *staging;
    size_t staging_capacity;
    size_t pending_offset;
    size_t pending_frames;
    size_t in_flight_frames;
    mipsnes2_ps2_audio_launch launch;
    mipsnes2_ps2_audio_poll poll;
    void *transport_context;
    uint64_t generated_frames;
    uint64_t offered_frames;
    uint64_t accepted_frames;
    uint64_t launch_calls;
    uint64_t poll_calls;
    uint64_t starvation_events;
    uint64_t pending_events;
    uint64_t backpressure_events;
    uint64_t transport_errors;
    uint64_t contract_errors;
    bool request_in_flight;
    bool initialized;
} mipsnes2_ps2_audio_adapter;

/* Borrows fixed staging storage and configures native 32 kHz PCM conversion to
 * the PS2 48 kHz boundary. No heap allocation or device API is used here. */
bool mipsnes2_ps2_audio_adapter_init(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_frame *staging,
    size_t staging_capacity, mipsnes2_ps2_audio_launch launch,
    mipsnes2_ps2_audio_poll poll, void *transport_context);

/* Discards retained output and counters while preserving configuration.
 * Refuses to reset an in-flight request; complete it or shut down the
 * transport and initialize the adapter again. */
bool mipsnes2_ps2_audio_adapter_reset(
    mipsnes2_ps2_audio_adapter *adapter);

/* Performs at most one transport operation: launch or poll. A retained tail
 * is always relaunched before more source frames are drained. STARVED does
 * not synthesize silence or run the emulator; device concealment and pacing
 * remain outside this adapter. */
mipsnes2_ps2_audio_pump_result mipsnes2_ps2_audio_adapter_pump(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_buffer *source,
    mipsnes2_ps2_audio_pump_report *report);

#endif
