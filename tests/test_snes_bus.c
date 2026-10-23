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

typedef struct {
    mipsnes2_cpu *cpu;
    mipsnes2_snes_bus *bus;
    unsigned calls;
    mipsnes2_master_clock timestamp;
    uint16_t pc;
    uint64_t processor_cycles;
    uint64_t bus_accesses;
    uint64_t bus_clocks;
} timed_observer;

typedef struct {
    mipsnes2_interrupt_lines *lines;
    bool nmi;
    unsigned calls;
} interrupt_event_context;

typedef struct {
    mipsnes2_cpu_timing_state *state;
    unsigned calls;
    mipsnes2_cpu_timing_result result;
} abort_event_context;

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
    CHECK(fallback.writes == 0U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x804213)) ==
          UINT8_C(0x91));
    CHECK(fallback.reads == 0U);

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80420d), UINT8_C(1));
    CHECK(bus.fastrom_enabled);
    CHECK(fallback.writes == 0U);

    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &cpu_io_state, 0U, UINT16_C(0x8124)));
    CHECK(mipsnes2_cpu_io_write(
        &cpu_io_state, UINT16_C(0x4200), UINT8_C(1)));
    CHECK(mipsnes2_cpu_io_begin_auto_joypad_read(&cpu_io_state));
    CHECK(mipsnes2_cpu_io_complete_auto_joypad_read(&cpu_io_state));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004218)) ==
          UINT8_C(0x24));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x804219)) ==
          UINT8_C(0x81));
    CHECK(fallback.reads == 0U);

    CHECK(mipsnes2_cpu_io_set_joypad_input(
        &cpu_io_state, 0U, UINT16_C(0x8000)));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004016), UINT8_C(1));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x804016), 0U);
    bus.open_value = UINT8_C(0x54);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004016)) ==
          UINT8_C(0x55));
    CHECK(fallback.reads == 0U && fallback.writes == 0U);

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004201), UINT8_C(0x5a));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x804213)) ==
          UINT8_C(0x5a));
    CHECK(fallback.reads == 0U && fallback.writes == 0U);
    return 0;
}

static int test_cpu_io_math_bus_cycles(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus_config config = base_config();
    mipsnes2_cpu_io cpu_io;
    unsigned cycle;

    (void)memset(wram, 0, sizeof(wram));
    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    config.cpu_io_state = &cpu_io;
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_OK);

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004202), UINT8_C(6));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x804203), UINT8_C(7));
    CHECK(cpu_io.math_cycles_remaining == UINT8_C(8));
    for (cycle = 0U; cycle < 7U; ++cycle) {
        (void)mipsnes2_snes_bus_read(&bus, UINT32_C(0x7e0000));
    }
    CHECK(cpu_io.math_cycles_remaining == UINT8_C(1));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004216)) == 0U);
    CHECK(!mipsnes2_cpu_io_math_busy(&cpu_io));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x804216)) ==
          UINT8_C(42));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004217)) == 0U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004214)) ==
          UINT8_C(7));

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004204), UINT8_C(0x34));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004205), UINT8_C(0x12));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004206), UINT8_C(0x11));
    CHECK(cpu_io.math_cycles_remaining == UINT8_C(16));
    CHECK(mipsnes2_snes_bus_account_internal_cycles(
              &bus, UINT64_C(15)) == MIPSNES2_SNES_BUS_OK);
    CHECK(cpu_io.math_cycles_remaining == UINT8_C(1));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004214)) ==
          UINT8_C(7));
    CHECK(!mipsnes2_cpu_io_math_busy(&cpu_io));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004214)) ==
          UINT8_C(0x12));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004215)) ==
          UINT8_C(0x01));
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x004216)) ==
          UINT8_C(0x02));

    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004206), 0U);
    CHECK(cpu_io.math_cycles_remaining == UINT8_C(16));
    bus.master_clocks = UINT64_MAX - UINT64_C(5);
    CHECK(mipsnes2_snes_bus_account_internal_cycles(
              &bus, UINT64_C(1)) ==
          MIPSNES2_SNES_BUS_TIMING_OVERFLOW);
    CHECK(cpu_io.math_cycles_remaining == UINT8_C(16));
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

static void observe_timed_state(void *opaque,
                                mipsnes2_master_clock timestamp,
                                mipsnes2_scheduler_token token)
{
    timed_observer *observer = opaque;

    (void)token;
    observer->calls++;
    observer->timestamp = timestamp;
    observer->pc = observer->cpu->pc;
    observer->processor_cycles = observer->cpu->instruction_cycles;
    observer->bus_accesses = observer->bus->access_count;
    observer->bus_clocks = observer->bus->master_clocks;
}

static void raise_interrupt_line(void *opaque,
                                 mipsnes2_master_clock timestamp,
                                 mipsnes2_scheduler_token token)
{
    interrupt_event_context *context = opaque;

    (void)timestamp;
    (void)token;
    context->calls++;
    if (context->nmi) {
        mipsnes2_interrupt_lines_latch_nmi(context->lines);
    } else {
        (void)mipsnes2_interrupt_lines_set_irq(
            context->lines, MIPSNES2_IRQ_SOURCE_CARTRIDGE, true);
    }
}

static void pulse_timed_abort(void *opaque,
                              mipsnes2_master_clock timestamp,
                              mipsnes2_scheduler_token token)
{
    abort_event_context *context = opaque;

    (void)timestamp;
    (void)token;
    context->calls++;
    context->result = mipsnes2_cpu_timing_pulse_abort(context->state);
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

static int test_resumable_timed_horizon_and_chunks(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_state state;
    mipsnes2_cpu_timing_report report;
    timed_observer observer;
    uint64_t reset_accesses;
    uint64_t reset_clocks;
    mipsnes2_master_clock horizon;

    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    reset_accesses = bus.access_count;
    reset_clocks = bus.master_clocks;
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    (void)memset(&observer, 0, sizeof(observer));
    observer.cpu = &cpu;
    observer.bus = &bus;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(4), 0U, observe_timed_state,
              &observer, NULL) == MIPSNES2_SCHEDULER_OK);

    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(4), 8U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(state.active && state.operation_pending);
    CHECK(state.pending_operation.kind == MIPSNES2_CPU_OPERATION_READ);
    CHECK(state.operation_start_time == 0U &&
          state.operation_end_time == UINT64_C(8));
    CHECK(observer.calls == 1U && observer.timestamp == UINT64_C(4));
    CHECK(observer.pc == UINT16_C(0x8000));
    CHECK(observer.processor_cycles == 0U);
    CHECK(observer.bus_accesses == reset_accesses);
    CHECK(observer.bus_clocks == reset_clocks);
    CHECK(report.processor_cycles == 0U && report.bus_accesses == 0U);
    CHECK(report.master_clocks == 0U && report.target_time == UINT64_C(4));

    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 8U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(!state.active && !state.operation_pending);
    CHECK(cpu.pc == UINT16_C(0x8001));
    CHECK(report.cpu_result == MIPSNES2_CPU_OK);
    CHECK(report.processor_cycles == UINT64_C(2));
    CHECK(report.bus_accesses == UINT64_C(1));
    CHECK(report.internal_cycles == UINT64_C(1));
    CHECK(report.master_clocks == UINT64_C(14));
    CHECK(report.events_dispatched == 1U);
    CHECK(report.target_time == UINT64_C(14));
    CHECK(scheduler.now == UINT64_C(14));

    /* An event exactly at an operation endpoint is delivered before commit. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    reset_accesses = bus.access_count;
    reset_clocks = bus.master_clocks;
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    (void)memset(&observer, 0, sizeof(observer));
    observer.cpu = &cpu;
    observer.bus = &bus;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(8), 0U, observe_timed_state,
              &observer, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 8U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(observer.calls == 1U && observer.timestamp == UINT64_C(8));
    CHECK(observer.pc == UINT16_C(0x8000));
    CHECK(observer.processor_cycles == 0U);
    CHECK(observer.bus_accesses == reset_accesses);
    CHECK(observer.bus_clocks == reset_clocks);
    CHECK(cpu.pc == UINT16_C(0x8001) && scheduler.now == UINT64_C(14));

    /* Cutting at every master clock must not restart either operation. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    reset_accesses = bus.access_count;
    reset_clocks = bus.master_clocks;
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    for (horizon = UINT64_C(1); horizon < UINT64_C(14); ++horizon) {
        CHECK(mipsnes2_cpu_timed_resume(
                  &cpu, &bus, &scheduler, &state, horizon, 0U,
                  &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
        CHECK(scheduler.now == horizon);
    }
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(14), 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.pc == UINT16_C(0x8001));
    CHECK(cpu.instruction_cycles == UINT64_C(2));
    CHECK(bus.access_count == reset_accesses + UINT64_C(1));
    CHECK(bus.master_clocks == reset_clocks + UINT64_C(14));
    CHECK(scheduler.now == UINT64_C(14));
    return 0;
}

static int test_resumable_timed_budget_and_write(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_state state;
    mipsnes2_cpu_timing_report report;
    timed_observer observer;
    uint64_t reset_accesses;
    uint64_t reset_clocks;
    uint16_t expected_pc;
    uint64_t expected_cycles;
    uint64_t expected_accesses;
    uint64_t expected_clocks;
    uint32_t expected_port;
    uint8_t expected_value;

    /* One-shot reference. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    reset_accesses = bus.access_count;
    reset_clocks = bus.master_clocks;
    cpu.a = UINT16_C(0x005a);
    cartridge.bank00[0x8000] = UINT8_C(0x8d); /* STA $2180 */
    cartridge.bank00[0x8001] = UINT8_C(0x80);
    cartridge.bank00[0x8002] = UINT8_C(0x21);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    (void)memset(&observer, 0, sizeof(observer));
    observer.cpu = &cpu;
    observer.bus = &bus;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(27), 0U, observe_timed_state,
              &observer, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(27), 0U, observe_timed_state,
              &observer, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 2U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(observer.calls == 2U && report.events_dispatched == 2U);
    CHECK(observer.pc == UINT16_C(0x8003));
    CHECK(observer.processor_cycles == UINT64_C(3));
    CHECK(observer.bus_accesses == reset_accesses + UINT64_C(3));
    CHECK(observer.bus_clocks == reset_clocks + UINT64_C(24));
    expected_pc = cpu.pc;
    expected_cycles = cpu.instruction_cycles;
    expected_accesses = bus.access_count - reset_accesses;
    expected_clocks = bus.master_clocks - reset_clocks;
    expected_port = bus.wram_port_address;
    expected_value = wram[0];
    CHECK(expected_pc == UINT16_C(0x8003));
    CHECK(expected_cycles == UINT64_C(4));
    CHECK(expected_accesses == UINT64_C(4));
    CHECK(expected_clocks == UINT64_C(30));
    CHECK(expected_port == UINT32_C(1) && expected_value == UINT8_C(0x5a));
    CHECK(scheduler.now == UINT64_C(30));

    /* Budget exhaustion keeps the final write pending and retryable. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    reset_accesses = bus.access_count;
    reset_clocks = bus.master_clocks;
    cpu.a = UINT16_C(0x005a);
    cartridge.bank00[0x8000] = UINT8_C(0x8d);
    cartridge.bank00[0x8001] = UINT8_C(0x80);
    cartridge.bank00[0x8002] = UINT8_C(0x21);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    (void)memset(&observer, 0, sizeof(observer));
    observer.cpu = &cpu;
    observer.bus = &bus;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(27), 0U, observe_timed_state,
              &observer, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(27), 0U, observe_timed_state,
              &observer, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 1U,
              &report) == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(report.scheduler_result ==
          MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED);
    CHECK(report.events_dispatched == 1U && observer.calls == 1U);
    CHECK(state.active && state.operation_pending);
    CHECK(state.pending_operation.kind == MIPSNES2_CPU_OPERATION_WRITE);
    CHECK(scheduler.now == UINT64_C(27));
    CHECK(cpu.pc == UINT16_C(0x8003));
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(bus.access_count == reset_accesses + UINT64_C(3));
    CHECK(bus.master_clocks == reset_clocks + UINT64_C(24));
    CHECK(wram[0] == 0U && bus.wram_port_address == 0U);

    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 1U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(observer.calls == 2U && report.events_dispatched == 2U);
    CHECK(!state.active && scheduler.event_count == 0U);
    CHECK(cpu.pc == expected_pc);
    CHECK(cpu.instruction_cycles == expected_cycles);
    CHECK(bus.access_count - reset_accesses == expected_accesses);
    CHECK(bus.master_clocks - reset_clocks == expected_clocks);
    CHECK(bus.wram_port_address == expected_port);
    CHECK(wram[0] == expected_value);
    CHECK(scheduler.now == UINT64_C(30));
    return 0;
}

static int test_resumable_timed_costs_reset_and_validation(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus_config config;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_state state;
    mipsnes2_cpu_timing_report report;
    timed_observer observer;
    route_capture cpu_route;
    uint64_t accesses_before;
    uint64_t clocks_before;

    /* Medium fetch plus one internal cycle. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(report.master_clocks == UINT64_C(14));

    /* FastROM fetch plus one internal cycle. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x00420d), UINT8_C(1));
    cpu.pbr = UINT8_C(0xc0);
    cpu.pc = UINT16_C(0x8000);
    cartridge.bankc0[0x8000] = UINT8_C(0xea);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(report.master_clocks == UINT64_C(12));

    /* $420D charges its own access before enabling later FastROM fetches. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cpu.pbr = UINT8_C(0xc0);
    cpu.pc = UINT16_C(0x8000);
    cpu.a = UINT16_C(1);
    cartridge.bankc0[0x8000] = UINT8_C(0x8d); /* STA $420D */
    cartridge.bankc0[0x8001] = UINT8_C(0x0d);
    cartridge.bankc0[0x8002] = UINT8_C(0x42);
    cartridge.bankc0[0x8003] = UINT8_C(0xea);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(report.master_clocks == UINT64_C(30));
    CHECK(bus.fastrom_enabled && cpu.pc == UINT16_C(0x8003));
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(report.master_clocks == UINT64_C(12));
    CHECK(scheduler.now == UINT64_C(42));

    /* Slow $4000 fetch supplied by a routed CPU-I/O fixture. */
    (void)memset(&cpu_route, 0, sizeof(cpu_route));
    cpu_route.read_value = UINT8_C(0xea);
    config = base_config();
    config.cpu_io = capture_interface(&cpu_route);
    (void)memset(&cartridge, 0, sizeof(cartridge));
    cartridge.bank00[0xfffc] = 0U;
    cartridge.bank00[0xfffd] = UINT8_C(0x40);
    config.cartridge = cartridge_interface(&cartridge);
    (void)memset(wram, 0, sizeof(wram));
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_OK);
    mipsnes2_cpu_init(&cpu, mipsnes2_snes_bus_make_interface(&bus));
    CHECK(mipsnes2_cpu_reset(&cpu) == MIPSNES2_CPU_OK);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(report.master_clocks == UINT64_C(18));
    CHECK(cpu_route.reads == 1U && cpu.pc == UINT16_C(0x4001));

    /* RESET is explicit: control is free and both vector reads are timed. */
    config = base_config();
    (void)memset(wram, 0, sizeof(wram));
    (void)memset(&cartridge, 0, sizeof(cartridge));
    cartridge.bank00[0xfffc] = UINT8_C(0x34);
    cartridge.bank00[0xfffd] = UINT8_C(0x12);
    config.cartridge = cartridge_interface(&cartridge);
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_OK);
    mipsnes2_cpu_init(&cpu, mipsnes2_snes_bus_make_interface(&bus));
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timing_state_begin_reset(&state) ==
          MIPSNES2_CPU_TIMING_OK);
    (void)memset(&observer, 0, sizeof(observer));
    observer.cpu = &cpu;
    observer.bus = &bus;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(4), 0U, observe_timed_state,
              &observer, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(4), 1U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(observer.calls == 1U && observer.pc == 0U);
    CHECK(observer.bus_accesses == 0U && observer.bus_clocks == 0U);
    CHECK(state.active && state.operation_pending && state.reset_sequence);
    CHECK(mipsnes2_cpu_timing_state_begin_reset(&state) ==
          MIPSNES2_CPU_TIMING_STALE_STATE);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(report.processor_cycles == 0U && report.internal_cycles == 0U);
    CHECK(report.bus_accesses == UINT64_C(2));
    CHECK(report.master_clocks == UINT64_C(16));
    CHECK(report.events_dispatched == 1U);
    CHECK(scheduler.now == UINT64_C(16));

    /* Overflow and stale descriptors fail before a bus effect. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    accesses_before = bus.access_count;
    bus.master_clocks = UINT64_MAX - UINT64_C(7);
    clocks_before = bus.master_clocks;
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW);
    CHECK(cpu.pc == UINT16_C(0x8000) && cpu.instruction_cycles == 0U);
    CHECK(bus.access_count == accesses_before &&
          bus.master_clocks == clocks_before);

    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    accesses_before = bus.access_count;
    clocks_before = bus.master_clocks;
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(4), 0U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    cpu.pc = UINT16_C(0x8001);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_STALE_STATE);
    CHECK(bus.access_count == accesses_before &&
          bus.master_clocks == clocks_before);
    CHECK(scheduler.now == UINT64_C(4));

    /* Descriptor equality alone does not authorize an altered CPU state. */
    cpu.pc = UINT16_C(0x8000);
    cpu.a = UINT16_C(0x1234);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_STALE_STATE);
    CHECK(bus.access_count == accesses_before &&
          bus.master_clocks == clocks_before);

    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    accesses_before = bus.access_count;
    clocks_before = bus.master_clocks;
    bus.access_count = UINT64_MAX;
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW);
    CHECK(cpu.pc == UINT16_C(0x8000));
    CHECK(bus.access_count == UINT64_MAX &&
          bus.master_clocks == clocks_before);

    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    accesses_before = bus.access_count;
    clocks_before = bus.master_clocks;
    mipsnes2_scheduler_init(&scheduler);
    scheduler.now = UINT64_MAX - UINT64_C(7);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW);
    CHECK(bus.access_count == accesses_before &&
          bus.master_clocks == clocks_before);
    CHECK(scheduler.now == UINT64_MAX - UINT64_C(7));

    CHECK(mipsnes2_cpu_timed_resume(
              NULL, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state,
              UINT64_MAX - UINT64_C(8), 0U,
              &report) == MIPSNES2_CPU_TIMING_TIME_REGRESSION);
    return 0;
}

static int test_resumable_timed_interrupt_order(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_interrupt_lines lines;
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv_timing;
    mipsnes2_cpu_timing_state state;
    mipsnes2_cpu_timing_report report;
    interrupt_event_context irq_event;
    interrupt_event_context nmi_event;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    mipsnes2_interrupt_lines_init(&lines);
    CHECK(initialize_cpu_bus_with_io(&bus, &cpu, &cpu_io) == 0);
    CHECK(mipsnes2_hv_timing_init(
              &hv_timing, MIPSNES2_HV_STANDARD_NTSC, false,
              &cpu_io) == MIPSNES2_HV_OK);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_I;
    irq_event.lines = &lines;
    irq_event.nmi = false;
    irq_event.calls = 0U;
    nmi_event.lines = &lines;
    nmi_event.nmi = true;
    nmi_event.calls = 0U;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(4), 0U, raise_interrupt_line,
              &irq_event, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(4), 1U, raise_interrupt_line,
              &nmi_event, NULL) == MIPSNES2_SCHEDULER_OK);

    CHECK(mipsnes2_cpu_timed_resume_hv_interrupts(
              &cpu, &bus, &scheduler, &hv_timing, &lines, &state,
              UINT64_MAX, 2U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.pc == UINT16_C(0x8001));
    CHECK(irq_event.calls == 1U && nmi_event.calls == 1U);
    CHECK(report.nmi_delivered && report.irq_level_delivered);
    CHECK(cpu.nmi_pending && cpu.irq_level);
    CHECK(scheduler.now == UINT64_C(14));
    CHECK(hv_timing.elapsed_master_clocks == scheduler.now);

    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume_hv_interrupts(
              &cpu, &bus, &scheduler, &hv_timing, &lines, &state,
              UINT64_MAX, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.pc == UINT16_C(0x9000));
    CHECK(!cpu.nmi_pending && cpu.irq_level);
    CHECK(report.processor_cycles == UINT64_C(7));
    CHECK(report.bus_accesses == UINT64_C(5));
    CHECK(report.internal_cycles == UINT64_C(2));
    CHECK(report.master_clocks == UINT64_C(52));
    CHECK(scheduler.now == UINT64_C(66));
    CHECK(hv_timing.elapsed_master_clocks == scheduler.now);
    return 0;
}

static int test_resumable_mid_instruction_abort(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_state state;
    mipsnes2_cpu_timing_report report;
    abort_event_context abort_event;
    uint64_t accesses_before;
    uint16_t stack_before;

    /* A read-only immediate operation keeps its external reads and clocks,
     * but the aborted instruction's internal registers return to the opcode. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xa9); /* LDA #$45 */
    cartridge.bank00[0x8001] = UINT8_C(0x45);
    cartridge.bank00[0xfff8] = UINT8_C(0x00);
    cartridge.bank00[0xfff9] = UINT8_C(0x92);
    cpu.a = UINT16_C(0x1234);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timing_pulse_abort(NULL) ==
          MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_UNSUPPORTED_OPERATION);
    accesses_before = bus.access_count;
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(4), 0U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_UNSUPPORTED_OPERATION);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(10), 0U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(state.abort_opcode_fetched && state.operation_pending);
    CHECK(state.pending_operation.kind == MIPSNES2_CPU_OPERATION_READ);
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(!state.active && cpu.abort_pending);
    CHECK(cpu.a == UINT16_C(0x1234) && cpu.pc == UINT16_C(0x8000));
    CHECK(report.processor_cycles == UINT64_C(2));
    CHECK(report.bus_accesses == UINT64_C(2));
    CHECK(bus.access_count == accesses_before + UINT64_C(2));
    CHECK(scheduler.now == UINT64_C(16));

    /* ABORT takes priority over simultaneously retained NMI and IRQ. */
    mipsnes2_cpu_pulse_nmi(&cpu);
    mipsnes2_cpu_set_irq(&cpu, true);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_I;
    stack_before = cpu.s;
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(!cpu.abort_pending && cpu.nmi_pending && cpu.irq_level);
    CHECK(cpu.pc == UINT16_C(0x9200));
    CHECK(wram[stack_before] == UINT8_C(0x80));
    CHECK(wram[UINT16_C(0x0100) |
               (uint8_t)(stack_before - UINT16_C(1))] == UINT8_C(0x00));

    /* A pending write cannot be silently committed after an ABORT pulse. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0x8d); /* STA $2180 */
    cartridge.bank00[0x8001] = UINT8_C(0x80);
    cartridge.bank00[0x8002] = UINT8_C(0x21);
    cpu.a = UINT16_C(0x005a);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(18), 0U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(state.abort_opcode_fetched);
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED);
    CHECK(state.active && !state.operation_pending);
    CHECK(wram[0] == 0U && bus.wram_port_address == 0U);
    CHECK(scheduler.now == UINT64_C(24));
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_INHIBIT) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(!state.active && cpu.abort_pending);
    CHECK(cpu.pc == UINT16_C(0x8000));
    CHECK(wram[0] == 0U && bus.wram_port_address == 0U);
    CHECK(bus.open_value == UINT8_C(0x5a));
    CHECK(report.bus_accesses == UINT64_C(4));
    CHECK(report.master_clocks == UINT64_C(30));
    CHECK(scheduler.now == UINT64_C(30));

    /* The event callback only changes caller-owned timing state. A budget
     * retry must not replay its pulse or the uncommitted operand read. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xa9);
    cartridge.bank00[0x8001] = UINT8_C(0x77);
    cpu.a = UINT16_C(0x0033);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    abort_event.state = &state;
    abort_event.calls = 0U;
    abort_event.result = MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(12), 0U, pulse_timed_abort,
              &abort_event, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(12), 1U, pulse_timed_abort,
              &abort_event, NULL) == MIPSNES2_SCHEDULER_OK);
    accesses_before = bus.access_count;
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 1U,
              &report) == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(abort_event.calls == 1U && state.abort_latched);
    CHECK(abort_event.result == MIPSNES2_CPU_TIMING_OK);
    CHECK(state.operation_pending && scheduler.now == UINT64_C(12));
    CHECK(bus.access_count == accesses_before + UINT64_C(1));
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 1U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(abort_event.calls == 2U && report.events_dispatched == 2U);
    CHECK(cpu.a == UINT16_C(0x0033) && cpu.pc == UINT16_C(0x8000));
    CHECK(cpu.abort_pending && bus.access_count ==
          accesses_before + UINT64_C(2));
    return 0;
}

static int test_resumable_abort_qualified_writes(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_state state;
    mipsnes2_cpu_timing_report report;
    abort_event_context abort_event;
    uint64_t access_before;
    uint16_t stack_before;
    uint8_t status_before;

    /* A pulse while a WRITE is already timed retains its original endpoint;
     * the caller resolves the exact pending descriptor after the block. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0x8d); /* STA $2180 */
    cartridge.bank00[0x8001] = UINT8_C(0x80);
    cartridge.bank00[0x8002] = UINT8_C(0x21);
    cpu.a = UINT16_C(0x004c);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    abort_event.state = &state;
    abort_event.calls = 0U;
    abort_event.result = MIPSNES2_CPU_TIMING_INVALID_ARGUMENT;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(27), 0U, pulse_timed_abort,
              &abort_event, NULL) == MIPSNES2_SCHEDULER_OK);
    access_before = bus.access_count;
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 1U,
              &report) == MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED);
    CHECK(abort_event.calls == 1U && abort_event.result ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(state.operation_pending && state.operation_end_time == UINT64_C(30));
    CHECK(scheduler.now == UINT64_C(30) && wram[0] == 0U);
    CHECK(bus.access_count == access_before + UINT64_C(3));
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_BLOCK) ==
          MIPSNES2_CPU_TIMING_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_EXTERNAL) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(!state.active && cpu.abort_pending && cpu.pc == UINT16_C(0x8000));
    CHECK(scheduler.now == UINT64_C(30));
    CHECK(wram[0] == UINT8_C(0x4c) && bus.wram_port_address == UINT32_C(1));
    CHECK(bus.access_count == access_before + UINT64_C(4));

    /* Same-endpoint callbacks and budget retry cannot duplicate a rejected
     * write; the write still consumes its original 6-clock interval. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0x8d);
    cartridge.bank00[0x8001] = UINT8_C(0x80);
    cartridge.bank00[0x8002] = UINT8_C(0x21);
    cpu.a = UINT16_C(0x004c);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    abort_event.state = &state;
    abort_event.calls = 0U;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(30), 0U, pulse_timed_abort,
              &abort_event, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(30), 1U, pulse_timed_abort,
              &abort_event, NULL) == MIPSNES2_SCHEDULER_OK);
    access_before = bus.access_count;
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 1U,
              &report) == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(abort_event.calls == 1U && abort_event.result ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(state.operation_pending && scheduler.now == UINT64_C(30));
    CHECK(bus.access_count == access_before + UINT64_C(3));
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 1U,
              &report) == MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED);
    CHECK(abort_event.calls == 2U && report.events_dispatched == 2U);
    CHECK(wram[0] == 0U && bus.wram_port_address == 0U);
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_INHIBIT) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(scheduler.now == UINT64_C(30) && cpu.abort_pending);
    CHECK(wram[0] == 0U && bus.wram_port_address == 0U);
    CHECK(bus.access_count == access_before + UINT64_C(4));

    /* Pre-modify RMW ABORT can externally inhibit the future write;
     * status and memory return to the pre-fetch values. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xe6); /* INC $00 */
    cartridge.bank00[0x8001] = 0U;
    wram[0] = UINT8_C(0x7f);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_N;
    status_before = cpu.p;
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(20), 0U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED);
    CHECK(!state.operation_pending && state.cpu_cycle.rmw_modify_committed);
    CHECK(wram[0] == UINT8_C(0x7f));
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_OK); /* same accepted pulse remains idempotent */
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_INHIBIT) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.abort_pending && cpu.pc == UINT16_C(0x8000));
    CHECK(cpu.p == status_before && wram[0] == UINT8_C(0x7f));
    CHECK(report.bus_accesses == UINT64_C(4) &&
          report.internal_cycles == UINT64_C(1));

    /* A late RMW pulse retains the status already changed by the modify
     * phase, while a caller-inhibited write leaves memory untouched. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xe6);
    cartridge.bank00[0x8001] = 0U;
    wram[0] = UINT8_C(0x7f);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_N;
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(30), 0U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(state.cpu_cycle.rmw_modify_committed);
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
    status_before = cpu.p;
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED);
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_INHIBIT) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.abort_pending && cpu.pc == UINT16_C(0x8000));
    CHECK(cpu.p == status_before && wram[0] == UINT8_C(0x7f));

    /* Stack writes are also qualified rather than silently committed. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0x48); /* PHA */
    cpu.a = UINT16_C(0x007a);
    stack_before = cpu.s;
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(10), 0U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED);
    CHECK(wram[stack_before] == 0U);
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_INHIBIT) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.s == stack_before && cpu.pc == UINT16_C(0x8000));
    CHECK(cpu.abort_pending && wram[stack_before] == 0U);

    /* Native 16-bit RMW needs an independent decision for each byte. The
     * ABORT entry then pushes the original bank and opcode address. */
    CHECK(initialize_cpu_bus(&bus, &cpu) == 0);
    cartridge.bank00[0x8000] = UINT8_C(0xee); /* INC $1000 */
    cartridge.bank00[0x8001] = UINT8_C(0x00);
    cartridge.bank00[0x8002] = UINT8_C(0x10);
    cartridge.bank00[0xffe8] = UINT8_C(0x00);
    cartridge.bank00[0xffe9] = UINT8_C(0x95);
    wram[UINT16_C(0x1000)] = UINT8_C(0xff);
    wram[UINT16_C(0x1001)] = UINT8_C(0x00);
    cpu.emulation = false;
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_M;
    cpu.s = UINT16_C(0x1ff0);
    status_before = cpu.p;
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_C(27), 0U,
              &report) == MIPSNES2_CPU_TIMING_HORIZON_REACHED);
    CHECK(mipsnes2_cpu_timing_pulse_abort(&state) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED);
    CHECK(scheduler.now == UINT64_C(46));
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_INHIBIT) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_ABORT_WRITE_BLOCKED);
    CHECK(scheduler.now == UINT64_C(54));
    CHECK(wram[UINT16_C(0x1000)] == UINT8_C(0xff));
    CHECK(mipsnes2_cpu_timing_resolve_abort_write(
              &state, MIPSNES2_CPU_ABORT_WRITE_INHIBIT) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.abort_pending && cpu.pc == UINT16_C(0x8000));
    CHECK(cpu.p == status_before && cpu.s == UINT16_C(0x1ff0));
    CHECK(wram[UINT16_C(0x1000)] == UINT8_C(0xff) &&
          wram[UINT16_C(0x1001)] == 0U);
    CHECK(report.processor_cycles == UINT64_C(8) &&
          report.bus_accesses == UINT64_C(7) &&
          report.master_clocks == UINT64_C(62));
    mipsnes2_cpu_timing_state_init(&state);
    CHECK(mipsnes2_cpu_timed_resume(
              &cpu, &bus, &scheduler, &state, UINT64_MAX, 0U,
              &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(cpu.pc == UINT16_C(0x9500) && cpu.s == UINT16_C(0x1fec));
    CHECK(wram[UINT16_C(0x1ff0)] == 0U &&
          wram[UINT16_C(0x1fef)] == UINT8_C(0x80) &&
          wram[UINT16_C(0x1fee)] == 0U);
    return 0;
}

static int test_resumable_timed_hv_budget_and_boundary_inputs(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_interrupt_lines lines;
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv_timing;
    mipsnes2_cpu_timing_state state;
    mipsnes2_cpu_timing_report report;
    timed_observer observer;
    interrupt_event_context irq_event;
    interrupt_event_context nmi_event;
    uint64_t accesses_before;
    uint64_t clocks_before;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    mipsnes2_interrupt_lines_init(&lines);
    CHECK(initialize_cpu_bus_with_io(&bus, &cpu, &cpu_io) == 0);
    CHECK(mipsnes2_hv_timing_init(
              &hv_timing, MIPSNES2_HV_STANDARD_NTSC, false,
              &cpu_io) == MIPSNES2_HV_OK);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    cartridge.bank00[0x8000] = UINT8_C(0xea);
    accesses_before = bus.access_count;
    clocks_before = bus.master_clocks;
    (void)memset(&observer, 0, sizeof(observer));
    observer.cpu = &cpu;
    observer.bus = &bus;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, UINT64_C(4), 0U, observe_timed_state,
              &observer, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_cpu_timed_resume_hv(
              &cpu, &bus, &scheduler, &hv_timing, &state,
              UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(report.scheduler_result ==
          MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED);
    CHECK(state.active && state.operation_pending);
    CHECK(scheduler.now == 0U && hv_timing.elapsed_master_clocks == 0U);
    CHECK(observer.calls == 0U && cpu.pc == UINT16_C(0x8000));
    CHECK(bus.access_count == accesses_before &&
          bus.master_clocks == clocks_before);
    CHECK(mipsnes2_cpu_timed_resume_hv(
              &cpu, &bus, &scheduler, &hv_timing, &state,
              UINT64_MAX, 1U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(observer.calls == 1U && observer.timestamp == UINT64_C(4));
    CHECK(hv_timing.elapsed_master_clocks == scheduler.now &&
          scheduler.now == UINT64_C(14));

    /* Current-timestamp events precede CONTROL; NMI wins over IRQ. */
    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    mipsnes2_interrupt_lines_init(&lines);
    CHECK(initialize_cpu_bus_with_io(&bus, &cpu, &cpu_io) == 0);
    CHECK(mipsnes2_hv_timing_init(
              &hv_timing, MIPSNES2_HV_STANDARD_NTSC, false,
              &cpu_io) == MIPSNES2_HV_OK);
    mipsnes2_scheduler_init(&scheduler);
    mipsnes2_cpu_timing_state_init(&state);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_I;
    irq_event.lines = &lines;
    irq_event.nmi = false;
    irq_event.calls = 0U;
    nmi_event.lines = &lines;
    nmi_event.nmi = true;
    nmi_event.calls = 0U;
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, 0U, 0U, raise_interrupt_line,
              &irq_event, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(
              &scheduler, 0U, 1U, raise_interrupt_line,
              &nmi_event, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_cpu_timed_resume_hv_interrupts(
              &cpu, &bus, &scheduler, &hv_timing, &lines, &state,
              UINT64_MAX, 0U, &report) ==
          MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(irq_event.calls == 0U && nmi_event.calls == 0U);
    CHECK(!state.active && cpu.pc == UINT16_C(0x8000));
    CHECK(mipsnes2_cpu_timed_resume_hv_interrupts(
              &cpu, &bus, &scheduler, &hv_timing, &lines, &state,
              UINT64_MAX, 2U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(irq_event.calls == 1U && nmi_event.calls == 1U);
    CHECK(report.nmi_delivered && report.irq_level_delivered);
    CHECK(cpu.pc == UINT16_C(0x9000));
    CHECK(scheduler.now == UINT64_C(52));
    return 0;
}

static int test_timed_cpu_math_progress(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_io cpu_io;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu_timing_report report;
    unsigned step;

    mipsnes2_cpu_io_init(&cpu_io, UINT8_C(2));
    CHECK(initialize_cpu_bus_with_io(&bus, &cpu, &cpu_io) == 0);
    mipsnes2_scheduler_init(&scheduler);
    cartridge.bank00[0x8000] = UINT8_C(0xa9); /* LDA #$06 */
    cartridge.bank00[0x8001] = UINT8_C(0x06);
    cartridge.bank00[0x8002] = UINT8_C(0x8d); /* STA $4202 */
    cartridge.bank00[0x8003] = UINT8_C(0x02);
    cartridge.bank00[0x8004] = UINT8_C(0x42);
    cartridge.bank00[0x8005] = UINT8_C(0xa9); /* LDA #$07 */
    cartridge.bank00[0x8006] = UINT8_C(0x07);
    cartridge.bank00[0x8007] = UINT8_C(0x8d); /* STA $4203 */
    cartridge.bank00[0x8008] = UINT8_C(0x03);
    cartridge.bank00[0x8009] = UINT8_C(0x42);
    for (step = 0U; step < 4U; ++step) {
        cartridge.bank00[0x800aU + step] = UINT8_C(0xea);
    }
    cartridge.bank00[0x800e] = UINT8_C(0xad); /* LDA $4216 */
    cartridge.bank00[0x800f] = UINT8_C(0x16);
    cartridge.bank00[0x8010] = UINT8_C(0x42);

    for (step = 0U; step < 4U; ++step) {
        CHECK(mipsnes2_cpu_timed_step(
                  &cpu, &bus, &scheduler, 8U, &report) ==
              MIPSNES2_CPU_TIMING_OK);
    }
    CHECK(cpu_io.math_cycles_remaining == UINT8_C(8));
    for (step = 0U; step < 4U; ++step) {
        CHECK(mipsnes2_cpu_timed_step(
                  &cpu, &bus, &scheduler, 8U, &report) ==
              MIPSNES2_CPU_TIMING_OK);
        CHECK(report.processor_cycles == UINT64_C(2));
        CHECK(cpu_io.math_cycles_remaining == (uint8_t)(6U - step * 2U));
    }
    CHECK(!mipsnes2_cpu_io_math_busy(&cpu_io));
    CHECK(mipsnes2_cpu_timed_step(
              &cpu, &bus, &scheduler, 8U, &report) ==
          MIPSNES2_CPU_TIMING_OK);
    CHECK((uint8_t)cpu.a == UINT8_C(42));
    CHECK(cpu.pc == UINT16_C(0x8011));
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

static int test_ppu_counter_wiring(void)
{
    mipsnes2_snes_bus bus;
    mipsnes2_snes_bus before_bus;
    mipsnes2_snes_bus_config config = base_config();
    mipsnes2_ppu_counters counters;
    mipsnes2_ppu_counters before_counters;
    mipsnes2_cpu_io io;
    mipsnes2_hv_timing timing;
    mipsnes2_hv_timing wrong_timing;
    mipsnes2_scheduler scheduler;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_timing_report report;
    route_capture fallback;
    unsigned bank;

    (void)memset(&fallback, 0, sizeof(fallback));
    fallback.read_value = UINT8_C(0x55);
    config.ppu = capture_interface(&fallback);
    mipsnes2_cpu_io_init(&io, 2U);
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &io) == MIPSNES2_HV_OK);
    mipsnes2_ppu_counters_init(&counters, 3U, UINT8_C(0xa4));
    config.ppu_counters = &counters;
    (void)memset(&bus, 0x5a, sizeof(bus));
    before_bus = bus;
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_INVALID_ARGUMENT);
    CHECK(memcmp(&before_bus, &bus, sizeof(bus)) == 0);
    config.hv_timing = &timing;
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_INVALID_ARGUMENT);
    config.cpu_io_state = &io;
    counters.initialized = false;
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_INVALID_ARGUMENT);
    counters.initialized = true;
    CHECK(mipsnes2_snes_bus_init(&bus, &config) == MIPSNES2_SNES_BUS_OK);
    CHECK(!mipsnes2_snes_bus_sync_hv_latch(NULL));
    CHECK(!mipsnes2_snes_bus_sync_hv_latch(&bus));

    timing.h_master_clock = 1298U;
    timing.v_counter = 260U;
    for (bank = 0U; bank < 256U; ++bank) {
        uint32_t base;

        if ((bank & 0x40U) != 0U) {
            continue;
        }
        base = (uint32_t)bank << 16;
        bus.open_value = UINT8_C(0x5a);
        CHECK(mipsnes2_snes_bus_read(&bus, base | UINT32_C(0x2137)) == UINT8_C(0x5a));
        CHECK(counters.horizontal == 324U && counters.vertical == 260U);
        CHECK(mipsnes2_snes_bus_read(&bus, base | UINT32_C(0x213c)) == UINT8_C(0x44));
        CHECK(mipsnes2_snes_bus_read(&bus, base | UINT32_C(0x213d)) == UINT8_C(0x04));
        CHECK(mipsnes2_snes_bus_read(&bus, base | UINT32_C(0x213c)) == UINT8_C(0x05));
        CHECK(mipsnes2_snes_bus_read(&bus, base | UINT32_C(0x213d)) == UINT8_C(0x05));
        CHECK(mipsnes2_snes_bus_read(&bus, base | UINT32_C(0x213f)) == UINT8_C(0x43));
    }
    CHECK(bus.access_count == UINT64_C(128) * 6U);
    CHECK(bus.master_clocks == bus.access_count * 6U);
    CHECK(fallback.reads == 0U && fallback.writes == 0U);
    before_counters = counters;
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x802137), 0U);
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80213c), 0U);
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80213d), 0U);
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x80213f), 0U);
    CHECK(memcmp(&before_counters, &counters, sizeof(counters)) == 0);
    CHECK(fallback.writes == 0U);
    CHECK(mipsnes2_snes_bus_read(&bus, UINT32_C(0x00213e)) == UINT8_C(0x55));
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x002100), UINT8_C(0x80));
    CHECK(fallback.reads == 1U && fallback.writes == 1U);

    timing.h_master_clock = 800U;
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x804201), UINT8_C(0x7f));
    CHECK(counters.horizontal == 200U && counters.latched);
    CHECK(!io.hv_latch_edge_pending);
    timing.h_master_clock = 1000U;
    (void)mipsnes2_snes_bus_read(&bus, UINT32_C(0x002137));
    CHECK(counters.horizontal == 200U);
    CHECK((mipsnes2_snes_bus_read(&bus, UINT32_C(0x00213f)) & UINT8_C(0x40)) != 0U);
    CHECK(counters.latched);
    mipsnes2_snes_bus_write(&bus, UINT32_C(0x004201), UINT8_MAX);
    CHECK(mipsnes2_cpu_io_set_io_port_input(&io, UINT8_C(0x7f)));
    timing.h_master_clock = 1364U;
    CHECK(!mipsnes2_snes_bus_sync_hv_latch(&bus));
    CHECK(io.hv_latch_edge_pending);
    timing.h_master_clock = 1000U;
    CHECK(mipsnes2_snes_bus_sync_hv_latch(&bus));
    CHECK(counters.horizontal == 250U);
    CHECK(!mipsnes2_snes_bus_sync_hv_latch(&bus));
    (void)mipsnes2_snes_bus_read(&bus, UINT32_C(0x00213f));
    CHECK(!counters.latched);

    /* CPU access sees the explicit pre-instruction H/V snapshot, not cycle-perfect time. */
    CHECK(initialize_cpu_bus_with_io(&bus, &cpu, &io) == 0);
    CHECK(mipsnes2_hv_timing_init(&timing, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &io) == MIPSNES2_HV_OK);
    bus.ppu_counters = &counters;
    bus.hv_timing = &timing;
    mipsnes2_scheduler_init(&scheduler);
    cartridge.bank00[0x8000] = UINT8_C(0xad);
    cartridge.bank00[0x8001] = UINT8_C(0x37);
    cartridge.bank00[0x8002] = UINT8_C(0x21);
    CHECK(mipsnes2_cpu_timed_step_hv(&cpu, &bus, &scheduler, &timing,
                                     8U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(counters.horizontal == 0U && counters.vertical == 0U);
    CHECK(timing.h_master_clock == 30U && scheduler.now == 30U);
    wrong_timing = timing;
    CHECK(mipsnes2_cpu_timed_step_hv(&cpu, &bus, &scheduler, &wrong_timing,
                                     8U, &report) == MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH);
    CHECK(mipsnes2_cpu_timed_step(&cpu, &bus, &scheduler, 8U, &report) ==
          MIPSNES2_CPU_TIMING_DOMAIN_MISMATCH);
    CHECK(cpu.pc == UINT16_C(0x8003));
    return 0;
}

int main(void)
{
    CHECK(test_init_and_wram_mapping() == 0);
    CHECK(test_wram_port_and_fastrom_timing() == 0);
    CHECK(test_mmio_and_cartridge_routing() == 0);
    CHECK(test_cpu_io_state_routing() == 0);
    CHECK(test_cpu_io_math_bus_cycles() == 0);
    CHECK(test_timed_cpu_steps() == 0);
    CHECK(test_resumable_timed_horizon_and_chunks() == 0);
    CHECK(test_resumable_timed_budget_and_write() == 0);
    CHECK(test_resumable_timed_costs_reset_and_validation() == 0);
    CHECK(test_resumable_timed_interrupt_order() == 0);
    CHECK(test_resumable_mid_instruction_abort() == 0);
    CHECK(test_resumable_abort_qualified_writes() == 0);
    CHECK(test_resumable_timed_hv_budget_and_boundary_inputs() == 0);
    CHECK(test_timed_cpu_math_progress() == 0);
    CHECK(test_timed_cpu_hv_steps() == 0);
    CHECK(test_timed_cpu_interrupt_handoff() == 0);
    CHECK(test_ppu_counter_wiring() == 0);
    (void)puts("PASS: SNES bus and timed CPU/H-V/interrupt foundation");
    return 0;
}
