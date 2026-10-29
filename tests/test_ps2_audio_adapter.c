/* Synthetic PS2 audio-adapter fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform/ps2/audio_adapter.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

typedef struct {
    int launch_results[8];
    size_t launch_result_count;
    size_t launch_calls;
    int poll_results[12];
    size_t poll_accepts[12];
    size_t poll_result_count;
    size_t poll_calls;
    mipsnes2_pcm_frame offered[48];
    size_t offered_count;
    size_t offer_sizes[8];
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

static int launch_async(void *context,
                        const mipsnes2_pcm_frame *frames,
                        size_t frame_count)
{
    transport_fixture *fixture = context;
    size_t call = fixture->launch_calls;
    size_t i;
    int result = 0;

    if (call < fixture->launch_result_count) {
        result = fixture->launch_results[call];
    }
    fixture->offer_sizes[call] = frame_count;
    ++fixture->launch_calls;
    for (i = 0U; i < frame_count; ++i) {
        fixture->offered[fixture->offered_count++] = frames[i];
    }
    return result;
}

static int poll_async(void *context, size_t *accepted_frames)
{
    transport_fixture *fixture = context;
    size_t call = fixture->poll_calls;
    int result = 0;

    if (call < fixture->poll_result_count) {
        result = fixture->poll_results[call];
        if (result == 1) {
            *accepted_frames = fixture->poll_accepts[call];
        }
    }
    ++fixture->poll_calls;
    return result;
}

int main(void)
{
    mipsnes2_ps2_audio_adapter adapter = {0};
    mipsnes2_ps2_audio_pump_report report;
    mipsnes2_pcm_buffer source = {0};
    mipsnes2_pcm_frame source_storage[8];
    mipsnes2_pcm_frame staging[5];
    transport_fixture transport = {0};

    transport.poll_results[0] = 0;
    transport.poll_results[1] = 1;
    transport.poll_accepts[1] = 2U;
    transport.poll_results[2] = 1;
    transport.poll_accepts[2] = 0U;
    transport.poll_results[3] = 1;
    transport.poll_accepts[3] = 3U;
    transport.poll_results[4] = 1;
    transport.poll_accepts[4] = 3U;
    transport.poll_result_count = 5U;

    CHECK(MIPSNES2_PS2_AUDIO_OUTPUT_RATE == 48000);
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        NULL, staging, 5U, launch_async, poll_async, &transport));
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        &adapter, NULL, 5U, launch_async, poll_async, &transport));
    CHECK(!adapter.initialized);
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        &adapter, staging, 0U, launch_async, poll_async, &transport));
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        &adapter, staging, 5U, NULL, poll_async, &transport));
    CHECK(!mipsnes2_ps2_audio_adapter_init(
        &adapter, staging, 5U, launch_async, NULL, &transport));
    CHECK(mipsnes2_ps2_audio_adapter_init(
        &adapter, staging, 5U, launch_async, poll_async, &transport));
    CHECK(mipsnes2_pcm_buffer_init(&source, source_storage, 8U));

    (void)memset(&report, 0x5a, sizeof(report));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_STARVED);
    CHECK(report.generated_frames == 0U && report.offered_frames == 0U &&
          report.accepted_frames == 0U && report.pending_frames == 0U &&
          report.transport_result == 0);
    CHECK(adapter.starvation_events == UINT64_C(1) &&
          transport.launch_calls == 0U && transport.poll_calls == 0U);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, NULL) ==
          MIPSNES2_PS2_AUDIO_PUMP_INVALID_ARGUMENT);

    CHECK(mipsnes2_pcm_buffer_push(&source, frame(0, 0)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(3000, -3000)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(6000, -6000)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(9000, -9000)));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(report.generated_frames == 5U && report.offered_frames == 5U &&
          report.accepted_frames == 0U && report.pending_frames == 5U);
    CHECK(source.count == 0U && adapter.request_in_flight &&
          adapter.in_flight_frames == 5U);
    CHECK(frame_equal(transport.offered[0], frame(0, 0)));
    CHECK(frame_equal(transport.offered[1], frame(2000, -2000)));

    CHECK(mipsnes2_pcm_buffer_push(&source, frame(12000, -12000)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(15000, -15000)));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_PENDING);
    CHECK(report.generated_frames == 0U && report.offered_frames == 0U &&
          report.accepted_frames == 0U && report.pending_frames == 5U);
    CHECK(source.count == 2U && adapter.resampler.source_frames == UINT64_C(4));

    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE);
    CHECK(report.accepted_frames == 2U && report.pending_frames == 3U &&
          !adapter.request_in_flight && source.count == 2U);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(report.generated_frames == 0U && report.offered_frames == 3U &&
          report.pending_frames == 3U && source.count == 2U);
    CHECK(frame_equal(transport.offered[5], frame(4000, -4000)));
    CHECK(frame_equal(transport.offered[6], frame(6000, -6000)));
    CHECK(frame_equal(transport.offered[7], frame(8000, -8000)));
    CHECK(!mipsnes2_ps2_audio_adapter_reset(&adapter));

    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE);
    CHECK(report.accepted_frames == 0U && report.pending_frames == 3U);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_OK);
    CHECK(report.accepted_frames == 3U && report.pending_frames == 0U);

    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(report.generated_frames == 3U && report.offered_frames == 3U &&
          report.pending_frames == 3U && source.count == 0U);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_OK);
    CHECK(report.accepted_frames == 3U && report.pending_frames == 0U);
    CHECK(adapter.generated_frames == UINT64_C(8) &&
          adapter.offered_frames == UINT64_C(14) &&
          adapter.accepted_frames == UINT64_C(8) &&
          adapter.launch_calls == UINT64_C(4) &&
          adapter.poll_calls == UINT64_C(5) &&
          adapter.pending_events == UINT64_C(1) &&
          adapter.backpressure_events == UINT64_C(2));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_STARVED);
    CHECK(adapter.starvation_events == UINT64_C(2));

    CHECK(mipsnes2_ps2_audio_adapter_reset(&adapter));
    CHECK(adapter.initialized && adapter.staging == staging &&
          adapter.staging_capacity == 5U && adapter.launch == launch_async &&
          adapter.poll == poll_async &&
          adapter.transport_context == &transport);
    CHECK(adapter.pending_frames == 0U && adapter.pending_offset == 0U &&
          adapter.generated_frames == 0U && adapter.accepted_frames == 0U &&
          adapter.starvation_events == 0U &&
          adapter.resampler.output_rate == MIPSNES2_PS2_AUDIO_OUTPUT_RATE);

    (void)memset(&transport, 0, sizeof(transport));
    transport.launch_results[0] = -4;
    transport.launch_result_count = 1U;
    transport.poll_results[0] = -9;
    transport.poll_results[1] = 1;
    transport.poll_accepts[1] = 2U;
    transport.poll_results[2] = 1;
    transport.poll_accepts[2] = 1U;
    transport.poll_result_count = 3U;
    mipsnes2_pcm_buffer_reset(&source);
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(7, -7)));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_ERROR);
    CHECK(report.transport_result == -4 && adapter.pending_frames == 1U &&
          !adapter.request_in_flight);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(!mipsnes2_ps2_audio_adapter_reset(&adapter));
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_ERROR);
    CHECK(report.transport_result == -9 && adapter.pending_frames == 1U &&
          !adapter.request_in_flight);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_CONTRACT_ERROR);
    CHECK(adapter.pending_frames == 1U && !adapter.request_in_flight);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_OK);
    CHECK(frame_equal(staging[0], frame(7, -7)));

    adapter.generated_frames = UINT64_MAX;
    adapter.offered_frames = UINT64_MAX;
    adapter.accepted_frames = UINT64_MAX;
    adapter.launch_calls = UINT64_MAX;
    adapter.poll_calls = UINT64_MAX;
    adapter.starvation_events = UINT64_MAX;
    adapter.pending_events = UINT64_MAX;
    adapter.backpressure_events = UINT64_MAX;
    adapter.transport_errors = UINT64_MAX;
    adapter.contract_errors = UINT64_MAX;
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_STARVED);
    CHECK(adapter.generated_frames == UINT64_MAX &&
          adapter.offered_frames == UINT64_MAX &&
          adapter.accepted_frames == UINT64_MAX &&
          adapter.launch_calls == UINT64_MAX &&
          adapter.poll_calls == UINT64_MAX &&
          adapter.starvation_events == UINT64_MAX &&
          adapter.pending_events == UINT64_MAX &&
          adapter.backpressure_events == UINT64_MAX &&
          adapter.transport_errors == UINT64_MAX &&
          adapter.contract_errors == UINT64_MAX);

    CHECK(!mipsnes2_ps2_audio_adapter_reset(NULL));
    (void)puts("PS2 asynchronous bounded audio adapter: OK");
    return 0;
}
