/* PS2 RFAuds2 lifecycle coordinator. See LICENSE for GPL-3.0 terms. */
#include "platform/ps2/rfauds2_runtime.h"

#include <limits.h>
#include <string.h>

#include <rfauds2/rfauds2.h>

static void increment_saturating(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        ++*value;
    }
}

static void add_saturating(uint64_t *value, size_t amount)
{
    uint64_t increment = (uint64_t)amount;

    if (*value > UINT64_MAX - increment) {
        *value = UINT64_MAX;
    } else {
        *value += increment;
    }
}

static void fill_report(
    const mipsnes2_ps2_rfauds2_runtime *runtime,
    mipsnes2_ps2_rfauds2_runtime_report *report)
{
    report->accepted_frames = runtime->accepted_frames;
    report->pump_calls = runtime->pump_calls;
    report->prebuffer_frames = runtime->prebuffer_frames;
    report->device_result = runtime->last_device_result;
    report->started = runtime->started;
}

mipsnes2_ps2_rfauds2_runtime_result mipsnes2_ps2_rfauds2_runtime_begin(
    mipsnes2_ps2_rfauds2_runtime *runtime,
    const void *irx, uint32_t irx_size,
    mipsnes2_pcm_frame *staging, size_t staging_capacity,
    uint32_t latency_ms, size_t prebuffer_frames)
{
    int result;

    if (runtime == NULL) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT;
    }
    (void)memset(runtime, 0, sizeof(*runtime));
    if (irx == NULL || irx_size == 0U || staging == NULL ||
        staging_capacity == 0U ||
        staging_capacity > RFAUDS2_ASYNC_MAX_FRAMES ||
        latency_ms == 0U ||
        prebuffer_frames < 2U * RFAUDS2_OUTPUT_BLOCK_FRAMES ||
        prebuffer_frames > RFAUDS2_RING_CAPACITY_FRAMES) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT;
    }

    result = rfauds2_init(irx, (u32)irx_size);
    runtime->last_device_result = result;
    if (result < 0) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR;
    }
    runtime->device_bound = true;

    result = rfauds2_set_latency_ms((u32)latency_ms);
    runtime->last_device_result = result;
    if (result < 0) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR;
    }
    result = rfauds2_set_volume(RFAUDS2_VOLUME_MAX);
    runtime->last_device_result = result;
    if (result < 0) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR;
    }
    result = rfauds2_reset_stats();
    runtime->last_device_result = result;
    if (result < 0) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR;
    }
    if (!mipsnes2_ps2_rfauds2_audio_init(
            &runtime->adapter, staging, staging_capacity)) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_CONTRACT_ERROR;
    }
    runtime->prebuffer_frames = prebuffer_frames;
    runtime->last_device_result = 0;
    return MIPSNES2_PS2_RFAUDS2_RUNTIME_OK;
}

static mipsnes2_ps2_rfauds2_runtime_result map_pump_result(
    mipsnes2_ps2_audio_pump_result result)
{
    switch (result) {
        case MIPSNES2_PS2_AUDIO_PUMP_OK:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_OK;
        case MIPSNES2_PS2_AUDIO_PUMP_STARVED:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_STARVED;
        case MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_SUBMITTED;
        case MIPSNES2_PS2_AUDIO_PUMP_PENDING:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_PENDING;
        case MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_BACKPRESSURE;
        case MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_ERROR:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR;
        case MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_CONTRACT_ERROR:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_CONTRACT_ERROR;
        case MIPSNES2_PS2_AUDIO_PUMP_INVALID_ARGUMENT:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT;
        case MIPSNES2_PS2_AUDIO_PUMP_NOT_READY:
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY;
    }
    return MIPSNES2_PS2_RFAUDS2_RUNTIME_CONTRACT_ERROR;
}

mipsnes2_ps2_rfauds2_runtime_result mipsnes2_ps2_rfauds2_runtime_pump(
    mipsnes2_ps2_rfauds2_runtime *runtime,
    mipsnes2_pcm_buffer *source,
    mipsnes2_ps2_rfauds2_runtime_report *report)
{
    mipsnes2_ps2_audio_pump_result pump_result;
    mipsnes2_ps2_rfauds2_runtime_result runtime_result;
    int result;

    if (report != NULL) {
        (void)memset(report, 0, sizeof(*report));
    }
    if (runtime == NULL || source == NULL || report == NULL) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT;
    }
    if (!runtime->device_bound || !runtime->adapter.initialized) {
        fill_report(runtime, report);
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY;
    }

    increment_saturating(&runtime->pump_calls);
    pump_result = mipsnes2_ps2_audio_adapter_pump(
        &runtime->adapter, source, &report->pump);
    runtime_result = map_pump_result(pump_result);
    runtime->last_device_result = report->pump.transport_result;
    if (pump_result != MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_CONTRACT_ERROR &&
        pump_result != MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_ERROR &&
        pump_result != MIPSNES2_PS2_AUDIO_PUMP_INVALID_ARGUMENT &&
        pump_result != MIPSNES2_PS2_AUDIO_PUMP_NOT_READY) {
        add_saturating(
            &runtime->accepted_frames, report->pump.accepted_frames);
    }
    if (runtime_result == MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR ||
        runtime_result == MIPSNES2_PS2_RFAUDS2_RUNTIME_CONTRACT_ERROR ||
        runtime_result == MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT ||
        runtime_result == MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY) {
        fill_report(runtime, report);
        return runtime_result;
    }

    if (!runtime->started &&
        runtime->accepted_frames >= runtime->prebuffer_frames &&
        !runtime->adapter.request_in_flight) {
        result = rfauds2_start();
        runtime->last_device_result = result;
        if (result < 0) {
            fill_report(runtime, report);
            return MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR;
        }
        runtime->started = true;
        fill_report(runtime, report);
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_STARTED;
    }

    fill_report(runtime, report);
    return runtime_result;
}

mipsnes2_ps2_rfauds2_runtime_result mipsnes2_ps2_rfauds2_runtime_snapshot(
    mipsnes2_ps2_rfauds2_runtime *runtime,
    mipsnes2_ps2_rfauds2_diagnostics *diagnostics)
{
    rfauds2_stats stats;
    int result;

    if (diagnostics != NULL) {
        (void)memset(diagnostics, 0, sizeof(*diagnostics));
    }
    if (runtime == NULL || diagnostics == NULL) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT;
    }
    if (!runtime->device_bound || !runtime->adapter.initialized) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY;
    }
    if (runtime->adapter.request_in_flight) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_BUSY;
    }

    result = rfauds2_get_stats(&stats);
    runtime->last_device_result = result;
    if (result < 0) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR;
    }
    diagnostics->queued_frames = stats.queued_frames;
    diagnostics->capacity_frames = stats.capacity_frames;
    diagnostics->underruns = stats.underruns;
    diagnostics->overruns = stats.overruns;
    diagnostics->latency_ms = stats.latency_ms;
    diagnostics->volume = stats.volume;
    diagnostics->min_queued_frames = stats.min_queued_frames;
    diagnostics->max_queued_frames = stats.max_queued_frames;
    diagnostics->refill_count = stats.refill_count;
    diagnostics->silent_frames = stats.silent_frames;
    diagnostics->missed_refills = stats.missed_refills;
    diagnostics->started = stats.started != 0U;
    diagnostics->paused = stats.paused != 0U;
    diagnostics->valid = true;
    return MIPSNES2_PS2_RFAUDS2_RUNTIME_OK;
}

mipsnes2_ps2_rfauds2_runtime_result mipsnes2_ps2_rfauds2_runtime_end(
    mipsnes2_ps2_rfauds2_runtime *runtime)
{
    int first_error = 0;
    int result;

    if (runtime == NULL) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT;
    }
    if (!runtime->device_bound) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_OK;
    }
    if (runtime->adapter.request_in_flight) {
        return MIPSNES2_PS2_RFAUDS2_RUNTIME_BUSY;
    }

    if (runtime->started) {
        result = rfauds2_stop();
        if (result < 0) {
            first_error = result;
        } else {
            runtime->started = false;
        }
    }
    result = rfauds2_flush();
    if (result < 0 && first_error == 0) {
        first_error = result;
    }
    result = rfauds2_shutdown();
    if (result < 0 && first_error == 0) {
        first_error = result;
    }
    if (result >= 0) {
        runtime->device_bound = false;
        runtime->started = false;
        (void)memset(&runtime->adapter, 0, sizeof(runtime->adapter));
    }
    runtime->last_device_result = first_error;
    return first_error < 0
        ? MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR
        : MIPSNES2_PS2_RFAUDS2_RUNTIME_OK;
}
