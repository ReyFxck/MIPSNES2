/* PS2 RFAuds2 lifecycle coordinator. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PS2_RFAUDS2_RUNTIME_H
#define MIPSNES2_PS2_RFAUDS2_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/ps2/rfauds2_transport.h"

typedef enum {
    MIPSNES2_PS2_RFAUDS2_RUNTIME_OK = 0,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_SUBMITTED,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_PENDING,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_BACKPRESSURE,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_STARVED,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_STARTED,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_BUSY,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_CONTRACT_ERROR,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT,
    MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY
} mipsnes2_ps2_rfauds2_runtime_result;

typedef struct {
    mipsnes2_ps2_audio_pump_report pump;
    uint64_t accepted_frames;
    uint64_t pump_calls;
    size_t prebuffer_frames;
    int device_result;
    bool started;
} mipsnes2_ps2_rfauds2_runtime_report;

typedef struct {
    uint32_t queued_frames;
    uint32_t capacity_frames;
    uint32_t underruns;
    uint32_t overruns;
    uint32_t latency_ms;
    uint32_t volume;
    uint32_t min_queued_frames;
    uint32_t max_queued_frames;
    uint32_t refill_count;
    uint32_t silent_frames;
    uint32_t missed_refills;
    bool started;
    bool paused;
    bool valid;
} mipsnes2_ps2_rfauds2_diagnostics;

typedef struct {
    mipsnes2_ps2_audio_adapter adapter;
    size_t prebuffer_frames;
    uint64_t accepted_frames;
    uint64_t pump_calls;
    int last_device_result;
    bool device_bound;
    bool started;
} mipsnes2_ps2_rfauds2_runtime;

/* Loads the caller-provided IRX, configures the bounded device queue and
 * borrows fixed staging storage. Playback remains stopped until pump() has
 * observed at least prebuffer_frames accepted by the IOP ring. */
mipsnes2_ps2_rfauds2_runtime_result mipsnes2_ps2_rfauds2_runtime_begin(
    mipsnes2_ps2_rfauds2_runtime *runtime,
    const void *irx, uint32_t irx_size,
    mipsnes2_pcm_frame *staging, size_t staging_capacity,
    uint32_t latency_ms, size_t prebuffer_frames);

/* Performs at most one asynchronous launch or poll. It never runs the
 * machine and never fabricates source PCM. */
mipsnes2_ps2_rfauds2_runtime_result mipsnes2_ps2_rfauds2_runtime_pump(
    mipsnes2_ps2_rfauds2_runtime *runtime,
    mipsnes2_pcm_buffer *source,
    mipsnes2_ps2_rfauds2_runtime_report *report);

/* Synchronous diagnostic query. It refuses to collide with an asynchronous
 * PCM request instead of spinning on the device. */
mipsnes2_ps2_rfauds2_runtime_result mipsnes2_ps2_rfauds2_runtime_snapshot(
    mipsnes2_ps2_rfauds2_runtime *runtime,
    mipsnes2_ps2_rfauds2_diagnostics *diagnostics);

/* Stops DMA, discards queued device PCM and releases RFAuds2 resources. An
 * outstanding asynchronous request must first be collected with pump(). */
mipsnes2_ps2_rfauds2_runtime_result mipsnes2_ps2_rfauds2_runtime_end(
    mipsnes2_ps2_rfauds2_runtime *runtime);

#endif
