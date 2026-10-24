/* MIPSNES2 platform-neutral machine composition. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_MACHINE_H
#define MIPSNES2_MACHINE_H

#include "core/cartridge/prepare.h"
#include "core/scheduler/cpu_timing.h"

typedef enum {
    MIPSNES2_MACHINE_OK = 0,
    MIPSNES2_MACHINE_INVALID_ARGUMENT,
    MIPSNES2_MACHINE_IMAGE_ERROR,
    MIPSNES2_MACHINE_CARTRIDGE_ERROR,
    MIPSNES2_MACHINE_TIMING_ERROR,
    MIPSNES2_MACHINE_BUS_ERROR,
    MIPSNES2_MACHINE_NOT_READY
} mipsnes2_machine_result;

typedef struct {
    const uint8_t *image;
    size_t image_size;
    mipsnes2_copier_header_policy copier_policy;
    uint8_t *wram;
    size_t wram_size;
    uint8_t *sram;
    size_t sram_capacity;
    mipsnes2_hv_standard standard;
    bool overscan;
    mipsnes2_bus_interface ppu;
    mipsnes2_bus_interface apu;
    mipsnes2_bus_interface dma;
} mipsnes2_machine_config;

/* Stable address required: bus and CPU callbacks point into this object.
 * Image, WRAM and SRAM remain caller-owned for the entire machine lifetime.
 * Initialize in place; do not copy or move an initialized machine.
 */
typedef struct {
    mipsnes2_cartridge cartridge;
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv;
    mipsnes2_interrupt_lines interrupts;
    mipsnes2_cpu_io io;
    mipsnes2_ppu_counters counters;
    mipsnes2_cpu_timing_state cursor;
    mipsnes2_bus_interface ppu;
    mipsnes2_bus_interface apu;
    mipsnes2_bus_interface dma;
    uint8_t *wram;
    size_t wram_size;
    mipsnes2_hv_standard standard;
    bool overscan;
    bool ready;
} mipsnes2_machine;

/* On failure machine is not ready. No image or SRAM bytes are modified.
 * The caller must supply at least 128 KiB WRAM and header-declared SRAM.
 */
mipsnes2_machine_result mipsnes2_machine_init(
    mipsnes2_machine *machine, const mipsnes2_machine_config *config);
/* Power cycle the CPU, I/O, counters, clock and event queue. WRAM and SRAM
 * remain caller-owned and retain their bytes. Pending events are discarded.
 * The timed RESET vector sequence is then performed by resume().
 */
mipsnes2_machine_result mipsnes2_machine_power_on(mipsnes2_machine *machine);
/* One RESET sequence or CPU instruction, resumable at an absolute horizon.
 * HORIZON_REACHED and event budget exhaustion preserve the active cursor.
 * Inspect report->cpu_result after OK for WAI/STP or other CPU status.
 */
mipsnes2_cpu_timing_result mipsnes2_machine_resume(
    mipsnes2_machine *machine, mipsnes2_master_clock horizon,
    size_t event_budget, mipsnes2_cpu_timing_report *report);

#endif
