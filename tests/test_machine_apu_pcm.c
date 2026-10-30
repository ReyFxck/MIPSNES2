/* Integrated machine/APU/PCM determinism fixture. See LICENSE. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/machine/machine.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

enum {
    PCM_CAPACITY = 128,
    RUN_SEQUENCE_LIMIT = 100000,
    RUN_EVENT_BUDGET = 64
};

static uint8_t rom[64U * 1024U];
static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
static uint8_t sram[8U * 1024U];
static uint8_t aram[MIPSNES2_SMP_RAM_SIZE];
static uint8_t synthetic_ipl[MIPSNES2_SMP_IPL_SIZE];
static mipsnes2_pcm_frame pcm_storage[PCM_CAPACITY];
static mipsnes2_pcm_frame pcm_output[PCM_CAPACITY];
static mipsnes2_machine machine;

typedef struct {
    uint64_t pcm_hash;
    uint64_t frame_count;
    uint64_t sample_count;
    uint64_t smp_cycles;
    uint64_t master_clocks;
    uint64_t enqueued_frames;
    uint64_t consumed_frames;
    uint64_t dropped_frames;
    uint16_t noise_lfsr;
    uint16_t envelope_counter;
    int16_t last_left;
    int16_t last_right;
    uint8_t sample_phase;
    mipsnes2_spc700_wait_state wait_state;
} machine_pcm_result;

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
    /* BRA -2 keeps the 5A22 moving while the integrated APU advances. */
    rom[0] = UINT8_C(0x80);
    rom[1] = UINT8_C(0xfe);
    (void)memcpy(header, "MIPSNES2 APU PCM TEST", MIPSNES2_INTERNAL_TITLE_SIZE);
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

static void emit_dsp_write(size_t *offset, uint8_t address, uint8_t value)
{
    synthetic_ipl[(*offset)++] = UINT8_C(0x8f); /* MOV $F2,#address */
    synthetic_ipl[(*offset)++] = address;
    synthetic_ipl[(*offset)++] = UINT8_C(0xf2);
    synthetic_ipl[(*offset)++] = UINT8_C(0x8f); /* MOV $F3,#value */
    synthetic_ipl[(*offset)++] = value;
    synthetic_ipl[(*offset)++] = UINT8_C(0xf3);
}

static int make_ipl(void)
{
    size_t offset = 0U;

    (void)memset(synthetic_ipl, 0, sizeof(synthetic_ipl));
    emit_dsp_write(&offset, MIPSNES2_DSP_REGISTER_FLG, UINT8_C(0x1f));
    emit_dsp_write(&offset, MIPSNES2_DSP_REGISTER_NON, UINT8_C(1));
    emit_dsp_write(&offset, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT,
                   UINT8_C(0x60));
    emit_dsp_write(&offset, MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT,
                   UINT8_C(0xc0));
    emit_dsp_write(&offset, MIPSNES2_DSP_VOICE_REGISTER_ADSR1, 0U);
    emit_dsp_write(&offset, MIPSNES2_DSP_VOICE_REGISTER_GAIN,
                   UINT8_C(0x50));
    emit_dsp_write(&offset, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT,
                   UINT8_C(0x55));
    emit_dsp_write(&offset, MIPSNES2_DSP_REGISTER_MAIN_VOLUME_RIGHT,
                   UINT8_C(0xb0));
    emit_dsp_write(&offset, MIPSNES2_DSP_REGISTER_KON, UINT8_C(1));
    CHECK(offset < sizeof(synthetic_ipl));
    synthetic_ipl[offset] = UINT8_C(0xef); /* SLEEP */
    return 0;
}

static uint64_t fnv1a_byte(uint64_t hash, uint8_t value)
{
    return (hash ^ value) * UINT64_C(1099511628211);
}

static uint64_t hash_frame(uint64_t hash, mipsnes2_pcm_frame frame)
{
    uint16_t left = (uint16_t)frame.left;
    uint16_t right = (uint16_t)frame.right;

    hash = fnv1a_byte(hash, (uint8_t)left);
    hash = fnv1a_byte(hash, (uint8_t)(left >> 8U));
    hash = fnv1a_byte(hash, (uint8_t)right);
    return fnv1a_byte(hash, (uint8_t)(right >> 8U));
}

static void drain_pcm(uint64_t *hash, uint64_t *frame_count)
{
    size_t count = mipsnes2_pcm_buffer_read(
        &machine.pcm, pcm_output, PCM_CAPACITY);
    size_t index;

    for (index = 0U; index < count; ++index) {
        *hash = hash_frame(*hash, pcm_output[index]);
    }
    *frame_count += count;
}

static int run_machine_pcm(const uint64_t *chunks, size_t chunk_count,
                           machine_pcm_result *result)
{
    const mipsnes2_master_clock final_horizon = UINT64_C(1500019);
    mipsnes2_spc700_registers smp_registers = {0};
    mipsnes2_machine_config config;
    mipsnes2_cpu_timing_report report;
    mipsnes2_cpu_timing_result timing_result;
    mipsnes2_master_clock horizon = 0U;
    uint64_t hash = UINT64_C(14695981039346656037);
    uint64_t frame_count = 0U;
    size_t chunk_index = 0U;
    size_t sequences;

    CHECK(chunks != NULL && chunk_count != 0U && result != NULL);
    CHECK(make_image() == 0);
    CHECK(make_ipl() == 0);
    (void)memset(wram, 0, sizeof(wram));
    (void)memset(sram, 0, sizeof(sram));
    (void)memset(aram, 0, sizeof(aram));
    (void)memset(&config, 0, sizeof(config));
    smp_registers.pc = UINT16_C(0xffc0);
    config.image = rom;
    config.image_size = sizeof(rom);
    config.copier_policy = MIPSNES2_COPIER_HEADER_REJECT;
    config.wram = wram;
    config.wram_size = sizeof(wram);
    config.sram = sram;
    config.sram_capacity = sizeof(sram);
    config.aram = aram;
    config.aram_size = sizeof(aram);
    config.smp_ipl = synthetic_ipl;
    config.smp_ipl_size = sizeof(synthetic_ipl);
    config.smp_initial_registers = &smp_registers;
    config.pcm_frames = pcm_storage;
    config.pcm_capacity = PCM_CAPACITY;
    config.standard = MIPSNES2_HV_STANDARD_NTSC;
    CHECK(mipsnes2_machine_init(&machine, &config) == MIPSNES2_MACHINE_OK);
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
        drain_pcm(&hash, &frame_count);
        horizon = next;
        ++chunk_index;
    }
    result->pcm_hash = hash;
    result->frame_count = frame_count;
    result->sample_count = machine.dsp.sample_count;
    result->smp_cycles = machine.dsp.smp_cycles;
    result->master_clocks = machine.apu_timing.master_clocks;
    result->enqueued_frames = machine.pcm.enqueued_frames;
    result->consumed_frames = machine.pcm.consumed_frames;
    result->dropped_frames = machine.pcm.dropped_frames;
    result->noise_lfsr = machine.dsp.noise_lfsr;
    result->envelope_counter = machine.dsp.envelope_counter;
    result->last_left = machine.dsp.output.dac_sample.left;
    result->last_right = machine.dsp.output.dac_sample.right;
    result->sample_phase = machine.dsp.sample_phase;
    result->wait_state = machine.smp_cpu.wait_state;
    CHECK(machine.pcm.count == 0U);
    CHECK(machine.apu_timing.master_clocks == final_horizon);
    CHECK(machine.scheduler.now == final_horizon);
    CHECK(machine.dsp.registers[MIPSNES2_DSP_REGISTER_FLG] ==
          UINT8_C(0x1f));
    CHECK(machine.dsp.registers[MIPSNES2_DSP_REGISTER_NON] == UINT8_C(1));
    CHECK(machine.dsp.registers[MIPSNES2_DSP_VOICE_REGISTER_GAIN] ==
          UINT8_C(0x50));
    return 0;
}

int main(void)
{
    static const uint64_t regular_chunks[] = {UINT64_C(50000)};
    static const uint64_t irregular_chunks[] = {
        UINT64_C(1), UINT64_C(31), UINT64_C(4096), UINT64_C(49999),
        UINT64_C(777), UINT64_C(32000), UINT64_C(17), UINT64_C(45000)
    };
    machine_pcm_result regular;
    machine_pcm_result irregular;

    CHECK(run_machine_pcm(regular_chunks,
                          sizeof(regular_chunks) /
                              sizeof(regular_chunks[0]),
                          &regular) == 0);
    CHECK(run_machine_pcm(irregular_chunks,
                          sizeof(irregular_chunks) /
                              sizeof(irregular_chunks[0]),
                          &irregular) == 0);
    CHECK(regular.pcm_hash == irregular.pcm_hash);
    CHECK(regular.frame_count == irregular.frame_count);
    CHECK(regular.sample_count == irregular.sample_count);
    CHECK(regular.smp_cycles == irregular.smp_cycles);
    CHECK(regular.master_clocks == irregular.master_clocks);
    CHECK(regular.enqueued_frames == irregular.enqueued_frames);
    CHECK(regular.consumed_frames == irregular.consumed_frames);
    CHECK(regular.dropped_frames == irregular.dropped_frames);
    CHECK(regular.noise_lfsr == irregular.noise_lfsr);
    CHECK(regular.envelope_counter == irregular.envelope_counter);
    CHECK(regular.last_left == irregular.last_left);
    CHECK(regular.last_right == irregular.last_right);
    CHECK(regular.sample_phase == irregular.sample_phase);
    CHECK(regular.wait_state == irregular.wait_state);
    CHECK(regular.frame_count == regular.sample_count);
    CHECK(regular.enqueued_frames == regular.frame_count);
    CHECK(regular.consumed_frames == regular.frame_count);
    CHECK(regular.dropped_frames == 0U);
    CHECK(regular.wait_state == MIPSNES2_SPC700_SLEEPING);
    CHECK(regular.last_left != 0 || regular.last_right != 0);
    if (regular.pcm_hash != UINT64_C(0x412f0b445f6c4610) ||
        regular.frame_count != UINT64_C(2234)) {
        (void)fprintf(stderr,
                      "machine PCM hash: %016llx; frames: %llu\n",
                      (unsigned long long)regular.pcm_hash,
                      (unsigned long long)regular.frame_count);
    }
    CHECK(regular.pcm_hash == UINT64_C(0x412f0b445f6c4610));
    CHECK(regular.frame_count == UINT64_C(2234));
    (void)puts("machine APU PCM integration: OK");
    return 0;
}
