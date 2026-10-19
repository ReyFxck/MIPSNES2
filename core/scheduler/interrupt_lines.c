/* MIPSNES2 interrupt-line aggregation foundation. See LICENSE for GPL-3.0 terms. */
#include "core/scheduler/interrupt_lines.h"

#include <string.h>

void mipsnes2_interrupt_lines_init(mipsnes2_interrupt_lines *lines)
{
    if (lines != NULL) {
        (void)memset(lines, 0, sizeof(*lines));
    }
}

bool mipsnes2_interrupt_lines_set_irq(mipsnes2_interrupt_lines *lines,
                                      uint32_t source_mask,
                                      bool asserted)
{
    if (lines == NULL || source_mask == 0U) {
        return false;
    }
    if (asserted) {
        lines->irq_sources |= source_mask;
    } else {
        lines->irq_sources &= ~source_mask;
    }
    return true;
}

void mipsnes2_interrupt_lines_latch_nmi(mipsnes2_interrupt_lines *lines)
{
    if (lines != NULL) {
        lines->nmi_pending = true;
    }
}

bool mipsnes2_interrupt_lines_sample_cpu_io(
    mipsnes2_interrupt_lines *lines,
    mipsnes2_cpu_io *cpu_io)
{
    if (lines == NULL || cpu_io == NULL) {
        return false;
    }
    (void)mipsnes2_interrupt_lines_set_irq(
        lines, MIPSNES2_IRQ_SOURCE_CPU_TIMER,
        mipsnes2_cpu_io_irq_asserted(cpu_io));
    if (mipsnes2_cpu_io_take_nmi_edge(cpu_io)) {
        mipsnes2_interrupt_lines_latch_nmi(lines);
    }
    return true;
}

bool mipsnes2_interrupt_lines_drive_cpu(
    mipsnes2_interrupt_lines *lines,
    mipsnes2_cpu *cpu,
    bool *nmi_delivered)
{
    if (nmi_delivered != NULL) {
        *nmi_delivered = false;
    }
    if (lines == NULL || cpu == NULL) {
        return false;
    }
    mipsnes2_cpu_set_irq(cpu, lines->irq_sources != 0U);
    if (lines->nmi_pending) {
        mipsnes2_cpu_pulse_nmi(cpu);
        lines->nmi_pending = false;
        if (nmi_delivered != NULL) {
            *nmi_delivered = true;
        }
    }
    return true;
}
