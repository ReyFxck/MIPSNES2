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

static void store_absolute(mipsnes2_cpu *cpu, uint16_t value, bool wide)
{
    uint16_t offset = fetch16(cpu);
    write8(cpu, mipsnes2_address_make(cpu->dbr, offset), (uint8_t)value);
    if (wide) {
        write8(cpu, mipsnes2_address_make(cpu->dbr,
                                          (uint16_t)(offset + UINT16_C(1))),
               (uint8_t)(value >> 8));
        cpu->instruction_cycles += UINT64_C(5);
    } else {
        cpu->instruction_cycles += UINT64_C(4);
    }
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
    cpu->stopped = false;
    cpu->stop_reason = MIPSNES2_CPU_OK;
    cpu->last_opcode = 0U;
    cpu->last_opcode_address = mipsnes2_address_make(cpu->pbr, cpu->pc);
    return MIPSNES2_CPU_OK;
}

mipsnes2_cpu_result mipsnes2_cpu_step(mipsnes2_cpu *cpu)
{
    uint8_t operand;
    uint16_t old_pc;

    if (cpu == NULL || cpu->bus.read8 == NULL) {
        return MIPSNES2_CPU_INVALID_BUS;
    }
    if (cpu->stopped) {
        return cpu->stop_reason;
    }

    cpu->last_opcode_address = mipsnes2_address_make(cpu->pbr, cpu->pc);
    cpu->last_opcode = fetch8(cpu);

    switch (cpu->last_opcode) {
    case 0x18: /* CLC */
        cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x38: /* SEC */
        cpu->p |= MIPSNES2_CPU_FLAG_C;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x4c: /* JMP absolute */
        cpu->pc = fetch16(cpu);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0x80: /* BRA */
        operand = fetch8(cpu);
        old_pc = cpu->pc;
        cpu->pc = (uint16_t)(cpu->pc + (int8_t)operand);
        cpu->instruction_cycles += UINT64_C(3);
        if (cpu->emulation && (old_pc & UINT16_C(0xff00)) !=
                                  (cpu->pc & UINT16_C(0xff00))) {
            cpu->instruction_cycles += UINT64_C(1);
        }
        break;
    case 0x8c: /* STY absolute */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        store_absolute(cpu, cpu->y, !index_is_8_bit(cpu));
        break;
    case 0x8d: /* STA absolute */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        store_absolute(cpu, cpu->a, !accumulator_is_8_bit(cpu));
        break;
    case 0x8e: /* STX absolute */
        if (cpu->bus.write8 == NULL) {
            cpu->stopped = true;
            cpu->stop_reason = MIPSNES2_CPU_INVALID_BUS;
            return cpu->stop_reason;
        }
        store_absolute(cpu, cpu->x, !index_is_8_bit(cpu));
        break;
    case 0xa0: /* LDY immediate */
        load_index_immediate(cpu, &cpu->y);
        break;
    case 0xa2: /* LDX immediate */
        load_index_immediate(cpu, &cpu->x);
        break;
    case 0xa9: /* LDA immediate */
        load_accumulator_immediate(cpu);
        break;
    case 0xc2: /* REP immediate */
        operand = fetch8(cpu);
        cpu->p &= (uint8_t)~operand;
        apply_mode_invariants(cpu);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0xe2: /* SEP immediate */
        operand = fetch8(cpu);
        cpu->p |= operand;
        apply_mode_invariants(cpu);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0xea: /* NOP */
        cpu->instruction_cycles += UINT64_C(2);
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
    default:
        cpu->stopped = true;
        cpu->stop_reason = MIPSNES2_CPU_UNIMPLEMENTED_OPCODE;
        return cpu->stop_reason;
    }

    return MIPSNES2_CPU_OK;
}
