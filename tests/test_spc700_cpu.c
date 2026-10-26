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
    case UINT8_C(0x5d):
    case UINT8_C(0x60):
    case UINT8_C(0x7d):
    case UINT8_C(0x80):
    case UINT8_C(0x8d):
    case UINT8_C(0x9d):
    case UINT8_C(0xa0):
    case UINT8_C(0xbd):
    case UINT8_C(0xc0):
    case UINT8_C(0xc4):
    case UINT8_C(0xc5):
    case UINT8_C(0xc9):
    case UINT8_C(0xcb):
    case UINT8_C(0xcc):
    case UINT8_C(0xcd):
    case UINT8_C(0xd4):
    case UINT8_C(0xd5):
    case UINT8_C(0xd6):
    case UINT8_C(0xd8):
    case UINT8_C(0xd9):
    case UINT8_C(0xdb):
    case UINT8_C(0xdd):
    case UINT8_C(0xe0):
    case UINT8_C(0xe4):
    case UINT8_C(0xe5):
    case UINT8_C(0xe8):
    case UINT8_C(0xe9):
    case UINT8_C(0xeb):
    case UINT8_C(0xec):
    case UINT8_C(0xed):
    case UINT8_C(0xf4):
    case UINT8_C(0xf5):
    case UINT8_C(0xf6):
    case UINT8_C(0xf8):
    case UINT8_C(0xf9):
    case UINT8_C(0xfb):
    case UINT8_C(0xfd):
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
    uint8_t expected_psw;
    uint8_t observed;
    unsigned i;
    unsigned page_index;
    unsigned value_index;

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

    /* Immediate MOV loads A/X/Y, consumes two bytes and derives only N/Z. */
    {
        static const uint8_t opcodes[] = {
            UINT8_C(0xe8), UINT8_C(0xcd), UINT8_C(0x8d)
        };
        static const uint8_t values[] = {
            UINT8_C(0x00), UINT8_C(0x7f), UINT8_C(0x80)
        };

        initial.pc = UINT16_C(0x2400);
        initial.psw = UINT8_C(0xff);
        for (i = 0U; i < sizeof(opcodes) / sizeof(opcodes[0]); ++i) {
            for (value_index = 0U;
                 value_index < sizeof(values) / sizeof(values[0]);
                 ++value_index) {
                ram[initial.pc] = opcodes[i];
                ram[initial.pc + UINT16_C(1)] = values[value_index];
                CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
                expected_psw = UINT8_C(0x7d);
                if (values[value_index] == 0U) {
                    expected_psw |= MIPSNES2_SPC700_FLAG_Z;
                } else if ((values[value_index] & UINT8_C(0x80)) != 0U) {
                    expected_psw |= MIPSNES2_SPC700_FLAG_N;
                }
                CHECK(cpu.registers.psw == expected_psw);
                CHECK(cpu.registers.pc == UINT16_C(0x2402));
                CHECK(cpu.instruction_cycles == UINT64_C(2));
                if (opcodes[i] == UINT8_C(0xe8)) {
                    CHECK(cpu.registers.a == values[value_index] &&
                          cpu.registers.x == initial.x &&
                          cpu.registers.y == initial.y);
                } else if (opcodes[i] == UINT8_C(0xcd)) {
                    CHECK(cpu.registers.x == values[value_index] &&
                          cpu.registers.a == initial.a &&
                          cpu.registers.y == initial.y);
                } else {
                    CHECK(cpu.registers.y == values[value_index] &&
                          cpu.registers.a == initial.a &&
                          cpu.registers.x == initial.x);
                }
            }
        }
    }

    /* Register MOV forms transfer the documented source and derive N/Z except
     * for MOV SP,X, which preserves the complete PSW. */
    initial.pc = UINT16_C(0x2500);
    initial.psw = UINT8_C(0xff);
    initial.a = UINT8_C(0x00);
    initial.x = UINT8_C(0x7f);
    initial.y = UINT8_C(0x80);
    initial.sp = UINT8_C(0x80);
    ram[initial.pc] = UINT8_C(0x7d);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x7f) &&
          cpu.registers.psw == UINT8_C(0x7d));

    ram[initial.pc] = UINT8_C(0xdd);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x80) &&
          cpu.registers.psw == UINT8_C(0xfd));

    ram[initial.pc] = UINT8_C(0x5d);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.x == UINT8_C(0x00) &&
          cpu.registers.psw == UINT8_C(0x7f));

    ram[initial.pc] = UINT8_C(0xfd);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.y == UINT8_C(0x00) &&
          cpu.registers.psw == UINT8_C(0x7f));

    ram[initial.pc] = UINT8_C(0x9d);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.x == UINT8_C(0x80) &&
          cpu.registers.psw == UINT8_C(0xfd));

    ram[initial.pc] = UINT8_C(0xbd);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.sp == UINT8_C(0x7f) &&
          cpu.registers.psw == UINT8_C(0xff));

    /* Direct-page loads select page 0/1 with P, consume two bytes and derive
     * only N/Z from the mapped byte. */
    {
        static const struct {
            uint8_t opcode;
            unsigned target;
        } loads[] = {
            {UINT8_C(0xe4), 0U},
            {UINT8_C(0xf8), 1U},
            {UINT8_C(0xeb), 2U}
        };
        static const uint8_t values[] = {
            UINT8_C(0x00), UINT8_C(0x7f), UINT8_C(0x80)
        };

        initial.pc = UINT16_C(0x2600);
        initial.a = UINT8_C(0x12);
        initial.x = UINT8_C(0x34);
        initial.y = UINT8_C(0x56);
        initial.sp = UINT8_C(0x78);
        for (page_index = 0U; page_index < 2U; ++page_index) {
            initial.psw = page_index == 0U
                ? UINT8_C(0xdf) : UINT8_C(0xff);
            for (i = 0U; i < sizeof(loads) / sizeof(loads[0]); ++i) {
                for (value_index = 0U;
                     value_index < sizeof(values) / sizeof(values[0]);
                     ++value_index) {
                    ram[initial.pc] = loads[i].opcode;
                    ram[initial.pc + UINT16_C(1)] = UINT8_C(0x22);
                    ram[UINT16_C(0x0022) +
                        (uint16_t)(page_index * UINT16_C(0x0100))] =
                        values[value_index];
                    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
                    expected_psw = (uint8_t)(
                        initial.psw &
                        (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
                                   MIPSNES2_SPC700_FLAG_Z));
                    if (values[value_index] == 0U) {
                        expected_psw |= MIPSNES2_SPC700_FLAG_Z;
                    } else if ((values[value_index] & UINT8_C(0x80)) != 0U) {
                        expected_psw |= MIPSNES2_SPC700_FLAG_N;
                    }
                    CHECK(cpu.registers.pc == UINT16_C(0x2602));
                    CHECK(cpu.registers.psw == expected_psw);
                    CHECK(cpu.instruction_cycles == UINT64_C(3));
                    if (loads[i].target == 0U) {
                        CHECK(cpu.registers.a == values[value_index] &&
                              cpu.registers.x == initial.x &&
                              cpu.registers.y == initial.y);
                    } else if (loads[i].target == 1U) {
                        CHECK(cpu.registers.x == values[value_index] &&
                              cpu.registers.a == initial.a &&
                              cpu.registers.y == initial.y);
                    } else {
                        CHECK(cpu.registers.y == values[value_index] &&
                              cpu.registers.a == initial.a &&
                              cpu.registers.x == initial.x);
                    }
                }
            }
        }
    }

    /* Direct-page stores select the same two pages, preserve every register
     * and flag, and use the documented four-cycle cost. */
    {
        static const struct {
            uint8_t opcode;
            uint8_t value;
        } stores[] = {
            {UINT8_C(0xc4), UINT8_C(0x12)},
            {UINT8_C(0xd8), UINT8_C(0x34)},
            {UINT8_C(0xcb), UINT8_C(0x56)}
        };

        initial.pc = UINT16_C(0x2700);
        initial.a = UINT8_C(0x12);
        initial.x = UINT8_C(0x34);
        initial.y = UINT8_C(0x56);
        initial.sp = UINT8_C(0x78);
        for (page_index = 0U; page_index < 2U; ++page_index) {
            initial.psw = page_index == 0U
                ? UINT8_C(0x5f) : UINT8_C(0x7f);
            for (i = 0U; i < sizeof(stores) / sizeof(stores[0]); ++i) {
                uint16_t address = (uint16_t)(
                    UINT16_C(0x0030) +
                    (uint16_t)(page_index * UINT16_C(0x0100)));

                ram[initial.pc] = stores[i].opcode;
                ram[initial.pc + UINT16_C(1)] = UINT8_C(0x30);
                ram[address] = UINT8_C(0xcc);
                CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
                CHECK(ram[address] == stores[i].value);
                CHECK(cpu.registers.pc == UINT16_C(0x2702));
                CHECK(cpu.registers.a == initial.a &&
                      cpu.registers.x == initial.x &&
                      cpu.registers.y == initial.y &&
                      cpu.registers.sp == initial.sp &&
                      cpu.registers.psw == initial.psw);
                CHECK(cpu.instruction_cycles == UINT64_C(4));
            }
        }
    }

    /* Indexed direct-page loads wrap the 8-bit offset before selecting page
     * 0/1, consume two bytes and derive only N/Z from the mapped byte. */
    {
        static const struct {
            uint8_t opcode;
            uint8_t index;
            unsigned target;
        } loads[] = {
            {UINT8_C(0xf4), UINT8_C(0x34), 0U},
            {UINT8_C(0xf9), UINT8_C(0x56), 1U},
            {UINT8_C(0xfb), UINT8_C(0x34), 2U}
        };
        static const uint8_t values[] = {
            UINT8_C(0x00), UINT8_C(0x7f), UINT8_C(0x80)
        };

        initial.pc = UINT16_C(0x2780);
        initial.a = UINT8_C(0x12);
        initial.x = UINT8_C(0x34);
        initial.y = UINT8_C(0x56);
        initial.sp = UINT8_C(0x78);
        for (page_index = 0U; page_index < 2U; ++page_index) {
            initial.psw = page_index == 0U
                ? UINT8_C(0xdf) : UINT8_C(0xff);
            for (i = 0U; i < sizeof(loads) / sizeof(loads[0]); ++i) {
                uint16_t address = (uint16_t)(
                    (uint8_t)(UINT8_C(0xf0) + loads[i].index) |
                    (uint16_t)(page_index * UINT16_C(0x0100)));

                for (value_index = 0U;
                     value_index < sizeof(values) / sizeof(values[0]);
                     ++value_index) {
                    ram[initial.pc] = loads[i].opcode;
                    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
                    ram[address] = values[value_index];
                    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                    CHECK(mipsnes2_spc700_step(&cpu) ==
                          MIPSNES2_SPC700_OK);
                    expected_psw = (uint8_t)(
                        initial.psw &
                        (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
                                   MIPSNES2_SPC700_FLAG_Z));
                    if (values[value_index] == 0U) {
                        expected_psw |= MIPSNES2_SPC700_FLAG_Z;
                    } else if ((values[value_index] & UINT8_C(0x80)) != 0U) {
                        expected_psw |= MIPSNES2_SPC700_FLAG_N;
                    }
                    CHECK(cpu.registers.pc == UINT16_C(0x2782));
                    CHECK(cpu.registers.psw == expected_psw);
                    CHECK(cpu.instruction_cycles == UINT64_C(4));
                    if (loads[i].target == 0U) {
                        CHECK(cpu.registers.a == values[value_index] &&
                              cpu.registers.x == initial.x &&
                              cpu.registers.y == initial.y);
                    } else if (loads[i].target == 1U) {
                        CHECK(cpu.registers.x == values[value_index] &&
                              cpu.registers.a == initial.a &&
                              cpu.registers.y == initial.y);
                    } else {
                        CHECK(cpu.registers.y == values[value_index] &&
                              cpu.registers.a == initial.a &&
                              cpu.registers.x == initial.x);
                    }
                }
            }
        }
    }

    /* Indexed direct-page stores use the same wrapped address, preserve CPU
     * state apart from PC, and use the documented five-cycle cost. */
    {
        static const struct {
            uint8_t opcode;
            uint8_t index;
            uint8_t value;
        } stores[] = {
            {UINT8_C(0xd4), UINT8_C(0x34), UINT8_C(0x12)},
            {UINT8_C(0xd9), UINT8_C(0x56), UINT8_C(0x34)},
            {UINT8_C(0xdb), UINT8_C(0x34), UINT8_C(0x56)}
        };

        initial.pc = UINT16_C(0x27c0);
        initial.a = UINT8_C(0x12);
        initial.x = UINT8_C(0x34);
        initial.y = UINT8_C(0x56);
        initial.sp = UINT8_C(0x78);
        for (page_index = 0U; page_index < 2U; ++page_index) {
            initial.psw = page_index == 0U
                ? UINT8_C(0x5f) : UINT8_C(0x7f);
            for (i = 0U; i < sizeof(stores) / sizeof(stores[0]); ++i) {
                uint16_t address = (uint16_t)(
                    (uint8_t)(UINT8_C(0xf0) + stores[i].index) |
                    (uint16_t)(page_index * UINT16_C(0x0100)));

                ram[initial.pc] = stores[i].opcode;
                ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
                ram[address] = UINT8_C(0xcc);
                CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                CHECK(mipsnes2_spc700_step(&cpu) ==
                      MIPSNES2_SPC700_OK);
                CHECK(ram[address] == stores[i].value);
                CHECK(cpu.registers.pc == UINT16_C(0x27c2));
                CHECK(cpu.registers.a == initial.a &&
                      cpu.registers.x == initial.x &&
                      cpu.registers.y == initial.y &&
                      cpu.registers.sp == initial.sp &&
                      cpu.registers.psw == initial.psw);
                CHECK(cpu.instruction_cycles == UINT64_C(5));
            }
        }
    }

    /* Absolute-indexed loads add X/Y to the full 16-bit operand with wrap,
     * ignore PSW.P, consume three bytes and derive only N/Z. */
    {
        static const struct {
            uint8_t opcode;
            uint8_t index;
            uint16_t address;
        } loads[] = {
            {UINT8_C(0xf5), UINT8_C(0x20), UINT16_C(0x0010)},
            {UINT8_C(0xf6), UINT8_C(0x30), UINT16_C(0x0020)}
        };
        static const uint8_t values[] = {
            UINT8_C(0x00), UINT8_C(0x7f), UINT8_C(0x80)
        };

        initial.pc = UINT16_C(0x27d0);
        initial.a = UINT8_C(0x12);
        initial.x = UINT8_C(0x20);
        initial.y = UINT8_C(0x30);
        initial.sp = UINT8_C(0x78);
        for (page_index = 0U; page_index < 2U; ++page_index) {
            initial.psw = page_index == 0U
                ? UINT8_C(0xdf) : UINT8_C(0xff);
            for (i = 0U; i < sizeof(loads) / sizeof(loads[0]); ++i) {
                CHECK(loads[i].index ==
                      (loads[i].opcode == UINT8_C(0xf5)
                           ? initial.x : initial.y));
                for (value_index = 0U;
                     value_index < sizeof(values) / sizeof(values[0]);
                     ++value_index) {
                    ram[initial.pc] = loads[i].opcode;
                    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
                    ram[initial.pc + UINT16_C(2)] = UINT8_C(0xff);
                    ram[loads[i].address] = values[value_index];
                    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                    CHECK(mipsnes2_spc700_step(&cpu) ==
                          MIPSNES2_SPC700_OK);
                    expected_psw = (uint8_t)(
                        initial.psw &
                        (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
                                   MIPSNES2_SPC700_FLAG_Z));
                    if (values[value_index] == 0U) {
                        expected_psw |= MIPSNES2_SPC700_FLAG_Z;
                    } else if ((values[value_index] & UINT8_C(0x80)) != 0U) {
                        expected_psw |= MIPSNES2_SPC700_FLAG_N;
                    }
                    CHECK(cpu.registers.pc == UINT16_C(0x27d3));
                    CHECK(cpu.registers.a == values[value_index]);
                    CHECK(cpu.registers.x == initial.x &&
                          cpu.registers.y == initial.y &&
                          cpu.registers.sp == initial.sp &&
                          cpu.registers.psw == expected_psw);
                    CHECK(cpu.instruction_cycles == UINT64_C(5));
                }
            }
        }
    }

    /* Absolute-indexed stores use the same 16-bit wrap, preserve registers
     * and flags, and use the documented six-cycle cost. */
    {
        static const struct {
            uint8_t opcode;
            uint16_t address;
        } stores[] = {
            {UINT8_C(0xd5), UINT16_C(0x0010)},
            {UINT8_C(0xd6), UINT16_C(0x0020)}
        };

        initial.pc = UINT16_C(0x27d8);
        initial.a = UINT8_C(0x5a);
        initial.x = UINT8_C(0x20);
        initial.y = UINT8_C(0x30);
        initial.sp = UINT8_C(0x78);
        for (page_index = 0U; page_index < 2U; ++page_index) {
            initial.psw = page_index == 0U
                ? UINT8_C(0x5f) : UINT8_C(0x7f);
            for (i = 0U; i < sizeof(stores) / sizeof(stores[0]); ++i) {
                ram[initial.pc] = stores[i].opcode;
                ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
                ram[initial.pc + UINT16_C(2)] = UINT8_C(0xff);
                ram[stores[i].address] = UINT8_C(0xcc);
                CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                CHECK(mipsnes2_spc700_step(&cpu) ==
                      MIPSNES2_SPC700_OK);
                CHECK(ram[stores[i].address] == UINT8_C(0x5a));
                CHECK(cpu.registers.pc == UINT16_C(0x27db));
                CHECK(cpu.registers.a == initial.a &&
                      cpu.registers.x == initial.x &&
                      cpu.registers.y == initial.y &&
                      cpu.registers.sp == initial.sp &&
                      cpu.registers.psw == initial.psw);
                CHECK(cpu.instruction_cycles == UINT64_C(6));
            }
        }
    }

    /* Absolute loads fetch a little-endian 16-bit address, ignore PSW.P,
     * consume three bytes and derive only N/Z from the mapped byte. */
    {
        static const struct {
            uint8_t opcode;
            unsigned target;
        } loads[] = {
            {UINT8_C(0xe5), 0U},
            {UINT8_C(0xe9), 1U},
            {UINT8_C(0xec), 2U}
        };
        static const uint8_t values[] = {
            UINT8_C(0x00), UINT8_C(0x7f), UINT8_C(0x80)
        };

        initial.pc = UINT16_C(0x27e0);
        initial.a = UINT8_C(0x12);
        initial.x = UINT8_C(0x34);
        initial.y = UINT8_C(0x56);
        initial.sp = UINT8_C(0x78);
        for (page_index = 0U; page_index < 2U; ++page_index) {
            initial.psw = page_index == 0U
                ? UINT8_C(0xdf) : UINT8_C(0xff);
            for (i = 0U; i < sizeof(loads) / sizeof(loads[0]); ++i) {
                for (value_index = 0U;
                     value_index < sizeof(values) / sizeof(values[0]);
                     ++value_index) {
                    ram[initial.pc] = loads[i].opcode;
                    ram[initial.pc + UINT16_C(1)] = UINT8_C(0x56);
                    ram[initial.pc + UINT16_C(2)] = UINT8_C(0x34);
                    ram[UINT16_C(0x3456)] = values[value_index];
                    ram[UINT16_C(0x5634)] = UINT8_C(0xcc);
                    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                    CHECK(mipsnes2_spc700_step(&cpu) ==
                          MIPSNES2_SPC700_OK);
                    expected_psw = (uint8_t)(
                        initial.psw &
                        (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
                                   MIPSNES2_SPC700_FLAG_Z));
                    if (values[value_index] == 0U) {
                        expected_psw |= MIPSNES2_SPC700_FLAG_Z;
                    } else if ((values[value_index] & UINT8_C(0x80)) != 0U) {
                        expected_psw |= MIPSNES2_SPC700_FLAG_N;
                    }
                    CHECK(cpu.registers.pc == UINT16_C(0x27e3));
                    CHECK(cpu.registers.psw == expected_psw);
                    CHECK(cpu.instruction_cycles == UINT64_C(4));
                    if (loads[i].target == 0U) {
                        CHECK(cpu.registers.a == values[value_index] &&
                              cpu.registers.x == initial.x &&
                              cpu.registers.y == initial.y);
                    } else if (loads[i].target == 1U) {
                        CHECK(cpu.registers.x == values[value_index] &&
                              cpu.registers.a == initial.a &&
                              cpu.registers.y == initial.y);
                    } else {
                        CHECK(cpu.registers.y == values[value_index] &&
                              cpu.registers.a == initial.a &&
                              cpu.registers.x == initial.x);
                    }
                }
            }
        }
    }

    /* Absolute stores use the same little-endian address, preserve every
     * register and flag, and use the documented five-cycle cost. */
    {
        static const struct {
            uint8_t opcode;
            uint8_t value;
        } stores[] = {
            {UINT8_C(0xc5), UINT8_C(0x12)},
            {UINT8_C(0xc9), UINT8_C(0x34)},
            {UINT8_C(0xcc), UINT8_C(0x56)}
        };

        initial.pc = UINT16_C(0x27f0);
        initial.a = UINT8_C(0x12);
        initial.x = UINT8_C(0x34);
        initial.y = UINT8_C(0x56);
        initial.sp = UINT8_C(0x78);
        for (page_index = 0U; page_index < 2U; ++page_index) {
            initial.psw = page_index == 0U
                ? UINT8_C(0x5f) : UINT8_C(0x7f);
            for (i = 0U; i < sizeof(stores) / sizeof(stores[0]); ++i) {
                ram[initial.pc] = stores[i].opcode;
                ram[initial.pc + UINT16_C(1)] = UINT8_C(0x56);
                ram[initial.pc + UINT16_C(2)] = UINT8_C(0x34);
                ram[UINT16_C(0x3456)] = UINT8_C(0xcc);
                ram[UINT16_C(0x5634)] = UINT8_C(0xa5);
                CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                CHECK(mipsnes2_spc700_step(&cpu) ==
                      MIPSNES2_SPC700_OK);
                CHECK(ram[UINT16_C(0x3456)] == stores[i].value);
                CHECK(ram[UINT16_C(0x5634)] == UINT8_C(0xa5));
                CHECK(cpu.registers.pc == UINT16_C(0x27f3));
                CHECK(cpu.registers.a == initial.a &&
                      cpu.registers.x == initial.x &&
                      cpu.registers.y == initial.y &&
                      cpu.registers.sp == initial.sp &&
                      cpu.registers.psw == initial.psw);
                CHECK(cpu.instruction_cycles == UINT64_C(5));
            }
        }
    }

    /* Page-zero direct addressing reaches the existing S-SMP MMIO map. */
    initial.pc = UINT16_C(0x2800);
    initial.a = UINT8_C(0x11);
    initial.x = UINT8_C(0x22);
    initial.y = UINT8_C(0x33);
    initial.sp = UINT8_C(0x44);
    initial.psw = UINT8_C(0x00);
    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x2140),
                                          UINT8_C(0x80)));
    ram[initial.pc] = UINT8_C(0xe4);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x80) &&
          cpu.registers.psw == MIPSNES2_SPC700_FLAG_N);

    initial.a = UINT8_C(0x5a);
    ram[initial.pc] = UINT8_C(0xc4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0x5a));

    /* With P set, the same operand reaches page-1 ARAM, not page-zero MMIO. */
    initial.a = UINT8_C(0x11);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    ram[UINT16_C(0x01f4)] = UINT8_C(0x7f);
    ram[initial.pc] = UINT8_C(0xe4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x7f) &&
          cpu.registers.psw == MIPSNES2_SPC700_FLAG_P);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0x5a));

    /* Wrapped indexed page-zero addressing also reaches MMIO, while P routes
     * the same effective offset to page-1 ARAM. */
    initial.pc = UINT16_C(0x2880);
    initial.a = UINT8_C(0x11);
    initial.x = UINT8_C(0x04);
    initial.y = UINT8_C(0x33);
    initial.sp = UINT8_C(0x44);
    initial.psw = UINT8_C(0x00);
    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x2140),
                                          UINT8_C(0x80)));
    ram[initial.pc] = UINT8_C(0xf4);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x80) &&
          cpu.registers.psw == MIPSNES2_SPC700_FLAG_N);

    initial.a = UINT8_C(0xa5);
    ram[initial.pc] = UINT8_C(0xd4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0xa5));

    initial.a = UINT8_C(0x11);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    ram[UINT16_C(0x01f4)] = UINT8_C(0x7f);
    ram[initial.pc] = UINT8_C(0xf4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x7f) &&
          cpu.registers.psw == MIPSNES2_SPC700_FLAG_P);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0xa5));

    initial.a = UINT8_C(0x3c);
    ram[initial.pc] = UINT8_C(0xd4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[UINT16_C(0x01f4)] == UINT8_C(0x3c));
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0xa5));

    /* Absolute addressing reaches page-zero MMIO without consulting PSW.P;
     * an explicit page-one address remains ordinary ARAM. */
    initial.pc = UINT16_C(0x28c0);
    initial.a = UINT8_C(0x11);
    initial.x = UINT8_C(0x22);
    initial.y = UINT8_C(0x33);
    initial.sp = UINT8_C(0x44);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x2140),
                                          UINT8_C(0x80)));
    ram[initial.pc] = UINT8_C(0xe5);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf4);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0x00);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x80) &&
          cpu.registers.psw ==
              (MIPSNES2_SPC700_FLAG_P | MIPSNES2_SPC700_FLAG_N));

    initial.a = UINT8_C(0x69);
    ram[initial.pc] = UINT8_C(0xc5);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0x69));

    initial.a = UINT8_C(0x11);
    ram[UINT16_C(0x01f4)] = UINT8_C(0x7f);
    ram[initial.pc] = UINT8_C(0xe5);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0x01);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x7f) &&
          cpu.registers.psw == MIPSNES2_SPC700_FLAG_P);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0x69));

    /* Absolute indexing is applied before mapped access, so either index can
     * reach page-zero MMIO and a page-one result remains ordinary ARAM. */
    initial.pc = UINT16_C(0x28e0);
    initial.a = UINT8_C(0x11);
    initial.x = UINT8_C(0x04);
    initial.y = UINT8_C(0x04);
    initial.sp = UINT8_C(0x44);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x2140),
                                          UINT8_C(0x80)));
    ram[initial.pc] = UINT8_C(0xf5);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0x00);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x80) &&
          cpu.registers.psw ==
              (MIPSNES2_SPC700_FLAG_P | MIPSNES2_SPC700_FLAG_N));

    initial.a = UINT8_C(0x96);
    ram[initial.pc] = UINT8_C(0xd6);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0x96));

    initial.a = UINT8_C(0x11);
    ram[UINT16_C(0x01f4)] = UINT8_C(0x7f);
    ram[initial.pc] = UINT8_C(0xf5);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0x01);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x7f) &&
          cpu.registers.psw == MIPSNES2_SPC700_FLAG_P);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0x96));

    /* An unimplemented page-zero MMIO target rejects either direction before
     * CPU/time commit. A rejected store does not alter its ARAM backing byte. */
    initial.pc = UINT16_C(0x2900);
    initial.psw = UINT8_C(0x00);
    ram[initial.pc] = UINT8_C(0xe4);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U);

    ram[initial.pc] = UINT8_C(0xc4);
    ram[0x00f0] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U && ram[0x00f0] == UINT8_C(0xcc));

    /* Indexed addressing reaches the same unavailable MMIO byte only after
     * wrapping the direct-page sum; either direction remains atomic. */
    initial.pc = UINT16_C(0x2980);
    initial.psw = UINT8_C(0x00);
    initial.x = UINT8_C(0x04);
    ram[initial.pc] = UINT8_C(0xf4);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xec);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U);

    ram[initial.pc] = UINT8_C(0xd4);
    ram[UINT16_C(0x00f0)] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U &&
          ram[UINT16_C(0x00f0)] == UINT8_C(0xcc));

    /* Indexed-absolute unavailable MMIO targets reject both paths atomically. */
    initial.pc = UINT16_C(0x29e0);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    initial.x = UINT8_C(0x04);
    initial.y = UINT8_C(0x04);
    ram[initial.pc] = UINT8_C(0xf5);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xec);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0x00);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U);

    ram[initial.pc] = UINT8_C(0xd6);
    ram[UINT16_C(0x00f0)] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U &&
          ram[UINT16_C(0x00f0)] == UINT8_C(0xcc));

    /* Absolute unavailable MMIO targets reject either direction atomically. */
    initial.pc = UINT16_C(0x29c0);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    ram[initial.pc] = UINT8_C(0xe5);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0x00);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U);

    ram[initial.pc] = UINT8_C(0xc5);
    ram[UINT16_C(0x00f0)] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U &&
          ram[UINT16_C(0x00f0)] == UINT8_C(0xcc));

    /* The immediate operand wraps independently through the mapped address. */
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1), 0U));
    initial.pc = UINT16_C(0xffff);
    initial.psw = 0U;
    ram[0xffff] = UINT8_C(0xe8);
    ram[0x0000] = UINT8_C(0x80);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x0001) &&
          cpu.registers.a == UINT8_C(0x80) &&
          cpu.registers.psw == MIPSNES2_SPC700_FLAG_N);

    /* Both bytes of an absolute operand wrap independently with the 16-bit PC. */
    initial.pc = UINT16_C(0xfffe);
    initial.psw = UINT8_C(0x00);
    ram[UINT16_C(0xfffe)] = UINT8_C(0xe5);
    ram[UINT16_C(0xffff)] = UINT8_C(0x34);
    ram[UINT16_C(0x0000)] = UINT8_C(0x12);
    ram[UINT16_C(0x1234)] = UINT8_C(0x80);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x0001) &&
          cpu.registers.a == UINT8_C(0x80) &&
          cpu.registers.psw == MIPSNES2_SPC700_FLAG_N);

    /* Fetch observes the IPL overlay and the 16-bit PC wraps. */
    initial.psw = UINT8_C(0x5a);
    initial.pc = UINT16_C(0xffc0);
    synthetic_ipl[0] = UINT8_C(0x00);
    ram[0xffc0] = UINT8_C(0x01);
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x80)));
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

    /* Register-MOV cycles feed the existing timer block exactly once. */
    CHECK(mipsnes2_smp_memory_reset(&memory));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00fc),
                                    UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x84)));
    initial.pc = UINT16_C(0x3000);
    for (i = 0U; i < 8U; ++i) {
        ram[initial.pc + i] = UINT8_C(0x7d);
    }
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    for (i = 0U; i < 8U; ++i) {
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    }
    CHECK(cpu.instruction_cycles == UINT64_C(16));
    CHECK(memory.timers.output[2] == UINT8_C(1));

    /* Four direct-page stores contribute their four cycles to timer 2. */
    CHECK(mipsnes2_smp_memory_reset(&memory));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00fc),
                                    UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x84)));
    initial.pc = UINT16_C(0x3100);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    initial.a = UINT8_C(0x5a);
    for (i = 0U; i < 4U; ++i) {
        ram[initial.pc + i * 2U] = UINT8_C(0xc4);
        ram[initial.pc + i * 2U + 1U] = (uint8_t)(UINT8_C(0x40) + i);
    }
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    for (i = 0U; i < 4U; ++i) {
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    }
    CHECK(cpu.instruction_cycles == UINT64_C(16));
    CHECK(memory.timers.output[2] == UINT8_C(1));
    for (i = 0U; i < 4U; ++i) {
        CHECK(ram[UINT16_C(0x0140) + i] == UINT8_C(0x5a));
    }

    /* Four indexed direct-page loads likewise contribute four cycles each. */
    CHECK(mipsnes2_smp_memory_reset(&memory));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00fc),
                                    UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x84)));
    initial.pc = UINT16_C(0x3180);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    initial.x = UINT8_C(0x20);
    for (i = 0U; i < 4U; ++i) {
        ram[initial.pc + i * 2U] = UINT8_C(0xf4);
        ram[initial.pc + i * 2U + 1U] = (uint8_t)(
            UINT8_C(0xe0) + i);
        ram[UINT16_C(0x0100) + i] = (uint8_t)i;
    }
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    for (i = 0U; i < 4U; ++i) {
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    }
    CHECK(cpu.instruction_cycles == UINT64_C(16));
    CHECK(memory.timers.output[2] == UINT8_C(1));
    CHECK(cpu.registers.a == UINT8_C(3));

    /* Two indexed-absolute loads and one store total one timer-2 edge. */
    CHECK(mipsnes2_smp_memory_reset(&memory));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00fc),
                                    UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x84)));
    initial.pc = UINT16_C(0x3240);
    initial.psw = UINT8_C(0x00);
    initial.x = UINT8_C(0x00);
    initial.y = UINT8_C(0x00);
    ram[UINT16_C(0x3240)] = UINT8_C(0xf5);
    ram[UINT16_C(0x3241)] = UINT8_C(0x40);
    ram[UINT16_C(0x3242)] = UINT8_C(0x02);
    ram[UINT16_C(0x3243)] = UINT8_C(0xf6);
    ram[UINT16_C(0x3244)] = UINT8_C(0x41);
    ram[UINT16_C(0x3245)] = UINT8_C(0x02);
    ram[UINT16_C(0x3246)] = UINT8_C(0xd5);
    ram[UINT16_C(0x3247)] = UINT8_C(0x42);
    ram[UINT16_C(0x3248)] = UINT8_C(0x02);
    ram[UINT16_C(0x0240)] = UINT8_C(0x12);
    ram[UINT16_C(0x0241)] = UINT8_C(0x5a);
    ram[UINT16_C(0x0242)] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.instruction_cycles == UINT64_C(16));
    CHECK(memory.timers.output[2] == UINT8_C(1));
    CHECK(cpu.registers.a == UINT8_C(0x5a));
    CHECK(ram[UINT16_C(0x0242)] == UINT8_C(0x5a));

    /* Four absolute loads contribute four cycles each to timer 2. */
    CHECK(mipsnes2_smp_memory_reset(&memory));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00fc),
                                    UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x84)));
    initial.pc = UINT16_C(0x3200);
    initial.psw = UINT8_C(0x00);
    for (i = 0U; i < 4U; ++i) {
        ram[initial.pc + i * 3U] = UINT8_C(0xe5);
        ram[initial.pc + i * 3U + 1U] = (uint8_t)(
            UINT8_C(0x40) + i);
        ram[initial.pc + i * 3U + 2U] = UINT8_C(0x02);
        ram[UINT16_C(0x0240) + i] = (uint8_t)i;
    }
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    for (i = 0U; i < 4U; ++i) {
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    }
    CHECK(cpu.instruction_cycles == UINT64_C(16));
    CHECK(memory.timers.output[2] == UINT8_C(1));
    CHECK(cpu.registers.a == UINT8_C(3));

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

    /* Store overflow is rejected before operand fetch or memory mutation. */
    initial.pc = UINT16_C(0x3000);
    initial.psw = 0U;
    initial.a = UINT8_C(0x5a);
    ram[0x3000] = UINT8_C(0xc4);
    ram[0x3001] = UINT8_C(0x30);
    ram[0x0030] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    cpu.instruction_cycles = UINT64_MAX - UINT64_C(3);
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_CYCLE_OVERFLOW);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == UINT64_MAX - UINT64_C(3));
    CHECK(ram[0x0030] == UINT8_C(0xcc));

    /* Indexed-store overflow is rejected before operand fetch or mutation. */
    initial.pc = UINT16_C(0x3000);
    initial.psw = 0U;
    initial.a = UINT8_C(0x5a);
    initial.x = UINT8_C(0x34);
    ram[0x3000] = UINT8_C(0xd4);
    ram[0x3001] = UINT8_C(0xf0);
    ram[0x0024] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    cpu.instruction_cycles = UINT64_MAX - UINT64_C(4);
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_CYCLE_OVERFLOW);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == UINT64_MAX - UINT64_C(4));
    CHECK(ram[0x0024] == UINT8_C(0xcc));

    /* Absolute-store overflow is rejected before operands or target mutation. */
    initial.pc = UINT16_C(0x3000);
    initial.psw = 0U;
    initial.a = UINT8_C(0x5a);
    ram[UINT16_C(0x3000)] = UINT8_C(0xc5);
    ram[UINT16_C(0x3001)] = UINT8_C(0x56);
    ram[UINT16_C(0x3002)] = UINT8_C(0x34);
    ram[UINT16_C(0x3456)] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    cpu.instruction_cycles = UINT64_MAX - UINT64_C(4);
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_CYCLE_OVERFLOW);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == UINT64_MAX - UINT64_C(4));
    CHECK(ram[UINT16_C(0x3456)] == UINT8_C(0xcc));

    /* Indexed-absolute-store overflow rejects operands and target mutation. */
    initial.pc = UINT16_C(0x3000);
    initial.psw = 0U;
    initial.a = UINT8_C(0x5a);
    initial.x = UINT8_C(0x20);
    ram[UINT16_C(0x3000)] = UINT8_C(0xd5);
    ram[UINT16_C(0x3001)] = UINT8_C(0xf0);
    ram[UINT16_C(0x3002)] = UINT8_C(0xff);
    ram[UINT16_C(0x0010)] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    cpu.instruction_cycles = UINT64_MAX - UINT64_C(5);
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_CYCLE_OVERFLOW);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == UINT64_MAX - UINT64_C(5));
    CHECK(ram[UINT16_C(0x0010)] == UINT8_C(0xcc));

    /* Overflow is rejected before an immediate operand or register commit. */
    initial.pc = UINT16_C(0x3000);
    ram[0x3000] = UINT8_C(0xe8);
    ram[0x3001] = UINT8_C(0x80);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    cpu.instruction_cycles = UINT64_MAX - UINT64_C(1);
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_CYCLE_OVERFLOW);
    CHECK(same_registers(&cpu.registers, &before));
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

    /* A valid opcode followed by an unavailable IPL operand is atomic for CPU
     * state and time, while retaining the fetched-opcode diagnostic. */
    initial.pc = UINT16_C(0xffbf);
    ram[0xffbf] = UINT8_C(0xe8);
    CHECK(mipsnes2_spc700_init(&cpu, &no_ipl_memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.has_fetched_opcode && cpu.last_opcode == UINT8_C(0xe8));
    CHECK(cpu.instruction_cycles == 0U);

    /* A missing absolute high byte retains fetched-opcode diagnostics but
     * commits neither the low byte nor programmer-visible state/time. */
    initial.pc = UINT16_C(0xffbe);
    ram[UINT16_C(0xffbe)] = UINT8_C(0xe5);
    ram[UINT16_C(0xffbf)] = UINT8_C(0x34);
    CHECK(mipsnes2_spc700_init(&cpu, &no_ipl_memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.has_fetched_opcode && cpu.last_opcode == UINT8_C(0xe5));
    CHECK(cpu.instruction_cycles == 0U);

    (void)puts("SPC700 register/immediate/direct/absolute MOV addressing: OK");
    return 0;
}
