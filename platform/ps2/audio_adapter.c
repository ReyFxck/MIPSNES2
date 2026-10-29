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
    size_t staging_capacity, mipsnes2_ps2_audio_submit submit,
    void *submit_context)
{
    if (adapter == NULL) {
        return false;
    }
    (void)memset(adapter, 0, sizeof(*adapter));
    if (staging == NULL || staging_capacity == 0U || submit == NULL ||
        !mipsnes2_pcm_resampler_init(
            &adapter->resampler, MIPSNES2_PS2_AUDIO_OUTPUT_RATE)) {
        return false;
    }
    adapter->staging = staging;
    adapter->staging_capacity = staging_capacity;
    adapter->submit = submit;
    adapter->submit_context = submit_context;
    adapter->initialized = true;
    return true;
}

void mipsnes2_ps2_audio_adapter_reset(
    mipsnes2_ps2_audio_adapter *adapter)
{
    mipsnes2_pcm_frame *staging;
    size_t staging_capacity;
    mipsnes2_ps2_audio_submit submit;
    void *submit_context;

    if (adapter == NULL || !adapter->initialized) {
        return;
    }
    staging = adapter->staging;
    staging_capacity = adapter->staging_capacity;
    submit = adapter->submit;
    submit_context = adapter->submit_context;
    (void)memset(adapter, 0, sizeof(*adapter));
    adapter->staging = staging;
    adapter->staging_capacity = staging_capacity;
    adapter->submit = submit;
    adapter->submit_context = submit_context;
    if (!mipsnes2_pcm_resampler_init(
            &adapter->resampler, MIPSNES2_PS2_AUDIO_OUTPUT_RATE)) {
        return;
    }
    adapter->initialized = true;
}

mipsnes2_ps2_audio_pump_result mipsnes2_ps2_audio_adapter_pump(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_buffer *source,
    mipsnes2_ps2_audio_pump_report *report)
{
    size_t offered;
    size_t accepted;

    if (report != NULL) {
        (void)memset(report, 0, sizeof(*report));
    }
    if (adapter == NULL || source == NULL || report == NULL) {
        return MIPSNES2_PS2_AUDIO_PUMP_INVALID_ARGUMENT;
    }
    if (!adapter->initialized || adapter->staging == NULL ||
        adapter->staging_capacity == 0U || adapter->submit == NULL ||
        !adapter->resampler.initialized || !source->initialized ||
        adapter->pending_offset > adapter->staging_capacity ||
        adapter->pending_frames >
            adapter->staging_capacity - adapter->pending_offset) {
        return MIPSNES2_PS2_AUDIO_PUMP_NOT_READY;
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
    increment_saturating(&adapter->submit_calls);
    add_saturating(&adapter->offered_frames, offered);
    accepted = adapter->submit(
        adapter->submit_context, adapter->staging + adapter->pending_offset,
        offered);
    if (accepted > offered) {
        increment_saturating(&adapter->contract_errors);
        report->pending_frames = adapter->pending_frames;
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
