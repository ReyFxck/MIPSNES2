/* Synthetic PS2 audio-adapter fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform/ps2/audio_adapter.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

typedef struct {
    size_t limits[8];
    size_t limit_count;
    size_t calls;
    mipsnes2_pcm_frame accepted[32];
    size_t accepted_count;
} transport_fixture;

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

static size_t submit_prefix(void *context,
                            const mipsnes2_pcm_frame *frames,
                            size_t frame_count)
{
    transport_fixture *fixture = context;
    size_t accepted = frame_count;
    size_t i;

    if (fixture->calls < fixture->limit_count) {
        accepted = fixture->limits[fixture->calls];
    }
    ++fixture->calls;
    if (accepted > frame_count) {
        return accepted;
    }
    for (i = 0U; i < accepted; ++i) {
        fixture->accepted[fixture->accepted_count] = frames[i];
        ++fixture->accepted_count;
    }
    return accepted;
}

int main(void)
{
    mipsnes2_ps2_audio_adapter adapter = {0};
    mipsnes2_ps2_audio_pump_report report;
    mipsnes2_pcm_buffer source = {0};
    mipsnes2_pcm_frame source_storage[8];
    mipsnes2_pcm_frame staging[5];
    transport_fixture transport = {{2U, 0U, 3U}, 3U, 0U, {{0}}, 0U};

    CHECK(MIPSNES2_PS2_AUDIO_OUTPUT_RATE == 48000);
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        NULL, staging, 5U, submit_prefix, &transport));
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        &adapter, NULL, 5U, submit_prefix, &transport));
    CHECK(!adapter.initialized);
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        &adapter, staging, 0U, submit_prefix, &transport));
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        &adapter, staging, 5U, NULL, &transport));
    CHECK(mipsnes2_ps2_audio_adapter_init(
        &adapter, staging, 5U, submit_prefix, &transport));
    CHECK(mipsnes2_pcm_buffer_init(&source, source_storage, 8U));

    (void)memset(&report, 0x5a, sizeof(report));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_STARVED);
    CHECK(report.generated_frames == 0U && report.offered_frames == 0U &&
          report.accepted_frames == 0U && report.pending_frames == 0U);
    CHECK(adapter.starvation_events == UINT64_C(1) && transport.calls == 0U);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, NULL) ==
          MIPSNES2_PS2_AUDIO_PUMP_INVALID_ARGUMENT);

    CHECK(mipsnes2_pcm_buffer_push(&source, frame(0, 0)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(3000, -3000)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(6000, -6000)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(9000, -9000)));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE);
    CHECK(report.generated_frames == 5U && report.offered_frames == 5U &&
          report.accepted_frames == 2U && report.pending_frames == 3U);
    CHECK(source.count == 0U && adapter.pending_offset == 2U &&
          adapter.pending_frames == 3U);
    CHECK(transport.accepted_count == 2U);
    CHECK(frame_equal(transport.accepted[0], frame(0, 0)));
    CHECK(frame_equal(transport.accepted[1], frame(2000, -2000)));

    CHECK(mipsnes2_pcm_buffer_push(&source, frame(12000, -12000)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(15000, -15000)));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE);
    CHECK(report.generated_frames == 0U && report.offered_frames == 3U &&
          report.accepted_frames == 0U && report.pending_frames == 3U);
    CHECK(source.count == 2U && adapter.resampler.source_frames == UINT64_C(4));

    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) == MIPSNES2_PS2_AUDIO_PUMP_OK);
    CHECK(report.generated_frames == 0U && report.offered_frames == 3U &&
          report.accepted_frames == 3U && report.pending_frames == 0U);
    CHECK(source.count == 2U && transport.accepted_count == 5U);
    CHECK(frame_equal(transport.accepted[2], frame(4000, -4000)));
    CHECK(frame_equal(transport.accepted[3], frame(6000, -6000)));
    CHECK(frame_equal(transport.accepted[4], frame(8000, -8000)));

    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) == MIPSNES2_PS2_AUDIO_PUMP_OK);
    CHECK(report.generated_frames == 3U && report.offered_frames == 3U &&
          report.accepted_frames == 3U && report.pending_frames == 0U);
    CHECK(source.count == 0U && transport.accepted_count == 8U);
    CHECK(frame_equal(transport.accepted[5], frame(10000, -10000)));
    CHECK(frame_equal(transport.accepted[6], frame(12000, -12000)));
    CHECK(frame_equal(transport.accepted[7], frame(14000, -14000)));
    CHECK(adapter.generated_frames == UINT64_C(8) &&
          adapter.offered_frames == UINT64_C(14) &&
          adapter.accepted_frames == UINT64_C(8) &&
          adapter.submit_calls == UINT64_C(4) &&
          adapter.backpressure_events == UINT64_C(2));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_STARVED);
    CHECK(adapter.starvation_events == UINT64_C(2));

    mipsnes2_ps2_audio_adapter_reset(&adapter);
    CHECK(adapter.initialized && adapter.staging == staging &&
          adapter.staging_capacity == 5U && adapter.submit == submit_prefix &&
          adapter.submit_context == &transport);
    CHECK(adapter.pending_frames == 0U && adapter.pending_offset == 0U &&
          adapter.generated_frames == 0U && adapter.accepted_frames == 0U &&
          adapter.starvation_events == 0U &&
          adapter.resampler.output_rate == MIPSNES2_PS2_AUDIO_OUTPUT_RATE);

    (void)memset(&transport, 0, sizeof(transport));
    transport.limits[0] = 2U;
    transport.limits[1] = 1U;
    transport.limit_count = 2U;
    mipsnes2_pcm_buffer_reset(&source);
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(7, -7)));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_CONTRACT_ERROR);
    CHECK(report.generated_frames == 1U && report.offered_frames == 1U &&
          report.accepted_frames == 0U && report.pending_frames == 1U);
    CHECK(adapter.contract_errors == UINT64_C(1) &&
          adapter.pending_frames == 1U);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) == MIPSNES2_PS2_AUDIO_PUMP_OK);
    CHECK(report.generated_frames == 0U && report.accepted_frames == 1U &&
          adapter.pending_frames == 0U && transport.accepted_count == 1U);
    CHECK(frame_equal(transport.accepted[0], frame(7, -7)));

    adapter.generated_frames = UINT64_MAX;
    adapter.offered_frames = UINT64_MAX;
    adapter.accepted_frames = UINT64_MAX;
    adapter.submit_calls = UINT64_MAX;
    adapter.starvation_events = UINT64_MAX;
    adapter.backpressure_events = UINT64_MAX;
    adapter.contract_errors = UINT64_MAX;
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_STARVED);
    CHECK(adapter.generated_frames == UINT64_MAX &&
          adapter.offered_frames == UINT64_MAX &&
          adapter.accepted_frames == UINT64_MAX &&
          adapter.submit_calls == UINT64_MAX &&
          adapter.starvation_events == UINT64_MAX &&
          adapter.backpressure_events == UINT64_MAX &&
          adapter.contract_errors == UINT64_MAX);

    mipsnes2_ps2_audio_adapter_reset(NULL);
    (void)puts("PS2 bounded audio adapter: OK");
    return 0;
}
