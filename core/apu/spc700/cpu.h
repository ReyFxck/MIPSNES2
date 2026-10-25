/* MIPSNES2 SPC700 execution foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_SPC700_CPU_H
#define MIPSNES2_SPC700_CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "core/apu/spc700/memory.h"

enum {
    MIPSNES2_SPC700_FLAG_C = 0x01,
    MIPSNES2_SPC700_FLAG_Z = 0x02,
    MIPSNES2_SPC700_FLAG_I = 0x04,
    MIPSNES2_SPC700_FLAG_H = 0x08,
    MIPSNES2_SPC700_FLAG_B = 0x10,
    MIPSNES2_SPC700_FLAG_P = 0x20,
    MIPSNES2_SPC700_FLAG_V = 0x40,
    MIPSNES2_SPC700_FLAG_N = 0x80
};

typedef struct {
    uint16_t pc;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    uint8_t psw;
} mipsnes2_spc700_registers;

typedef enum {
    MIPSNES2_SPC700_OK = 0,
    MIPSNES2_SPC700_INVALID_ARGUMENT,
    MIPSNES2_SPC700_NOT_READY,
    MIPSNES2_SPC700_MEMORY_UNAVAILABLE,
    MIPSNES2_SPC700_UNIMPLEMENTED_OPCODE,
    MIPSNES2_SPC700_CYCLE_OVERFLOW
} mipsnes2_spc700_result;

/* Caller-owned execution state. The memory object is borrowed and must remain
 * at a stable address. Initialization copies caller-selected registers instead
 * of claiming undocumented hardware power-on values. */
typedef struct {
    mipsnes2_spc700_registers registers;
    mipsnes2_smp_memory *memory;
    uint64_t instruction_cycles;
    uint16_t last_opcode_address;
    uint8_t last_opcode;
    bool has_fetched_opcode;
    bool initialized;
} mipsnes2_spc700;

bool mipsnes2_spc700_init(mipsnes2_spc700 *cpu,
                          mipsnes2_smp_memory *memory,
                          const mipsnes2_spc700_registers *registers);

/* Executes one complete supported instruction atomically. A rejected opcode or
 * unavailable operand is recorded for diagnostics but does not change
 * programmer-visible registers, the cycle total or timers. Memory-mapped fetch
 * side effects remain those of mipsnes2_smp_memory_read. */
mipsnes2_spc700_result mipsnes2_spc700_step(mipsnes2_spc700 *cpu);

#endif
