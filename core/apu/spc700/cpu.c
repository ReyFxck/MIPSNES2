/* MIPSNES2 SPC700 execution foundation. See LICENSE for GPL-3.0 terms. */
#include "core/apu/spc700/cpu.h"

#include <stddef.h>

static bool is_alu_a_opcode(uint8_t opcode)
{
    uint8_t group = opcode & UINT8_C(0xe0);
    uint8_t mode = opcode & UINT8_C(0x1f);

    return group <= UINT8_C(0xa0) &&
        (mode == UINT8_C(0x04) || mode == UINT8_C(0x05) ||
         mode == UINT8_C(0x06) || mode == UINT8_C(0x07) ||
         mode == UINT8_C(0x08) || mode == UINT8_C(0x14) ||
         mode == UINT8_C(0x15) || mode == UINT8_C(0x16) ||
         mode == UINT8_C(0x17));
}

static bool is_alu_memory_opcode(uint8_t opcode)
{
    uint8_t group = opcode & UINT8_C(0xe0);
    uint8_t mode = opcode & UINT8_C(0x1f);

    return group <= UINT8_C(0xa0) &&
        (mode == UINT8_C(0x09) || mode == UINT8_C(0x18) ||
         mode == UINT8_C(0x19));
}

static bool is_shift_opcode(uint8_t opcode)
{
    uint8_t mode = opcode & UINT8_C(0x1f);

    return (opcode & UINT8_C(0x80)) == 0U &&
        (mode == UINT8_C(0x0b) || mode == UINT8_C(0x0c) ||
         mode == UINT8_C(0x1b) || mode == UINT8_C(0x1c));
}

static bool is_inc_dec_opcode(uint8_t opcode)
{
    switch (opcode) {
    case UINT8_C(0x1d): /* DEC X */
    case UINT8_C(0x3d): /* INC X */
    case UINT8_C(0x8b): /* DEC dp */
    case UINT8_C(0x8c): /* DEC !abs */
    case UINT8_C(0x9b): /* DEC dp+X */
    case UINT8_C(0x9c): /* DEC A */
    case UINT8_C(0xab): /* INC dp */
    case UINT8_C(0xac): /* INC !abs */
    case UINT8_C(0xbb): /* INC dp+X */
    case UINT8_C(0xbc): /* INC A */
    case UINT8_C(0xdc): /* DEC Y */
    case UINT8_C(0xfc): /* INC Y */
        return true;
    default:
        return false;
    }
}

static uint8_t opcode_cycles(uint8_t opcode)
{
    uint8_t mode = opcode & UINT8_C(0x1f);

    if (is_alu_a_opcode(opcode)) {
        if (mode == UINT8_C(0x08)) {
            return UINT8_C(2);
        }
        if (mode == UINT8_C(0x04) || mode == UINT8_C(0x06)) {
            return UINT8_C(3);
        }
        if (mode == UINT8_C(0x05) || mode == UINT8_C(0x14)) {
            return UINT8_C(4);
        }
        if (mode == UINT8_C(0x15) || mode == UINT8_C(0x16)) {
            return UINT8_C(5);
        }
        return UINT8_C(6);
    }
    if (is_alu_memory_opcode(opcode)) {
        return mode == UINT8_C(0x09) ? UINT8_C(6) : UINT8_C(5);
    }
    if (is_shift_opcode(opcode)) {
        if (mode == UINT8_C(0x1c)) {
            return UINT8_C(2);
        }
        return mode == UINT8_C(0x0b) ? UINT8_C(4) : UINT8_C(5);
    }
    if (is_inc_dec_opcode(opcode)) {
        switch (opcode) {
        case UINT8_C(0x8b):
        case UINT8_C(0xab):
            return UINT8_C(4);
        case UINT8_C(0x8c):
        case UINT8_C(0x9b):
        case UINT8_C(0xac):
        case UINT8_C(0xbb):
            return UINT8_C(5);
        default:
            return UINT8_C(2);
        }
    }

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
    case UINT8_C(0xe6): /* MOV A,(X) */
    case UINT8_C(0xc0): /* DI */
    case UINT8_C(0xe4): /* MOV A,dp */
    case UINT8_C(0xeb): /* MOV Y,dp */
    case UINT8_C(0xed): /* NOTC */
    case UINT8_C(0xf8): /* MOV X,dp */
        return UINT8_C(3);
    case UINT8_C(0xaf): /* MOV (X)+,A */
    case UINT8_C(0xbf): /* MOV A,(X)+ */
    case UINT8_C(0xc4): /* MOV dp,A */
    case UINT8_C(0xcb): /* MOV dp,Y */
    case UINT8_C(0xc6): /* MOV (X),A */
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
    case UINT8_C(0xf5): /* MOV A,!abs+X */
    case UINT8_C(0xf6): /* MOV A,!abs+Y */
    case UINT8_C(0x8f): /* MOV dp,#imm */
    case UINT8_C(0xfa): /* MOV dp,dp */
        return UINT8_C(5);
    case UINT8_C(0xd5): /* MOV !abs+X,A */
    case UINT8_C(0xd6): /* MOV !abs+Y,A */
        return UINT8_C(6);
    case UINT8_C(0xe7): /* MOV A,[dp+X] */
    case UINT8_C(0xf7): /* MOV A,[dp]+Y */
        return UINT8_C(6);
    case UINT8_C(0xc7): /* MOV [dp+X],A */
    case UINT8_C(0xd7): /* MOV [dp]+Y,A */
        return UINT8_C(7);
    default:
        return 0U;
    }
}

static bool opcode_has_two_byte_operands(uint8_t opcode)
{
    uint8_t mode = opcode & UINT8_C(0x1f);

    return opcode == UINT8_C(0x8f) || opcode == UINT8_C(0xfa) ||
        (is_alu_memory_opcode(opcode) &&
         (mode == UINT8_C(0x09) || mode == UINT8_C(0x18)));
}

static bool opcode_has_word_operand(uint8_t opcode)
{
    uint8_t mode = opcode & UINT8_C(0x1f);

    if ((is_alu_a_opcode(opcode) &&
         (mode == UINT8_C(0x05) || mode == UINT8_C(0x15) ||
          mode == UINT8_C(0x16))) ||
        (is_shift_opcode(opcode) && mode == UINT8_C(0x0c)) ||
        opcode == UINT8_C(0x8c) || opcode == UINT8_C(0xac)) {
        return true;
    }

    switch (opcode) {
    case UINT8_C(0xc5): /* MOV !abs,A */
    case UINT8_C(0xc9): /* MOV !abs,X */
    case UINT8_C(0xcc): /* MOV !abs,Y */
    case UINT8_C(0xd5): /* MOV !abs+X,A */
    case UINT8_C(0xd6): /* MOV !abs+Y,A */
    case UINT8_C(0xe5): /* MOV A,!abs */
    case UINT8_C(0xe9): /* MOV X,!abs */
    case UINT8_C(0xec): /* MOV Y,!abs */
    case UINT8_C(0xf5): /* MOV A,!abs+X */
    case UINT8_C(0xf6): /* MOV A,!abs+Y */
        return true;
    default:
        return false;
    }
}

static bool opcode_has_byte_operand(uint8_t opcode)
{
    uint8_t mode = opcode & UINT8_C(0x1f);

    if ((is_alu_a_opcode(opcode) &&
         (mode == UINT8_C(0x04) || mode == UINT8_C(0x07) ||
          mode == UINT8_C(0x08) || mode == UINT8_C(0x14) ||
          mode == UINT8_C(0x17))) ||
        (is_shift_opcode(opcode) &&
         (mode == UINT8_C(0x0b) || mode == UINT8_C(0x1b))) ||
        opcode == UINT8_C(0x8b) || opcode == UINT8_C(0x9b) ||
        opcode == UINT8_C(0xab) || opcode == UINT8_C(0xbb)) {
        return true;
    }

    switch (opcode) {
    case UINT8_C(0x8d): /* MOV Y,#imm */
    case UINT8_C(0xc4): /* MOV dp,A */
    case UINT8_C(0xc7): /* MOV [dp+X],A */
    case UINT8_C(0xcb): /* MOV dp,Y */
    case UINT8_C(0xcd): /* MOV X,#imm */
    case UINT8_C(0xd4): /* MOV dp+X,A */
    case UINT8_C(0xd7): /* MOV [dp]+Y,A */
    case UINT8_C(0xd8): /* MOV dp,X */
    case UINT8_C(0xd9): /* MOV dp+Y,X */
    case UINT8_C(0xdb): /* MOV dp+X,Y */
    case UINT8_C(0xe4): /* MOV A,dp */
    case UINT8_C(0xe7): /* MOV A,[dp+X] */
    case UINT8_C(0xe8): /* MOV A,#imm */
    case UINT8_C(0xeb): /* MOV Y,dp */
    case UINT8_C(0xf4): /* MOV A,dp+X */
    case UINT8_C(0xf7): /* MOV A,[dp]+Y */
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

static uint16_t indexed_absolute_address(uint16_t address, uint8_t index)
{
    return (uint16_t)(address + index);
}

static bool read_direct_page_pointer(mipsnes2_smp_memory *memory,
                                     uint8_t psw, uint8_t offset,
                                     uint16_t *address)
{
    uint8_t low;
    uint8_t high;

    if (!mipsnes2_smp_memory_read(memory,
                                  direct_page_address(psw, offset),
                                  &low) ||
        !mipsnes2_smp_memory_read(
            memory, direct_page_address(psw, (uint8_t)(offset + 1U)),
            &high)) {
        return false;
    }

    *address = (uint16_t)((uint16_t)((uint16_t)high << 8U) | low);
    return true;
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

static void set_flag(uint8_t *psw, uint8_t flag, bool enabled)
{
    if (enabled) {
        *psw |= flag;
    } else {
        *psw &= (uint8_t)~flag;
    }
}

static uint8_t execute_alu(uint8_t operation, uint8_t left, uint8_t right,
                           uint8_t *psw)
{
    uint8_t result;
    unsigned carry;
    unsigned sum;
    int difference;

    switch (operation) {
    case 0U: /* OR */
        result = left | right;
        break;
    case 1U: /* AND */
        result = left & right;
        break;
    case 2U: /* EOR */
        result = left ^ right;
        break;
    case 3U: /* CMP */
        result = (uint8_t)(left - right);
        set_flag(psw, MIPSNES2_SPC700_FLAG_C, left >= right);
        break;
    case 4U: /* ADC */
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
    default: /* SBC */
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

static bool read_alu_a_source(mipsnes2_smp_memory *memory,
                              const mipsnes2_spc700_registers *registers,
                              uint8_t opcode, uint8_t operand,
                              uint16_t absolute_address, uint8_t *value)
{
    uint16_t address;
    uint8_t mode = opcode & UINT8_C(0x1f);

    switch (mode) {
    case UINT8_C(0x04):
        address = direct_page_address(registers->psw, operand);
        break;
    case UINT8_C(0x05):
        address = absolute_address;
        break;
    case UINT8_C(0x06):
        address = direct_page_address(registers->psw, registers->x);
        break;
    case UINT8_C(0x07):
        if (!read_direct_page_pointer(
                memory, registers->psw,
                (uint8_t)(operand + registers->x), &address)) {
            return false;
        }
        break;
    case UINT8_C(0x08):
        *value = operand;
        return true;
    case UINT8_C(0x14):
        address = indexed_direct_page_address(registers->psw, operand,
                                              registers->x);
        break;
    case UINT8_C(0x15):
        address = indexed_absolute_address(absolute_address, registers->x);
        break;
    case UINT8_C(0x16):
        address = indexed_absolute_address(absolute_address, registers->y);
        break;
    default: /* [dp]+Y */
        if (!read_direct_page_pointer(memory, registers->psw, operand,
                                      &address)) {
            return false;
        }
        address = indexed_absolute_address(address, registers->y);
        break;
    }

    return mipsnes2_smp_memory_read(memory, address, value);
}

static mipsnes2_spc700_result execute_alu_a_opcode(
    mipsnes2_smp_memory *memory, mipsnes2_spc700_registers *registers,
    uint8_t opcode, uint8_t operand, uint16_t absolute_address)
{
    uint8_t source;
    uint8_t operation = opcode >> 5U;
    uint8_t result;

    if (!read_alu_a_source(memory, registers, opcode, operand,
                           absolute_address, &source)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }
    result = execute_alu(operation, registers->a, source, &registers->psw);
    if (operation != 3U) {
        registers->a = result;
    }
    return MIPSNES2_SPC700_OK;
}

static mipsnes2_spc700_result execute_alu_memory_opcode(
    mipsnes2_smp_memory *memory, mipsnes2_spc700_registers *registers,
    uint8_t opcode, uint8_t operand, uint8_t second_operand)
{
    uint16_t source_address;
    uint16_t destination_address;
    uint8_t source;
    uint8_t destination;
    uint8_t operation = opcode >> 5U;
    uint8_t mode = opcode & UINT8_C(0x1f);
    uint8_t result;

    if (mode == UINT8_C(0x09)) {
        source_address = direct_page_address(registers->psw, operand);
        destination_address = direct_page_address(registers->psw,
                                                  second_operand);
        if (!mipsnes2_smp_memory_read(memory, source_address, &source)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (mode == UINT8_C(0x18)) {
        source = operand;
        destination_address = direct_page_address(registers->psw,
                                                  second_operand);
    } else {
        source_address = direct_page_address(registers->psw, registers->y);
        destination_address = direct_page_address(registers->psw,
                                                  registers->x);
        if (!mipsnes2_smp_memory_read(memory, source_address, &source)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    }

    if (!mipsnes2_smp_memory_read(memory, destination_address,
                                  &destination)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }
    result = execute_alu(operation, destination, source, &registers->psw);
    if (operation != 3U &&
        !mipsnes2_smp_memory_write(memory, destination_address, result)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }
    return MIPSNES2_SPC700_OK;
}

static mipsnes2_spc700_result execute_shift_opcode(
    mipsnes2_smp_memory *memory, mipsnes2_spc700_registers *registers,
    uint8_t opcode, uint8_t operand, uint16_t absolute_address)
{
    uint16_t address = 0U;
    uint8_t mode = opcode & UINT8_C(0x1f);
    uint8_t operation = opcode >> 5U;
    uint8_t value;
    uint8_t result;
    bool accumulator = mode == UINT8_C(0x1c);
    bool old_carry = (registers->psw & MIPSNES2_SPC700_FLAG_C) != 0U;

    if (accumulator) {
        value = registers->a;
    } else {
        if (mode == UINT8_C(0x0b)) {
            address = direct_page_address(registers->psw, operand);
        } else if (mode == UINT8_C(0x0c)) {
            address = absolute_address;
        } else {
            address = indexed_direct_page_address(registers->psw, operand,
                                                  registers->x);
        }
        if (!mipsnes2_smp_memory_read(memory, address, &value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    }

    if (operation == 0U) { /* ASL */
        set_flag(&registers->psw, MIPSNES2_SPC700_FLAG_C,
                 (value & UINT8_C(0x80)) != 0U);
        result = (uint8_t)(value << 1U);
    } else if (operation == 1U) { /* ROL */
        set_flag(&registers->psw, MIPSNES2_SPC700_FLAG_C,
                 (value & UINT8_C(0x80)) != 0U);
        result = (uint8_t)((uint8_t)(value << 1U) |
                           (old_carry ? UINT8_C(1) : UINT8_C(0)));
    } else if (operation == 2U) { /* LSR */
        set_flag(&registers->psw, MIPSNES2_SPC700_FLAG_C,
                 (value & UINT8_C(1)) != 0U);
        result = value >> 1U;
    } else { /* ROR */
        set_flag(&registers->psw, MIPSNES2_SPC700_FLAG_C,
                 (value & UINT8_C(1)) != 0U);
        result = (uint8_t)((value >> 1U) |
                           (old_carry ? UINT8_C(0x80) : UINT8_C(0)));
    }
    set_negative_zero(&registers->psw, result);

    if (accumulator) {
        registers->a = result;
    } else if (!mipsnes2_smp_memory_write(memory, address, result)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }
    return MIPSNES2_SPC700_OK;
}

static mipsnes2_spc700_result execute_inc_dec_opcode(
    mipsnes2_smp_memory *memory, mipsnes2_spc700_registers *registers,
    uint8_t opcode, uint8_t operand, uint16_t absolute_address)
{
    uint16_t address = 0U;
    uint8_t *target = NULL;
    uint8_t value;
    bool decrement = opcode == UINT8_C(0x1d) ||
        opcode == UINT8_C(0x8b) || opcode == UINT8_C(0x8c) ||
        opcode == UINT8_C(0x9b) || opcode == UINT8_C(0x9c) ||
        opcode == UINT8_C(0xdc);

    switch (opcode) {
    case UINT8_C(0x1d):
    case UINT8_C(0x3d):
        target = &registers->x;
        break;
    case UINT8_C(0x9c):
    case UINT8_C(0xbc):
        target = &registers->a;
        break;
    case UINT8_C(0xdc):
    case UINT8_C(0xfc):
        target = &registers->y;
        break;
    case UINT8_C(0x8b):
    case UINT8_C(0xab):
        address = direct_page_address(registers->psw, operand);
        break;
    case UINT8_C(0x8c):
    case UINT8_C(0xac):
        address = absolute_address;
        break;
    default:
        address = indexed_direct_page_address(registers->psw, operand,
                                              registers->x);
        break;
    }

    if (target != NULL) {
        value = *target;
    } else if (!mipsnes2_smp_memory_read(memory, address, &value)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }
    value = decrement ? (uint8_t)(value - 1U) : (uint8_t)(value + 1U);
    set_negative_zero(&registers->psw, value);
    if (target != NULL) {
        *target = value;
    } else if (!mipsnes2_smp_memory_write(memory, address, value)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }
    return MIPSNES2_SPC700_OK;
}

static mipsnes2_spc700_result execute_extended_opcode(
    mipsnes2_smp_memory *memory, mipsnes2_spc700_registers *registers,
    uint8_t opcode, uint8_t operand, uint8_t second_operand,
    uint16_t absolute_address, bool *handled)
{
    *handled = true;
    if (is_alu_a_opcode(opcode)) {
        return execute_alu_a_opcode(memory, registers, opcode, operand,
                                    absolute_address);
    }
    if (is_alu_memory_opcode(opcode)) {
        return execute_alu_memory_opcode(memory, registers, opcode, operand,
                                         second_operand);
    }
    if (is_shift_opcode(opcode)) {
        return execute_shift_opcode(memory, registers, opcode, operand,
                                    absolute_address);
    }
    if (is_inc_dec_opcode(opcode)) {
        return execute_inc_dec_opcode(memory, registers, opcode, operand,
                                      absolute_address);
    }
    *handled = false;
    return MIPSNES2_SPC700_OK;
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
    uint16_t effective_address = 0U;
    uint8_t opcode;
    uint8_t operand = 0U;
    uint8_t second_operand = 0U;
    uint8_t absolute_high = 0U;
    uint8_t memory_value = 0U;
    uint8_t cycles;
    bool extended_handled;
    mipsnes2_spc700_result extended_result;

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
    if (opcode_has_two_byte_operands(opcode)) {
        if (!mipsnes2_smp_memory_read(cpu->memory, next.pc, &operand)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        next.pc = (uint16_t)(next.pc + UINT16_C(1));
        if (!mipsnes2_smp_memory_read(cpu->memory, next.pc,
                                      &second_operand)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        next.pc = (uint16_t)(next.pc + UINT16_C(1));
    } else if (opcode_has_byte_operand(opcode)) {
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

    extended_result = execute_extended_opcode(
        cpu->memory, &next, opcode, operand, second_operand,
        absolute_address, &extended_handled);
    if (extended_result != MIPSNES2_SPC700_OK) {
        return extended_result;
    }
    if (extended_handled) {
        goto commit_instruction;
    }

    if (opcode == UINT8_C(0xc7) || opcode == UINT8_C(0xe7)) {
        if (!read_direct_page_pointer(
                cpu->memory, next.psw, (uint8_t)(operand + next.x),
                &effective_address)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xd7) || opcode == UINT8_C(0xf7)) {
        if (!read_direct_page_pointer(cpu->memory, next.psw, operand,
                                      &effective_address)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        effective_address = indexed_absolute_address(effective_address,
                                                     next.y);
    }

    if (opcode == UINT8_C(0xfa)) {
        if (!mipsnes2_smp_memory_read(
                cpu->memory, direct_page_address(next.psw, operand),
                &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xe7) || opcode == UINT8_C(0xf7)) {
        if (!mipsnes2_smp_memory_read(cpu->memory, effective_address,
                                      &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xe6) || opcode == UINT8_C(0xbf)) {
        if (!mipsnes2_smp_memory_read(
                cpu->memory, direct_page_address(next.psw, next.x),
                &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xf5)) {
        if (!mipsnes2_smp_memory_read(
                cpu->memory,
                indexed_absolute_address(absolute_address, next.x),
                &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xf6)) {
        if (!mipsnes2_smp_memory_read(
                cpu->memory,
                indexed_absolute_address(absolute_address, next.y),
                &memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
    } else if (opcode == UINT8_C(0xe5) || opcode == UINT8_C(0xe9) ||
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
    case UINT8_C(0x8f): /* MOV dp,#imm */
        if (!mipsnes2_smp_memory_write(
                cpu->memory,
                direct_page_address(next.psw, second_operand), operand)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0x9d): /* MOV X,SP */
        next.x = next.sp;
        set_negative_zero(&next.psw, next.x);
        break;
    case UINT8_C(0xa0): /* EI */
        next.psw |= MIPSNES2_SPC700_FLAG_I;
        break;
    case UINT8_C(0xaf): /* MOV (X)+,A */
        if (!mipsnes2_smp_memory_write(
                cpu->memory, direct_page_address(next.psw, next.x),
                next.a)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        next.x = (uint8_t)(next.x + 1U);
        break;
    case UINT8_C(0xbd): /* MOV SP,X */
        next.sp = next.x;
        break;
    case UINT8_C(0xbf): /* MOV A,(X)+ */
        next.a = memory_value;
        next.x = (uint8_t)(next.x + 1U);
        set_negative_zero(&next.psw, next.a);
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
    case UINT8_C(0xc6): /* MOV (X),A */
        if (!mipsnes2_smp_memory_write(
                cpu->memory, direct_page_address(next.psw, next.x),
                next.a)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xc7): /* MOV [dp+X],A */
        if (!mipsnes2_smp_memory_write(cpu->memory, effective_address,
                                       next.a)) {
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
    case UINT8_C(0xd5): /* MOV !abs+X,A */
        if (!mipsnes2_smp_memory_write(
                cpu->memory,
                indexed_absolute_address(absolute_address, next.x),
                next.a)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xd6): /* MOV !abs+Y,A */
        if (!mipsnes2_smp_memory_write(
                cpu->memory,
                indexed_absolute_address(absolute_address, next.y),
                next.a)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
        break;
    case UINT8_C(0xd7): /* MOV [dp]+Y,A */
        if (!mipsnes2_smp_memory_write(cpu->memory, effective_address,
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
    case UINT8_C(0xe6): /* MOV A,(X) */
    case UINT8_C(0xe7): /* MOV A,[dp+X] */
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
    case UINT8_C(0xf5): /* MOV A,!abs+X */
    case UINT8_C(0xf6): /* MOV A,!abs+Y */
        next.a = memory_value;
        set_negative_zero(&next.psw, next.a);
        break;
    case UINT8_C(0xf7): /* MOV A,[dp]+Y */
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
    case UINT8_C(0xfa): /* MOV dp,dp */
        if (!mipsnes2_smp_memory_write(
                cpu->memory,
                direct_page_address(next.psw, second_operand),
                memory_value)) {
            return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
        }
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

commit_instruction:
    if (!mipsnes2_smp_memory_advance(cpu->memory, cycles)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }

    cpu->registers = next;
    cpu->instruction_cycles += cycles;
    return MIPSNES2_SPC700_OK;
}
