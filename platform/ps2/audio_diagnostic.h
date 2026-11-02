/* ROM-free PS2 audio transport diagnostic. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PS2_AUDIO_DIAGNOSTIC_H
#define MIPSNES2_PS2_AUDIO_DIAGNOSTIC_H

#include "platform/ps2/rfauds2_runtime.h"

enum {
    MIPSNES2_PS2_AUDIO_DIAG_SOURCE_FRAMES = 2048,
    MIPSNES2_PS2_AUDIO_DIAG_STAGING_FRAMES = 960,
    MIPSNES2_PS2_AUDIO_DIAG_MAX_PUMPS = 32
};

typedef struct {
    mipsnes2_ps2_rfauds2_runtime_result begin;
    mipsnes2_ps2_rfauds2_runtime_result pump;
    mipsnes2_ps2_rfauds2_runtime_result snapshot;
    mipsnes2_ps2_rfauds2_runtime_result end;
    mipsnes2_ps2_rfauds2_runtime_report report;
    mipsnes2_ps2_rfauds2_diagnostics diagnostics;
    unsigned int pump_attempts;
    unsigned int rebind_attempts;
    int initial_device_result;
    int device_result;
    bool device_bound;
} mipsnes2_ps2_audio_diagnostic_report;

/* Caller owns both arrays. Uses synthetic signed stereo input and a bounded
 * number of asynchronous operations. No ROM, machine, clock or heap involved.
 * An optional caller pause after each fourth pending attempt lets target RPC
 * progress without advancing the SNES machine. If the final RPC is still
 * Pass NULL/0 for an IRX already loaded by the caller (with its load result
 * checked); this binds without loading the module again. If the final RPC is
 * pending, end reports BUSY and the device remains
 * bound; caller must retain storage until process exit or a later poll. */
bool mipsnes2_ps2_audio_diagnostic_run(
    const void *irx, uint32_t irx_size,
    mipsnes2_pcm_frame *source_storage, size_t source_capacity,
    mipsnes2_pcm_frame *staging, size_t staging_capacity,
    void (*pause_pending)(void *context), void *pause_context,
    mipsnes2_ps2_audio_diagnostic_report *report);

#endif
