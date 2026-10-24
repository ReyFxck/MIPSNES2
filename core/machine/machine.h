/* MIPSNES2 platform-neutral machine composition. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_MACHINE_H
#define MIPSNES2_MACHINE_H

#include "core/cartridge/prepare.h"
#include "core/ppu/cgram_ports.h"
#include "core/ppu/color_math.h"
#include "core/ppu/bg_registers.h"
#include "core/ppu/mode0_mix.h"
#include "core/ppu/oam_ports.h"
#include "core/ppu/sprite_scanline.h"
#include "core/ppu/vram_ports.h"
#include "core/ppu/window_mask.h"
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
    uint8_t *vram;
    size_t vram_size;
    uint8_t *cgram;
    size_t cgram_size;
    uint8_t *oam;
    size_t oam_size;
    mipsnes2_hv_standard standard;
    bool overscan;
    mipsnes2_bus_interface ppu;
    mipsnes2_bus_interface apu;
    mipsnes2_bus_interface dma;
} mipsnes2_machine_config;

/* Stable address required: bus and CPU callbacks point into this object.
 * Image, WRAM, SRAM and optional PPU memory remain caller-owned for the entire
 * machine lifetime.
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
    mipsnes2_ppu_vram_ports vram_ports;
    mipsnes2_ppu_cgram_ports cgram_ports;
    mipsnes2_ppu_oam_ports oam_ports;
    mipsnes2_ppu_obj_scanline obj_line;
    mipsnes2_ppu_obj_pixels obj_pixels;
    mipsnes2_ppu_bg_registers bg_registers;
    mipsnes2_ppu_bg2_scanline bg1_line;
    mipsnes2_ppu_bg2_scanline bg_other_lines[3];
    mipsnes2_ppu_mode0_line mode0_line;
    mipsnes2_ppu_mode0_color_line mode0_raw_line;
    mipsnes2_ppu_mode0_color_line mode0_color_line;
    mipsnes2_ppu_mode0_line sub_line;
    mipsnes2_ppu_mode0_color_line sub_color_line;
    mipsnes2_ppu_window_mask windows;
    uint8_t window_hidden[256];
    uint8_t sub_window_hidden[256];
    mipsnes2_cpu_timing_state cursor;
    mipsnes2_bus_interface ppu;
    mipsnes2_bus_interface apu;
    mipsnes2_bus_interface dma;
    uint8_t *wram;
    size_t wram_size;
    uint8_t *vram;
    size_t vram_size;
    uint8_t *cgram;
    size_t cgram_size;
    uint8_t *oam;
    size_t oam_size;
    mipsnes2_hv_standard standard;
    bool overscan;
    bool ppu_forced_blank;
    uint8_t ppu_brightness;
    uint8_t ppu1_open_bus;
    uint8_t obj_overflow;
    uint8_t main_screen_enable;
    uint8_t sub_screen_enable;
    uint16_t fixed_color;
    uint8_t color_math_select;
    uint8_t color_math_control;
    bool ready;
} mipsnes2_machine;

/* On failure machine is not ready. No image or SRAM bytes are modified.
 * The caller must supply at least 128 KiB WRAM and header-declared SRAM.
 */
mipsnes2_machine_result mipsnes2_machine_init(
    mipsnes2_machine *machine, const mipsnes2_machine_config *config);
/* Power cycle the CPU, I/O, counters, PPU ports, clock and event queue.
 * Caller-owned WRAM/SRAM/VRAM/CGRAM/OAM retain bytes. Events are discarded.
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

typedef enum {
    MIPSNES2_MACHINE_RUN_HORIZON = 0,
    MIPSNES2_MACHINE_RUN_SEQUENCE_LIMIT,
    MIPSNES2_MACHINE_RUN_CPU_STOP,
    MIPSNES2_MACHINE_RUN_TIMING_ERROR,
    MIPSNES2_MACHINE_RUN_INVALID_ARGUMENT
} mipsnes2_machine_run_result;

/* Bounded CPU-only execution: report holds the final sequence or paused
 * operation, and sequences counts only fully completed RESET/instructions.
 * A horizon return retains the operation cursor for the next call. Callers
 * must arrange missing device events, renderer and audio separately.
 */
mipsnes2_machine_run_result mipsnes2_machine_run_until(
    mipsnes2_machine *machine, mipsnes2_master_clock horizon,
    size_t sequence_limit, size_t event_budget,
    size_t *sequences, mipsnes2_cpu_timing_report *report,
    mipsnes2_cpu_timing_result *timing_result);

#endif
