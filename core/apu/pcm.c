/* MIPSNES2 bounded platform-neutral PCM handoff. See LICENSE for GPL-3.0 terms. */
#include "core/apu/pcm.h"

#include <string.h>

static void increment_saturating(uint64_t *value)
{
    if (*value != UINT64_MAX) {
        ++*value;
    }
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
