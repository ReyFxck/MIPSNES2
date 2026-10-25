/* Synthetic SPC700 execution-foundation fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/apu/spc700/cpu.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t ram[MIPSNES2_SMP_RAM_SIZE];
static uint8_t synthetic_ipl[MIPSNES2_SMP_IPL_SIZE];

static int same_registers(const mipsnes2_spc700_registers *left,
                          const mipsnes2_spc700_registers *right)
{
    return left->pc == right->pc && left->a == right->a &&
           left->x == right->x && left->y == right->y &&
           left->sp == right->sp && left->psw == right->psw;
}

static int supported_opcode(uint8_t opcode)
{
    switch (opcode) {
    case UINT8_C(0x00):
    case UINT8_C(0x20):
    case UINT8_C(0x40):
    case UINT8_C(0x60):
    case UINT8_C(0x80):
    case UINT8_C(0xa0):
    case UINT8_C(0xc0):
    case UINT8_C(0xe0):
    case UINT8_C(0xed):
        return 1;
    default:
        return 0;
    }
}

int main(void)
{
    static const struct {
        uint8_t opcode;
        uint8_t initial_psw;
        uint8_t expected_psw;
        uint8_t cycles;
    } cases[] = {
        {UINT8_C(0x00), UINT8_C(0xa5), UINT8_C(0xa5), UINT8_C(2)},
        {UINT8_C(0x20), UINT8_C(0xff), UINT8_C(0xdf), UINT8_C(2)},
        {UINT8_C(0x40), UINT8_C(0x00), UINT8_C(0x20), UINT8_C(2)},
        {UINT8_C(0x60), UINT8_C(0xff), UINT8_C(0xfe), UINT8_C(2)},
        {UINT8_C(0x80), UINT8_C(0x00), UINT8_C(0x01), UINT8_C(2)},
        {UINT8_C(0xa0), UINT8_C(0x00), UINT8_C(0x04), UINT8_C(3)},
        {UINT8_C(0xc0), UINT8_C(0xff), UINT8_C(0xfb), UINT8_C(3)},
        {UINT8_C(0xe0), UINT8_C(0xff), UINT8_C(0xb7), UINT8_C(2)},
        {UINT8_C(0xed), UINT8_C(0x00), UINT8_C(0x01), UINT8_C(3)},
        {UINT8_C(0xed), UINT8_C(0xff), UINT8_C(0xfe), UINT8_C(3)}
    };
    mipsnes2_apu_io_ports ports;
    mipsnes2_smp_memory memory;
    mipsnes2_smp_memory no_ipl_memory;
    mipsnes2_spc700 cpu;
    mipsnes2_spc700 blank_cpu;
    mipsnes2_spc700_registers initial;
    mipsnes2_spc700_registers before;
    unsigned i;

    (void)memset(ram, UINT8_C(0xcc), sizeof(ram));
    (void)memset(synthetic_ipl, UINT8_C(0x01), sizeof(synthetic_ipl));
    mipsnes2_apu_io_ports_init(&ports);
    CHECK(mipsnes2_smp_memory_init(&memory, ram, sizeof(ram), &ports,
                                   synthetic_ipl, sizeof(synthetic_ipl)));

    initial.pc = UINT16_C(0x2345);
    initial.a = UINT8_C(0x12);
    initial.x = UINT8_C(0x34);
    initial.y = UINT8_C(0x56);
    initial.sp = UINT8_C(0x78);
    initial.psw = UINT8_C(0x9a);
    (void)memset(&blank_cpu, 0, sizeof(blank_cpu));
    CHECK(mipsnes2_spc700_step(NULL) ==
          MIPSNES2_SPC700_INVALID_ARGUMENT);
    CHECK(mipsnes2_spc700_step(&blank_cpu) == MIPSNES2_SPC700_NOT_READY);
    CHECK(!mipsnes2_spc700_init(NULL, &memory, &initial));
    CHECK(!mipsnes2_spc700_init(&cpu, NULL, &initial));
    CHECK(!cpu.initialized);
    CHECK(!mipsnes2_spc700_init(&cpu, &memory, NULL));
    CHECK(!cpu.initialized);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(cpu.initialized && cpu.memory == &memory &&
          same_registers(&cpu.registers, &initial) &&
          cpu.instruction_cycles == 0U && !cpu.has_fetched_opcode);

    /* Every byte reaches a deterministic dispatch result. Unsupported values
     * retain the programmer-visible state and timer phase. */
    for (i = 0U; i <= UINT8_MAX; ++i) {
        if (supported_opcode((uint8_t)i)) {
            continue;
        }
        ram[initial.pc] = (uint8_t)i;
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        before = cpu.registers;
        CHECK(mipsnes2_spc700_step(&cpu) ==
              MIPSNES2_SPC700_UNIMPLEMENTED_OPCODE);
        CHECK(same_registers(&cpu.registers, &before));
        CHECK(cpu.instruction_cycles == 0U && cpu.has_fetched_opcode);
        CHECK(cpu.last_opcode == (uint8_t)i &&
              cpu.last_opcode_address == initial.pc);
    }

    /* The first implied instruction family changes only documented PSW bits. */
    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        initial.psw = cases[i].initial_psw;
        ram[initial.pc] = cases[i].opcode;
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.registers.pc == (uint16_t)(initial.pc + UINT16_C(1)));
        CHECK(cpu.registers.a == initial.a && cpu.registers.x == initial.x &&
              cpu.registers.y == initial.y && cpu.registers.sp == initial.sp);
        CHECK(cpu.registers.psw == cases[i].expected_psw);
        CHECK(cpu.instruction_cycles == cases[i].cycles);
        CHECK(cpu.last_opcode == cases[i].opcode &&
              cpu.last_opcode_address == initial.pc);
    }

    /* Fetch observes the IPL overlay and the 16-bit PC wraps. */
    initial.psw = UINT8_C(0x5a);
    initial.pc = UINT16_C(0xffc0);
    synthetic_ipl[0] = UINT8_C(0x00);
    ram[0xffc0] = UINT8_C(0x01);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0xffc1) &&
          cpu.last_opcode == UINT8_C(0x00));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1), 0U));
    initial.pc = UINT16_C(0xffff);
    ram[0xffff] = UINT8_C(0x00);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == 0U);

    /* Instruction cycles feed the existing timer block exactly once. */
    CHECK(mipsnes2_smp_memory_reset(&memory));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00fc),
                                    UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x84)));
    initial.pc = UINT16_C(0x3000);
    for (i = 0U; i < 8U; ++i) {
        ram[initial.pc + i] = UINT8_C(0x00);
    }
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    for (i = 0U; i < 8U; ++i) {
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    }
    CHECK(cpu.instruction_cycles == UINT64_C(16));
    CHECK(memory.timers.output[2] == UINT8_C(1));

    /* Overflow and unavailable mapped bytes fail before architectural commit. */
    CHECK(mipsnes2_smp_memory_reset(&memory));
    initial.pc = UINT16_C(0x3000);
    ram[initial.pc] = UINT8_C(0x00);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    cpu.instruction_cycles = UINT64_MAX - UINT64_C(1);
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_CYCLE_OVERFLOW);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == UINT64_MAX - UINT64_C(1));
    CHECK(memory.timers.stage1_phase == 0U);

    CHECK(mipsnes2_smp_memory_init(&no_ipl_memory, ram, sizeof(ram), &ports,
                                   NULL, 0U));
    initial.pc = UINT16_C(0xffc0);
    CHECK(mipsnes2_spc700_init(&cpu, &no_ipl_memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(!cpu.has_fetched_opcode && cpu.instruction_cycles == 0U);

    (void)puts("SPC700 register, fetch and initial dispatch foundation: OK");
    return 0;
}
