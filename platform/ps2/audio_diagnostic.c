/* ROM-free PS2 audio transport diagnostic. See LICENSE for GPL-3.0 terms. */
#include "platform/ps2/audio_diagnostic.h"

#include <string.h>

/* Exact pinned RFAuds2 0.1.0 client returns this after 5,000 bind attempts
 * without a visible RPC server. It has already loaded the IRX at this point. */
enum { RFAUDS2_BIND_TIMEOUT = -2000, AUDIO_DIAG_REBINDS = 2 };

bool mipsnes2_ps2_audio_diagnostic_run(
    const void *irx, uint32_t irx_size,
    mipsnes2_pcm_frame *source_storage, size_t source_capacity,
    mipsnes2_pcm_frame *staging, size_t staging_capacity,
    void (*pause_pending)(void *context), void *pause_context,
    mipsnes2_ps2_audio_diagnostic_report *report)
{
    mipsnes2_ps2_rfauds2_runtime runtime;
    mipsnes2_pcm_buffer source;
    unsigned int index;

    if (report == NULL || source_storage == NULL ||
        source_capacity < MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES ||
        staging == NULL ||
        staging_capacity < MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES) {
        return false;
    }
    (void)memset(report, 0, sizeof(*report));
    report->pump = MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY;
    report->snapshot = MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY;
    report->end = MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY;
    if (!mipsnes2_pcm_buffer_init(
            &source, source_storage,
            MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES)) {
        return false;
    }
    /* Fixed nonzero waveform also reaches the signed extrema. The transport
     * fixture checks lifecycle, not sound quality or S-DSP output. */
    for (index = 0U; index < MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES;
         ++index) {
        mipsnes2_pcm_frame sample;

        sample.left = (index & 1U) != 0U ? INT16_MAX : INT16_MIN;
        sample.right = (index & 1U) != 0U ? INT16_MIN : INT16_MAX;
        if (!mipsnes2_pcm_buffer_push(&source, sample)) {
            return false;
        }
    }

    report->begin = mipsnes2_ps2_rfauds2_runtime_begin(
        &runtime, irx, irx_size, staging,
        MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES, 43U, 1536U);
    report->initial_device_result = runtime.last_device_result;
    if (report->begin == MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR &&
        report->initial_device_result == RFAUDS2_BIND_TIMEOUT) {
        for (index = 0U; index < AUDIO_DIAG_REBINDS; ++index) {
            if (pause_pending != NULL) {
                pause_pending(pause_context);
            }
            ++report->rebind_attempts;
            report->begin = mipsnes2_ps2_rfauds2_runtime_bind_loaded(
                &runtime, staging, MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES,
                43U, 1536U);
            if (report->begin == MIPSNES2_PS2_RFAUDS2_RUNTIME_OK ||
                runtime.last_device_result != RFAUDS2_BIND_TIMEOUT) {
                break;
            }
        }
    }
    if (report->begin == MIPSNES2_PS2_RFAUDS2_RUNTIME_OK) {
        for (index = 0U; index < MIPSNES2_PS2_AUDIO_DIAG_MAX_PUMPS;
             ++index) {
            report->pump = mipsnes2_ps2_rfauds2_runtime_pump(
                &runtime, &source, &report->report);
            ++report->pump_attempts;
            if (report->pump == MIPSNES2_PS2_RFAUDS2_RUNTIME_PENDING &&
                report->pump_attempts % 4U == 0U &&
                report->pump_attempts < MIPSNES2_PS2_AUDIO_DIAG_MAX_PUMPS &&
                pause_pending != NULL) {
                pause_pending(pause_context);
            }
            if (report->pump == MIPSNES2_PS2_RFAUDS2_RUNTIME_STARTED ||
                report->pump == MIPSNES2_PS2_RFAUDS2_RUNTIME_DEVICE_ERROR ||
                report->pump == MIPSNES2_PS2_RFAUDS2_RUNTIME_CONTRACT_ERROR ||
                report->pump == MIPSNES2_PS2_RFAUDS2_RUNTIME_NOT_READY) {
                break;
            }
        }
        report->snapshot = mipsnes2_ps2_rfauds2_runtime_snapshot(
            &runtime, &report->diagnostics);
    }
    report->end = mipsnes2_ps2_rfauds2_runtime_end(&runtime);
    report->device_bound = runtime.device_bound;
    report->device_result = runtime.last_device_result;
    return true;
}
