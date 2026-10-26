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
    case UINT8_C(0xe4): /* MOV A,dp */
    case UINT8_C(0xeb): /* MOV Y,dp */
    case UINT8_C(0xed): /* NOTC */
    case UINT8_C(0xf8): /* MOV X,dp */
        return UINT8_C(3);
    case UINT8_C(0xc4): /* MOV dp,A */
    case UINT8_C(0xcb): /* MOV dp,Y */
    case UINT8_C(0xd8): /* MOV dp,X */
    case UINT8_C(0xe5): /* MOV A,!abs */
    case UINT8_C(0xe9): /* MOV X,!abs */
    case UINT8_C(0xec): /* MOV Y,!abs */
    case UINT8_C(0xf4): /* MOV A,dp+X */
    case UINT8_C(0xf9): /* MOV X,dp+Y */
    case UINT8_C(0xfb): /* MOV Y,dp+X */
        return UINT8_C(4);
    case UINT8_C(0xc5): /* MOV !abs,A */
    case UINT8_C(0xc9): /* MOV !abs,X */
    case UINT8_C(0xcc): /* MOV !abs,Y */
    case UINT8_C(0xd4): /* MOV dp+X,A */
    case UINT8_C(0xd9): /* MOV dp+Y,X */
    case UINT8_C(0xdb): /* MOV dp+X,Y */
        return UINT8_C(5);
    default:
        return 0U;
    }
}

static bool opcode_has_word_operand(uint8_t opcode)
{
    switch (opcode) {
    case UINT8_C(0xc5): /* MOV !abs,A */
    case UINT8_C(0xc9): /* MOV !abs,X */
    case UINT8_C(0xcc): /* MOV !abs,Y */
    case UINT8_C(0xe5): /* MOV A,!abs */
    case UINT8_C(0xe9): /* MOV X,!abs */
    case UINT8_C(0xec): /* MOV Y,!abs */
        return true;
    default:
        return false;
    }
}

static bool opcode_has_byte_operand(uint8_t opcode)
{
    switch (opcode) {
    case UINT8_C(0x8d): /* MOV Y,#imm */
    case UINT8_C(0xc4): /* MOV dp,A */
    case UINT8_C(0xcb): /* MOV dp,Y */
    case UINT8_C(0xcd): /* MOV X,#imm */
    case UINT8_C(0xd4): /* MOV dp+X,A */
    case UINT8_C(0xd8): /* MOV dp,X */
    case UINT8_C(0xd9): /* MOV dp+Y,X */
    case UINT8_C(0xdb): /* MOV dp+X,Y */
    case UINT8_C(0xe4): /* MOV A,dp */
    case UINT8_C(0xe8): /* MOV A,#imm */
    case UINT8_C(0xeb): /* MOV Y,dp */
    case UINT8_C(0xf4): /* MOV A,dp+X */
    case UINT8_C(0xf8): /* MOV X,dp */
    case UINT8_C(0xf9): /* MOV X,dp+Y */
    case UINT8_C(0xfb): /* MOV Y,dp+X */
        return true;
    default:
        return false;
    }
}

static uint16_t direct_page_address(uint8_t psw, uint8_t offset)
{
    uint16_t base = (psw & MIPSNES2_SPC700_FLAG_P) != 0U
        ? UINT16_C(0x0100) : UINT16_C(0x0000);

    return (uint16_t)(base | offset);
}

static uint16_t indexed_direct_page_address(uint8_t psw, uint8_t offset,
                                            uint8_t index)
{
    return direct_page_address(psw, (uint8_t)(offset + index));
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
    uint16_t absolute_address = 0U;
    uint8_t opcode;
    uint8_t operand = 0U;
    uint8_t absolute_high = 0U;
    uint8_t memory_value = 0U;
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
    if (opcode_has_byte_operand(opcode)) {
        if (!mipsnes2_smp_memory_read(cpu->memory, next.pc, &operand)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        next.pc = (uint16_t)(next.pc + UINT16_C(1));
    } else if (opcode_has_word_operand(opcode)) {
        if (!mipsnes2_smp_memory_read(cpu->memory, next.pc, &operand)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        next.pc = (uint16_t)(next.pc + UINT16_C(1));
        if (!mipsnes2_smp_memory_read(cpu->memory, next.pc,
                                      &absolute_high)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        next.pc = (uint16_t)(next.pc + UINT16_C(1));
        absolute_address = (uint16_t)(
            (uint16_t)((uint16_t)absolute_high << 8U) | operand);
    }

    if (opcode == UINT8_C(0xe5) || opcode == UINT8_C(0xe9) ||
        opcode == UINT8_C(0xec)) {
        if (!mipsnes2_smp_memory_read(cpu->memory, absolute_address,
                                      &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xe4) || opcode == UINT8_C(0xeb) ||
        opcode == UINT8_C(0xf8)) {
        if (!mipsnes2_smp_memory_read(
                cpu->memory, direct_page_address(next.psw, operand),
                &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xf4) || opcode == UINT8_C(0xfb)) {
        if (!mipsnes2_smp_memory_read(
                cpu->memory,
                indexed_direct_page_address(next.psw, operand, next.x),
                &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xf9)) {
        if (!mipsnes2_smp_memory_read(
                cpu->memory,
                indexed_direct_page_address(next.psw, operand, next.y),
                &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
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
    case UINT8_C(0xc4): /* MOV dp,A */
        if (!mipsnes2_smp_memory_write(
                cpu->memory, direct_page_address(next.psw, operand),
                next.a)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xc5): /* MOV !abs,A */
        if (!mipsnes2_smp_memory_write(
                cpu->memory, absolute_address, next.a)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xc9): /* MOV !abs,X */
        if (!mipsnes2_smp_memory_write(
                cpu->memory, absolute_address, next.x)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xcb): /* MOV dp,Y */
        if (!mipsnes2_smp_memory_write(
                cpu->memory, direct_page_address(next.psw, operand),
                next.y)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xcc): /* MOV !abs,Y */
        if (!mipsnes2_smp_memory_write(
                cpu->memory, absolute_address, next.y)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xcd): /* MOV X,#imm */
        next.x = operand;
        set_negative_zero(&next.psw, next.x);
        break;
    case UINT8_C(0xd4): /* MOV dp+X,A */
        if (!mipsnes2_smp_memory_write(
                cpu->memory,
                indexed_direct_page_address(next.psw, operand, next.x),
                next.a)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xd8): /* MOV dp,X */
        if (!mipsnes2_smp_memory_write(
                cpu->memory, direct_page_address(next.psw, operand),
                next.x)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xd9): /* MOV dp+Y,X */
        if (!mipsnes2_smp_memory_write(
                cpu->memory,
                indexed_direct_page_address(next.psw, operand, next.y),
                next.x)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xdb): /* MOV dp+X,Y */
        if (!mipsnes2_smp_memory_write(
                cpu->memory,
                indexed_direct_page_address(next.psw, operand, next.x),
                next.y)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xdd): /* MOV A,Y */
        next.a = next.y;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0xe0): /* CLRV also clears half carry. */
        next.psw &= (uint8_t)~(MIPSNES2_SPC700_FLAG_V |
                               MIPSNES2_SPC700_FLAG_H);
        break;
    case UINT8_C(0xe4): /* MOV A,dp */
        next.a = memory_value;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0xe5): /* MOV A,!abs */
        next.a = memory_value;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0xe8): /* MOV A,#imm */
        next.a = operand;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0xe9): /* MOV X,!abs */
        next.x = memory_value;
        set_negative_zero(&next.psw, next.x);
        break;
    case UINT8_C(0xeb): /* MOV Y,dp */
        next.y = memory_value;
        set_negative_zero(&next.psw, next.y);
        break;
    case UINT8_C(0xec): /* MOV Y,!abs */
        next.y = memory_value;
        set_negative_zero(&next.psw, next.y);
        break;
    case UINT8_C(0xed): /* NOTC */
        next.psw ^= MIPSNES2_SPC700_FLAG_C;
        break;
    case UINT8_C(0xf4): /* MOV A,dp+X */
        next.a = memory_value;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0xf8): /* MOV X,dp */
        next.x = memory_value;
        set_negative_zero(&next.psw, next.x);
        break;
    case UINT8_C(0xf9): /* MOV X,dp+Y */
        next.x = memory_value;
        set_negative_zero(&next.psw, next.x);
        break;
    case UINT8_C(0xfb): /* MOV Y,dp+X */
        next.y = memory_value;
        set_negative_zero(&next.psw, next.y);
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
