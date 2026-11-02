/* Synthetic RFAuds2 lifecycle fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <rfauds2/rfauds2.h>

#include "platform/ps2/rfauds2_runtime.h"
#include "platform/ps2/audio_diagnostic.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

enum mock_call {
    MOCK_INIT = 1,
    MOCK_LATENCY,
    MOCK_VOLUME,
    MOCK_RESET_STATS,
    MOCK_LAUNCH,
    MOCK_POLL,
    MOCK_START,
    MOCK_STATS,
    MOCK_STOP,
    MOCK_FLUSH,
    MOCK_SHUTDOWN
};

static enum mock_call call_log[64];
static size_t call_count;
static int init_result;
static int latency_result;
static int volume_result;
static int reset_stats_result;
static int launch_result;
static int poll_result;
static int start_result;
static int stats_result;
static int stop_result;
static int flush_result;
static int shutdown_result;
static u32 launched_frames;
static rfauds2_stats mock_stats;
static int inspect_diagnostic_samples;
static unsigned int pause_count;

static void pause_pending(void *context)
{
    (void)context;
    ++pause_count;
}

static void record(enum mock_call call)
{
    if (call_count < sizeof(call_log) / sizeof(call_log[0])) {
        call_log[call_count++] = call;
    }
}

static void reset_mocks(void)
{
    (void)memset(call_log, 0, sizeof(call_log));
    call_count = 0U;
    init_result = 0;
    latency_result = 0;
    volume_result = 0;
    reset_stats_result = 0;
    launch_result = 0;
    poll_result = 1;
    start_result = 0;
    stats_result = 0;
    stop_result = 0;
    flush_result = 0;
    shutdown_result = 0;
    launched_frames = 0U;
    inspect_diagnostic_samples = 0;
    pause_count = 0U;
    (void)memset(&mock_stats, 0, sizeof(mock_stats));
}

int rfauds2_init(const void *irx, u32 irx_size)
{
    record(MOCK_INIT);
    CHECK(irx != NULL && irx_size == 4U);
    return init_result;
}

int rfauds2_set_latency_ms(u32 latency_ms)
{
    record(MOCK_LATENCY);
    CHECK(latency_ms == 43U);
    return latency_result;
}

int rfauds2_set_volume(u32 volume)
{
    record(MOCK_VOLUME);
    CHECK(volume == RFAUDS2_VOLUME_MAX);
    return volume_result;
}

int rfauds2_reset_stats(void)
{
    record(MOCK_RESET_STATS);
    return reset_stats_result;
}

int rfauds2_submit_s16_async(const s16 *samples, u32 frames)
{
    record(MOCK_LAUNCH);
    CHECK(samples != NULL && frames != 0U &&
          frames <= RFAUDS2_ASYNC_MAX_FRAMES);
    launched_frames = frames;
    if (inspect_diagnostic_samples) {
        CHECK(samples[0] == INT16_MIN && samples[1] == INT16_MAX);
        inspect_diagnostic_samples = 0;
    }
    return launch_result;
}

int rfauds2_submit_poll(u32 *accepted_frames)
{
    record(MOCK_POLL);
    if (poll_result == 1) {
        *accepted_frames = launched_frames;
    }
    return poll_result;
}

int rfauds2_start(void)
{
    record(MOCK_START);
    return start_result;
}

int rfauds2_get_stats(rfauds2_stats *stats)
{
    record(MOCK_STATS);
    if (stats_result == 0) {
        *stats = mock_stats;
    }
    return stats_result;
}

int rfauds2_stop(void)
{
    record(MOCK_STOP);
    return stop_result;
}

int rfauds2_flush(void)
{
    record(MOCK_FLUSH);
    return flush_result;
}

int rfauds2_shutdown(void)
{
    record(MOCK_SHUTDOWN);
    return shutdown_result;
}

static mipsnes2_pcm_frame frame(unsigned int index)
{
    mipsnes2_pcm_frame result;

    result.left = (int16_t)(index * 17U);
    result.right = (int16_t)(0U - index * 19U);
    return result;
}

static int fill_source(mipsnes2_pcm_buffer *source,
                       mipsnes2_pcm_frame *storage, size_t count)
{
    size_t i;

    CHECK(mipsnes2_pcm_buffer_init(source, storage, count));
    for (i = 0U; i < count; ++i) {
        CHECK(mipsnes2_pcm_buffer_push(source, frame((unsigned int)i)));
    }
    return 0;
}

static int pump_until_start(
    mipsnes2_ps2_rfauds2_runtime *runtime,
    mipsnes2_pcm_buffer *source,
    mipsnes2_ps2_rfauds2_runtime_report *report)
{
    mipsnes2_ps2_rfauds2_runtime_result result;
    unsigned int calls;

    for (calls = 0U; calls < 8U; ++calls) {
        result = mipsnes2_ps2_rfauds2_runtime_pump(
            runtime, source, report);
        if (result == MIPSNES2_PS2_RFAUDS2_RUNTIME_STARTED ||
            result == MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR ||
            result == MIPSNES2_PS2_RFAUDS2_RUNTIME_CONTRACT_ERROR) {
            return (int)result;
        }
    }
    return -1;
}

static int test_normal_lifecycle(void)
{
    static const unsigned char irx[4] = {1U, 2U, 3U, 4U};
    mipsnes2_ps2_rfauds2_runtime runtime;
    mipsnes2_ps2_rfauds2_runtime_report report;
    mipsnes2_ps2_rfauds2_diagnostics diagnostics;
    mipsnes2_pcm_buffer source;
    mipsnes2_pcm_frame source_storage[1024];
    mipsnes2_pcm_frame staging[RFAUDS2_ASYNC_MAX_FRAMES];

    reset_mocks();
    CHECK(mipsnes2_ps2_rfauds2_runtime_begin(
              &runtime, irx, sizeof(irx), staging,
              RFAUDS2_ASYNC_MAX_FRAMES, 43U, 1024U) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_OK);
    CHECK(call_count == 4U && call_log[0] == MOCK_INIT &&
          call_log[1] == MOCK_LATENCY && call_log[2] == MOCK_VOLUME &&
          call_log[3] == MOCK_RESET_STATS);
    CHECK(fill_source(&source, source_storage, 1024U) == 0);

    CHECK(mipsnes2_ps2_rfauds2_runtime_pump(
              &runtime, &source, &report) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_SUBMITTED);
    CHECK(report.pump.offered_frames == RFAUDS2_ASYNC_MAX_FRAMES);
    CHECK(mipsnes2_ps2_rfauds2_runtime_snapshot(
              &runtime, &diagnostics) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_BUSY);
    CHECK(mipsnes2_ps2_rfauds2_runtime_end(&runtime) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_BUSY);

    CHECK(pump_until_start(&runtime, &source, &report) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_STARTED);
    CHECK(runtime.started && report.started &&
          report.accepted_frames >= 1024U);

    mock_stats.queued_frames = 1408U;
    mock_stats.capacity_frames = 2048U;
    mock_stats.latency_ms = 43U;
    mock_stats.started = 1U;
    mock_stats.min_queued_frames = 512U;
    mock_stats.max_queued_frames = 1535U;
    mock_stats.refill_count = 7U;
    CHECK(mipsnes2_ps2_rfauds2_runtime_snapshot(
              &runtime, &diagnostics) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_OK);
    CHECK(diagnostics.valid && diagnostics.started &&
          diagnostics.queued_frames == 1408U &&
          diagnostics.capacity_frames == 2048U &&
          diagnostics.refill_count == 7U);

    CHECK(mipsnes2_ps2_rfauds2_runtime_end(&runtime) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_OK);
    CHECK(!runtime.device_bound && !runtime.started);
    CHECK(call_log[call_count - 3U] == MOCK_STOP &&
          call_log[call_count - 2U] == MOCK_FLUSH &&
          call_log[call_count - 1U] == MOCK_SHUTDOWN);
    return 0;
}

static int test_begin_and_start_failures(void)
{
    static const unsigned char irx[4] = {1U, 2U, 3U, 4U};
    mipsnes2_ps2_rfauds2_runtime runtime;
    mipsnes2_ps2_rfauds2_runtime_report report;
    mipsnes2_pcm_buffer source;
    mipsnes2_pcm_frame source_storage[1024];
    mipsnes2_pcm_frame staging[RFAUDS2_ASYNC_MAX_FRAMES];

    reset_mocks();
    CHECK(mipsnes2_ps2_rfauds2_runtime_begin(
              &runtime, irx, sizeof(irx), staging,
              RFAUDS2_ASYNC_MAX_FRAMES, 43U, 511U) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_INVALID_ARGUMENT);
    CHECK(call_count == 0U);

    reset_mocks();
    init_result = -9;
    CHECK(mipsnes2_ps2_rfauds2_runtime_begin(
              &runtime, irx, sizeof(irx), staging,
              RFAUDS2_ASYNC_MAX_FRAMES, 43U, 1024U) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR);
    CHECK(!runtime.device_bound && runtime.last_device_result == -9);

    reset_mocks();
    latency_result = -8;
    CHECK(mipsnes2_ps2_rfauds2_runtime_begin(
              &runtime, irx, sizeof(irx), staging,
              RFAUDS2_ASYNC_MAX_FRAMES, 43U, 1024U) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR);
    CHECK(runtime.device_bound && runtime.last_device_result == -8);
    CHECK(mipsnes2_ps2_rfauds2_runtime_end(&runtime) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_OK);

    reset_mocks();
    start_result = -33;
    CHECK(mipsnes2_ps2_rfauds2_runtime_begin(
              &runtime, irx, sizeof(irx), staging,
              RFAUDS2_ASYNC_MAX_FRAMES, 43U, 1024U) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_OK);
    CHECK(fill_source(&source, source_storage, 1024U) == 0);
    CHECK(pump_until_start(&runtime, &source, &report) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR);
    CHECK(!runtime.started && runtime.last_device_result == -33);
    CHECK(fill_source(&source, source_storage, 1024U) == 0);
    CHECK(mipsnes2_ps2_rfauds2_runtime_pump(
              &runtime, &source, &report) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_SUBMITTED);
    CHECK(call_log[call_count - 1U] == MOCK_LAUNCH &&
          runtime.adapter.request_in_flight);
    CHECK(mipsnes2_ps2_rfauds2_runtime_pump(
              &runtime, &source, &report) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR);
    CHECK(call_log[call_count - 1U] == MOCK_START &&
          !runtime.adapter.request_in_flight);
    CHECK(mipsnes2_ps2_rfauds2_runtime_end(&runtime) ==
          MIPSNES2_PS2_RFAUDS2_RUNTIME_OK);
    CHECK(call_log[call_count - 2U] == MOCK_FLUSH &&
          call_log[call_count - 1U] == MOCK_SHUTDOWN);
    return 0;
}

static int test_rom_free_diagnostic(void)
{
    static const unsigned char irx[4] = {1U, 2U, 3U, 4U};
    mipsnes2_ps2_audio_diagnostic_report report;
    mipsnes2_pcm_frame source[MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES];
    mipsnes2_pcm_frame staging[MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES];
    size_t i;

    reset_mocks();
    inspect_diagnostic_samples = 1;
    mock_stats.queued_frames = 1536U;
    mock_stats.started = 1U;
    CHECK(mipsnes2_ps2_audio_diagnostic_run(
        irx, sizeof(irx), source, MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES,
        staging, MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES,
        pause_pending, NULL, &report));
    CHECK(report.begin == MIPSNES2_PS2_RFAUDS2_RUNTIME_OK);
    CHECK(report.pump == MIPSNES2_PS2_RFAUDS2_RUNTIME_STARTED);
    CHECK(report.pump_attempts == 4U && report.report.started);
    CHECK(report.report.accepted_frames == 1920U);
    CHECK(report.snapshot == MIPSNES2_PS2_RFAUDS2_RUNTIME_OK &&
          report.diagnostics.valid && report.diagnostics.queued_frames == 1536U);
    CHECK(report.end == MIPSNES2_PS2_RFAUDS2_RUNTIME_OK &&
          !report.device_bound);
    CHECK(call_log[call_count - 4U] == MOCK_STATS &&
          call_log[call_count - 3U] == MOCK_STOP &&
          call_log[call_count - 2U] == MOCK_FLUSH &&
          call_log[call_count - 1U] == MOCK_SHUTDOWN);

    reset_mocks();
    poll_result = 0;
    CHECK(mipsnes2_ps2_audio_diagnostic_run(
        irx, sizeof(irx), source, MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES,
        staging, MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES,
        pause_pending, NULL, &report));
    CHECK(report.pump_attempts == MIPSNES2_PS2_AUDIO_DIAG_MAX_PUMPS);
    CHECK(report.pump == MIPSNES2_PS2_RFAUDS2_RUNTIME_PENDING);
    CHECK(report.snapshot == MIPSNES2_PS2_RFAUDS2_RUNTIME_BUSY);
    CHECK(report.end == MIPSNES2_PS2_RFAUDS2_RUNTIME_BUSY &&
          report.device_bound && !report.report.started);
    CHECK(pause_count == 7U);
    for (i = 0U; i < call_count; ++i) {
        CHECK(call_log[i] != MOCK_SHUTDOWN && call_log[i] != MOCK_START);
    }

    reset_mocks();
    latency_result = -8;
    CHECK(mipsnes2_ps2_audio_diagnostic_run(
        irx, sizeof(irx), source, MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES,
        staging, MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES,
        pause_pending, NULL, &report));
    CHECK(report.begin == MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR);
    CHECK(report.pump_attempts == 0U &&
          report.snapshot == MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY);
    CHECK(report.end == MIPSNES2_PS2_RFAUDS2_RUNTIME_OK &&
          !report.device_bound && call_log[call_count - 1U] == MOCK_SHUTDOWN);

    reset_mocks();
    CHECK(!mipsnes2_ps2_audio_diagnostic_run(
        irx, sizeof(irx), source, MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES - 1U,
        staging, MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES,
        pause_pending, NULL, &report));
    CHECK(call_count == 0U);
    return 0;
}

int main(void)
{
    CHECK(test_normal_lifecycle() == 0);
    CHECK(test_begin_and_start_failures() == 0);
    CHECK(test_rom_free_diagnostic() == 0);
    (void)puts("PS2 RFAuds2 lifecycle coordinator: OK");
    return 0;
}
