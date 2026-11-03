/* Integrated machine/PPU line-event determinism fixture. See LICENSE. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/machine/machine.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

enum {
    VISIBLE_LINES = 224,
    RUN_SEQUENCE_LIMIT = 100000,
    RUN_EVENT_BUDGET = 64
};

static uint8_t rom[64U * 1024U];
static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
static uint8_t sram[8U * 1024U];
static uint8_t vram[MIPSNES2_PPU_VRAM_SIZE];
static uint8_t cgram[MIPSNES2_PPU_CGRAM_SIZE];
static mipsnes2_machine machine;

typedef struct {
    uint64_t hash;
    uint64_t line_count;
    uint64_t nonzero_pixels;
    bool ordered;
} frame_capture;

typedef struct {
    uint64_t hash;
    uint64_t line_count;
    uint64_t nonzero_pixels;
    uint64_t master_clocks;
    uint64_t frame_count;
    uint16_t v_counter;
    uint16_t h_master_clock;
    bool ordered;
} machine_ppu_result;

static void put16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static int make_image(void)
{
    mipsnes2_cartridge_image_view view;
    uint8_t *header = rom + MIPSNES2_LOROM_HEADER_OFFSET;
    uint16_t checksum;
    size_t mirrored;

    (void)memset(rom, 0, sizeof(rom));
    rom[0] = UINT8_C(0x80); /* BRA -2 */
    rom[1] = UINT8_C(0xfe);
    (void)memcpy(header, "MIPSNES2 PPU FRAME  ", MIPSNES2_INTERNAL_TITLE_SIZE);
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
    put16(header + 0x1c, (uint16_t)(checksum ^ UINT16_C(0xffff)));
    put16(header + 0x1e, checksum);
    return 0;
}

static void make_scene(void)
{
    unsigned row;
    unsigned index;

    (void)memset(vram, 0, sizeof(vram));
    (void)memset(cgram, 0, sizeof(cgram));
    for (row = 0U; row < 8U; ++row) {
        vram[row * 2U] = (uint8_t)(UINT8_C(0x81) >> (row & 3U));
        vram[row * 2U + 1U] = (uint8_t)(UINT8_C(0x18) << (row & 1U));
    }
    for (index = 0U; index < 32U * 32U; ++index) {
        uint16_t entry = (uint16_t)(((index / 32U + index) & 7U) << 10U);
        size_t address = 0x800U + index * 2U;

        vram[address] = (uint8_t)entry;
        vram[address + 1U] = (uint8_t)(entry >> 8U);
    }
    for (index = 0U; index < 256U; ++index) {
        uint16_t color = (uint16_t)(
            (index & 31U) |
            (((index * 3U) & 31U) << 5U) |
            (((index * 7U) & 31U) << 10U));

        cgram[index * 2U] = (uint8_t)color;
        cgram[index * 2U + 1U] = (uint8_t)(color >> 8U);
    }
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

    if (capture == NULL || pixels == NULL) {
        return;
    }
    if (line_y != (uint16_t)(capture->line_count % VISIBLE_LINES)) {
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
    }
    ++capture->line_count;
}

static int run_machine_ppu(const uint64_t *chunks, size_t chunk_count,
                           machine_ppu_result *result)
{
    const mipsnes2_master_clock normal_frame =
        (mipsnes2_master_clock)MIPSNES2_HV_NTSC_SCANLINES *
        MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS;
    const mipsnes2_master_clock final_horizon =
        normal_frame +
        (normal_frame - (MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS -
                         MIPSNES2_HV_NTSC_SHORT_SCANLINE_CLOCKS)) +
        MIPSNES2_HV_HBLANK_START_CLOCK - 1U;
    mipsnes2_machine_config config;
    mipsnes2_cpu_timing_report report;
    mipsnes2_cpu_timing_result timing_result;
    frame_capture capture;
    mipsnes2_master_clock horizon = 0U;
    size_t chunk_index = 0U;
    size_t sequences;

    CHECK(chunks != NULL && chunk_count != 0U && result != NULL);
    CHECK(make_image() == 0);
    make_scene();
    (void)memset(wram, 0, sizeof(wram));
    (void)memset(sram, 0, sizeof(sram));
    (void)memset(&capture, 0, sizeof(capture));
    capture.hash = UINT64_C(14695981039346656037);
    capture.ordered = true;
    (void)memset(&config, 0, sizeof(config));
    config.image = rom;
    config.image_size = sizeof(rom);
    config.copier_policy = MIPSNES2_COPIER_HEADER_REJECT;
    config.wram = wram;
    config.wram_size = sizeof(wram);
    config.sram = sram;
    config.sram_capacity = sizeof(sram);
    config.vram = vram;
    config.vram_size = sizeof(vram);
    config.cgram = cgram;
    config.cgram_size = sizeof(cgram);
    config.standard = MIPSNES2_HV_STANDARD_NTSC;
    config.video_line = capture_line;
    config.video_context = &capture;
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002107), UINT8_C(4));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x00212c), UINT8_C(1));
    mipsnes2_snes_bus_write(&machine.bus, UINT32_C(0x002100), UINT8_C(15));
    while (horizon < final_horizon) {
        mipsnes2_master_clock delta = chunks[chunk_index % chunk_count];
        mipsnes2_master_clock next;

        CHECK(delta != 0U);
        next = delta > final_horizon - horizon
            ? final_horizon : horizon + delta;
        CHECK(mipsnes2_machine_run_until(
                  &machine, next, RUN_SEQUENCE_LIMIT, RUN_EVENT_BUDGET,
                  &sequences, &report, &timing_result) ==
              MIPSNES2_MACHINE_RUN_HORIZON);
        CHECK(timing_result == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
        CHECK(machine.scheduler.now == next);
        horizon = next;
        ++chunk_index;
    }
    result->hash = capture.hash;
    result->line_count = capture.line_count;
    result->nonzero_pixels = capture.nonzero_pixels;
    result->master_clocks = machine.scheduler.now;
    result->frame_count = machine.hv.frame_count;
    result->v_counter = machine.hv.v_counter;
    result->h_master_clock = machine.hv.h_master_clock;
    result->ordered = capture.ordered;
    return 0;
}

int main(void)
{
    static const uint64_t regular_chunks[] = {UINT64_C(50000)};
    static const uint64_t irregular_chunks[] = {
        UINT64_C(1), UINT64_C(1095), UINT64_C(1), UINT64_C(1363),
        UINT64_C(32768), UINT64_C(49999), UINT64_C(17), UINT64_C(41000)
    };
    machine_ppu_result regular;
    machine_ppu_result irregular;

    CHECK(run_machine_ppu(regular_chunks,
                          sizeof(regular_chunks) /
                              sizeof(regular_chunks[0]),
                          &regular) == 0);
    CHECK(run_machine_ppu(irregular_chunks,
                          sizeof(irregular_chunks) /
                              sizeof(irregular_chunks[0]),
                          &irregular) == 0);
    CHECK(regular.hash == irregular.hash);
    CHECK(regular.line_count == irregular.line_count);
    CHECK(regular.nonzero_pixels == irregular.nonzero_pixels);
    CHECK(regular.master_clocks == irregular.master_clocks);
    CHECK(regular.frame_count == irregular.frame_count);
    CHECK(regular.v_counter == irregular.v_counter);
    CHECK(regular.h_master_clock == irregular.h_master_clock);
    CHECK(regular.ordered && irregular.ordered);
    CHECK(regular.line_count == UINT64_C(448));
    CHECK(regular.nonzero_pixels != 0U);
    CHECK(regular.frame_count == UINT64_C(2));
    CHECK(regular.v_counter == 0U);
    CHECK(regular.h_master_clock ==
          MIPSNES2_HV_HBLANK_START_CLOCK - 1U);
    if (regular.hash != UINT64_C(0x911273792f0b2425)) {
        (void)fprintf(stderr,
                      "machine PPU hash: %016llx; nonzero: %llu\n",
                      (unsigned long long)regular.hash,
                      (unsigned long long)regular.nonzero_pixels);
    }
    CHECK(regular.hash == UINT64_C(0x911273792f0b2425));
    (void)puts("machine PPU frame-event integration: OK");
    return 0;
}
