/* MIPSNES2 bounded platform-neutral PCM handoff. See LICENSE for GPL-3.0 terms. */
#include "core/apu/pcm.h"

#include <string.h>

static void increment_saturating(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        ++*value;
    }
}

static bool resampler_take_source(mipsnes2_pcm_resampler *resampler,
                                  mipsnes2_pcm_buffer *source,
                                  mipsnes2_pcm_frame *frame)
{
    if (mipsnes2_pcm_buffer_read(source, frame, 1U) != 1U) {
        return false;
    }
    increment_saturating(&resampler->source_frames);
    return true;
}

static int16_t resampler_interpolate(int16_t current, int16_t next,
                                     uint32_t phase, uint32_t output_rate)
{
    int64_t value = (int64_t)current * (output_rate - phase) +
                    (int64_t)next * phase;
    int64_t half = (int64_t)output_rate / INT64_C(2);

    if (value >= 0) {
        value = (value + half) / output_rate;
    } else {
        value = (value - half) / output_rate;
    }
    return (int16_t)value;
}

bool mipsnes2_pcm_buffer_init(mipsnes2_pcm_buffer *buffer,
                              mipsnes2_pcm_frame *frames,
                              size_t capacity)
{
    if (buffer == NULL) {
        return false;
    }
    (void)memset(buffer, 0, sizeof(*buffer));
    if (frames == NULL || capacity == 0U) {
        return false;
    }
    buffer->frames = frames;
    buffer->capacity = capacity;
    buffer->initialized = true;
    return true;
}

void mipsnes2_pcm_buffer_reset(mipsnes2_pcm_buffer *buffer)
{
    if (buffer == NULL || !buffer->initialized) {
        return;
    }
    buffer->read_index = 0U;
    buffer->write_index = 0U;
    buffer->count = 0U;
    buffer->enqueued_frames = 0U;
    buffer->consumed_frames = 0U;
    buffer->dropped_frames = 0U;
}

size_t mipsnes2_pcm_buffer_available(const mipsnes2_pcm_buffer *buffer)
{
    if (buffer == NULL || !buffer->initialized) {
        return 0U;
    }
    return buffer->count;
}

size_t mipsnes2_pcm_buffer_free(const mipsnes2_pcm_buffer *buffer)
{
    if (buffer == NULL || !buffer->initialized) {
        return 0U;
    }
    return buffer->capacity - buffer->count;
}

bool mipsnes2_pcm_buffer_push(mipsnes2_pcm_buffer *buffer,
                              mipsnes2_pcm_frame frame)
{
    if (buffer == NULL || !buffer->initialized) {
        return false;
    }
    if (buffer->count == buffer->capacity) {
        increment_saturating(&buffer->dropped_frames);
        return false;
    }
    buffer->frames[buffer->write_index] = frame;
    ++buffer->write_index;
    if (buffer->write_index == buffer->capacity) {
        buffer->write_index = 0U;
    }
    ++buffer->count;
    increment_saturating(&buffer->enqueued_frames);
    return true;
}

size_t mipsnes2_pcm_buffer_read(mipsnes2_pcm_buffer *buffer,
                                mipsnes2_pcm_frame *frames,
                                size_t max_frames)
{
    size_t copied = 0U;

    if (buffer == NULL || !buffer->initialized ||
        (frames == NULL && max_frames != 0U)) {
        return 0U;
    }
    while (copied < max_frames && buffer->count != 0U) {
        frames[copied] = buffer->frames[buffer->read_index];
        ++buffer->read_index;
        if (buffer->read_index == buffer->capacity) {
            buffer->read_index = 0U;
        }
        --buffer->count;
        ++copied;
        increment_saturating(&buffer->consumed_frames);
    }
    return copied;
}

bool mipsnes2_pcm_resampler_init(mipsnes2_pcm_resampler *resampler,
                                 uint32_t output_rate)
{
    if (resampler == NULL) {
        return false;
    }
    (void)memset(resampler, 0, sizeof(*resampler));
    if (output_rate < MIPSNES2_PCM_SAMPLE_RATE) {
        return false;
    }
    resampler->output_rate = output_rate;
    resampler->initialized = true;
    return true;
}

void mipsnes2_pcm_resampler_reset(mipsnes2_pcm_resampler *resampler)
{
    uint32_t output_rate;

    if (resampler == NULL || !resampler->initialized) {
        return;
    }
    output_rate = resampler->output_rate;
    (void)memset(resampler, 0, sizeof(*resampler));
    resampler->output_rate = output_rate;
    resampler->initialized = true;
}

size_t mipsnes2_pcm_resampler_process(mipsnes2_pcm_resampler *resampler,
                                      mipsnes2_pcm_buffer *source,
                                      mipsnes2_pcm_frame *frames,
                                      size_t max_frames)
{
    size_t produced = 0U;

    if (resampler == NULL || !resampler->initialized || source == NULL ||
        !source->initialized ||
        resampler->output_rate < MIPSNES2_PCM_SAMPLE_RATE ||
        resampler->phase >= resampler->output_rate ||
        (resampler->has_next && !resampler->has_current) ||
        (frames == NULL && max_frames != 0U)) {
        return 0U;
    }
    while (produced < max_frames) {
        mipsnes2_pcm_frame result;
        uint64_t next_phase;

        if (!resampler->has_current) {
            if (!resampler_take_source(resampler, source,
                                       &resampler->current)) {
                break;
            }
            resampler->has_current = true;
        }
        if (resampler->phase != 0U && !resampler->has_next) {
            if (!resampler_take_source(resampler, source,
                                       &resampler->next)) {
                break;
            }
            resampler->has_next = true;
        }
        if (resampler->phase == 0U) {
            result = resampler->current;
        } else {
            result.left = resampler_interpolate(
                resampler->current.left, resampler->next.left,
                resampler->phase, resampler->output_rate);
            result.right = resampler_interpolate(
                resampler->current.right, resampler->next.right,
                resampler->phase, resampler->output_rate);
        }
        frames[produced] = result;
        ++produced;
        increment_saturating(&resampler->output_frames);

        next_phase = (uint64_t)resampler->phase +
                     MIPSNES2_PCM_SAMPLE_RATE;
        if (next_phase >= resampler->output_rate) {
            next_phase -= resampler->output_rate;
            if (resampler->has_next) {
                resampler->current = resampler->next;
                resampler->has_next = false;
            } else {
                resampler->has_current = false;
            }
        }
        resampler->phase = (uint32_t)next_phase;
    }
    return produced;
}
