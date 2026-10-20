/* Original CPU access-clock fixtures. See LICENSE for GPL-3.0 terms. */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "core/scheduler/cpu_timing.h"
#include "core/scheduler/master_timing.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

typedef struct {
    uint64_t time;
    mipsnes2_address address;
    uint8_t value;
    bool write;
} access_record;

typedef struct {
    mipsnes2_cpu cpu;
    mipsnes2_snes_bus bus;
    mipsnes2_cpu_io io;
    mipsnes2_scheduler scheduler;
    mipsnes2_hv_timing hv;
    mipsnes2_ppu_counters counters;
    access_record trace[16];
    size_t count;
    bool trace_overflow;
    uint8_t input;
    uint8_t output;
    unsigned events;
    uint64_t observed_accesses;
    uint8_t observed_output;
    bool observed_hblank;
    bool reenter_port;
    unsigned reentry_kind;
    mipsnes2_cpu_timing_result reentry_result;
    mipsnes2_snes_bus_result reentry_bus_result;
} fixture;

static uint8_t wram[MIPSNES2_SNES_WRAM_SIZE];
static uint8_t rom[65536];

static void record(fixture *f, mipsnes2_address address, uint8_t value,
                    bool write)
{
    access_record *entry;

    if (f->bus.clock == NULL) {
        return;
    }
    if (f->count == sizeof(f->trace) / sizeof(f->trace[0])) {
        f->trace_overflow = true;
        return;
    }
    entry = &f->trace[f->count++];
    entry->time = f->scheduler.now;
    entry->address = address;
    entry->value = value;
    entry->write = write;
}

static uint8_t port_read(void *opaque, mipsnes2_address address)
{
    fixture *f = opaque;
    uint8_t value = ((uint16_t)address & 1U) != 0U ? UINT8_C(0x12) : f->input;

    record(f, address, value, false);
    if (f->reenter_port) {
        mipsnes2_snes_bus_write(&f->bus, UINT32_C(0x7e0000), UINT8_C(0xa5));
    }
    return value;
}

static void port_write(void *opaque, mipsnes2_address address, uint8_t value)
{
    fixture *f = opaque;

    record(f, address, value, true);
    f->output = value;
    if (f->reenter_port) {
        (void)mipsnes2_snes_bus_read(&f->bus, UINT32_C(0x002180));
    }
}

static bool rom_read(void *opaque, mipsnes2_address address, uint8_t *value)
{
    fixture *f = opaque;

    *value = rom[(uint16_t)address];
    record(f, address, *value, false);
    return true;
}

static int initialize(fixture *f, bool counters)
{
    mipsnes2_snes_bus_config config;

    (void)memset(f, 0, sizeof(*f));
    (void)memset(wram, 0, sizeof(wram));
    (void)memset(rom, 0xea, sizeof(rom));
    (void)memset(&config, 0, sizeof(config));
    mipsnes2_cpu_io_init(&f->io, 2U);
    CHECK(mipsnes2_hv_timing_init(&f->hv, MIPSNES2_HV_STANDARD_NTSC,
                                  false, &f->io) == MIPSNES2_HV_OK);
    mipsnes2_scheduler_init(&f->scheduler);
    config.wram = wram;
    config.wram_size = sizeof(wram);
    config.cpu_io_state = &f->io;
    config.ppu.context = f;
    config.ppu.read8 = port_read;
    config.ppu.write8 = port_write;
    config.cpu_io = config.ppu;
    config.cartridge.context = f;
    config.cartridge.read8 = rom_read;
    if (counters) {
        mipsnes2_ppu_counters_init(&f->counters, 3U, 0U);
        config.ppu_counters = &f->counters;
        config.hv_timing = &f->hv;
    }
    CHECK(mipsnes2_snes_bus_init(&f->bus, &config) == MIPSNES2_SNES_BUS_OK);
    rom[0xfffc] = 0U;
    rom[0xfffd] = UINT8_C(0x80);
    mipsnes2_cpu_init(&f->cpu, mipsnes2_snes_bus_make_interface(&f->bus));
    CHECK(mipsnes2_cpu_reset(&f->cpu) == MIPSNES2_CPU_OK);
    f->input = UINT8_C(0x34);
    return 0;
}

static void absolute_program(uint8_t opcode, uint16_t address)
{
    rom[0x8000] = opcode;
    rom[0x8001] = (uint8_t)address;
    rom[0x8002] = (uint8_t)(address >> 8);
}

static mipsnes2_cpu_timing_result step(fixture *f, size_t budget,
                                      mipsnes2_cpu_timing_report *report)
{
    return mipsnes2_cpu_timed_step_hv(&f->cpu, &f->bus, &f->scheduler,
                                      &f->hv, budget, report);
}

static void observe_and_change_input(void *opaque, uint64_t timestamp,
                                      mipsnes2_scheduler_token token)
{
    fixture *f = opaque;

    (void)timestamp;
    (void)token;
    f->events++;
    f->observed_accesses = f->bus.access_count - 2U; /* RESET vector excluded. */
    f->observed_output = f->output;
    f->observed_hblank = f->io.hblank;
    f->input = UINT8_C(0x7f);
}

static int test_access_traces(void)
{
    fixture f;
    unsigned mode, fast, slow_port, store;

    for (mode = 0U; mode < 3U; mode++) {
        for (fast = 0U; fast < 2U; fast++) {
            for (slow_port = 0U; slow_port < 2U; slow_port++) {
                for (store = 0U; store < 2U; store++) {
                    mipsnes2_cpu_timing_report report;
                    uint16_t port = slow_port != 0U ? 0x4018U : 0x2100U;
                    uint64_t fetch_clocks = fast != 0U ? 6U : 8U;
                    uint64_t data_clocks = slow_port != 0U ? 12U : 6U;
                    size_t width = mode == 2U ? 2U : 1U;
                    size_t i;

                    CHECK(initialize(&f, false) == 0);
                    if (mode != 0U) {
                        f.cpu.emulation = false;
                    }
                    if (mode == 2U) {
                        f.cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_M;
                    }
                    f.bus.fastrom_enabled = fast != 0U;
                    f.cpu.pbr = fast != 0U ? UINT8_C(0xc0) : 0U;
                    f.cpu.a = UINT16_C(0x1234);
                    absolute_program(store != 0U ? 0x8dU : 0xadU, port);
                    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
                    CHECK(f.count == 3U + width && !f.trace_overflow);
                    for (i = 0U; i < f.count; i++) {
                        uint64_t expected = i < 3U
                            ? (i + 1U) * fetch_clocks
                            : 3U * fetch_clocks + (i - 2U) * data_clocks;
                        CHECK(f.trace[i].time == expected);
                        CHECK(f.trace[i].address == (i < 3U
                            ? mipsnes2_address_make(f.cpu.pbr, (uint16_t)(0x8000U + i))
                            : (mipsnes2_address)(port + i - 3U)));
                        CHECK(f.trace[i].write == (store != 0U && i >= 3U));
                        if (i >= 3U) {
                            CHECK(f.trace[i].value == (i == 3U ? 0x34U : 0x12U));
                        }
                    }
                    CHECK(report.processor_cycles == 3U + width);
                    CHECK(report.bus_accesses == 3U + width);
                    CHECK(report.internal_cycles == 0U);
                    CHECK(report.master_clocks == 3U * fetch_clocks + width * data_clocks);
                    CHECK(report.elapsed_master_clocks == report.master_clocks);
                    CHECK(f.scheduler.now == f.hv.elapsed_master_clocks);
                    CHECK(!report.recovery_required && f.bus.clock == NULL);
                    CHECK(!f.bus.transaction_active);
                }
            }
        }
    }
    return 0;
}

static int test_event_phase_sweep(void)
{
    fixture f;
    uint64_t tick;
    unsigned store;

    for (store = 0U; store < 2U; store++) {
        for (tick = 0U; tick <= 31U; tick++) {
            mipsnes2_cpu_timing_report report;
            uint64_t completed_fetches = tick > 24U ? 3U :
                                        tick > 16U ? 2U : tick > 8U ? 1U : 0U;

            CHECK(initialize(&f, false) == 0);
            absolute_program(store != 0U ? 0x8dU : 0xadU, UINT16_C(0x2100));
            f.cpu.a = UINT16_C(0x55);
            CHECK(mipsnes2_scheduler_schedule(&f.scheduler, tick, 0U,
                    observe_and_change_input, &f, NULL) == MIPSNES2_SCHEDULER_OK);
            CHECK(step(&f, 1U, &report) == MIPSNES2_CPU_TIMING_OK);
            CHECK(report.master_clocks == 30U && report.elapsed_master_clocks == 30U);
            CHECK(report.events_dispatched == (tick <= 30U ? 1U : 0U));
            CHECK(f.events == (tick <= 30U ? 1U : 0U));
            if (tick <= 30U) {
                CHECK(f.observed_accesses == completed_fetches);
                CHECK(f.observed_output == 0U); /* Equal-time callback precedes write. */
            }
            if (store != 0U) {
                CHECK(f.output == UINT8_C(0x55));
            } else {
                CHECK(f.cpu.a == (tick <= 30U ? UINT16_C(0x7f) : UINT16_C(0x34)));
            }
        }
    }
    return 0;
}

static int advance_to(fixture *f, uint64_t time)
{
    mipsnes2_master_timing_report report;

    CHECK(mipsnes2_master_timing_run_until(&f->scheduler, &f->hv, &f->io,
                    time, 0U, &report) == MIPSNES2_MASTER_TIMING_OK);
    return 0;
}

static int test_hv_register_observation(void)
{
    fixture f;
    mipsnes2_cpu_timing_report report;
    unsigned delta;

    for (delta = 0U; delta < 3U; delta++) {
        CHECK(initialize(&f, true) == 0);
        absolute_program(UINT8_C(0xad), UINT16_C(0x4212));
        CHECK(advance_to(&f, UINT64_C(1065) + delta) == 0);
        CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
        CHECK((f.cpu.a & UINT16_C(0x40)) == (delta == 0U ? 0U : 0x40U));
        CHECK(report.elapsed_master_clocks == 30U);
    }
    CHECK(initialize(&f, true) == 0);
    absolute_program(UINT8_C(0xad), UINT16_C(0x4212));
    CHECK(advance_to(&f, UINT64_C(1066)) == 0);
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 1096U, 0U,
                    observe_and_change_input, &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(step(&f, 1U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(f.observed_hblank && (f.cpu.a & UINT16_C(0x40)) != 0U);

    CHECK(initialize(&f, true) == 0);
    absolute_program(UINT8_C(0xad), UINT16_C(0x2137));
    CHECK(advance_to(&f, UINT64_C(1340)) == 0);
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(f.counters.horizontal == 1U && f.counters.vertical == 1U);
    CHECK(f.scheduler.now == 1370U && f.hv.h_master_clock == 6U);
    CHECK(initialize(&f, true) == 0);
    absolute_program(UINT8_C(0x8d), UINT16_C(0x4201));
    f.cpu.a = UINT16_C(0x7f);
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(f.counters.horizontal == 7U && f.counters.latched);
    CHECK(!f.io.hv_latch_edge_pending);
    return 0;
}

static void reenter(void *opaque, uint64_t timestamp,
                      mipsnes2_scheduler_token token)
{
    fixture *f = opaque;
    mipsnes2_cpu_timing_report report;

    (void)timestamp;
    (void)token;
    f->events++;
    if (f->reentry_kind == 0U) {
        mipsnes2_snes_bus_write(&f->bus, UINT32_C(0x7e0000), UINT8_C(0xa5));
    } else if (f->reentry_kind == 1U) {
        (void)mipsnes2_snes_bus_read(&f->bus, UINT32_C(0x002180));
    } else if (f->reentry_kind == 2U) {
        f->reentry_result = step(f, 8U, &report);
    } else {
        f->reentry_bus_result = mipsnes2_snes_bus_account_internal_cycles(&f->bus, 1U);
    }
}

static int check_latched_failure(fixture *f)
{
    mipsnes2_cpu_timing_report report;
    mipsnes2_cpu before = f->cpu;
    uint64_t accesses = f->bus.access_count;
    uint64_t clocks = f->bus.master_clocks;
    uint64_t time = f->scheduler.now;
    uint8_t open = f->bus.open_value;
    uint8_t math = f->io.math_cycles_remaining;

    CHECK(f->bus.clock == NULL && !f->bus.transaction_active);
    CHECK(step(f, 64U, &report) == MIPSNES2_CPU_TIMING_RECOVERY_REQUIRED);
    CHECK(report.recovery_required && report.cpu_result == MIPSNES2_CPU_NOT_READY);
    CHECK(memcmp(&before, &f->cpu, sizeof(before)) == 0);
    mipsnes2_snes_bus_write(&f->bus, UINT32_C(0x7e0000), UINT8_C(0xa5));
    CHECK(mipsnes2_snes_bus_read(&f->bus, UINT32_C(0x002180)) == open);
    CHECK(mipsnes2_snes_bus_account_internal_cycles(&f->bus, 2U) != MIPSNES2_SNES_BUS_OK);
    CHECK(wram[0] == 0U && f->bus.wram_port_address == 0U);
    CHECK(f->bus.access_count == accesses && f->bus.master_clocks == clocks);
    CHECK(f->scheduler.now == time && f->io.math_cycles_remaining == math);
    return 0;
}

static int test_fail_stop_and_retry(void)
{
    fixture f;
    mipsnes2_cpu_timing_report report;
    unsigned kind;

    CHECK(initialize(&f, false) == 0);
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 0U, 0U,
                    observe_and_change_input, &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(!report.recovery_required && report.cpu_result == MIPSNES2_CPU_NOT_READY);
    CHECK(f.cpu.pc == UINT16_C(0x8000) && f.bus.clock == NULL);
    CHECK(step(&f, 1U, &report) == MIPSNES2_CPU_TIMING_OK);
    CHECK(report.events_dispatched == 1U && report.master_clocks == 14U);

    CHECK(initialize(&f, false) == 0);
    f.hv.initialized = false;
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_HV_ERROR);
    CHECK(!report.recovery_required && f.cpu.pc == UINT16_C(0x8000));
    CHECK(f.bus.clock == NULL && report.elapsed_master_clocks == 0U);
    f.hv.initialized = true;
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_OK);

    CHECK(initialize(&f, false) == 0);
    absolute_program(UINT8_C(0x8d), UINT16_C(0x2100));
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 16U, 0U,
                    observe_and_change_input, &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 28U, 0U,
                    observe_and_change_input, &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(step(&f, 1U, &report) == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(report.scheduler_result == MIPSNES2_SCHEDULER_EVENT_BUDGET_EXHAUSTED);
    CHECK(report.recovery_required && f.output == 0U);
    CHECK(report.bus_accesses == 3U && report.master_clocks == 24U);
    CHECK(report.elapsed_master_clocks == 24U && report.target_time == 30U);
    CHECK(report.events_dispatched == 1U && f.scheduler.event_count == 1U);
    CHECK(f.scheduler.now == f.hv.elapsed_master_clocks);
    CHECK(check_latched_failure(&f) == 0);

    /* One event commits inside the failed access, but its write does not. */
    CHECK(initialize(&f, false) == 0);
    absolute_program(UINT8_C(0x8d), UINT16_C(0x2100));
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 25U, 0U,
                    observe_and_change_input, &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 28U, 0U,
                    observe_and_change_input, &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(step(&f, 1U, &report) == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(report.master_clocks == 24U && report.elapsed_master_clocks == 25U);
    CHECK(f.scheduler.now == 25U && f.hv.elapsed_master_clocks == 25U);
    CHECK(f.output == 0U && report.recovery_required);
    CHECK(check_latched_failure(&f) == 0);

    /* Remainder failure after a completed NOP fetch must also latch. */
    CHECK(initialize(&f, false) == 0);
    CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 12U, 0U,
                    observe_and_change_input, &f, NULL) == MIPSNES2_SCHEDULER_OK);
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_SCHEDULER_ERROR);
    CHECK(report.recovery_required && report.master_clocks == 8U);
    CHECK(report.internal_cycles == 1U && report.elapsed_master_clocks == 8U);
    CHECK(check_latched_failure(&f) == 0);

    for (kind = 0U; kind < 4U; kind++) {
        CHECK(initialize(&f, false) == 0);
        f.reentry_kind = kind;
        CHECK(mipsnes2_scheduler_schedule(&f.scheduler, 4U, 0U,
                        reenter, &f, NULL) == MIPSNES2_SCHEDULER_OK);
        CHECK(step(&f, 8U, &report) == MIPSNES2_CPU_TIMING_REENTRANT_ACCESS);
        CHECK(report.recovery_required && report.bus_accesses == 0U);
        CHECK(report.master_clocks == 0U && report.elapsed_master_clocks == 8U);
        CHECK(f.bus.timing_fault == MIPSNES2_SNES_BUS_REENTRANT_ACCESS);
        if (kind == 2U) {
            CHECK(f.reentry_result == MIPSNES2_CPU_TIMING_REENTRANT_ACCESS);
        } else if (kind == 3U) {
            CHECK(f.reentry_bus_result == MIPSNES2_SNES_BUS_REENTRANT_ACCESS);
        }
        CHECK(check_latched_failure(&f) == 0);
    }
    CHECK(initialize(&f, false) == 0);
    f.bus.access_count = UINT64_MAX;
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW);
    CHECK(report.recovery_required && report.elapsed_master_clocks == 0U);
    CHECK(check_latched_failure(&f) == 0);
    CHECK(initialize(&f, false) == 0);
    f.bus.master_clocks = UINT64_MAX - 7U;
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW);
    CHECK(report.recovery_required && report.elapsed_master_clocks == 0U);
    CHECK(check_latched_failure(&f) == 0);
    CHECK(initialize(&f, false) == 0);
    f.bus.master_clocks = UINT64_MAX - 13U;
    CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW);
    CHECK(report.recovery_required && report.elapsed_master_clocks == 8U);
    CHECK(report.bus_accesses == 1U && report.master_clocks == 8U);
    CHECK(check_latched_failure(&f) == 0);
    CHECK(initialize(&f, false) == 0);
    f.scheduler.now = UINT64_MAX - 7U;
    CHECK(mipsnes2_cpu_timed_step(&f.cpu, &f.bus, &f.scheduler, 0U, &report) ==
          MIPSNES2_CPU_TIMING_CLOCK_OVERFLOW);
    CHECK(report.recovery_required && report.elapsed_master_clocks == 0U);
    CHECK(check_latched_failure(&f) == 0);
    return 0;
}

static int test_mmio_callback_reentry(void)
{
    fixture f;
    unsigned store;

    for (store = 0U; store < 2U; store++) {
        mipsnes2_cpu_timing_report report;

        CHECK(initialize(&f, false) == 0);
        CHECK(mipsnes2_cpu_io_write(&f.io, UINT16_C(0x4203), 2U));
        CHECK(f.io.math_cycles_remaining == 8U);
        absolute_program(store != 0U ? 0x8dU : 0xadU, UINT16_C(0x2100));
        f.cpu.a = UINT16_C(0x55);
        f.reenter_port = true;
        CHECK(step(&f, 0U, &report) == MIPSNES2_CPU_TIMING_REENTRANT_ACCESS);
        CHECK(report.recovery_required && report.bus_accesses == 4U);
        CHECK(report.master_clocks == 30U && report.elapsed_master_clocks == 30U);
        CHECK(f.bus.open_value == UINT8_C(0x21)); /* Last successful fetch. */
        CHECK(f.io.math_cycles_remaining == (store != 0U ? 4U : 5U));
        CHECK(f.output == (store != 0U ? UINT8_C(0x55) : 0U));
        CHECK(check_latched_failure(&f) == 0);
    }
    return 0;
}

int main(void)
{
    CHECK(test_access_traces() == 0);
    CHECK(test_event_phase_sweep() == 0);
    CHECK(test_hv_register_observation() == 0);
    CHECK(test_fail_stop_and_retry() == 0);
    CHECK(test_mmio_callback_reentry() == 0);
    (void)puts("PASS: meaningful CPU access-clock ordering and fail-stop recovery contract");
    return 0;
}
