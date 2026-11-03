/* Original end-to-end synthetic cartridge boot fixture. See LICENSE. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/machine/machine.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

enum {
    FIXTURE_RUNS = 2,
    VISIBLE_LINES = 224,
    BOOT_HORIZON = 50000,
    RUN_SEQUENCE_LIMIT = 100000,
    RUN_EVENT_BUDGET = 64,
    PROGRAM_LIMIT = MIPSNES2_LOROM_HEADER_OFFSET
};

typedef struct {
    size_t offset;
    bool ok;
} rom_builder;

typedef struct {
    uint64_t hash;
    uint64_t line_count;
    uint64_t nonzero_pixels;
    uint16_t expected_color;
    bool armed;
    bool ordered;
    bool expected_pixels;
} frame_capture;

typedef struct {
    uint64_t hash;
    uint64_t line_count;
    uint64_t nonzero_pixels;
    uint64_t master_clocks;
    uint64_t frame_count;
    uint16_t cpu_pc;
    uint16_t smp_pc;
    uint8_t cpu_a;
    uint8_t smp_a;
    mipsnes2_spc700_wait_state smp_wait_state;
} fixture_result;

static uint8_t rom[64U * 1024U];
static uint8_t wram[FIXTURE_RUNS][MIPSNES2_SNES_WRAM_SIZE];
static uint8_t sram[FIXTURE_RUNS][8U * 1024U];
static uint8_t vram[FIXTURE_RUNS][MIPSNES2_PPU_VRAM_SIZE];
static uint8_t cgram[FIXTURE_RUNS][MIPSNES2_PPU_CGRAM_SIZE];
static uint8_t aram[FIXTURE_RUNS][MIPSNES2_SMP_RAM_SIZE];
static mipsnes2_machine machines[FIXTURE_RUNS];

static void put16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void emit8(rom_builder *builder, uint8_t value)
{
    if (builder == NULL || !builder->ok || builder->offset >= PROGRAM_LIMIT) {
        if (builder != NULL) {
            builder->ok = false;
        }
        return;
    }
    rom[builder->offset++] = value;
}

static void emit16(rom_builder *builder, uint16_t value)
{
    emit8(builder, (uint8_t)value);
    emit8(builder, (uint8_t)(value >> 8U));
}

static void emit_lda_immediate(rom_builder *builder, uint8_t value)
{
    emit8(builder, UINT8_C(0xa9));
    emit8(builder, value);
}

static void emit_lda_absolute(rom_builder *builder, uint16_t address)
{
    emit8(builder, UINT8_C(0xad));
    emit16(builder, address);
}

static void emit_sta_absolute(rom_builder *builder, uint16_t address)
{
    emit8(builder, UINT8_C(0x8d));
    emit16(builder, address);
}

static void emit_store(rom_builder *builder, uint16_t address, uint8_t value)
{
    emit_lda_immediate(builder, value);
    emit_sta_absolute(builder, address);
}

static void emit_wait_port(rom_builder *builder, uint16_t address,
                           uint8_t value)
{
    size_t loop = builder->offset;
    size_t operand;
    ptrdiff_t relative;

    emit_lda_absolute(builder, address);
    emit8(builder, UINT8_C(0xc9)); /* CMP #value */
    emit8(builder, value);
    emit8(builder, UINT8_C(0xd0)); /* BNE loop */
    operand = builder->offset;
    emit8(builder, 0U);
    relative = (ptrdiff_t)loop - (ptrdiff_t)builder->offset;
    if (!builder->ok || relative < -128 || relative > 127) {
        builder->ok = false;
        return;
    }
    rom[operand] = (uint8_t)relative;
}

static void emit_smp_byte(rom_builder *builder, uint8_t index, uint8_t value)
{
    emit_store(builder, UINT16_C(0x2141), value);
    emit_store(builder, UINT16_C(0x2140), index);
    emit_wait_port(builder, UINT16_C(0x2140), index);
}

static int make_image(void)
{
    static const uint8_t smp_program[] = {
        UINT8_C(0xe8), UINT8_C(0x42), /* MOV A,#$42 */
        UINT8_C(0xef)                 /* SLEEP */
    };
    mipsnes2_cartridge_image_view view;
    uint8_t *header = rom + MIPSNES2_LOROM_HEADER_OFFSET;
    rom_builder builder = {0U, true};
    uint16_t checksum;
    size_t mirrored;
    unsigned row;
    unsigned index;

    (void)memset(rom, 0, sizeof(rom));

    /* Wait for the firmware-free S-SMP boot handshake, upload an original
     * three-byte program to $0200 and start it through the CPU I/O ports. */
    emit_wait_port(&builder, UINT16_C(0x2140), UINT8_C(0xaa));
    emit_wait_port(&builder, UINT16_C(0x2141), UINT8_C(0xbb));
    emit_store(&builder, UINT16_C(0x2142), UINT8_C(0x00));
    emit_store(&builder, UINT16_C(0x2143), UINT8_C(0x02));
    emit_store(&builder, UINT16_C(0x2141), UINT8_C(0x01));
    emit_store(&builder, UINT16_C(0x2140), UINT8_C(0xcc));
    emit_wait_port(&builder, UINT16_C(0x2140), UINT8_C(0xcc));
    for (index = 0U; index < sizeof(smp_program); ++index) {
        emit_smp_byte(&builder, (uint8_t)index, smp_program[index]);
    }
    emit_store(&builder, UINT16_C(0x2142), UINT8_C(0x00));
    emit_store(&builder, UINT16_C(0x2143), UINT8_C(0x02));
    emit_store(&builder, UINT16_C(0x2141), UINT8_C(0x00));
    emit_store(&builder, UINT16_C(0x2140), UINT8_C(0x04));
    emit_wait_port(&builder, UINT16_C(0x2140), UINT8_C(0x04));

    /* Build a Mode 1 BG1 scene only through 65C816 PPU MMIO. Every blank
     * tilemap entry selects tile zero, whose color-one pixels are green. */
    emit_store(&builder, UINT16_C(0x2100), UINT8_C(0x8f));
    emit_store(&builder, UINT16_C(0x2105), UINT8_C(0x01));
    emit_store(&builder, UINT16_C(0x2107), UINT8_C(0x00));
    emit_store(&builder, UINT16_C(0x210b), UINT8_C(0x01));
    emit_store(&builder, UINT16_C(0x212c), UINT8_C(0x01));
    emit_store(&builder, UINT16_C(0x2115), UINT8_C(0x80));
    emit_store(&builder, UINT16_C(0x2116), UINT8_C(0x00));
    emit_store(&builder, UINT16_C(0x2117), UINT8_C(0x10));
    for (row = 0U; row < 8U; ++row) {
        emit_store(&builder, UINT16_C(0x2118), UINT8_C(0xff));
        emit_store(&builder, UINT16_C(0x2119), UINT8_C(0x00));
    }
    emit_store(&builder, UINT16_C(0x2121), UINT8_C(0x01));
    emit_store(&builder, UINT16_C(0x2122), UINT8_C(0xe0));
    emit_store(&builder, UINT16_C(0x2122), UINT8_C(0x03));
    emit_store(&builder, UINT16_C(0x2100), UINT8_C(0x0f));
    emit_store(&builder, UINT16_C(0x0000), UINT8_C(0xa5));
    emit8(&builder, UINT8_C(0x80)); /* BRA -2 */
    emit8(&builder, UINT8_C(0xfe));
    CHECK(builder.ok);

    (void)memcpy(header, "MIPSNES2 CART BOOT  ",
                 MIPSNES2_INTERNAL_TITLE_SIZE);
    header[0x15] = UINT8_C(0x20);
    header[0x16] = UINT8_C(0x02);
    header[0x17] = UINT8_C(6);
    header[0x18] = UINT8_C(3);
    put16(header + 0x3c, UINT16_C(0x8000));
    CHECK(mipsnes2_cartridge_image_view_init(
        &view, rom, sizeof(rom), MIPSNES2_COPIER_HEADER_REJECT) ==
        MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mipsnes2_cartridge_checksum_calculate(
        &view, MIPSNES2_CARTRIDGE_LOROM, &checksum, &mirrored) ==
        MIPSNES2_CARTRIDGE_IMAGE_OK);
    CHECK(mirrored == sizeof(rom));
    put16(header + 0x1c, (uint16_t)(checksum ^ UINT16_C(0xffff)));
    put16(header + 0x1e, checksum);
    return 0;
}

static uint64_t fnv1a_byte(uint64_t hash, uint8_t value)
{
    return (hash ^ value) * UINT64_C(1099511628211);
}

static void capture_line(void *context, uint16_t line_y,
                         const uint16_t pixels[256])
{
    frame_capture *capture = context;
    unsigned x;

    if (capture == NULL || pixels == NULL || !capture->armed) {
        return;
    }
    if (line_y != capture->line_count ||
        capture->line_count >= VISIBLE_LINES) {
        capture->ordered = false;
    }
    capture->hash = fnv1a_byte(capture->hash, (uint8_t)line_y);
    capture->hash = fnv1a_byte(capture->hash, (uint8_t)(line_y >> 8U));
    for (x = 0U; x < 256U; ++x) {
        uint16_t color = pixels[x];

        capture->hash = fnv1a_byte(capture->hash, (uint8_t)color);
        capture->hash = fnv1a_byte(capture->hash,
                                   (uint8_t)(color >> 8U));
        if (color != 0U) {
            ++capture->nonzero_pixels;
        }
        if (color != capture->expected_color) {
            capture->expected_pixels = false;
        }
    }
    ++capture->line_count;
}

static int run_to(mipsnes2_machine *machine, mipsnes2_master_clock target,
                  const uint64_t *chunks, size_t chunk_count,
                  size_t *chunk_index)
{
    mipsnes2_cpu_timing_report report;
    mipsnes2_cpu_timing_result timing_result;
    size_t sequences;

    CHECK(machine != NULL && chunks != NULL && chunk_count != 0U &&
          chunk_index != NULL && target >= machine->scheduler.now);
    while (machine->scheduler.now < target) {
        mipsnes2_master_clock delta = chunks[*chunk_index % chunk_count];
        mipsnes2_master_clock horizon;

        CHECK(delta != 0U);
        horizon = delta > target - machine->scheduler.now
                      ? target : machine->scheduler.now + delta;
        CHECK(mipsnes2_machine_run_until(
                  machine, horizon, RUN_SEQUENCE_LIMIT, RUN_EVENT_BUDGET,
                  &sequences, &report, &timing_result) ==
              MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(timing_result == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
        CHECK(machine->scheduler.now == horizon);
        ++*chunk_index;
    }
    return 0;
}

static int run_fixture(unsigned run, const uint64_t *chunks,
                       size_t chunk_count, fixture_result *result)
{
    static const uint8_t expected_smp[] = {
        UINT8_C(0xe8), UINT8_C(0x42), UINT8_C(0xef)
    };
    mipsnes2_machine_config config;
    mipsnes2_machine *machine;
    frame_capture capture;
    uint64_t to_boundary;
    mipsnes2_master_clock target;
    size_t chunk_index = 0U;
    unsigned row;

    CHECK(run < FIXTURE_RUNS && chunks != NULL && chunk_count != 0U &&
          result != NULL);
    machine = &machines[run];
    (void)memset(wram[run], 0, sizeof(wram[run]));
    (void)memset(sram[run], 0, sizeof(sram[run]));
    (void)memset(vram[run], 0, sizeof(vram[run]));
    (void)memset(cgram[run], 0, sizeof(cgram[run]));
    (void)memset(aram[run], 0, sizeof(aram[run]));
    (void)memset(&capture, 0, sizeof(capture));
    capture.hash = UINT64_C(14695981039346656037);
    capture.expected_color = UINT16_C(0x03e0);
    capture.ordered = true;
    capture.expected_pixels = true;
    (void)memset(&config, 0, sizeof(config));
    config.image = rom;
    config.image_size = sizeof(rom);
    config.copier_policy = MIPSNES2_COPIER_HEADER_REJECT;
    config.wram = wram[run];
    config.wram_size = sizeof(wram[run]);
    config.sram = sram[run];
    config.sram_capacity = sizeof(sram[run]);
    config.vram = vram[run];
    config.vram_size = sizeof(vram[run]);
    config.cgram = cgram[run];
    config.cgram_size = sizeof(cgram[run]);
    config.aram = aram[run];
    config.aram_size = sizeof(aram[run]);
    config.use_smp_boot_protocol = true;
    config.use_cartridge_region = true;
    config.standard = MIPSNES2_HV_STANDARD_PAL;
    config.video_line = capture_line;
    config.video_context = &capture;
    CHECK(mipsnes2_machine_init(machine, &config) == MIPSNES2_MACHINE_OK);
    CHECK(machine->cartridge.mapper == MIPSNES2_CARTRIDGE_LOROM);
    CHECK(machine->cartridge_region == MIPSNES2_CARTRIDGE_REGION_NTSC);
    CHECK(machine->standard == MIPSNES2_HV_STANDARD_NTSC);

    CHECK(run_to(machine, BOOT_HORIZON, chunks, chunk_count,
                 &chunk_index) == 0);
    CHECK(wram[run][0] == UINT8_C(0xa5));
    CHECK(machine->smp_boot.state == MIPSNES2_SMP_BOOT_COMPLETE);
    CHECK(machine->smp_cpu_enabled && !machine->smp_boot_failed);
    CHECK(memcmp(aram[run] + 0x200U, expected_smp,
                 sizeof(expected_smp)) == 0);
    CHECK(machine->smp_cpu.registers.a == UINT8_C(0x42));
    CHECK(machine->smp_cpu.wait_state == MIPSNES2_SPC700_SLEEPING);
    CHECK(machine->bg_registers.mode == UINT8_C(0x01));
    CHECK(machine->main_screen_enable == UINT8_C(0x01));
    CHECK(!machine->ppu_forced_blank && machine->ppu_brightness == 15U);
    for (row = 0U; row < 8U; ++row) {
        CHECK(vram[run][0x2000U + row * 2U] == UINT8_C(0xff));
        CHECK(vram[run][0x2001U + row * 2U] == UINT8_C(0x00));
    }
    CHECK(cgram[run][2] == UINT8_C(0xe0) &&
          cgram[run][3] == UINT8_C(0x03));

    CHECK(mipsnes2_hv_clocks_until_frame_boundary(
        &machine->hv, &to_boundary));
    CHECK(to_boundary <= UINT64_MAX - machine->scheduler.now);
    target = machine->scheduler.now + to_boundary;
    CHECK(run_to(machine, target, chunks, chunk_count, &chunk_index) == 0);
    CHECK(machine->hv.v_counter == 0U && machine->hv.h_master_clock == 0U);

    capture.armed = true;
    CHECK(mipsnes2_hv_clocks_until_frame_boundary(
        &machine->hv, &to_boundary));
    CHECK(to_boundary <= UINT64_MAX - machine->scheduler.now);
    target = machine->scheduler.now + to_boundary;
    CHECK(run_to(machine, target, chunks, chunk_count, &chunk_index) == 0);
    capture.armed = false;
    CHECK(capture.ordered && capture.expected_pixels);
    CHECK(capture.line_count == VISIBLE_LINES);
    CHECK(capture.nonzero_pixels ==
          UINT64_C(256) * (uint64_t)VISIBLE_LINES);

    result->hash = capture.hash;
    result->line_count = capture.line_count;
    result->nonzero_pixels = capture.nonzero_pixels;
    result->master_clocks = machine->scheduler.now;
    result->frame_count = machine->hv.frame_count;
    result->cpu_pc = machine->cpu.pc;
    result->smp_pc = machine->smp_cpu.registers.pc;
    result->cpu_a = (uint8_t)machine->cpu.a;
    result->smp_a = machine->smp_cpu.registers.a;
    result->smp_wait_state = machine->smp_cpu.wait_state;
    return 0;
}

int main(void)
{
    static const uint64_t regular_chunks[] = {UINT64_C(50000)};
    static const uint64_t irregular_chunks[] = {
        UINT64_C(1), UINT64_C(1095), UINT64_C(1), UINT64_C(1363),
        UINT64_C(17), UINT64_C(32768), UINT64_C(49999), UINT64_C(41000)
    };
    fixture_result regular;
    fixture_result irregular;

    CHECK(make_image() == 0);
    CHECK(run_fixture(0U, regular_chunks,
                      sizeof(regular_chunks) / sizeof(regular_chunks[0]),
                      &regular) == 0);
    CHECK(run_fixture(1U, irregular_chunks,
                      sizeof(irregular_chunks) /
                          sizeof(irregular_chunks[0]),
                      &irregular) == 0);
    CHECK(regular.hash == irregular.hash);
    CHECK(regular.line_count == irregular.line_count);
    CHECK(regular.nonzero_pixels == irregular.nonzero_pixels);
    CHECK(regular.master_clocks == irregular.master_clocks);
    CHECK(regular.frame_count == irregular.frame_count);
    CHECK(regular.cpu_pc == irregular.cpu_pc);
    CHECK(regular.smp_pc == irregular.smp_pc);
    CHECK(regular.cpu_a == irregular.cpu_a);
    CHECK(regular.smp_a == irregular.smp_a);
    CHECK(regular.smp_wait_state == irregular.smp_wait_state);
    if (regular.hash != UINT64_C(0x0c68c1b754439e25)) {
        (void)fprintf(stderr, "cartridge boot frame hash: %016llx\n",
                      (unsigned long long)regular.hash);
    }
    CHECK(regular.hash == UINT64_C(0x0c68c1b754439e25));
    (void)puts("machine synthetic cartridge boot: OK");
    return 0;
}
