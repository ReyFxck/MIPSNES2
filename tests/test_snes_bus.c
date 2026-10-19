/* MIPSNES2 SNES bus/timing integration tests. See LICENSE for GPL-3.0 terms. */
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/bus/snes_bus.h"
#include "core/cpu/65c816/cpu.h"
#include "core/scheduler/cpu_timing.h"
#include "core/scheduler/master_timing.h"
#include "core/scheduler/scheduler.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

typedef struct {
    unsigned reads;
    unsigned writes;
    mipsnes2_address last_address;
    uint8_t last_value;
    uint8_t read_value;
} route_capture;

typedef struct {
    uint8_t bank00[UINT32_C(0x10000)];
    uint8_t bankc0[UINT32_C(0x10000)];
    route_capture capture;
} cartridge_fixture;

typedef struct {
    mipsnes2_cpu *cpu;
    unsigned calls;
    mipsnes2_master_clock timestamp;
} nmi_context;

static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
static cartridge_fixture cartridge;

static uint8_t capture_read(void *opaque, mipsnes2_address address)
{
    route_capture *capture = opaque;

    capture->reads++;
    capture->last_address = address;
    return capture->read_value;
}

static void capture_write(void *opaque,
                          mipsnes2_address address,
                          uint8_t value)
{
    route_capture *capture = opaque;

    capture->writes++;
    capture->last_address = address;
    capture->last_value = value;
}

static bool cartridge_read(void *opaque,
                           mipsnes2_address address,
                           uint8_t *value)
{
    cartridge_fixture *fixture = opaque;
    uint8_t bank = (uint8_t)(address >> 16);

    fixture->capture.reads++;
    fixture->capture.last_address = address;
    if (bank == 0U) {
        *value = fixture->bank00[(uint16_t)address];
    } else if (bank == UINT8_C(0xc0)) {
        *value = fixture->bankc0[(uint16_t)address];
    } else {
        *value = fixture->capture.read_value;
    }
    return true;
}

static bool cartridge_write(void *opaque,
                            mipsnes2_address address,
                            uint8_t value)
{
    cartridge_fixture *fixture = opaque;

    fixture->capture.writes++;
    fixture->capture.last_address = address;
    fixture->capture.last_value = value;
    return true;
}

static bool capture_cartridge_read(void *opaque,
                                   mipsnes2_address address,
                                   uint8_t *value)
{
    *value = capture_read(opaque, address);
    return true;
}

static bool capture_cartridge_write(void *opaque,
                                    mipsnes2_address address,
                                    uint8_t value)
{
    capture_write(opaque, address, value);
    return true;
}

static mipsnes2_bus_interface capture_interface(route_capture *capture)
{
    mipsnes2_bus_interface interface;

    interface.context = capture;
    interface.read8 = capture_read;
    interface.write8 = capture_write;
    return interface;
}

static mipsnes2_cartridge_interface capture_cartridge_interface(
    route_capture *capture)
{
    mipsnes2_cartridge_interface interface;

    interface.context = capture;
    interface.read8 = capture_cartridge_read;
    interface.write8 = capture_cartridge_write;
    return interface;
}

static mipsnes2_cartridge_interface cartridge_interface(
    cartridge_fixture *fixture)
{
    mipsnes2_cartridge_interface interface;

    interface.context = fixture;
    interface.read8 = cartridge_read;
    interface.write8 = cartridge_write;
    return interface;
}

static mipsnes2_snes_bus_config base_config(void)
{
    mipsnes2_snes_bus_config config;

    (void)memset(&config, 0, sizeof(config));
    config.wram = wram;
    config.wram_size = sizeof(wram);
    config.initial_open_value = UINT8_C(0xa5);
    return config;
}

static int test_init_and_wram_mapping(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus_config config = base_config();
    mipsnes2_snes_bus_config too_small = config;
    mipsnes2_bus_interface interface;

    (void)memset(wram, 0, sizeof(wram));
    too_small.wram_size = sizeof(wram) - 1U;
    CHECK(mipsnes2_snes_bus_init(NULL, &config) ==
          MIPSNES2_SNES_BUS_INVALID_ARGUMENT);
    CHECK(mipsnes2_snes_bus_init(&bus, NULL) ==
          MIPSNES2_SNES_BUS_INVALID_ARGUMENT);
    CHECK(mipsnes2_snes_bus_init(&bus, &too_small) ==
          MIPSNES2_SNES_BUS_INVALID_ARGUMENT);
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_OK);

    interface = mipsnes2_snes_bus_make_interface(&bus);
    CHECK(mipsnes2_snes_bus_interface_matches(&bus, interface));
    CHECK(!mipsnes2_snes_bus_interface_matches(NULL, interface));
    interface = mipsnes2_snes_bus_make_interface(NULL);
    CHECK(interface.context == NULL && interface.read8 == NULL &&
          interface.write8 == NULL);

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x001234), UINT8_C(0x11));
    CHECK(wram[0x1234] == UINT8_C(0x11));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x801234)) ==
          UINT8_C(0x11));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x7effff), UINT8_C(0x22));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x7f0000), UINT8_C(0x33));
    CHECK(wram[0xffff] == UINT8_C(0x22));
    CHECK(wram[0x10000] == UINT8_C(0x33));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x01001234)) ==
          UINT8_C(0x11));
    CHECK(bus.access_count == UINT64_C(5));
    CHECK(bus.master_clocks == UINT64_C(40));
    CHECK(bus.open_value == UINT8_C(0x11));
    return 0;
}

static int test_wram_port_and_fastrom_timing(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus_config config = base_config();
    route_capture cart = {0U, 0U, 0U, 0U, UINT8_C(0x6d)};
    uint64_t before;

    (void)memset(wram, 0, sizeof(wram));
    config.cartridge = capture_cartridge_interface(&cart);
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_OK);

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x002181), UINT8_C(0xfe));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x002182), UINT8_C(0xff));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x002183), UINT8_C(0x01));
    CHECK(bus.wram_port_address == UINT32_C(0x1fffe));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x802180), UINT8_C(0x31));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x002180), UINT8_C(0x32));
    CHECK(wram[0x1fffe] == UINT8_C(0x31));
    CHECK(wram[0x1ffff] == UINT8_C(0x32));
    CHECK(bus.wram_port_address == 0U);
    wram[0] = UINT8_C(0x44);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x002180)) ==
          UINT8_C(0x44));
    CHECK(bus.wram_port_address == 1U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x002181)) ==
          UINT8_C(0x44));

    before = bus.master_clocks;
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x00420d), UINT8_C(0x01));
    CHECK(bus.fastrom_enabled);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0xc01234)) ==
          UINT8_C(0x6d));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0xbf8000)) ==
          UINT8_C(0x6d));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x008000)) ==
          UINT8_C(0x6d));
    CHECK(bus.master_clocks - before == UINT64_C(26));
    CHECK(cart.reads == 3U);

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80420d), UINT8_C(0x02));
    CHECK(!bus.fastrom_enabled);
    before = bus.master_clocks;
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0xc01234)) ==
          UINT8_C(0x6d));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004016)) ==
          UINT8_C(0x6d));
    CHECK(bus.master_clocks - before == UINT64_C(20));

    before = bus.master_clocks;
    CHECK(mipsnes2_snes_bus_account_internal_cycles(&bus, UINT64_C(3)) ==
          MIPSNES2_SNES_BUS_OK);
    CHECK(bus.master_clocks - before == UINT64_C(18));
    bus.master_clocks = UINT64_MAX - UINT64_C(5);
    CHECK(mipsnes2_snes_bus_account_internal_cycles(&bus, UINT64_C(1)) ==
          MIPSNES2_SNES_BUS_TIMING_OVERFLOW);
    CHECK(bus.timing_overflow);
    return 0;
}

static int test_mmio_and_cartridge_routing(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus_config config = base_config();
    route_capture ppu = {0U, 0U, 0U, 0U, UINT8_C(0x10)};
    route_capture apu = {0U, 0U, 0U, 0U, UINT8_C(0x20)};
    route_capture cpu_io = {0U, 0U, 0U, 0U, UINT8_C(0x30)};
    route_capture dma = {0U, 0U, 0U, 0U, UINT8_C(0x40)};
    route_capture cart = {0U, 0U, 0U, 0U, UINT8_C(0x50)};

    config.ppu = capture_interface(&ppu);
    config.apu = capture_interface(&apu);
    config.cpu_io = capture_interface(&cpu_io);
    config.dma = capture_interface(&dma);
    config.cartridge = capture_cartridge_interface(&cart);
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_OK);

    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x002100)) == 0x10U);
    CHECK(ppu.reads == 1U && ppu.last_address == UINT32_C(0x002100));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80213f), UINT8_C(0x91));
    CHECK(ppu.writes == 1U && ppu.last_address == UINT32_C(0x80213f));

    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x002140)) == 0x20U);
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80217f), UINT8_C(0x92));
    CHECK(apu.reads == 1U && apu.writes == 1U);

    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004016)) == 0x30U);
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80421f), UINT8_C(0x93));
    CHECK(cpu_io.reads == 1U && cpu_io.writes == 1U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x00420d)) ==
          UINT8_C(0x93));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80420d), UINT8_C(0x01));
    CHECK(cpu_io.reads == 1U && cpu_io.writes == 1U);
    CHECK(bus.fastrom_enabled);

    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004300)) == 0x40U);
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80437f), UINT8_C(0x94));
    CHECK(dma.reads == 1U && dma.writes == 1U);

    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x002000)) == 0x50U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x002200)) == 0x50U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x006000)) == 0x50U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x008000)) == 0x50U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x401234)) == 0x50U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0xc01234)) == 0x50U);
    CHECK(cart.reads == 6U);

    bus.open_value = UINT8_C(0xa7);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x002184)) ==
          UINT8_C(0xa7));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004380)) ==
          UINT8_C(0xa7));
    CHECK(cart.reads == 6U);
    return 0;
}

static int test_cpu_io_state_routing(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus_config config = base_config();
    mipsnes2_cpu_io cpu_io_state;
    route_capture fallback = {0U, 0U, 0U, 0U, UINT8_C(0x3c)};

    mipsnes2_cpu_io_init(&cpu_io_state, UINT8_C(2));
    config.cpu_io = capture_interface(&fallback);
    config.cpu_io_state = &cpu_io_state;
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_OK);

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x804207), UINT8_C(0x34));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004208), UINT8_C(0x01));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x804209), UINT8_C(0x56));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x00420a), UINT8_C(0x00));
    CHECK(cpu_io_state.htime == UINT16_C(0x0134));
    CHECK(cpu_io_state.vtime == UINT16_C(0x0056));
    CHECK(fallback.writes == 0U);

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004200), UINT8_C(0x80));
    mipsnes2_cpu_io_set_vblank(&cpu_io_state, true);
    mipsnes2_cpu_io_signal_nmi_flag(&cpu_io_state);
    bus.open_value = UINT8_C(0x70);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x804210)) ==
          UINT8_C(0xf2));
    CHECK(!cpu_io_state.nmi_flag);
    CHECK(mipsnes2_cpu_io_take_nmi_edge(&cpu_io_state));

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004201), UINT8_C(0x91));
    CHECK(fallback.writes == 1U &&
          fallback.last_address == UINT32_C(0x004201));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x804213)) ==
          UINT8_C(0x3c));
    CHECK(fallback.reads == 1U &&
          fallback.last_address == UINT32_C(0x804213));

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80420d), UINT8_C(1));
    CHECK(bus.fastrom_enabled);
    CHECK(fallback.writes == 1U);
    return 0;
}

static void pulse_nmi(void *opaque,
                      mipsnes2_master_clock timestamp,
                      mipsnes2_scheduler_token token)
{
    nmi_context *context = opaque;

    (void)token;
    context->calls++;
    context->timestamp = timestamp;
    mipsnes2_cpu_pulse_nmi(context->cpu);
}

static int initialize_cpu_bus_with_io(mipsnes2_snes_bus *bus,
                                      mipsnes2_cpu *cpu,
                                      mipsnes2_cpu_io *cpu_io)
{
    mipsnes2_snes_bus_config config = base_config();

    (void)memset(wram, 0, sizeof(wram));
    (void)memset(&cartridge, 0, sizeof(cartridge));
    cartridge.capture.read_value = UINT8_C(0xa5);
    cartridge.bank00[0xfffc] = 0x00U;
    cartridge.bank00[0xfffd] = 0x80U;
    cartridge.bank00[0xfffa] = 0x00U;
    cartridge.bank00[0xfffb] = 0x90U;
    config.cartridge = cartridge_interface(&cartridge);
    config.cpu_io_state = cpu_io;
    CHECK(mipsnes2_snes_bus_init(bus, &config) == MIPSNES2_SNES_BUS_OK);
    mipsnes2_cpu_init(cpu, mipsnes2_snes_bus_make_interface(bus));
    CHECK(mipsnes2_cpu_reset(cpu) == MIPSNES2_CPU_OK);
    return 0;
}

static int initialize_cpu_bus(mipsnes2_snes_bus *bus,
                              mipsnes2_cpu *cpu)
{
    return initialize_cpu_bus_with_io(bus, cpu, NULL);
}

static int test_timed_cpu_steps(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_report report;
    nmi_context nmi = {&cpu, 0U, 0U};
    uint64_t reset_clocks;

    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    reset_clocks = bus.master_clocks;
    CHECK(reset_clocks == UINT64_C(16));
    mipsnes2_scheduler_init(&scheduler);
    cartridge.bank00[0x8000] = 0xeaU;
    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 8U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(report.cpu_result == MIPSNES2_CPU_OK);
    CHECK(report.processor_cycles == UINT64_C(2));
    CHECK(report.bus_accesses == UINT64_C(1));
    CHECK(report.internal_cycles == UINT64_C(1));
    CHECK(report.master_clocks == UINT64_C(14));
    CHECK(report.target_time == UINT64_C(14));
    CHECK(scheduler.now == UINT64_C(14));

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x00420d), UINT8_C(1));
    cpu.pbr = UINT8_C(0xc0);
    cpu.pc = UINT16_C(0x8000);
    cpu.dbr = UINT8_C(0x7e);
    cartridge.bankc0[0x8000] = 0xadU;
    cartridge.bankc0[0x8001] = 0x34U;
    cartridge.bankc0[0x8002] = 0x12U;
    wram[0x1234] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 8U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.a == UINT16_C(0x0080));
    CHECK(report.processor_cycles == UINT64_C(4));
    CHECK(report.bus_accesses == UINT64_C(4));
    CHECK(report.internal_cycles == 0U);
    CHECK(report.master_clocks == UINT64_C(26));
    CHECK(scheduler.now == UINT64_C(40));

    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    mipsnes2_scheduler_init(&scheduler);
    cartridge.bank00[0x8000] = 0xeaU;
    CHECK(mipsnes2_scheduler_schedule(&scheduler, UINT64_C(8), 0U,
                                      pulse_nmi, &nmi, NULL) ==
          MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 8U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.pc == UINT16_C(0x8001));
    CHECK(nmi.calls == 1U && nmi.timestamp == UINT64_C(8));
    CHECK(cpu.nmi_pending);
    CHECK(report.events_dispatched == 1U);
    CHECK(scheduler.now == UINT64_C(14));

    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 8U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(report.cpu_result == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x9000));
    CHECK(report.processor_cycles == UINT64_C(7));
    CHECK(report.bus_accesses == UINT64_C(5));
    CHECK(report.internal_cycles == UINT64_C(2));
    CHECK(report.master_clocks == UINT64_C(52));
    CHECK(scheduler.now == UINT64_C(66));

    cpu.bus.write8 = NULL;
    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 8U, &report) ==
          MIPSNES2_CPU_TIMING_BUS_MISMATCH);
    CHECK(mipsnes2_cpu_timed_step(NULL, &bus, &scheduler, 8U, &report) ==
          MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 8U, NULL) ==
          MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);
    return 0;
}

static int test_timed_cpu_hv_steps(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv_timing;
    mipsnes2_cpu_timing_report report;
    unsigned step;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(initialize_cpu_bus_with_io(&bus, &cpu, &cpu_io) == 0);
    mipsnes2_scheduler_init(&scheduler);
    CHECK(mipsnes2_hv_timing_init(&hv_timing,
                                  MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4207), UINT8_C(10)));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4208), 0U));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(0x10)));
    for (step = 0U; step < 4U; ++step) {
        cartridge.bank00[0x8000U + step] = UINT8_C(0xea);
        CHECK(mipsnes2_cpu_timed_step_hv(
                  &cpu, &bus, &scheduler, &hv_timing, 8U, &report) ==
              MIPSNES2_CPU_TIMING_OK);
        CHECK(report.cpu_result == MIPSNES2_CPU_OK);
        CHECK(report.master_clocks == UINT64_C(14));
        CHECK(scheduler.now == (uint64_t)(step + 1U) * UINT64_C(14));
        CHECK(hv_timing.elapsed_master_clocks == scheduler.now);
        CHECK(report.target_time == scheduler.now);
        CHECK(report.hv_result == MIPSNES2_HV_OK);
        if (step < 3U) {
            CHECK(!cpu_io.irq_flag);
        }
    }
    CHECK(cpu.pc == UINT16_C(0x8004));
    CHECK(cpu_io.irq_flag);
    CHECK(!cpu.irq_level);
    CHECK(hv_timing.h_master_clock == UINT16_C(56));

    CHECK(mipsnes2_cpu_timed_step_hv(
              &cpu, &bus, &scheduler, NULL, 8U, &report) ==
          MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);

    bus.cpu_io_state = NULL;
    CHECK(mipsnes2_cpu_timed_step_hv(
              &cpu, &bus, &scheduler, &hv_timing, 8U, &report) ==
          MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);
    bus.cpu_io_state = &cpu_io;

    hv_timing.elapsed_master_clocks++;
    CHECK(mipsnes2_cpu_timed_step_hv(
              &cpu, &bus, &scheduler, &hv_timing, 8U, &report) ==
          MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH);
    CHECK(cpu.pc == UINT16_C(0x8004));
    CHECK(scheduler.now == UINT64_C(56));
    CHECK(hv_timing.h_master_clock == UINT16_C(56));
    return 0;
}

static int test_timed_cpu_interrupt_handoff(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_interrupt_lines interrupt_lines;
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv_timing;
    mipsnes2_cpu_timing_report report;
    mipsnes2_master_timing_report master_report;
    uint8_t value;
    unsigned step;
    const uint64_t nmi_time =
        UINT64_C(225) * MIPSNES2_HV_NORMAL_SCANLINE_CLOCKS + UINT64_C(2);

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    mipsnes2_interrupt_lines_init(&interrupt_lines);
    CHECK(initialize_cpu_bus_with_io(&bus, &cpu, &cpu_io) == 0);
    mipsnes2_scheduler_init(&scheduler);
    CHECK(mipsnes2_hv_timing_init(&hv_timing,
                                  MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4207), UINT8_C(10)));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4208), 0U));
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(0x10)));
    for (step = 0U; step < 4U; ++step) {
        cartridge.bank00[0x8000U + step] = UINT8_C(0xea);
        CHECK(mipsnes2_cpu_timed_step_hv_interrupts(
                  &cpu, &bus, &scheduler, &hv_timing, &interrupt_lines,
                  8U, &report) == MIPSNES2_CPU_TIMING_OK);
        CHECK(!report.nmi_delivered);
        CHECK(report.irq_level_delivered == (step == 3U));
    }
    CHECK(cpu_io.irq_flag && cpu.irq_level);
    CHECK(interrupt_lines.irq_sources == MIPSNES2_IRQ_SOURCE_CPU_TIMER);

    CHECK(mipsnes2_cpu_io_read(&cpu_io, UINT16_C(0x4211), 0U, &value));
    CHECK((value & UINT8_C(0x80)) != 0U && !cpu_io.irq_flag);
    CHECK(mipsnes2_interrupt_lines_set_irq(
              &interrupt_lines, MIPSNES2_IRQ_SOURCE_CARTRIDGE, true));
    cartridge.bank00[0x8004] = UINT8_C(0xea);
    CHECK(mipsnes2_cpu_timed_step_hv_interrupts(
              &cpu, &bus, &scheduler, &hv_timing, &interrupt_lines,
              8U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(interrupt_lines.irq_sources == MIPSNES2_IRQ_SOURCE_CARTRIDGE);
    CHECK(cpu.irq_level && report.irq_level_delivered);

    CHECK(mipsnes2_interrupt_lines_set_irq(
              &interrupt_lines, MIPSNES2_IRQ_SOURCE_CARTRIDGE, false));
    cartridge.bank00[0x8005] = UINT8_C(0xea);
    CHECK(mipsnes2_cpu_timed_step_hv_interrupts(
              &cpu, &bus, &scheduler, &hv_timing, &interrupt_lines,
              8U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(interrupt_lines.irq_sources == 0U);
    CHECK(!cpu.irq_level && !report.irq_level_delivered);

    CHECK(mipsnes2_cpu_timed_step_hv_interrupts(
              &cpu, &bus, &scheduler, &hv_timing, NULL, 8U, &report) ==
          MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_timed_step_hv_interrupts(
              &cpu, &bus, &scheduler, NULL, &interrupt_lines, 8U,
              &report) == MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    mipsnes2_interrupt_lines_init(&interrupt_lines);
    CHECK(initialize_cpu_bus_with_io(&bus, &cpu, &cpu_io) == 0);
    mipsnes2_scheduler_init(&scheduler);
    CHECK(mipsnes2_hv_timing_init(&hv_timing,
                                  MIPSNES2_HV_STANDARD_NTSC,
                                  false, &cpu_io) == MIPSNES2_HV_OK);
    CHECK(mipsnes2_cpu_io_write(&cpu_io, UINT16_C(0x4200), UINT8_C(0x80)));
    CHECK(mipsnes2_master_timing_run_until(
              &scheduler, &hv_timing, &cpu_io, nmi_time, 0U,
              &master_report) == MIPSNES2_MASTER_TIMING_OK);
    CHECK(cpu_io.nmi_edge_pending && !cpu.nmi_pending);
    CHECK(mipsnes2_interrupt_lines_set_irq(
              &interrupt_lines, MIPSNES2_IRQ_SOURCE_CARTRIDGE, true));
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_I;
    CHECK(mipsnes2_cpu_timed_step_hv_interrupts(
              &cpu, &bus, &scheduler, &hv_timing, &interrupt_lines,
              8U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(report.nmi_delivered && report.irq_level_delivered);
    CHECK(report.cpu_result == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x9000) && cpu.pbr == 0U);
    CHECK(!cpu.nmi_pending && !interrupt_lines.nmi_pending);
    CHECK(cpu.irq_level);
    CHECK(scheduler.now == nmi_time + UINT64_C(52));
    CHECK(hv_timing.elapsed_master_clocks == scheduler.now);
    return 0;
}

int main(void)
{
    CHECK(test_init_and_wram_mapping() == 0);
    CHECK(test_wram_port_and_fastrom_timing() == 0);
    CHECK(test_mmio_and_cartridge_routing() == 0);
    CHECK(test_cpu_io_state_routing() == 0);
    CHECK(test_timed_cpu_steps() == 0);
    CHECK(test_timed_cpu_hv_steps() == 0);
    CHECK(test_timed_cpu_interrupt_handoff() == 0);
    (void)puts("PASS: SNES bus and timed CPU/H-V/interrupt foundation");
    return 0;
}
