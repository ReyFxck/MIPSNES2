/* MIPSNES2 interrupt-line aggregation tests. See LICENSE for GPL-3.0 terms. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/bus/cpu_io.h"
#include "core/cpu/65c816/cpu.h"
#include "core/scheduler/interrupt_lines.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static int test_source_aggregation(void)
{
    mipsnes2_interrupt_lines lines;
    mipsnes2_cpu cpu;
    bool delivered = true;

    (void)memset(&lines, 0xff, sizeof(lines));
    (void)memset(&cpu, 0, sizeof(cpu));
    mipsnes2_interrupt_lines_init(&lines);
    CHECK(lines.irq_sources == 0U && !lines.nmi_pending);
    CHECK(!mipsnes2_interrupt_lines_set_irq(NULL, 1U, true));
    CHECK(!mipsnes2_interrupt_lines_set_irq(&lines, 0U, true));
    CHECK(mipsnes2_interrupt_lines_set_irq(
              &lines, MIPSNES2_IRQ_SOURCE_CARTRIDGE, true));
    CHECK(mipsnes2_interrupt_lines_set_irq(
              &lines, MIPSNES2_IRQ_SOURCE_EXPANSION, true));
    CHECK(lines.irq_sources ==
          (MIPSNES2_IRQ_SOURCE_CARTRIDGE |
           MIPSNES2_IRQ_SOURCE_EXPANSION));
    CHECK(mipsnes2_interrupt_lines_set_irq(
              &lines, MIPSNES2_IRQ_SOURCE_CARTRIDGE, false));
    CHECK(lines.irq_sources == MIPSNES2_IRQ_SOURCE_EXPANSION);

    mipsnes2_interrupt_lines_latch_nmi(&lines);
    CHECK(lines.nmi_pending);
    CHECK(mipsnes2_interrupt_lines_drive_cpu(&lines, &cpu, &delivered));
    CHECK(delivered && cpu.nmi_pending && cpu.irq_level);
    CHECK(!lines.nmi_pending);
    CHECK(mipsnes2_interrupt_lines_drive_cpu(&lines, &cpu, &delivered));
    CHECK(!delivered && cpu.nmi_pending && cpu.irq_level);

    CHECK(mipsnes2_interrupt_lines_set_irq(
              &lines, MIPSNES2_IRQ_SOURCE_EXPANSION, false));
    CHECK(mipsnes2_interrupt_lines_drive_cpu(&lines, &cpu, NULL));
    CHECK(!cpu.irq_level);
    CHECK(!mipsnes2_interrupt_lines_drive_cpu(NULL, &cpu, NULL));
    CHECK(!mipsnes2_interrupt_lines_drive_cpu(&lines, NULL, NULL));
    return 0;
}

static int test_cpu_io_sampling_preserves_other_sources(void)
{
    mipsnes2_interrupt_lines lines;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_cpu cpu;
    uint8_t value;
    bool delivered;

    mipsnes2_interrupt_lines_init(&lines);
    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    (void)memset(&cpu, 0, sizeof(cpu));
    CHECK(mipsnes2_interrupt_lines_set_irq(
              &lines, MIPSNES2_IRQ_SOURCE_CARTRIDGE, true));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200),
                                UINT8_C(0x90)));
    mipsnes2_cpu_io_signal_timer_irq(&cpu_io);
    mipsnes2_cpu_io_signal_nmi_flag(&cpu_io);
    CHECK(cpu_io.irq_flag && cpu_io.nmi_edge_pending);

    CHECK(!mipsnes2_interrupt_lines_sample_cpu_io(NULL, &cpu_io));
    CHECK(!mipsnes2_interrupt_lines_sample_cpu_io(&lines, NULL));
    CHECK(mipsnes2_interrupt_lines_sample_cpu_io(&lines, &cpu_io));
    CHECK(lines.irq_sources ==
          (MIPSNES2_IRQ_SOURCE_CPU_TIMER |
           MIPSNES2_IRQ_SOURCE_CARTRIDGE));
    CHECK(lines.nmi_pending && !cpu_io.nmi_edge_pending);
    CHECK(mipsnes2_interrupt_lines_sample_cpu_io(&lines, &cpu_io));
    CHECK(lines.nmi_pending);

    CHECK(mipsnes2_interrupt_lines_drive_cpu(&lines, &cpu, &delivered));
    CHECK(delivered && cpu.nmi_pending && cpu.irq_level);
    CHECK(mipsnes2_cpu_io_read(&cpu_io, UINT16_C(0x4211), 0U, &value));
    CHECK((value & UINT8_C(0x80)) != 0U && !cpu_io.irq_flag);
    CHECK(mipsnes2_interrupt_lines_sample_cpu_io(&lines, &cpu_io));
    CHECK(lines.irq_sources == MIPSNES2_IRQ_SOURCE_CARTRIDGE);
    CHECK(mipsnes2_interrupt_lines_drive_cpu(&lines, &cpu, NULL));
    CHECK(cpu.irq_level);

    CHECK(mipsnes2_interrupt_lines_set_irq(
              &lines, MIPSNES2_IRQ_SOURCE_CARTRIDGE, false));
    CHECK(mipsnes2_interrupt_lines_drive_cpu(&lines, &cpu, NULL));
    CHECK(!cpu.irq_level);
    return 0;
}

int main(void)
{
    CHECK(test_source_aggregation() == 0);
    CHECK(test_cpu_io_sampling_preserves_other_sources() == 0);
    (void)puts("PASS: CPU interrupt-line aggregation and CPU-I/O handoff");
    return 0;
}
