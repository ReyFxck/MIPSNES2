/* Host-only bounded ROM runner. See LICENSE for GPL-3.0 terms. */
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/machine/machine.h"

enum {
    RUNNER_ROM_CAPACITY = 8 * 1024 * 1024 + MIPSNES2_COPIER_HEADER_SIZE,
    RUNNER_WRAM_SIZE = 128 * 1024,
    RUNNER_SRAM_SIZE = MIPSNES2_LOROM_SRAM_MAX_SIZE,
    RUNNER_VRAM_SIZE = 64 * 1024,
    RUNNER_CGRAM_SIZE = 512,
    RUNNER_OAM_SIZE = 544,
    RUNNER_ARAM_SIZE = 64 * 1024,
    RUNNER_DEFAULT_FRAMES = 120,
    RUNNER_SEQUENCE_LIMIT = 65536,
    RUNNER_EVENT_BUDGET = 65536
};

typedef struct {
    const char *rom_path;
    uint64_t frames;
    size_t watch_offset;
    size_t watch_size;
    size_t vram_offset;
    size_t vram_size;
    bool watch_enabled;
    bool vram_enabled;
} runner_options;

static uint8_t rom[RUNNER_ROM_CAPACITY];
static uint8_t wram[RUNNER_WRAM_SIZE];
static uint8_t sram[RUNNER_SRAM_SIZE];
static uint8_t vram[RUNNER_VRAM_SIZE];
static uint8_t cgram[RUNNER_CGRAM_SIZE];
static uint8_t oam[RUNNER_OAM_SIZE];
static uint8_t aram[RUNNER_ARAM_SIZE];
static mipsnes2_machine machine;

static void usage(FILE *stream, const char *program)
{
    (void)fprintf(stream,
                  "usage: %s [--frames N] [--watch-wram OFFSET] "
                  "[--watch-size 1|2|4|8] "
                  "[--dump-vram OFFSET SIZE] ROM\n",
                  program);
}

static bool parse_u64(const char *text, uint64_t maximum, uint64_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || text[0] == '\0' || value == NULL || text[0] == '-') {
        return false;
    }
    errno = 0;
    parsed = strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > maximum) {
        return false;
    }
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_options(int argc, char **argv, runner_options *options)
{
    int index;

    if (argv == NULL || options == NULL) {
        return false;
    }
    (void)memset(options, 0, sizeof(*options));
    options->frames = RUNNER_DEFAULT_FRAMES;
    options->watch_size = 2U;
    for (index = 1; index < argc; ++index) {
        uint64_t value;

        if (strcmp(argv[index], "--frames") == 0) {
            if (++index >= argc ||
                !parse_u64(argv[index], UINT64_C(1000000), &value) ||
                value == 0U) {
                return false;
            }
            options->frames = value;
        } else if (strcmp(argv[index], "--watch-wram") == 0) {
            if (++index >= argc ||
                !parse_u64(argv[index], RUNNER_WRAM_SIZE - 1U, &value)) {
                return false;
            }
            options->watch_offset = (size_t)value;
            options->watch_enabled = true;
        } else if (strcmp(argv[index], "--watch-size") == 0) {
            if (++index >= argc ||
                !parse_u64(argv[index], 8U, &value) ||
                (value != 1U && value != 2U && value != 4U && value != 8U)) {
                return false;
            }
            options->watch_size = (size_t)value;
        } else if (strcmp(argv[index], "--dump-vram") == 0) {
            if (++index >= argc ||
                !parse_u64(argv[index], RUNNER_VRAM_SIZE - 1U, &value)) {
                return false;
            }
            options->vram_offset = (size_t)value;
            if (++index >= argc ||
                !parse_u64(argv[index], 1024U, &value) || value == 0U) {
                return false;
            }
            options->vram_size = (size_t)value;
            options->vram_enabled = true;
        } else if (argv[index][0] == '-' || options->rom_path != NULL) {
            return false;
        } else {
            options->rom_path = argv[index];
        }
    }
    return options->rom_path != NULL &&
           (!options->watch_enabled ||
            options->watch_size <= RUNNER_WRAM_SIZE - options->watch_offset) &&
           (!options->vram_enabled ||
            options->vram_size <= RUNNER_VRAM_SIZE - options->vram_offset);
}

static bool load_rom(const char *path, size_t *size)
{
    FILE *stream;
    long length;
    size_t loaded;

    if (path == NULL || size == NULL) {
        return false;
    }
    stream = fopen(path, "rb");
    if (stream == NULL) {
        (void)fprintf(stderr, "ROM open failed: %s\n", strerror(errno));
        return false;
    }
    if (fseek(stream, 0L, SEEK_END) != 0 || (length = ftell(stream)) <= 0L ||
        (uint64_t)length > (uint64_t)sizeof(rom) ||
        fseek(stream, 0L, SEEK_SET) != 0) {
        (void)fputs("ROM size is invalid or exceeds the 8 MiB runner limit.\n",
                    stderr);
        (void)fclose(stream);
        return false;
    }
    loaded = fread(rom, 1U, (size_t)length, stream);
    if (loaded != (size_t)length || fclose(stream) != 0) {
        (void)fputs("ROM read failed.\n", stderr);
        return false;
    }
    *size = loaded;
    return true;
}

static const char *mapper_name(mipsnes2_cartridge_mapper mapper)
{
    switch (mapper) {
    case MIPSNES2_CARTRIDGE_LOROM:
        return "lorom";
    case MIPSNES2_CARTRIDGE_HIROM:
        return "hirom";
    case MIPSNES2_CARTRIDGE_EXHIROM:
        return "exhirom";
    }
    return "unknown";
}

static int run_frames(uint64_t requested_frames,
                      mipsnes2_machine_run_result *last_run,
                      mipsnes2_cpu_timing_result *last_timing,
                      mipsnes2_cpu_timing_report *last_report,
                      uint64_t *sequences_total)
{
    uint64_t target_frame = machine.hv.frame_count + requested_frames;

    while (machine.hv.frame_count < target_frame) {
        mipsnes2_master_clock to_boundary;
        mipsnes2_master_clock horizon;

        if (!mipsnes2_hv_clocks_until_frame_boundary(&machine.hv,
                                                       &to_boundary) ||
            to_boundary > UINT64_MAX - machine.scheduler.now) {
            return 1;
        }
        horizon = machine.scheduler.now + to_boundary;
        do {
            size_t sequences = 0U;

            *last_run = mipsnes2_machine_run_until(
                &machine, horizon, RUNNER_SEQUENCE_LIMIT,
                RUNNER_EVENT_BUDGET, &sequences, last_report, last_timing);
            *sequences_total += sequences;
        } while (*last_run == MIPSNES2_MACHINE_RUN_SEQUENCE_LIMIT);
        if (*last_run != MIPSNES2_MACHINE_RUN_HORIZON) {
            return 1;
        }
    }
    return 0;
}

static void print_watch(const runner_options *options)
{
    uint64_t value = 0U;
    size_t index;

    if (!options->watch_enabled) {
        return;
    }
    for (index = 0U; index < options->watch_size; ++index) {
        value |= (uint64_t)wram[options->watch_offset + index] << (index * 8U);
    }
    (void)printf("watch_wram_offset=0x%05zx watch_size=%zu "
                 "watch_value=0x%0*" PRIx64 "\n",
                 options->watch_offset, options->watch_size,
                 (int)(options->watch_size * 2U), value);
}

static void print_vram(const runner_options *options)
{
    size_t index;

    if (!options->vram_enabled) {
        return;
    }
    (void)printf("vram_offset=0x%04zx vram_size=%zu vram_hex=",
                 options->vram_offset, options->vram_size);
    for (index = 0U; index < options->vram_size; ++index) {
        (void)printf("%02x", vram[options->vram_offset + index]);
    }
    (void)putchar('\n');
}

int main(int argc, char **argv)
{
    runner_options options;
    mipsnes2_machine_config config;
    mipsnes2_machine_run_result run_result = MIPSNES2_MACHINE_RUN_HORIZON;
    mipsnes2_cpu_timing_result timing_result = MIPSNES2_CPU_TIMING_OK;
    mipsnes2_cpu_timing_report report;
    mipsnes2_machine_result machine_result;
    uint64_t sequences = 0U;
    size_t rom_size;
    int result;

    if (!parse_options(argc, argv, &options)) {
        usage(stderr, argc > 0 && argv != NULL ? argv[0] : "rom-runner");
        return 2;
    }
    if (!load_rom(options.rom_path, &rom_size)) {
        return 2;
    }
    (void)memset(&config, 0, sizeof(config));
    (void)memset(&report, 0, sizeof(report));
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
    config.aram = aram;
    config.aram_size = sizeof(aram);
    config.use_smp_boot_protocol = true;
    config.use_cartridge_region = true;
    config.standard = MIPSNES2_HV_STANDARD_NTSC;
    machine_result = mipsnes2_machine_init(&machine, &config);
    if (machine_result != MIPSNES2_MACHINE_OK) {
        (void)fprintf(stderr, "machine_init=%u\n", (unsigned)machine_result);
        return 2;
    }

    result = run_frames(options.frames, &run_result, &timing_result,
                        &report, &sequences);
    (void)printf("rom_size=%zu mapper=%s region=%u frames=%" PRIu64
                 " master_clocks=%" PRIu64 " sequences=%" PRIu64 "\n",
                 rom_size, mapper_name(machine.cartridge.mapper),
                 (unsigned)machine.cartridge_region, machine.hv.frame_count,
                 machine.scheduler.now, sequences);
    (void)printf("run_result=%u timing_result=%u cpu_result=%u "
                 "pbr_pc=%02x:%04x opcode=%02x opcode_address=%06x\n",
                 (unsigned)run_result, (unsigned)timing_result,
                 (unsigned)report.cpu_result, machine.cpu.pbr, machine.cpu.pc,
                 machine.cpu.last_opcode,
                 (unsigned)machine.cpu.last_opcode_address);
    (void)printf("a=%04x x=%04x y=%04x s=%04x d=%04x dbr=%02x p=%02x "
                 "emulation=%u waiting=%u stopped=%u\n",
                 machine.cpu.a, machine.cpu.x, machine.cpu.y, machine.cpu.s,
                 machine.cpu.d, machine.cpu.dbr, machine.cpu.p,
                 machine.cpu.emulation ? 1U : 0U,
                 machine.cpu.waiting ? 1U : 0U,
                 machine.cpu.stopped ? 1U : 0U);
    print_watch(&options);
    print_vram(&options);
    return result;
}
