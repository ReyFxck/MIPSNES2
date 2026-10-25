/* MIPSNES2 platform-neutral machine composition. See LICENSE for GPL-3.0 terms. */
#include "core/machine/machine.h"
#include "core/ppu/bg4_scanline.h"
#include "core/ppu/mode1_mix.h"

#include <string.h>

enum { MIPSNES2_OAM_RELOAD_H_CLOCK = 40 };
enum { MIPSNES2_HDMA_FRAME_H_CLOCK = 24,
       MIPSNES2_HDMA_LINE_H_CLOCK = 1112 };

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

/* Request ownership only. Device effects run after the current CPU/DMA slot. */
static void machine_hdma_frame(void *context,
                               mipsnes2_master_clock timestamp,
                               mipsnes2_scheduler_token token)
{
    mipsnes2_machine *machine = context;
    mipsnes2_master_clock delay = machine_frame_clocks(machine);

    (void)token;
    machine->bus.hdma_frame_pending = true;
    if (timestamp > UINT64_MAX - delay ||
        mipsnes2_scheduler_schedule(&machine->scheduler, timestamp + delay,
                                    0U, machine_hdma_frame, machine, NULL) !=
            MIPSNES2_SCHEDULER_OK) {
        machine->ready = false;
    }
}

static void machine_hdma_line(void *context,
                              mipsnes2_master_clock timestamp,
                              mipsnes2_scheduler_token token)
{
    mipsnes2_machine *machine = context;
    uint16_t line = machine->hv.v_counter;
    uint16_t vblank = mipsnes2_hv_vblank_start_line(&machine->hv);
    mipsnes2_master_clock delay = line + 1U < vblank
        ? MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS
        : machine_frame_clocks(machine) -
              (mipsnes2_master_clock)line *
                  MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;

    (void)token;
    if ((machine->bus.hdma_enable_mask &
         (uint8_t)~machine->hdma_state.terminated_mask) != 0U) {
        machine->bus.hdma_line_pending = true;
    }
    if (timestamp > UINT64_MAX - delay ||
        mipsnes2_scheduler_schedule(&machine->scheduler, timestamp + delay,
                                    0U, machine_hdma_line, machine, NULL) !=
            MIPSNES2_SCHEDULER_OK) {
        machine->ready = false;
    }
}

/* Start recurring HDMA events only after software enables a channel. A late
 * enable first uses caller-programmed A2A/NLTR; frame reload waits for V=0. */
static void machine_hdma_enable_changed(void *context, uint8_t mask)
{
    mipsnes2_machine *machine = context;
    const mipsnes2_hv_timing *hv = &machine->hv;
    bool frame_field = hv->field;
    mipsnes2_master_clock frame_clocks;
    mipsnes2_master_clock position;
    mipsnes2_master_clock frame_delay;
    mipsnes2_master_clock line_delay;
    mipsnes2_master_clock now = machine->scheduler.now;
    uint16_t vblank = mipsnes2_hv_vblank_start_line(hv);

    if (mask == 0U || machine->hdma_events_scheduled) {
        return;
    }
    if (hv->v_counter == 0U &&
        hv->h_master_clock < MIPSNES2_HV_HBLANK_END_CLOCK) {
        frame_field = !frame_field;
    }
    frame_clocks = (mipsnes2_master_clock)mipsnes2_hv_frame_scanlines(hv) *
                   MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
    if (machine->standard == MIPSNES2_HV_STANDARD_NTSC && frame_field) {
        frame_clocks -= 4U;
    }
    position = (mipsnes2_master_clock)hv->v_counter *
               MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + hv->h_master_clock;
    if (machine->standard == MIPSNES2_HV_STANDARD_NTSC && frame_field &&
        hv->v_counter > 240U) {
        position -= 4U;
    }
    frame_delay = hv->v_counter == 0U &&
                  hv->h_master_clock < MIPSNES2_HDMA_FRAME_H_CLOCK
        ? (mipsnes2_master_clock)(MIPSNES2_HDMA_FRAME_H_CLOCK -
                                   hv->h_master_clock)
        : frame_clocks - position + MIPSNES2_HDMA_FRAME_H_CLOCK;
    if (hv->v_counter < vblank &&
        hv->h_master_clock < MIPSNES2_HDMA_LINE_H_CLOCK) {
        line_delay = MIPSNES2_HDMA_LINE_H_CLOCK - hv->h_master_clock;
    } else if (hv->v_counter + 1U < vblank) {
        line_delay = MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS -
                     hv->h_master_clock + MIPSNES2_HDMA_LINE_H_CLOCK;
    } else {
        line_delay = frame_clocks - position +
                     MIPSNES2_HDMA_LINE_H_CLOCK;
    }
    if (now > UINT64_MAX - frame_delay ||
        now > UINT64_MAX - line_delay ||
        mipsnes2_scheduler_schedule(&machine->scheduler, now + frame_delay,
                                    0U, machine_hdma_frame, machine, NULL) !=
            MIPSNES2_SCHEDULER_OK ||
        mipsnes2_scheduler_schedule(&machine->scheduler, now + line_delay,
                                    0U, machine_hdma_line, machine, NULL) !=
            MIPSNES2_SCHEDULER_OK) {
        machine->ready = false;
        return;
    }
    machine->hdma_events_scheduled = true;
}

/* The range is prepared for the next visible line at this HBlank boundary.
 * The exact hardware phase of the two status bits remains unverified. */
static void machine_ppu_line(void *context, mipsnes2_master_clock timestamp,
                             mipsnes2_scheduler_token token)
{
    mipsnes2_machine *machine = context;
    uint16_t line = machine->hv.v_counter;
    uint16_t vblank = mipsnes2_hv_vblank_start_line(&machine->hv);
    mipsnes2_master_clock delay;
    uint8_t first_sprite;
    mipsnes2_ppu_bg2_config bg_config;
    const mipsnes2_ppu_bg2_scanline *bg_lines[4];
    unsigned bg;

    (void)token;
    (void)memset(&machine->obj_line, 0, sizeof(machine->obj_line));
    (void)memset(&machine->obj_pixels, 0, sizeof(machine->obj_pixels));
    (void)memset(&machine->bg1_line, 0, sizeof(machine->bg1_line));
    (void)memset(&machine->bg_other_lines, 0,
                 sizeof(machine->bg_other_lines));
    (void)memset(&machine->mode0_line, 0, sizeof(machine->mode0_line));
    (void)memset(&machine->mode0_raw_line, 0,
                 sizeof(machine->mode0_raw_line));
    (void)memset(&machine->mode0_color_line, 0,
                 sizeof(machine->mode0_color_line));
    (void)memset(&machine->sub_line, 0, sizeof(machine->sub_line));
    (void)memset(&machine->sub_color_line, 0,
                 sizeof(machine->sub_color_line));
    (void)memset(machine->color_window_inside, 0,
                 sizeof(machine->color_window_inside));
    if (!machine->ppu_forced_blank && machine->vram != NULL &&
        line + 1U < vblank) {
        unsigned mode = machine->bg_registers.mode & 7U;

        for (bg = 0U; bg < (mode == 0U ? 4U : 3U); ++bg) {
            mipsnes2_ppu_bg2_scanline *target = bg == 0U
                ? &machine->bg1_line : &machine->bg_other_lines[bg - 1U];

            if (((machine->main_screen_enable | machine->sub_screen_enable) &
                 (1U << bg)) != 0U &&
                ((mode == 0U && mipsnes2_ppu_bg_mode0_config(
                    &machine->bg_registers, bg, &bg_config)) ||
                 (mode == 1U && mipsnes2_ppu_bg_mode1_config(
                    &machine->bg_registers, bg, &bg_config)))) {
                if (mode == 1U && bg < 2U) {
                    (void)mipsnes2_ppu_bg4_render_scanline(
                        machine->vram, machine->vram_size, &bg_config,
                        line, target);
                } else {
                    (void)mipsnes2_ppu_bg2_render_scanline(
                        machine->vram, machine->vram_size, &bg_config,
                        line, target);
                }
            }
        }
    }
    if (!machine->ppu_forced_blank && line + 1U < vblank &&
        mipsnes2_ppu_oam_first_sprite(&machine->oam_ports, &first_sprite) &&
        mipsnes2_ppu_obj_select_scanline(
            &machine->oam_ports, (uint8_t)line, first_sprite,
            &machine->obj_line)) {
        if (machine->obj_line.range_over) {
            machine->obj_overflow |= UINT8_C(0x40);
        }
        if (machine->obj_line.time_over) {
            machine->obj_overflow |= UINT8_C(0x80);
        }
        if (machine->vram != NULL) {
            (void)mipsnes2_ppu_obj_compose_scanline(
                &machine->oam_ports, machine->vram, machine->vram_size,
                (uint8_t)line, &machine->obj_line, &machine->obj_pixels);
        }
    }
    if (!machine->ppu_forced_blank && machine->vram != NULL &&
        line + 1U < vblank && (machine->bg_registers.mode & 7U) <= 1U) {
        bg_lines[0] = &machine->bg1_line;
        for (bg = 1U; bg < 4U; ++bg) {
            bg_lines[bg] = &machine->bg_other_lines[bg - 1U];
        }
        (void)mipsnes2_ppu_window_mask_line(
            &machine->windows, machine->window_hidden);
        if ((machine->bg_registers.mode & 7U) == 0U) {
            (void)mipsnes2_ppu_mode0_mix_masked(
                bg_lines, &machine->obj_pixels,
                machine->main_screen_enable, machine->window_hidden,
                &machine->mode0_line);
        } else {
            (void)mipsnes2_ppu_mode1_mix_masked(
                bg_lines, &machine->obj_pixels,
                machine->main_screen_enable,
                (machine->bg_registers.mode & 8U) != 0U,
                machine->window_hidden, &machine->mode0_line);
        }
        (void)mipsnes2_ppu_window_mask_sub_line(
            &machine->windows, machine->sub_window_hidden);
        if ((machine->bg_registers.mode & 7U) == 0U) {
            (void)mipsnes2_ppu_mode0_mix_masked(
                bg_lines, &machine->obj_pixels,
                machine->sub_screen_enable, machine->sub_window_hidden,
                &machine->sub_line);
        } else {
            (void)mipsnes2_ppu_mode1_mix_masked(
                bg_lines, &machine->obj_pixels,
                machine->sub_screen_enable,
                (machine->bg_registers.mode & 8U) != 0U,
                machine->sub_window_hidden, &machine->sub_line);
        }
        if (machine->cgram != NULL) {
            (void)mipsnes2_ppu_mode0_resolve_color(
                &machine->mode0_line, machine->cgram, machine->cgram_size,
                15U, false, &machine->mode0_raw_line);
            (void)mipsnes2_ppu_mode0_resolve_sub_color(
                &machine->sub_line, machine->cgram, machine->cgram_size,
                machine->fixed_color, &machine->sub_color_line);
            (void)mipsnes2_ppu_window_color_line(
                &machine->windows, machine->color_window_inside);
            (void)mipsnes2_ppu_color_math_line_windowed(
                &machine->mode0_line, &machine->mode0_raw_line,
                &machine->sub_line, &machine->sub_color_line,
                machine->fixed_color, machine->color_math_select,
                machine->color_math_control, machine->ppu_brightness,
                false, machine->color_window_inside,
                &machine->mode0_color_line);
        }
    }
    if (machine->video_line != NULL && line + 1U < vblank) {
        machine->video_line(machine->video_context, line,
                            machine->mode0_color_line.pixels);
    }
    delay = line + 2U < vblank
                ? MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS
                : machine_frame_clocks(machine) -
                      (mipsnes2_master_clock)line *
                          MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
    if (timestamp > UINT64_MAX - delay ||
        mipsnes2_scheduler_schedule(&machine->scheduler, timestamp + delay,
                                    0U, machine_ppu_line, machine, NULL) !=
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

    (void)mipsnes2_ppu_bg_registers_write(&machine->bg_registers,
                                          (uint16_t)address, value);
    (void)mipsnes2_ppu_window_mask_write(&machine->windows,
                                          (uint16_t)address, value);
    if ((uint16_t)address == UINT16_C(0x212c)) {
        machine->main_screen_enable = value & UINT8_C(0x1f);
    }
    if ((uint16_t)address == UINT16_C(0x212d)) {
        machine->sub_screen_enable = value & UINT8_C(0x1f);
    }
    if ((uint16_t)address == UINT16_C(0x2130)) {
        machine->color_math_select = value;
    }
    if ((uint16_t)address == UINT16_C(0x2131)) {
        machine->color_math_control = value;
    }
    if ((uint16_t)address == UINT16_C(0x2132)) {
        uint16_t channel = value & UINT8_C(0x1f);

        if ((value & UINT8_C(0x20)) != 0U) {
            machine->fixed_color =
                (machine->fixed_color & UINT16_C(0x7fe0)) | channel;
        }
        if ((value & UINT8_C(0x40)) != 0U) {
            machine->fixed_color =
                (machine->fixed_color & UINT16_C(0x7c1f)) |
                (uint16_t)(channel << 5);
        }
        if ((value & UINT8_C(0x80)) != 0U) {
            machine->fixed_color =
                (machine->fixed_color & UINT16_C(0x03ff)) |
                (uint16_t)(channel << 10);
        }
    }

    if ((uint16_t)address == UINT16_C(0x2100)) {
        bool next_forced_blank = (value & UINT8_C(0x80)) != 0U;

        if (machine->ppu_forced_blank && !next_forced_blank &&
            machine->hv.v_counter ==
                mipsnes2_hv_vblank_start_line(&machine->hv)) {
            (void)mipsnes2_ppu_oam_ports_vblank_reload(&machine->oam_ports,
                                                       false);
        }
        machine->ppu_forced_blank = next_forced_blank;
        machine->ppu_brightness = value & UINT8_C(0x0f);
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
    mipsnes2_hdma_cursor hdma_seed;

    if (machine == NULL || !machine->ready) {
        return MIPSNES2_MACHINE_NOT_READY;
    }
    mipsnes2_cpu_io_init(&machine->io, UINT8_C(2));
    mipsnes2_ppu_counters_init(&machine->counters, UINT8_C(3), 0U);
    machine->ppu_forced_blank = true;
    machine->ppu_brightness = 0U;
    machine->ppu1_open_bus = 0U;
    machine->obj_overflow = 0U;
    (void)memset(&machine->obj_line, 0, sizeof(machine->obj_line));
    (void)memset(&machine->obj_pixels, 0, sizeof(machine->obj_pixels));
    mipsnes2_ppu_bg_registers_init(&machine->bg_registers);
    mipsnes2_dma_registers_init(&machine->dma_registers);
    mipsnes2_hdma_state_init(&machine->hdma_state);
    mipsnes2_hdma_cursor_init(&hdma_seed);
    if (mipsnes2_hdma_cursor_begin_frame(
            &hdma_seed, &machine->hdma_state,
            &machine->dma_registers, 0U) != MIPSNES2_HDMA_CURSOR_COMPLETE) {
        machine->ready = false;
        return MIPSNES2_MACHINE_TIMING_ERROR;
    }
    machine->hdma_events_scheduled = false;
    mipsnes2_ppu_window_mask_init(&machine->windows);
    (void)memset(machine->window_hidden, 0, sizeof(machine->window_hidden));
    (void)memset(machine->sub_window_hidden, 0,
                 sizeof(machine->sub_window_hidden));
    (void)memset(machine->color_window_inside, 0,
                 sizeof(machine->color_window_inside));
    (void)memset(&machine->bg1_line, 0, sizeof(machine->bg1_line));
    (void)memset(&machine->bg_other_lines, 0,
                 sizeof(machine->bg_other_lines));
    (void)memset(&machine->mode0_line, 0, sizeof(machine->mode0_line));
    (void)memset(&machine->mode0_raw_line, 0,
                 sizeof(machine->mode0_raw_line));
    (void)memset(&machine->mode0_color_line, 0,
                 sizeof(machine->mode0_color_line));
    (void)memset(&machine->sub_line, 0, sizeof(machine->sub_line));
    (void)memset(&machine->sub_color_line, 0,
                 sizeof(machine->sub_color_line));
    machine->main_screen_enable = 0U;
    machine->sub_screen_enable = 0U;
    machine->fixed_color = 0U;
    machine->color_math_select = 0U;
    machine->color_math_control = 0U;
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
    config.dma_registers = &machine->dma_registers;
    config.hdma_state = &machine->hdma_state;
    config.hdma_enable_changed = machine_hdma_enable_changed;
    config.hdma_enable_context = machine;
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
    if ((machine->oam != NULL || machine->vram != NULL ||
         machine->video_line != NULL) &&
        (mipsnes2_scheduler_schedule(
             &machine->scheduler, MIPSNES2_HV_HBLANK_START_CLOCK, 0U,
             machine_ppu_line, machine, NULL) != MIPSNES2_SCHEDULER_OK ||
         (machine->oam != NULL &&
         mipsnes2_scheduler_schedule(
             &machine->scheduler,
             (mipsnes2_master_clock)mipsnes2_hv_frame_scanlines(&machine->hv) *
                 MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS -
                 (machine->standard == MIPSNES2_HV_STANDARD_NTSC ? 4U : 0U),
             0U, machine_obj_frame, machine, NULL) !=
             MIPSNES2_SCHEDULER_OK))) {
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
    machine->video_line = config->video_line;
    machine->video_context = config->video_context;
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
