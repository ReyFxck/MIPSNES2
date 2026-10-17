/* MIPSNES2 SNES bus/timing integration tests. See LICENSE for GPL-3.0 terms. */
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/bus/snes_bus.h"
#include "core/cpu/65c816/cpu.h"
#include "core/scheduler/cpu_timing.h"
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

static uint8_t cartridge_read(void *opaque, mipsnes2_address address)
{
    cartridge_fixture *fixture = opaque;
    uint8_t bank = (uint8_t)(address >> 16);

    fixture->capture.reads++;
    fixture->capture.last_address = address;
    if (bank == 0U) {
        return fixture->bank00[(uint16_t)address];
    }
    if (bank == UINT8_C(0xc0)) {
        return fixture->bankc0[(uint16_t)address];
    }
    return fixture->capture.read_value;
}

static void cartridge_write(void *opaque,
                            mipsnes2_address address,
                            uint8_t value)
{
    cartridge_fixture *fixture = opaque;

    fixture->capture.writes++;
    fixture->capture.last_address = address;
    fixture->capture.last_value = value;
}

static mipsnes2_bus_interface capture_interface(route_capture *capture)
{
    mipsnes2_bus_interface interface;

    interface.context = capture;
    interface.read8 = capture_read;
    interface.write8 = capture_write;
    return interface;
}

static mipsnes2_bus_interface cartridge_interface(
    cartridge_fixture *fixture)
{
    mipsnes2_bus_interface interface;

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
    config.cartridge = capture_interface(&cart);
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
    config.cartridge = capture_interface(&cart);
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

static int initialize_cpu_bus(mipsnes2_snes_bus *bus,
                              mipsnes2_cpu *cpu)
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
    CHECK(mipsnes2_snes_bus_init(bus, &config) == MIPSNES2_SNES_BUS_OK);
    mipsnes2_cpu_init(cpu, mipsnes2_snes_bus_make_interface(bus));
    CHECK(mipsnes2_cpu_reset(cpu) == MIPSNES2_CPU_OK);
    return 0;
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

int main(void)
{
    CHECK(test_init_and_wram_mapping() == 0);
    CHECK(test_wram_port_and_fastrom_timing() == 0);
    CHECK(test_mmio_and_cartridge_routing() == 0);
    CHECK(test_timed_cpu_steps() == 0);
    (void)puts("PASS: SNES WRAM/MMIO routing and timed CPU step foundation");
    return 0;
}
