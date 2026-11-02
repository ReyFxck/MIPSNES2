/* Bounded PS2 ROM-frame execution; no SDK calls. See LICENSE. */
#include "platform/ps2/rom_frame.h"

#include <stdint.h>

#include "core/bus/cpu_io.h"

bool mipsnes2_ps2_rom_exit_requested(uint16_t snes_word, bool connected)
{
    return connected &&
           (snes_word & (MIPSNES2_JOYPAD_START | MIPSNES2_JOYPAD_SELECT)) ==
               (MIPSNES2_JOYPAD_START | MIPSNES2_JOYPAD_SELECT);
}

mipsnes2_machine_run_result mipsnes2_ps2_rom_frame_run(
    mipsnes2_machine *machine, size_t frame_sequence_cap,
    size_t slice_sequence_cap, size_t event_budget, size_t *sequences,
    mipsnes2_cpu_timing_report *report,
    mipsnes2_cpu_timing_result *timing_result)
{
    mipsnes2_master_clock delta;
    mipsnes2_master_clock horizon;

    if (machine == NULL || !machine->ready || sequences == NULL ||
        report == NULL || timing_result == NULL || frame_sequence_cap == 0U ||
        slice_sequence_cap == 0U || event_budget == 0U ||
        !mipsnes2_hv_clocks_until_frame_boundary(&machine->hv, &delta) ||
        delta > UINT64_MAX - machine->scheduler.now) {
        return MIPSNES2_MACHINE_RUN_INVALID_ARGUMENT;
    }
    horizon = machine->scheduler.now + delta;
    *sequences = 0U;
    while (*sequences < frame_sequence_cap) {
        size_t count = 0U;
        size_t limit = frame_sequence_cap - *sequences;
        mipsnes2_machine_run_result result;

        if (limit > slice_sequence_cap) {
            limit = slice_sequence_cap;
        }
        result = mipsnes2_machine_run_until(machine, horizon, limit,
                                            event_budget, &count, report,
                                            timing_result);
        *sequences += count;
        if (result != MIPSNES2_MACHINE_RUN_SEQUENCE_LIMIT) {
            return result;
        }
        /* A sequence-limit return with no completed sequence cannot make
         * progress. Do not spin indefinitely on a corrupt state. */
        if (count == 0U) {
            return MIPSNES2_MACHINE_RUN_TIMING_ERROR;
        }
    }
    return MIPSNES2_MACHINE_RUN_SEQUENCE_LIMIT;
}
