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

static void write16_linear(const mipsnes2_cpu *cpu,
                           mipsnes2_address address, uint16_t value)
{
    write8(cpu, address, (uint8_t)value);
    write8(cpu, address + UINT32_C(1), (uint8_t)(value >> 8));
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
    mipsnes2_address address;
    uint64_t base_cycles;

    if (cpu == NULL || cpu->bus.read8 == NULL) {
        return MIPSNES2_CPU_INVALID_BUS;
    }
    if (cpu->stopped) {
        return cpu->stop_reason;
    }

    cpu->last_opcode_address = mipsnes2_address_make(cpu->pbr, cpu->pc);
    cpu->last_opcode = fetch8(cpu);

    switch (cpu->last_opcode) {
    case 0x1b: /* TCS */
        transfer_to_stack(cpu, cpu->a);
        break;
    case 0x18: /* CLC */
        cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x38: /* SEC */
        cpu->p |= MIPSNES2_CPU_FLAG_C;
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x3b: /* TSC */
        transfer16_to_accumulator(cpu, cpu->s);
        break;
    case 0x4c: /* JMP absolute */
        cpu->pc = fetch16(cpu);
        cpu->instruction_cycles += UINT64_C(3);
        break;
    case 0x5b: /* TCD */
        cpu->d = cpu->a;
        set_nz16(cpu, cpu->d);
        cpu->instruction_cycles += UINT64_C(2);
        break;
    case 0x7b: /* TDC */
        transfer16_to_accumulator(cpu, cpu->d);
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
    case 0x8a: /* TXA */
        transfer_to_accumulator(cpu, index_register_value(cpu, cpu->x));
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
    case 0xeb: /* XBA */
        exchange_accumulator_bytes(cpu);
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
