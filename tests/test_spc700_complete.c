/* Complete SPC700 opcode-dispatch fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/apu/spc700/cpu.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t ram[MIPSNES2_SMP_RAM_SIZE];
static mipsnes2_apu_io_ports ports;
static mipsnes2_smp_memory memory;

static const uint8_t expected_base_cycles[256] = {
    2,8,4,5,3,4,3,6,2,6,5,4,5,4,6,8,
    2,8,4,5,4,5,5,6,5,5,6,5,2,2,4,6,
    2,8,4,5,3,4,3,6,2,6,5,4,5,4,5,4,
    2,8,4,5,4,5,5,6,5,5,6,5,2,2,3,8,
    2,8,4,5,3,4,3,6,2,6,4,4,5,4,6,6,
    2,8,4,5,4,5,5,6,5,5,4,5,2,2,4,3,
    2,8,4,5,3,4,3,6,2,6,4,4,5,4,5,5,
    2,8,4,5,4,5,5,6,5,5,5,5,2,2,3,6,
    2,8,4,5,3,4,3,6,2,6,5,4,5,2,4,5,
    2,8,4,5,4,5,5,6,5,5,5,5,2,2,12,5,
    3,8,4,5,3,4,3,6,2,6,4,4,5,2,4,4,
    2,8,4,5,4,5,5,6,5,5,5,5,2,2,3,4,
    3,8,4,5,4,5,4,7,2,5,6,4,5,2,4,9,
    2,8,4,5,5,6,6,7,4,5,5,5,2,2,6,3,
    2,8,4,5,3,4,3,6,2,4,5,3,4,3,4,3,
    2,8,4,5,4,5,5,6,3,4,5,4,2,2,4,3
};

static int setup_memory(void)
{
    (void)memset(ram, 0, sizeof(ram));
    mipsnes2_apu_io_ports_init(&ports);
    if (!mipsnes2_smp_memory_init(&memory, ram, sizeof(ram), &ports,
                                   NULL, 0U)) {
        return 0;
    }
    memory.ipl_enabled = false;
    return 1;
}

static int old_opcode(uint8_t opcode)
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
    case UINT8_C(0x00): case UINT8_C(0x20): case UINT8_C(0x40):
    case UINT8_C(0x5d): case UINT8_C(0x60): case UINT8_C(0x7d):
    case UINT8_C(0x80): case UINT8_C(0x8b): case UINT8_C(0x8c):
    case UINT8_C(0x8d): case UINT8_C(0x8f): case UINT8_C(0x9b):
    case UINT8_C(0x9c): case UINT8_C(0x9d): case UINT8_C(0xa0):
    case UINT8_C(0xab): case UINT8_C(0xac): case UINT8_C(0xaf):
    case UINT8_C(0xbb): case UINT8_C(0xbc): case UINT8_C(0xbd):
    case UINT8_C(0xbf): case UINT8_C(0xc0): case UINT8_C(0xc4):
    case UINT8_C(0xc5): case UINT8_C(0xc6): case UINT8_C(0xc7):
    case UINT8_C(0xc9): case UINT8_C(0xcb): case UINT8_C(0xcc):
    case UINT8_C(0xcd): case UINT8_C(0xd4): case UINT8_C(0xd5):
    case UINT8_C(0xd6): case UINT8_C(0xd7): case UINT8_C(0xd8):
    case UINT8_C(0xd9): case UINT8_C(0xdb): case UINT8_C(0xdc):
    case UINT8_C(0xdd): case UINT8_C(0xe0): case UINT8_C(0xe4):
    case UINT8_C(0xe5): case UINT8_C(0xe6): case UINT8_C(0xe7):
    case UINT8_C(0xe8): case UINT8_C(0xe9): case UINT8_C(0xeb):
    case UINT8_C(0xec): case UINT8_C(0xed): case UINT8_C(0xf4):
    case UINT8_C(0xf5): case UINT8_C(0xf6): case UINT8_C(0xf7):
    case UINT8_C(0xf8): case UINT8_C(0xf9): case UINT8_C(0xfa):
    case UINT8_C(0xfb): case UINT8_C(0xfc): case UINT8_C(0xfd):
    case UINT8_C(0x1d): case UINT8_C(0x3d):
        return 1;
    default:
        return 0;
    }
}

static void make_branch_not_taken(uint8_t opcode,
                                  mipsnes2_spc700_registers *registers)
{
    uint8_t flag;

    if ((opcode & UINT8_C(0x1f)) == UINT8_C(0x10)) {
        switch (opcode & UINT8_C(0xc0)) {
        case UINT8_C(0x00): flag = MIPSNES2_SPC700_FLAG_N; break;
        case UINT8_C(0x40): flag = MIPSNES2_SPC700_FLAG_V; break;
        case UINT8_C(0x80): flag = MIPSNES2_SPC700_FLAG_C; break;
        default: flag = MIPSNES2_SPC700_FLAG_Z; break;
        }
        if ((opcode & UINT8_C(0x20)) == 0U) {
            registers->psw |= flag;
        } else {
            registers->psw &= (uint8_t)~flag;
        }
    }
}

static void make_branch_taken(uint8_t opcode,
                              mipsnes2_spc700_registers *registers)
{
    uint8_t flag;

    make_branch_not_taken(opcode, registers);
    switch (opcode & UINT8_C(0xc0)) {
    case UINT8_C(0x00): flag = MIPSNES2_SPC700_FLAG_N; break;
    case UINT8_C(0x40): flag = MIPSNES2_SPC700_FLAG_V; break;
    case UINT8_C(0x80): flag = MIPSNES2_SPC700_FLAG_C; break;
    default: flag = MIPSNES2_SPC700_FLAG_Z; break;
    }
    registers->psw ^= flag;
}

static int test_complete_dispatch(void)
{
    mipsnes2_spc700 cpu;
    mipsnes2_spc700_registers initial;
    unsigned opcode;
    unsigned new_count = 0U;
    uint8_t bit;

    for (opcode = 0U; opcode <= UINT8_MAX; ++opcode) {
        CHECK(setup_memory());
        initial = (mipsnes2_spc700_registers){0};
        initial.pc = UINT16_C(0x2000);
        initial.sp = UINT8_C(0xef);
        initial.y = UINT8_C(1);
        make_branch_not_taken((uint8_t)opcode, &initial);
        ram[initial.pc] = (uint8_t)opcode;
        ram[(uint16_t)(initial.pc + 1U)] = 0U;
        ram[(uint16_t)(initial.pc + 2U)] = 0U;
        if (((uint8_t)opcode & UINT8_C(0x0f)) == UINT8_C(0x03)) {
            bit = (uint8_t)opcode >> 5U;
            ram[0] = ((uint8_t)opcode & UINT8_C(0x10)) != 0U
                ? (uint8_t)(UINT8_C(1) << bit) : 0U;
        }
        if ((uint8_t)opcode == UINT8_C(0x6e)) {
            ram[0] = UINT8_C(1);
        }
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.instruction_cycles == expected_base_cycles[opcode]);
        if (!old_opcode((uint8_t)opcode)) {
            ++new_count;
        }
    }
    CHECK(new_count == 106U);
    return 0;
}

static int test_bit_and_branch_families(void)
{
    static const uint8_t boolean_opcodes[] = {
        0x0aU, 0x2aU, 0x4aU, 0x6aU, 0x8aU, 0xaaU
    };
    mipsnes2_spc700 cpu;
    mipsnes2_spc700_registers initial;
    unsigned bit;
    unsigned carry;
    unsigned bit_value;
    unsigned operation;
    uint8_t opcode;
    uint8_t mask;
    bool expected;

    for (bit = 0U; bit < 8U; ++bit) {
        mask = (uint8_t)(UINT8_C(1) << bit);
        opcode = (uint8_t)(bit * 32U + 2U);
        CHECK(setup_memory());
        initial = (mipsnes2_spc700_registers){0};
        initial.pc = UINT16_C(0x4000);
        initial.psw = MIPSNES2_SPC700_FLAG_P;
        ram[0x4000] = opcode;
        ram[0x4001] = UINT8_C(0x80);
        ram[0x0180] = UINT8_C(0x55);
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(ram[0x0180] == (uint8_t)(UINT8_C(0x55) | mask));
        CHECK(cpu.instruction_cycles == 4U);

        ram[0x4000] = (uint8_t)(opcode | UINT8_C(0x10));
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(ram[0x0180] == (uint8_t)(UINT8_C(0x55) & (uint8_t)~mask));

        CHECK(setup_memory());
        initial.pc = UINT16_C(0x4000);
        initial.psw = 0U;
        ram[0x4000] = (uint8_t)(bit * 32U + 3U);
        ram[0x4001] = UINT8_C(0x20);
        ram[0x4002] = UINT8_C(0xfe);
        ram[0x0020] = mask;
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.registers.pc == UINT16_C(0x4001));
        CHECK(cpu.instruction_cycles == 7U);

        ram[0x4000] = (uint8_t)(bit * 32U + UINT8_C(0x13));
        ram[0x0020] = 0U;
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.registers.pc == UINT16_C(0x4001));
        CHECK(cpu.instruction_cycles == 7U);
    }

    /* Boolean bit addressing packs the bit number above a 13-bit address. */
    CHECK(setup_memory());
    initial = (mipsnes2_spc700_registers){0};
    initial.pc = UINT16_C(0x4000);
    ram[0x4001] = UINT8_C(0x34);
    ram[0x4002] = UINT8_C(0xb2); /* bit 5, address 1234 */
    for (operation = 0U;
         operation < sizeof(boolean_opcodes) / sizeof(boolean_opcodes[0]);
         ++operation) {
        for (carry = 0U; carry < 2U; ++carry) {
            for (bit_value = 0U; bit_value < 2U; ++bit_value) {
                initial.psw = carry != 0U ? MIPSNES2_SPC700_FLAG_C : 0U;
                ram[0x1234] = bit_value != 0U ? UINT8_C(0x20) : 0U;
                ram[0x4000] = boolean_opcodes[operation];
                switch (boolean_opcodes[operation]) {
                case UINT8_C(0x0a): expected = carry != 0U || bit_value != 0U; break;
                case UINT8_C(0x2a): expected = carry != 0U || bit_value == 0U; break;
                case UINT8_C(0x4a): expected = carry != 0U && bit_value != 0U; break;
                case UINT8_C(0x6a): expected = carry != 0U && bit_value == 0U; break;
                case UINT8_C(0x8a): expected = (carry != 0U) != (bit_value != 0U); break;
                default: expected = bit_value != 0U; break;
                }
                CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
                CHECK(((cpu.registers.psw & MIPSNES2_SPC700_FLAG_C) != 0U) ==
                      expected);
            }
        }
    }

    initial.psw = MIPSNES2_SPC700_FLAG_C;
    ram[0x1234] = UINT8_C(0x20);
    ram[0x4000] = UINT8_C(0x8a); /* EOR1 */
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK((cpu.registers.psw & MIPSNES2_SPC700_FLAG_C) == 0U);
    ram[0x4000] = UINT8_C(0xea); /* NOT1 */
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x1234] == 0U);
    ram[0x4000] = UINT8_C(0xca); /* MOV1 mem.bit,C */
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x1234] == UINT8_C(0x20));
    return 0;
}

static int test_flow_and_stack(void)
{
    mipsnes2_spc700 cpu;
    mipsnes2_spc700_registers initial;
    unsigned vector;
    uint16_t vector_address;

    for (vector = 0U; vector < 16U; ++vector) {
        CHECK(setup_memory());
        initial = (mipsnes2_spc700_registers){0};
        initial.pc = UINT16_C(0x2200);
        initial.sp = UINT8_C(0xff);
        ram[0x2200] = (uint8_t)(vector * 16U + 1U);
        vector_address = (uint16_t)(UINT16_C(0xffde) - vector * 2U);
        ram[vector_address] = (uint8_t)(UINT8_C(0x40) + vector);
        ram[(uint16_t)(vector_address + 1U)] = UINT8_C(0x80);
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.registers.pc ==
              (uint16_t)(UINT16_C(0x8040) + vector));
        CHECK(cpu.registers.sp == UINT8_C(0xfd));
        CHECK(ram[0x01ff] == UINT8_C(0x22) &&
              ram[0x01fe] == UINT8_C(0x01));
    }

    CHECK(setup_memory());
    initial = (mipsnes2_spc700_registers){0};
    initial.pc = UINT16_C(0x3000);
    initial.sp = UINT8_C(0xff);
    ram[0x3000] = UINT8_C(0x3f);
    ram[0x3001] = UINT8_C(0x56);
    ram[0x3002] = UINT8_C(0x34);
    ram[0x3456] = UINT8_C(0x6f);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x3456));
    CHECK(ram[0x01ff] == UINT8_C(0x30) &&
          ram[0x01fe] == UINT8_C(0x03));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x3003) &&
          cpu.registers.sp == UINT8_C(0xff));

    CHECK(setup_memory());
    initial.pc = UINT16_C(0x3100);
    initial.sp = UINT8_C(0xff);
    initial.psw = UINT8_C(0xa5);
    ram[0x3100] = UINT8_C(0x0f);
    ram[0xffde] = UINT8_C(0x78);
    ram[0xffdf] = UINT8_C(0x56);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x5678));
    CHECK(cpu.registers.sp == UINT8_C(0xfc));
    CHECK(ram[0x01ff] == UINT8_C(0x31) &&
          ram[0x01fe] == UINT8_C(0x01) &&
          ram[0x01fd] == UINT8_C(0xa5));
    CHECK((cpu.registers.psw & MIPSNES2_SPC700_FLAG_B) != 0U);
    CHECK((cpu.registers.psw & MIPSNES2_SPC700_FLAG_I) == 0U);

    ram[0x5678] = UINT8_C(0x7f);
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x3101));
    CHECK(cpu.registers.psw == UINT8_C(0xa5));
    CHECK(cpu.registers.sp == UINT8_C(0xff));

    /* Stack wraps within page 1 and POP does not alter N/Z. */
    CHECK(setup_memory());
    initial.pc = UINT16_C(0x3200);
    initial.sp = 0U;
    initial.a = UINT8_C(0x80);
    initial.psw = UINT8_C(0x5a);
    ram[0x3200] = UINT8_C(0x2d);
    ram[0x3201] = UINT8_C(0xae);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.sp == UINT8_C(0xff) && ram[0x0100] == UINT8_C(0x80));
    cpu.registers.a = 0U;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x80) && cpu.registers.sp == 0U &&
          cpu.registers.psw == UINT8_C(0x5a));

    /* The X, Y and PSW stack forms preserve byte order and pop verbatim. */
    CHECK(setup_memory());
    initial = (mipsnes2_spc700_registers){0};
    initial.pc = UINT16_C(0x3300);
    initial.sp = UINT8_C(0xff);
    initial.x = UINT8_C(0x12);
    initial.y = UINT8_C(0x34);
    initial.psw = UINT8_C(0xa5);
    ram[0x3300] = UINT8_C(0x0d); /* PUSH PSW */
    ram[0x3301] = UINT8_C(0x4d); /* PUSH X */
    ram[0x3302] = UINT8_C(0x6d); /* PUSH Y */
    ram[0x3303] = UINT8_C(0xee); /* POP Y */
    ram[0x3304] = UINT8_C(0xce); /* POP X */
    ram[0x3305] = UINT8_C(0x8e); /* POP PSW */
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x01ff] == UINT8_C(0xa5) &&
          ram[0x01fe] == UINT8_C(0x12) &&
          ram[0x01fd] == UINT8_C(0x34));
    cpu.registers.x = 0U;
    cpu.registers.y = 0U;
    cpu.registers.psw = 0U;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.x == UINT8_C(0x12) &&
          cpu.registers.y == UINT8_C(0x34) &&
          cpu.registers.psw == UINT8_C(0xa5) &&
          cpu.registers.sp == UINT8_C(0xff));
    return 0;
}

static int test_control_compare_and_test_bits(void)
{
    static const uint8_t branches[] = {
        0x10U, 0x30U, 0x50U, 0x70U, 0x90U, 0xb0U, 0xd0U, 0xf0U
    };
    static const struct {
        uint8_t opcode;
        bool use_x;
        uint8_t kind; /* 0 immediate, 1 direct, 2 absolute */
    } compares[] = {
        {0xc8U, true, 0U}, {0x3eU, true, 1U}, {0x1eU, true, 2U},
        {0xadU, false, 0U}, {0x7eU, false, 1U}, {0x5eU, false, 2U}
    };
    mipsnes2_spc700 cpu;
    mipsnes2_spc700_registers initial;
    unsigned i;

    for (i = 0U; i < sizeof(branches); ++i) {
        CHECK(setup_memory());
        initial = (mipsnes2_spc700_registers){0};
        initial.pc = UINT16_C(0x7000);
        make_branch_taken(branches[i], &initial);
        ram[0x7000] = branches[i];
        ram[0x7001] = UINT8_C(0xfc);
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.registers.pc == UINT16_C(0x6ffe));
        CHECK(cpu.instruction_cycles == 4U);
        make_branch_not_taken(branches[i], &initial);
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.registers.pc == UINT16_C(0x7002));
        CHECK(cpu.instruction_cycles == 2U);
    }

    CHECK(setup_memory());
    initial = (mipsnes2_spc700_registers){0};
    initial.pc = UINT16_C(0xfffe);
    ram[0xfffe] = UINT8_C(0x2f);
    ram[0xffff] = UINT8_C(2);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(2));

    /* CBNE, DBNZ and indexed CBNE use the second operand as displacement. */
    CHECK(setup_memory());
    initial.pc = UINT16_C(0x7100);
    initial.a = UINT8_C(0x10);
    initial.x = UINT8_C(3);
    initial.y = UINT8_C(2);
    initial.psw = UINT8_C(0x85);
    ram[0x7100] = UINT8_C(0x2e);
    ram[0x7101] = UINT8_C(0x20);
    ram[0x7102] = UINT8_C(4);
    ram[0x0020] = UINT8_C(0x11);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x7107) &&
          cpu.registers.psw == initial.psw && cpu.instruction_cycles == 7U);
    ram[0x7100] = UINT8_C(0xde);
    ram[0x0023] = UINT8_C(0x10);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x7103) &&
          cpu.instruction_cycles == 6U);
    ram[0x7100] = UINT8_C(0x6e);
    ram[0x0020] = UINT8_C(2);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x0020] == UINT8_C(1) &&
          cpu.registers.pc == UINT16_C(0x7107) &&
          cpu.registers.psw == initial.psw);
    ram[0x7100] = UINT8_C(0xfe);
    ram[0x7101] = UINT8_C(0xfe);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.y == UINT8_C(1) &&
          cpu.registers.pc == UINT16_C(0x7100) &&
          cpu.instruction_cycles == 6U);

    /* Both absolute jump forms, including 16-bit pointer wrap. */
    CHECK(setup_memory());
    initial = (mipsnes2_spc700_registers){0};
    initial.pc = UINT16_C(0x7200);
    ram[0x7200] = UINT8_C(0x5f);
    ram[0x7201] = UINT8_C(0x34);
    ram[0x7202] = UINT8_C(0x12);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x1234));
    ram[0x7200] = UINT8_C(0x1f);
    ram[0x7201] = UINT8_C(0xff);
    ram[0x7202] = UINT8_C(0xff);
    ram[0xffff] = UINT8_C(0x78);
    ram[0x0000] = UINT8_C(0x56);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0x5678));

    /* PCALL pushes the post-operand PC high byte first. */
    CHECK(setup_memory());
    initial.pc = UINT16_C(0x7300);
    initial.sp = UINT8_C(0xff);
    ram[0x7300] = UINT8_C(0x4f);
    ram[0x7301] = UINT8_C(0x80);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.pc == UINT16_C(0xff80) &&
          ram[0x01ff] == UINT8_C(0x73) && ram[0x01fe] == UINT8_C(0x02));

    for (i = 0U; i < sizeof(compares) / sizeof(compares[0]); ++i) {
        CHECK(setup_memory());
        initial = (mipsnes2_spc700_registers){0};
        initial.pc = UINT16_C(0x7400);
        initial.x = UINT8_C(0x80);
        initial.y = UINT8_C(0x80);
        initial.psw = UINT8_C(0x4c);
        ram[0x7400] = compares[i].opcode;
        if (compares[i].kind == 0U) {
            ram[0x7401] = UINT8_C(0x81);
        } else if (compares[i].kind == 1U) {
            ram[0x7401] = UINT8_C(0x20);
            ram[0x0020] = UINT8_C(0x81);
        } else {
            ram[0x7401] = UINT8_C(0x34);
            ram[0x7402] = UINT8_C(0x12);
            ram[0x1234] = UINT8_C(0x81);
        }
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK((cpu.registers.psw & (MIPSNES2_SPC700_FLAG_N |
              MIPSNES2_SPC700_FLAG_Z | MIPSNES2_SPC700_FLAG_C)) ==
              MIPSNES2_SPC700_FLAG_N);
        CHECK(cpu.registers.x == UINT8_C(0x80) &&
              cpu.registers.y == UINT8_C(0x80));
        (void)compares[i].use_x;
    }

    /* TSET1/TCLR1 derive N/Z from A-memory and preserve all other flags. */
    CHECK(setup_memory());
    initial.pc = UINT16_C(0x7500);
    initial.a = UINT8_C(0x0f);
    initial.psw = MIPSNES2_SPC700_FLAG_C;
    ram[0x7500] = UINT8_C(0x0e);
    ram[0x7501] = UINT8_C(0x34);
    ram[0x7502] = UINT8_C(0x12);
    ram[0x1234] = UINT8_C(0x0f);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x1234] == UINT8_C(0x0f));
    CHECK((cpu.registers.psw & (MIPSNES2_SPC700_FLAG_C |
          MIPSNES2_SPC700_FLAG_Z)) ==
          (MIPSNES2_SPC700_FLAG_C | MIPSNES2_SPC700_FLAG_Z));
    ram[0x7500] = UINT8_C(0x4e);
    ram[0x1234] = UINT8_C(0xff);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x1234] == UINT8_C(0xf0));
    return 0;
}

static int test_word_operations(void)
{
    mipsnes2_spc700 cpu;
    mipsnes2_spc700_registers initial;

    CHECK(setup_memory());
    initial = (mipsnes2_spc700_registers){0};
    initial.pc = UINT16_C(0x4000);
    initial.psw = MIPSNES2_SPC700_FLAG_P;
    ram[0x4000] = UINT8_C(0xba);
    ram[0x4001] = UINT8_C(0xff);
    ram[0x01ff] = UINT8_C(0x34);
    ram[0x0100] = UINT8_C(0x80);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0x34) &&
          cpu.registers.y == UINT8_C(0x80));
    CHECK((cpu.registers.psw & MIPSNES2_SPC700_FLAG_N) != 0U);

    ram[0x4000] = UINT8_C(0xda);
    initial.a = UINT8_C(0xcd);
    initial.y = UINT8_C(0xab);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x01ff] == UINT8_C(0xcd) && ram[0x0100] == UINT8_C(0xab));
    CHECK(cpu.instruction_cycles == 5U);

    CHECK(setup_memory());
    initial.pc = UINT16_C(0x4000);
    initial.psw = 0U;
    initial.a = UINT8_C(0xff);
    initial.y = UINT8_C(0x7f);
    ram[0x4000] = UINT8_C(0x7a);
    ram[0x4001] = UINT8_C(0x20);
    ram[0x0020] = UINT8_C(1);
    ram[0x0021] = 0U;
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == 0U && cpu.registers.y == UINT8_C(0x80));
    CHECK((cpu.registers.psw & MIPSNES2_SPC700_FLAG_V) != 0U);
    CHECK((cpu.registers.psw & MIPSNES2_SPC700_FLAG_C) == 0U);

    ram[0x4000] = UINT8_C(0x9a);
    initial.a = 0U;
    initial.y = 0U;
    ram[0x0020] = UINT8_C(1);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.registers.a == UINT8_C(0xff) &&
          cpu.registers.y == UINT8_C(0xff));
    CHECK((cpu.registers.psw & MIPSNES2_SPC700_FLAG_C) == 0U);
    CHECK((cpu.registers.psw & MIPSNES2_SPC700_FLAG_N) != 0U);

    ram[0x4000] = UINT8_C(0x5a);
    initial.a = UINT8_C(0x34);
    initial.y = UINT8_C(0x12);
    ram[0x0020] = UINT8_C(0x34);
    ram[0x0021] = UINT8_C(0x12);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK((cpu.registers.psw & (MIPSNES2_SPC700_FLAG_C |
          MIPSNES2_SPC700_FLAG_Z)) ==
          (MIPSNES2_SPC700_FLAG_C | MIPSNES2_SPC700_FLAG_Z));

    ram[0x4000] = UINT8_C(0x3a);
    ram[0x0020] = UINT8_C(0xff);
    ram[0x0021] = UINT8_C(0xff);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x0020] == 0U && ram[0x0021] == 0U &&
          (cpu.registers.psw & MIPSNES2_SPC700_FLAG_Z) != 0U);
    ram[0x4000] = UINT8_C(0x1a);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(ram[0x0020] == UINT8_C(0xff) &&
          ram[0x0021] == UINT8_C(0xff));
    return 0;
}

static void reference_div(uint8_t a, uint8_t y, uint8_t x,
                          uint8_t *result_a, uint8_t *result_y)
{
    uint16_t ya = (uint16_t)((uint16_t)((uint16_t)y << 8U) | a);
    uint32_t intermediate;

    if ((uint16_t)y < (uint16_t)x * 2U) {
        *result_a = (uint8_t)(ya / x);
        *result_y = (uint8_t)(ya % x);
    } else {
        intermediate = (uint32_t)ya - (uint32_t)x * 512U;
        *result_a = (uint8_t)(255U - intermediate / (256U - x));
        *result_y = (uint8_t)((uint32_t)x +
                              intermediate % (256U - x));
    }
}

static int test_math_decimal_and_halt(void)
{
    static const uint8_t samples[] = {0U, 1U, 0x55U, 0x80U, 0xfeU, 0xffU};
    mipsnes2_spc700 cpu;
    mipsnes2_spc700_registers initial;
    uint8_t expected_a;
    uint8_t expected_y;
    unsigned a;
    unsigned y;
    unsigned x;
    unsigned flags;
    unsigned sample;

    CHECK(setup_memory());
    initial = (mipsnes2_spc700_registers){0};
    initial.pc = UINT16_C(0x5000);
    ram[0x5000] = UINT8_C(0x9e);
    for (x = 0U; x <= UINT8_MAX; ++x) {
        for (y = 0U; y <= UINT8_MAX; ++y) {
            for (sample = 0U; sample < sizeof(samples); ++sample) {
                initial.a = samples[sample];
                initial.y = (uint8_t)y;
                initial.x = (uint8_t)x;
                initial.psw = UINT8_C(0x31);
                reference_div(initial.a, initial.y, initial.x,
                              &expected_a, &expected_y);
                CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
                CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
                CHECK(cpu.registers.a == expected_a &&
                      cpu.registers.y == expected_y);
                CHECK(((cpu.registers.psw & MIPSNES2_SPC700_FLAG_H) != 0U) ==
                      ((initial.y & UINT8_C(0x0f)) >=
                       (initial.x & UINT8_C(0x0f))));
                CHECK(((cpu.registers.psw & MIPSNES2_SPC700_FLAG_V) != 0U) ==
                      (initial.y >= initial.x));
            }
        }
    }

    ram[0x5000] = UINT8_C(0xcf);
    for (a = 0U; a <= UINT8_MAX; ++a) {
        for (y = 0U; y <= UINT8_MAX; ++y) {
            uint16_t product = (uint16_t)(a * y);
            initial.a = (uint8_t)a;
            initial.y = (uint8_t)y;
            CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
            CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
            CHECK(cpu.registers.a == (uint8_t)product &&
                  cpu.registers.y == (uint8_t)(product >> 8U));
        }
    }

    for (flags = 0U; flags < 4U; ++flags) {
        for (a = 0U; a <= UINT8_MAX; ++a) {
            uint8_t adjusted;
            uint8_t expected_psw;
            initial.a = (uint8_t)a;
            initial.psw = (flags & 1U) != 0U ? MIPSNES2_SPC700_FLAG_C : 0U;
            if ((flags & 2U) != 0U) {
                initial.psw |= MIPSNES2_SPC700_FLAG_H;
            }
            expected_psw = initial.psw;
            adjusted = initial.a;
            if ((expected_psw & MIPSNES2_SPC700_FLAG_C) != 0U ||
                adjusted > UINT8_C(0x99)) {
                adjusted = (uint8_t)(adjusted + UINT8_C(0x60));
                expected_psw |= MIPSNES2_SPC700_FLAG_C;
            }
            if ((expected_psw & MIPSNES2_SPC700_FLAG_H) != 0U ||
                (adjusted & UINT8_C(0x0f)) > UINT8_C(9)) {
                adjusted = (uint8_t)(adjusted + UINT8_C(6));
            }
            expected_psw &= (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
                                       MIPSNES2_SPC700_FLAG_Z);
            expected_psw |= adjusted == 0U ? MIPSNES2_SPC700_FLAG_Z :
                ((adjusted & UINT8_C(0x80)) != 0U
                    ? MIPSNES2_SPC700_FLAG_N : 0U);
            ram[0x5000] = UINT8_C(0xdf);
            CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
            CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
            CHECK(cpu.registers.a == adjusted &&
                  cpu.registers.psw == expected_psw);
        }
    }

    for (flags = 0U; flags < 4U; ++flags) {
        for (a = 0U; a <= UINT8_MAX; ++a) {
            uint8_t adjusted;
            uint8_t expected_psw;
            initial.a = (uint8_t)a;
            initial.psw = (flags & 1U) != 0U ? MIPSNES2_SPC700_FLAG_C : 0U;
            if ((flags & 2U) != 0U) {
                initial.psw |= MIPSNES2_SPC700_FLAG_H;
            }
            expected_psw = initial.psw;
            adjusted = initial.a;
            if ((expected_psw & MIPSNES2_SPC700_FLAG_C) == 0U ||
                adjusted > UINT8_C(0x99)) {
                adjusted = (uint8_t)(adjusted - UINT8_C(0x60));
                expected_psw &= (uint8_t)~MIPSNES2_SPC700_FLAG_C;
            }
            if ((expected_psw & MIPSNES2_SPC700_FLAG_H) == 0U ||
                (adjusted & UINT8_C(0x0f)) > UINT8_C(9)) {
                adjusted = (uint8_t)(adjusted - UINT8_C(6));
            }
            expected_psw &= (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
                                       MIPSNES2_SPC700_FLAG_Z);
            expected_psw |= adjusted == 0U ? MIPSNES2_SPC700_FLAG_Z :
                ((adjusted & UINT8_C(0x80)) != 0U
                    ? MIPSNES2_SPC700_FLAG_N : 0U);
            ram[0x5000] = UINT8_C(0xbe);
            CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
            CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
            CHECK(cpu.registers.a == adjusted &&
                  cpu.registers.psw == expected_psw);
        }
    }

    ram[0x5000] = UINT8_C(0x9f);
    for (a = 0U; a <= UINT8_MAX; ++a) {
        initial.a = (uint8_t)a;
        initial.psw = UINT8_C(0x7d);
        expected_a = (uint8_t)((initial.a << 4U) | (initial.a >> 4U));
        CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
        CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
        CHECK(cpu.registers.a == expected_a);
        CHECK((cpu.registers.psw & (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
              MIPSNES2_SPC700_FLAG_Z)) ==
              (initial.psw & (uint8_t)~(MIPSNES2_SPC700_FLAG_N |
              MIPSNES2_SPC700_FLAG_Z)));
    }

    initial = (mipsnes2_spc700_registers){0};
    initial.pc = UINT16_C(0x5000);
    ram[0x5000] = UINT8_C(0xef);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.wait_state == MIPSNES2_SPC700_SLEEPING &&
          cpu.instruction_cycles == 3U);
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_HALTED);
    ram[0x5000] = UINT8_C(0xff);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_OK);
    CHECK(cpu.wait_state == MIPSNES2_SPC700_STOPPED);
    return 0;
}

static int test_special_failures(void)
{
    mipsnes2_spc700 cpu;
    mipsnes2_spc700_registers initial = {0};
    mipsnes2_spc700_registers before;

    CHECK(setup_memory());
    initial.pc = UINT16_C(0x6000);
    initial.psw = 0U;
    ram[0x6000] = UINT8_C(0xba);
    ram[0x6001] = UINT8_C(0xef);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) ==
          MIPSNES2_SPC700_MEMORY_UNAVAILABLE);
    CHECK(memcmp(&before, &cpu.registers, sizeof(before)) == 0);
    CHECK(cpu.instruction_cycles == 0U);

    CHECK(setup_memory());
    initial.pc = UINT16_C(0x6000);
    initial.psw = MIPSNES2_SPC700_FLAG_Z;
    ram[0x6000] = UINT8_C(0xf0);
    ram[0x6001] = UINT8_C(0x7f);
    CHECK(mipsnes2_spc700_init(&cpu, &memory, &initial));
    cpu.instruction_cycles = UINT64_MAX - UINT64_C(3);
    before = cpu.registers;
    CHECK(mipsnes2_spc700_step(&cpu) == MIPSNES2_SPC700_CYCLE_OVERFLOW);
    CHECK(memcmp(&before, &cpu.registers, sizeof(before)) == 0);
    CHECK(cpu.instruction_cycles == UINT64_MAX - UINT64_C(3));
    return 0;
}

int main(void)
{
    CHECK(test_complete_dispatch() == 0);
    CHECK(test_bit_and_branch_families() == 0);
    CHECK(test_flow_and_stack() == 0);
    CHECK(test_control_compare_and_test_bits() == 0);
    CHECK(test_word_operations() == 0);
    CHECK(test_math_decimal_and_halt() == 0);
    CHECK(test_special_failures() == 0);
    (void)puts("SPC700 complete 256-opcode dispatch: OK");
    return 0;
}
