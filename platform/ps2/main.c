/* MIPSNES2 PS2 diagnostic launcher. See LICENSE for GPL-3.0 terms. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <debug.h>
#include <sifrpc.h>

#include "platform/ps2/diagnostic_suite.h"
#include "platform/ps2/rom_file.h"
#include "core/machine/machine.h"

enum {
    PS2_ROM_CAPACITY = MIPSNES2_EXHIROM_MAX_SIZE + MIPSNES2_COPIER_HEADER_SIZE,
    PS2_SRAM_CAPACITY = MIPSNES2_LOROM_SRAM_MAX_SIZE,
    PS2_CPU_STEP_LIMIT = 100000,
    PS2_EVENT_BUDGET = 64
};

/* EE memory, never the target stack. The file image remains live for the CPU. */
static uint8_t rom[PS2_ROM_CAPACITY];
static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
static uint8_t sram[PS2_SRAM_CAPACITY];
static uint8_t vram[MIPSNES2_PPU_VRAM_SIZE];
static uint8_t cgram[MIPSNES2_PPU_CGRAM_SIZE];
static uint8_t oam[MIPSNES2_PPU_OAM_SIZE];
static mipsnes2_machine machine;

static void print_result(void *context, const char *name, bool passed)
{
    (void)context;
    scr_printf("%s  %s\n", passed ? "PASS" : "FAIL", name);
    (void)printf("MIPSNES2 PS2 DIAG: %s %s\n",
                 passed ? "PASS" : "FAIL", name);
}

static void run_rom_path(const char *path)
{
    mipsnes2_machine_config config;
    mipsnes2_cpu_timing_report report;
    mipsnes2_ps2_rom_file_result file_result;
    mipsnes2_cpu_timing_result timing_result;
    mipsnes2_machine_run_result run_result;
    mipsnes2_master_clock horizon =
        (mipsnes2_master_clock)MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS *
        MIPSNES2_HV_NTSC_SCANLINES;
    size_t rom_size;
    size_t steps;

    scr_printf("MIPSNES2 PS2 ROM CPU PATH\n\n");
    file_result = mipsnes2_ps2_rom_file_load(
        path, rom, sizeof(rom), &rom_size);
    if (file_result != MIPSNES2_PS2_ROM_FILE_OK) {
        scr_printf("ROM read failed: %u\n", (unsigned)file_result);
        return;
    }
    (void)memset(&config, 0, sizeof(config));
    config.image = rom;
    config.image_size = rom_size;
    config.copier_policy = MIPSNES2_COPIER_HEADER_DETECT;
    config.wram = wram;
    config.wram_size = sizeof(wram);
    config.sram = sram;
    config.sram_capacity = sizeof(sram);
    config.vram = vram;
    config.vram_size = sizeof(vram);
    config.cgram = cgram;
    config.cgram_size = sizeof(cgram);
    config.oam = oam;
    config.oam_size = sizeof(oam);
    config.standard = MIPSNES2_HV_STANDARD_NTSC;
    if (mipsnes2_machine_init(&machine, &config) != MIPSNES2_MACHINE_OK) {
        scr_printf("ROM header/mapper unsupported.\n");
        return;
    }
    scr_printf("ROM: %u bytes; CPU running...\n", (unsigned)rom_size);
    run_result = mipsnes2_machine_run_until(
        &machine, horizon, PS2_CPU_STEP_LIMIT, PS2_EVENT_BUDGET,
        &steps, &report, &timing_result);
    scr_printf("CPU: %lu sequences, %lu clocks\n", (unsigned long)steps,
               (unsigned long)machine.scheduler.now);
    scr_printf("PC: %02X:%04X  A: %04X\n",
               (unsigned)machine.cpu.pbr, (unsigned)machine.cpu.pc,
               (unsigned)machine.cpu.a);
    scr_printf("Run: %u  Timing: %u  CPU: %u\n", (unsigned)run_result,
               (unsigned)timing_result,
               (unsigned)report.cpu_result);
    scr_printf("No SNES video/audio yet.\n");
    (void)printf("MIPSNES2 PS2 ROM CPU: %lu sequences, %lu clocks, "
                 "run=%u, timing=%u, cpu=%u\n", (unsigned long)steps,
                 (unsigned long)machine.scheduler.now,
                 (unsigned)run_result, (unsigned)timing_result,
                 (unsigned)report.cpu_result);
    (void)fflush(stdout);
}

int main(int argc, char **argv)
{
    mipsnes2_ps2_diagnostic_report report;

    sceSifInitRpc(0);
    init_scr();
    if (argc == 3 && argv != NULL && argv[1] != NULL && argv[2] != NULL &&
        strcmp(argv[1], "--rom") == 0) {
        run_rom_path(argv[2]);
        scr_printf("Use Android Back to exit.\n");
        for (;;) {
            (void)sleep(1);
        }
    }
    if (argc > 1) {
        scr_printf("Usage: mipsnes2-ps2.elf --rom PATH\n");
        scr_printf("No ROM path: default diagnostic.\n\n");
    }
    scr_printf("MIPSNES2 PS2 DIAGNOSTIC v1\n\n");
    mipsnes2_ps2_diagnostic_run(&report, print_result, NULL);
    scr_printf("\nRESULT: %u PASS / %u FAIL\n", report.passed, report.failed);
    scr_printf("Use Android Back to exit.\n");
    (void)printf("MIPSNES2 PS2 DIAG: RESULT %u PASS / %u FAIL\n",
                 report.passed, report.failed);
    (void)fflush(stdout);
    for (;;) {
        (void)sleep(1);
    }
}
