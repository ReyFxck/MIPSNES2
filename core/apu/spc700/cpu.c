/* MIPSNES2 SPC700 execution foundation. See LICENSE for GPL-3.0 terms. */
#include "core/apu/spc700/cpu.h"

#include <stddef.h>

static uint8_t opcode_cycles(uint8_t opcode)
{
    switch (opcode) {
    case UINT8_C(0x00): /* NOP */
    case UINT8_C(0x20): /* CLRP */
    case UINT8_C(0x40): /* SETP */
    case UINT8_C(0x5d): /* MOV X,A */
    case UINT8_C(0x60): /* CLRC */
    case UINT8_C(0x7d): /* MOV A,X */
    case UINT8_C(0x80): /* SETC */
    case UINT8_C(0x8d): /* MOV Y,#imm */
    case UINT8_C(0x9d): /* MOV X,SP */
    case UINT8_C(0xbd): /* MOV SP,X */
    case UINT8_C(0xcd): /* MOV X,#imm */
    case UINT8_C(0xdd): /* MOV A,Y */
    case UINT8_C(0xe0): /* CLRV */
    case UINT8_C(0xe8): /* MOV A,#imm */
    case UINT8_C(0xfd): /* MOV Y,A */
        return UINT8_C(2);
    case UINT8_C(0xa0): /* EI */
    case UINT8_C(0xc0): /* DI */
    case UINT8_C(0xed): /* NOTC */
        return UINT8_C(3);
    default:
        return 0U;
    }
}

static bool opcode_has_immediate(uint8_t opcode)
{
    return opcode == UINT8_C(0x8d) || opcode == UINT8_C(0xcd) ||
           opcode == UINT8_C(0xe8);
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

bool mipsnes2_spc700_init(mipsnes2_spc700 *cpu,
                          mipsnes2_smp_memory *memory,
                          const mipsnes2_spc700_registers *registers)
{
    if (cpu == NULL) {
        return false;
    }

    *cpu = (mipsnes2_spc700){0};
    if (memory == NULL || !memory->initialized || registers == NULL) {
        return false;
    }

    cpu->registers = *registers;
    cpu->memory = memory;
    cpu->initialized = true;
    return true;
}

mipsnes2_spc700_result mipsnes2_spc700_step(mipsnes2_spc700 *cpu)
{
    mipsnes2_spc700_registers next;
    uint16_t opcode_address;
    uint8_t opcode;
    uint8_t operand = 0U;
    uint8_t cycles;

    if (cpu == NULL) {
        return MIPSNES2_SPC700_INVALID_ARGUMENT;
    }
    if (!cpu->initialized || cpu->memory == NULL ||
        !cpu->memory->initialized) {
        return MIPSNES2_SPC700_NOT_READY;
    }

    opcode_address = cpu->registers.pc;
    if (!mipsnes2_smp_memory_read(cpu->memory, opcode_address, &opcode)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }
    cpu->last_opcode_address = opcode_address;
    cpu->last_opcode = opcode;
    cpu->has_fetched_opcode = true;

    cycles = opcode_cycles(opcode);
    if (cycles == 0U) {
        return MIPSNES2_SPC700_UNIMPLEMENTED_OPCODE;
    }
    if (cpu->instruction_cycles > UINT64_MAX - cycles) {
        return MIPSNES2_SPC700_CYCLE_OVERFLOW;
    }

    next = cpu->registers;
    next.pc = (uint16_t)(next.pc + UINT16_C(1));
    if (opcode_has_immediate(opcode)) {
        if (!mipsnes2_smp_memory_read(cpu->memory, next.pc, &operand)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        next.pc = (uint16_t)(next.pc + UINT16_C(1));
    }

    switch (opcode) {
    case UINT8_C(0x00): /* NOP */
        break;
    case UINT8_C(0x20): /* CLRP */
        next.psw &= (uint8_t)~MIPSNES2_SPC700_FLAG_P;
        break;
    case UINT8_C(0x40): /* SETP */
        next.psw |= MIPSNES2_SPC700_FLAG_P;
        break;
    case UINT8_C(0x5d): /* MOV X,A */
        next.x = next.a;
        set_negative_zero(&next.psw, next.x);
        break;
    case UINT8_C(0x60): /* CLRC */
        next.psw &= (uint8_t)~MIPSNES2_SPC700_FLAG_C;
        break;
    case UINT8_C(0x7d): /* MOV A,X */
        next.a = next.x;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0x80): /* SETC */
        next.psw |= MIPSNES2_SPC700_FLAG_C;
        break;
    case UINT8_C(0x8d): /* MOV Y,#imm */
        next.y = operand;
        set_negative_zero(&next.psw, next.y);
        break;
    case UINT8_C(0x9d): /* MOV X,SP */
        next.x = next.sp;
        set_negative_zero(&next.psw, next.x);
        break;
    case UINT8_C(0xa0): /* EI */
        next.psw |= MIPSNES2_SPC700_FLAG_I;
        break;
    case UINT8_C(0xbd): /* MOV SP,X */
        next.sp = next.x;
        break;
    case UINT8_C(0xc0): /* DI */
        next.psw &= (uint8_t)~MIPSNES2_SPC700_FLAG_I;
        break;
    case UINT8_C(0xcd): /* MOV X,#imm */
        next.x = operand;
        set_negative_zero(&next.psw, next.x);
        break;
    case UINT8_C(0xdd): /* MOV A,Y */
        next.a = next.y;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0xe0): /* CLRV also clears half carry. */
        next.psw &= (uint8_t)~(MIPSNES2_SPC700_FLAG_V |
                               MIPSNES2_SPC700_FLAG_H);
        break;
    case UINT8_C(0xe8): /* MOV A,#imm */
        next.a = operand;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0xed): /* NOTC */
        next.psw ^= MIPSNES2_SPC700_FLAG_C;
        break;
    case UINT8_C(0xfd): /* MOV Y,A */
        next.y = next.a;
        set_negative_zero(&next.psw, next.y);
        break;
    default:
        /* opcode_cycles rejects this before operand fetch or mutation. */
        return MIPSNES2_SPC700_UNIMPLEMENTED_OPCODE;
    }

    if (!mipsnes2_smp_memory_advance(cpu->memory, cycles)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }

    cpu->registers = next;
    cpu->instruction_cycles += cycles;
    return MIPSNES2_SPC700_OK;
}
