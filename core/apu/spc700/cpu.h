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
    MIPSNES2_SPC700_CYCLE_OVERFLOW,
    MIPSNES2_SPC700_HALTED
} mipsnes2_spc700_result;

typedef enum {
    MIPSNES2_SPC700_RUNNING = 0,
    MIPSNES2_SPC700_SLEEPING,
    MIPSNES2_SPC700_STOPPED
} mipsnes2_spc700_wait_state;

/* Caller-owned execution state. The memory object is borrowed and must remain
 * at a stable address. Initialization copies caller-selected registers instead
 * of claiming undocumented hardware power-on values. */
typedef struct {
    mipsnes2_spc700_registers registers;
    mipsnes2_smp_memory *memory;
    uint64_t instruction_cycles;
    uint16_t last_opcode_address;
    uint16_t prepared_opcode_address;
    uint8_t last_opcode;
    uint8_t prepared_opcode;
    mipsnes2_spc700_wait_state wait_state;
    bool has_fetched_opcode;
    bool opcode_prepared;
    bool initialized;
} mipsnes2_spc700;

bool mipsnes2_spc700_init(mipsnes2_spc700 *cpu,
                          mipsnes2_smp_memory *memory,
                          const mipsnes2_spc700_registers *registers);

/* Executes one complete instruction atomically. An unavailable operand/target
 * is recorded for diagnostics but does not change
 * programmer-visible registers, the cycle total or timers. Successful mapped
 * reads/writes retain the side effects owned by mipsnes2_smp_memory_read/write;
 * exact per-cycle bus ordering is not represented by this boundary. SLEEP and
 * STOP commit their documented three cycles and then return HALTED on later
 * calls until the caller reinitializes the CPU. */
mipsnes2_spc700_result mipsnes2_spc700_step(mipsnes2_spc700 *cpu);

/* Fetches and caches the next opcode, returning a conservative complete-
 * instruction cycle requirement. This lets an external clock domain admit an
 * atomic instruction without reading side-effectful MMIO twice. */
mipsnes2_spc700_result mipsnes2_spc700_prepare_step(
    mipsnes2_spc700 *cpu, uint8_t *maximum_cycles);

/* Executes one instruction but leaves S-SMP timer/DSP time to the caller. */
mipsnes2_spc700_result mipsnes2_spc700_step_clocked(mipsnes2_spc700 *cpu);

#endif
