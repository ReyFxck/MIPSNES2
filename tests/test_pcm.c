/* MIPSNES2 bounded PCM FIFO tests. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>

#include "core/apu/pcm.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static mipsnes2_pcm_frame frame(int16_t left, int16_t right)
{
    mipsnes2_pcm_frame result;

    result.left = left;
    result.right = right;
    return result;
}

static bool frame_equal(mipsnes2_pcm_frame left,
                        mipsnes2_pcm_frame right)
{
    return left.left == right.left && left.right == right.right;
}

int main(void)
{
    mipsnes2_pcm_buffer buffer = {0};
    mipsnes2_pcm_frame storage[3];
    mipsnes2_pcm_frame output[4];
    mipsnes2_pcm_frame first = frame(INT16_C(-32768), INT16_C(32767));
    mipsnes2_pcm_frame second = frame(INT16_C(-1), INT16_C(1));
    mipsnes2_pcm_frame third = frame(INT16_C(1234), INT16_C(-5678));
    mipsnes2_pcm_frame fourth = frame(INT16_C(2222), INT16_C(3333));
    mipsnes2_pcm_frame fifth = frame(INT16_C(4444), INT16_C(5555));

    CHECK(MIPSNES2_PCM_SAMPLE_RATE == 32000);
    CHECK(MIPSNES2_PCM_CHANNEL_COUNT == 2);
    CHECK(!mipsnes2_pcm_buffer_init(NULL, storage, 3U));
    CHECK(!mipsnes2_pcm_buffer_init(&buffer, NULL, 3U));
    CHECK(!buffer.initialized);
    CHECK(!mipsnes2_pcm_buffer_init(&buffer, storage, 0U));
    CHECK(!buffer.initialized);
    CHECK(mipsnes2_pcm_buffer_init(&buffer, storage, 3U));
    CHECK(mipsnes2_pcm_buffer_available(&buffer) == 0U);
    CHECK(mipsnes2_pcm_buffer_free(&buffer) == 3U);
    CHECK(mipsnes2_pcm_buffer_read(&buffer, output, 4U) == 0U);
    CHECK(mipsnes2_pcm_buffer_read(&buffer, NULL, 0U) == 0U);
    CHECK(mipsnes2_pcm_buffer_read(&buffer, NULL, 1U) == 0U);

    CHECK(mipsnes2_pcm_buffer_push(&buffer, first));
    CHECK(mipsnes2_pcm_buffer_push(&buffer, second));
    CHECK(mipsnes2_pcm_buffer_push(&buffer, third));
    CHECK(buffer.enqueued_frames == UINT64_C(3));
    CHECK(mipsnes2_pcm_buffer_available(&buffer) == 3U);
    CHECK(mipsnes2_pcm_buffer_free(&buffer) == 0U);
    CHECK(!mipsnes2_pcm_buffer_push(&buffer, fourth));
    CHECK(buffer.dropped_frames == UINT64_C(1));

    CHECK(mipsnes2_pcm_buffer_read(&buffer, output, 2U) == 2U);
    CHECK(frame_equal(output[0], first));
    CHECK(frame_equal(output[1], second));
    CHECK(buffer.consumed_frames == UINT64_C(2));
    CHECK(mipsnes2_pcm_buffer_push(&buffer, fourth));
    CHECK(mipsnes2_pcm_buffer_push(&buffer, fifth));
    CHECK(mipsnes2_pcm_buffer_read(&buffer, output, 4U) == 3U);
    CHECK(frame_equal(output[0], third));
    CHECK(frame_equal(output[1], fourth));
    CHECK(frame_equal(output[2], fifth));
    CHECK(buffer.enqueued_frames == UINT64_C(5));
    CHECK(buffer.consumed_frames == UINT64_C(5));
    CHECK(buffer.dropped_frames == UINT64_C(1));

    buffer.enqueued_frames = UINT64_MAX;
    buffer.consumed_frames = UINT64_MAX;
    buffer.dropped_frames = UINT64_MAX;
    CHECK(mipsnes2_pcm_buffer_push(&buffer, first));
    CHECK(buffer.enqueued_frames == UINT64_MAX);
    CHECK(mipsnes2_pcm_buffer_read(&buffer, output, 1U) == 1U);
    CHECK(buffer.consumed_frames == UINT64_MAX);
    CHECK(mipsnes2_pcm_buffer_push(&buffer, first));
    CHECK(mipsnes2_pcm_buffer_push(&buffer, second));
    CHECK(mipsnes2_pcm_buffer_push(&buffer, third));
    CHECK(!mipsnes2_pcm_buffer_push(&buffer, fourth));
    CHECK(buffer.dropped_frames == UINT64_MAX);

    mipsnes2_pcm_buffer_reset(&buffer);
    CHECK(buffer.initialized && buffer.frames == storage &&
          buffer.capacity == 3U);
    CHECK(buffer.count == 0U && buffer.read_index == 0U &&
          buffer.write_index == 0U);
    CHECK(buffer.enqueued_frames == 0U && buffer.consumed_frames == 0U &&
          buffer.dropped_frames == 0U);
    CHECK(mipsnes2_pcm_buffer_available(NULL) == 0U);
    CHECK(mipsnes2_pcm_buffer_free(NULL) == 0U);
    CHECK(!mipsnes2_pcm_buffer_push(NULL, first));

    CHECK(!mipsnes2_pcm_buffer_init(&buffer, NULL, 3U));
    CHECK(!buffer.initialized && buffer.frames == NULL);
    (void)puts("bounded PCM FIFO: OK");
    return 0;
}
