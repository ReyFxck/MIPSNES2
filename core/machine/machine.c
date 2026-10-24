/* MIPSNES2 platform-neutral machine composition. See LICENSE for GPL-3.0 terms. */
#include "core/machine/machine.h"

#include <string.h>

mipsnes2_machine_result mipsnes2_machine_power_on(mipsnes2_machine *machine)
{
    mipsnes2_snes_bus_config config;

    if (machine == NULL || !machine->ready) {
        return MIPSNES2_MACHINE_NOT_READY;
    }
    mipsnes2_cpu_io_init(&machine->io, UINT8_C(2));
    mipsnes2_ppu_counters_init(&machine->counters, UINT8_C(3), 0U);
    if (mipsnes2_hv_timing_init(&machine->hv, machine->standard,
                               machine->overscan, &machine->io) != MIPSNES2_HV_OK) {
        machine->ready = false;
        return MIPSNES2_MACHINE_TIMING_ERROR;
    }
    (void)memset(&config, 0, sizeof(config));
    config.wram = machine->wram;
    config.wram_size = machine->wram_size;
    config.cpu_io_state = &machine->io;
    config.ppu_counters = &machine->counters;
    config.hv_timing = &machine->hv;
    config.ppu = machine->ppu;
    config.apu = machine->apu;
    config.dma = machine->dma;
    config.cartridge = mipsnes2_cartridge_make_interface(&machine->cartridge);
    if (mipsnes2_snes_bus_init(&machine->bus, &config) != MIPSNES2_SNES_BUS_OK) {
        machine->ready = false;
        return MIPSNES2_MACHINE_BUS_ERROR;
    }
    mipsnes2_cpu_init(&machine->cpu,
                      mipsnes2_snes_bus_make_interface(&machine->bus));
    mipsnes2_scheduler_init(&machine->scheduler);
    mipsnes2_interrupt_lines_init(&machine->interrupts);
    mipsnes2_cpu_timing_state_init(&machine->cursor);
    if (mipsnes2_cpu_timing_state_begin_reset(&machine->cursor) !=
        MIPSNES2_CPU_TIMING_OK) {
        machine->ready = false;
        return MIPSNES2_MACHINE_TIMING_ERROR;
    }
    return MIPSNES2_MACHINE_OK;
}

mipsnes2_machine_result mipsnes2_machine_init(
    mipsnes2_machine *machine, const mipsnes2_machine_config *config)
{
    mipsnes2_cartridge_plan plan;
    mipsnes2_cartridge cartridge;

    if (machine == NULL || config == NULL) {
        return MIPSNES2_MACHINE_INVALID_ARGUMENT;
    }
    /* Invalidate the previous instance even if this new image is rejected. */
    machine->ready = false;
    if (config->image == NULL || config->wram == NULL ||
        config->wram_size < MIPSNES2_SNES_WRAM_SIZE ||
        (config->sram_capacity != 0U && config->sram == NULL) ||
        (config->standard != MIPSNES2_HV_STANDARD_NTSC &&
         config->standard != MIPSNES2_HV_STANDARD_PAL)) {
        return MIPSNES2_MACHINE_INVALID_ARGUMENT;
    }
    if (mipsnes2_cartridge_plan_create(&plan, config->image,
                                      config->image_size, config->copier_policy) !=
        MIPSNES2_CARTRIDGE_IMAGE_OK) {
        return MIPSNES2_MACHINE_IMAGE_ERROR;
    }
    if (mipsnes2_cartridge_construct(&cartridge, &plan, config->sram,
                                     config->sram_capacity) !=
        MIPSNES2_CARTRIDGE_OK) {
        return MIPSNES2_MACHINE_CARTRIDGE_ERROR;
    }
    (void)memset(machine, 0, sizeof(*machine));
    machine->cartridge = cartridge;
    machine->wram = config->wram;
    machine->wram_size = config->wram_size;
    machine->standard = config->standard;
    machine->overscan = config->overscan;
    machine->ppu = config->ppu;
    machine->apu = config->apu;
    machine->dma = config->dma;
    machine->ready = true;
    return mipsnes2_machine_power_on(machine);
}

mipsnes2_cpu_timing_result mipsnes2_machine_resume(
    mipsnes2_machine *machine, mipsnes2_master_clock horizon,
    size_t event_budget, mipsnes2_cpu_timing_report *report)
{
    if (machine == NULL || !machine->ready || report == NULL) {
        return MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    }
    return mipsnes2_cpu_timed_resume_hv_interrupts(
        &machine->cpu, &machine->bus, &machine->scheduler, &machine->hv,
        &machine->interrupts, &machine->cursor, horizon, event_budget, report);
}

mipsnes2_machine_run_result mipsnes2_machine_run_until(
    mipsnes2_machine *machine, mipsnes2_master_clock horizon,
    size_t sequence_limit, size_t event_budget,
    size_t *sequences, mipsnes2_cpu_timing_report *report,
    mipsnes2_cpu_timing_result *timing_result)
{
    size_t completed;

    if (machine == NULL || !machine->ready || sequences == NULL ||
        report == NULL || timing_result == NULL || sequence_limit == 0U ||
        horizon < machine->scheduler.now) {
        return MIPSNES2_MACHINE_RUN_INVALID_ARGUMENT;
    }
    *sequences = 0U;
    for (completed = 0U; completed < sequence_limit; ++completed) {
        *timing_result = mipsnes2_machine_resume(
            machine, horizon, event_budget, report);
        if (*timing_result == MIPSNES2_CPU_TIMING_HORIZON_REACHED) {
            return MIPSNES2_MACHINE_RUN_HORIZON;
        }
        if (*timing_result != MIPSNES2_CPU_TIMING_OK) {
            return MIPSNES2_MACHINE_RUN_TIMING_ERROR;
        }
        ++*sequences;
        if (report->cpu_result != MIPSNES2_CPU_OK) {
            return MIPSNES2_MACHINE_RUN_CPU_STOP;
        }
    }
    return MIPSNES2_MACHINE_RUN_SEQUENCE_LIMIT;
}
