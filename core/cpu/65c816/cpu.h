/* MIPSNES2 65C816 foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_CPU_65C816_H
#define MIPSNES2_CPU_65C816_H

#include <stdbool.h>
#include <stdint.h>

#include "core/bus/interface.h"

enum {
    MIPSNES2_CPU_FLAG_C = 0x01,
    MIPSNES2_CPU_FLAG_Z = 0x02,
    MIPSNES2_CPU_FLAG_I = 0x04,
    MIPSNES2_CPU_FLAG_D = 0x08,
    MIPSNES2_CPU_FLAG_X = 0x10,
    MIPSNES2_CPU_FLAG_M = 0x20,
    MIPSNES2_CPU_FLAG_V = 0x40,
    MIPSNES2_CPU_FLAG_N = 0x80
};

typedef enum {
    MIPSNES2_CPU_OK = 0,
    MIPSNES2_CPU_NOT_READY,
    MIPSNES2_CPU_INVALID_BUS,
    MIPSNES2_CPU_UNIMPLEMENTED_OPCODE,
    MIPSNES2_CPU_WAITING,
    MIPSNES2_CPU_CLOCK_STOPPED
} mipsnes2_cpu_result;

typedef struct {
    uint16_t a;
    uint16_t x;
    uint16_t y;
    uint16_t s;
    uint16_t d;
    uint16_t pc;
    uint8_t dbr;
    uint8_t pbr;
    uint8_t p;
    bool emulation;

    uint64_t instruction_cycles;
    mipsnes2_bus_interface bus;

    bool waiting;
    bool clock_stopped;
    bool irq_level;
    bool abort_pending;
    bool nmi_pending;
    bool stopped;
    mipsnes2_cpu_result stop_reason;
    uint8_t last_opcode;
    mipsnes2_address last_opcode_address;
} mipsnes2_cpu;

void mipsnes2_cpu_init(mipsnes2_cpu *cpu, mipsnes2_bus_interface bus);
mipsnes2_cpu_result mipsnes2_cpu_reset(mipsnes2_cpu *cpu);
mipsnes2_cpu_result mipsnes2_cpu_step(mipsnes2_cpu *cpu);
void mipsnes2_cpu_set_irq(mipsnes2_cpu *cpu, bool asserted);
void mipsnes2_cpu_pulse_abort(mipsnes2_cpu *cpu);
void mipsnes2_cpu_pulse_nmi(mipsnes2_cpu *cpu);

#endif
