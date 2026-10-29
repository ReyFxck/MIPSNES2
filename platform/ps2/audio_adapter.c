/* PS2 bounded PCM transport adapter. See LICENSE for GPL-3.0 terms. */
#include "platform/ps2/audio_adapter.h"

#include <string.h>

static void add_saturating(uint64_t *value, size_t amount)
{
    uint64_t increment = (uint64_t)amount;

    if (*value > UINT64_MAX - increment) {
        *value = UINT64_MAX;
    } else {
        *value += increment;
    }
}

static void increment_saturating(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        ++*value;
    }
}

bool mipsnes2_ps2_audio_adapter_init(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_frame *staging,
    size_t staging_capacity, mipsnes2_ps2_audio_launch launch,
    mipsnes2_ps2_audio_poll poll, void *transport_context)
{
    if (adapter == NULL) {
        return false;
    }
    (void)memset(adapter, 0, sizeof(*adapter));
    if (staging == NULL || staging_capacity == 0U || launch == NULL ||
        poll == NULL || !mipsnes2_pcm_resampler_init(
            &adapter->resampler, MIPSNES2_PS2_AUDIO_OUTPUT_RATE)) {
        return false;
    }
    adapter->staging = staging;
    adapter->staging_capacity = staging_capacity;
    adapter->launch = launch;
    adapter->poll = poll;
    adapter->transport_context = transport_context;
    adapter->initialized = true;
    return true;
}

bool mipsnes2_ps2_audio_adapter_reset(
    mipsnes2_ps2_audio_adapter *adapter)
{
    mipsnes2_pcm_frame *staging;
    size_t staging_capacity;
    mipsnes2_ps2_audio_launch launch;
    mipsnes2_ps2_audio_poll poll;
    void *transport_context;

    if (adapter == NULL || !adapter->initialized ||
        adapter->request_in_flight) {
        return false;
    }
    staging = adapter->staging;
    staging_capacity = adapter->staging_capacity;
    launch = adapter->launch;
    poll = adapter->poll;
    transport_context = adapter->transport_context;
    (void)memset(adapter, 0, sizeof(*adapter));
    adapter->staging = staging;
    adapter->staging_capacity = staging_capacity;
    adapter->launch = launch;
    adapter->poll = poll;
    adapter->transport_context = transport_context;
    if (!mipsnes2_pcm_resampler_init(
            &adapter->resampler, MIPSNES2_PS2_AUDIO_OUTPUT_RATE)) {
        return false;
    }
    adapter->initialized = true;
    return true;
}

static bool state_is_ready(const mipsnes2_ps2_audio_adapter *adapter,
                           const mipsnes2_pcm_buffer *source)
{
    if (!adapter->initialized || adapter->staging == NULL ||
        adapter->staging_capacity == 0U || adapter->launch == NULL ||
        adapter->poll == NULL || !adapter->resampler.initialized ||
        !source->initialized ||
        adapter->pending_offset > adapter->staging_capacity ||
        adapter->pending_frames >
            adapter->staging_capacity - adapter->pending_offset) {
        return false;
    }
    if (adapter->request_in_flight) {
        return adapter->in_flight_frames != 0U &&
               adapter->in_flight_frames == adapter->pending_frames;
    }
    return adapter->in_flight_frames == 0U;
}

static mipsnes2_ps2_audio_pump_result poll_request(
    mipsnes2_ps2_audio_adapter *adapter,
    mipsnes2_ps2_audio_pump_report *report)
{
    size_t accepted = 0U;
    size_t offered = adapter->in_flight_frames;
    int result;

    increment_saturating(&adapter->poll_calls);
    result = adapter->poll(adapter->transport_context, &accepted);
    report->transport_result = result;
    report->pending_frames = adapter->pending_frames;
    if (result == 0) {
        increment_saturating(&adapter->pending_events);
        return MIPSNES2_PS2_AUDIO_PUMP_PENDING;
    }

    adapter->request_in_flight = false;
    adapter->in_flight_frames = 0U;
    if (result < 0) {
        increment_saturating(&adapter->transport_errors);
        return MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_ERROR;
    }
    if (result != 1 || accepted > offered) {
        increment_saturating(&adapter->contract_errors);
        return MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_CONTRACT_ERROR;
    }

    report->accepted_frames = accepted;
    add_saturating(&adapter->accepted_frames, accepted);
    adapter->pending_offset += accepted;
    adapter->pending_frames -= accepted;
    report->pending_frames = adapter->pending_frames;
    if (adapter->pending_frames != 0U) {
        increment_saturating(&adapter->backpressure_events);
        return MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE;
    }
    adapter->pending_offset = 0U;
    return MIPSNES2_PS2_AUDIO_PUMP_OK;
}

mipsnes2_ps2_audio_pump_result mipsnes2_ps2_audio_adapter_pump(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_buffer *source,
    mipsnes2_ps2_audio_pump_report *report)
{
    size_t offered;
    int result;

    if (report != NULL) {
        (void)memset(report, 0, sizeof(*report));
    }
    if (adapter == NULL || source == NULL || report == NULL) {
        return MIPSNES2_PS2_AUDIO_PUMP_INVALID_ARGUMENT;
    }
    if (!state_is_ready(adapter, source)) {
        return MIPSNES2_PS2_AUDIO_PUMP_NOT_READY;
    }
    if (adapter->request_in_flight) {
        return poll_request(adapter, report);
    }

    if (adapter->pending_frames == 0U) {
        adapter->pending_offset = 0U;
        adapter->pending_frames = mipsnes2_pcm_resampler_process(
            &adapter->resampler, source, adapter->staging,
            adapter->staging_capacity);
        report->generated_frames = adapter->pending_frames;
        add_saturating(&adapter->generated_frames,
                       adapter->pending_frames);
        if (adapter->pending_frames == 0U) {
            increment_saturating(&adapter->starvation_events);
            return MIPSNES2_PS2_AUDIO_PUMP_STARVED;
        }
    }

    offered = adapter->pending_frames;
    report->offered_frames = offered;
    report->pending_frames = offered;
    increment_saturating(&adapter->launch_calls);
    add_saturating(&adapter->offered_frames, offered);
    result = adapter->launch(
        adapter->transport_context, adapter->staging + adapter->pending_offset,
        offered);
    report->transport_result = result;
    if (result < 0) {
        increment_saturating(&adapter->transport_errors);
        return MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_ERROR;
    }
    if (result != 0) {
        increment_saturating(&adapter->contract_errors);
        return MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_CONTRACT_ERROR;
    }

    adapter->in_flight_frames = offered;
    adapter->request_in_flight = true;
    return MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED;
}
