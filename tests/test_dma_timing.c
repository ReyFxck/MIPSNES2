/* Original synthetic DMA bus/timing fixture. See LICENSE. */
#include <stdio.h>
#include <string.h>
#include "core/scheduler/cpu_timing.h"

#define CHECK(x) do { if (!(x)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; \
} } while (0)

typedef struct {
    uint8_t wram[MIPSNES2_SNES_WRAM_SIZE], rom[65536];
    mipsnes2_dma_registers registers;
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_io io;
    mipsnes2_hv_timing hv;
    mipsnes2_interrupt_lines lines;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_state state;
    uint16_t ports[16];
    uint8_t values[16];
    unsigned writes, reads, observed_writes, events;
} fixture;
static fixture f;

static bool read_rom(void *context, mipsnes2_address address, uint8_t *value)
{
    fixture *instance = context;
    *value = instance->rom[(uint16_t)address];
    return true;
}
static uint8_t read_ppu(void *context, mipsnes2_address address)
{
    fixture *instance = context;
    (void)address;
    ++instance->reads;
    return UINT8_C(0xa5);
}
static void write_ppu(void *context, mipsnes2_address address, uint8_t value)
{
    fixture *instance = context;
    if (instance->writes < 16U) {
        instance->ports[instance->writes] = (uint16_t)address;
        instance->values[instance->writes] = value;
    }
    ++instance->writes;
}
static void observe(void *context, mipsnes2_master_clock timestamp,
                     mipsnes2_scheduler_token token)
{
    fixture *instance = context;
    (void)timestamp;
    (void)token;
    instance->observed_writes = instance->writes;
    ++instance->events;
}
static void latch_nmi(void *context, mipsnes2_master_clock timestamp,
                       mipsnes2_scheduler_token token)
{
    fixture *instance = context;
    (void)timestamp;
    (void)token;
    mipsnes2_interrupt_lines_latch_nmi(&instance->lines);
}
static int setup(void)
{
    mipsnes2_snes_bus_config config = {0};
    (void)memset(&f, 0, sizeof(f));
    mipsnes2_dma_registers_init(&f.registers);
    mipsnes2_cpu_io_init(&f.io, 2U);
    CHECK(mipsnes2_hv_timing_init(&f.hv, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &f.io) == MIPSNES2_HV_OK);
    config.wram = f.wram;
    config.wram_size = sizeof(f.wram);
    config.cpu_io_state = &f.io;
    config.dma_registers = &f.registers;
    config.ppu.context = &f;
    config.ppu.read8 = read_ppu;
    config.ppu.write8 = write_ppu;
    config.cartridge.context = &f;
    config.cartridge.read8 = read_rom;
    CHECK(mipsnes2_snes_bus_init(&f.bus, &config) == MIPSNES2_SNES_BUS_OK);
    mipsnes2_cpu_init(&f.cpu, mipsnes2_snes_bus_make_interface(&f.bus));
    CHECK(mipsnes2_cpu_reset(&f.cpu) == MIPSNES2_CPU_OK);
    f.bus.access_count = 0U;
    f.bus.master_clocks = 0U;
    mipsnes2_scheduler_init(&f.scheduler);
    mipsnes2_interrupt_lines_init(&f.lines);
    mipsnes2_cpu_timing_state_init(&f.state);
    /* STA $420B; NOP. The extra cycle is the NOP opcode fetch. */
    f.wram[0] = UINT8_C(0x8d);
    f.wram[1] = UINT8_C(0x0b);
    f.wram[2] = UINT8_C(0x42);
    f.wram[3] = UINT8_C(0xea);
    f.cpu.a = 1U;
    f.registers.channel[0][0] = 1U;
    f.registers.channel[0][1] = UINT8_C(0x18);
    f.registers.channel[0][2] = 0U;
    f.registers.channel[0][3] = UINT8_C(0x80);
    f.registers.channel[0][4] = 0U;
    f.registers.channel[0][5] = 4U;
    f.registers.channel[0][6] = 0U;
    f.rom[0x8000] = UINT8_C(0x12);
    f.rom[0x8001] = UINT8_C(0x34);
    f.rom[0x8002] = UINT8_C(0x56);
    f.rom[0x8003] = UINT8_C(0x78);
    return 0;
}
static mipsnes2_cpu_timing_result resume(uint64_t horizon, size_t budget,
                                         mipsnes2_cpu_timing_report *report)
{
    return mipsnes2_cpu_timed_resume_hv_interrupts(&f.cpu, &f.bus,
        &f.scheduler, &f.hv, &f.lines, &f.state, horizon, budget, report);
}
static int test_horizons(void)
{
    mipsnes2_cpu_timing_report report;
    mipsnes2_cpu paused_cpu;
    CHECK(setup() == 0);
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(f.scheduler.now == 30U && f.cpu.pc == 3U);
    CHECK(f.bus.dma_request_mask == 1U && f.bus.dma_grace_cycles == 1U);
    CHECK(f.writes == 0U && report.dma_bytes == 0U);
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 64U, 0U, observe,
                                       &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(resume(63U, 0U, &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(f.cpu.pc == 4U && f.writes == 0U && f.state.dma_active);
    CHECK(f.scheduler.now == 63U && f.hv.elapsed_master_clocks == 63U);
    paused_cpu = f.cpu;
    CHECK(resume(64U, 0U, &report) == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(report.scheduler_result == MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED);
    CHECK(f.writes == 0U && f.events == 0U);
    CHECK(resume(64U, 1U, &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(f.events == 1U && f.observed_writes == 0U && f.writes == 1U);
    CHECK(f.values[0] == UINT8_C(0x12) && f.ports[0] == UINT16_C(0x2118));
    CHECK(f.registers.channel[0][2] == 1U && f.registers.channel[0][5] == 3U);
    CHECK(report.dma_bytes == 1U && report.bus_accesses == 1U);
    CHECK(resume(64U, 0U, &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(f.writes == 1U);
    CHECK(resume(91U, 0U, &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(f.writes == 4U && f.state.dma_active);
    CHECK(memcmp(&f.cpu, &paused_cpu, sizeof(paused_cpu)) == 0);
    CHECK(f.ports[1] == UINT16_C(0x2119) && f.ports[2] == UINT16_C(0x2118));
    CHECK(f.values[3] == UINT8_C(0x78));
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(f.scheduler.now == 98U && f.hv.elapsed_master_clocks == 98U);
    CHECK(report.dma_master_clocks == 54U && report.master_clocks == 68U);
    CHECK(report.dma_bytes == 4U && report.processor_cycles == 2U);
    CHECK(report.bus_accesses == 1U && f.bus.access_count == 5U);
    CHECK(f.bus.master_clocks == 44U && !f.state.dma_active);
    CHECK(f.registers.channel[0][5] == 0U && f.registers.channel[0][2] == 4U);
    CHECK(setup() == 0);
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(f.scheduler.now == 98U && f.writes == 4U);
    CHECK(report.dma_master_clocks == 54U && report.dma_bytes == 4U);
    return 0;
}
static int test_bus_policy(void)
{
    CHECK(setup() == 0);
    mipsnes2_snes_bus_dma_byte(&f.bus, UINT32_C(0x008000), 0x2118U, false);
    CHECK(f.writes == 1U && f.values[0] == UINT8_C(0x12));
    mipsnes2_snes_bus_dma_byte(&f.bus, UINT32_C(0x7e1000), 0x2139U, true);
    CHECK(f.wram[0x1000] == UINT8_C(0xa5) && f.reads == 1U);
    f.bus.open_value = UINT8_C(0x5a);
    mipsnes2_snes_bus_dma_byte(&f.bus, UINT32_C(0x802139), 0x2118U, false);
    CHECK(f.values[1] == UINT8_C(0x5a) && f.reads == 1U);
    mipsnes2_snes_bus_dma_byte(&f.bus, UINT32_C(0x004300), 0x2139U, true);
    CHECK(f.registers.channel[0][0] == 1U);
    mipsnes2_snes_bus_dma_byte(&f.bus, UINT32_C(0x80420b), 0x2139U, true);
    CHECK(f.bus.dma_request_mask == 0U);
    f.wram[0x1200] = UINT8_C(0x33);
    f.bus.wram_port_address = UINT32_C(0x1000);
    mipsnes2_snes_bus_dma_byte(&f.bus, UINT32_C(0x001200), 0x2180U, false);
    CHECK(f.bus.wram_port_address == UINT32_C(0x1000));
    CHECK(f.wram[0x1000] == UINT8_C(0xa5));
    f.bus.open_value = UINT8_C(0x6b);
    mipsnes2_snes_bus_dma_byte(&f.bus, UINT32_C(0x7f1200), 0x2180U, true);
    CHECK(f.wram[0x11200] == UINT8_C(0x6b));
    CHECK(f.bus.wram_port_address == UINT32_C(0x1000));
    mipsnes2_snes_bus_dma_byte(&f.bus, UINT32_C(0x008001), 0x2180U, false);
    CHECK(f.wram[0x1000] == UINT8_C(0x34));
    CHECK(f.bus.wram_port_address == UINT32_C(0x1001));
    CHECK(f.bus.access_count == 0U && f.bus.master_clocks == 0U);
    return 0;
}
static int test_clock_classes_and_channels(void)
{
    mipsnes2_cpu_timing_report report;
    unsigned fast;

    for (fast = 0U; fast < 2U; ++fast) {
        CHECK(setup() == 0);
        f.cpu.pbr = UINT8_C(0xc0);
        f.cpu.pc = UINT16_C(0x9000);
        f.rom[0x9000] = UINT8_C(0xea);
        f.bus.fastrom_enabled = fast != 0U;
        /* Represents a request after its grace cycle already completed. */
        f.bus.dma_request_mask = 1U;
        CHECK(resume(fast != 0U ? 60U : 64U, 0U, &report) ==
              MIPSNES2_CPU_TIMING_HORIZON_REACHED);
        CHECK(f.writes == 4U && report.dma_bytes == 4U);
        CHECK(report.dma_master_clocks == (fast != 0U ? 60U : 64U));
        CHECK(report.processor_cycles == 0U && report.bus_accesses == 0U);
    }
    CHECK(setup() == 0);
    f.cpu.pc = UINT16_C(0x4016); /* next CPU bus cycle costs 12 clocks */
    f.bus.dma_request_mask = 1U;
    CHECK(resume(60U, 0U, &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(report.dma_master_clocks == 60U && report.processor_cycles == 0U);
    CHECK(f.state.dma_cpu_clocks == 12U);
    /* A second channel incurs its own eight-clock setup, then reverses a
     * byte into WRAM. No extra CPU accesses appear in the counters. */
    CHECK(setup() == 0);
    (void)memcpy(f.registers.channel[2], f.registers.channel[0], 12U);
    f.registers.channel[2][0] = UINT8_C(0x80);
    f.registers.channel[2][1] = UINT8_C(0x39);
    f.registers.channel[2][2] = 0U;
    f.registers.channel[2][3] = UINT8_C(0x10);
    f.registers.channel[2][4] = UINT8_C(0x7e);
    f.registers.channel[2][5] = 1U;
    f.cpu.a = 5U;
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(f.writes == 4U && f.reads == 1U);
    CHECK(f.wram[0x1000] == UINT8_C(0xa5));
    CHECK(report.dma_bytes == 5U && report.dma_master_clocks == 72U);
    CHECK(f.scheduler.now == 116U && report.bus_accesses == 1U);
    /* A mutated CPU during a paused DMA slot is rejected before a byte. */
    CHECK(setup() == 0);
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(resume(63U, 0U, &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    ++f.cpu.a;
    CHECK(resume(64U, 0U, &report) == MIPSNES2_CPU_TIMING_STALE_STATE);
    CHECK(f.writes == 0U);
    return 0;
}
static int test_interrupt_and_overflow(void)
{
    mipsnes2_cpu_timing_report report;

    CHECK(setup() == 0);
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 72U, 0U, latch_nmi,
                                       &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(resume(80U, 1U, &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(f.lines.nmi_pending && !f.cpu.nmi_pending);
    CHECK(f.writes == 3U && f.cpu.pc == 4U);
    CHECK(resume(UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(report.nmi_delivered && f.cpu.nmi_pending && !f.lines.nmi_pending);
    CHECK(f.scheduler.now == 98U && f.writes == 4U);
    CHECK(setup() == 0);
    f.scheduler.now = UINT64_MAX - 4U;
    f.bus.dma_request_mask = 1U;
    CHECK(mipsnes2_cpu_timed_resume(&f.cpu, &f.bus, &f.scheduler,
        &f.state, UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW);
    CHECK(f.writes == 0U && f.bus.access_count == 0U);
    return 0;
}
int main(void)
{
    CHECK(test_horizons() == 0);
    CHECK(test_bus_policy() == 0);
    CHECK(test_clock_classes_and_channels() == 0);
    CHECK(test_interrupt_and_overflow() == 0);
    (void)puts("DMA bus and resumable CPU stall: OK");
    return 0;
}
