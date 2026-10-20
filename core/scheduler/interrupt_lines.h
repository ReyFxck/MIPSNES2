/* MIPSNES2 interrupt-line aggregation foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_INTERRUPT_LINES_H
#define MIPSNES2_INTERRUPT_LINES_H

#include <stdbool.h>
#include <stdint.h>

#include "core/bus/cpu_io.h"
#include "core/cpu/65c816/cpu.h"

enum {
    MIPSNES2_IRQ_SOURCE_CPU_TIMER = 1U << 0,
    MIPSNES2_IRQ_SOURCE_CARTRIDGE = 1U << 1,
    MIPSNES2_IRQ_SOURCE_EXPANSION = 1U << 2
};

typedef struct {
    uint32_t irq_sources;
    bool nmi_pending;
} mipsnes2_interrupt_lines;

void mipsnes2_interrupt_lines_init(mipsnes2_interrupt_lines *lines);
bool mipsnes2_interrupt_lines_set_irq(mipsnes2_interrupt_lines *lines,
                                      uint32_t source_mask,
                                      bool asserted);
void mipsnes2_interrupt_lines_latch_nmi(mipsnes2_interrupt_lines *lines);
bool mipsnes2_interrupt_lines_sample_cpu_io(
    mipsnes2_interrupt_lines *lines,
    mipsnes2_cpu_io *cpu_io);
bool mipsnes2_interrupt_lines_drive_cpu(
    mipsnes2_interrupt_lines *lines,
    mipsnes2_cpu *cpu,
    bool *nmi_delivered);

#endif
