/* MIPSNES2 SPC700 execution foundation. See LICENSE for GPL-3.0 terms. */
#include "core/apu/spc700/cpu.h"

#include <stddef.h>

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

    next = cpu->registers;
    switch (opcode) {
    case UINT8_C(0x00): /* NOP */
        cycles = UINT8_C(2);
        break;
    case UINT8_C(0x20): /* CLRP */
        next.psw &= (uint8_t)~MIPSNES2_SPC700_FLAG_P;
        cycles = UINT8_C(2);
        break;
    case UINT8_C(0x40): /* SETP */
        next.psw |= MIPSNES2_SPC700_FLAG_P;
        cycles = UINT8_C(2);
        break;
    case UINT8_C(0x60): /* CLRC */
        next.psw &= (uint8_t)~MIPSNES2_SPC700_FLAG_C;
        cycles = UINT8_C(2);
        break;
    case UINT8_C(0x80): /* SETC */
        next.psw |= MIPSNES2_SPC700_FLAG_C;
        cycles = UINT8_C(2);
        break;
    case UINT8_C(0xa0): /* EI */
        next.psw |= MIPSNES2_SPC700_FLAG_I;
        cycles = UINT8_C(3);
        break;
    case UINT8_C(0xc0): /* DI */
        next.psw &= (uint8_t)~MIPSNES2_SPC700_FLAG_I;
        cycles = UINT8_C(3);
        break;
    case UINT8_C(0xe0): /* CLRV also clears half carry. */
        next.psw &= (uint8_t)~(MIPSNES2_SPC700_FLAG_V |
                               MIPSNES2_SPC700_FLAG_H);
        cycles = UINT8_C(2);
        break;
    case UINT8_C(0xed): /* NOTC */
        next.psw ^= MIPSNES2_SPC700_FLAG_C;
        cycles = UINT8_C(3);
        break;
    default:
        return MIPSNES2_SPC700_UNIMPLEMENTED_OPCODE;
    }

    if (cpu->instruction_cycles > UINT64_MAX - cycles) {
        return MIPSNES2_SPC700_CYCLE_OVERFLOW;
    }
    if (!mipsnes2_smp_memory_advance(cpu->memory, cycles)) {
        return MIPSNES2_SPC700_MEMORY_UNAVAILABLE;
    }

    next.pc = (uint16_t)(next.pc + UINT16_C(1));
    cpu->registers = next;
    cpu->instruction_cycles += cycles;
    return MIPSNES2_SPC700_OK;
}
