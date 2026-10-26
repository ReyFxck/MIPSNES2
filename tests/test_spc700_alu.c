/* Synthetic SPC700 ALU-family fixture. See LICENSE for GPL-3.0 terms. */
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

static void set_flag(uint8_t *psw, uint8_t flag, int enabled)
{
    if (enabled) {
        *psw |= flag;
    } else {
        *psw &= (uint8_t)~flag;
    }
}

static void set_negative_zero(uint8_t *psw, uint8_t value)
{
    *psw &= (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
                       MIPSNES2_SPC700_FLAG_Z);
    if (value == 0U) {
        *psw |= MIPSNES2_SPC700_FLAG_Z;
    } else if ((value & UINT8_C(0x80)) != 0U) {
        *psw |= MIPSNES2_SPC700_FLAG_N;
    }
}

static uint8_t reference_alu(unsigned operation, uint8_t left, uint8_t right,
                             uint8_t *psw)
{
    uint8_t result;
    unsigned carry;
    unsigned sum;
    int difference;

    switch (operation) {
    case 0U:
        result = left | right;
        break;
    case 1U:
        result = left & right;
        break;
    case 2U:
        result = left ^ right;
        break;
    case 3U:
        result = (uint8_t)(left - right);
        set_flag(psw, MIPSNES2_SPC700_FLAG_C, left >= right);
        break;
    case 4U:
        carry = (*psw & MIPSNES2_SPC700_FLAG_C) != 0U ? 1U : 0U;
        sum = (unsigned)left + (unsigned)right + carry;
        result = (uint8_t)sum;
        set_flag(psw, MIPSNES2_SPC700_FLAG_C, sum > UINT8_MAX);
        set_flag(psw, MIPSNES2_SPC700_FLAG_H,
                 (unsigned)(left & UINT8_C(0x0f)) +
                 (unsigned)(right & UINT8_C(0x0f)) + carry > 0x0fU);
        set_flag(psw, MIPSNES2_SPC700_FLAG_V,
                 ((uint8_t)~(left ^ right) & (left ^ result) &
                  UINT8_C(0x80)) != 0U);
        break;
    default:
        carry = (*psw & MIPSNES2_SPC700_FLAG_C) != 0U ? 0U : 1U;
        difference = (int)left - (int)right - (int)carry;
        result = (uint8_t)difference;
        set_flag(psw, MIPSNES2_SPC700_FLAG_C, difference >= 0);
        set_flag(psw, MIPSNES2_SPC700_FLAG_H,
                 (int)(left & UINT8_C(0x0f)) -
                 (int)(right & UINT8_C(0x0f)) - (int)carry >= 0);
        set_flag(psw, MIPSNES2_SPC700_FLAG_V,
                 ((left ^ right) & (left ^ result) & UINT8_C(0x80)) != 0U);
        break;
    }
    set_negative_zero(psw, result);
    return result;
}

static int is_new_opcode(uint8_t opcode)
{
    uint8_t group = opcode & UINT8_C(0xe0);
    uint8_t mode = opcode & UINT8_C(0x1f);

    if (group <= UINT8_C(0xa0) &&
        (mode == UINT8_C(0x04) || mode == UINT8_C(0x05) ||
         mode == UINT8_C(0x06) || mode == UINT8_C(0x07) ||
         mode == UINT8_C(0x08) || mode == UINT8_C(0x09) ||
         mode == UINT8_C(0x14) || mode == UINT8_C(0x15) ||
         mode == UINT8_C(0x16) || mode == UINT8_C(0x17) ||
         mode == UINT8_C(0x18) || mode == UINT8_C(0x19))) {
        return 1;
    }
    if ((opcode & UINT8_C(0x80)) == 0U &&
        (mode == UINT8_C(0x0b) || mode == UINT8_C(0x0c) ||
         mode == UINT8_C(0x1b) || mode == UINT8_C(0x1c))) {
        return 1;
    }
    switch (opcode) {
    case UINT8_C(0x1d):
    case UINT8_C(0x3d):
    case UINT8_C(0x8b):
    case UINT8_C(0x8c):
    case UINT8_C(0x9b):
    case UINT8_C(0x9c):
    case UINT8_C(0xab):
    case UINT8_C(0xac):
    case UINT8_C(0xbb):
    case UINT8_C(0xbc):
    case UINT8_C(0xdc):
    case UINT8_C(0xfc):
        return 1;
    default:
        return 0;
    }
}

int main(void)
{
    static const uint8_t alu_a_modes[] = {
        UINT8_C(0x04), UINT8_C(0x05), UINT8_C(0x06),
        UINT8_C(0x07), UINT8_C(0x08), UINT8_C(0x14),
        UINT8_C(0x15), UINT8_C(0x16), UINT8_C(0x17)
    };
    static const uint8_t alu_a_cycles[] = {
        UINT8_C(3), UINT8_C(4), UINT8_C(3),
        UINT8_C(6), UINT8_C(2), UINT8_C(4),
        UINT8_C(5), UINT8_C(5), UINT8_C(6)
    };
    static const uint8_t alu_a_lengths[] = {
        UINT8_C(2), UINT8_C(3), UINT8_C(1),
        UINT8_C(2), UINT8_C(2), UINT8_C(2),
        UINT8_C(3), UINT8_C(3), UINT8_C(2)
    };
    static const uint8_t memory_modes[] = {
        UINT8_C(0x09), UINT8_C(0x18), UINT8_C(0x19)
    };
    static const uint8_t shift_modes[] = {
        UINT8_C(0x0b), UINT8_C(0x0c),
        UINT8_C(0x1b), UINT8_C(0x1c)
    };
    static const struct {
        uint8_t opcode;
        uint8_t cycles;
        uint8_t length;
        unsigned target;
        int decrement;
    } inc_dec[] = {
        {UINT8_C(0x1d), UINT8_C(2), UINT8_C(1), 1U, 1},
        {UINT8_C(0x3d), UINT8_C(2), UINT8_C(1), 1U, 0},
        {UINT8_C(0x8b), UINT8_C(4), UINT8_C(2), 3U, 1},
        {UINT8_C(0x8c), UINT8_C(5), UINT8_C(3), 4U, 1},
        {UINT8_C(0x9b), UINT8_C(5), UINT8_C(2), 5U, 1},
        {UINT8_C(0x9c), UINT8_C(2), UINT8_C(1), 0U, 1},
        {UINT8_C(0xab), UINT8_C(4), UINT8_C(2), 3U, 0},
        {UINT8_C(0xac), UINT8_C(5), UINT8_C(3), 4U, 0},
        {UINT8_C(0xbb), UINT8_C(5), UINT8_C(2), 5U, 0},
        {UINT8_C(0xbc), UINT8_C(2), UINT8_C(1), 0U, 0},
        {UINT8_C(0xdc), UINT8_C(2), UINT8_C(1), 2U, 1},
        {UINT8_C(0xfc), UINT8_C(2), UINT8_C(1), 2U, 0}
    };
    mipsnes2_apu_io_ports ports;
    mipsnes2_smp_memory memory;
    mipsnes2_spc700 cpu;
    mipsnes2_spc700_registers initial;
    mipsnes2_spc700_registers before;
    uint8_t expected_psw;
    uint8_t expected_result;
    uint8_t observed;
    unsigned operation;
    unsigned mode_index;
    unsigned i;
    unsigned left;
    unsigned right;
    unsigned carry;
    unsigned count = 0U;

    (void)memset(ram, UINT8_C(0xcc), sizeof(ram));
    (void)memset(synthetic_ipl, UINT8_C(0x01), sizeof(synthetic_ipl));
    mipsnes2_apu_io_ports_init(&ports);
    CHECK(mipsnes2_smp_memory_init(&memory, ram, sizeof(ram), &ports,
                                   synthetic_ipl, sizeof(synthetic_ipl)));

    for (i = 0U; i <= UINT8_MAX; ++i) {
        count += is_new_opcode((uint8_t)i) ? 1U : 0U;
    }
    CHECK(count == 100U);

    /* All 54 accumulator ALU forms share operation/flag semantics while their
     * nine address modes retain exact lengths and cycle totals. */
    for (operation = 0U; operation < 6U; ++operation) {
        for (mode_index = 0U;
             mode_index < sizeof(alu_a_modes) / sizeof(alu_a_modes[0]);
             ++mode_index) {
            uint8_t opcode = (uint8_t)(operation * 0x20U +
                                       alu_a_modes[mode_index]);

            initial.pc = UINT16_C(0x3000);
            initial.a = UINT8_C(0x55);
            initial.x = UINT8_C(0x10);
            initial.y = UINT8_C(0x10);
            initial.sp = UINT8_C(0xef);
            initial.psw = (uint8_t)(MIPSNES2_SPC700_FLAG_P |
                                    MIPSNES2_SPC700_FLAG_B |
                                    MIPSNES2_SPC700_FLAG_I |
                                    MIPSNES2_SPC700_FLAG_C);
            ram[initial.pc] = opcode;
            ram[initial.pc + UINT16_C(1)] = UINT8_C(0x30);
            ram[initial.pc + UINT16_C(2)] = UINT8_C(0x20);
            ram[UINT16_C(0x0130)] = UINT8_C(0x3c);
            ram[UINT16_C(0x0140)] = UINT8_C(0x3c);
            ram[UINT16_C(0x2000)] = UINT8_C(0x3c);
            ram[UINT16_C(0x2030)] = UINT8_C(0x00);
            ram[UINT16_C(0x2031)] = UINT8_C(0x20);

            if (alu_a_modes[mode_index] == UINT8_C(0x05)) {
                ram[initial.pc + UINT16_C(1)] = UINT8_C(0x00);
            } else if (alu_a_modes[mode_index] == UINT8_C(0x06)) {
                initial.x = UINT8_C(0x40);
            } else if (alu_a_modes[mode_index] == UINT8_C(0x07)) {
                ram[UINT16_C(0x0140)] = UINT8_C(0x00);
                ram[UINT16_C(0x0141)] = UINT8_C(0x20);
            } else if (alu_a_modes[mode_index] == UINT8_C(0x08)) {
                ram[initial.pc + UINT16_C(1)] = UINT8_C(0x3c);
            } else if (alu_a_modes[mode_index] == UINT8_C(0x15) ||
                       alu_a_modes[mode_index] == UINT8_C(0x16)) {
                ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
                ram[initial.pc + UINT16_C(2)] = UINT8_C(0x1f);
            } else if (alu_a_modes[mode_index] == UINT8_C(0x17)) {
                ram[UINT16_C(0x0130)] = UINT8_C(0xf0);
                ram[UINT16_C(0x0131)] = UINT8_C(0x1f);
            }

            expected_psw = initial.psw;
            expected_result = reference_alu(operation, UINT8_C(0x55),
                                            UINT8_C(0x3c), &expected_psw);
            CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
            CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
            CHECK(cpu.registers.a ==
                  (operation == 3U ? initial.a : expected_result));
            CHECK(cpu.registers.x == initial.x &&
                  cpu.registers.y == initial.y &&
                  cpu.registers.sp == initial.sp &&
                  cpu.registers.psw == expected_psw);
            CHECK(cpu.registers.pc ==
                  (uint16_t)(initial.pc + alu_a_lengths[mode_index]));
            CHECK(cpu.instruction_cycles == alu_a_cycles[mode_index]);
        }
    }

    /* Every 8-bit ADC/SBC input and carry state is checked independently. */
    for (operation = 4U; operation < 6U; ++operation) {
        uint8_t opcode = operation == 4U ? UINT8_C(0x88) : UINT8_C(0xa8);

        initial.pc = UINT16_C(0x3100);
        initial.x = UINT8_C(0x12);
        initial.y = UINT8_C(0x34);
        initial.sp = UINT8_C(0x56);
        for (left = 0U; left <= UINT8_MAX; ++left) {
            for (right = 0U; right <= UINT8_MAX; ++right) {
                for (carry = 0U; carry < 2U; ++carry) {
                    initial.a = (uint8_t)left;
                    initial.psw = carry != 0U
                        ? MIPSNES2_SPC700_FLAG_C : UINT8_C(0x00);
                    ram[initial.pc] = opcode;
                    ram[initial.pc + UINT16_C(1)] = (uint8_t)right;
                    expected_psw = initial.psw;
                    expected_result = reference_alu(
                        operation, (uint8_t)left, (uint8_t)right,
                        &expected_psw);
                    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                    CHECK(mipsnes2_spc700_step(&cpu) ==
                          MIPSNES2_SPC700_OK);
                    CHECK(cpu.registers.a == expected_result &&
                          cpu.registers.psw == expected_psw &&
                          cpu.registers.pc == UINT16_C(0x3102) &&
                          cpu.instruction_cycles == UINT64_C(2));
                }
            }
        }
    }

    /* All 18 direct memory ALU forms use source-before-destination encoding,
     * preserve A/X/Y/SP and write no result for CMP. */
    for (operation = 0U; operation < 6U; ++operation) {
        for (mode_index = 0U;
             mode_index < sizeof(memory_modes) / sizeof(memory_modes[0]);
             ++mode_index) {
            uint8_t opcode = (uint8_t)(operation * 0x20U +
                                       memory_modes[mode_index]);

            initial.pc = UINT16_C(0x3200);
            initial.a = UINT8_C(0xa5);
            initial.x = UINT8_C(0x41);
            initial.y = UINT8_C(0x40);
            initial.sp = UINT8_C(0xef);
            initial.psw = (uint8_t)(MIPSNES2_SPC700_FLAG_P |
                                    MIPSNES2_SPC700_FLAG_B |
                                    MIPSNES2_SPC700_FLAG_C);
            ram[initial.pc] = opcode;
            ram[initial.pc + UINT16_C(1)] =
                memory_modes[mode_index] == UINT8_C(0x18)
                    ? UINT8_C(0x3c) : UINT8_C(0x40);
            ram[initial.pc + UINT16_C(2)] = UINT8_C(0x41);
            ram[UINT16_C(0x0140)] = UINT8_C(0x3c);
            ram[UINT16_C(0x0141)] = UINT8_C(0x55);
            expected_psw = initial.psw;
            expected_result = reference_alu(operation, UINT8_C(0x55),
                                            UINT8_C(0x3c), &expected_psw);
            CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
            CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
            CHECK(ram[UINT16_C(0x0141)] ==
                  (operation == 3U ? UINT8_C(0x55) : expected_result));
            CHECK(cpu.registers.a == initial.a &&
                  cpu.registers.x == initial.x &&
                  cpu.registers.y == initial.y &&
                  cpu.registers.sp == initial.sp &&
                  cpu.registers.psw == expected_psw);
            CHECK(cpu.registers.pc ==
                  (uint16_t)(initial.pc +
                      (memory_modes[mode_index] == UINT8_C(0x19) ? 1U : 3U)));
            CHECK(cpu.instruction_cycles ==
                  (memory_modes[mode_index] == UINT8_C(0x09)
                      ? UINT64_C(6) : UINT64_C(5)));
        }
    }

    /* ASL/ROL/LSR/ROR cover accumulator, direct, absolute and direct+X. */
    for (operation = 0U; operation < 4U; ++operation) {
        for (mode_index = 0U;
             mode_index < sizeof(shift_modes) / sizeof(shift_modes[0]);
             ++mode_index) {
            uint8_t opcode = (uint8_t)(operation * 0x20U +
                                       shift_modes[mode_index]);
            uint8_t value = UINT8_C(0x81);
            uint8_t result;
            int old_carry;

            initial.pc = UINT16_C(0x3300);
            initial.a = value;
            initial.x = UINT8_C(0x10);
            initial.y = UINT8_C(0x34);
            initial.sp = UINT8_C(0xef);
            initial.psw = (uint8_t)(MIPSNES2_SPC700_FLAG_P |
                                    MIPSNES2_SPC700_FLAG_B |
                                    MIPSNES2_SPC700_FLAG_I |
                                    MIPSNES2_SPC700_FLAG_H |
                                    MIPSNES2_SPC700_FLAG_V |
                                    MIPSNES2_SPC700_FLAG_C);
            ram[initial.pc] = opcode;
            ram[initial.pc + UINT16_C(1)] = UINT8_C(0x30);
            ram[initial.pc + UINT16_C(2)] = UINT8_C(0x20);
            ram[UINT16_C(0x0130)] = value;
            ram[UINT16_C(0x0140)] = value;
            ram[UINT16_C(0x2030)] = value;
            expected_psw = initial.psw;
            old_carry = (expected_psw & MIPSNES2_SPC700_FLAG_C) != 0U;
            if (operation == 0U) {
                set_flag(&expected_psw, MIPSNES2_SPC700_FLAG_C,
                         (value & UINT8_C(0x80)) != 0U);
                result = (uint8_t)(value << 1U);
            } else if (operation == 1U) {
                set_flag(&expected_psw, MIPSNES2_SPC700_FLAG_C,
                         (value & UINT8_C(0x80)) != 0U);
                result = (uint8_t)((uint8_t)(value << 1U) |
                                   (old_carry ? UINT8_C(1) : UINT8_C(0)));
            } else if (operation == 2U) {
                set_flag(&expected_psw, MIPSNES2_SPC700_FLAG_C,
                         (value & UINT8_C(1)) != 0U);
                result = value >> 1U;
            } else {
                set_flag(&expected_psw, MIPSNES2_SPC700_FLAG_C,
                         (value & UINT8_C(1)) != 0U);
                result = (uint8_t)((value >> 1U) |
                                   (old_carry ? UINT8_C(0x80) : UINT8_C(0)));
            }
            set_negative_zero(&expected_psw, result);
            CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
            CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
            if (shift_modes[mode_index] == UINT8_C(0x1c)) {
                CHECK(cpu.registers.a == result);
            } else if (shift_modes[mode_index] == UINT8_C(0x0b)) {
                CHECK(ram[UINT16_C(0x0130)] == result);
            } else if (shift_modes[mode_index] == UINT8_C(0x0c)) {
                CHECK(ram[UINT16_C(0x2030)] == result);
            } else {
                CHECK(ram[UINT16_C(0x0140)] == result);
            }
            CHECK(cpu.registers.psw == expected_psw);
            if (shift_modes[mode_index] == UINT8_C(0x0b)) {
                CHECK(cpu.registers.pc == UINT16_C(0x3302) &&
                      cpu.instruction_cycles == UINT64_C(4));
            } else if (shift_modes[mode_index] == UINT8_C(0x0c)) {
                CHECK(cpu.registers.pc == UINT16_C(0x3303) &&
                      cpu.instruction_cycles == UINT64_C(5));
            } else if (shift_modes[mode_index] == UINT8_C(0x1b)) {
                CHECK(cpu.registers.pc == UINT16_C(0x3302) &&
                      cpu.instruction_cycles == UINT64_C(5));
            } else {
                CHECK(cpu.registers.pc == UINT16_C(0x3301) &&
                      cpu.instruction_cycles == UINT64_C(2));
            }
        }
    }

    /* Twelve INC/DEC forms cover A/X/Y plus direct, absolute and direct+X. */
    for (i = 0U; i < sizeof(inc_dec) / sizeof(inc_dec[0]); ++i) {
        uint8_t value = inc_dec[i].decrement ? UINT8_C(0x00) : UINT8_C(0xff);
        uint8_t result = inc_dec[i].decrement
            ? UINT8_C(0xff) : UINT8_C(0x00);

        initial.pc = UINT16_C(0x3400);
        initial.a = value;
        initial.x = inc_dec[i].target == 5U ? UINT8_C(0x10) : value;
        initial.y = value;
        initial.sp = UINT8_C(0xef);
        initial.psw = (uint8_t)(MIPSNES2_SPC700_FLAG_P |
                                MIPSNES2_SPC700_FLAG_B |
                                MIPSNES2_SPC700_FLAG_I |
                                MIPSNES2_SPC700_FLAG_H |
                                MIPSNES2_SPC700_FLAG_V |
                                MIPSNES2_SPC700_FLAG_C);
        ram[initial.pc] = inc_dec[i].opcode;
        ram[initial.pc + UINT16_C(1)] = UINT8_C(0x30);
        ram[initial.pc + UINT16_C(2)] = UINT8_C(0x20);
        ram[UINT16_C(0x0130)] = value;
        ram[UINT16_C(0x0140)] = value;
        ram[UINT16_C(0x2030)] = value;
        expected_psw = initial.psw;
        set_negative_zero(&expected_psw, result);
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.registers.pc ==
              (uint16_t)(initial.pc + inc_dec[i].length));
        CHECK(cpu.instruction_cycles == inc_dec[i].cycles &&
              cpu.registers.psw == expected_psw);
        if (inc_dec[i].target == 0U) {
            CHECK(cpu.registers.a == result);
        } else if (inc_dec[i].target == 1U) {
            CHECK(cpu.registers.x == result);
        } else if (inc_dec[i].target == 2U) {
            CHECK(cpu.registers.y == result);
        } else if (inc_dec[i].target == 3U) {
            CHECK(ram[UINT16_C(0x0130)] == result);
        } else if (inc_dec[i].target == 4U) {
            CHECK(ram[UINT16_C(0x2030)] == result);
        } else {
            CHECK(ram[UINT16_C(0x0140)] == result);
        }
    }

    /* New operations traverse MMIO and retain P-selected page-one ARAM. */
    initial.pc = UINT16_C(0x3500);
    initial.a = UINT8_C(0x0f);
    initial.x = UINT8_C(0x10);
    initial.y = UINT8_C(0x20);
    initial.sp = UINT8_C(0xef);
    initial.psw = UINT8_C(0x00);
    CHECK(mipsnes2_apu_io_ports_cpu_write(&ports, UINT16_C(0x2140),
                                          UINT8_C(0x80)));
    ram[initial.pc] = UINT8_C(0x04);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x8f));

    ram[initial.pc] = UINT8_C(0x18);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0x0f);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0xf4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0x8f));

    initial.psw = MIPSNES2_SPC700_FLAG_P;
    ram[UINT16_C(0x01f4)] = UINT8_C(0x40);
    ram[initial.pc] = UINT8_C(0x04);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf4);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x4f));
    CHECK(mipsnes2_apu_io_ports_cpu_read(&ports, UINT16_C(0x2140),
                                         &observed));
    CHECK(observed == UINT8_C(0x8f));

    /* Unavailable MMIO and cycle overflow reject state/time and writes. */
    initial.pc = UINT16_C(0x3600);
    initial.a = UINT8_C(0x55);
    initial.x = UINT8_C(0x10);
    initial.y = UINT8_C(0x20);
    initial.psw = UINT8_C(0x00);
    ram[initial.pc] = UINT8_C(0x04);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0xf0);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U);

    ram[initial.pc] = UINT8_C(0x18);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0x0f);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0xf0);
    ram[UINT16_C(0x00f0)] = UINT8_C(0xcc);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(cpu.instruction_cycles == 0U &&
          ram[UINT16_C(0x00f0)] == UINT8_C(0xcc));

    ram[initial.pc] = UINT8_C(0x0c);
    ram[initial.pc + UINT16_C(1)] = UINT8_C(0x00);
    ram[initial.pc + UINT16_C(2)] = UINT8_C(0x20);
    ram[UINT16_C(0x2000)] = UINT8_C(0x81);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    cpu.instruction_cycles = UINT64_MAX - UINT64_C(4);
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_CYCLE_OVERFLOW);
    CHECK(same_registers(&cpu.registers, &before));
    CHECK(ram[UINT16_C(0x2000)] == UINT8_C(0x81));

    /* Eight immediate logical operations deliver sixteen timer cycles. */
    CHECK(mipsnes2_smp_memory_reset(&memory));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00fc),
                                    UINT8_C(1)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x84)));
    initial.pc = UINT16_C(0x3700);
    initial.a = UINT8_C(0x00);
    initial.psw = UINT8_C(0x00);
    for (i = 0U; i < 8U; ++i) {
        ram[initial.pc + i * 2U] = UINT8_C(0x08);
        ram[initial.pc + i * 2U + 1U] = (uint8_t)i;
    }
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    for (i = 0U; i < 8U; ++i) {
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    }
    CHECK(cpu.instruction_cycles == UINT64_C(16));
    CHECK(memory.timers.output[2] == UINT8_C(1));

    (void)puts("SPC700 100-opcode ALU/shift/inc-dec block: OK");
    return 0;
}
