/* Synthetic RFAuds2 binding fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>

#include <rfauds2/rfauds2.h>

#include "platform/ps2/rfauds2_transport.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static int launch_result;
static int poll_results[4];
static u32 poll_accepts[4];
static size_t poll_calls;
static size_t launched_frames;
static s16 launched_samples[16];

int rfauds2_submit_s16_async(const s16 *samples, u32 frames)
{
    u32 i;

    launched_frames = frames;
    for (i = 0U; i < frames * 2U; ++i) {
        launched_samples[i] = samples[i];
    }
    return launch_result;
}

int rfauds2_submit_poll(u32 *accepted_frames)
{
    size_t call = poll_calls++;
    int result = poll_results[call];

    if (result == 1) {
        *accepted_frames = poll_accepts[call];
    }
    return result;
}

static mipsnes2_pcm_frame frame(int16_t left, int16_t right)
{
    mipsnes2_pcm_frame result;

    result.left = left;
    result.right = right;
    return result;
}

int main(void)
{
    mipsnes2_ps2_audio_adapter adapter = {0};
    mipsnes2_ps2_audio_pump_report report;
    mipsnes2_pcm_buffer source = {0};
    mipsnes2_pcm_frame source_storage[4];
    mipsnes2_pcm_frame staging[5];
    mipsnes2_pcm_frame oversized[961];

    CHECK(!mipsnes2_ps2_rfauds2_audio_init(
        &adapter, oversized, 961U));
    CHECK(mipsnes2_ps2_rfauds2_audio_init(&adapter, staging, 5U));
    CHECK(mipsnes2_pcm_buffer_init(&source, source_storage, 4U));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(0, 0)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(3000, -3000)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(6000, -6000)));
    CHECK(mipsnes2_pcm_buffer_push(&source, frame(9000, -9000)));

    poll_results[0] = 0;
    poll_results[1] = 1;
    poll_accepts[1] = 2U;
    poll_results[2] = 1;
    poll_accepts[2] = 3U;
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(launched_frames == 5U && launched_samples[0] == 0 &&
          launched_samples[2] == 2000 && launched_samples[3] == -2000);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_PENDING);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_BACKPRESSURE);
    CHECK(report.accepted_frames == 2U && report.pending_frames == 3U);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_SUBMITTED);
    CHECK(launched_frames == 3U && launched_samples[0] == 4000 &&
          launched_samples[1] == -4000);
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_OK);
    CHECK(report.accepted_frames == 3U && adapter.pending_frames == 0U);

    CHECK(mipsnes2_pcm_buffer_push(&source, frame(12, -12)));
    launch_result = -4;
    CHECK(mipsnes2_ps2_audio_adapter_pump(
              &adapter, &source, &report) ==
          MIPSNES2_PS2_AUDIO_PUMP_TRANSPORT_ERROR);
    CHECK(report.transport_result == -4 && adapter.pending_frames == 2U &&
          !adapter.request_in_flight);

    (void)puts("PS2 RFAuds2 asynchronous binding: OK");
    return 0;
}
