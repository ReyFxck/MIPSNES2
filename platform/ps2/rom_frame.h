/* Bounded PS2 ROM-frame execution; no SDK calls. See LICENSE. */
#ifndef MIPSNES2_PS2_ROM_FRAME_H
#define MIPSNES2_PS2_ROM_FRAME_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/machine/machine.h"

/* A sequence cap bounds one frame even if a ROM never reaches useful video.
 * A smaller slice cap permits exact continuation through sequence limits. */
mipsnes2_machine_run_result mipsnes2_ps2_rom_frame_run(
    mipsnes2_machine *machine, size_t frame_sequence_cap,
    size_t slice_sequence_cap, size_t event_budget, size_t *sequences,
    mipsnes2_cpu_timing_report *report,
    mipsnes2_cpu_timing_result *timing_result);

bool mipsnes2_ps2_rom_exit_requested(uint16_t snes_word, bool connected);

#endif
