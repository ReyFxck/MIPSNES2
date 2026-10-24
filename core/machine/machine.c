/* MIPSNES2 platform-neutral machine composition. See LICENSE for GPL-3.0 terms. */
#include "core/machine/machine.h"

#include <string.h>

enum { MIPSNES2_OAM_RELOAD_H_CLOCK = 40 };

static mipsnes2_master_clock machine_frame_clocks(const mipsnes2_machine *machine)
{
    mipsnes2_master_clock clocks =
        (mipsnes2_master_clock)mipsnes2_hv_frame_scanlines(&machine->hv) *
        MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;

    if (machine->standard == MIPSNES2_HV_STANDARD_NTSC && machine->hv.field) {
        clocks -= MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS -
                  MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS;
    }
    return clocks;
}

/* The range is prepared for the next visible line at this HBlank boundary.
 * The exact hardware phase of the two status bits remains unverified. */
static void machine_obj_line(void *context, mipsnes2_master_clock timestamp,
                             mipsnes2_scheduler_token token)
{
    mipsnes2_machine *machine = context;
    uint16_t line = machine->hv.v_counter;
    uint16_t vblank = mipsnes2_hv_vblank_start_line(&machine->hv);
    mipsnes2_master_clock delay;

    (void)token;
    if (!machine->ppu_forced_blank && line + 1U < vblank &&
        mipsnes2_ppu_obj_select_scanline(
            &machine->oam_ports, (uint8_t)line, 0U,
            &machine->obj_line)) {
        if (machine->obj_line.range_over) {
            machine->obj_overflow |= UINT8_C(0x40);
        }
        if (machine->obj_line.time_over) {
            machine->obj_overflow |= UINT8_C(0x80);
        }
    }
    delay = line + 2U < vblank
                ? MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS
                : machine_frame_clocks(machine) -
                      (mipsnes2_master_clock)line *
                          MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
    if (timestamp > UINT64_MAX - delay ||
        mipsnes2_scheduler_schedule(&machine->scheduler, timestamp + delay,
                                    0U, machine_obj_line, machine, NULL) !=
            MIPSNES2_SCHEDULER_OK) {
        machine->ready = false;
    }
}

static void machine_obj_frame(void *context, mipsnes2_master_clock timestamp,
                              mipsnes2_scheduler_token token)
{
    mipsnes2_machine *machine = context;
    mipsnes2_master_clock delay =
        (mipsnes2_master_clock)mipsnes2_hv_frame_scanlines(&machine->hv) *
        MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;

    /* At H=0 the field toggle at H=4 has not happened yet. */
    if (machine->standard == MIPSNES2_HV_STANDARD_NTSC &&
        !machine->hv.field) {
        delay -= 4U;
    }

    (void)token;
    machine->obj_overflow = 0U;
    if (timestamp > UINT64_MAX - delay ||
        mipsnes2_scheduler_schedule(&machine->scheduler, timestamp + delay,
                                    0U, machine_obj_frame, machine, NULL) !=
            MIPSNES2_SCHEDULER_OK) {
        machine->ready = false;
    }
}

static void machine_oam_vblank(void *context,
                                mipsnes2_master_clock timestamp,
                                mipsnes2_scheduler_token token)
{
    mipsnes2_machine *machine = context;
    mipsnes2_master_clock frame_clocks = machine_frame_clocks(machine);

    (void)token;
    (void)mipsnes2_ppu_oam_ports_vblank_reload(
        &machine->oam_ports, machine->ppu_forced_blank);
    if (timestamp > UINT64_MAX - frame_clocks) {
        return; /* No representable timestamp for the next frame. */
    }
    if (mipsnes2_scheduler_schedule(&machine->scheduler,
                                    timestamp + frame_clocks, 0U,
                                    machine_oam_vblank, machine, NULL) !=
        MIPSNES2_SCHEDULER_OK) {
        machine->ready = false;
    }
}

static uint8_t machine_ppu_read(void *context, mipsnes2_address address)
{
    mipsnes2_machine *machine = context;
    uint8_t value;

    if ((uint16_t)address == UINT16_C(0x213e)) {
        value = (uint8_t)(machine->obj_overflow |
                          (machine->ppu1_open_bus & UINT8_C(0x20)) | 1U);
        machine->ppu1_open_bus = value;
        return value;
    }

    if (mipsnes2_ppu_oam_ports_read(&machine->oam_ports,
                                   (uint16_t)address, &value)) {
        machine->ppu1_open_bus = value;
        return value;
    }
    if (mipsnes2_ppu_vram_ports_read(&machine->vram_ports,
                                    (uint16_t)address, &value)) {
        machine->ppu1_open_bus = value;
        return value;
    }
    if (mipsnes2_ppu_cgram_ports_read(&machine->cgram_ports,
                                     (uint16_t)address,
                                     machine->counters.open_bus2, &value)) {
        machine->counters.open_bus2 = value;
        return value;
    }
    if (machine->ppu.read8 != NULL) {
        return machine->ppu.read8(machine->ppu.context, address);
    }
    return machine->bus.open_value;
}

static void machine_ppu_write(void *context, mipsnes2_address address,
                              uint8_t value)
{
    mipsnes2_machine *machine = context;

    if ((uint16_t)address == UINT16_C(0x2100)) {
        bool next_forced_blank = (value & UINT8_C(0x80)) != 0U;

        if (machine->ppu_forced_blank && !next_forced_blank &&
            machine->hv.v_counter ==
                mipsnes2_hv_vblank_start_line(&machine->hv)) {
            (void)mipsnes2_ppu_oam_ports_vblank_reload(&machine->oam_ports,
                                                       false);
        }
        machine->ppu_forced_blank = next_forced_blank;
        /* Other display-control behavior remains with the PPU callback. */
    }
    if (mipsnes2_ppu_oam_ports_write(&machine->oam_ports,
                                    (uint16_t)address, value)) {
        return;
    }
    if (mipsnes2_ppu_vram_ports_write(&machine->vram_ports,
                                     (uint16_t)address, value)) {
        return;
    }
    if (mipsnes2_ppu_cgram_ports_write(&machine->cgram_ports,
                                      (uint16_t)address, value)) {
        return;
    }
    if (machine->ppu.write8 != NULL) {
        machine->ppu.write8(machine->ppu.context, address, value);
    }
}

mipsnes2_machine_result mipsnes2_machine_power_on(mipsnes2_machine *machine)
{
    mipsnes2_snes_bus_config config;

    if (machine == NULL || !machine->ready) {
        return MIPSNES2_MACHINE_NOT_READY;
    }
    mipsnes2_cpu_io_init(&machine->io, UINT8_C(2));
    mipsnes2_ppu_counters_init(&machine->counters, UINT8_C(3), 0U);
    machine->ppu_forced_blank = true;
    machine->ppu1_open_bus = 0U;
    machine->obj_overflow = 0U;
    (void)memset(&machine->obj_line, 0, sizeof(machine->obj_line));
    if (machine->vram != NULL &&
        !mipsnes2_ppu_vram_ports_init(&machine->vram_ports,
                                     machine->vram, machine->vram_size)) {
        machine->ready = false;
        return MIPSNES2_MACHINE_INVALID_ARGUMENT;
    }
    if (machine->cgram != NULL &&
        !mipsnes2_ppu_cgram_ports_init(&machine->cgram_ports,
                                      machine->cgram, machine->cgram_size)) {
        machine->ready = false;
        return MIPSNES2_MACHINE_INVALID_ARGUMENT;
    }
    if (machine->oam != NULL &&
        !mipsnes2_ppu_oam_ports_init(&machine->oam_ports,
                                    machine->oam, machine->oam_size)) {
        machine->ready = false;
        return MIPSNES2_MACHINE_INVALID_ARGUMENT;
    }
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
    if (machine->vram != NULL || machine->cgram != NULL ||
        machine->oam != NULL) {
        config.ppu.context = machine;
        config.ppu.read8 = machine_ppu_read;
        config.ppu.write8 = machine_ppu_write;
    } else {
        config.ppu = machine->ppu;
    }
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
    if (machine->oam != NULL &&
        mipsnes2_scheduler_schedule(
            &machine->scheduler,
            (mipsnes2_master_clock)mipsnes2_hv_vblank_start_line(&machine->hv) *
                MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + MIPSNES2_OAM_RELOAD_H_CLOCK,
            0U, machine_oam_vblank, machine, NULL) != MIPSNES2_SCHEDULER_OK) {
        machine->ready = false;
        return MIPSNES2_MACHINE_TIMING_ERROR;
    }
    if (machine->oam != NULL &&
        (mipsnes2_scheduler_schedule(
             &machine->scheduler, MIPSNES2_HV_HBLANK_START_CLOCK, 0U,
             machine_obj_line, machine, NULL) != MIPSNES2_SCHEDULER_OK ||
         mipsnes2_scheduler_schedule(
             &machine->scheduler,
             (mipsnes2_master_clock)mipsnes2_hv_frame_scanlines(&machine->hv) *
                 MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS -
                 (machine->standard == MIPSNES2_HV_STANDARD_NTSC ? 4U : 0U),
             0U, machine_obj_frame, machine, NULL) !=
             MIPSNES2_SCHEDULER_OK)) {
        machine->ready = false;
        return MIPSNES2_MACHINE_TIMING_ERROR;
    }
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
        ((config->vram == NULL && config->vram_size != 0U) ||
         (config->vram != NULL &&
          config->vram_size < MIPSNES2_PPU_VRAM_SIZE)) ||
        ((config->cgram == NULL && config->cgram_size != 0U) ||
         (config->cgram != NULL &&
          config->cgram_size < MIPSNES2_PPU_CGRAM_SIZE)) ||
        ((config->oam == NULL && config->oam_size != 0U) ||
         (config->oam != NULL &&
          config->oam_size < MIPSNES2_PPU_OAM_SIZE)) ||
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
    machine->vram = config->vram;
    machine->vram_size = config->vram_size;
    machine->cgram = config->cgram;
    machine->cgram_size = config->cgram_size;
    machine->oam = config->oam;
    machine->oam_size = config->oam_size;
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
