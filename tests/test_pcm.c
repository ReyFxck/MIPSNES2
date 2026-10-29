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

static int test_identity_resampler(void)
{
    mipsnes2_pcm_buffer source = {0};
    mipsnes2_pcm_resampler resampler = {0};
    mipsnes2_pcm_frame storage[2];
    mipsnes2_pcm_frame output[3];
    mipsnes2_pcm_frame first = frame(INT16_C(-32768), INT16_C(32767));
    mipsnes2_pcm_frame second = frame(INT16_C(1234), INT16_C(-5678));

    CHECK(!mipsnes2_pcm_resampler_init(NULL, 32000U));
    CHECK(!mipsnes2_pcm_resampler_init(&resampler, 31999U));
    CHECK(!resampler.initialized);
    CHECK(mipsnes2_pcm_resampler_init(&resampler, 32000U));
    CHECK(mipsnes2_pcm_buffer_init(&source, storage, 2U));
    CHECK(mipsnes2_pcm_buffer_push(&source, first));
    CHECK(mipsnes2_pcm_buffer_push(&source, second));
    CHECK(mipsnes2_pcm_resampler_process(
              &resampler, &source, NULL, 0U) == 0U);
    CHECK(source.count == 2U && resampler.source_frames == 0U);
    CHECK(mipsnes2_pcm_resampler_process(
              &resampler, &source, output, 3U) == 2U);
    CHECK(frame_equal(output[0], first));
    CHECK(frame_equal(output[1], second));
    CHECK(source.count == 0U && source.consumed_frames == UINT64_C(2));
    CHECK(resampler.source_frames == UINT64_C(2) &&
          resampler.output_frames == UINT64_C(2));
    CHECK(!resampler.has_current && !resampler.has_next &&
          resampler.phase == 0U);

    mipsnes2_pcm_resampler_reset(&resampler);
    CHECK(resampler.initialized && resampler.output_rate == 32000U);
    CHECK(resampler.phase == 0U && !resampler.has_current &&
          !resampler.has_next && resampler.source_frames == 0U &&
          resampler.output_frames == 0U);
    mipsnes2_pcm_resampler_reset(NULL);
    return 0;
}

static int test_ps2_rate_resampler(void)
{
    mipsnes2_pcm_buffer source = {0};
    mipsnes2_pcm_resampler resampler = {0};
    mipsnes2_pcm_frame storage[4];
    mipsnes2_pcm_frame output[4];
    mipsnes2_pcm_frame first = frame(INT16_C(0), INT16_C(0));
    mipsnes2_pcm_frame second = frame(INT16_C(3000), INT16_C(-3000));
    mipsnes2_pcm_frame third = frame(INT16_C(6000), INT16_C(-6000));
    mipsnes2_pcm_frame fourth = frame(INT16_C(9000), INT16_C(-9000));

    CHECK(mipsnes2_pcm_buffer_init(&source, storage, 4U));
    CHECK(mipsnes2_pcm_resampler_init(&resampler, 48000U));
    CHECK(mipsnes2_pcm_buffer_push(&source, first));
    CHECK(mipsnes2_pcm_resampler_process(
              &resampler, &source, output, 4U) == 1U);
    CHECK(frame_equal(output[0], first));
    CHECK(resampler.phase == 32000U && resampler.has_current &&
          !resampler.has_next);
    CHECK(mipsnes2_pcm_resampler_process(
              &resampler, &source, output, 1U) == 0U);

    CHECK(mipsnes2_pcm_buffer_push(&source, second));
    CHECK(mipsnes2_pcm_resampler_process(
              &resampler, &source, output, 1U) == 1U);
    CHECK(frame_equal(output[0], frame(INT16_C(2000), INT16_C(-2000))));
    CHECK(resampler.phase == 16000U && resampler.has_current &&
          !resampler.has_next);

    CHECK(mipsnes2_pcm_buffer_push(&source, third));
    CHECK(mipsnes2_pcm_resampler_process(
              &resampler, &source, output, 2U) == 2U);
    CHECK(frame_equal(output[0], frame(INT16_C(4000), INT16_C(-4000))));
    CHECK(frame_equal(output[1], third));
    CHECK(mipsnes2_pcm_buffer_push(&source, fourth));
    CHECK(mipsnes2_pcm_resampler_process(
              &resampler, &source, output, 1U) == 1U);
    CHECK(frame_equal(output[0], frame(INT16_C(8000), INT16_C(-8000))));
    CHECK(resampler.source_frames == UINT64_C(4) &&
          resampler.output_frames == UINT64_C(5));
    return 0;
}

static int test_resampler_chunk_equivalence(void)
{
    static const mipsnes2_pcm_frame input[] = {
        {INT16_C(-32768), INT16_C(32767)},
        {INT16_C(-12345), INT16_C(23456)},
        {INT16_C(-1), INT16_C(1)},
        {INT16_C(0), INT16_C(0)},
        {INT16_C(12345), INT16_C(-23456)},
        {INT16_C(32767), INT16_C(-32768)}
    };
    mipsnes2_pcm_buffer one_shot_source = {0};
    mipsnes2_pcm_buffer chunked_source = {0};
    mipsnes2_pcm_resampler one_shot = {0};
    mipsnes2_pcm_resampler chunked = {0};
    mipsnes2_pcm_frame one_shot_storage[6];
    mipsnes2_pcm_frame chunked_storage[6];
    mipsnes2_pcm_frame one_shot_output[16];
    mipsnes2_pcm_frame chunked_output[16];
    size_t one_shot_count;
    size_t chunked_count = 0U;
    size_t i;

    CHECK(mipsnes2_pcm_buffer_init(
              &one_shot_source, one_shot_storage, 6U));
    CHECK(mipsnes2_pcm_buffer_init(
              &chunked_source, chunked_storage, 6U));
    CHECK(mipsnes2_pcm_resampler_init(&one_shot, 48000U));
    CHECK(mipsnes2_pcm_resampler_init(&chunked, 48000U));
    for (i = 0U; i < sizeof(input) / sizeof(input[0]); ++i) {
        CHECK(mipsnes2_pcm_buffer_push(&one_shot_source, input[i]));
        CHECK(mipsnes2_pcm_buffer_push(&chunked_source, input[i]));
    }
    one_shot_count = mipsnes2_pcm_resampler_process(
        &one_shot, &one_shot_source, one_shot_output, 16U);
    for (;;) {
        size_t produced = mipsnes2_pcm_resampler_process(
            &chunked, &chunked_source, &chunked_output[chunked_count], 1U);

        if (produced == 0U) {
            break;
        }
        chunked_count += produced;
        CHECK(chunked_count < 16U);
    }
    CHECK(one_shot_count == 8U && chunked_count == one_shot_count);
    for (i = 0U; i < one_shot_count; ++i) {
        CHECK(frame_equal(one_shot_output[i], chunked_output[i]));
    }
    CHECK(one_shot.phase == chunked.phase &&
          one_shot.has_current == chunked.has_current &&
          one_shot.has_next == chunked.has_next &&
          frame_equal(one_shot.current, chunked.current));
    CHECK(one_shot.source_frames == UINT64_C(6) &&
          one_shot.output_frames == UINT64_C(8));

    mipsnes2_pcm_resampler_reset(&one_shot);
    one_shot.source_frames = UINT64_MAX;
    one_shot.output_frames = UINT64_MAX;
    CHECK(mipsnes2_pcm_buffer_push(
              &one_shot_source, frame(INT16_C(7), INT16_C(-7))));
    CHECK(mipsnes2_pcm_resampler_process(
              &one_shot, &one_shot_source, one_shot_output, 1U) == 1U);
    CHECK(one_shot.source_frames == UINT64_MAX &&
          one_shot.output_frames == UINT64_MAX);
    return 0;
}

static int test_resampler_long_ratio(void)
{
    enum { INPUT_COUNT = 1000, OUTPUT_CAPACITY = 1400 };
    mipsnes2_pcm_buffer source = {0};
    mipsnes2_pcm_resampler resampler = {0};
    mipsnes2_pcm_frame storage[INPUT_COUNT];
    mipsnes2_pcm_frame output[OUTPUT_CAPACITY];
    size_t expected = 1U +
        ((INPUT_COUNT - 1U) * UINT32_C(44100)) / MIPSNES2_PCM_SAMPLE_RATE;
    size_t produced;
    size_t i;

    CHECK(mipsnes2_pcm_buffer_init(&source, storage, INPUT_COUNT));
    CHECK(mipsnes2_pcm_resampler_init(&resampler, UINT32_C(44100)));
    for (i = 0U; i < INPUT_COUNT; ++i) {
        CHECK(mipsnes2_pcm_buffer_push(
            &source, frame((int16_t)((int)i - 500),
                           (int16_t)(500 - (int)i))));
    }
    produced = mipsnes2_pcm_resampler_process(
        &resampler, &source, output, OUTPUT_CAPACITY);
    CHECK(produced == expected);
    CHECK(resampler.source_frames == INPUT_COUNT &&
          resampler.output_frames == expected);
    CHECK(source.count == 0U && resampler.phase < resampler.output_rate);
    CHECK(mipsnes2_pcm_resampler_process(
              &resampler, &source, output, OUTPUT_CAPACITY) == 0U);
    return 0;
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
    CHECK(test_identity_resampler() == 0);
    CHECK(test_ps2_rate_resampler() == 0);
    CHECK(test_resampler_chunk_equivalence() == 0);
    CHECK(test_resampler_long_ratio() == 0);
    (void)puts("bounded PCM FIFO and deterministic resampler: OK");
    return 0;
}
