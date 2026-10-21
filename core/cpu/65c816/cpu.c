/* MIPSNES2 65C816 foundation. See LICENSE for GPL-3.0 terms. */
#include "core/cpu/65c816/cpu.h"

#include <string.h>

static bool accumulator_is_8_bit(const mipsnes2_cpu *cpu)
{
    return cpu->emulation || (cpu->p & MIPSNES2_CPU_FLAG_M) != 0U;
}

static bool index_is_8_bit(const mipsnes2_cpu *cpu)
{
    return cpu->emulation || (cpu->p & MIPSNES2_CPU_FLAG_X) != 0U;
}

static void apply_mode_invariants(mipsnes2_cpu *cpu)
{
    if (cpu->emulation) {
        cpu->p |= MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X;
        cpu->s = (uint16_t)(UINT16_C(0x0100) | (cpu->s & UINT16_C(0x00ff)));
    }
    if ((cpu->p & MIPSNES2_CPU_FLAG_X) != 0U) {
        cpu->x &= UINT16_C(0x00ff);
        cpu->y &= UINT16_C(0x00ff);
    }
}

static uint8_t read8(const mipsnes2_cpu *cpu, mipsnes2_address address)
{
    return cpu->bus.read8(cpu->bus.context, mipsnes2_address24(address));
}

static void write8(const mipsnes2_cpu *cpu, mipsnes2_address address,
                   uint8_t value)
{
    cpu->bus.write8(cpu->bus.context, mipsnes2_address24(address), value);
}

static bool require_write_bus(mipsnes2_cpu *cpu)
{
    if (cpu->bus.write8 != NULL) {
        return true;
    }
    cpu->stopped = true;
    cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
    return false;
}

static uint8_t fetch8(mipsnes2_cpu *cpu)
{
    uint8_t value = read8(cpu, mipsnes2_address_make(cpu->pbr, cpu->pc));
    cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
    return value;
}

static uint16_t fetch16(mipsnes2_cpu *cpu)
{
    uint16_t low = fetch8(cpu);
    uint16_t high = fetch8(cpu);
    return (uint16_t)(low | (uint16_t)(high << 8));
}

static mipsnes2_address fetch24(mipsnes2_cpu *cpu)
{
    mipsnes2_address low = fetch8(cpu);
    mipsnes2_address high = fetch8(cpu);
    mipsnes2_address bank = fetch8(cpu);
    return low | (high << 8) | (bank << 16);
}

static uint16_t read16_linear(const mipsnes2_cpu *cpu,
                              mipsnes2_address address)
{
    uint16_t low = read8(cpu, address);
    uint16_t high = read8(cpu, address + UINT32_C(1));
    return (uint16_t)(low | (uint16_t)(high << 8));
}

static uint16_t read16_bank_zero(const mipsnes2_cpu *cpu, uint16_t address)
{
    uint16_t low = read8(cpu, address);
    uint16_t high = read8(cpu, (uint16_t)(address + UINT16_C(1)));

    return (uint16_t)(low | (uint16_t)(high << 8));
}

static uint16_t read16_program_bank(const mipsnes2_cpu *cpu,
                                    uint16_t address)
{
    uint16_t low = read8(cpu, mipsnes2_address_make(cpu->pbr, address));
    uint16_t high = read8(
        cpu, mipsnes2_address_make(cpu->pbr,
                                   (uint16_t)(address + UINT16_C(1))));

    return (uint16_t)(low | (uint16_t)(high << 8));
}

static mipsnes2_address read24_bank_zero(const mipsnes2_cpu *cpu,
                                         uint16_t address)
{
    mipsnes2_address low = read8(cpu, address);
    mipsnes2_address high = read8(
        cpu, (uint16_t)(address + UINT16_C(1)));
    mipsnes2_address bank = read8(
        cpu, (uint16_t)(address + UINT16_C(2)));

    return low | (high << 8) | (bank << 16);
}

static void write16_bank_zero(const mipsnes2_cpu *cpu, uint16_t address,
                              uint16_t value)
{
    write8(cpu, address, (uint8_t)value);
    write8(cpu, (uint16_t)(address + UINT16_C(1)),
           (uint8_t)(value >> 8));
}

static void write16_linear(const mipsnes2_cpu *cpu,
                           mipsnes2_address address, uint16_t value)
{
    write8(cpu, address, (uint8_t)value);
    write8(cpu, address + UINT32_C(1), (uint8_t)(value >> 8));
}

static void stack_decrement(mipsnes2_cpu *cpu)
{
    cpu->s = (uint16_t)(cpu->s - UINT16_C(1));
    if (cpu->emulation) {
        cpu->s = (uint16_t)(UINT16_C(0x0100) |
                            (cpu->s & UINT16_C(0x00ff)));
    }
}

static void stack_increment(mipsnes2_cpu *cpu)
{
    cpu->s = (uint16_t)(cpu->s + UINT16_C(1));
    if (cpu->emulation) {
        cpu->s = (uint16_t)(UINT16_C(0x0100) |
                            (cpu->s & UINT16_C(0x00ff)));
    }
}

static void stack_push8(mipsnes2_cpu *cpu, uint8_t value)
{
    write8(cpu, cpu->s, value);
    stack_decrement(cpu);
}

static uint8_t stack_pull8(mipsnes2_cpu *cpu)
{
    stack_increment(cpu);
    return read8(cpu, cpu->s);
}

static void stack_push16(mipsnes2_cpu *cpu, uint16_t value)
{
    stack_push8(cpu, (uint8_t)(value >> 8));
    stack_push8(cpu, (uint8_t)value);
}

static uint16_t stack_pull16(mipsnes2_cpu *cpu)
{
    uint16_t low = stack_pull8(cpu);
    uint16_t high = stack_pull8(cpu);

    return (uint16_t)(low | (uint16_t)(high << 8));
}

/*
 * WDC documents these fixed-width operations as exceptions that may access
 * outside 01xx while E=1. Keep their linear address sequence separate from
 * ordinary byte-at-a-time stack wrapping.
 */
static void stack_push16_linear(mipsnes2_cpu *cpu, uint16_t value)
{
    uint16_t initial = cpu->s;

    write8(cpu, initial, (uint8_t)(value >> 8));
    write8(cpu, (uint16_t)(initial - UINT16_C(1)), (uint8_t)value);
    cpu->s = (uint16_t)(initial - UINT16_C(2));
    apply_mode_invariants(cpu);
}

static uint16_t stack_pull16_linear(mipsnes2_cpu *cpu)
{
    uint16_t initial = cpu->s;
    uint16_t low = read8(cpu, (uint16_t)(initial + UINT16_C(1)));
    uint16_t high = read8(cpu, (uint16_t)(initial + UINT16_C(2)));

    cpu->s = (uint16_t)(initial + UINT16_C(2));
    apply_mode_invariants(cpu);
    return (uint16_t)(low | (uint16_t)(high << 8));
}

static mipsnes2_address direct_data_next_address(const mipsnes2_cpu *cpu,
                                                 mipsnes2_address address)
{
    uint16_t next = (uint16_t)(address + UINT32_C(1));

    if (cpu->emulation && (cpu->d & UINT16_C(0x00ff)) == 0U) {
        next = (uint16_t)((address & UINT32_C(0xff00)) |
                          (next & UINT16_C(0x00ff)));
    }
    return next;
}

static uint16_t read16_direct(const mipsnes2_cpu *cpu,
                              mipsnes2_address address)
{
    uint16_t low = read8(cpu, address);
    uint16_t high = read8(cpu, direct_data_next_address(cpu, address));
    return (uint16_t)(low | (uint16_t)(high << 8));
}

static void write16_direct(const mipsnes2_cpu *cpu,
                           mipsnes2_address address, uint16_t value)
{
    write8(cpu, address, (uint8_t)value);
    write8(cpu, direct_data_next_address(cpu, address),
           (uint8_t)(value >> 8));
}

static mipsnes2_address read24_direct_long_pointer(const mipsnes2_cpu *cpu,
                                                    mipsnes2_address address)
{
    mipsnes2_address low = read8(cpu, address);
    mipsnes2_address high = read8(cpu, (uint16_t)(address + UINT32_C(1)));
    mipsnes2_address bank = read8(cpu, (uint16_t)(address + UINT32_C(2)));

    return low | (high << 8) | (bank << 16);
}

static void set_nz8(mipsnes2_cpu *cpu, uint8_t value)
{
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z);
    if (value == 0U) {
        cpu->p |= MIPSNES2_CPU_FLAG_Z;
    }
    if ((value & UINT8_C(0x80)) != 0U) {
        cpu->p |= MIPSNES2_CPU_FLAG_N;
    }
}

static void set_nz16(mipsnes2_cpu *cpu, uint16_t value)
{
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z);
    if (value == 0U) {
        cpu->p |= MIPSNES2_CPU_FLAG_Z;
    }
    if ((value & UINT16_C(0x8000)) != 0U) {
        cpu->p |= MIPSNES2_CPU_FLAG_N;
    }
}

static uint16_t index_register_value(const mipsnes2_cpu *cpu,
                                     uint16_t value)
{
    return index_is_8_bit(cpu) ? (uint8_t)value : value;
}

static void transfer_to_accumulator(mipsnes2_cpu *cpu, uint16_t value)
{
    if (accumulator_is_8_bit(cpu)) {
        uint8_t low = (uint8_t)value;

        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | low);
        set_nz8(cpu, low);
    } else {
        cpu->a = value;
        set_nz16(cpu, value);
    }
    cpu->instruction_cycles += UINT64_C(2);
}

static void transfer16_to_accumulator(mipsnes2_cpu *cpu, uint16_t value)
{
    cpu->a = value;
    set_nz16(cpu, value);
    cpu->instruction_cycles += UINT64_C(2);
}

static void transfer_to_index(mipsnes2_cpu *cpu, uint16_t *target,
                              uint16_t value)
{
    if (index_is_8_bit(cpu)) {
        uint8_t low = (uint8_t)value;

        *target = low;
        set_nz8(cpu, low);
    } else {
        *target = value;
        set_nz16(cpu, value);
    }
    cpu->instruction_cycles += UINT64_C(2);
}

static void transfer_to_stack(mipsnes2_cpu *cpu, uint16_t value)
{
    cpu->s = cpu->emulation
                 ? (uint16_t)(UINT16_C(0x0100) |
                              (value & UINT16_C(0x00ff)))
                 : value;
    cpu->instruction_cycles += UINT64_C(2);
}

static void exchange_accumulator_bytes(mipsnes2_cpu *cpu)
{
    cpu->a = (uint16_t)((cpu->a >> 8) | (cpu->a << 8));
    set_nz8(cpu, (uint8_t)cpu->a);
    cpu->instruction_cycles += UINT64_C(3);
}

static void change_accumulator(mipsnes2_cpu *cpu, bool increment)
{
    if (accumulator_is_8_bit(cpu)) {
        uint8_t value = (uint8_t)cpu->a;

        value = increment ? (uint8_t)(value + UINT8_C(1))
                          : (uint8_t)(value - UINT8_C(1));
        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | value);
        set_nz8(cpu, value);
    } else {
        cpu->a = increment ? (uint16_t)(cpu->a + UINT16_C(1))
                           : (uint16_t)(cpu->a - UINT16_C(1));
        set_nz16(cpu, cpu->a);
    }
    cpu->instruction_cycles += UINT64_C(2);
}

static void change_index(mipsnes2_cpu *cpu, uint16_t *target,
                         bool increment)
{
    if (index_is_8_bit(cpu)) {
        uint8_t value = (uint8_t)*target;

        value = increment ? (uint8_t)(value + UINT8_C(1))
                          : (uint8_t)(value - UINT8_C(1));
        *target = value;
        set_nz8(cpu, value);
    } else {
        *target = increment ? (uint16_t)(*target + UINT16_C(1))
                            : (uint16_t)(*target - UINT16_C(1));
        set_nz16(cpu, *target);
    }
    cpu->instruction_cycles += UINT64_C(2);
}

typedef enum {
    SHIFT_LEFT,
    ROTATE_LEFT,
    SHIFT_RIGHT,
    ROTATE_RIGHT
} shift_operation;

static uint16_t shift_value(mipsnes2_cpu *cpu, uint16_t value, bool wide,
                            shift_operation operation)
{
    uint16_t mask = wide ? UINT16_MAX : UINT8_MAX;
    uint16_t sign = wide ? UINT16_C(0x8000) : UINT16_C(0x0080);
    bool carry_in = (cpu->p & MIPSNES2_CPU_FLAG_C) != 0U;
    bool carry_out;
    uint16_t result;

    value &= mask;
    switch (operation) {
    case SHIFT_LEFT:
        carry_out = (value & sign) != 0U;
        result = (uint16_t)((value << 1) & mask);
        break;
    case ROTATE_LEFT:
        carry_out = (value & sign) != 0U;
        result = (uint16_t)(((value << 1) |
                             (carry_in ? UINT16_C(1) : UINT16_C(0))) &
                            mask);
        break;
    case SHIFT_RIGHT:
        carry_out = (value & UINT16_C(1)) != 0U;
        result = (uint16_t)(value >> 1);
        break;
    case ROTATE_RIGHT:
        carry_out = (value & UINT16_C(1)) != 0U;
        result = (uint16_t)((value >> 1) | (carry_in ? sign : 0U));
        break;
    default:
        return value;
    }

    cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
    if (carry_out) {
        cpu->p |= MIPSNES2_CPU_FLAG_C;
    }
    if (wide) {
        set_nz16(cpu, result);
    } else {
        set_nz8(cpu, (uint8_t)result);
    }
    return result;
}

static void shift_accumulator(mipsnes2_cpu *cpu, shift_operation operation)
{
    bool wide = !accumulator_is_8_bit(cpu);
    uint16_t result = shift_value(cpu, cpu->a, wide, operation);

    cpu->a = wide ? result
                  : (uint16_t)((cpu->a & UINT16_C(0xff00)) | result);
    cpu->instruction_cycles += UINT64_C(2);
}

typedef enum {
    MEMORY_ASL,
    MEMORY_ROL,
    MEMORY_LSR,
    MEMORY_ROR,
    MEMORY_DEC,
    MEMORY_INC,
    MEMORY_TSB,
    MEMORY_TRB
} memory_modify_operation;

static uint16_t modify_memory_value(mipsnes2_cpu *cpu, uint16_t value,
                                    bool wide,
                                    memory_modify_operation operation)
{
    uint16_t mask = wide ? UINT16_MAX : UINT8_MAX;
    uint16_t result;

    value &= mask;
    switch (operation) {
    case MEMORY_ASL:
        return shift_value(cpu, value, wide, SHIFT_LEFT);
    case MEMORY_ROL:
        return shift_value(cpu, value, wide, ROTATE_LEFT);
    case MEMORY_LSR:
        return shift_value(cpu, value, wide, SHIFT_RIGHT);
    case MEMORY_ROR:
        return shift_value(cpu, value, wide, ROTATE_RIGHT);
    case MEMORY_DEC:
        result = (uint16_t)((value - UINT16_C(1)) & mask);
        break;
    case MEMORY_INC:
        result = (uint16_t)((value + UINT16_C(1)) & mask);
        break;
    case MEMORY_TSB:
    case MEMORY_TRB:
        cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_Z;
        if (((cpu->a & mask) & value) == 0U) {
            cpu->p |= MIPSNES2_CPU_FLAG_Z;
        }
        return operation == MEMORY_TSB
                   ? (uint16_t)((value | cpu->a) & mask)
                   : (uint16_t)((value & (uint16_t)~cpu->a) & mask);
    default:
        return value;
    }

    if (wide) {
        set_nz16(cpu, result);
    } else {
        set_nz8(cpu, (uint8_t)result);
    }
    return result;
}

typedef enum {
    ACCUMULATOR_LOGIC_OR,
    ACCUMULATOR_LOGIC_AND,
    ACCUMULATOR_LOGIC_XOR
} accumulator_logic_operation;

static uint16_t apply_logic(uint16_t left, uint16_t right,
                            accumulator_logic_operation operation)
{
    switch (operation) {
    case ACCUMULATOR_LOGIC_OR:
        return (uint16_t)(left | right);
    case ACCUMULATOR_LOGIC_AND:
        return (uint16_t)(left & right);
    case ACCUMULATOR_LOGIC_XOR:
        return (uint16_t)(left ^ right);
    }
    return 0U;
}

static void logic_accumulator(mipsnes2_cpu *cpu, uint16_t operand,
                              accumulator_logic_operation operation,
                              uint64_t base_cycles)
{
    if (accumulator_is_8_bit(cpu)) {
        uint8_t result = (uint8_t)apply_logic((uint8_t)cpu->a,
                                              (uint8_t)operand,
                                              operation);

        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | result);
        set_nz8(cpu, result);
        cpu->instruction_cycles += base_cycles;
    } else {
        cpu->a = apply_logic(cpu->a, operand, operation);
        set_nz16(cpu, cpu->a);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    }
}

static void compare_value(mipsnes2_cpu *cpu, uint16_t left,
                          uint16_t right, bool wide,
                          uint64_t base_cycles)
{
    cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
    if (wide) {
        uint16_t result = (uint16_t)(left - right);

        if (left >= right) {
            cpu->p |= MIPSNES2_CPU_FLAG_C;
        }
        set_nz16(cpu, result);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    } else {
        uint8_t left8 = (uint8_t)left;
        uint8_t right8 = (uint8_t)right;
        uint8_t result = (uint8_t)(left8 - right8);

        if (left8 >= right8) {
            cpu->p |= MIPSNES2_CPU_FLAG_C;
        }
        set_nz8(cpu, result);
        cpu->instruction_cycles += base_cycles;
    }
}

static void arithmetic_accumulator(mipsnes2_cpu *cpu,
                                   uint16_t operand,
                                   bool subtract,
                                   uint64_t base_cycles)
{
    bool wide = !accumulator_is_8_bit(cpu);
    uint32_t mask = wide ? UINT32_C(0xffff) : UINT32_C(0x00ff);
    uint32_t sign = wide ? UINT32_C(0x8000) : UINT32_C(0x0080);
    uint32_t left = cpu->a & mask;
    uint32_t right = operand & mask;
    uint32_t binary_result;
    uint32_t result;
    bool carry;
    bool overflow;

    if (subtract) {
        uint32_t borrow = (cpu->p & MIPSNES2_CPU_FLAG_C) != 0U ? 0U : 1U;
        uint32_t subtrahend = right + borrow;

        binary_result = (left - subtrahend) & mask;
        carry = left >= subtrahend;
        overflow = ((left ^ right) & (left ^ binary_result) & sign) != 0U;
    } else {
        uint32_t carry_in = (cpu->p & MIPSNES2_CPU_FLAG_C) != 0U ? 1U : 0U;
        uint32_t sum = left + right + carry_in;

        binary_result = sum & mask;
        carry = sum > mask;
        overflow = ((~(left ^ right) & (left ^ binary_result)) & sign) != 0U;
    }

    result = binary_result;
    if ((cpu->p & MIPSNES2_CPU_FLAG_D) != 0U) {
        unsigned digit_count = wide ? 4U : 2U;
        unsigned digit;
        uint32_t decimal_result = 0U;

        if (subtract) {
            int32_t borrow = (cpu->p & MIPSNES2_CPU_FLAG_C) != 0U ? 0 : 1;

            for (digit = 0U; digit < digit_count; ++digit) {
                unsigned shift = digit * 4U;
                int32_t difference =
                    (int32_t)((left >> shift) & UINT32_C(0x0f)) -
                    (int32_t)((right >> shift) & UINT32_C(0x0f)) - borrow;

                if (difference < 0) {
                    difference -= 6;
                    borrow = 1;
                } else {
                    borrow = 0;
                }
                decimal_result |=
                    ((uint32_t)difference & UINT32_C(0x0f)) << shift;
            }
            carry = borrow == 0;
        } else {
            uint32_t decimal_carry =
                (cpu->p & MIPSNES2_CPU_FLAG_C) != 0U ? 1U : 0U;
            uint32_t highest_binary_nibble = 0U;

            for (digit = 0U; digit < digit_count; ++digit) {
                unsigned shift = digit * 4U;
                uint32_t sum = ((left >> shift) & UINT32_C(0x0f)) +
                               ((right >> shift) & UINT32_C(0x0f)) +
                               decimal_carry;

                if (digit + 1U == digit_count) {
                    highest_binary_nibble = sum << shift;
                }
                if (sum > UINT32_C(9)) {
                    sum += UINT32_C(6);
                }
                decimal_result |= (sum & UINT32_C(0x0f)) << shift;
                decimal_carry = sum > UINT32_C(0x0f) ? 1U : 0U;
            }
            carry = decimal_carry != 0U;
            overflow = ((~(left ^ right) &
                         (left ^ highest_binary_nibble)) & sign) != 0U;
        }
        result = decimal_result & mask;
    }

    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_C);
    if (wide) {
        cpu->a = (uint16_t)result;
        set_nz16(cpu, cpu->a);
    } else {
        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) |
                            (uint16_t)result);
        set_nz8(cpu, (uint8_t)result);
    }
    if (overflow) {
        cpu->p |= MIPSNES2_CPU_FLAG_V;
    }
    if (carry) {
        cpu->p |= MIPSNES2_CPU_FLAG_C;
    }
    cpu->instruction_cycles += base_cycles + (wide ? UINT64_C(1)
                                                    : UINT64_C(0));
}

static void test_bits(mipsnes2_cpu *cpu, uint16_t operand,
                      bool immediate, uint64_t base_cycles)
{
    bool wide = !accumulator_is_8_bit(cpu);
    uint16_t mask = wide ? UINT16_MAX : UINT8_MAX;
    uint16_t negative = wide ? UINT16_C(0x8000) : UINT16_C(0x0080);
    uint16_t overflow = wide ? UINT16_C(0x4000) : UINT16_C(0x0040);

    cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_Z;
    if (((cpu->a & mask) & (operand & mask)) == 0U) {
        cpu->p |= MIPSNES2_CPU_FLAG_Z;
    }
    if (!immediate) {
        cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V);
        if ((operand & negative) != 0U) {
            cpu->p |= MIPSNES2_CPU_FLAG_N;
        }
        if ((operand & overflow) != 0U) {
            cpu->p |= MIPSNES2_CPU_FLAG_V;
        }
    }
    cpu->instruction_cycles += base_cycles + (wide ? UINT64_C(1)
                                                    : UINT64_C(0));
}

static int16_t signed_offset8(uint8_t value)
{
    return (value & UINT8_C(0x80)) != 0U
               ? (int16_t)((int16_t)value - INT16_C(0x0100))
               : (int16_t)value;
}

static int32_t signed_offset16(uint16_t value)
{
    return (value & UINT16_C(0x8000)) != 0U
               ? (int32_t)value - INT32_C(0x00010000)
               : (int32_t)value;
}

static void branch_relative8(mipsnes2_cpu *cpu, bool condition)
{
    uint8_t operand = fetch8(cpu);
    uint16_t base_pc = cpu->pc;

    cpu->instruction_cycles += UINT64_C(2);
    if (!condition) {
        return;
    }

    cpu->pc = (uint16_t)((int32_t)base_pc + signed_offset8(operand));
    cpu->instruction_cycles += UINT64_C(1);
    if (cpu->emulation && (base_pc & UINT16_C(0xff00)) !=
                              (cpu->pc & UINT16_C(0xff00))) {
        cpu->instruction_cycles += UINT64_C(1);
    }
}

static void branch_relative16(mipsnes2_cpu *cpu)
{
    uint16_t operand = fetch16(cpu);

    cpu->pc = (uint16_t)((int32_t)cpu->pc + signed_offset16(operand));
    cpu->instruction_cycles += UINT64_C(4);
}

static void load_accumulator_immediate(mipsnes2_cpu *cpu)
{
    if (accumulator_is_8_bit(cpu)) {
        uint8_t value = fetch8(cpu);
        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | value);
        set_nz8(cpu, value);
        cpu->instruction_cycles += UINT64_C(2);
    } else {
        cpu->a = fetch16(cpu);
        set_nz16(cpu, cpu->a);
        cpu->instruction_cycles += UINT64_C(3);
    }
}

static void load_index_immediate(mipsnes2_cpu *cpu, uint16_t *target)
{
    if (index_is_8_bit(cpu)) {
        uint8_t value = fetch8(cpu);
        *target = value;
        set_nz8(cpu, value);
        cpu->instruction_cycles += UINT64_C(2);
    } else {
        *target = fetch16(cpu);
        set_nz16(cpu, *target);
        cpu->instruction_cycles += UINT64_C(3);
    }
}

static void load_accumulator_memory(mipsnes2_cpu *cpu,
                                    mipsnes2_address address,
                                    uint64_t base_cycles)
{
    if (accumulator_is_8_bit(cpu)) {
        uint8_t value = read8(cpu, address);
        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | value);
        set_nz8(cpu, value);
        cpu->instruction_cycles += base_cycles;
    } else {
        cpu->a = read16_linear(cpu, address);
        set_nz16(cpu, cpu->a);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    }
}

static void load_index_memory(mipsnes2_cpu *cpu, uint16_t *target,
                              mipsnes2_address address,
                              uint64_t base_cycles)
{
    if (index_is_8_bit(cpu)) {
        uint8_t value = read8(cpu, address);
        *target = value;
        set_nz8(cpu, value);
        cpu->instruction_cycles += base_cycles;
    } else {
        *target = read16_linear(cpu, address);
        set_nz16(cpu, *target);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    }
}

static void store_memory(mipsnes2_cpu *cpu, mipsnes2_address address,
                         uint16_t value, bool wide, uint64_t base_cycles)
{
    if (wide) {
        write16_linear(cpu, address, value);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    } else {
        write8(cpu, address, (uint8_t)value);
        cpu->instruction_cycles += base_cycles;
    }
}

static void load_accumulator_direct(mipsnes2_cpu *cpu,
                                    mipsnes2_address address,
                                    uint64_t base_cycles)
{
    if (accumulator_is_8_bit(cpu)) {
        uint8_t value = read8(cpu, address);
        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | value);
        set_nz8(cpu, value);
        cpu->instruction_cycles += base_cycles;
    } else {
        cpu->a = read16_direct(cpu, address);
        set_nz16(cpu, cpu->a);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    }
}

static void load_index_direct(mipsnes2_cpu *cpu, uint16_t *target,
                              mipsnes2_address address,
                              uint64_t base_cycles)
{
    if (index_is_8_bit(cpu)) {
        uint8_t value = read8(cpu, address);
        *target = value;
        set_nz8(cpu, value);
        cpu->instruction_cycles += base_cycles;
    } else {
        *target = read16_direct(cpu, address);
        set_nz16(cpu, *target);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    }
}

static void store_direct(mipsnes2_cpu *cpu, mipsnes2_address address,
                         uint16_t value, bool wide, uint64_t base_cycles)
{
    if (wide) {
        write16_direct(cpu, address, value);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    } else {
        write8(cpu, address, (uint8_t)value);
        cpu->instruction_cycles += base_cycles;
    }
}

static uint16_t fetch_accumulator_operand(mipsnes2_cpu *cpu)
{
    return accumulator_is_8_bit(cpu) ? fetch8(cpu) : fetch16(cpu);
}

static uint16_t read_accumulator_operand(const mipsnes2_cpu *cpu,
                                         mipsnes2_address address,
                                         bool direct)
{
    if (accumulator_is_8_bit(cpu)) {
        return read8(cpu, address);
    }
    return direct ? read16_direct(cpu, address)
                  : read16_linear(cpu, address);
}

static void modify_memory(mipsnes2_cpu *cpu, mipsnes2_address address,
                          bool direct, memory_modify_operation operation,
                          uint64_t base_cycles)
{
    bool wide = !accumulator_is_8_bit(cpu);
    uint16_t value = read_accumulator_operand(cpu, address, direct);
    uint16_t result = modify_memory_value(cpu, value, wide, operation);

    if (wide) {
        if (direct) {
            write16_direct(cpu, address, result);
        } else {
            write16_linear(cpu, address, result);
        }
    } else {
        write8(cpu, address, (uint8_t)result);
    }
    cpu->instruction_cycles += base_cycles + (wide ? UINT64_C(2)
                                                    : UINT64_C(0));
}

static uint16_t read_accumulator_stack_operand(const mipsnes2_cpu *cpu,
                                               uint16_t address)
{
    return accumulator_is_8_bit(cpu) ? read8(cpu, address)
                                     : read16_bank_zero(cpu, address);
}

static void load_accumulator_stack(mipsnes2_cpu *cpu, uint16_t address,
                                   uint64_t base_cycles)
{
    if (accumulator_is_8_bit(cpu)) {
        uint8_t value = read8(cpu, address);
        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | value);
        set_nz8(cpu, value);
        cpu->instruction_cycles += base_cycles;
    } else {
        cpu->a = read16_bank_zero(cpu, address);
        set_nz16(cpu, cpu->a);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    }
}

static void store_stack(mipsnes2_cpu *cpu, uint16_t address, uint16_t value,
                        bool wide, uint64_t base_cycles)
{
    if (wide) {
        write16_bank_zero(cpu, address, value);
        cpu->instruction_cycles += base_cycles + UINT64_C(1);
    } else {
        write8(cpu, address, (uint8_t)value);
        cpu->instruction_cycles += base_cycles;
    }
}

static uint16_t fetch_index_operand(mipsnes2_cpu *cpu)
{
    return index_is_8_bit(cpu) ? fetch8(cpu) : fetch16(cpu);
}

static uint16_t read_index_operand(const mipsnes2_cpu *cpu,
                                   mipsnes2_address address,
                                   bool direct)
{
    if (index_is_8_bit(cpu)) {
        return read8(cpu, address);
    }
    return direct ? read16_direct(cpu, address)
                  : read16_linear(cpu, address);
}

static mipsnes2_address absolute_address(mipsnes2_cpu *cpu)
{
    return mipsnes2_address_make(cpu->dbr, fetch16(cpu));
}

static mipsnes2_address absolute_indexed_address(mipsnes2_cpu *cpu,
                                                 uint16_t index,
                                                 bool write,
                                                 uint64_t *base_cycles)
{
    uint16_t base = fetch16(cpu);
    bool page_crossed = (uint32_t)(base & UINT16_C(0x00ff)) + index >
                        UINT32_C(0x00ff);
    mipsnes2_address address = mipsnes2_address_make(cpu->dbr, base);

    *base_cycles = UINT64_C(4);
    if (write || !index_is_8_bit(cpu) || page_crossed) {
        *base_cycles += UINT64_C(1);
    }
    return mipsnes2_address24(address + index);
}

static mipsnes2_address absolute_long_indexed_address(mipsnes2_cpu *cpu,
                                                      uint16_t index)
{
    return mipsnes2_address24(fetch24(cpu) + index);
}

static mipsnes2_address direct_address(mipsnes2_cpu *cpu, uint16_t index)
{
    uint16_t offset = fetch8(cpu);
    uint16_t address = (uint16_t)(cpu->d + offset + index);

    if (cpu->emulation && (cpu->d & UINT16_C(0x00ff)) == 0U) {
        address = (uint16_t)((cpu->d & UINT16_C(0xff00)) |
                             ((offset + index) & UINT16_C(0x00ff)));
    }
    return address;
}

static mipsnes2_address direct_indirect_address(mipsnes2_cpu *cpu,
                                                uint16_t pre_index,
                                                uint16_t post_index,
                                                uint16_t *pointer_value)
{
    uint16_t pointer = read16_direct(cpu, direct_address(cpu, pre_index));

    if (pointer_value != NULL) {
        *pointer_value = pointer;
    }
    return mipsnes2_address24(mipsnes2_address_make(cpu->dbr, pointer) +
                              post_index);
}

static mipsnes2_address direct_indirect_long_address(mipsnes2_cpu *cpu,
                                                     uint16_t post_index)
{
    mipsnes2_address pointer = read24_direct_long_pointer(
        cpu, direct_address(cpu, 0U));

    return mipsnes2_address24(pointer + post_index);
}

static uint16_t stack_relative_address(mipsnes2_cpu *cpu)
{
    uint16_t offset = fetch8(cpu);

    return (uint16_t)(cpu->s + offset);
}

static mipsnes2_address stack_relative_indirect_indexed_address(
    mipsnes2_cpu *cpu)
{
    uint16_t pointer = read16_bank_zero(cpu, stack_relative_address(cpu));
    mipsnes2_address base = mipsnes2_address_make(cpu->dbr, pointer);

    return mipsnes2_address24(base + cpu->y);
}

static uint64_t direct_penalized_cycles(const mipsnes2_cpu *cpu,
                                        uint64_t base_cycles)
{
    if ((cpu->d & UINT16_C(0x00ff)) != 0U) {
        base_cycles += UINT64_C(1);
    }
    return base_cycles;
}

static uint64_t direct_base_cycles(const mipsnes2_cpu *cpu, bool indexed)
{
    return direct_penalized_cycles(cpu,
                                    indexed ? UINT64_C(4) : UINT64_C(3));
}

typedef enum {
    INTERRUPT_NONE = 0,
    INTERRUPT_IRQ,
    INTERRUPT_NMI,
    INTERRUPT_ABORT,
    INTERRUPT_BRK,
    INTERRUPT_COP
} interrupt_kind;

static uint16_t interrupt_vector(const mipsnes2_cpu *cpu,
                                 interrupt_kind kind)
{
    if (cpu->emulation) {
        switch (kind) {
        case INTERRUPT_COP:
            return UINT16_C(0xfff4);
        case INTERRUPT_ABORT:
            return UINT16_C(0xfff8);
        case INTERRUPT_NMI:
            return UINT16_C(0xfffa);
        case INTERRUPT_IRQ:
        case INTERRUPT_BRK:
            return UINT16_C(0xfffe);
        case INTERRUPT_NONE:
            break;
        }
    }

    switch (kind) {
    case INTERRUPT_COP:
        return UINT16_C(0xffe4);
    case INTERRUPT_BRK:
        return UINT16_C(0xffe6);
    case INTERRUPT_ABORT:
        return UINT16_C(0xffe8);
    case INTERRUPT_NMI:
        return UINT16_C(0xffea);
    case INTERRUPT_IRQ:
        return UINT16_C(0xffee);
    case INTERRUPT_NONE:
        break;
    }
    return UINT16_C(0xfffe);
}

static mipsnes2_cpu_result enter_interrupt(mipsnes2_cpu *cpu,
                                           interrupt_kind kind)
{
    uint16_t vector;
    uint8_t pushed_status = cpu->p;

    if (!require_write_bus(cpu)) {
        return cpu->stop_reason;
    }

    if (!cpu->emulation) {
        stack_push8(cpu, cpu->pbr);
    } else if (kind == INTERRUPT_BRK) {
        pushed_status |= MIPSNES2_CPU_FLAG_X;
    } else {
        pushed_status &= (uint8_t)~MIPSNES2_CPU_FLAG_X;
    }
    stack_push16(cpu, cpu->pc);
    stack_push8(cpu, pushed_status);

    cpu->p |= MIPSNES2_CPU_FLAG_I;
    cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_D;
    cpu->pbr = 0U;
    vector = interrupt_vector(cpu, kind);
    cpu->pc = read16_bank_zero(cpu, vector);
    cpu->instruction_cycles += cpu->emulation ? UINT64_C(7)
                                               : UINT64_C(8);
    return MIPSNES2_CPU_OK;
}

static void block_move(mipsnes2_cpu *cpu, bool decrement)
{
    uint8_t destination_bank = fetch8(cpu);
    uint8_t source_bank = fetch8(cpu);
    uint8_t value = read8(cpu, mipsnes2_address_make(source_bank, cpu->x));

    write8(cpu, mipsnes2_address_make(destination_bank, cpu->y), value);
    cpu->dbr = destination_bank;
    cpu->a = (uint16_t)(cpu->a - UINT16_C(1));

    if (index_is_8_bit(cpu)) {
        uint8_t delta = decrement ? UINT8_C(0xff) : UINT8_C(1);

        cpu->x = (uint8_t)((uint8_t)cpu->x + delta);
        cpu->y = (uint8_t)((uint8_t)cpu->y + delta);
    } else if (decrement) {
        cpu->x = (uint16_t)(cpu->x - UINT16_C(1));
        cpu->y = (uint16_t)(cpu->y - UINT16_C(1));
    } else {
        cpu->x = (uint16_t)(cpu->x + UINT16_C(1));
        cpu->y = (uint16_t)(cpu->y + UINT16_C(1));
    }

    if (cpu->a != UINT16_C(0xffff)) {
        cpu->pc = (uint16_t)(cpu->pc - UINT16_C(3));
    }
    cpu->instruction_cycles += UINT64_C(7);
}

void mipsnes2_cpu_init(mipsnes2_cpu *cpu, mipsnes2_bus_interface bus)
{
    if (cpu == NULL) {
        return;
    }
    (void)memset(cpu, 0, sizeof(*cpu));
    cpu->bus = bus;
    cpu->stopped = true;
    cpu->stop_reason = MIPSNES2_CPU_NOT_READY;
}

mipsnes2_cpu_result mipsnes2_cpu_reset(mipsnes2_cpu *cpu)
{
    uint16_t low;
    uint16_t high;

    if (cpu == NULL || cpu->bus.read8 == NULL) {
        return MIPSNES2_CPU_INVALID_BUS;
    }

    cpu->emulation = true;
    cpu->p = (uint8_t)((cpu->p & (MIPSNES2_CPU_FLAG_N |
                                  MIPSNES2_CPU_FLAG_V |
                                  MIPSNES2_CPU_FLAG_Z |
                                  MIPSNES2_CPU_FLAG_C)) |
                       MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
                       MIPSNES2_CPU_FLAG_I);
    cpu->d = 0U;
    cpu->dbr = 0U;
    cpu->pbr = 0U;
    apply_mode_invariants(cpu);

    low = read8(cpu, UINT32_C(0x00fffc));
    high = read8(cpu, UINT32_C(0x00fffd));
    cpu->pc = (uint16_t)(low | (uint16_t)(high << 8));
    cpu->instruction_cycles = 0U;
    cpu->waiting = false;
    cpu->clock_stopped = false;
    cpu->abort_pending = false;
    cpu->nmi_pending = false;
    cpu->stopped = false;
    cpu->stop_reason = MIPSNES2_CPU_OK;
    cpu->last_opcode = 0U;
    cpu->last_opcode_address = mipsnes2_address_make(cpu->pbr, cpu->pc);
    return MIPSNES2_CPU_OK;
}

void mipsnes2_cpu_set_irq(mipsnes2_cpu *cpu, bool asserted)
{
    if (cpu != NULL) {
        cpu->irq_level = asserted;
    }
}

void mipsnes2_cpu_pulse_abort(mipsnes2_cpu *cpu)
{
    if (cpu != NULL) {
        cpu->abort_pending = true;
    }
}

void mipsnes2_cpu_pulse_nmi(mipsnes2_cpu *cpu)
{
    if (cpu != NULL) {
        cpu->nmi_pending = true;
    }
}

static mipsnes2_cpu_result execute_fetched_opcode(mipsnes2_cpu *cpu)
{
    uint8_t operand;
    mipsnes2_address address;
    uint64_t base_cycles;

    switch (cpu->last_opcode) {
    case 0x00: /* BRK signature */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        (void)fetch8(cpu);
        return enter_interrupt(cpu, INTERRUPT_BRK);
    case 0x01: /* ORA (direct,X) */
        address = direct_indirect_address(cpu, cpu->x, 0U, NULL);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x02: /* COP signature */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        (void)fetch8(cpu);
        return enter_interrupt(cpu, INTERRUPT_COP);
    case 0x03: /* ORA stack-relative */
        address = stack_relative_address(cpu);
        logic_accumulator(
            cpu, read_accumulator_stack_operand(cpu, (uint16_t)address),
            ACCUMULATOR_LOGIC_OR, UINT64_C(4));
        break;
    case 0x04: /* TSB direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        modify_memory(cpu, address, true, MEMORY_TSB,
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x05: /* ORA direct */
        address = direct_address(cpu, 0U);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, true),
                          ACCUMULATOR_LOGIC_OR,
                          direct_base_cycles(cpu, false));
        break;
    case 0x06: /* ASL direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        modify_memory(cpu, address, true, MEMORY_ASL,
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x07: /* ORA [direct] */
        address = direct_indirect_long_address(cpu, 0U);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x08: /* PHP */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        stack_push8(cpu, cpu->p);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0x09: /* ORA immediate */
        logic_accumulator(cpu, fetch_accumulator_operand(cpu),
                          ACCUMULATOR_LOGIC_OR, UINT64_C(2));
        break;
    case 0x0a: /* ASL accumulator */
        shift_accumulator(cpu, SHIFT_LEFT);
        break;
    case 0x0b: /* PHD */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        stack_push16_linear(cpu, cpu->d);
        cpu->instruction_cycles += UINT64_C(4);
        break;
    case 0x0c: /* TSB absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_address(cpu);
        modify_memory(cpu, address, false, MEMORY_TSB, UINT64_C(6));
        break;
    case 0x0d: /* ORA absolute */
        address = absolute_address(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR, UINT64_C(4));
        break;
    case 0x0e: /* ASL absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_address(cpu);
        modify_memory(cpu, address, false, MEMORY_ASL, UINT64_C(6));
        break;
    case 0x0f: /* ORA absolute long */
        address = fetch24(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR, UINT64_C(5));
        break;
    case 0x10: /* BPL */
        branch_relative8(cpu, (cpu->p & MIPSNES2_CPU_FLAG_N) == 0U);
        break;
    case 0x11: /* ORA (direct),Y */
    {
        uint16_t pointer;

        address = direct_indirect_address(cpu, 0U, cpu->y, &pointer);
        base_cycles = direct_penalized_cycles(cpu, UINT64_C(5));
        if (!index_is_8_bit(cpu) ||
            (uint32_t)(pointer & UINT16_C(0x00ff)) + cpu->y >
                UINT32_C(0x00ff)) {
            base_cycles += UINT64_C(1);
        }
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR, base_cycles);
        break;
    }
    case 0x12: /* ORA (direct) */
        address = direct_indirect_address(cpu, 0U, 0U, NULL);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR,
                          direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x13: /* ORA (stack-relative),Y */
        address = stack_relative_indirect_indexed_address(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR, UINT64_C(7));
        break;
    case 0x14: /* TRB direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        modify_memory(cpu, address, true, MEMORY_TRB,
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x15: /* ORA direct,X */
        address = direct_address(cpu, cpu->x);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, true),
                          ACCUMULATOR_LOGIC_OR,
                          direct_base_cycles(cpu, true));
        break;
    case 0x16: /* ASL direct,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        modify_memory(cpu, address, true, MEMORY_ASL,
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x17: /* ORA [direct],Y */
        address = direct_indirect_long_address(cpu, cpu->y);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x18: /* CLC */
        cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x19: /* ORA absolute,Y */
        address = absolute_indexed_address(cpu, cpu->y, false,
                                           &base_cycles);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR, base_cycles);
        break;
    case 0x1a: /* INC accumulator */
        change_accumulator(cpu, true);
        break;
    case 0x1b: /* TCS */
        transfer_to_stack(cpu, cpu->a);
        break;
    case 0x1c: /* TRB absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_address(cpu);
        modify_memory(cpu, address, false, MEMORY_TRB, UINT64_C(6));
        break;
    case 0x1d: /* ORA absolute,X */
        address = absolute_indexed_address(cpu, cpu->x, false,
                                           &base_cycles);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR, base_cycles);
        break;
    case 0x1e: /* ASL absolute,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->x, true,
                                           &base_cycles);
        modify_memory(cpu, address, false, MEMORY_ASL, UINT64_C(7));
        break;
    case 0x1f: /* ORA absolute long,X */
        address = absolute_long_indexed_address(cpu, cpu->x);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_OR, UINT64_C(5));
        break;
    case 0x20: { /* JSR absolute */
        uint16_t target;
        uint16_t return_address;

        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        target = fetch16(cpu);
        return_address = (uint16_t)(cpu->pc - UINT16_C(1));
        stack_push16(cpu, return_address);
        cpu->pc = target;
        cpu->instruction_cycles += UINT64_C(6);
        break;
    }
    case 0x21: /* AND (direct,X) */
        address = direct_indirect_address(cpu, cpu->x, 0U, NULL);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x22: { /* JSL absolute long */
        uint16_t target_low;
        uint16_t return_address;
        uint16_t initial_stack;
        uint8_t target_bank;

        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        target_low = fetch8(cpu);
        target_low |= (uint16_t)((uint16_t)fetch8(cpu) << 8);
        return_address = cpu->pc;
        initial_stack = cpu->s;
        write8(cpu, initial_stack, cpu->pbr);
        target_bank = fetch8(cpu);
        write8(cpu, (uint16_t)(initial_stack - UINT16_C(1)),
               (uint8_t)(return_address >> 8));
        write8(cpu, (uint16_t)(initial_stack - UINT16_C(2)),
               (uint8_t)return_address);
        cpu->s = (uint16_t)(initial_stack - UINT16_C(3));
        apply_mode_invariants(cpu);
        cpu->pc = target_low;
        cpu->pbr = target_bank;
        cpu->instruction_cycles += UINT64_C(8);
        break;
    }
    case 0x23: /* AND stack-relative */
        address = stack_relative_address(cpu);
        logic_accumulator(
            cpu, read_accumulator_stack_operand(cpu, (uint16_t)address),
            ACCUMULATOR_LOGIC_AND, UINT64_C(4));
        break;
    case 0x24: /* BIT direct */
        address = direct_address(cpu, 0U);
        test_bits(cpu, read_accumulator_operand(cpu, address, true), false,
                  direct_base_cycles(cpu, false));
        break;
    case 0x25: /* AND direct */
        address = direct_address(cpu, 0U);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, true),
                          ACCUMULATOR_LOGIC_AND,
                          direct_base_cycles(cpu, false));
        break;
    case 0x26: /* ROL direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        modify_memory(cpu, address, true, MEMORY_ROL,
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x27: /* AND [direct] */
        address = direct_indirect_long_address(cpu, 0U);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x28: /* PLP */
        cpu->p = stack_pull8(cpu);
        apply_mode_invariants(cpu);
        cpu->instruction_cycles += UINT64_C(4);
        break;
    case 0x29: /* AND immediate */
        logic_accumulator(cpu, fetch_accumulator_operand(cpu),
                          ACCUMULATOR_LOGIC_AND, UINT64_C(2));
        break;
    case 0x2a: /* ROL accumulator */
        shift_accumulator(cpu, ROTATE_LEFT);
        break;
    case 0x2b: /* PLD */
        cpu->d = stack_pull16_linear(cpu);
        set_nz16(cpu, cpu->d);
        cpu->instruction_cycles += UINT64_C(5);
        break;
    case 0x2c: /* BIT absolute */
        address = absolute_address(cpu);
        test_bits(cpu, read_accumulator_operand(cpu, address, false), false,
                  UINT64_C(4));
        break;
    case 0x2d: /* AND absolute */
        address = absolute_address(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND, UINT64_C(4));
        break;
    case 0x2e: /* ROL absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_address(cpu);
        modify_memory(cpu, address, false, MEMORY_ROL, UINT64_C(6));
        break;
    case 0x2f: /* AND absolute long */
        address = fetch24(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND, UINT64_C(5));
        break;
    case 0x30: /* BMI */
        branch_relative8(cpu, (cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
        break;
    case 0x31: /* AND (direct),Y */
    {
        uint16_t pointer;

        address = direct_indirect_address(cpu, 0U, cpu->y, &pointer);
        base_cycles = direct_penalized_cycles(cpu, UINT64_C(5));
        if (!index_is_8_bit(cpu) ||
            (uint32_t)(pointer & UINT16_C(0x00ff)) + cpu->y >
                UINT32_C(0x00ff)) {
            base_cycles += UINT64_C(1);
        }
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND, base_cycles);
        break;
    }
    case 0x32: /* AND (direct) */
        address = direct_indirect_address(cpu, 0U, 0U, NULL);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND,
                          direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x33: /* AND (stack-relative),Y */
        address = stack_relative_indirect_indexed_address(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND, UINT64_C(7));
        break;
    case 0x34: /* BIT direct,X */
        address = direct_address(cpu, cpu->x);
        test_bits(cpu, read_accumulator_operand(cpu, address, true), false,
                  direct_base_cycles(cpu, true));
        break;
    case 0x35: /* AND direct,X */
        address = direct_address(cpu, cpu->x);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, true),
                          ACCUMULATOR_LOGIC_AND,
                          direct_base_cycles(cpu, true));
        break;
    case 0x36: /* ROL direct,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        modify_memory(cpu, address, true, MEMORY_ROL,
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x37: /* AND [direct],Y */
        address = direct_indirect_long_address(cpu, cpu->y);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x38: /* SEC */
        cpu->p |= MIPSNES2_CPU_FLAG_C;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x39: /* AND absolute,Y */
        address = absolute_indexed_address(cpu, cpu->y, false,
                                           &base_cycles);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND, base_cycles);
        break;
    case 0x3a: /* DEC accumulator */
        change_accumulator(cpu, false);
        break;
    case 0x3b: /* TSC */
        transfer16_to_accumulator(cpu, cpu->s);
        break;
    case 0x3c: /* BIT absolute,X */
        address = absolute_indexed_address(cpu, cpu->x, false,
                                           &base_cycles);
        test_bits(cpu, read_accumulator_operand(cpu, address, false), false,
                  base_cycles);
        break;
    case 0x3d: /* AND absolute,X */
        address = absolute_indexed_address(cpu, cpu->x, false,
                                           &base_cycles);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND, base_cycles);
        break;
    case 0x3e: /* ROL absolute,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->x, true,
                                           &base_cycles);
        modify_memory(cpu, address, false, MEMORY_ROL, UINT64_C(7));
        break;
    case 0x3f: /* AND absolute long,X */
        address = absolute_long_indexed_address(cpu, cpu->x);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_AND, UINT64_C(5));
        break;
    case 0x40: /* RTI */
        cpu->p = stack_pull8(cpu);
        apply_mode_invariants(cpu);
        cpu->pc = stack_pull16(cpu);
        if (!cpu->emulation) {
            cpu->pbr = stack_pull8(cpu);
        }
        cpu->instruction_cycles += cpu->emulation ? UINT64_C(6)
                                                   : UINT64_C(7);
        break;
    case 0x41: /* EOR (direct,X) */
        address = direct_indirect_address(cpu, cpu->x, 0U, NULL);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x42: /* WDM signature */
        (void)fetch8(cpu);
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x43: /* EOR stack-relative */
        address = stack_relative_address(cpu);
        logic_accumulator(
            cpu, read_accumulator_stack_operand(cpu, (uint16_t)address),
            ACCUMULATOR_LOGIC_XOR, UINT64_C(4));
        break;
    case 0x44: /* MVP destination bank, source bank */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        block_move(cpu, true);
        break;
    case 0x45: /* EOR direct */
        address = direct_address(cpu, 0U);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, true),
                          ACCUMULATOR_LOGIC_XOR,
                          direct_base_cycles(cpu, false));
        break;
    case 0x46: /* LSR direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        modify_memory(cpu, address, true, MEMORY_LSR,
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x47: /* EOR [direct] */
        address = direct_indirect_long_address(cpu, 0U);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x48: /* PHA */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        if (accumulator_is_8_bit(cpu)) {
            stack_push8(cpu, (uint8_t)cpu->a);
            cpu->instruction_cycles += UINT64_C(3);
        } else {
            stack_push16(cpu, cpu->a);
            cpu->instruction_cycles += UINT64_C(4);
        }
        break;
    case 0x49: /* EOR immediate */
        logic_accumulator(cpu, fetch_accumulator_operand(cpu),
                          ACCUMULATOR_LOGIC_XOR, UINT64_C(2));
        break;
    case 0x4a: /* LSR accumulator */
        shift_accumulator(cpu, SHIFT_RIGHT);
        break;
    case 0x4b: /* PHK */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        stack_push8(cpu, cpu->pbr);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0x4c: /* JMP absolute */
        cpu->pc = fetch16(cpu);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0x4d: /* EOR absolute */
        address = absolute_address(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR, UINT64_C(4));
        break;
    case 0x4e: /* LSR absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_address(cpu);
        modify_memory(cpu, address, false, MEMORY_LSR, UINT64_C(6));
        break;
    case 0x4f: /* EOR absolute long */
        address = fetch24(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR, UINT64_C(5));
        break;
    case 0x50: /* BVC */
        branch_relative8(cpu, (cpu->p & MIPSNES2_CPU_FLAG_V) == 0U);
        break;
    case 0x51: /* EOR (direct),Y */
    {
        uint16_t pointer;

        address = direct_indirect_address(cpu, 0U, cpu->y, &pointer);
        base_cycles = direct_penalized_cycles(cpu, UINT64_C(5));
        if (!index_is_8_bit(cpu) ||
            (uint32_t)(pointer & UINT16_C(0x00ff)) + cpu->y >
                UINT32_C(0x00ff)) {
            base_cycles += UINT64_C(1);
        }
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR, base_cycles);
        break;
    }
    case 0x52: /* EOR (direct) */
        address = direct_indirect_address(cpu, 0U, 0U, NULL);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR,
                          direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x53: /* EOR (stack-relative),Y */
        address = stack_relative_indirect_indexed_address(cpu);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR, UINT64_C(7));
        break;
    case 0x54: /* MVN destination bank, source bank */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        block_move(cpu, false);
        break;
    case 0x55: /* EOR direct,X */
        address = direct_address(cpu, cpu->x);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, true),
                          ACCUMULATOR_LOGIC_XOR,
                          direct_base_cycles(cpu, true));
        break;
    case 0x56: /* LSR direct,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        modify_memory(cpu, address, true, MEMORY_LSR,
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x57: /* EOR [direct],Y */
        address = direct_indirect_long_address(cpu, cpu->y);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR,
                          direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x58: /* CLI */
        cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_I;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x59: /* EOR absolute,Y */
        address = absolute_indexed_address(cpu, cpu->y, false,
                                           &base_cycles);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR, base_cycles);
        break;
    case 0x5a: /* PHY */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        if (index_is_8_bit(cpu)) {
            stack_push8(cpu, (uint8_t)cpu->y);
            cpu->instruction_cycles += UINT64_C(3);
        } else {
            stack_push16(cpu, cpu->y);
            cpu->instruction_cycles += UINT64_C(4);
        }
        break;
    case 0x5b: /* TCD */
        cpu->d = cpu->a;
        set_nz16(cpu, cpu->d);
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x5c: { /* JMP absolute long */
        mipsnes2_address target = fetch24(cpu);

        cpu->pc = (uint16_t)target;
        cpu->pbr = (uint8_t)(target >> 16);
        cpu->instruction_cycles += UINT64_C(4);
        break;
    }
    case 0x5d: /* EOR absolute,X */
        address = absolute_indexed_address(cpu, cpu->x, false,
                                           &base_cycles);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR, base_cycles);
        break;
    case 0x5e: /* LSR absolute,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->x, true,
                                           &base_cycles);
        modify_memory(cpu, address, false, MEMORY_LSR, UINT64_C(7));
        break;
    case 0x5f: /* EOR absolute long,X */
        address = absolute_long_indexed_address(cpu, cpu->x);
        logic_accumulator(cpu,
                          read_accumulator_operand(cpu, address, false),
                          ACCUMULATOR_LOGIC_XOR, UINT64_C(5));
        break;
    case 0x60: /* RTS */
        cpu->pc = (uint16_t)(stack_pull16(cpu) + UINT16_C(1));
        cpu->instruction_cycles += UINT64_C(6);
        break;
    case 0x61: /* ADC (direct,X), binary/decimal modes */
        address = direct_indirect_address(cpu, cpu->x, 0U, NULL);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x62: { /* PER relative long */
        uint16_t relative;
        uint16_t target;

        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        relative = fetch16(cpu);
        target = (uint16_t)((int32_t)cpu->pc + signed_offset16(relative));
        stack_push16_linear(cpu, target);
        cpu->instruction_cycles += UINT64_C(6);
        break;
    }
    case 0x63: /* ADC stack-relative, binary/decimal modes */
        address = stack_relative_address(cpu);
        arithmetic_accumulator(
            cpu, read_accumulator_stack_operand(cpu, (uint16_t)address),
            false, UINT64_C(4));
        break;
    case 0x64: /* STZ direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        store_direct(cpu, address, 0U, !accumulator_is_8_bit(cpu),
                     direct_base_cycles(cpu, false));
        break;
    case 0x65: /* ADC direct, binary/decimal modes */
        address = direct_address(cpu, 0U);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, true), false,
            direct_base_cycles(cpu, false));
        break;
    case 0x66: /* ROR direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        modify_memory(cpu, address, true, MEMORY_ROR,
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x67: /* ADC [direct], binary/decimal modes */
        address = direct_indirect_long_address(cpu, 0U);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x68: /* PLA */
        if (accumulator_is_8_bit(cpu)) {
            uint8_t value = stack_pull8(cpu);

            cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | value);
            set_nz8(cpu, value);
            cpu->instruction_cycles += UINT64_C(4);
        } else {
            cpu->a = stack_pull16(cpu);
            set_nz16(cpu, cpu->a);
            cpu->instruction_cycles += UINT64_C(5);
        }
        break;
    case 0x69: /* ADC immediate, binary/decimal modes */
        arithmetic_accumulator(cpu, fetch_accumulator_operand(cpu),
                                      false, UINT64_C(2));
        break;
    case 0x6a: /* ROR accumulator */
        shift_accumulator(cpu, ROTATE_RIGHT);
        break;
    case 0x6b: { /* RTL */
        uint16_t initial = cpu->s;
        uint16_t low = read8(cpu, (uint16_t)(initial + UINT16_C(1)));
        uint16_t high = read8(cpu, (uint16_t)(initial + UINT16_C(2)));

        cpu->pbr = read8(cpu, (uint16_t)(initial + UINT16_C(3)));
        cpu->s = (uint16_t)(initial + UINT16_C(3));
        apply_mode_invariants(cpu);
        cpu->pc = (uint16_t)((low | (uint16_t)(high << 8)) +
                             UINT16_C(1));
        cpu->instruction_cycles += UINT64_C(6);
        break;
    }
    case 0x6c: /* JMP absolute indirect */
        cpu->pc = read16_bank_zero(cpu, fetch16(cpu));
        cpu->instruction_cycles += UINT64_C(5);
        break;
    case 0x6d: /* ADC absolute, binary/decimal modes */
        address = absolute_address(cpu);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            UINT64_C(4));
        break;
    case 0x6e: /* ROR absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_address(cpu);
        modify_memory(cpu, address, false, MEMORY_ROR, UINT64_C(6));
        break;
    case 0x6f: /* ADC absolute long, binary/decimal modes */
        address = fetch24(cpu);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            UINT64_C(5));
        break;
    case 0x70: /* BVS */
        branch_relative8(cpu, (cpu->p & MIPSNES2_CPU_FLAG_V) != 0U);
        break;
    case 0x71: /* ADC (direct),Y, binary/decimal modes */
    {
        uint16_t pointer;

        address = direct_indirect_address(cpu, 0U, cpu->y, &pointer);
        base_cycles = direct_penalized_cycles(cpu, UINT64_C(5));
        if (!index_is_8_bit(cpu) ||
            (uint32_t)(pointer & UINT16_C(0x00ff)) + cpu->y >
                UINT32_C(0x00ff)) {
            base_cycles += UINT64_C(1);
        }
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            base_cycles);
        break;
    }
    case 0x72: /* ADC (direct), binary/decimal modes */
        address = direct_indirect_address(cpu, 0U, 0U, NULL);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x73: /* ADC (stack-relative),Y, binary/decimal modes */
        address = stack_relative_indirect_indexed_address(cpu);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            UINT64_C(7));
        break;
    case 0x74: /* STZ direct,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        store_direct(cpu, address, 0U, !accumulator_is_8_bit(cpu),
                     direct_base_cycles(cpu, true));
        break;
    case 0x75: /* ADC direct,X, binary/decimal modes */
        address = direct_address(cpu, cpu->x);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, true), false,
            direct_base_cycles(cpu, true));
        break;
    case 0x76: /* ROR direct,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        modify_memory(cpu, address, true, MEMORY_ROR,
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x77: /* ADC [direct],Y, binary/decimal modes */
        address = direct_indirect_long_address(cpu, cpu->y);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x78: /* SEI */
        cpu->p |= MIPSNES2_CPU_FLAG_I;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x79: /* ADC absolute,Y, binary/decimal modes */
        address = absolute_indexed_address(cpu, cpu->y, false,
                                           &base_cycles);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            base_cycles);
        break;
    case 0x7a: /* PLY */
        if (index_is_8_bit(cpu)) {
            cpu->y = stack_pull8(cpu);
            set_nz8(cpu, (uint8_t)cpu->y);
            cpu->instruction_cycles += UINT64_C(4);
        } else {
            cpu->y = stack_pull16(cpu);
            set_nz16(cpu, cpu->y);
            cpu->instruction_cycles += UINT64_C(5);
        }
        break;
    case 0x7b: /* TDC */
        transfer16_to_accumulator(cpu, cpu->d);
        break;
    case 0x7c: { /* JMP absolute indexed indirect */
        uint16_t pointer = (uint16_t)(fetch16(cpu) + cpu->x);

        cpu->pc = read16_program_bank(cpu, pointer);
        cpu->instruction_cycles += UINT64_C(6);
        break;
    }
    case 0x7d: /* ADC absolute,X, binary/decimal modes */
        address = absolute_indexed_address(cpu, cpu->x, false,
                                           &base_cycles);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            base_cycles);
        break;
    case 0x7e: /* ROR absolute,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->x, true,
                                           &base_cycles);
        modify_memory(cpu, address, false, MEMORY_ROR, UINT64_C(7));
        break;
    case 0x7f: /* ADC absolute long,X, binary/decimal modes */
        address = absolute_long_indexed_address(cpu, cpu->x);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), false,
            UINT64_C(5));
        break;
    case 0x80: /* BRA */
        branch_relative8(cpu, true);
        break;
    case 0x82: /* BRL */
        branch_relative16(cpu);
        break;
    case 0x81: /* STA (direct,X) */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_indirect_address(cpu, cpu->x, 0U, NULL);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x83: /* STA stack-relative */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = stack_relative_address(cpu);
        store_stack(cpu, (uint16_t)address, cpu->a,
                    !accumulator_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0x84: /* STY direct */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        store_direct(cpu, address, cpu->y, !index_is_8_bit(cpu),
                     direct_base_cycles(cpu, false));
        break;
    case 0x85: /* STA direct */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        store_direct(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     direct_base_cycles(cpu, false));
        break;
    case 0x86: /* STX direct */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        store_direct(cpu, address, cpu->x, !index_is_8_bit(cpu),
                     direct_base_cycles(cpu, false));
        break;
    case 0x87: /* STA [direct] */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_indirect_long_address(cpu, 0U);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x89: /* BIT immediate */
        test_bits(cpu, fetch_accumulator_operand(cpu), true, UINT64_C(2));
        break;
    case 0x88: /* DEY */
        change_index(cpu, &cpu->y, false);
        break;
    case 0x8a: /* TXA */
        transfer_to_accumulator(cpu, index_register_value(cpu, cpu->x));
        break;
    case 0x8b: /* PHB */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        stack_push8(cpu, cpu->dbr);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0x8f: /* STA absolute long */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = fetch24(cpu);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     UINT64_C(5));
        break;
    case 0x90: /* BCC */
        branch_relative8(cpu, (cpu->p & MIPSNES2_CPU_FLAG_C) == 0U);
        break;
    case 0x91: /* STA (direct),Y */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_indirect_address(cpu, 0U, cpu->y, NULL);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0x92: /* STA (direct) */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_indirect_address(cpu, 0U, 0U, NULL);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0x93: /* STA (stack-relative),Y */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = stack_relative_indirect_indexed_address(cpu);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     UINT64_C(7));
        break;
    case 0x98: /* TYA */
        transfer_to_accumulator(cpu, index_register_value(cpu, cpu->y));
        break;
    case 0x9a: /* TXS */
        transfer_to_stack(cpu, index_register_value(cpu, cpu->x));
        break;
    case 0x99: /* STA absolute,Y */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->y, true, &base_cycles);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     base_cycles);
        break;
    case 0x9d: /* STA absolute,X */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->x, true, &base_cycles);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     base_cycles);
        break;
    case 0x9e: /* STZ absolute,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->x, true, &base_cycles);
        store_memory(cpu, address, 0U, !accumulator_is_8_bit(cpu),
                     base_cycles);
        break;
    case 0x9f: /* STA absolute long,X */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = absolute_long_indexed_address(cpu, cpu->x);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     UINT64_C(5));
        break;
    case 0x9b: /* TXY */
        transfer_to_index(cpu, &cpu->y, index_register_value(cpu, cpu->x));
        break;
    case 0x9c: /* STZ absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        store_memory(cpu, absolute_address(cpu), 0U,
                     !accumulator_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0x8c: /* STY absolute */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        store_memory(cpu, absolute_address(cpu), cpu->y,
                     !index_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0x8d: /* STA absolute */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        store_memory(cpu, absolute_address(cpu), cpu->a,
                     !accumulator_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0x8e: /* STX absolute */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        store_memory(cpu, absolute_address(cpu), cpu->x,
                     !index_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0x94: /* STY direct,X */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        store_direct(cpu, address, cpu->y, !index_is_8_bit(cpu),
                     direct_base_cycles(cpu, true));
        break;
    case 0x95: /* STA direct,X */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        store_direct(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     direct_base_cycles(cpu, true));
        break;
    case 0x96: /* STX direct,Y */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->y);
        store_direct(cpu, address, cpu->x, !index_is_8_bit(cpu),
                     direct_base_cycles(cpu, true));
        break;
    case 0x97: /* STA [direct],Y */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        address = direct_indirect_long_address(cpu, cpu->y);
        store_memory(cpu, address, cpu->a, !accumulator_is_8_bit(cpu),
                     direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xa0: /* LDY immediate */
        load_index_immediate(cpu, &cpu->y);
        break;
    case 0xa1: /* LDA (direct,X) */
        address = direct_indirect_address(cpu, cpu->x, 0U, NULL);
        load_accumulator_memory(cpu, address,
                                direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xa2: /* LDX immediate */
        load_index_immediate(cpu, &cpu->x);
        break;
    case 0xa3: /* LDA stack-relative */
        address = stack_relative_address(cpu);
        load_accumulator_stack(cpu, (uint16_t)address, UINT64_C(4));
        break;
    case 0xa4: /* LDY direct */
        address = direct_address(cpu, 0U);
        load_index_direct(cpu, &cpu->y, address,
                          direct_base_cycles(cpu, false));
        break;
    case 0xa5: /* LDA direct */
        address = direct_address(cpu, 0U);
        load_accumulator_direct(cpu, address,
                                direct_base_cycles(cpu, false));
        break;
    case 0xa6: /* LDX direct */
        address = direct_address(cpu, 0U);
        load_index_direct(cpu, &cpu->x, address,
                          direct_base_cycles(cpu, false));
        break;
    case 0xa7: /* LDA [direct] */
        address = direct_indirect_long_address(cpu, 0U);
        load_accumulator_memory(cpu, address,
                                direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xa8: /* TAY */
        transfer_to_index(cpu, &cpu->y, cpu->a);
        break;
    case 0xa9: /* LDA immediate */
        load_accumulator_immediate(cpu);
        break;
    case 0xaa: /* TAX */
        transfer_to_index(cpu, &cpu->x, cpu->a);
        break;
    case 0xab: /* PLB */
        cpu->dbr = stack_pull8(cpu);
        set_nz8(cpu, cpu->dbr);
        cpu->instruction_cycles += UINT64_C(4);
        break;
    case 0xac: /* LDY absolute */
        load_index_memory(cpu, &cpu->y, absolute_address(cpu), UINT64_C(4));
        break;
    case 0xad: /* LDA absolute */
        load_accumulator_memory(cpu, absolute_address(cpu), UINT64_C(4));
        break;
    case 0xae: /* LDX absolute */
        load_index_memory(cpu, &cpu->x, absolute_address(cpu), UINT64_C(4));
        break;
    case 0xaf: /* LDA absolute long */
        load_accumulator_memory(cpu, fetch24(cpu), UINT64_C(5));
        break;
    case 0xb0: /* BCS */
        branch_relative8(cpu, (cpu->p & MIPSNES2_CPU_FLAG_C) != 0U);
        break;
    case 0xb1: /* LDA (direct),Y */
    {
        uint16_t pointer;

        address = direct_indirect_address(cpu, 0U, cpu->y, &pointer);
        base_cycles = direct_penalized_cycles(cpu, UINT64_C(5));
        if (!index_is_8_bit(cpu) ||
            (uint32_t)(pointer & UINT16_C(0x00ff)) + cpu->y >
                UINT32_C(0x00ff)) {
            base_cycles += UINT64_C(1);
        }
        load_accumulator_memory(cpu, address, base_cycles);
        break;
    }
    case 0xb2: /* LDA (direct) */
        address = direct_indirect_address(cpu, 0U, 0U, NULL);
        load_accumulator_memory(cpu, address,
                                direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0xb3: /* LDA (stack-relative),Y */
        address = stack_relative_indirect_indexed_address(cpu);
        load_accumulator_memory(cpu, address, UINT64_C(7));
        break;
    case 0xb4: /* LDY direct,X */
        address = direct_address(cpu, cpu->x);
        load_index_direct(cpu, &cpu->y, address,
                          direct_base_cycles(cpu, true));
        break;
    case 0xb5: /* LDA direct,X */
        address = direct_address(cpu, cpu->x);
        load_accumulator_direct(cpu, address,
                                direct_base_cycles(cpu, true));
        break;
    case 0xb6: /* LDX direct,Y */
        address = direct_address(cpu, cpu->y);
        load_index_direct(cpu, &cpu->x, address,
                          direct_base_cycles(cpu, true));
        break;
    case 0xb7: /* LDA [direct],Y */
        address = direct_indirect_long_address(cpu, cpu->y);
        load_accumulator_memory(cpu, address,
                                direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xb8: /* CLV */
        cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_V;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0xba: /* TSX */
        transfer_to_index(cpu, &cpu->x, cpu->s);
        break;
    case 0xbb: /* TYX */
        transfer_to_index(cpu, &cpu->x, index_register_value(cpu, cpu->y));
        break;
    case 0xb9: /* LDA absolute,Y */
        address = absolute_indexed_address(cpu, cpu->y, false, &base_cycles);
        load_accumulator_memory(cpu, address, base_cycles);
        break;
    case 0xbc: /* LDY absolute,X */
        address = absolute_indexed_address(cpu, cpu->x, false, &base_cycles);
        load_index_memory(cpu, &cpu->y, address, base_cycles);
        break;
    case 0xbd: /* LDA absolute,X */
        address = absolute_indexed_address(cpu, cpu->x, false, &base_cycles);
        load_accumulator_memory(cpu, address, base_cycles);
        break;
    case 0xbe: /* LDX absolute,Y */
        address = absolute_indexed_address(cpu, cpu->y, false, &base_cycles);
        load_index_memory(cpu, &cpu->x, address, base_cycles);
        break;
    case 0xbf: /* LDA absolute long,X */
        address = absolute_long_indexed_address(cpu, cpu->x);
        load_accumulator_memory(cpu, address, UINT64_C(5));
        break;
    case 0xc0: /* CPY immediate */
        compare_value(cpu, cpu->y, fetch_index_operand(cpu),
                      !index_is_8_bit(cpu), UINT64_C(2));
        break;
    case 0xc1: /* CMP (direct,X) */
        address = direct_indirect_address(cpu, cpu->x, 0U, NULL);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu),
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xc2: /* REP immediate */
        operand = fetch8(cpu);
        cpu->p &= (uint8_t)~operand;
        apply_mode_invariants(cpu);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0xc3: /* CMP stack-relative */
        address = stack_relative_address(cpu);
        compare_value(
            cpu, cpu->a,
            read_accumulator_stack_operand(cpu, (uint16_t)address),
            !accumulator_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0xc4: /* CPY direct */
        address = direct_address(cpu, 0U);
        compare_value(cpu, cpu->y, read_index_operand(cpu, address, true),
                      !index_is_8_bit(cpu),
                      direct_base_cycles(cpu, false));
        break;
    case 0xc5: /* CMP direct */
        address = direct_address(cpu, 0U);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, true),
                      !accumulator_is_8_bit(cpu),
                      direct_base_cycles(cpu, false));
        break;
    case 0xc6: /* DEC direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        modify_memory(cpu, address, true, MEMORY_DEC,
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0xc7: /* CMP [direct] */
        address = direct_indirect_long_address(cpu, 0U);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu),
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xc8: /* INY */
        change_index(cpu, &cpu->y, true);
        break;
    case 0xc9: /* CMP immediate */
        compare_value(cpu, cpu->a, fetch_accumulator_operand(cpu),
                      !accumulator_is_8_bit(cpu), UINT64_C(2));
        break;
    case 0xca: /* DEX */
        change_index(cpu, &cpu->x, false);
        break;
    case 0xcb: /* WAI */
        cpu->waiting = true;
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0xcc: /* CPY absolute */
        address = absolute_address(cpu);
        compare_value(cpu, cpu->y,
                      read_index_operand(cpu, address, false),
                      !index_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0xcd: /* CMP absolute */
        address = absolute_address(cpu);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0xce: /* DEC absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_address(cpu);
        modify_memory(cpu, address, false, MEMORY_DEC, UINT64_C(6));
        break;
    case 0xcf: /* CMP absolute long */
        address = fetch24(cpu);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu), UINT64_C(5));
        break;
    case 0xd0: /* BNE */
        branch_relative8(cpu, (cpu->p & MIPSNES2_CPU_FLAG_Z) == 0U);
        break;
    case 0xd1: /* CMP (direct),Y */
    {
        uint16_t pointer;

        address = direct_indirect_address(cpu, 0U, cpu->y, &pointer);
        base_cycles = direct_penalized_cycles(cpu, UINT64_C(5));
        if (!index_is_8_bit(cpu) ||
            (uint32_t)(pointer & UINT16_C(0x00ff)) + cpu->y >
                UINT32_C(0x00ff)) {
            base_cycles += UINT64_C(1);
        }
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu), base_cycles);
        break;
    }
    case 0xd2: /* CMP (direct) */
        address = direct_indirect_address(cpu, 0U, 0U, NULL);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu),
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0xd3: /* CMP (stack-relative),Y */
        address = stack_relative_indirect_indexed_address(cpu);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu), UINT64_C(7));
        break;
    case 0xd4: { /* PEI direct */
        uint16_t value;

        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        value = read16_direct(cpu, address);
        stack_push16_linear(cpu, value);
        cpu->instruction_cycles += direct_penalized_cycles(
            cpu, UINT64_C(6));
        break;
    }
    case 0xd5: /* CMP direct,X */
        address = direct_address(cpu, cpu->x);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, true),
                      !accumulator_is_8_bit(cpu),
                      direct_base_cycles(cpu, true));
        break;
    case 0xd6: /* DEC direct,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        modify_memory(cpu, address, true, MEMORY_DEC,
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xd7: /* CMP [direct],Y */
        address = direct_indirect_long_address(cpu, cpu->y);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu),
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xd8: /* CLD */
        cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_D;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0xd9: /* CMP absolute,Y */
        address = absolute_indexed_address(cpu, cpu->y, false,
                                           &base_cycles);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu), base_cycles);
        break;
    case 0xda: /* PHX */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        if (index_is_8_bit(cpu)) {
            stack_push8(cpu, (uint8_t)cpu->x);
            cpu->instruction_cycles += UINT64_C(3);
        } else {
            stack_push16(cpu, cpu->x);
            cpu->instruction_cycles += UINT64_C(4);
        }
        break;
    case 0xdb: /* STP */
        cpu->clock_stopped = true;
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0xdc: { /* JMP absolute indirect long */
        mipsnes2_address target = read24_bank_zero(cpu, fetch16(cpu));

        cpu->pc = (uint16_t)target;
        cpu->pbr = (uint8_t)(target >> 16);
        cpu->instruction_cycles += UINT64_C(6);
        break;
    }
    case 0xdd: /* CMP absolute,X */
        address = absolute_indexed_address(cpu, cpu->x, false,
                                           &base_cycles);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu), base_cycles);
        break;
    case 0xde: /* DEC absolute,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->x, true,
                                           &base_cycles);
        modify_memory(cpu, address, false, MEMORY_DEC, UINT64_C(7));
        break;
    case 0xdf: /* CMP absolute long,X */
        address = absolute_long_indexed_address(cpu, cpu->x);
        compare_value(cpu, cpu->a,
                      read_accumulator_operand(cpu, address, false),
                      !accumulator_is_8_bit(cpu), UINT64_C(5));
        break;
    case 0xe0: /* CPX immediate */
        compare_value(cpu, cpu->x, fetch_index_operand(cpu),
                      !index_is_8_bit(cpu), UINT64_C(2));
        break;
    case 0xe1: /* SBC (direct,X), binary/decimal modes */
        address = direct_indirect_address(cpu, cpu->x, 0U, NULL);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xe2: /* SEP immediate */
        operand = fetch8(cpu);
        cpu->p |= operand;
        apply_mode_invariants(cpu);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0xe3: /* SBC stack-relative, binary/decimal modes */
        address = stack_relative_address(cpu);
        arithmetic_accumulator(
            cpu, read_accumulator_stack_operand(cpu, (uint16_t)address),
            true, UINT64_C(4));
        break;
    case 0xe4: /* CPX direct */
        address = direct_address(cpu, 0U);
        compare_value(cpu, cpu->x, read_index_operand(cpu, address, true),
                      !index_is_8_bit(cpu),
                      direct_base_cycles(cpu, false));
        break;
    case 0xe5: /* SBC direct, binary/decimal modes */
        address = direct_address(cpu, 0U);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, true), true,
            direct_base_cycles(cpu, false));
        break;
    case 0xe6: /* INC direct */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, 0U);
        modify_memory(cpu, address, true, MEMORY_INC,
                      direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0xe7: /* SBC [direct], binary/decimal modes */
        address = direct_indirect_long_address(cpu, 0U);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xe8: /* INX */
        change_index(cpu, &cpu->x, true);
        break;
    case 0xe9: /* SBC immediate, binary/decimal modes */
        arithmetic_accumulator(cpu, fetch_accumulator_operand(cpu),
                                      true, UINT64_C(2));
        break;
    case 0xea: /* NOP */
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0xeb: /* XBA */
        exchange_accumulator_bytes(cpu);
        break;
    case 0xec: /* CPX absolute */
        address = absolute_address(cpu);
        compare_value(cpu, cpu->x,
                      read_index_operand(cpu, address, false),
                      !index_is_8_bit(cpu), UINT64_C(4));
        break;
    case 0xed: /* SBC absolute, binary/decimal modes */
        address = absolute_address(cpu);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            UINT64_C(4));
        break;
    case 0xee: /* INC absolute */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_address(cpu);
        modify_memory(cpu, address, false, MEMORY_INC, UINT64_C(6));
        break;
    case 0xef: /* SBC absolute long, binary/decimal modes */
        address = fetch24(cpu);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            UINT64_C(5));
        break;
    case 0xf0: /* BEQ */
        branch_relative8(cpu, (cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
        break;
    case 0xf1: /* SBC (direct),Y, binary/decimal modes */
    {
        uint16_t pointer;

        address = direct_indirect_address(cpu, 0U, cpu->y, &pointer);
        base_cycles = direct_penalized_cycles(cpu, UINT64_C(5));
        if (!index_is_8_bit(cpu) ||
            (uint32_t)(pointer & UINT16_C(0x00ff)) + cpu->y >
                UINT32_C(0x00ff)) {
            base_cycles += UINT64_C(1);
        }
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            base_cycles);
        break;
    }
    case 0xf2: /* SBC (direct), binary/decimal modes */
        address = direct_indirect_address(cpu, 0U, 0U, NULL);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            direct_penalized_cycles(cpu, UINT64_C(5)));
        break;
    case 0xf3: /* SBC (stack-relative),Y, binary/decimal modes */
        address = stack_relative_indirect_indexed_address(cpu);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            UINT64_C(7));
        break;
    case 0xf4: { /* PEA absolute */
        uint16_t value;

        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        value = fetch16(cpu);
        stack_push16_linear(cpu, value);
        cpu->instruction_cycles += UINT64_C(5);
        break;
    }
    case 0xf5: /* SBC direct,X, binary/decimal modes */
        address = direct_address(cpu, cpu->x);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, true), true,
            direct_base_cycles(cpu, true));
        break;
    case 0xf6: /* INC direct,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = direct_address(cpu, cpu->x);
        modify_memory(cpu, address, true, MEMORY_INC,
                      direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xf7: /* SBC [direct],Y, binary/decimal modes */
        address = direct_indirect_long_address(cpu, cpu->y);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            direct_penalized_cycles(cpu, UINT64_C(6)));
        break;
    case 0xf8: /* SED */
        cpu->p |= MIPSNES2_CPU_FLAG_D;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0xf9: /* SBC absolute,Y, binary/decimal modes */
        address = absolute_indexed_address(cpu, cpu->y, false,
                                           &base_cycles);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            base_cycles);
        break;
    case 0xfa: /* PLX */
        if (index_is_8_bit(cpu)) {
            cpu->x = stack_pull8(cpu);
            set_nz8(cpu, (uint8_t)cpu->x);
            cpu->instruction_cycles += UINT64_C(4);
        } else {
            cpu->x = stack_pull16(cpu);
            set_nz16(cpu, cpu->x);
            cpu->instruction_cycles += UINT64_C(5);
        }
        break;
    case 0xfb: { /* XCE */
        bool old_carry = (cpu->p & MIPSNES2_CPU_FLAG_C) != 0U;
        bool old_emulation = cpu->emulation;
        cpu->emulation = old_carry;
        if (old_emulation) {
            cpu->p |= MIPSNES2_CPU_FLAG_C;
        } else {
            cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
        }
        apply_mode_invariants(cpu);
        cpu->instruction_cycles += UINT64_C(2);
        break;
    }
    case 0xfc: { /* JSR absolute indexed indirect */
        uint16_t pointer_low;
        uint16_t pointer;
        uint16_t return_address;
        uint16_t initial_stack;

        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        pointer_low = fetch8(cpu);
        return_address = cpu->pc;
        initial_stack = cpu->s;
        write8(cpu, initial_stack, (uint8_t)(return_address >> 8));
        write8(cpu, (uint16_t)(initial_stack - UINT16_C(1)),
               (uint8_t)return_address);
        cpu->s = (uint16_t)(initial_stack - UINT16_C(2));
        apply_mode_invariants(cpu);
        pointer = (uint16_t)(pointer_low |
                             (uint16_t)((uint16_t)fetch8(cpu) << 8));
        pointer = (uint16_t)(pointer + cpu->x);
        cpu->pc = read16_program_bank(cpu, pointer);
        cpu->instruction_cycles += UINT64_C(8);
        break;
    }
    case 0xfd: /* SBC absolute,X, binary/decimal modes */
        address = absolute_indexed_address(cpu, cpu->x, false,
                                           &base_cycles);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            base_cycles);
        break;
    case 0xfe: /* INC absolute,X */
        if (!require_write_bus(cpu)) {
            return cpu->stop_reason;
        }
        address = absolute_indexed_address(cpu, cpu->x, true,
                                           &base_cycles);
        modify_memory(cpu, address, false, MEMORY_INC, UINT64_C(7));
        break;
    case 0xff: /* SBC absolute long,X, binary/decimal modes */
        address = absolute_long_indexed_address(cpu, cpu->x);
        arithmetic_accumulator(
            cpu, read_accumulator_operand(cpu, address, false), true,
            UINT64_C(5));
        break;
    default:
        cpu->stopped = true;
        cpu->stop_reason = MIPSNES2_CPU_UNIMPLEMENTED_OPCODE;
        return cpu->stop_reason;
    }

    return MIPSNES2_CPU_OK;
}

typedef enum {
    CPU_CYCLE_PHASE_IDLE = 0,
    CPU_CYCLE_PHASE_BOUNDARY,
    CPU_CYCLE_PHASE_FETCH,
    CPU_CYCLE_PHASE_NOP_INTERNAL,
    CPU_CYCLE_PHASE_LDA8_VALUE,
    CPU_CYCLE_PHASE_LDA16_LOW,
    CPU_CYCLE_PHASE_LDA16_HIGH,
    CPU_CYCLE_PHASE_STA_ADDRESS_LOW,
    CPU_CYCLE_PHASE_STA_ADDRESS_HIGH,
    CPU_CYCLE_PHASE_STA_WRITE8,
    CPU_CYCLE_PHASE_STA_WRITE16_LOW,
    CPU_CYCLE_PHASE_STA_WRITE16_HIGH,
    CPU_CYCLE_PHASE_REP_VALUE,
    CPU_CYCLE_PHASE_REP_INTERNAL,
    CPU_CYCLE_PHASE_SEP_VALUE,
    CPU_CYCLE_PHASE_SEP_INTERNAL,
    CPU_CYCLE_PHASE_SOFTWARE_INTERRUPT_SIGNATURE,
    CPU_CYCLE_PHASE_INTERRUPT_INTERNAL_1,
    CPU_CYCLE_PHASE_INTERRUPT_INTERNAL_2,
    CPU_CYCLE_PHASE_INTERRUPT_PUSH_PBR,
    CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCH,
    CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCL,
    CPU_CYCLE_PHASE_INTERRUPT_PUSH_STATUS,
    CPU_CYCLE_PHASE_INTERRUPT_VECTOR_LOW,
    CPU_CYCLE_PHASE_INTERRUPT_VECTOR_HIGH,
    CPU_CYCLE_PHASE_LEGACY_ATOMIC,
    CPU_CYCLE_PHASE_COMPLETE
} cpu_cycle_phase;

static void set_operation(mipsnes2_cpu_operation *operation,
                          mipsnes2_cpu_operation_kind kind,
                          mipsnes2_address address, uint8_t value)
{
    operation->kind = kind;
    operation->address = mipsnes2_address24(address);
    operation->value = value;
}

static bool operation_matches(const mipsnes2_cpu_operation *left,
                              const mipsnes2_cpu_operation *right)
{
    return left->kind == right->kind && left->address == right->address &&
           left->value == right->value;
}

enum {
    CPU_BOUNDARY_ABORT = 1U << 0,
    CPU_BOUNDARY_NMI = 1U << 1,
    CPU_BOUNDARY_IRQ_LEVEL = 1U << 2,
    CPU_BOUNDARY_IRQ_ACCEPTED = 1U << 3,
    CPU_BOUNDARY_WAITING = 1U << 4,
    CPU_BOUNDARY_NATIVE = 1U << 5,
    CPU_BOUNDARY_NO_WRITE = 1U << 6,
    CPU_BOUNDARY_NO_READ = 1U << 7
};

static uint8_t boundary_snapshot(const mipsnes2_cpu *cpu)
{
    uint8_t snapshot = 0U;

    if (cpu->abort_pending) {
        snapshot |= CPU_BOUNDARY_ABORT;
    }
    if (cpu->nmi_pending) {
        snapshot |= CPU_BOUNDARY_NMI;
    }
    if (cpu->irq_level) {
        snapshot |= CPU_BOUNDARY_IRQ_LEVEL;
        if ((cpu->p & MIPSNES2_CPU_FLAG_I) == 0U) {
            snapshot |= CPU_BOUNDARY_IRQ_ACCEPTED;
        }
    }
    if (cpu->waiting) {
        snapshot |= CPU_BOUNDARY_WAITING;
    }
    if (snapshot != 0U) {
        if (!cpu->emulation) {
            snapshot |= CPU_BOUNDARY_NATIVE;
        }
        if (cpu->bus.write8 == NULL) {
            snapshot |= CPU_BOUNDARY_NO_WRITE;
        }
        if (cpu->bus.read8 == NULL) {
            snapshot |= CPU_BOUNDARY_NO_READ;
        }
    }
    return snapshot;
}

static void complete_cycle_state(mipsnes2_cpu_cycle_state *state)
{
    state->phase = (uint8_t)CPU_CYCLE_PHASE_COMPLETE;
    state->active = false;
}

static void prepare_incremental_interrupt(
    const mipsnes2_cpu *cpu, mipsnes2_cpu_cycle_state *state,
    interrupt_kind kind, bool hardware_entry)
{
    uint8_t pushed_status = cpu->p;

    state->interrupt_kind = (uint8_t)kind;
    state->interrupt_pc = cpu->pc;
    state->interrupt_vector = interrupt_vector(cpu, kind);
    state->interrupt_pbr = cpu->pbr;
    state->interrupt_emulation = cpu->emulation;
    if (cpu->emulation && kind == INTERRUPT_BRK) {
        pushed_status |= MIPSNES2_CPU_FLAG_X;
    } else if (cpu->emulation) {
        pushed_status &= (uint8_t)~MIPSNES2_CPU_FLAG_X;
    }
    state->pushed_status = pushed_status;
    state->vector_low = 0U;
    if (hardware_entry) {
        state->phase = (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_INTERNAL_1;
    } else {
        state->phase = cpu->emulation
                           ? (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCH
                           : (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_PUSH_PBR;
    }
}

static mipsnes2_cpu_cycle_result begin_boundary_interrupt(
    mipsnes2_cpu *cpu, mipsnes2_cpu_cycle_state *state,
    interrupt_kind kind, mipsnes2_cpu_result *instruction_result)
{
    if (!require_write_bus(cpu)) {
        *instruction_result = cpu->stop_reason;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    }
    prepare_incremental_interrupt(cpu, state, kind, true);
    return MIPSNES2_CPU_CYCLE_READY;
}

static mipsnes2_cpu_cycle_result commit_boundary(
    mipsnes2_cpu *cpu, mipsnes2_cpu_cycle_state *state,
    mipsnes2_cpu_result *instruction_result)
{
    if (cpu->bus.read8 == NULL) {
        *instruction_result = MIPSNES2_CPU_INVALID_BUS;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    }
    if (cpu->stopped) {
        *instruction_result = cpu->stop_reason;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    }
    if (cpu->clock_stopped) {
        *instruction_result = MIPSNES2_CPU_CLOCK_STOPPED;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    }
    if (cpu->abort_pending) {
        cpu->abort_pending = false;
        if (cpu->waiting) {
            /* WDC 7.13: ABORT does not release WAI; a later interrupt
             * must return to the WAI opcode rather than its successor. */
            cpu->pbr = (uint8_t)(cpu->last_opcode_address >> 16);
            cpu->pc = (uint16_t)cpu->last_opcode_address;
        } else {
            return begin_boundary_interrupt(
                cpu, state, INTERRUPT_ABORT, instruction_result);
        }
    }
    if (cpu->nmi_pending) {
        cpu->waiting = false;
        cpu->nmi_pending = false;
        return begin_boundary_interrupt(
            cpu, state, INTERRUPT_NMI, instruction_result);
    }
    if (cpu->irq_level && (cpu->p & MIPSNES2_CPU_FLAG_I) == 0U) {
        cpu->waiting = false;
        return begin_boundary_interrupt(
            cpu, state, INTERRUPT_IRQ, instruction_result);
    }
    if (cpu->waiting) {
        if (!cpu->irq_level) {
            *instruction_result = MIPSNES2_CPU_WAITING;
            complete_cycle_state(state);
            return MIPSNES2_CPU_CYCLE_COMPLETE;
        }
        cpu->waiting = false;
    }

    state->phase = (uint8_t)CPU_CYCLE_PHASE_FETCH;
    return MIPSNES2_CPU_CYCLE_READY;
}

static void decode_incremental_opcode(mipsnes2_cpu *cpu,
                                      mipsnes2_cpu_cycle_state *state)
{
    switch (state->opcode) {
    case 0x00: /* BRK signature */
        state->interrupt_kind = (uint8_t)INTERRUPT_BRK;
        state->phase = cpu->bus.write8 == NULL
                           ? (uint8_t)CPU_CYCLE_PHASE_LEGACY_ATOMIC
                           : (uint8_t)CPU_CYCLE_PHASE_SOFTWARE_INTERRUPT_SIGNATURE;
        break;
    case 0x02: /* COP signature */
        state->interrupt_kind = (uint8_t)INTERRUPT_COP;
        state->phase = cpu->bus.write8 == NULL
                           ? (uint8_t)CPU_CYCLE_PHASE_LEGACY_ATOMIC
                           : (uint8_t)CPU_CYCLE_PHASE_SOFTWARE_INTERRUPT_SIGNATURE;
        break;
    case 0x8d: /* STA absolute */
        state->phase = cpu->bus.write8 == NULL
                           ? (uint8_t)CPU_CYCLE_PHASE_LEGACY_ATOMIC
                           : (uint8_t)CPU_CYCLE_PHASE_STA_ADDRESS_LOW;
        break;
    case 0xa9: /* LDA immediate */
        state->phase = accumulator_is_8_bit(cpu)
                           ? (uint8_t)CPU_CYCLE_PHASE_LDA8_VALUE
                           : (uint8_t)CPU_CYCLE_PHASE_LDA16_LOW;
        break;
    case 0xc2: /* REP immediate */
        state->phase = (uint8_t)CPU_CYCLE_PHASE_REP_VALUE;
        break;
    case 0xe2: /* SEP immediate */
        state->phase = (uint8_t)CPU_CYCLE_PHASE_SEP_VALUE;
        break;
    case 0xea: /* NOP */
        state->phase = (uint8_t)CPU_CYCLE_PHASE_NOP_INTERNAL;
        break;
    default:
        state->phase = (uint8_t)CPU_CYCLE_PHASE_LEGACY_ATOMIC;
        break;
    }
}

void mipsnes2_cpu_cycle_state_init(mipsnes2_cpu_cycle_state *state)
{
    if (state != NULL) {
        (void)memset(state, 0, sizeof(*state));
    }
}

mipsnes2_cpu_cycle_result mipsnes2_cpu_cycle_begin(
    mipsnes2_cpu_cycle_state *state)
{
    if (state == NULL) {
        return MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT;
    }
    if (state->active) {
        return MIPSNES2_CPU_CYCLE_INVALID_STATE;
    }

    state->operand = 0U;
    state->interrupt_pc = 0U;
    state->interrupt_vector = 0U;
    state->opcode = 0U;
    state->interrupt_kind = (uint8_t)INTERRUPT_NONE;
    state->interrupt_pbr = 0U;
    state->pushed_status = 0U;
    state->vector_low = 0U;
    state->interrupt_emulation = false;
    state->phase = (uint8_t)CPU_CYCLE_PHASE_BOUNDARY;
    state->active = true;
    return MIPSNES2_CPU_CYCLE_READY;
}

mipsnes2_cpu_cycle_result mipsnes2_cpu_cycle_peek(
    const mipsnes2_cpu *cpu,
    const mipsnes2_cpu_cycle_state *state,
    mipsnes2_cpu_operation *operation)
{
    mipsnes2_address address;

    if (cpu == NULL || state == NULL || operation == NULL) {
        return MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT;
    }
    if (!state->active) {
        return state->phase == (uint8_t)CPU_CYCLE_PHASE_COMPLETE
                   ? MIPSNES2_CPU_CYCLE_COMPLETE
                   : MIPSNES2_CPU_CYCLE_INVALID_STATE;
    }

    switch ((cpu_cycle_phase)state->phase) {
    case CPU_CYCLE_PHASE_BOUNDARY:
        set_operation(operation, MIPSNES2_CPU_OPERATION_CONTROL,
                      0U, boundary_snapshot(cpu));
        break;
    case CPU_CYCLE_PHASE_FETCH:
    case CPU_CYCLE_PHASE_LDA8_VALUE:
    case CPU_CYCLE_PHASE_LDA16_LOW:
    case CPU_CYCLE_PHASE_LDA16_HIGH:
    case CPU_CYCLE_PHASE_STA_ADDRESS_LOW:
    case CPU_CYCLE_PHASE_STA_ADDRESS_HIGH:
    case CPU_CYCLE_PHASE_REP_VALUE:
    case CPU_CYCLE_PHASE_SEP_VALUE:
    case CPU_CYCLE_PHASE_SOFTWARE_INTERRUPT_SIGNATURE:
        set_operation(operation, MIPSNES2_CPU_OPERATION_READ,
                      mipsnes2_address_make(cpu->pbr, cpu->pc), 0U);
        break;
    case CPU_CYCLE_PHASE_NOP_INTERNAL:
    case CPU_CYCLE_PHASE_REP_INTERNAL:
    case CPU_CYCLE_PHASE_SEP_INTERNAL:
        set_operation(operation, MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 0U);
        break;
    case CPU_CYCLE_PHASE_INTERRUPT_INTERNAL_1:
        set_operation(operation, MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U);
        break;
    case CPU_CYCLE_PHASE_INTERRUPT_INTERNAL_2:
        set_operation(operation, MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U);
        break;
    case CPU_CYCLE_PHASE_INTERRUPT_PUSH_PBR:
        set_operation(operation, MIPSNES2_CPU_OPERATION_WRITE, cpu->s,
                      state->interrupt_pbr);
        break;
    case CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCH:
        set_operation(operation, MIPSNES2_CPU_OPERATION_WRITE, cpu->s,
                      (uint8_t)(state->interrupt_pc >> 8));
        break;
    case CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCL:
        set_operation(operation, MIPSNES2_CPU_OPERATION_WRITE, cpu->s,
                      (uint8_t)state->interrupt_pc);
        break;
    case CPU_CYCLE_PHASE_INTERRUPT_PUSH_STATUS:
        set_operation(operation, MIPSNES2_CPU_OPERATION_WRITE, cpu->s,
                      state->pushed_status);
        break;
    case CPU_CYCLE_PHASE_INTERRUPT_VECTOR_LOW:
        set_operation(operation, MIPSNES2_CPU_OPERATION_READ,
                      state->interrupt_vector, 0U);
        break;
    case CPU_CYCLE_PHASE_INTERRUPT_VECTOR_HIGH:
        set_operation(operation, MIPSNES2_CPU_OPERATION_READ,
                      (uint16_t)(state->interrupt_vector + UINT16_C(1)), 0U);
        break;
    case CPU_CYCLE_PHASE_STA_WRITE8:
    case CPU_CYCLE_PHASE_STA_WRITE16_LOW:
        address = mipsnes2_address_make(cpu->dbr, state->operand);
        set_operation(operation, MIPSNES2_CPU_OPERATION_WRITE, address,
                      (uint8_t)cpu->a);
        break;
    case CPU_CYCLE_PHASE_STA_WRITE16_HIGH:
        address = mipsnes2_address_make(cpu->dbr, state->operand) +
                  UINT32_C(1);
        set_operation(operation, MIPSNES2_CPU_OPERATION_WRITE, address,
                      (uint8_t)(cpu->a >> 8));
        break;
    case CPU_CYCLE_PHASE_LEGACY_ATOMIC:
        set_operation(operation, MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                      0U, 0U);
        break;
    case CPU_CYCLE_PHASE_COMPLETE:
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_IDLE:
    default:
        return MIPSNES2_CPU_CYCLE_INVALID_STATE;
    }
    return MIPSNES2_CPU_CYCLE_READY;
}

mipsnes2_cpu_cycle_result mipsnes2_cpu_cycle_commit(
    mipsnes2_cpu *cpu,
    mipsnes2_cpu_cycle_state *state,
    const mipsnes2_cpu_operation *operation,
    mipsnes2_cpu_result *instruction_result)
{
    mipsnes2_cpu_operation current;
    mipsnes2_cpu_cycle_result peek_result;
    uint8_t value;

    if (cpu == NULL || state == NULL || operation == NULL ||
        instruction_result == NULL) {
        return MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT;
    }
    *instruction_result = MIPSNES2_CPU_NOT_READY;
    peek_result = mipsnes2_cpu_cycle_peek(cpu, state, &current);
    if (peek_result != MIPSNES2_CPU_CYCLE_READY) {
        return peek_result;
    }
    if (!operation_matches(operation, &current)) {
        return MIPSNES2_CPU_CYCLE_STALE_OPERATION;
    }

    switch ((cpu_cycle_phase)state->phase) {
    case CPU_CYCLE_PHASE_BOUNDARY:
        return commit_boundary(cpu, state, instruction_result);
    case CPU_CYCLE_PHASE_FETCH:
        cpu->last_opcode_address = current.address;
        value = read8(cpu, current.address);
        cpu->last_opcode = value;
        cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
        cpu->instruction_cycles += UINT64_C(1);
        state->opcode = value;
        decode_incremental_opcode(cpu, state);
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_NOP_INTERNAL:
        cpu->instruction_cycles += UINT64_C(1);
        *instruction_result = MIPSNES2_CPU_OK;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_LDA8_VALUE:
        value = read8(cpu, current.address);
        cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
        cpu->a = (uint16_t)((cpu->a & UINT16_C(0xff00)) | value);
        set_nz8(cpu, value);
        cpu->instruction_cycles += UINT64_C(1);
        *instruction_result = MIPSNES2_CPU_OK;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_LDA16_LOW:
        state->operand = read8(cpu, current.address);
        cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_LDA16_HIGH;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_LDA16_HIGH:
        value = read8(cpu, current.address);
        cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
        cpu->a = (uint16_t)(state->operand | (uint16_t)(value << 8));
        set_nz16(cpu, cpu->a);
        cpu->instruction_cycles += UINT64_C(1);
        *instruction_result = MIPSNES2_CPU_OK;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_STA_ADDRESS_LOW:
        state->operand = read8(cpu, current.address);
        cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_STA_ADDRESS_HIGH;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_STA_ADDRESS_HIGH:
        value = read8(cpu, current.address);
        cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
        cpu->instruction_cycles += UINT64_C(1);
        state->operand |= (uint16_t)(value << 8);
        state->phase = accumulator_is_8_bit(cpu)
                           ? (uint8_t)CPU_CYCLE_PHASE_STA_WRITE8
                           : (uint8_t)CPU_CYCLE_PHASE_STA_WRITE16_LOW;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_STA_WRITE8:
        write8(cpu, current.address, current.value);
        cpu->instruction_cycles += UINT64_C(1);
        *instruction_result = MIPSNES2_CPU_OK;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_STA_WRITE16_LOW:
        write8(cpu, current.address, current.value);
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_STA_WRITE16_HIGH;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_STA_WRITE16_HIGH:
        write8(cpu, current.address, current.value);
        cpu->instruction_cycles += UINT64_C(1);
        *instruction_result = MIPSNES2_CPU_OK;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_REP_VALUE:
    case CPU_CYCLE_PHASE_SEP_VALUE:
        state->operand = read8(cpu, current.address);
        cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = state->phase ==
                                   (uint8_t)CPU_CYCLE_PHASE_REP_VALUE
                           ? (uint8_t)CPU_CYCLE_PHASE_REP_INTERNAL
                           : (uint8_t)CPU_CYCLE_PHASE_SEP_INTERNAL;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_REP_INTERNAL:
        cpu->p &= (uint8_t)~(uint8_t)state->operand;
        apply_mode_invariants(cpu);
        cpu->instruction_cycles += UINT64_C(1);
        *instruction_result = MIPSNES2_CPU_OK;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_SEP_INTERNAL:
        cpu->p |= (uint8_t)state->operand;
        apply_mode_invariants(cpu);
        cpu->instruction_cycles += UINT64_C(1);
        *instruction_result = MIPSNES2_CPU_OK;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_SOFTWARE_INTERRUPT_SIGNATURE:
        (void)read8(cpu, current.address);
        cpu->pc = (uint16_t)(cpu->pc + UINT16_C(1));
        cpu->instruction_cycles += UINT64_C(1);
        prepare_incremental_interrupt(
            cpu, state, (interrupt_kind)state->interrupt_kind, false);
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_INTERRUPT_INTERNAL_1:
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_INTERNAL_2;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_INTERRUPT_INTERNAL_2:
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = state->interrupt_emulation
                           ? (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCH
                           : (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_PUSH_PBR;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_INTERRUPT_PUSH_PBR:
        write8(cpu, current.address, current.value);
        stack_decrement(cpu);
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCH;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCH:
        write8(cpu, current.address, current.value);
        stack_decrement(cpu);
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCL;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_INTERRUPT_PUSH_PCL:
        write8(cpu, current.address, current.value);
        stack_decrement(cpu);
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_PUSH_STATUS;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_INTERRUPT_PUSH_STATUS:
        write8(cpu, current.address, current.value);
        stack_decrement(cpu);
        cpu->p |= MIPSNES2_CPU_FLAG_I;
        cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_D;
        cpu->pbr = 0U;
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_VECTOR_LOW;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_INTERRUPT_VECTOR_LOW:
        state->vector_low = read8(cpu, current.address);
        cpu->instruction_cycles += UINT64_C(1);
        state->phase = (uint8_t)CPU_CYCLE_PHASE_INTERRUPT_VECTOR_HIGH;
        return MIPSNES2_CPU_CYCLE_READY;
    case CPU_CYCLE_PHASE_INTERRUPT_VECTOR_HIGH:
        value = read8(cpu, current.address);
        cpu->pc = (uint16_t)(state->vector_low | (uint16_t)(value << 8));
        cpu->instruction_cycles += UINT64_C(1);
        *instruction_result = MIPSNES2_CPU_OK;
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    case CPU_CYCLE_PHASE_LEGACY_ATOMIC: {
        uint64_t cycles_before = cpu->instruction_cycles;

        *instruction_result = execute_fetched_opcode(cpu);
        if (cpu->instruction_cycles > cycles_before) {
            /* The incremental fetch already charged the opcode cycle. */
            cpu->instruction_cycles -= UINT64_C(1);
        } else if (*instruction_result != MIPSNES2_CPU_OK) {
            /* Preserve the legacy fail-stop accounting contract until that
             * opcode has a fully resumable sequence of its own. */
            cpu->instruction_cycles -= UINT64_C(1);
        }
        complete_cycle_state(state);
        return MIPSNES2_CPU_CYCLE_COMPLETE;
    }
    case CPU_CYCLE_PHASE_IDLE:
    case CPU_CYCLE_PHASE_COMPLETE:
    default:
        return MIPSNES2_CPU_CYCLE_INVALID_STATE;
    }
}

mipsnes2_cpu_result mipsnes2_cpu_step(mipsnes2_cpu *cpu)
{
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_result instruction_result = MIPSNES2_CPU_NOT_READY;
    mipsnes2_cpu_cycle_result cycle_result;

    if (cpu == NULL) {
        return MIPSNES2_CPU_INVALID_BUS;
    }

    mipsnes2_cpu_cycle_state_init(&state);
    cycle_result = mipsnes2_cpu_cycle_begin(&state);
    while (cycle_result == MIPSNES2_CPU_CYCLE_READY) {
        cycle_result = mipsnes2_cpu_cycle_peek(cpu, &state, &operation);
        if (cycle_result != MIPSNES2_CPU_CYCLE_READY) {
            break;
        }
        cycle_result = mipsnes2_cpu_cycle_commit(
            cpu, &state, &operation, &instruction_result);
    }

    if (cycle_result == MIPSNES2_CPU_CYCLE_COMPLETE) {
        return instruction_result;
    }
    return MIPSNES2_CPU_NOT_READY;
}
