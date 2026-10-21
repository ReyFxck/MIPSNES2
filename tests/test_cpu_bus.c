/* MIPSNES2 CPU/bus foundation tests. See LICENSE for GPL-3.0 terms. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/bus/bus.h"
#include "core/cpu/65c816/cpu.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static uint8_t bank00[UINT32_C(0x10000)];
static uint8_t bank7e[UINT32_C(0x10000)];
static uint8_t bank7f[UINT32_C(0x10000)];
static uint8_t bank80[UINT32_C(0x10000)];
static uint8_t bankff[UINT32_C(0x10000)];

typedef struct {
    unsigned reads;
    unsigned writes;
    mipsnes2_address last_address;
    uint8_t last_value;
} mmio_capture;

typedef struct {
    mipsnes2_bus bus;
    mipsnes2_cpu cpu;
} cpu_fixture;

enum { TRACE_ACCESS_CAPACITY = 32 };

typedef struct {
    bool write;
    mipsnes2_address address;
    uint8_t value;
} trace_access;

typedef struct {
    trace_access accesses[TRACE_ACCESS_CAPACITY];
    size_t count;
} trace_bus_context;

static uint8_t *test_bank(mipsnes2_address address)
{
    switch ((uint8_t)(address >> 16)) {
    case 0x00:
        return bank00;
    case 0x7e:
        return bank7e;
    case 0x7f:
        return bank7f;
    case 0x80:
        return bank80;
    case 0xff:
        return bankff;
    default:
        return NULL;
    }
}

static void record_trace_access(trace_bus_context *context, bool write,
                                mipsnes2_address address, uint8_t value)
{
    if (context->count < TRACE_ACCESS_CAPACITY) {
        trace_access *access = &context->accesses[context->count];

        access->write = write;
        access->address = mipsnes2_address24(address);
        access->value = value;
    }
    context->count++;
}

static uint8_t trace_read(void *opaque, mipsnes2_address address)
{
    trace_bus_context *context = opaque;
    uint8_t *bank = test_bank(address);
    uint8_t value = bank != NULL ? bank[(uint16_t)address] : UINT8_C(0xa5);

    record_trace_access(context, false, address, value);
    return value;
}

static void trace_write(void *opaque, mipsnes2_address address, uint8_t value)
{
    trace_bus_context *context = opaque;
    uint8_t *bank = test_bank(address);

    record_trace_access(context, true, address, value);
    if (bank != NULL) {
        bank[(uint16_t)address] = value;
    }
}

static int initialize_trace_cpu(mipsnes2_cpu *cpu,
                                trace_bus_context *context,
                                uint16_t reset_pc)
{
    mipsnes2_bus_interface interface;

    (void)memset(bank00, 0, sizeof(bank00));
    (void)memset(bank7e, 0, sizeof(bank7e));
    (void)memset(bank7f, 0, sizeof(bank7f));
    (void)memset(bank80, 0, sizeof(bank80));
    (void)memset(bankff, 0, sizeof(bankff));
    (void)memset(context, 0, sizeof(*context));
    bank00[0xfffc] = (uint8_t)reset_pc;
    bank00[0xfffd] = (uint8_t)(reset_pc >> 8);
    interface.context = context;
    interface.read8 = trace_read;
    interface.write8 = trace_write;
    mipsnes2_cpu_init(cpu, interface);
    CHECK(mipsnes2_cpu_reset(cpu) == MIPSNES2_CPU_OK);
    context->count = 0U;
    return 0;
}

static uint8_t capture_read(void *context, mipsnes2_address address)
{
    mmio_capture *capture = context;
    capture->reads++;
    capture->last_address = address;
    return UINT8_C(0x6d);
}

static void capture_write(void *context, mipsnes2_address address,
                          uint8_t value)
{
    mmio_capture *capture = context;
    capture->writes++;
    capture->last_address = address;
    capture->last_value = value;
}

static int initialize_cpu_fixture(cpu_fixture *fixture, uint16_t reset_pc)
{
    mipsnes2_bus_interface interface;

    (void)memset(bank00, 0, sizeof(bank00));
    (void)memset(bank7e, 0, sizeof(bank7e));
    (void)memset(bank7f, 0, sizeof(bank7f));
    (void)memset(bank80, 0, sizeof(bank80));
    (void)memset(bankff, 0, sizeof(bankff));
    mipsnes2_bus_init(&fixture->bus, UINT8_C(0xa5));
    CHECK(mipsnes2_bus_map_direct(&fixture->bus, UINT32_C(0x000000),
                                  sizeof(bank00), bank00, true) ==
          MIPSNES2_BUS_OK);
    CHECK(mipsnes2_bus_map_direct(&fixture->bus, UINT32_C(0x7e0000),
                                  sizeof(bank7e), bank7e, true) ==
          MIPSNES2_BUS_OK);
    CHECK(mipsnes2_bus_map_direct(&fixture->bus, UINT32_C(0x7f0000),
                                  sizeof(bank7f), bank7f, true) ==
          MIPSNES2_BUS_OK);
    CHECK(mipsnes2_bus_map_direct(&fixture->bus, UINT32_C(0x800000),
                                  sizeof(bank80), bank80, true) ==
          MIPSNES2_BUS_OK);
    CHECK(mipsnes2_bus_map_direct(&fixture->bus, UINT32_C(0xff0000),
                                  sizeof(bankff), bankff, true) ==
          MIPSNES2_BUS_OK);
    bank00[0xfffc] = (uint8_t)reset_pc;
    bank00[0xfffd] = (uint8_t)(reset_pc >> 8);
    interface = mipsnes2_bus_make_interface(&fixture->bus);
    mipsnes2_cpu_init(&fixture->cpu, interface);
    return 0;
}

static int reset_fixture(cpu_fixture *fixture, uint16_t reset_pc)
{
    CHECK(initialize_cpu_fixture(fixture, reset_pc) == 0);
    CHECK(mipsnes2_cpu_reset(&fixture->cpu) == MIPSNES2_CPU_OK);
    return 0;
}

static int test_bus(void)
{
    mipsnes2_bus bus;
    mipsnes2_bus limit_bus;
    mipsnes2_bus_interface null_interface;
    mmio_capture capture = {0U, 0U, 0U, 0U};
    uint8_t direct[4] = {UINT8_C(1), UINT8_C(2), UINT8_C(3), UINT8_C(4)};
    uint8_t read_only = UINT8_C(0x55);
    uint8_t slots[MIPSNES2_BUS_MAX_REGIONS];
    size_t index;

    mipsnes2_bus_init(&bus, UINT8_C(0xa5));
    null_interface = mipsnes2_bus_make_interface(NULL);
    CHECK(null_interface.context == NULL);
    CHECK(null_interface.read8 == NULL);
    CHECK(null_interface.write8 == NULL);
    CHECK(mipsnes2_bus_read(&bus, UINT32_C(0x345678)) == UINT8_C(0xa5));
    mipsnes2_bus_write(&bus, UINT32_C(0x123456), UINT8_C(0x3c));
    CHECK(mipsnes2_bus_read(&bus, UINT32_C(0x654321)) == UINT8_C(0x3c));

    CHECK(mipsnes2_bus_map_direct(&bus, UINT32_C(0x123400), sizeof(direct),
                                  direct, true) == MIPSNES2_BUS_OK);
    CHECK(mipsnes2_bus_read(&bus, UINT32_C(0xab123401)) == UINT8_C(2));
    mipsnes2_bus_write(&bus, UINT32_C(0xff123402), UINT8_C(0x9a));
    CHECK(direct[2] == UINT8_C(0x9a));
    CHECK(mipsnes2_bus_map_direct(&bus, UINT32_C(0x123403), 2U, direct,
                                  true) == MIPSNES2_BUS_OVERLAP);
    CHECK(mipsnes2_bus_map_direct(&bus, UINT32_C(0xffffff), 2U, direct,
                                  true) == MIPSNES2_BUS_OUT_OF_RANGE);
    CHECK(mipsnes2_bus_map_direct(&bus, UINT32_C(0x200000), 1U, NULL,
                                  true) == MIPSNES2_BUS_INVALID_ARGUMENT);

    CHECK(mipsnes2_bus_map_direct(&bus, UINT32_C(0x200000), 1U, &read_only,
                                  false) == MIPSNES2_BUS_OK);
    mipsnes2_bus_write(&bus, UINT32_C(0x200000), UINT8_C(0xee));
    CHECK(read_only == UINT8_C(0x55));
    CHECK(bus.open_value == UINT8_C(0xee));
    CHECK(mipsnes2_bus_read(&bus, UINT32_C(0x200000)) == UINT8_C(0x55));

    CHECK(mipsnes2_bus_map_mmio(&bus, UINT32_C(0x300000), 2U,
                                capture_read, capture_write, &capture) ==
          MIPSNES2_BUS_OK);
    CHECK(mipsnes2_bus_read(&bus, UINT32_C(0x300001)) == UINT8_C(0x6d));
    CHECK(capture.reads == 1U);
    CHECK(capture.last_address == UINT32_C(0x300001));
    mipsnes2_bus_write(&bus, UINT32_C(0x300000), UINT8_C(0x42));
    CHECK(capture.writes == 1U);
    CHECK(capture.last_address == UINT32_C(0x300000));
    CHECK(capture.last_value == UINT8_C(0x42));
    CHECK(mipsnes2_bus_map_mmio(&bus, UINT32_C(0x300010), 1U,
                                capture_read, NULL, &capture) ==
          MIPSNES2_BUS_OK);
    mipsnes2_bus_write(&bus, UINT32_C(0x300010), UINT8_C(0x77));
    CHECK(capture.writes == 1U);
    CHECK(bus.open_value == UINT8_C(0x77));
    CHECK(mipsnes2_bus_map_mmio(&bus, UINT32_C(0x300020), 1U,
                                NULL, capture_write, &capture) ==
          MIPSNES2_BUS_OK);
    CHECK(mipsnes2_bus_read(&bus, UINT32_C(0x300020)) == UINT8_C(0x77));
    CHECK(mipsnes2_bus_map_mmio(&bus, UINT32_C(0x400000), 1U, NULL, NULL,
                                NULL) == MIPSNES2_BUS_INVALID_ARGUMENT);

    mipsnes2_bus_init(&limit_bus, 0U);
    for (index = 0; index < MIPSNES2_BUS_MAX_REGIONS; ++index) {
        CHECK(mipsnes2_bus_map_direct(&limit_bus, (mipsnes2_address)(index * 2U),
                                      1U, &slots[index], true) ==
              MIPSNES2_BUS_OK);
    }
    CHECK(mipsnes2_bus_map_direct(&limit_bus, UINT32_C(0x100), 1U, slots,
                                  true) == MIPSNES2_BUS_REGION_LIMIT);
    return 0;
}

static int test_reset(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    mipsnes2_bus_interface empty = {NULL, NULL, NULL};
    mipsnes2_bus_interface read_only;
    mipsnes2_cpu invalid;

    CHECK(initialize_cpu_fixture(&fixture, UINT16_C(0x9234)) == 0);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_NOT_READY);
    cpu->a = UINT16_C(0xbeef);
    cpu->x = UINT16_C(0xab34);
    cpu->y = UINT16_C(0xcd56);
    cpu->s = UINT16_C(0x9876);
    cpu->d = UINT16_C(0x4321);
    cpu->dbr = UINT8_C(0x12);
    cpu->pbr = UINT8_C(0x34);
    cpu->p = MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
             MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_Z |
             MIPSNES2_CPU_FLAG_C;
    cpu->emulation = false;
    cpu->instruction_cycles = UINT64_C(99);
    CHECK(mipsnes2_cpu_reset(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xbeef));
    CHECK(cpu->x == UINT16_C(0x0034));
    CHECK(cpu->y == UINT16_C(0x0056));
    CHECK(cpu->s == UINT16_C(0x0176));
    CHECK(cpu->d == 0U);
    CHECK(cpu->dbr == 0U);
    CHECK(cpu->pbr == 0U);
    CHECK(cpu->pc == UINT16_C(0x9234));
    CHECK(cpu->emulation);
    CHECK(cpu->p == (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
                     MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
                     MIPSNES2_CPU_FLAG_I | MIPSNES2_CPU_FLAG_Z |
                     MIPSNES2_CPU_FLAG_C));
    CHECK(cpu->instruction_cycles == 0U);
    CHECK(!cpu->stopped);

    mipsnes2_cpu_init(&invalid, empty);
    CHECK(mipsnes2_cpu_reset(&invalid) == MIPSNES2_CPU_INVALID_BUS);
    CHECK(mipsnes2_cpu_step(&invalid) == MIPSNES2_CPU_INVALID_BUS);

    read_only = mipsnes2_bus_make_interface(&fixture.bus);
    read_only.write8 = NULL;
    mipsnes2_cpu_init(&invalid, read_only);
    CHECK(mipsnes2_cpu_reset(&invalid) == MIPSNES2_CPU_OK);
    invalid.pbr = UINT8_C(0x7e);
    bank7e[0x9234] = UINT8_C(0x8f);
    CHECK(mipsnes2_cpu_step(&invalid) == MIPSNES2_CPU_INVALID_BUS);
    CHECK(invalid.stopped);
    CHECK(invalid.pc == UINT16_C(0x9235));
    CHECK(invalid.instruction_cycles == 0U);
    return 0;
}

static int test_emulation_mode_instructions(void)
{
    static const uint8_t program[] = {
        0x18,                   /* CLC */
        0x38,                   /* SEC */
        0xea,                   /* NOP */
        0xa9, 0x80,             /* LDA #$80 */
        0xa2, 0x00,             /* LDX #$00 */
        0xa0, 0x7f,             /* LDY #$7f */
        0x8d, 0x00, 0x20,       /* STA $2000 */
        0x8e, 0x01, 0x20,       /* STX $2001 */
        0x8c, 0x02, 0x20,       /* STY $2002 */
        0x4c, 0x00, 0x90        /* JMP $9000 */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8000)) == 0);
    (void)memcpy(&bank7e[0x8000], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->a = UINT16_C(0x5a00);
    cpu->p |= MIPSNES2_CPU_FLAG_C;

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_C) == 0U);
    CHECK(cpu->pc == UINT16_C(0x8001));
    CHECK(cpu->instruction_cycles == UINT64_C(2));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_C) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a80));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(8));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) == 0U);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x007f));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2000] == UINT8_C(0x80));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2001] == 0U);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2002] == UINT8_C(0x7f));
    CHECK(cpu->instruction_cycles == UINT64_C(24));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x9000));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->instruction_cycles == UINT64_C(27));
    return 0;
}

static int test_width_and_mode_instructions(void)
{
    static const uint8_t program[] = {
        0x18,                   /* CLC */
        0xfb,                   /* XCE: enter native */
        0xc2, 0x30,             /* REP #$30 */
        0xa9, 0x34, 0x12,       /* LDA #$1234 */
        0xa2, 0x78, 0x56,       /* LDX #$5678 */
        0xa0, 0x00, 0x80,       /* LDY #$8000 */
        0x8d, 0xff, 0xff,       /* STA $ffff */
        0x8e, 0x00, 0x10,       /* STX $1000 */
        0x8c, 0x02, 0x10,       /* STY $1002 */
        0xe2, 0x10,             /* SEP #$10 */
        0xe2, 0x20,             /* SEP #$20 */
        0xa9, 0x00,             /* LDA #$00 */
        0x38,                   /* SEC */
        0xfb                    /* XCE: enter emulation */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8000)) == 0);
    (void)memcpy(&bank7e[0x8000], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu->emulation);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_C) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X)) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(7));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x5678));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x8000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(16));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0xffff] == UINT8_C(0x34));
    CHECK(bank80[0x0000] == UINT8_C(0x12));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x1000] == UINT8_C(0x78));
    CHECK(bank7f[0x1001] == UINT8_C(0x56));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x1002] == UINT8_C(0x00));
    CHECK(bank7f[0x1003] == UINT8_C(0x80));
    CHECK(cpu->instruction_cycles == UINT64_C(31));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_X) != 0U);
    CHECK(cpu->x == UINT16_C(0x0078));
    CHECK(cpu->y == 0U);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_M) != 0U);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1200));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(39));

    cpu->s = UINT16_C(0xabcd);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->emulation);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_C) == 0U);
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X)) ==
          (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X));
    CHECK(cpu->s == UINT16_C(0x01cd));
    CHECK(cpu->instruction_cycles == UINT64_C(43));
    return 0;
}

static int test_absolute_loads(void)
{
    static const uint8_t byte_program[] = {
        0xad, 0x00, 0x20,       /* LDA $2000 */
        0xae, 0x01, 0x20,       /* LDX $2001 */
        0xac, 0x02, 0x20        /* LDY $2002 */
    };
    static const uint8_t word_program[] = {
        0xad, 0xff, 0xff,       /* LDA $ffff */
        0xae, 0x10, 0x20,       /* LDX $2010 */
        0xac, 0x12, 0x20        /* LDY $2012 */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8000)) == 0);
    (void)memcpy(&bank7e[0x8000], byte_program, sizeof(byte_program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->a = UINT16_C(0x5a00);
    bank7f[0x2000] = UINT8_C(0x80);
    bank7f[0x2001] = UINT8_C(0x00);
    bank7f[0x2002] = UINT8_C(0x7f);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a80));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(8));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x007f));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(12));

    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    cpu->pc = UINT16_C(0x8100);
    cpu->instruction_cycles = 0U;
    (void)memcpy(&bank7e[0x8100], word_program, sizeof(word_program));
    bank7f[0xffff] = UINT8_C(0x34);
    bank80[0x0000] = UINT8_C(0x12);
    bank7f[0x2010] = UINT8_C(0x00);
    bank7f[0x2011] = UINT8_C(0x80);
    bank7f[0x2012] = UINT8_C(0x00);
    bank7f[0x2013] = UINT8_C(0x00);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(cpu->instruction_cycles == UINT64_C(5));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x8000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(10));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(15));
    return 0;
}

static int test_absolute_indexed_loads_and_stores(void)
{
    static const uint8_t byte_program[] = {
        0xbd, 0x00, 0x20,       /* LDA $2000,X */
        0xb9, 0xf0, 0xff,       /* LDA $fff0,Y */
        0x99, 0x00, 0x21,       /* STA $2100,Y */
        0x9d, 0xff, 0xff        /* STA $ffff,X */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8200)) == 0);
    (void)memcpy(&bank7e[0x8200], byte_program, sizeof(byte_program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->x = UINT16_C(0x0001);
    cpu->y = UINT16_C(0x0020);
    bank7f[0x2001] = UINT8_C(0x11);
    bank80[0x0010] = UINT8_C(0x80);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->a & UINT16_C(0x00ff)) == UINT16_C(0x0011));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->a & UINT16_C(0x00ff)) == UINT16_C(0x0080));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(9));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2120] == UINT8_C(0x80));
    CHECK(cpu->instruction_cycles == UINT64_C(14));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank80[0x0000] == UINT8_C(0x80));
    CHECK(cpu->instruction_cycles == UINT64_C(19));

    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    cpu->x = UINT16_C(0x0001);
    cpu->pc = UINT16_C(0x8300);
    cpu->instruction_cycles = 0U;
    bank7e[0x8300] = UINT8_C(0xbd);
    bank7e[0x8301] = UINT8_C(0xff);
    bank7e[0x8302] = UINT8_C(0xff);
    bank80[0x0000] = UINT8_C(0xcd);
    bank80[0x0001] = UINT8_C(0xab);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xabcd));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->x = UINT16_C(0x0001);
    cpu->pc = UINT16_C(0x8400);
    cpu->instruction_cycles = 0U;
    cpu->a = UINT16_C(0x5678);
    bank7e[0x8400] = UINT8_C(0x9d);
    bank7e[0x8401] = UINT8_C(0xfe);
    bank7e[0x8402] = UINT8_C(0xff);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0xffff] == UINT8_C(0x78));
    CHECK(bank80[0x0000] == UINT8_C(0x56));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->x = UINT16_C(0x0001);
    cpu->y = UINT16_C(0x0002);
    cpu->pc = UINT16_C(0x8500);
    cpu->instruction_cycles = 0U;
    bank7e[0x8500] = UINT8_C(0xbc);
    bank7e[0x8501] = UINT8_C(0x00);
    bank7e[0x8502] = UINT8_C(0x22);
    bank7e[0x8503] = UINT8_C(0xbe);
    bank7e[0x8504] = UINT8_C(0x00);
    bank7e[0x8505] = UINT8_C(0x23);
    bank7f[0x2201] = UINT8_C(0x34);
    bank7f[0x2202] = UINT8_C(0x12);
    bank7f[0x3534] = UINT8_C(0x00);
    bank7f[0x3535] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x1234));
    CHECK(cpu->instruction_cycles == UINT64_C(6));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x8000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(12));
    return 0;
}

static int test_absolute_long_loads_and_stores(void)
{
    static const uint8_t byte_program[] = {
        0xaf, 0x34, 0x12, 0x80, /* LDA $801234 */
        0x8f, 0x00, 0x20, 0x80, /* STA $802000 */
        0xbf, 0xf0, 0xff, 0x7f, /* LDA $7ffff0,X */
        0x9f, 0x00, 0x21, 0x80  /* STA $802100,X */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8600)) == 0);
    (void)memcpy(&bank7e[0x8600], byte_program, sizeof(byte_program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x12);
    cpu->x = UINT16_C(0x0020);
    cpu->a = UINT16_C(0x5a00);
    bank80[0x1234] = UINT8_C(0x7c);
    bank80[0x0010] = UINT8_C(0x80);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a7c));
    CHECK(cpu->instruction_cycles == UINT64_C(5));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank80[0x2000] == UINT8_C(0x7c));
    CHECK(cpu->instruction_cycles == UINT64_C(10));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a80));
    CHECK(cpu->instruction_cycles == UINT64_C(15));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank80[0x2120] == UINT8_C(0x80));
    CHECK(cpu->instruction_cycles == UINT64_C(20));

    cpu->pc = UINT16_C(0xfffe);
    cpu->instruction_cycles = 0U;
    bank7e[0xfffe] = UINT8_C(0xaf);
    bank7e[0xffff] = UINT8_C(0x34);
    bank7e[0x0000] = UINT8_C(0x12);
    bank7e[0x0001] = UINT8_C(0x80);
    bank80[0x1234] = UINT8_C(0x42);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a42));
    CHECK(cpu->pc == UINT16_C(0x0002));
    CHECK(cpu->instruction_cycles == UINT64_C(5));

    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    cpu->x = UINT16_C(0x0001);
    cpu->pc = UINT16_C(0x8700);
    cpu->instruction_cycles = 0U;
    bank7e[0x8700] = UINT8_C(0xaf);
    bank7e[0x8701] = UINT8_C(0xff);
    bank7e[0x8702] = UINT8_C(0xff);
    bank7e[0x8703] = UINT8_C(0xff);
    bankff[0xffff] = UINT8_C(0x34);
    bank00[0x0000] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->a = UINT16_C(0x5678);
    cpu->pc = UINT16_C(0x8800);
    cpu->instruction_cycles = 0U;
    bank7e[0x8800] = UINT8_C(0x8f);
    bank7e[0x8801] = UINT8_C(0xff);
    bank7e[0x8802] = UINT8_C(0xff);
    bank7e[0x8803] = UINT8_C(0x7f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0xffff] == UINT8_C(0x78));
    CHECK(bank80[0x0000] == UINT8_C(0x56));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->pc = UINT16_C(0x8900);
    cpu->instruction_cycles = 0U;
    bank7e[0x8900] = UINT8_C(0xbf);
    bank7e[0x8901] = UINT8_C(0xff);
    bank7e[0x8902] = UINT8_C(0xff);
    bank7e[0x8903] = UINT8_C(0x7f);
    bank80[0x0000] = UINT8_C(0xcd);
    bank80[0x0001] = UINT8_C(0xab);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xabcd));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->a = UINT16_C(0x2468);
    cpu->pc = UINT16_C(0x8a00);
    cpu->instruction_cycles = 0U;
    bank7e[0x8a00] = UINT8_C(0x9f);
    bank7e[0x8a01] = UINT8_C(0xfe);
    bank7e[0x8a02] = UINT8_C(0xff);
    bank7e[0x8a03] = UINT8_C(0x7f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0xffff] == UINT8_C(0x68));
    CHECK(bank80[0x0000] == UINT8_C(0x24));
    CHECK(cpu->instruction_cycles == UINT64_C(6));
    return 0;
}

static int test_direct_page_emulation_loads_and_stores(void)
{
    static const uint8_t program[] = {
        0xa5, 0x10,             /* LDA $10 */
        0xa6, 0x11,             /* LDX $11 */
        0xa4, 0x12,             /* LDY $12 */
        0x85, 0x13,             /* STA $13 */
        0x86, 0x14,             /* STX $14 */
        0x84, 0x15,             /* STY $15 */
        0xb5, 0xf0,             /* LDA $f0,X */
        0xb6, 0xf0,             /* LDX $f0,Y */
        0xb4, 0xf0,             /* LDY $f0,X */
        0x95, 0xf0,             /* STA $f0,X */
        0x96, 0xf0,             /* STX $f0,Y */
        0x94, 0xf0              /* STY $f0,X */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8b00)) == 0);
    (void)memcpy(&bank7e[0x8b00], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->d = UINT16_C(0x1200);
    cpu->a = UINT16_C(0x5a00);
    bank00[0x1210] = UINT8_C(0x80);
    bank00[0x1211] = UINT8_C(0x00);
    bank00[0x1212] = UINT8_C(0x7f);
    bank7f[0x1210] = UINT8_C(0x44);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a80));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(3));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(6));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x007f));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(9));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x1213] == UINT8_C(0x80));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x1214] == UINT8_C(0x00));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x1215] == UINT8_C(0x7f));
    CHECK(bank7f[0x1213] == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(18));

    cpu->x = UINT16_C(0x0020);
    bank00[0x1210] = UINT8_C(0x11);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->a & UINT16_C(0x00ff)) == UINT16_C(0x0011));
    CHECK(cpu->instruction_cycles == UINT64_C(22));

    cpu->y = UINT16_C(0x0030);
    bank00[0x1220] = UINT8_C(0x22);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x0022));
    CHECK(cpu->instruction_cycles == UINT64_C(26));

    cpu->x = UINT16_C(0x0040);
    bank00[0x1230] = UINT8_C(0x33);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x0033));
    CHECK(cpu->instruction_cycles == UINT64_C(30));

    cpu->x = UINT16_C(0x0050);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x1240] == UINT8_C(0x11));
    CHECK(cpu->instruction_cycles == UINT64_C(34));

    cpu->y = UINT16_C(0x0060);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x1250] == UINT8_C(0x50));
    CHECK(cpu->instruction_cycles == UINT64_C(38));

    cpu->x = UINT16_C(0x0070);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x1260] == UINT8_C(0x60));
    CHECK(bank7f[0x1260] == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(42));
    CHECK(cpu->pc == UINT16_C(0x8b18));
    return 0;
}

static int test_direct_page_native_width_and_wrap(void)
{
    static const uint8_t program[] = {
        0xa5, 0xff,             /* LDA $ff */
        0x85, 0xfd,             /* STA $fd */
        0xa6, 0x10,             /* LDX $10 */
        0x86, 0x20,             /* STX $20 */
        0xa4, 0x30,             /* LDY $30 */
        0x84, 0x40,             /* STY $40 */
        0xb5, 0xfe,             /* LDA $fe,X */
        0xb6, 0xfe,             /* LDX $fe,Y */
        0xb4, 0xfe,             /* LDY $fe,X */
        0x95, 0xfe,             /* STA $fe,X */
        0x96, 0xfe,             /* STX $fe,Y */
        0x94, 0xfe              /* STY $fe,X */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8c00)) == 0);
    (void)memcpy(&bank7e[0x8c00], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->d = UINT16_C(0xff00);
    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    bank00[0xffff] = UINT8_C(0x34);
    bank00[0x0000] = UINT8_C(0x12);
    bank00[0xff10] = UINT8_C(0x00);
    bank00[0xff11] = UINT8_C(0x80);
    bank00[0xff30] = UINT8_C(0x00);
    bank00[0xff31] = UINT8_C(0x00);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0xfffd] == UINT8_C(0x34));
    CHECK(bank00[0xfffe] == UINT8_C(0x12));
    CHECK(cpu->instruction_cycles == UINT64_C(8));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x8000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(12));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0xff20] == UINT8_C(0x00));
    CHECK(bank00[0xff21] == UINT8_C(0x80));
    CHECK(cpu->instruction_cycles == UINT64_C(16));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(20));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0xff40] == UINT8_C(0x00));
    CHECK(bank00[0xff41] == UINT8_C(0x00));
    CHECK(cpu->instruction_cycles == UINT64_C(24));

    cpu->x = UINT16_C(0x0001);
    bank00[0xffff] = UINT8_C(0x78);
    bank00[0x0000] = UINT8_C(0x56);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5678));
    CHECK(cpu->instruction_cycles == UINT64_C(29));

    cpu->y = UINT16_C(0x0001);
    bank00[0xffff] = UINT8_C(0xbc);
    bank00[0x0000] = UINT8_C(0x9a);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x9abc));
    CHECK(cpu->instruction_cycles == UINT64_C(34));

    cpu->x = UINT16_C(0x0001);
    bank00[0xffff] = UINT8_C(0x57);
    bank00[0x0000] = UINT8_C(0x13);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x1357));
    CHECK(cpu->instruction_cycles == UINT64_C(39));

    cpu->x = UINT16_C(0x0001);
    cpu->a = UINT16_C(0x2468);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0xffff] == UINT8_C(0x68));
    CHECK(bank00[0x0000] == UINT8_C(0x24));
    CHECK(cpu->instruction_cycles == UINT64_C(44));

    cpu->y = UINT16_C(0x0001);
    cpu->x = UINT16_C(0xabcd);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0xffff] == UINT8_C(0xcd));
    CHECK(bank00[0x0000] == UINT8_C(0xab));
    CHECK(cpu->instruction_cycles == UINT64_C(49));

    cpu->x = UINT16_C(0x0001);
    cpu->y = UINT16_C(0xbeef);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0xffff] == UINT8_C(0xef));
    CHECK(bank00[0x0000] == UINT8_C(0xbe));
    CHECK(cpu->instruction_cycles == UINT64_C(54));
    CHECK(cpu->pc == UINT16_C(0x8c18));
    return 0;
}

static int test_direct_page_unaligned_penalty(void)
{
    static const uint8_t byte_program[] = {
        0xa5, 0x20,             /* LDA $20 */
        0xb5, 0xf0,             /* LDA $f0,X */
        0x85, 0x0f,             /* STA $0f */
        0x95, 0xf0              /* STA $f0,X */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8d00)) == 0);
    (void)memcpy(&bank7e[0x8d00], byte_program, sizeof(byte_program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->d = UINT16_C(0x12f1);
    cpu->a = UINT16_C(0x5a00);
    bank00[0x1311] = UINT8_C(0x80);
    bank00[0x1401] = UINT8_C(0x42);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a80));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    cpu->x = UINT16_C(0x0020);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a42));
    CHECK(cpu->instruction_cycles == UINT64_C(9));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x1300] == UINT8_C(0x42));
    CHECK(cpu->instruction_cycles == UINT64_C(13));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x1401] == UINT8_C(0x42));
    CHECK(cpu->instruction_cycles == UINT64_C(18));

    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    cpu->pc = UINT16_C(0x8e00);
    cpu->instruction_cycles = 0U;
    bank7e[0x8e00] = UINT8_C(0xa5);
    bank7e[0x8e01] = UINT8_C(0x0f);
    bank7e[0x8e02] = UINT8_C(0xb5);
    bank7e[0x8e03] = UINT8_C(0x0e);
    bank00[0x1300] = UINT8_C(0x00);
    bank00[0x1301] = UINT8_C(0x80);
    cpu->x = UINT16_C(0x0001);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x8000));
    CHECK(cpu->instruction_cycles == UINT64_C(5));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x8000));
    CHECK(cpu->instruction_cycles == UINT64_C(11));
    return 0;
}

static int test_direct_indirect_emulation_pointer_rules(void)
{
    static const uint8_t program[] = {
        0xa1, 0xf0,             /* LDA ($f0,X) */
        0x81, 0xf0,             /* STA ($f0,X) */
        0xb2, 0xff,             /* LDA ($ff) */
        0x92, 0xff,             /* STA ($ff) */
        0xb1, 0xff,             /* LDA ($ff),Y */
        0x91, 0xff,             /* STA ($ff),Y */
        0xa7, 0xff,             /* LDA [$ff] */
        0x87, 0xff,             /* STA [$ff] */
        0xb7, 0xff,             /* LDA [$ff],Y */
        0x97, 0xff              /* STA [$ff],Y */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8f00)) == 0);
    (void)memcpy(&bank7e[0x8f00], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->d = UINT16_C(0x1200);
    cpu->a = UINT16_C(0x5a00);
    cpu->x = UINT16_C(0x000f);
    cpu->y = UINT16_C(0x0001);

    bank00[0x12ff] = UINT8_C(0x00);
    bank00[0x1200] = UINT8_C(0x20);
    bank00[0x1300] = UINT8_C(0x30);
    bank00[0x1301] = UINT8_C(0x80);
    bank00[0x1201] = UINT8_C(0x55);
    bank7f[0x2000] = UINT8_C(0x80);
    bank7f[0x2001] = UINT8_C(0x00);
    bank80[0x3000] = UINT8_C(0x7f);
    bank80[0x3001] = UINT8_C(0x81);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a80));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->a = UINT16_C(0x5a42);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2000] == UINT8_C(0x42));
    CHECK(cpu->instruction_cycles == UINT64_C(12));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a42));
    CHECK(cpu->instruction_cycles == UINT64_C(17));

    cpu->a = UINT16_C(0x5a33);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2000] == UINT8_C(0x33));
    CHECK(cpu->instruction_cycles == UINT64_C(22));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a00));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(27));

    cpu->a = UINT16_C(0x5a44);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2001] == UINT8_C(0x44));
    CHECK(cpu->instruction_cycles == UINT64_C(33));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a7f));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(39));

    cpu->a = UINT16_C(0x5a55);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank80[0x3000] == UINT8_C(0x55));
    CHECK(cpu->instruction_cycles == UINT64_C(45));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5a81));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(51));

    cpu->a = UINT16_C(0x5a66);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank80[0x3001] == UINT8_C(0x66));
    CHECK(cpu->instruction_cycles == UINT64_C(57));
    CHECK(cpu->pc == UINT16_C(0x8f14));
    return 0;
}

static int test_direct_indirect_native_width_and_wrap(void)
{
    static const uint8_t program[] = {
        0xa1, 0xff,             /* LDA ($ff,X) */
        0x81, 0xff,             /* STA ($ff,X) */
        0xb2, 0xff,             /* LDA ($ff) */
        0x92, 0xff,             /* STA ($ff) */
        0xb1, 0xff,             /* LDA ($ff),Y */
        0x91, 0xff,             /* STA ($ff),Y */
        0xa7, 0xff,             /* LDA [$ff] */
        0x87, 0xff,             /* STA [$ff] */
        0xb7, 0xff,             /* LDA [$ff],Y */
        0x97, 0xff              /* STA [$ff],Y */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9000)) == 0);
    (void)memcpy(&bank7e[0x9000], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->d = UINT16_C(0xff00);
    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    cpu->x = 0U;
    cpu->y = UINT16_C(0x0001);

    bank00[0xffff] = UINT8_C(0xfe);
    bank00[0x0000] = UINT8_C(0xff);
    bank7f[0xfffe] = UINT8_C(0x34);
    bank7f[0xffff] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(cpu->instruction_cycles == UINT64_C(7));

    cpu->a = UINT16_C(0x5678);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0xfffe] == UINT8_C(0x78));
    CHECK(bank7f[0xffff] == UINT8_C(0x56));
    CHECK(cpu->instruction_cycles == UINT64_C(14));

    bank00[0xffff] = UINT8_C(0xff);
    bank00[0x0000] = UINT8_C(0xff);
    bank7f[0xffff] = UINT8_C(0xbc);
    bank80[0x0000] = UINT8_C(0x9a);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x9abc));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(20));

    cpu->a = UINT16_C(0x2468);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0xffff] == UINT8_C(0x68));
    CHECK(bank80[0x0000] == UINT8_C(0x24));
    CHECK(cpu->instruction_cycles == UINT64_C(26));

    bank00[0xffff] = UINT8_C(0xff);
    bank00[0x0000] = UINT8_C(0xff);
    bank80[0x0000] = UINT8_C(0x57);
    bank80[0x0001] = UINT8_C(0x13);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1357));
    CHECK(cpu->instruction_cycles == UINT64_C(33));

    cpu->a = UINT16_C(0xbeef);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank80[0x0000] == UINT8_C(0xef));
    CHECK(bank80[0x0001] == UINT8_C(0xbe));
    CHECK(cpu->instruction_cycles == UINT64_C(40));

    bank00[0xffff] = UINT8_C(0xff);
    bank00[0x0000] = UINT8_C(0xff);
    bank00[0x0001] = UINT8_C(0x7e);
    bank7e[0xffff] = UINT8_C(0xad);
    bank7f[0x0000] = UINT8_C(0x0b);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x0bad));
    CHECK(cpu->instruction_cycles == UINT64_C(47));

    cpu->a = UINT16_C(0xcdef);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7e[0xffff] == UINT8_C(0xef));
    CHECK(bank7f[0x0000] == UINT8_C(0xcd));
    CHECK(cpu->instruction_cycles == UINT64_C(54));

    bank00[0xffff] = UINT8_C(0xff);
    bank00[0x0000] = UINT8_C(0xff);
    bank00[0x0001] = UINT8_C(0x7f);
    bank80[0x0000] = UINT8_C(0x43);
    bank80[0x0001] = UINT8_C(0x42);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x4243));
    CHECK(cpu->instruction_cycles == UINT64_C(61));

    cpu->a = UINT16_C(0x600d);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank80[0x0000] == UINT8_C(0x0d));
    CHECK(bank80[0x0001] == UINT8_C(0x60));
    CHECK(cpu->instruction_cycles == UINT64_C(68));
    CHECK(cpu->pc == UINT16_C(0x9014));
    return 0;
}

static int test_direct_indirect_cycle_penalties(void)
{
    static const uint8_t program[] = {
        0xb1, 0x10,             /* LDA ($10),Y: no page cross */
        0xb1, 0x10,             /* LDA ($10),Y: page cross */
        0x91, 0x10,             /* STA ($10),Y */
        0xa1, 0x10,             /* LDA ($10,X) */
        0x81, 0x10,             /* STA ($10,X) */
        0xb2, 0x10,             /* LDA ($10) */
        0x92, 0x10,             /* STA ($10) */
        0xa7, 0x10,             /* LDA [$10] */
        0x87, 0x10,             /* STA [$10] */
        0xb7, 0x10,             /* LDA [$10],Y */
        0x97, 0x10              /* STA [$10],Y */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9100)) == 0);
    (void)memcpy(&bank7e[0x9100], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->d = UINT16_C(0x1201);
    cpu->x = 0U;
    cpu->y = UINT16_C(0x0001);
    bank00[0x1211] = UINT8_C(0x00);
    bank00[0x1212] = UINT8_C(0x20);
    bank00[0x1213] = UINT8_C(0x7f);
    bank7f[0x2000] = UINT8_C(0x11);
    bank7f[0x2001] = UINT8_C(0x22);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x0022));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    bank00[0x1211] = UINT8_C(0xff);
    bank7f[0x2100] = UINT8_C(0x33);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x0033));
    CHECK(cpu->instruction_cycles == UINT64_C(13));

    bank00[0x1211] = UINT8_C(0x00);
    cpu->a = UINT16_C(0x0044);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2001] == UINT8_C(0x44));
    CHECK(cpu->instruction_cycles == UINT64_C(20));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(27));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(34));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(40));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(46));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(53));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(60));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(67));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->instruction_cycles == UINT64_C(74));

    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    cpu->pc = UINT16_C(0x9200);
    cpu->instruction_cycles = 0U;
    bank7e[0x9200] = UINT8_C(0xb1);
    bank7e[0x9201] = UINT8_C(0x10);
    bank7e[0x9202] = UINT8_C(0x91);
    bank7e[0x9203] = UINT8_C(0x10);
    bank00[0x1211] = UINT8_C(0x00);
    bank00[0x1212] = UINT8_C(0x20);
    bank7f[0x2001] = UINT8_C(0x34);
    bank7f[0x2002] = UINT8_C(0x12);
    cpu->a = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(cpu->instruction_cycles == UINT64_C(8));
    cpu->a = UINT16_C(0xabcd);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x2001] == UINT8_C(0xcd));
    CHECK(bank7f[0x2002] == UINT8_C(0xab));
    CHECK(cpu->instruction_cycles == UINT64_C(16));
    return 0;
}

static int test_register_transfers_emulation_width(void)
{
    static const uint8_t program[] = {
        0xaa, /* TAX */
        0xa8, /* TAY */
        0x8a, /* TXA */
        0x98, /* TYA */
        0x9b, /* TXY */
        0xbb  /* TYX */
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9300)) == 0);
    (void)memcpy(&bank7e[0x9300], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->p |= preserved;

    cpu->a = UINT16_C(0x3480);
    cpu->x = UINT16_C(0xabcd);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x0080));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) == 0U);

    cpu->a = UINT16_C(0x5600);
    cpu->y = UINT16_C(0xcdef);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) == 0U);

    cpu->x = UINT16_C(0x0081);
    cpu->a = UINT16_C(0xab11);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xab81));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);

    cpu->y = 0U;
    cpu->a = UINT16_C(0xcdff);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xcd00));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);

    cpu->x = UINT16_C(0x007f);
    cpu->y = UINT16_C(0xbeef);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x007f));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);

    cpu->y = UINT16_C(0x0080);
    cpu->x = UINT16_C(0xcafe);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x0080));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) == 0U);
    CHECK((cpu->p & preserved) == preserved);
    CHECK(cpu->instruction_cycles == UINT64_C(12));
    CHECK(cpu->pc == UINT16_C(0x9306));
    return 0;
}

static int test_register_transfers_native_width(void)
{
    static const uint8_t program[] = {
        0xaa, /* TAX */
        0xa8, /* TAY */
        0x8a, /* TXA */
        0x98, /* TYA */
        0x9b, /* TXY */
        0xbb  /* TYX */
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9310)) == 0);
    (void)memcpy(&bank7e[0x9310], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->emulation = false;
    cpu->p = preserved;

    cpu->a = UINT16_C(0x8000);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x8000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);

    cpu->a = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);

    cpu->x = UINT16_C(0x1234);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);

    cpu->y = UINT16_C(0x8001);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x8001));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);

    cpu->x = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);

    cpu->y = UINT16_C(0x7fff);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x7fff));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);
    CHECK((cpu->p & preserved) == preserved);
    CHECK(cpu->instruction_cycles == UINT64_C(12));
    CHECK(cpu->pc == UINT16_C(0x9316));
    return 0;
}

static int test_register_transfers_mixed_widths(void)
{
    static const uint8_t program[] = {
        0xaa, /* TAX: M=1, X=0 */
        0xa8, /* TAY: M=1, X=0 */
        0x8a, /* TXA: M=1, X=0 */
        0x98, /* TYA: M=1, X=0 */
        0xaa, /* TAX: M=0, X=1 */
        0xa8, /* TAY: M=0, X=1 */
        0x8a, /* TXA: M=0, X=1 */
        0x98  /* TYA: M=0, X=1 */
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9320)) == 0);
    (void)memcpy(&bank7e[0x9320], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->emulation = false;
    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_M);

    cpu->a = UINT16_C(0x8000);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x8000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x8000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);

    cpu->x = UINT16_C(0x0080);
    cpu->a = UINT16_C(0x5511);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x5580));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);

    cpu->y = UINT16_C(0x1200);
    cpu->a = UINT16_C(0x66ff);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x6600));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);

    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_X);
    cpu->x &= UINT16_C(0x00ff);
    cpu->y &= UINT16_C(0x00ff);
    cpu->a = UINT16_C(0x1280);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x0080));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);

    cpu->a = UINT16_C(0x1200);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);

    cpu->x = UINT16_C(0x0080);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x0080));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);

    cpu->y = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) == 0U);
    CHECK((cpu->p & preserved) == preserved);
    CHECK(cpu->instruction_cycles == UINT64_C(16));
    CHECK(cpu->pc == UINT16_C(0x9328));
    return 0;
}

static int test_special_transfers_emulation_mode(void)
{
    static const uint8_t program[] = {
        0x1b, /* TCS */
        0x3b, /* TSC */
        0x5b, /* TCD */
        0x7b, /* TDC */
        0xba, /* TSX */
        0x9a  /* TXS */
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V;
    const uint8_t widths = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9340)) == 0);
    (void)memcpy(&bank7e[0x9340], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);

    cpu->p = (uint8_t)(preserved | widths | MIPSNES2_CPU_FLAG_N |
                       MIPSNES2_CPU_FLAG_Z);
    cpu->a = UINT16_C(0xabcd);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->s == UINT16_C(0x01cd));
    CHECK(cpu->p == (uint8_t)(preserved | widths |
                              MIPSNES2_CPU_FLAG_N |
                              MIPSNES2_CPU_FLAG_Z));

    cpu->p = (uint8_t)(preserved | widths | MIPSNES2_CPU_FLAG_Z);
    cpu->s = UINT16_C(0x0180);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x0180));
    CHECK(cpu->p == (uint8_t)(preserved | widths));

    cpu->a = UINT16_C(0x8000);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->d == UINT16_C(0x8000));
    CHECK(cpu->p == (uint8_t)(preserved | widths |
                              MIPSNES2_CPU_FLAG_N));

    cpu->d = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == 0U);
    CHECK(cpu->p == (uint8_t)(preserved | widths |
                              MIPSNES2_CPU_FLAG_Z));

    cpu->s = UINT16_C(0x0180);
    cpu->x = UINT16_C(0xabcd);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x0080));
    CHECK(cpu->p == (uint8_t)(preserved | widths |
                              MIPSNES2_CPU_FLAG_N));

    cpu->p = (uint8_t)(preserved | widths | MIPSNES2_CPU_FLAG_N |
                       MIPSNES2_CPU_FLAG_Z);
    cpu->x = UINT16_C(0x0042);
    cpu->s = UINT16_C(0x01aa);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->s == UINT16_C(0x0142));
    CHECK(cpu->p == (uint8_t)(preserved | widths |
                              MIPSNES2_CPU_FLAG_N |
                              MIPSNES2_CPU_FLAG_Z));
    CHECK(cpu->instruction_cycles == UINT64_C(12));
    CHECK(cpu->pc == UINT16_C(0x9346));
    return 0;
}

static int test_special_transfers_native_mode(void)
{
    static const uint8_t program[] = {
        0x1b, /* TCS */
        0x3b, /* TSC */
        0x5b, /* TCD */
        0x7b, /* TDC */
        0xba, /* TSX */
        0x9a  /* TXS */
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9350)) == 0);
    (void)memcpy(&bank7e[0x9350], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->emulation = false;

    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_N |
                       MIPSNES2_CPU_FLAG_Z);
    cpu->a = UINT16_C(0xabcd);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->s == UINT16_C(0xabcd));
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_N |
                              MIPSNES2_CPU_FLAG_Z));

    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_Z);
    cpu->s = UINT16_C(0x8000);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x8000));
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_N));

    cpu->a = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->d == 0U);
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_Z));

    cpu->d = UINT16_C(0x1234);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(cpu->p == preserved);

    cpu->s = UINT16_C(0x8001);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x8001));
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_N));

    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_N |
                       MIPSNES2_CPU_FLAG_Z);
    cpu->x = UINT16_C(0x4567);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->s == UINT16_C(0x4567));
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_N |
                              MIPSNES2_CPU_FLAG_Z));
    CHECK(cpu->instruction_cycles == UINT64_C(12));
    CHECK(cpu->pc == UINT16_C(0x9356));
    return 0;
}

static int test_stack_transfers_native_8_bit_index(void)
{
    static const uint8_t program[] = {
        0xba, /* TSX */
        0x9a  /* TXS */
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9360)) == 0);
    (void)memcpy(&bank7e[0x9360], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->emulation = false;
    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_X);

    cpu->s = UINT16_C(0x0180);
    cpu->x = UINT16_C(0xabcd);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x0080));
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_N));

    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_X |
                       MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z);
    cpu->x = UINT16_C(0x0042);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->s == UINT16_C(0x0042));
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_N |
                              MIPSNES2_CPU_FLAG_Z));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(cpu->pc == UINT16_C(0x9362));
    return 0;
}

static int test_xba_width_independence(void)
{
    static const uint8_t program[] = {
        0xeb, /* XBA in emulation mode */
        0xeb  /* XBA in native 16-bit accumulator mode */
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9370)) == 0);
    (void)memcpy(&bank7e[0x9370], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_M |
                       MIPSNES2_CPU_FLAG_X | MIPSNES2_CPU_FLAG_Z);
    cpu->a = UINT16_C(0x8012);

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1280));
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_M |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_N));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->emulation = false;
    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_N);
    cpu->a = UINT16_C(0x007f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x7f00));
    CHECK(cpu->p == (uint8_t)(preserved | MIPSNES2_CPU_FLAG_Z));
    CHECK(cpu->instruction_cycles == UINT64_C(6));
    CHECK(cpu->pc == UINT16_C(0x9372));
    return 0;
}

static int test_status_control_instructions(void)
{
    static const uint8_t program[] = {
        0xd8, /* CLD */
        0x58, /* CLI */
        0xb8, /* CLV */
        0xf8, /* SED */
        0x78  /* SEI */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    uint16_t initial_a;
    uint16_t initial_x;
    uint16_t initial_y;
    uint16_t initial_s;
    uint16_t initial_d;

    CHECK(reset_fixture(&fixture, UINT16_C(0x93a0)) == 0);
    (void)memcpy(&bank7e[0x93a0], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x45);
    cpu->a = UINT16_C(0x1234);
    cpu->x = UINT16_C(0x0056);
    cpu->y = UINT16_C(0x0078);
    cpu->s = UINT16_C(0x019a);
    cpu->d = UINT16_C(0xbcde);
    cpu->p = UINT8_C(0xff);
    initial_a = cpu->a;
    initial_x = cpu->x;
    initial_y = cpu->y;
    initial_s = cpu->s;
    initial_d = cpu->d;

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->p == UINT8_C(0xf7));
    CHECK(cpu->pc == UINT16_C(0x93a1));
    CHECK(cpu->instruction_cycles == UINT64_C(2));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->p == UINT8_C(0xf3));
    CHECK(cpu->pc == UINT16_C(0x93a2));
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->p == UINT8_C(0xb3));
    CHECK(cpu->pc == UINT16_C(0x93a3));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->p == UINT8_C(0xbb));
    CHECK(cpu->pc == UINT16_C(0x93a4));
    CHECK(cpu->instruction_cycles == UINT64_C(8));

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->p == UINT8_C(0xbf));
    CHECK(cpu->pc == UINT16_C(0x93a5));
    CHECK(cpu->instruction_cycles == UINT64_C(10));
    CHECK(cpu->emulation);
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->dbr == UINT8_C(0x45));
    CHECK(cpu->a == initial_a);
    CHECK(cpu->x == initial_x);
    CHECK(cpu->y == initial_y);
    CHECK(cpu->s == initial_s);
    CHECK(cpu->d == initial_d);

    CHECK(reset_fixture(&fixture, UINT16_C(0x93b0)) == 0);
    (void)memcpy(&bank7e[0x93b0], program, sizeof(program));
    cpu->pbr = UINT8_C(0x7e);
    cpu->emulation = false;
    cpu->p = MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
             MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
             MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_I |
             MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C;

    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu->emulation);
    CHECK(cpu->p == UINT8_C(0xbf));
    CHECK(cpu->pc == UINT16_C(0x93b5));
    CHECK(cpu->instruction_cycles == UINT64_C(10));
    return 0;
}

static int test_accumulator_increment_decrement(void)
{
    static const struct {
        uint8_t opcode;
        bool emulation;
        bool accumulator_8_bit;
        uint16_t before;
        uint16_t after;
        uint8_t expected_nz;
    } cases[] = {
        {UINT8_C(0x1a), true, true, UINT16_C(0x55ff),
         UINT16_C(0x5500), MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x3a), true, true, UINT16_C(0x5500),
         UINT16_C(0x55ff), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x1a), false, true, UINT16_C(0xab7f),
         UINT16_C(0xab80), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x3a), false, true, UINT16_C(0xab01),
         UINT16_C(0xab00), MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x1a), false, false, UINT16_C(0xffff),
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x1a), false, false, UINT16_C(0x7fff),
         UINT16_C(0x8000), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x3a), false, false, UINT16_C(0x0000),
         UINT16_C(0xffff), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x3a), false, false, UINT16_C(0x0001),
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V |
                              MIPSNES2_CPU_FLAG_X;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint8_t width_flags = cases[index].accumulator_8_bit
                                  ? MIPSNES2_CPU_FLAG_M
                                  : 0U;

        CHECK(reset_fixture(&fixture, UINT16_C(0x93c0)) == 0);
        bank7e[0x93c0] = cases[index].opcode;
        cpu->pbr = UINT8_C(0x7e);
        cpu->dbr = UINT8_C(0x45);
        cpu->emulation = cases[index].emulation;
        cpu->p = (uint8_t)(preserved | width_flags |
                           MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z);
        cpu->a = cases[index].before;
        cpu->x = UINT16_C(0x0056);
        cpu->y = UINT16_C(0x0078);
        cpu->s = UINT16_C(0x019a);
        cpu->d = UINT16_C(0xbcde);

        CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
        CHECK(cpu->a == cases[index].after);
        CHECK(cpu->p == (uint8_t)(preserved | width_flags |
                                  cases[index].expected_nz));
        CHECK(cpu->emulation == cases[index].emulation);
        CHECK(cpu->x == UINT16_C(0x0056));
        CHECK(cpu->y == UINT16_C(0x0078));
        CHECK(cpu->s == UINT16_C(0x019a));
        CHECK(cpu->d == UINT16_C(0xbcde));
        CHECK(cpu->dbr == UINT8_C(0x45));
        CHECK(cpu->pbr == UINT8_C(0x7e));
        CHECK(cpu->pc == UINT16_C(0x93c1));
        CHECK(cpu->instruction_cycles == UINT64_C(2));
    }
    return 0;
}

static int test_index_increment_decrement(void)
{
    static const struct {
        uint8_t opcode;
        bool emulation;
        bool index_8_bit;
        bool target_x;
        uint16_t before;
        uint16_t after;
        uint8_t expected_nz;
    } cases[] = {
        {UINT8_C(0xe8), true, true, true, UINT16_C(0x00ff),
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0xca), true, true, true, UINT16_C(0x0000),
         UINT16_C(0x00ff), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0xc8), true, true, false, UINT16_C(0x007f),
         UINT16_C(0x0080), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x88), true, true, false, UINT16_C(0x0001),
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0xe8), false, true, true, UINT16_C(0x007f),
         UINT16_C(0x0080), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0xca), false, true, true, UINT16_C(0x0001),
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0xc8), false, true, false, UINT16_C(0x00ff),
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x88), false, true, false, UINT16_C(0x0000),
         UINT16_C(0x00ff), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0xe8), false, false, true, UINT16_C(0xffff),
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0xca), false, false, true, UINT16_C(0x0000),
         UINT16_C(0xffff), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0xc8), false, false, false, UINT16_C(0x7fff),
         UINT16_C(0x8000), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x88), false, false, false, UINT16_C(0x0001),
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_M |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint8_t width_flags = cases[index].index_8_bit
                                  ? MIPSNES2_CPU_FLAG_X
                                  : 0U;
        uint16_t other_value = cases[index].index_8_bit
                                   ? UINT16_C(0x0056)
                                   : UINT16_C(0x3456);

        CHECK(reset_fixture(&fixture, UINT16_C(0x93d0)) == 0);
        bank7e[0x93d0] = cases[index].opcode;
        cpu->pbr = UINT8_C(0x7e);
        cpu->dbr = UINT8_C(0x45);
        cpu->emulation = cases[index].emulation;
        cpu->p = (uint8_t)(preserved | width_flags |
                           MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z);
        cpu->a = UINT16_C(0xabcd);
        cpu->x = cases[index].target_x ? cases[index].before : other_value;
        cpu->y = cases[index].target_x ? other_value : cases[index].before;
        cpu->s = UINT16_C(0x019a);
        cpu->d = UINT16_C(0xbcde);

        CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
        CHECK(cpu->x == (cases[index].target_x
                             ? cases[index].after
                             : other_value));
        CHECK(cpu->y == (cases[index].target_x
                             ? other_value
                             : cases[index].after));
        CHECK(cpu->p == (uint8_t)(preserved | width_flags |
                                  cases[index].expected_nz));
        CHECK(cpu->emulation == cases[index].emulation);
        CHECK(cpu->a == UINT16_C(0xabcd));
        CHECK(cpu->s == UINT16_C(0x019a));
        CHECK(cpu->d == UINT16_C(0xbcde));
        CHECK(cpu->dbr == UINT8_C(0x45));
        CHECK(cpu->pbr == UINT8_C(0x7e));
        CHECK(cpu->pc == UINT16_C(0x93d1));
        CHECK(cpu->instruction_cycles == UINT64_C(2));
    }
    return 0;
}

static int test_logic_foundation(void)
{
    enum operand_mode {
        OPERAND_IMMEDIATE,
        OPERAND_DIRECT,
        OPERAND_ABSOLUTE
    };
    static const struct {
        uint8_t opcode;
        enum operand_mode mode;
        uint16_t before8;
        uint16_t operand8;
        uint16_t result8;
        uint8_t nz8;
        uint16_t before16;
        uint16_t operand16;
        uint16_t result16;
        uint8_t nz16;
    } cases[] = {
        {UINT8_C(0x09), OPERAND_IMMEDIATE, UINT16_C(0xab50),
         UINT16_C(0x000f), UINT16_C(0xab5f), 0U, UINT16_C(0x4080),
         UINT16_C(0x8001), UINT16_C(0xc081), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x05), OPERAND_DIRECT, UINT16_C(0xab50),
         UINT16_C(0x000f), UINT16_C(0xab5f), 0U, UINT16_C(0x4080),
         UINT16_C(0x8001), UINT16_C(0xc081), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x0d), OPERAND_ABSOLUTE, UINT16_C(0xab50),
         UINT16_C(0x000f), UINT16_C(0xab5f), 0U, UINT16_C(0x4080),
         UINT16_C(0x8001), UINT16_C(0xc081), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0x29), OPERAND_IMMEDIATE, UINT16_C(0xabf0),
         UINT16_C(0x000f), UINT16_C(0xab00), MIPSNES2_CPU_FLAG_Z,
         UINT16_C(0xf0f0), UINT16_C(0x0ff0), UINT16_C(0x00f0), 0U},
        {UINT8_C(0x25), OPERAND_DIRECT, UINT16_C(0xabf0),
         UINT16_C(0x000f), UINT16_C(0xab00), MIPSNES2_CPU_FLAG_Z,
         UINT16_C(0xf0f0), UINT16_C(0x0ff0), UINT16_C(0x00f0), 0U},
        {UINT8_C(0x2d), OPERAND_ABSOLUTE, UINT16_C(0xabf0),
         UINT16_C(0x000f), UINT16_C(0xab00), MIPSNES2_CPU_FLAG_Z,
         UINT16_C(0xf0f0), UINT16_C(0x0ff0), UINT16_C(0x00f0), 0U},
        {UINT8_C(0x49), OPERAND_IMMEDIATE, UINT16_C(0xab7f),
         UINT16_C(0x00ff), UINT16_C(0xab80), MIPSNES2_CPU_FLAG_N,
         UINT16_C(0xffff), UINT16_C(0xffff), UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x45), OPERAND_DIRECT, UINT16_C(0xab7f),
         UINT16_C(0x00ff), UINT16_C(0xab80), MIPSNES2_CPU_FLAG_N,
         UINT16_C(0xffff), UINT16_C(0xffff), UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x4d), OPERAND_ABSOLUTE, UINT16_C(0xab7f),
         UINT16_C(0x00ff), UINT16_C(0xab80), MIPSNES2_CPU_FLAG_N,
         UINT16_C(0xffff), UINT16_C(0xffff), UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_Z}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t before = wide != 0U ? cases[index].before16
                                        : cases[index].before8;
            uint16_t operand = wide != 0U ? cases[index].operand16
                                         : cases[index].operand8;
            uint16_t expected = wide != 0U ? cases[index].result16
                                          : cases[index].result8;
            uint8_t expected_nz = wide != 0U ? cases[index].nz16
                                            : cases[index].nz8;
            uint16_t expected_pc;
            uint64_t expected_cycles;

            CHECK(reset_fixture(&fixture, UINT16_C(0x9400)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = UINT16_C(0x1201);
            cpu->emulation = false;
            cpu->p = (uint8_t)(preserved |
                               (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                               MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z);
            cpu->a = before;
            cpu->x = UINT16_C(0x0020);
            cpu->y = UINT16_C(0x0030);
            bank7e[0x9400] = cases[index].opcode;

            if (cases[index].mode == OPERAND_IMMEDIATE) {
                bank7e[0x9401] = (uint8_t)operand;
                bank7e[0x9402] = (uint8_t)(operand >> 8);
                expected_pc = (uint16_t)(wide != 0U ? 0x9403 : 0x9402);
                expected_cycles = UINT64_C(2) + wide;
            } else if (cases[index].mode == OPERAND_DIRECT) {
                bank7e[0x9401] = UINT8_C(0x34);
                bank00[0x1235] = (uint8_t)operand;
                bank00[0x1236] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9402);
                expected_cycles = UINT64_C(4) + wide;
            } else {
                bank7e[0x9401] = UINT8_C(0xff);
                bank7e[0x9402] = UINT8_C(0xff);
                bank7f[0xffff] = (uint8_t)operand;
                bank80[0x0000] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9403);
                expected_cycles = UINT64_C(4) + wide;
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a == expected);
            CHECK(cpu->p == (uint8_t)(preserved |
                                      (wide != 0U
                                           ? 0U
                                           : MIPSNES2_CPU_FLAG_M) |
                                      expected_nz));
            CHECK(cpu->pc == expected_pc);
            CHECK(cpu->instruction_cycles == expected_cycles);
            CHECK(cpu->x == UINT16_C(0x0020));
            CHECK(cpu->y == UINT16_C(0x0030));
            CHECK(cpu->d == UINT16_C(0x1201));
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->emulation);
        }
    }

    CHECK(reset_fixture(&fixture, UINT16_C(0x9410)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->a = UINT16_C(0xaa00);
    cpu->p = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X;
    bank7e[0x9410] = UINT8_C(0x09);
    bank7e[0x9411] = UINT8_C(0x00);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xaa00));
    CHECK(cpu->p == (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
                     MIPSNES2_CPU_FLAG_Z));
    CHECK(cpu->emulation);
    CHECK(cpu->pc == UINT16_C(0x9412));
    CHECK(cpu->instruction_cycles == UINT64_C(2));
    return 0;
}

static int test_accumulator_extended_addressing(void)
{
    enum accumulator_operation {
        OPERATION_ORA,
        OPERATION_AND,
        OPERATION_EOR,
        OPERATION_CMP
    };
    enum logic_address_mode {
        LOGIC_DIRECT_INDEXED_INDIRECT,
        LOGIC_DIRECT_INDIRECT_LONG,
        LOGIC_ABSOLUTE_LONG,
        LOGIC_DIRECT_INDIRECT_INDEXED,
        LOGIC_DIRECT_INDIRECT,
        LOGIC_DIRECT_X,
        LOGIC_DIRECT_INDIRECT_LONG_INDEXED,
        LOGIC_ABSOLUTE_Y,
        LOGIC_ABSOLUTE_X,
        LOGIC_ABSOLUTE_LONG_X
    };
    static const struct {
        uint8_t opcode;
        enum accumulator_operation operation;
        enum logic_address_mode mode;
    } cases[] = {
        {UINT8_C(0x01), OPERATION_ORA, LOGIC_DIRECT_INDEXED_INDIRECT},
        {UINT8_C(0x07), OPERATION_ORA, LOGIC_DIRECT_INDIRECT_LONG},
        {UINT8_C(0x0f), OPERATION_ORA, LOGIC_ABSOLUTE_LONG},
        {UINT8_C(0x11), OPERATION_ORA, LOGIC_DIRECT_INDIRECT_INDEXED},
        {UINT8_C(0x12), OPERATION_ORA, LOGIC_DIRECT_INDIRECT},
        {UINT8_C(0x15), OPERATION_ORA, LOGIC_DIRECT_X},
        {UINT8_C(0x17), OPERATION_ORA,
         LOGIC_DIRECT_INDIRECT_LONG_INDEXED},
        {UINT8_C(0x19), OPERATION_ORA, LOGIC_ABSOLUTE_Y},
        {UINT8_C(0x1d), OPERATION_ORA, LOGIC_ABSOLUTE_X},
        {UINT8_C(0x1f), OPERATION_ORA, LOGIC_ABSOLUTE_LONG_X},
        {UINT8_C(0x21), OPERATION_AND, LOGIC_DIRECT_INDEXED_INDIRECT},
        {UINT8_C(0x27), OPERATION_AND, LOGIC_DIRECT_INDIRECT_LONG},
        {UINT8_C(0x2f), OPERATION_AND, LOGIC_ABSOLUTE_LONG},
        {UINT8_C(0x31), OPERATION_AND, LOGIC_DIRECT_INDIRECT_INDEXED},
        {UINT8_C(0x32), OPERATION_AND, LOGIC_DIRECT_INDIRECT},
        {UINT8_C(0x35), OPERATION_AND, LOGIC_DIRECT_X},
        {UINT8_C(0x37), OPERATION_AND,
         LOGIC_DIRECT_INDIRECT_LONG_INDEXED},
        {UINT8_C(0x39), OPERATION_AND, LOGIC_ABSOLUTE_Y},
        {UINT8_C(0x3d), OPERATION_AND, LOGIC_ABSOLUTE_X},
        {UINT8_C(0x3f), OPERATION_AND, LOGIC_ABSOLUTE_LONG_X},
        {UINT8_C(0x41), OPERATION_EOR, LOGIC_DIRECT_INDEXED_INDIRECT},
        {UINT8_C(0x47), OPERATION_EOR, LOGIC_DIRECT_INDIRECT_LONG},
        {UINT8_C(0x4f), OPERATION_EOR, LOGIC_ABSOLUTE_LONG},
        {UINT8_C(0x51), OPERATION_EOR, LOGIC_DIRECT_INDIRECT_INDEXED},
        {UINT8_C(0x52), OPERATION_EOR, LOGIC_DIRECT_INDIRECT},
        {UINT8_C(0x55), OPERATION_EOR, LOGIC_DIRECT_X},
        {UINT8_C(0x57), OPERATION_EOR,
         LOGIC_DIRECT_INDIRECT_LONG_INDEXED},
        {UINT8_C(0x59), OPERATION_EOR, LOGIC_ABSOLUTE_Y},
        {UINT8_C(0x5d), OPERATION_EOR, LOGIC_ABSOLUTE_X},
        {UINT8_C(0x5f), OPERATION_EOR, LOGIC_ABSOLUTE_LONG_X},
        {UINT8_C(0xc1), OPERATION_CMP, LOGIC_DIRECT_INDEXED_INDIRECT},
        {UINT8_C(0xc7), OPERATION_CMP, LOGIC_DIRECT_INDIRECT_LONG},
        {UINT8_C(0xcf), OPERATION_CMP, LOGIC_ABSOLUTE_LONG},
        {UINT8_C(0xd1), OPERATION_CMP, LOGIC_DIRECT_INDIRECT_INDEXED},
        {UINT8_C(0xd2), OPERATION_CMP, LOGIC_DIRECT_INDIRECT},
        {UINT8_C(0xd5), OPERATION_CMP, LOGIC_DIRECT_X},
        {UINT8_C(0xd7), OPERATION_CMP,
         LOGIC_DIRECT_INDIRECT_LONG_INDEXED},
        {UINT8_C(0xd9), OPERATION_CMP, LOGIC_ABSOLUTE_Y},
        {UINT8_C(0xdd), OPERATION_CMP, LOGIC_ABSOLUTE_X},
        {UINT8_C(0xdf), OPERATION_CMP, LOGIC_ABSOLUTE_LONG_X}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            bool is_compare = cases[index].operation == OPERATION_CMP;
            uint16_t before;
            uint16_t operand;
            uint16_t expected;
            uint8_t expected_flags;
            uint16_t expected_pc;
            uint64_t expected_cycles;

            switch (cases[index].operation) {
            case OPERATION_ORA:
                before = wide != 0U ? UINT16_C(0x4000)
                                     : UINT16_C(0xab50);
                operand = wide != 0U ? UINT16_C(0x8001)
                                      : UINT16_C(0x008f);
                expected = wide != 0U ? UINT16_C(0xc001)
                                       : UINT16_C(0xabdf);
                expected_flags = MIPSNES2_CPU_FLAG_N;
                break;
            case OPERATION_AND:
                before = wide != 0U ? UINT16_C(0xf0f0)
                                     : UINT16_C(0xabf0);
                operand = wide != 0U ? UINT16_C(0x0ff0)
                                      : UINT16_C(0x000f);
                expected = wide != 0U ? UINT16_C(0x00f0)
                                       : UINT16_C(0xab00);
                expected_flags = wide != 0U ? 0U : MIPSNES2_CPU_FLAG_Z;
                break;
            case OPERATION_EOR:
                before = wide != 0U ? UINT16_C(0xffff)
                                     : UINT16_C(0xab7f);
                operand = wide != 0U ? UINT16_C(0xffff)
                                      : UINT16_C(0x00ff);
                expected = wide != 0U ? UINT16_C(0x0000)
                                       : UINT16_C(0xab80);
                expected_flags = wide != 0U ? MIPSNES2_CPU_FLAG_Z
                                             : MIPSNES2_CPU_FLAG_N;
                break;
            case OPERATION_CMP:
                before = wide != 0U ? UINT16_C(0x1000)
                                     : UINT16_C(0xab44);
                operand = wide != 0U ? UINT16_C(0x2000)
                                      : UINT16_C(0x0044);
                expected = before;
                expected_flags = wide != 0U
                                     ? MIPSNES2_CPU_FLAG_N
                                     : MIPSNES2_CPU_FLAG_Z |
                                           MIPSNES2_CPU_FLAG_C;
                break;
            default:
                return 1;
            }

            CHECK(reset_fixture(&fixture, UINT16_C(0x9700)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = UINT16_C(0x1201);
            cpu->emulation = false;
            cpu->p = (uint8_t)(preserved |
                               MIPSNES2_CPU_FLAG_C |
                               (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_Z);
            cpu->a = before;
            cpu->x = UINT16_C(0x0020);
            cpu->y = UINT16_C(0x0020);
            bank7e[0x9700] = cases[index].opcode;

            switch (cases[index].mode) {
            case LOGIC_DIRECT_INDEXED_INDIRECT:
                bank7e[0x9701] = UINT8_C(0x30);
                bank00[0x1251] = UINT8_C(0xf0);
                bank00[0x1252] = UINT8_C(0xff);
                bank7f[0xfff0] = (uint8_t)operand;
                bank7f[0xfff1] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9702);
                expected_cycles = UINT64_C(7) + wide;
                break;
            case LOGIC_DIRECT_INDIRECT_LONG:
                bank7e[0x9701] = UINT8_C(0x34);
                bank00[0x1235] = UINT8_C(0xff);
                bank00[0x1236] = UINT8_C(0xff);
                bank00[0x1237] = UINT8_C(0x7f);
                bank7f[0xffff] = (uint8_t)operand;
                bank80[0x0000] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9702);
                expected_cycles = UINT64_C(7) + wide;
                break;
            case LOGIC_ABSOLUTE_LONG:
                bank7e[0x9701] = UINT8_C(0xff);
                bank7e[0x9702] = UINT8_C(0xff);
                bank7e[0x9703] = UINT8_C(0x7f);
                bank7f[0xffff] = (uint8_t)operand;
                bank80[0x0000] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9704);
                expected_cycles = UINT64_C(5) + wide;
                break;
            case LOGIC_DIRECT_INDIRECT_INDEXED:
                bank7e[0x9701] = UINT8_C(0x34);
                bank00[0x1235] = UINT8_C(0xf0);
                bank00[0x1236] = UINT8_C(0xff);
                bank80[0x0010] = (uint8_t)operand;
                bank80[0x0011] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9702);
                expected_cycles = UINT64_C(7) + wide;
                break;
            case LOGIC_DIRECT_INDIRECT:
                bank7e[0x9701] = UINT8_C(0x34);
                bank00[0x1235] = UINT8_C(0xff);
                bank00[0x1236] = UINT8_C(0xff);
                bank7f[0xffff] = (uint8_t)operand;
                bank80[0x0000] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9702);
                expected_cycles = UINT64_C(6) + wide;
                break;
            case LOGIC_DIRECT_X:
                bank7e[0x9701] = UINT8_C(0x30);
                bank00[0x1251] = (uint8_t)operand;
                bank00[0x1252] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9702);
                expected_cycles = UINT64_C(5) + wide;
                break;
            case LOGIC_DIRECT_INDIRECT_LONG_INDEXED:
                bank7e[0x9701] = UINT8_C(0x34);
                bank00[0x1235] = UINT8_C(0xf0);
                bank00[0x1236] = UINT8_C(0xff);
                bank00[0x1237] = UINT8_C(0x7f);
                bank80[0x0010] = (uint8_t)operand;
                bank80[0x0011] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9702);
                expected_cycles = UINT64_C(7) + wide;
                break;
            case LOGIC_ABSOLUTE_Y:
            case LOGIC_ABSOLUTE_X:
                bank7e[0x9701] = UINT8_C(0xf0);
                bank7e[0x9702] = UINT8_C(0xff);
                bank80[0x0010] = (uint8_t)operand;
                bank80[0x0011] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9703);
                expected_cycles = UINT64_C(5) + wide;
                break;
            case LOGIC_ABSOLUTE_LONG_X:
                bank7e[0x9701] = UINT8_C(0xf0);
                bank7e[0x9702] = UINT8_C(0xff);
                bank7e[0x9703] = UINT8_C(0x7f);
                bank80[0x0010] = (uint8_t)operand;
                bank80[0x0011] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9704);
                expected_cycles = UINT64_C(5) + wide;
                break;
            default:
                return 1;
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a == expected);
            CHECK(cpu->p == (uint8_t)(preserved |
                                      (wide != 0U
                                           ? 0U
                                           : MIPSNES2_CPU_FLAG_M) |
                                      (is_compare
                                           ? 0U
                                           : MIPSNES2_CPU_FLAG_C) |
                                      expected_flags));
            CHECK(cpu->pc == expected_pc);
            CHECK(cpu->instruction_cycles == expected_cycles);
            CHECK(cpu->x == UINT16_C(0x0020));
            CHECK(cpu->y == UINT16_C(0x0020));
            CHECK(cpu->d == UINT16_C(0x1201));
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->emulation);
        }
    }

    CHECK(reset_fixture(&fixture, UINT16_C(0x9720)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->d = UINT16_C(0x1200);
    cpu->x = UINT16_C(0x0020);
    cpu->a = UINT16_C(0xabf0);
    bank7e[0x9720] = UINT8_C(0x35);
    bank7e[0x9721] = UINT8_C(0xf0);
    bank00[0x1210] = UINT8_C(0x0f);
    bank00[0x1310] = UINT8_C(0xff);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xab00));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) ==
          MIPSNES2_CPU_FLAG_Z);
    CHECK(cpu->pc == UINT16_C(0x9722));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(cpu->emulation);

    CHECK(reset_fixture(&fixture, UINT16_C(0x9730)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->emulation = false;
    cpu->p = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_I;
    cpu->x = UINT16_C(0x0001);
    cpu->a = UINT16_C(0xab50);
    bank7e[0x9730] = UINT8_C(0x1d);
    bank7e[0x9731] = UINT8_C(0x00);
    bank7e[0x9732] = UINT8_C(0x20);
    bank7f[0x2001] = UINT8_C(0x0f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xab5f));
    CHECK(cpu->p == (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_I));
    CHECK(cpu->pc == UINT16_C(0x9733));
    CHECK(cpu->instruction_cycles == UINT64_C(5));

    CHECK(reset_fixture(&fixture, UINT16_C(0x9740)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->d = UINT16_C(0x1200);
    cpu->emulation = false;
    cpu->p = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_I;
    cpu->y = UINT16_C(0x0001);
    cpu->a = UINT16_C(0xabf0);
    bank7e[0x9740] = UINT8_C(0x31);
    bank7e[0x9741] = UINT8_C(0x34);
    bank00[0x1234] = UINT8_C(0x00);
    bank00[0x1235] = UINT8_C(0x20);
    bank7f[0x2001] = UINT8_C(0x0f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xab00));
    CHECK(cpu->p == (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_I |
                     MIPSNES2_CPU_FLAG_Z));
    CHECK(cpu->pc == UINT16_C(0x9742));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    return 0;
}

static int test_stack_relative_accumulator_alu(void)
{
    enum accumulator_operation {
        OPERATION_ORA,
        OPERATION_AND,
        OPERATION_EOR,
        OPERATION_CMP
    };
    static const struct {
        uint8_t opcode;
        enum accumulator_operation operation;
        bool indirect_indexed;
    } cases[] = {
        {UINT8_C(0x03), OPERATION_ORA, false},
        {UINT8_C(0x13), OPERATION_ORA, true},
        {UINT8_C(0x23), OPERATION_AND, false},
        {UINT8_C(0x33), OPERATION_AND, true},
        {UINT8_C(0x43), OPERATION_EOR, false},
        {UINT8_C(0x53), OPERATION_EOR, true},
        {UINT8_C(0xc3), OPERATION_CMP, false},
        {UINT8_C(0xd3), OPERATION_CMP, true}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            bool is_compare = cases[index].operation == OPERATION_CMP;
            uint16_t before;
            uint16_t operand;
            uint16_t expected;
            uint8_t expected_flags;

            switch (cases[index].operation) {
            case OPERATION_ORA:
                before = wide != 0U ? UINT16_C(0x4000)
                                     : UINT16_C(0xab50);
                operand = wide != 0U ? UINT16_C(0x8001)
                                      : UINT16_C(0x008f);
                expected = wide != 0U ? UINT16_C(0xc001)
                                       : UINT16_C(0xabdf);
                expected_flags = MIPSNES2_CPU_FLAG_N;
                break;
            case OPERATION_AND:
                before = wide != 0U ? UINT16_C(0xf0f0)
                                     : UINT16_C(0xabf0);
                operand = wide != 0U ? UINT16_C(0x0ff0)
                                      : UINT16_C(0x000f);
                expected = wide != 0U ? UINT16_C(0x00f0)
                                       : UINT16_C(0xab00);
                expected_flags = wide != 0U ? 0U : MIPSNES2_CPU_FLAG_Z;
                break;
            case OPERATION_EOR:
                before = wide != 0U ? UINT16_C(0xffff)
                                     : UINT16_C(0xab7f);
                operand = wide != 0U ? UINT16_C(0xffff)
                                      : UINT16_C(0x00ff);
                expected = wide != 0U ? UINT16_C(0x0000)
                                       : UINT16_C(0xab80);
                expected_flags = wide != 0U ? MIPSNES2_CPU_FLAG_Z
                                             : MIPSNES2_CPU_FLAG_N;
                break;
            case OPERATION_CMP:
                before = wide != 0U ? UINT16_C(0x1000)
                                     : UINT16_C(0xab44);
                operand = wide != 0U ? UINT16_C(0x2000)
                                      : UINT16_C(0x0044);
                expected = before;
                expected_flags = wide != 0U
                                     ? MIPSNES2_CPU_FLAG_N
                                     : MIPSNES2_CPU_FLAG_Z |
                                           MIPSNES2_CPU_FLAG_C;
                break;
            default:
                return 1;
            }

            CHECK(reset_fixture(&fixture, UINT16_C(0x9760)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->s = UINT16_C(0xfff0);
            cpu->y = UINT16_C(0x0020);
            cpu->a = before;
            cpu->emulation = false;
            cpu->p = (uint8_t)(preserved |
                               MIPSNES2_CPU_FLAG_C |
                               (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_Z);
            bank7e[0x9760] = cases[index].opcode;
            bank7e[0x9761] = UINT8_C(0x0f);

            if (cases[index].indirect_indexed) {
                bank00[0xffff] = UINT8_C(0xf0);
                bank00[0x0000] = UINT8_C(0xff);
                bank80[0x0010] = (uint8_t)operand;
                bank80[0x0011] = (uint8_t)(operand >> 8);
            } else {
                bank00[0xffff] = (uint8_t)operand;
                bank00[0x0000] = (uint8_t)(operand >> 8);
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a == expected);
            CHECK(cpu->p == (uint8_t)(preserved |
                                      (wide != 0U
                                           ? 0U
                                           : MIPSNES2_CPU_FLAG_M) |
                                      (is_compare
                                           ? 0U
                                           : MIPSNES2_CPU_FLAG_C) |
                                      expected_flags));
            CHECK(cpu->pc == UINT16_C(0x9762));
            CHECK(cpu->instruction_cycles ==
                  (cases[index].indirect_indexed ? UINT64_C(7)
                                                 : UINT64_C(4)) + wide);
            CHECK(cpu->s == UINT16_C(0xfff0));
            CHECK(cpu->y == UINT16_C(0x0020));
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->emulation);
        }
    }

    CHECK(reset_fixture(&fixture, UINT16_C(0x9770)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->s = UINT16_C(0x01f0);
    cpu->a = UINT16_C(0xab50);
    bank7e[0x9770] = UINT8_C(0x03);
    bank7e[0x9771] = UINT8_C(0x30);
    bank00[0x0120] = UINT8_C(0xff);
    bank00[0x0220] = UINT8_C(0x0f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xab5f));
    CHECK(cpu->pc == UINT16_C(0x9772));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(cpu->emulation);
    CHECK(cpu->s == UINT16_C(0x01f0));

    CHECK(reset_fixture(&fixture, UINT16_C(0x9780)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->s = UINT16_C(0x1200);
    cpu->y = UINT16_C(0x0020);
    cpu->a = UINT16_C(0xab50);
    cpu->emulation = false;
    cpu->p = MIPSNES2_CPU_FLAG_M;
    bank7e[0x9780] = UINT8_C(0x13);
    bank7e[0x9781] = UINT8_C(0x34);
    bank00[0x1234] = UINT8_C(0xf0);
    bank00[0x1235] = UINT8_C(0xff);
    bank80[0x0010] = UINT8_C(0x0f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xab5f));
    CHECK(cpu->pc == UINT16_C(0x9782));
    CHECK(cpu->instruction_cycles == UINT64_C(7));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_X) == 0U);
    return 0;
}

static int test_stack_relative_accumulator_load_store(void)
{
    static const struct {
        uint8_t opcode;
        bool load;
        bool indirect_indexed;
    } cases[] = {
        {UINT8_C(0x83), false, false},
        {UINT8_C(0x93), false, true},
        {UINT8_C(0xa3), true, false},
        {UINT8_C(0xb3), true, true}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V |
                              MIPSNES2_CPU_FLAG_C;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t value = wide != 0U ? UINT16_C(0x1234)
                                        : UINT16_C(0x0080);
            uint8_t status;

            CHECK(reset_fixture(&fixture, UINT16_C(0x9790)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->s = UINT16_C(0xfff0);
            cpu->y = UINT16_C(0x0020);
            cpu->a = cases[index].load
                         ? (wide != 0U ? UINT16_C(0xdead)
                                      : UINT16_C(0xab55))
                         : value;
            cpu->emulation = false;
            cpu->p = (uint8_t)(preserved |
                               (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_Z);
            status = cpu->p;
            bank7e[0x9790] = cases[index].opcode;
            bank7e[0x9791] = UINT8_C(0x0f);

            if (cases[index].indirect_indexed) {
                bank00[0xffff] = UINT8_C(0xf0);
                bank00[0x0000] = UINT8_C(0xff);
                bank80[0x0010] = cases[index].load
                                     ? (uint8_t)value
                                     : UINT8_C(0x00);
                bank80[0x0011] = cases[index].load
                                     ? (uint8_t)(value >> 8)
                                     : UINT8_C(0x5a);
            } else {
                bank00[0xffff] = cases[index].load
                                     ? (uint8_t)value
                                     : UINT8_C(0x00);
                bank00[0x0000] = cases[index].load
                                     ? (uint8_t)(value >> 8)
                                     : UINT8_C(0x5a);
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            if (cases[index].load) {
                CHECK(cpu->a == (wide != 0U ? value
                                             : UINT16_C(0xab80)));
                CHECK(cpu->p ==
                      (uint8_t)((status &
                                 (uint8_t)~(MIPSNES2_CPU_FLAG_N |
                                            MIPSNES2_CPU_FLAG_Z)) |
                                (wide != 0U ? 0U
                                            : MIPSNES2_CPU_FLAG_N)));
            } else {
                CHECK(cpu->a == value);
                CHECK(cpu->p == status);
                if (cases[index].indirect_indexed) {
                    CHECK(bank80[0x0010] == (uint8_t)value);
                    CHECK(bank80[0x0011] ==
                          (wide != 0U ? (uint8_t)(value >> 8)
                                      : UINT8_C(0x5a)));
                } else {
                    CHECK(bank00[0xffff] == (uint8_t)value);
                    CHECK(bank00[0x0000] ==
                          (wide != 0U ? (uint8_t)(value >> 8)
                                      : UINT8_C(0x5a)));
                }
            }
            CHECK(cpu->pc == UINT16_C(0x9792));
            CHECK(cpu->instruction_cycles ==
                  (cases[index].indirect_indexed ? UINT64_C(7)
                                                 : UINT64_C(4)) + wide);
            CHECK(cpu->s == UINT16_C(0xfff0));
            CHECK(cpu->y == UINT16_C(0x0020));
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->emulation);
        }
    }

    CHECK(reset_fixture(&fixture, UINT16_C(0x97a0)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->s = UINT16_C(0x01f0);
    cpu->a = UINT16_C(0xab5c);
    bank7e[0x97a0] = UINT8_C(0x83);
    bank7e[0x97a1] = UINT8_C(0x30);
    bank00[0x0120] = UINT8_C(0x11);
    bank00[0x0220] = UINT8_C(0x22);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x0120] == UINT8_C(0x11));
    CHECK(bank00[0x0220] == UINT8_C(0x5c));
    CHECK(cpu->a == UINT16_C(0xab5c));
    CHECK(cpu->pc == UINT16_C(0x97a2));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(cpu->emulation);
    CHECK(cpu->s == UINT16_C(0x01f0));
    return 0;
}

static int test_binary_arithmetic_foundation(void)
{
    enum arithmetic_indexed_mode {
        ARITHMETIC_DIRECT_X,
        ARITHMETIC_ABSOLUTE_X,
        ARITHMETIC_ABSOLUTE_Y
    };
    static const struct {
        uint8_t opcode;
        bool wide;
        uint16_t accumulator;
        uint16_t operand;
        bool carry_in;
        uint16_t expected;
        uint8_t expected_flags;
    } cases[] = {
        {UINT8_C(0x69), false, UINT16_C(0x007f), UINT16_C(0x0001),
         false, UINT16_C(0x0080),
         MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V},
        {UINT8_C(0x69), false, UINT16_C(0x00ff), UINT16_C(0x0000),
         true, UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0x69), false, UINT16_C(0x0080), UINT16_C(0x0080),
         false, UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_Z |
             MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0x69), true, UINT16_C(0x7fff), UINT16_C(0x0001),
         false, UINT16_C(0x8000),
         MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V},
        {UINT8_C(0x69), true, UINT16_C(0xffff), UINT16_C(0x0000),
         true, UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0x69), true, UINT16_C(0x8000), UINT16_C(0x8000),
         false, UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_Z |
             MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0xe9), false, UINT16_C(0x0080), UINT16_C(0x0001),
         true, UINT16_C(0x007f),
         MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0xe9), false, UINT16_C(0x0000), UINT16_C(0x0000),
         false, UINT16_C(0x00ff), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0xe9), false, UINT16_C(0x0000), UINT16_C(0x0000),
         true, UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0xe9), true, UINT16_C(0x8000), UINT16_C(0x0001),
         true, UINT16_C(0x7fff),
         MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0xe9), true, UINT16_C(0x0000), UINT16_C(0x0000),
         false, UINT16_C(0xffff), MIPSNES2_CPU_FLAG_N},
        {UINT8_C(0xe9), true, UINT16_C(0x0000), UINT16_C(0x0000),
         true, UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C}
    };
    static const struct {
        uint8_t opcode;
        bool subtract;
        bool direct;
        bool direct_penalty;
    } memory_cases[] = {
        {UINT8_C(0x65), false, true, false},
        {UINT8_C(0x6d), false, false, false},
        {UINT8_C(0xe5), true, true, true},
        {UINT8_C(0xed), true, false, false}
    };
    static const struct {
        uint8_t opcode;
        bool subtract;
        enum arithmetic_indexed_mode mode;
        bool direct_penalty;
        bool page_crossed;
        bool wide_index;
        bool bank_crossed;
    } indexed_cases[] = {
        {UINT8_C(0x75), false, ARITHMETIC_DIRECT_X,
         true, false, true, false},
        {UINT8_C(0x79), false, ARITHMETIC_ABSOLUTE_Y,
         false, true, false, false},
        {UINT8_C(0x7d), false, ARITHMETIC_ABSOLUTE_X,
         false, false, false, false},
        {UINT8_C(0xf5), true, ARITHMETIC_DIRECT_X,
         false, false, false, false},
        {UINT8_C(0xf9), true, ARITHMETIC_ABSOLUTE_Y,
         false, true, false, true},
        {UINT8_C(0xfd), true, ARITHMETIC_ABSOLUTE_X,
         false, false, true, false}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_X;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint8_t width_flag = cases[index].wide
                                 ? 0U
                                 : MIPSNES2_CPU_FLAG_M;

        CHECK(reset_fixture(&fixture, UINT16_C(0x97b0)) == 0);
        cpu->pbr = UINT8_C(0x7e);
        cpu->emulation = false;
        cpu->a = cases[index].wide
                     ? cases[index].accumulator
                     : (uint16_t)(UINT16_C(0xab00) |
                                  cases[index].accumulator);
        cpu->p = (uint8_t)(preserved | width_flag |
                           MIPSNES2_CPU_FLAG_N |
                           MIPSNES2_CPU_FLAG_V |
                           MIPSNES2_CPU_FLAG_Z |
                           (cases[index].carry_in
                                ? MIPSNES2_CPU_FLAG_C
                                : 0U));
        bank7e[0x97b0] = cases[index].opcode;
        bank7e[0x97b1] = (uint8_t)cases[index].operand;
        bank7e[0x97b2] = (uint8_t)(cases[index].operand >> 8);

        CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
        CHECK(cpu->a ==
              (cases[index].wide
                   ? cases[index].expected
                   : (uint16_t)(UINT16_C(0xab00) |
                                cases[index].expected)));
        CHECK(cpu->p == (uint8_t)(preserved | width_flag |
                                  cases[index].expected_flags));
        CHECK(cpu->pc == (cases[index].wide ? UINT16_C(0x97b3)
                                             : UINT16_C(0x97b2)));
        CHECK(cpu->instruction_cycles ==
              (cases[index].wide ? UINT64_C(3) : UINT64_C(2)));
        CHECK(!cpu->stopped);
        CHECK(cpu->last_opcode == cases[index].opcode);
        CHECK(cpu->last_opcode_address == UINT32_C(0x7e97b0));
        CHECK(!cpu->emulation);
    }

    for (index = 0U;
         index < sizeof(memory_cases) / sizeof(memory_cases[0]);
         ++index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t accumulator = memory_cases[index].subtract
                                       ? (wide != 0U
                                              ? UINT16_C(0x8000)
                                              : UINT16_C(0x0080))
                                       : (wide != 0U
                                              ? UINT16_C(0x7fff)
                                              : UINT16_C(0x007f));
            uint16_t expected = memory_cases[index].subtract
                                    ? (wide != 0U
                                           ? UINT16_C(0x7fff)
                                           : UINT16_C(0x007f))
                                    : (wide != 0U
                                           ? UINT16_C(0x8000)
                                           : UINT16_C(0x0080));
            uint8_t arithmetic_flags = memory_cases[index].subtract
                                           ? MIPSNES2_CPU_FLAG_V |
                                                 MIPSNES2_CPU_FLAG_C
                                           : MIPSNES2_CPU_FLAG_N |
                                                 MIPSNES2_CPU_FLAG_V;
            uint8_t width_flag = wide != 0U ? 0U
                                             : MIPSNES2_CPU_FLAG_M;

            CHECK(reset_fixture(&fixture, UINT16_C(0x97d0)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = memory_cases[index].direct_penalty
                         ? UINT16_C(0xfff0)
                         : UINT16_C(0xff00);
            cpu->emulation = false;
            cpu->a = wide != 0U
                         ? accumulator
                         : (uint16_t)(UINT16_C(0xab00) | accumulator);
            cpu->p = (uint8_t)(preserved | width_flag |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               (memory_cases[index].subtract
                                    ? MIPSNES2_CPU_FLAG_C
                                    : 0U));
            bank7e[0x97d0] = memory_cases[index].opcode;
            bank7e[0x97d1] = UINT8_C(0xff);
            bank7e[0x97d2] = UINT8_C(0xff);

            if (memory_cases[index].direct) {
                bank7e[0x97d1] = memory_cases[index].direct_penalty
                                     ? UINT8_C(0x0f)
                                     : UINT8_C(0xff);
                bank00[0xffff] = UINT8_C(0x01);
                bank00[0x0000] = UINT8_C(0x00);
            } else {
                bank7f[0xffff] = UINT8_C(0x01);
                bank80[0x0000] = UINT8_C(0x00);
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a ==
                  (wide != 0U
                       ? expected
                       : (uint16_t)(UINT16_C(0xab00) | expected)));
            CHECK(cpu->p == (uint8_t)(preserved | width_flag |
                                      arithmetic_flags));
            CHECK(cpu->pc ==
                  (memory_cases[index].direct ? UINT16_C(0x97d2)
                                              : UINT16_C(0x97d3)));
            CHECK(cpu->instruction_cycles ==
                  (memory_cases[index].direct
                       ? UINT64_C(3) +
                             (memory_cases[index].direct_penalty
                                  ? UINT64_C(1)
                                  : UINT64_C(0))
                       : UINT64_C(4)) + wide);
            CHECK(cpu->d ==
                  (memory_cases[index].direct_penalty
                       ? UINT16_C(0xfff0)
                       : UINT16_C(0xff00)));
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->stopped);
            CHECK(!cpu->emulation);
        }
    }

    for (index = 0U;
         index < sizeof(indexed_cases) / sizeof(indexed_cases[0]);
         ++index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t accumulator = indexed_cases[index].subtract
                                       ? (wide != 0U
                                              ? UINT16_C(0x8000)
                                              : UINT16_C(0x0080))
                                       : (wide != 0U
                                              ? UINT16_C(0x7fff)
                                              : UINT16_C(0x007f));
            uint16_t expected = indexed_cases[index].subtract
                                    ? (wide != 0U
                                           ? UINT16_C(0x7fff)
                                           : UINT16_C(0x007f))
                                    : (wide != 0U
                                           ? UINT16_C(0x8000)
                                           : UINT16_C(0x0080));
            uint8_t arithmetic_flags = indexed_cases[index].subtract
                                           ? MIPSNES2_CPU_FLAG_V |
                                                 MIPSNES2_CPU_FLAG_C
                                           : MIPSNES2_CPU_FLAG_N |
                                                 MIPSNES2_CPU_FLAG_V;
            uint8_t width_flags = (uint8_t)(
                (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                (indexed_cases[index].wide_index
                     ? 0U
                     : MIPSNES2_CPU_FLAG_X));
            uint16_t base;
            uint16_t index_value;
            uint64_t expected_cycles;
            uint8_t *target_bank;
            uint16_t target_offset;

            CHECK(reset_fixture(&fixture, UINT16_C(0x97e0)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->emulation = false;
            cpu->a = wide != 0U
                         ? accumulator
                         : (uint16_t)(UINT16_C(0xab00) | accumulator);
            cpu->p = (uint8_t)(MIPSNES2_CPU_FLAG_I | width_flags |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               (indexed_cases[index].subtract
                                    ? MIPSNES2_CPU_FLAG_C
                                    : 0U));
            bank7e[0x97e0] = indexed_cases[index].opcode;

            if (indexed_cases[index].mode == ARITHMETIC_DIRECT_X) {
                cpu->d = indexed_cases[index].direct_penalty
                             ? UINT16_C(0xfff0)
                             : UINT16_C(0xff00);
                cpu->x = UINT16_C(0x0001);
                bank7e[0x97e1] = indexed_cases[index].direct_penalty
                                     ? UINT8_C(0x0f)
                                     : UINT8_C(0xff);
                target_bank = bank00;
                target_offset = UINT16_C(0x0000);
                expected_cycles = UINT64_C(4) +
                    (indexed_cases[index].direct_penalty
                         ? UINT64_C(1)
                         : UINT64_C(0));
            } else {
                index_value = indexed_cases[index].page_crossed
                                  ? UINT16_C(0x0020)
                                  : UINT16_C(0x0001);
                base = indexed_cases[index].bank_crossed
                           ? UINT16_C(0xfff0)
                           : (indexed_cases[index].page_crossed
                                  ? UINT16_C(0x10f0)
                                  : UINT16_C(0x1000));
                if (indexed_cases[index].mode == ARITHMETIC_ABSOLUTE_X) {
                    cpu->x = index_value;
                } else {
                    cpu->y = index_value;
                }
                bank7e[0x97e1] = (uint8_t)base;
                bank7e[0x97e2] = (uint8_t)(base >> 8);
                target_bank = indexed_cases[index].bank_crossed
                                  ? bank80
                                  : bank7f;
                target_offset = (uint16_t)(base + index_value);
                expected_cycles = UINT64_C(4) +
                    ((indexed_cases[index].page_crossed ||
                      indexed_cases[index].wide_index)
                         ? UINT64_C(1)
                         : UINT64_C(0));
            }

            target_bank[target_offset] = UINT8_C(0x01);
            target_bank[(uint16_t)(target_offset + UINT16_C(1))] =
                UINT8_C(0x00);

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a ==
                  (wide != 0U
                       ? expected
                       : (uint16_t)(UINT16_C(0xab00) | expected)));
            CHECK(cpu->p == (uint8_t)(MIPSNES2_CPU_FLAG_I |
                                      width_flags |
                                      arithmetic_flags));
            CHECK(cpu->pc ==
                  (indexed_cases[index].mode == ARITHMETIC_DIRECT_X
                       ? UINT16_C(0x97e2)
                       : UINT16_C(0x97e3)));
            CHECK(cpu->instruction_cycles == expected_cycles + wide);
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->stopped);
            CHECK(!cpu->emulation);
        }
    }

    return 0;
}

static uint16_t packed_bcd(unsigned value)
{
    return (uint16_t)((((value / 1000U) % 10U) << 12) |
                      (((value / 100U) % 10U) << 8) |
                      (((value / 10U) % 10U) << 4) |
                      (value % 10U));
}

static int test_decimal_arithmetic(void)
{
    enum decimal_addressing_mode {
        DECIMAL_DIRECT_INDEXED_INDIRECT,
        DECIMAL_STACK_RELATIVE,
        DECIMAL_DIRECT,
        DECIMAL_DIRECT_INDIRECT_LONG,
        DECIMAL_IMMEDIATE,
        DECIMAL_ABSOLUTE,
        DECIMAL_ABSOLUTE_LONG,
        DECIMAL_DIRECT_INDIRECT_INDEXED,
        DECIMAL_DIRECT_INDIRECT,
        DECIMAL_STACK_RELATIVE_INDIRECT_INDEXED,
        DECIMAL_DIRECT_X,
        DECIMAL_DIRECT_INDIRECT_LONG_INDEXED,
        DECIMAL_ABSOLUTE_Y,
        DECIMAL_ABSOLUTE_X,
        DECIMAL_ABSOLUTE_LONG_X
    };
    static const struct {
        bool wide;
        bool subtract;
        uint16_t accumulator;
        uint16_t operand;
        bool carry_in;
        uint16_t expected;
        uint8_t expected_flags;
    } vectors[] = {
        {false, false, UINT16_C(0x0009), UINT16_C(0x0001), false,
         UINT16_C(0x0010), 0U},
        {false, false, UINT16_C(0x0099), UINT16_C(0x0001), false,
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C},
        {false, false, UINT16_C(0x0050), UINT16_C(0x0050), false,
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               MIPSNES2_CPU_FLAG_C},
        {false, false, UINT16_C(0x0049), UINT16_C(0x0050), true,
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               MIPSNES2_CPU_FLAG_C},
        {false, false, UINT16_C(0x00da), UINT16_C(0x00a1), false,
         UINT16_C(0x00e1), MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_C},
        {true, false, UINT16_C(0x0009), UINT16_C(0x0001), false,
         UINT16_C(0x0010), 0U},
        {true, false, UINT16_C(0x9999), UINT16_C(0x0001), false,
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C},
        {true, false, UINT16_C(0x4999), UINT16_C(0x5001), false,
         UINT16_C(0x0000), MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               MIPSNES2_CPU_FLAG_C},
        {true, false, UINT16_C(0x069e), UINT16_C(0x7921), true,
         UINT16_C(0x8626), MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V},
        {false, true, UINT16_C(0x0010), UINT16_C(0x0001), true,
         UINT16_C(0x0009), MIPSNES2_CPU_FLAG_C},
        {false, true, UINT16_C(0x0000), UINT16_C(0x0001), true,
         UINT16_C(0x0099), MIPSNES2_CPU_FLAG_N},
        {false, true, UINT16_C(0x0000), UINT16_C(0x0000), false,
         UINT16_C(0x0099), MIPSNES2_CPU_FLAG_N},
        {false, true, UINT16_C(0x0080), UINT16_C(0x0001), true,
         UINT16_C(0x0079), MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_C},
        {true, true, UINT16_C(0x1000), UINT16_C(0x0001), true,
         UINT16_C(0x0999), MIPSNES2_CPU_FLAG_C},
        {true, true, UINT16_C(0x0000), UINT16_C(0x0001), true,
         UINT16_C(0x9999), MIPSNES2_CPU_FLAG_N},
        {true, true, UINT16_C(0x8000), UINT16_C(0x0001), true,
         UINT16_C(0x7999), MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_C},
        {true, true, UINT16_C(0x0000), UINT16_C(0x0000), false,
         UINT16_C(0x9999), MIPSNES2_CPU_FLAG_N}
    };
    static const struct {
        uint8_t opcode;
        bool subtract;
        enum decimal_addressing_mode mode;
        uint64_t base_cycles;
    } opcodes[] = {
        {UINT8_C(0x61), false, DECIMAL_DIRECT_INDEXED_INDIRECT, UINT64_C(6)},
        {UINT8_C(0x63), false, DECIMAL_STACK_RELATIVE, UINT64_C(4)},
        {UINT8_C(0x65), false, DECIMAL_DIRECT, UINT64_C(3)},
        {UINT8_C(0x67), false, DECIMAL_DIRECT_INDIRECT_LONG, UINT64_C(6)},
        {UINT8_C(0x69), false, DECIMAL_IMMEDIATE, UINT64_C(2)},
        {UINT8_C(0x6d), false, DECIMAL_ABSOLUTE, UINT64_C(4)},
        {UINT8_C(0x6f), false, DECIMAL_ABSOLUTE_LONG, UINT64_C(5)},
        {UINT8_C(0x71), false, DECIMAL_DIRECT_INDIRECT_INDEXED, UINT64_C(5)},
        {UINT8_C(0x72), false, DECIMAL_DIRECT_INDIRECT, UINT64_C(5)},
        {UINT8_C(0x73), false, DECIMAL_STACK_RELATIVE_INDIRECT_INDEXED,
         UINT64_C(7)},
        {UINT8_C(0x75), false, DECIMAL_DIRECT_X, UINT64_C(4)},
        {UINT8_C(0x77), false, DECIMAL_DIRECT_INDIRECT_LONG_INDEXED,
         UINT64_C(6)},
        {UINT8_C(0x79), false, DECIMAL_ABSOLUTE_Y, UINT64_C(4)},
        {UINT8_C(0x7d), false, DECIMAL_ABSOLUTE_X, UINT64_C(4)},
        {UINT8_C(0x7f), false, DECIMAL_ABSOLUTE_LONG_X, UINT64_C(5)},
        {UINT8_C(0xe1), true, DECIMAL_DIRECT_INDEXED_INDIRECT, UINT64_C(6)},
        {UINT8_C(0xe3), true, DECIMAL_STACK_RELATIVE, UINT64_C(4)},
        {UINT8_C(0xe5), true, DECIMAL_DIRECT, UINT64_C(3)},
        {UINT8_C(0xe7), true, DECIMAL_DIRECT_INDIRECT_LONG, UINT64_C(6)},
        {UINT8_C(0xe9), true, DECIMAL_IMMEDIATE, UINT64_C(2)},
        {UINT8_C(0xed), true, DECIMAL_ABSOLUTE, UINT64_C(4)},
        {UINT8_C(0xef), true, DECIMAL_ABSOLUTE_LONG, UINT64_C(5)},
        {UINT8_C(0xf1), true, DECIMAL_DIRECT_INDIRECT_INDEXED, UINT64_C(5)},
        {UINT8_C(0xf2), true, DECIMAL_DIRECT_INDIRECT, UINT64_C(5)},
        {UINT8_C(0xf3), true, DECIMAL_STACK_RELATIVE_INDIRECT_INDEXED,
         UINT64_C(7)},
        {UINT8_C(0xf5), true, DECIMAL_DIRECT_X, UINT64_C(4)},
        {UINT8_C(0xf7), true, DECIMAL_DIRECT_INDIRECT_LONG_INDEXED,
         UINT64_C(6)},
        {UINT8_C(0xf9), true, DECIMAL_ABSOLUTE_Y, UINT64_C(4)},
        {UINT8_C(0xfd), true, DECIMAL_ABSOLUTE_X, UINT64_C(4)},
        {UINT8_C(0xff), true, DECIMAL_ABSOLUTE_LONG_X, UINT64_C(5)}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_D;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;

    for (index = 0U; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        uint8_t width_flag = vectors[index].wide ? 0U
                                                 : MIPSNES2_CPU_FLAG_M;

        CHECK(reset_fixture(&fixture, UINT16_C(0x97c0)) == 0);
        cpu->pbr = UINT8_C(0x7e);
        cpu->emulation = false;
        cpu->a = vectors[index].wide
                     ? vectors[index].accumulator
                     : (uint16_t)(UINT16_C(0xab00) |
                                  vectors[index].accumulator);
        cpu->p = (uint8_t)(preserved | width_flag |
                           MIPSNES2_CPU_FLAG_N |
                           MIPSNES2_CPU_FLAG_V |
                           MIPSNES2_CPU_FLAG_Z |
                           (vectors[index].carry_in
                                ? MIPSNES2_CPU_FLAG_C
                                : 0U));
        bank7e[0x97c0] = vectors[index].subtract ? UINT8_C(0xe9)
                                                 : UINT8_C(0x69);
        bank7e[0x97c1] = (uint8_t)vectors[index].operand;
        bank7e[0x97c2] = (uint8_t)(vectors[index].operand >> 8);

        CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
        CHECK(cpu->a ==
              (vectors[index].wide
                   ? vectors[index].expected
                   : (uint16_t)(UINT16_C(0xab00) |
                                vectors[index].expected)));
        CHECK(cpu->p == (uint8_t)(preserved | width_flag |
                                  vectors[index].expected_flags));
        CHECK(cpu->pc == (vectors[index].wide ? UINT16_C(0x97c3)
                                               : UINT16_C(0x97c2)));
        CHECK(cpu->instruction_cycles ==
              (vectors[index].wide ? UINT64_C(3) : UINT64_C(2)));
        CHECK(!cpu->stopped);
    }

    CHECK(reset_fixture(&fixture, UINT16_C(0x97c0)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->emulation = false;
    for (index = 0U; index < 2U; ++index) {
        bool subtract = index != 0U;
        unsigned left;

        for (left = 0U; left < 100U; ++left) {
            unsigned right;

            for (right = 0U; right < 100U; ++right) {
                unsigned carry_in;

                for (carry_in = 0U; carry_in < 2U; ++carry_in) {
                    uint8_t left_bcd = (uint8_t)packed_bcd(left);
                    uint8_t right_bcd = (uint8_t)packed_bcd(right);
                    int expected_value;
                    bool expected_carry;
                    uint8_t expected_bcd;
                    uint8_t binary_result;
                    bool expected_overflow;
                    uint8_t expected_flags = (uint8_t)(preserved |
                        MIPSNES2_CPU_FLAG_M);

                    if (subtract) {
                        expected_value = (int)left - (int)right -
                                         (carry_in != 0U ? 0 : 1);
                        expected_carry = expected_value >= 0;
                        if (expected_value < 0) {
                            expected_value += 100;
                        }
                        binary_result = (uint8_t)(left_bcd - right_bcd -
                            (carry_in != 0U ? 0U : 1U));
                        expected_overflow =
                            ((left_bcd ^ right_bcd) &
                             (left_bcd ^ binary_result) & UINT8_C(0x80)) != 0U;
                    } else {
                        unsigned low_sum;
                        unsigned high_sum;

                        expected_value = (int)left + (int)right +
                                         (int)carry_in;
                        expected_carry = expected_value >= 100;
                        if (expected_carry) {
                            expected_value -= 100;
                        }
                        binary_result = (uint8_t)(left_bcd + right_bcd +
                                                  carry_in);
                        low_sum = (left_bcd & UINT8_C(0x0f)) +
                                  (right_bcd & UINT8_C(0x0f)) + carry_in;
                        high_sum = (left_bcd & UINT8_C(0xf0)) +
                                   (right_bcd & UINT8_C(0xf0)) +
                                   (low_sum > 9U ? 0x10U : 0U);
                        expected_overflow =
                            ((uint8_t)~(left_bcd ^ right_bcd) &
                             (left_bcd ^ high_sum) & UINT8_C(0x80)) != 0U;
                    }
                    expected_bcd = (uint8_t)packed_bcd((unsigned)expected_value);
                    if ((expected_bcd & UINT8_C(0x80)) != 0U) {
                        expected_flags |= MIPSNES2_CPU_FLAG_N;
                    }
                    if (expected_bcd == 0U) {
                        expected_flags |= MIPSNES2_CPU_FLAG_Z;
                    }
                    if (expected_overflow) {
                        expected_flags |= MIPSNES2_CPU_FLAG_V;
                    }
                    if (expected_carry) {
                        expected_flags |= MIPSNES2_CPU_FLAG_C;
                    }

                    cpu->pc = UINT16_C(0x97c0);
                    cpu->a = (uint16_t)(UINT16_C(0xab00) | left_bcd);
                    cpu->p = (uint8_t)(preserved | MIPSNES2_CPU_FLAG_M |
                                       (carry_in != 0U
                                            ? MIPSNES2_CPU_FLAG_C
                                            : 0U));
                    cpu->instruction_cycles = 0U;
                    bank7e[0x97c0] = subtract ? UINT8_C(0xe9)
                                              : UINT8_C(0x69);
                    bank7e[0x97c1] = right_bcd;

                    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
                    CHECK(cpu->a == (uint16_t)(UINT16_C(0xab00) |
                                               expected_bcd));
                    CHECK(cpu->p == expected_flags);
                    CHECK(cpu->pc == UINT16_C(0x97c2));
                    CHECK(cpu->instruction_cycles == UINT64_C(2));
                }
            }
        }
    }

    for (index = 0U; index < sizeof(opcodes) / sizeof(opcodes[0]); ++index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            uint8_t width_flag = wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M;
            uint8_t expected_flags = (uint8_t)(preserved | width_flag |
                (opcodes[index].subtract ? MIPSNES2_CPU_FLAG_C : 0U));
            uint16_t expected = opcodes[index].subtract
                                    ? UINT16_C(0x0009)
                                    : UINT16_C(0x0010);
            uint16_t expected_pc = 0U;
            uint8_t *target = NULL;

            CHECK(reset_fixture(&fixture, UINT16_C(0x97d0)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = UINT16_C(0x1200);
            cpu->s = UINT16_C(0x0100);
            cpu->x = UINT16_C(0x0020);
            cpu->y = UINT16_C(0x0020);
            cpu->emulation = false;
            cpu->a = wide != 0U
                         ? (opcodes[index].subtract
                                ? UINT16_C(0x0010)
                                : UINT16_C(0x0009))
                         : (uint16_t)(UINT16_C(0xab00) |
                                      (opcodes[index].subtract
                                           ? UINT16_C(0x0010)
                                           : UINT16_C(0x0009)));
            cpu->p = (uint8_t)(preserved | width_flag |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               (opcodes[index].subtract
                                    ? MIPSNES2_CPU_FLAG_C
                                    : 0U));
            bank7e[0x97d0] = opcodes[index].opcode;

            switch (opcodes[index].mode) {
            case DECIMAL_DIRECT_INDEXED_INDIRECT:
                bank7e[0x97d1] = UINT8_C(0x10);
                bank00[0x1230] = UINT8_C(0x00);
                bank00[0x1231] = UINT8_C(0x20);
                target = &bank7f[0x2000];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_STACK_RELATIVE:
                bank7e[0x97d1] = UINT8_C(0x10);
                target = &bank00[0x0110];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_DIRECT:
                bank7e[0x97d1] = UINT8_C(0x10);
                target = &bank00[0x1210];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_DIRECT_INDIRECT_LONG:
                bank7e[0x97d1] = UINT8_C(0x10);
                bank00[0x1210] = UINT8_C(0x00);
                bank00[0x1211] = UINT8_C(0x20);
                bank00[0x1212] = UINT8_C(0x7f);
                target = &bank7f[0x2000];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_IMMEDIATE:
                bank7e[0x97d1] = UINT8_C(0x01);
                bank7e[0x97d2] = 0U;
                expected_pc = wide != 0U ? UINT16_C(0x97d3)
                                         : UINT16_C(0x97d2);
                break;
            case DECIMAL_ABSOLUTE:
                bank7e[0x97d1] = UINT8_C(0x00);
                bank7e[0x97d2] = UINT8_C(0x20);
                target = &bank7f[0x2000];
                expected_pc = UINT16_C(0x97d3);
                break;
            case DECIMAL_ABSOLUTE_LONG:
                bank7e[0x97d1] = UINT8_C(0x00);
                bank7e[0x97d2] = UINT8_C(0x20);
                bank7e[0x97d3] = UINT8_C(0x7f);
                target = &bank7f[0x2000];
                expected_pc = UINT16_C(0x97d4);
                break;
            case DECIMAL_DIRECT_INDIRECT_INDEXED:
                bank7e[0x97d1] = UINT8_C(0x10);
                bank00[0x1210] = UINT8_C(0x00);
                bank00[0x1211] = UINT8_C(0x20);
                target = &bank7f[0x2020];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_DIRECT_INDIRECT:
                bank7e[0x97d1] = UINT8_C(0x10);
                bank00[0x1210] = UINT8_C(0x00);
                bank00[0x1211] = UINT8_C(0x20);
                target = &bank7f[0x2000];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_STACK_RELATIVE_INDIRECT_INDEXED:
                bank7e[0x97d1] = UINT8_C(0x10);
                bank00[0x0110] = UINT8_C(0x00);
                bank00[0x0111] = UINT8_C(0x20);
                target = &bank7f[0x2020];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_DIRECT_X:
                bank7e[0x97d1] = UINT8_C(0x10);
                target = &bank00[0x1230];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_DIRECT_INDIRECT_LONG_INDEXED:
                bank7e[0x97d1] = UINT8_C(0x10);
                bank00[0x1210] = UINT8_C(0x00);
                bank00[0x1211] = UINT8_C(0x20);
                bank00[0x1212] = UINT8_C(0x7f);
                target = &bank7f[0x2020];
                expected_pc = UINT16_C(0x97d2);
                break;
            case DECIMAL_ABSOLUTE_Y:
            case DECIMAL_ABSOLUTE_X:
                bank7e[0x97d1] = UINT8_C(0x00);
                bank7e[0x97d2] = UINT8_C(0x20);
                target = &bank7f[0x2020];
                expected_pc = UINT16_C(0x97d3);
                break;
            case DECIMAL_ABSOLUTE_LONG_X:
                bank7e[0x97d1] = UINT8_C(0x00);
                bank7e[0x97d2] = UINT8_C(0x20);
                bank7e[0x97d3] = UINT8_C(0x7f);
                target = &bank7f[0x2020];
                expected_pc = UINT16_C(0x97d4);
                break;
            }
            if (target != NULL) {
                target[0] = UINT8_C(0x01);
                target[1] = 0U;
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a == (wide != 0U
                                 ? expected
                                 : (uint16_t)(UINT16_C(0xab00) | expected)));
            CHECK(cpu->p == expected_flags);
            CHECK(cpu->pc == expected_pc);
            CHECK(cpu->instruction_cycles ==
                  opcodes[index].base_cycles + wide);
            CHECK(cpu->last_opcode == opcodes[index].opcode);
            CHECK(cpu->last_opcode_address == UINT32_C(0x7e97d0));
            CHECK(!cpu->stopped);
        }
    }
    return 0;
}

typedef enum {
    RMW_DIRECT,
    RMW_DIRECT_X,
    RMW_ABSOLUTE,
    RMW_ABSOLUTE_X
} rmw_test_addressing;

static int test_shift_rotate_family(void)
{
    static const struct {
        uint8_t accumulator_opcode;
        uint8_t direct_opcode;
        uint8_t direct_x_opcode;
        uint8_t absolute_opcode;
        uint8_t absolute_x_opcode;
        bool carry_in;
        uint16_t input8;
        uint16_t input16;
        uint16_t expected8;
        uint16_t expected16;
        uint8_t expected_flags;
    } operations[] = {
        {UINT8_C(0x0a), UINT8_C(0x06), UINT8_C(0x16), UINT8_C(0x0e),
         UINT8_C(0x1e), false, UINT16_C(0x0081), UINT16_C(0x8001),
         UINT16_C(0x0002), UINT16_C(0x0002), MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0x2a), UINT8_C(0x26), UINT8_C(0x36), UINT8_C(0x2e),
         UINT8_C(0x3e), true, UINT16_C(0x0080), UINT16_C(0x8000),
         UINT16_C(0x0001), UINT16_C(0x0001), MIPSNES2_CPU_FLAG_C},
        {UINT8_C(0x4a), UINT8_C(0x46), UINT8_C(0x56), UINT8_C(0x4e),
         UINT8_C(0x5e), false, UINT16_C(0x0001), UINT16_C(0x0001),
         UINT16_C(0x0000), UINT16_C(0x0000),
         MIPSNES2_CPU_FLAG_C | MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x6a), UINT8_C(0x66), UINT8_C(0x76), UINT8_C(0x6e),
         UINT8_C(0x7e), true, UINT16_C(0x0001), UINT16_C(0x0001),
         UINT16_C(0x0080), UINT16_C(0x8000),
         MIPSNES2_CPU_FLAG_C | MIPSNES2_CPU_FLAG_N}
    };
    static const struct {
        rmw_test_addressing mode;
        uint64_t base_cycles;
        uint16_t pc_advance;
    } modes[] = {
        {RMW_DIRECT, UINT64_C(5), UINT16_C(2)},
        {RMW_DIRECT_X, UINT64_C(6), UINT16_C(2)},
        {RMW_ABSOLUTE, UINT64_C(6), UINT16_C(3)},
        {RMW_ABSOLUTE_X, UINT64_C(7), UINT16_C(3)}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t operation;

    for (operation = 0U;
         operation < sizeof(operations) / sizeof(operations[0]);
         ++operation) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t input = wide != 0U ? operations[operation].input16
                                        : operations[operation].input8;
            uint16_t expected = wide != 0U
                                    ? operations[operation].expected16
                                    : operations[operation].expected8;
            uint8_t width_flag = wide != 0U ? 0U
                                             : MIPSNES2_CPU_FLAG_M;

            CHECK(reset_fixture(&fixture, UINT16_C(0x9830)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->emulation = false;
            cpu->a = wide != 0U
                         ? input
                         : (uint16_t)(UINT16_C(0xab00) | input);
            cpu->p = (uint8_t)(preserved | width_flag |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_Z |
                               (operations[operation].carry_in
                                    ? MIPSNES2_CPU_FLAG_C
                                    : 0U));
            bank7e[0x9830] = operations[operation].accumulator_opcode;

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a == (wide != 0U
                                 ? expected
                                 : (uint16_t)(UINT16_C(0xab00) | expected)));
            CHECK(cpu->p == (uint8_t)(preserved | width_flag |
                                      operations[operation].expected_flags));
            CHECK(cpu->pc == UINT16_C(0x9831));
            CHECK(cpu->instruction_cycles == UINT64_C(2));
            CHECK(!cpu->stopped);
        }
    }

    for (operation = 0U;
         operation < sizeof(operations) / sizeof(operations[0]);
         ++operation) {
        size_t mode;

        for (mode = 0U; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
            unsigned wide;

            for (wide = 0U; wide < 2U; ++wide) {
                uint8_t opcode;
                uint8_t width_flag = wide != 0U ? 0U
                                                 : MIPSNES2_CPU_FLAG_M;
                uint16_t input = wide != 0U
                                     ? operations[operation].input16
                                     : operations[operation].input8;
                uint16_t expected = wide != 0U
                                        ? operations[operation].expected16
                                        : operations[operation].expected8;
                uint8_t *low = NULL;
                uint8_t *high = NULL;
                uint64_t expected_cycles = modes[mode].base_cycles +
                    (wide != 0U ? UINT64_C(2) : UINT64_C(0));

                CHECK(reset_fixture(&fixture, UINT16_C(0x9840)) == 0);
                cpu->pbr = UINT8_C(0x7e);
                cpu->dbr = UINT8_C(0x7f);
                cpu->d = UINT16_C(0x1201);
                cpu->x = UINT16_C(0x0020);
                cpu->emulation = false;
                cpu->a = wide != 0U ? UINT16_C(0x0f0f)
                                     : UINT16_C(0xab0f);
                cpu->p = (uint8_t)(preserved | width_flag |
                                   MIPSNES2_CPU_FLAG_N |
                                   MIPSNES2_CPU_FLAG_Z |
                                   (operations[operation].carry_in
                                        ? MIPSNES2_CPU_FLAG_C
                                        : 0U));

                switch (modes[mode].mode) {
                case RMW_DIRECT:
                    opcode = operations[operation].direct_opcode;
                    bank7e[0x9841] = UINT8_C(0x10);
                    low = &bank00[0x1211];
                    high = &bank00[0x1212];
                    expected_cycles += UINT64_C(1);
                    break;
                case RMW_DIRECT_X:
                    opcode = operations[operation].direct_x_opcode;
                    bank7e[0x9841] = UINT8_C(0x10);
                    low = &bank00[0x1231];
                    high = &bank00[0x1232];
                    expected_cycles += UINT64_C(1);
                    break;
                case RMW_ABSOLUTE:
                    opcode = operations[operation].absolute_opcode;
                    bank7e[0x9841] = UINT8_C(0xff);
                    bank7e[0x9842] = UINT8_C(0xff);
                    low = &bank7f[0xffff];
                    high = &bank80[0x0000];
                    break;
                case RMW_ABSOLUTE_X:
                    opcode = operations[operation].absolute_x_opcode;
                    bank7e[0x9841] = UINT8_C(0xf0);
                    bank7e[0x9842] = UINT8_C(0xff);
                    low = &bank80[0x0010];
                    high = &bank80[0x0011];
                    break;
                default:
                    return 1;
                }
                bank7e[0x9840] = opcode;
                *low = (uint8_t)input;
                *high = wide != 0U ? (uint8_t)(input >> 8)
                                   : UINT8_C(0x5a);

                CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
                CHECK(*low == (uint8_t)expected);
                CHECK(*high == (wide != 0U
                                    ? (uint8_t)(expected >> 8)
                                    : UINT8_C(0x5a)));
                CHECK(cpu->a == (wide != 0U ? UINT16_C(0x0f0f)
                                             : UINT16_C(0xab0f)));
                CHECK(cpu->p ==
                      (uint8_t)(preserved | width_flag |
                                operations[operation].expected_flags));
                CHECK(cpu->pc ==
                      (uint16_t)(UINT16_C(0x9840) +
                                 modes[mode].pc_advance));
                CHECK(cpu->instruction_cycles == expected_cycles);
                CHECK(!cpu->stopped);
            }
        }
    }

    CHECK(reset_fixture(&fixture, UINT16_C(0x9848)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->dbr = UINT8_C(0x7f);
    cpu->x = UINT16_C(0x0120);
    cpu->a = UINT16_C(0xab34);
    cpu->emulation = false;
    cpu->p = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_I |
             MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_V |
             MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C;
    bank7e[0x9848] = UINT8_C(0x1e);
    bank7e[0x9849] = UINT8_C(0xf0);
    bank7e[0x984a] = UINT8_C(0xff);
    bank80[0x0110] = UINT8_C(0x40);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank80[0x0110] == UINT8_C(0x80));
    CHECK(cpu->p == (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_I |
                     MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_V |
                     MIPSNES2_CPU_FLAG_N));
    CHECK(cpu->pc == UINT16_C(0x984b));
    CHECK(cpu->instruction_cycles == UINT64_C(7));

    return 0;
}

static int test_memory_increment_decrement(void)
{
    static const struct {
        uint8_t opcode;
        rmw_test_addressing mode;
        bool increment;
        uint64_t base_cycles;
        uint16_t pc_advance;
    } cases[] = {
        {UINT8_C(0xc6), RMW_DIRECT, false, UINT64_C(5), UINT16_C(2)},
        {UINT8_C(0xd6), RMW_DIRECT_X, false, UINT64_C(6), UINT16_C(2)},
        {UINT8_C(0xce), RMW_ABSOLUTE, false, UINT64_C(6), UINT16_C(3)},
        {UINT8_C(0xde), RMW_ABSOLUTE_X, false, UINT64_C(7), UINT16_C(3)},
        {UINT8_C(0xe6), RMW_DIRECT, true, UINT64_C(5), UINT16_C(2)},
        {UINT8_C(0xf6), RMW_DIRECT_X, true, UINT64_C(6), UINT16_C(2)},
        {UINT8_C(0xee), RMW_ABSOLUTE, true, UINT64_C(6), UINT16_C(3)},
        {UINT8_C(0xfe), RMW_ABSOLUTE_X, true, UINT64_C(7), UINT16_C(3)}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_V |
                              MIPSNES2_CPU_FLAG_C;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            uint8_t width_flag = wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M;
            uint16_t input = cases[index].increment
                                 ? (wide != 0U ? UINT16_MAX : UINT8_MAX)
                                 : 0U;
            uint16_t expected = cases[index].increment
                                    ? 0U
                                    : (wide != 0U ? UINT16_MAX : UINT8_MAX);
            uint8_t expected_nz = cases[index].increment
                                      ? MIPSNES2_CPU_FLAG_Z
                                      : MIPSNES2_CPU_FLAG_N;
            uint8_t *low = NULL;
            uint8_t *high = NULL;
            uint64_t expected_cycles = cases[index].base_cycles +
                (wide != 0U ? UINT64_C(2) : UINT64_C(0));

            CHECK(reset_fixture(&fixture, UINT16_C(0x9850)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = UINT16_C(0x1201);
            cpu->x = UINT16_C(0x0020);
            cpu->a = wide != 0U ? UINT16_C(0x1234)
                                 : UINT16_C(0xab34);
            cpu->emulation = false;
            cpu->p = (uint8_t)(preserved | width_flag |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_Z);
            bank7e[0x9850] = cases[index].opcode;

            switch (cases[index].mode) {
            case RMW_DIRECT:
                bank7e[0x9851] = UINT8_C(0x10);
                low = &bank00[0x1211];
                high = &bank00[0x1212];
                expected_cycles += UINT64_C(1);
                break;
            case RMW_DIRECT_X:
                bank7e[0x9851] = UINT8_C(0x10);
                low = &bank00[0x1231];
                high = &bank00[0x1232];
                expected_cycles += UINT64_C(1);
                break;
            case RMW_ABSOLUTE:
                bank7e[0x9851] = UINT8_C(0xff);
                bank7e[0x9852] = UINT8_C(0xff);
                low = &bank7f[0xffff];
                high = &bank80[0x0000];
                break;
            case RMW_ABSOLUTE_X:
                bank7e[0x9851] = UINT8_C(0xf0);
                bank7e[0x9852] = UINT8_C(0xff);
                low = &bank80[0x0010];
                high = &bank80[0x0011];
                break;
            default:
                return 1;
            }
            *low = (uint8_t)input;
            *high = wide != 0U ? (uint8_t)(input >> 8)
                               : UINT8_C(0x5a);

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(*low == (uint8_t)expected);
            CHECK(*high == (wide != 0U
                                ? (uint8_t)(expected >> 8)
                                : UINT8_C(0x5a)));
            CHECK(cpu->a == (wide != 0U ? UINT16_C(0x1234)
                                         : UINT16_C(0xab34)));
            CHECK(cpu->p == (uint8_t)(preserved | width_flag |
                                      expected_nz));
            CHECK(cpu->pc ==
                  (uint16_t)(UINT16_C(0x9850) + cases[index].pc_advance));
            CHECK(cpu->instruction_cycles == expected_cycles);
            CHECK(!cpu->stopped);
        }
    }
    return 0;
}

static int test_memory_bit_and_store_zero(void)
{
    static const struct {
        uint8_t opcode;
        bool reset_bits;
        bool direct;
        uint64_t base_cycles;
        uint16_t pc_advance;
    } bit_cases[] = {
        {UINT8_C(0x04), false, true, UINT64_C(5), UINT16_C(2)},
        {UINT8_C(0x0c), false, false, UINT64_C(6), UINT16_C(3)},
        {UINT8_C(0x14), true, true, UINT64_C(5), UINT16_C(2)},
        {UINT8_C(0x1c), true, false, UINT64_C(6), UINT16_C(3)}
    };
    static const struct {
        uint8_t opcode;
        rmw_test_addressing mode;
        uint64_t base_cycles;
        uint16_t pc_advance;
    } zero_cases[] = {
        {UINT8_C(0x64), RMW_DIRECT, UINT64_C(3), UINT16_C(2)},
        {UINT8_C(0x74), RMW_DIRECT_X, UINT64_C(4), UINT16_C(2)},
        {UINT8_C(0x9c), RMW_ABSOLUTE, UINT64_C(4), UINT16_C(3)},
        {UINT8_C(0x9e), RMW_ABSOLUTE_X, UINT64_C(5), UINT16_C(3)}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_N |
                              MIPSNES2_CPU_FLAG_V |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_C;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;

    for (index = 0U;
         index < sizeof(bit_cases) / sizeof(bit_cases[0]); ++index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            uint8_t width_flag = wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M;
            uint16_t accumulator = wide != 0U ? UINT16_C(0x00ff)
                                               : UINT16_C(0x000f);
            uint16_t input = bit_cases[index].reset_bits
                                 ? (wide != 0U ? UINT16_MAX : UINT8_MAX)
                                 : (wide != 0U ? UINT16_C(0xff00)
                                              : UINT16_C(0x00f0));
            uint16_t expected = bit_cases[index].reset_bits
                                    ? (wide != 0U ? UINT16_C(0xff00)
                                                 : UINT16_C(0x00f0))
                                    : (wide != 0U ? UINT16_MAX
                                                 : UINT8_MAX);
            uint8_t *low;
            uint8_t *high;
            uint64_t expected_cycles = bit_cases[index].base_cycles +
                (wide != 0U ? UINT64_C(2) : UINT64_C(0));

            CHECK(reset_fixture(&fixture, UINT16_C(0x9860)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = UINT16_C(0x1201);
            cpu->emulation = false;
            cpu->a = wide != 0U
                         ? accumulator
                         : (uint16_t)(UINT16_C(0xab00) | accumulator);
            cpu->p = (uint8_t)(preserved | width_flag |
                               MIPSNES2_CPU_FLAG_Z);
            bank7e[0x9860] = bit_cases[index].opcode;
            if (bit_cases[index].direct) {
                bank7e[0x9861] = UINT8_C(0x10);
                low = &bank00[0x1211];
                high = &bank00[0x1212];
                expected_cycles += UINT64_C(1);
            } else {
                bank7e[0x9861] = UINT8_C(0xff);
                bank7e[0x9862] = UINT8_C(0xff);
                low = &bank7f[0xffff];
                high = &bank80[0x0000];
            }
            *low = (uint8_t)input;
            *high = wide != 0U ? (uint8_t)(input >> 8)
                               : UINT8_C(0x5a);

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(*low == (uint8_t)expected);
            CHECK(*high == (wide != 0U
                                ? (uint8_t)(expected >> 8)
                                : UINT8_C(0x5a)));
            CHECK(cpu->a == (wide != 0U
                                 ? accumulator
                                 : (uint16_t)(UINT16_C(0xab00) |
                                              accumulator)));
            CHECK(cpu->p ==
                  (uint8_t)(preserved | width_flag |
                            (bit_cases[index].reset_bits
                                 ? 0U
                                 : MIPSNES2_CPU_FLAG_Z)));
            CHECK(cpu->pc ==
                  (uint16_t)(UINT16_C(0x9860) +
                             bit_cases[index].pc_advance));
            CHECK(cpu->instruction_cycles == expected_cycles);
        }
    }

    for (index = 0U;
         index < sizeof(zero_cases) / sizeof(zero_cases[0]); ++index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            uint8_t width_flag = wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M;
            uint8_t status = (uint8_t)(preserved | width_flag |
                                       MIPSNES2_CPU_FLAG_Z);
            uint8_t *low = NULL;
            uint8_t *high = NULL;
            uint64_t expected_cycles = zero_cases[index].base_cycles +
                (wide != 0U ? UINT64_C(1) : UINT64_C(0));

            CHECK(reset_fixture(&fixture, UINT16_C(0x9870)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = UINT16_C(0x1201);
            cpu->x = UINT16_C(0x0020);
            cpu->a = wide != 0U ? UINT16_C(0x1234)
                                 : UINT16_C(0xab34);
            cpu->emulation = false;
            cpu->p = status;
            bank7e[0x9870] = zero_cases[index].opcode;

            switch (zero_cases[index].mode) {
            case RMW_DIRECT:
                bank7e[0x9871] = UINT8_C(0x10);
                low = &bank00[0x1211];
                high = &bank00[0x1212];
                expected_cycles += UINT64_C(1);
                break;
            case RMW_DIRECT_X:
                bank7e[0x9871] = UINT8_C(0x10);
                low = &bank00[0x1231];
                high = &bank00[0x1232];
                expected_cycles += UINT64_C(1);
                break;
            case RMW_ABSOLUTE:
                bank7e[0x9871] = UINT8_C(0xff);
                bank7e[0x9872] = UINT8_C(0xff);
                low = &bank7f[0xffff];
                high = &bank80[0x0000];
                break;
            case RMW_ABSOLUTE_X:
                bank7e[0x9871] = UINT8_C(0xf0);
                bank7e[0x9872] = UINT8_C(0xff);
                low = &bank80[0x0010];
                high = &bank80[0x0011];
                break;
            default:
                return 1;
            }
            *low = UINT8_C(0xaa);
            *high = UINT8_C(0xbb);

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(*low == 0U);
            CHECK(*high == (wide != 0U ? 0U : UINT8_C(0xbb)));
            CHECK(cpu->a == (wide != 0U ? UINT16_C(0x1234)
                                         : UINT16_C(0xab34)));
            CHECK(cpu->p == status);
            CHECK(cpu->pc ==
                  (uint16_t)(UINT16_C(0x9870) +
                             zero_cases[index].pc_advance));
            CHECK(cpu->instruction_cycles == expected_cycles);
        }
    }

    {
        mipsnes2_bus_interface read_only;
        mipsnes2_cpu invalid;

        CHECK(initialize_cpu_fixture(&fixture, UINT16_C(0x9880)) == 0);
        read_only = mipsnes2_bus_make_interface(&fixture.bus);
        read_only.write8 = NULL;
        mipsnes2_cpu_init(&invalid, read_only);
        CHECK(mipsnes2_cpu_reset(&invalid) == MIPSNES2_CPU_OK);
        invalid.pbr = UINT8_C(0x7e);
        bank7e[0x9880] = UINT8_C(0x06);
        bank7e[0x9881] = UINT8_C(0x10);
        bank00[0x0010] = UINT8_C(0x81);
        CHECK(mipsnes2_cpu_step(&invalid) == MIPSNES2_CPU_INVALID_BUS);
        CHECK(invalid.stopped);
        CHECK(invalid.pc == UINT16_C(0x9881));
        CHECK(invalid.instruction_cycles == 0U);
        CHECK(bank00[0x0010] == UINT8_C(0x81));
    }

    return 0;
}

static int test_stack_relative_binary_arithmetic(void)
{
    static const struct {
        uint8_t opcode;
        bool subtract;
        bool indirect_indexed;
    } cases[] = {
        {UINT8_C(0x63), false, false},
        {UINT8_C(0x73), false, true},
        {UINT8_C(0xe3), true, false},
        {UINT8_C(0xf3), true, true}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_X;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t accumulator = cases[index].subtract
                                       ? (wide != 0U
                                              ? UINT16_C(0x8000)
                                              : UINT16_C(0x0080))
                                       : (wide != 0U
                                              ? UINT16_C(0x7fff)
                                              : UINT16_C(0x007f));
            uint16_t expected = cases[index].subtract
                                    ? (wide != 0U
                                           ? UINT16_C(0x7fff)
                                           : UINT16_C(0x007f))
                                    : (wide != 0U
                                           ? UINT16_C(0x8000)
                                           : UINT16_C(0x0080));
            uint8_t expected_flags = cases[index].subtract
                                         ? MIPSNES2_CPU_FLAG_V |
                                               MIPSNES2_CPU_FLAG_C
                                         : MIPSNES2_CPU_FLAG_N |
                                               MIPSNES2_CPU_FLAG_V;
            uint8_t width_flag = wide != 0U ? 0U
                                             : MIPSNES2_CPU_FLAG_M;

            CHECK(reset_fixture(&fixture, UINT16_C(0x9810)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->s = UINT16_C(0xfff0);
            cpu->y = UINT16_C(0x0020);
            cpu->a = wide != 0U
                         ? accumulator
                         : (uint16_t)(UINT16_C(0xab00) | accumulator);
            cpu->emulation = false;
            cpu->p = (uint8_t)(preserved | width_flag |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               (cases[index].subtract
                                    ? MIPSNES2_CPU_FLAG_C
                                    : 0U));
            bank7e[0x9810] = cases[index].opcode;
            bank7e[0x9811] = UINT8_C(0x0f);

            if (cases[index].indirect_indexed) {
                bank00[0xffff] = UINT8_C(0xf0);
                bank00[0x0000] = UINT8_C(0xff);
                bank80[0x0010] = UINT8_C(0x01);
                bank80[0x0011] = UINT8_C(0x00);
            } else {
                bank00[0xffff] = UINT8_C(0x01);
                bank00[0x0000] = UINT8_C(0x00);
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a ==
                  (wide != 0U
                       ? expected
                       : (uint16_t)(UINT16_C(0xab00) | expected)));
            CHECK(cpu->p == (uint8_t)(preserved | width_flag |
                                      expected_flags));
            CHECK(cpu->pc == UINT16_C(0x9812));
            CHECK(cpu->instruction_cycles ==
                  (cases[index].indirect_indexed ? UINT64_C(7)
                                                 : UINT64_C(4)) + wide);
            CHECK(cpu->s == UINT16_C(0xfff0));
            CHECK(cpu->y == UINT16_C(0x0020));
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->stopped);
            CHECK(!cpu->emulation);
        }
    }

    CHECK(reset_fixture(&fixture, UINT16_C(0x9820)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->s = UINT16_C(0x01f0);
    cpu->a = UINT16_C(0xab10);
    bank7e[0x9820] = UINT8_C(0x63);
    bank7e[0x9821] = UINT8_C(0x30);
    bank00[0x0120] = UINT8_C(0x05);
    bank00[0x0220] = UINT8_C(0x01);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xab11));
    CHECK(cpu->pc == UINT16_C(0x9822));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    CHECK(cpu->emulation);
    CHECK(cpu->s == UINT16_C(0x01f0));
    return 0;
}

static int test_direct_indirect_binary_arithmetic(void)
{
    enum arithmetic_indirect_mode {
        ARITHMETIC_DIRECT_INDEXED_INDIRECT,
        ARITHMETIC_DIRECT_INDIRECT_INDEXED,
        ARITHMETIC_DIRECT_INDIRECT
    };
    static const struct {
        uint8_t opcode;
        bool subtract;
        enum arithmetic_indirect_mode mode;
        bool direct_penalty;
        bool page_crossed;
        bool wide_index;
    } cases[] = {
        {UINT8_C(0x61), false, ARITHMETIC_DIRECT_INDEXED_INDIRECT,
         true, false, false},
        {UINT8_C(0x71), false, ARITHMETIC_DIRECT_INDIRECT_INDEXED,
         true, true, false},
        {UINT8_C(0x72), false, ARITHMETIC_DIRECT_INDIRECT,
         true, false, false},
        {UINT8_C(0xe1), true, ARITHMETIC_DIRECT_INDEXED_INDIRECT,
         false, false, false},
        {UINT8_C(0xf1), true, ARITHMETIC_DIRECT_INDIRECT_INDEXED,
         false, false, true},
        {UINT8_C(0xf2), true, ARITHMETIC_DIRECT_INDIRECT,
         false, false, false}
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t accumulator = cases[index].subtract
                                       ? (wide != 0U
                                              ? UINT16_C(0x8000)
                                              : UINT16_C(0x0080))
                                       : (wide != 0U
                                              ? UINT16_C(0x7fff)
                                              : UINT16_C(0x007f));
            uint16_t expected = cases[index].subtract
                                    ? (wide != 0U
                                           ? UINT16_C(0x7fff)
                                           : UINT16_C(0x007f))
                                    : (wide != 0U
                                           ? UINT16_C(0x8000)
                                           : UINT16_C(0x0080));
            uint8_t expected_flags = cases[index].subtract
                                         ? MIPSNES2_CPU_FLAG_V |
                                               MIPSNES2_CPU_FLAG_C
                                         : MIPSNES2_CPU_FLAG_N |
                                               MIPSNES2_CPU_FLAG_V;
            uint8_t width_flags = (uint8_t)(
                (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                (cases[index].wide_index ? 0U : MIPSNES2_CPU_FLAG_X));
            uint16_t pointer_address;
            uint16_t pointer;
            uint64_t base_cycles;
            uint8_t *target_bank;
            uint16_t target_offset;

            CHECK(reset_fixture(&fixture, UINT16_C(0x9830)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->emulation = false;
            cpu->a = wide != 0U
                         ? accumulator
                         : (uint16_t)(UINT16_C(0xab00) | accumulator);
            cpu->p = (uint8_t)(MIPSNES2_CPU_FLAG_I | width_flags |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               (cases[index].subtract
                                    ? MIPSNES2_CPU_FLAG_C
                                    : 0U));
            bank7e[0x9830] = cases[index].opcode;

            if (cases[index].direct_penalty) {
                cpu->d = cases[index].mode ==
                                 ARITHMETIC_DIRECT_INDEXED_INDIRECT
                             ? UINT16_C(0xffe0)
                             : UINT16_C(0xfff0);
                bank7e[0x9831] = UINT8_C(0x0f);
                if (cases[index].mode ==
                    ARITHMETIC_DIRECT_INDEXED_INDIRECT) {
                    cpu->x = UINT16_C(0x0010);
                }
                pointer_address = UINT16_C(0xffff);
            } else {
                cpu->d = UINT16_C(0x1200);
                bank7e[0x9831] = UINT8_C(0x34);
                if (cases[index].mode ==
                    ARITHMETIC_DIRECT_INDEXED_INDIRECT) {
                    cpu->x = UINT16_C(0x0001);
                    pointer_address = UINT16_C(0x1235);
                } else {
                    pointer_address = UINT16_C(0x1234);
                }
            }

            if (cases[index].mode ==
                ARITHMETIC_DIRECT_INDIRECT_INDEXED) {
                cpu->y = cases[index].page_crossed
                             ? UINT16_C(0x0020)
                             : UINT16_C(0x0001);
                pointer = cases[index].page_crossed
                              ? UINT16_C(0xfff0)
                              : UINT16_C(0x20f0);
            } else {
                pointer = UINT16_C(0x2000);
            }

            bank00[pointer_address] = (uint8_t)pointer;
            bank00[(uint16_t)(pointer_address + UINT16_C(1))] =
                (uint8_t)(pointer >> 8);
            target_bank = cases[index].page_crossed ? bank80 : bank7f;
            target_offset = cases[index].mode ==
                                    ARITHMETIC_DIRECT_INDIRECT_INDEXED
                                ? (uint16_t)(pointer + cpu->y)
                                : pointer;
            target_bank[target_offset] = UINT8_C(0x01);
            target_bank[(uint16_t)(target_offset + UINT16_C(1))] =
                UINT8_C(0x00);

            base_cycles = cases[index].mode ==
                                  ARITHMETIC_DIRECT_INDEXED_INDIRECT
                              ? UINT64_C(6)
                              : UINT64_C(5);
            if (cases[index].direct_penalty) {
                base_cycles += UINT64_C(1);
            }
            if (cases[index].mode ==
                    ARITHMETIC_DIRECT_INDIRECT_INDEXED &&
                (cases[index].page_crossed || cases[index].wide_index)) {
                base_cycles += UINT64_C(1);
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a ==
                  (wide != 0U
                       ? expected
                       : (uint16_t)(UINT16_C(0xab00) | expected)));
            CHECK(cpu->p == (uint8_t)(MIPSNES2_CPU_FLAG_I |
                                      width_flags |
                                      expected_flags));
            CHECK(cpu->pc == UINT16_C(0x9832));
            CHECK(cpu->instruction_cycles == base_cycles + wide);
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->stopped);
            CHECK(!cpu->emulation);
        }
    }
    return 0;
}

static int test_long_binary_arithmetic(void)
{
    enum arithmetic_long_mode {
        ARITHMETIC_DIRECT_INDIRECT_LONG,
        ARITHMETIC_DIRECT_INDIRECT_LONG_Y,
        ARITHMETIC_ABSOLUTE_LONG,
        ARITHMETIC_ABSOLUTE_LONG_X
    };
    static const struct {
        uint8_t opcode;
        bool subtract;
        enum arithmetic_long_mode mode;
        bool direct_penalty;
        bool wide_index;
    } cases[] = {
        {UINT8_C(0x67), false, ARITHMETIC_DIRECT_INDIRECT_LONG,
         true, false},
        {UINT8_C(0x6f), false, ARITHMETIC_ABSOLUTE_LONG,
         false, false},
        {UINT8_C(0x77), false, ARITHMETIC_DIRECT_INDIRECT_LONG_Y,
         true, false},
        {UINT8_C(0x7f), false, ARITHMETIC_ABSOLUTE_LONG_X,
         false, false},
        {UINT8_C(0xe7), true, ARITHMETIC_DIRECT_INDIRECT_LONG,
         false, false},
        {UINT8_C(0xef), true, ARITHMETIC_ABSOLUTE_LONG,
         false, false},
        {UINT8_C(0xf7), true, ARITHMETIC_DIRECT_INDIRECT_LONG_Y,
         false, true},
        {UINT8_C(0xff), true, ARITHMETIC_ABSOLUTE_LONG_X,
         false, true}
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            bool direct = cases[index].mode ==
                              ARITHMETIC_DIRECT_INDIRECT_LONG ||
                          cases[index].mode ==
                              ARITHMETIC_DIRECT_INDIRECT_LONG_Y;
            bool indexed = cases[index].mode ==
                               ARITHMETIC_DIRECT_INDIRECT_LONG_Y ||
                           cases[index].mode ==
                               ARITHMETIC_ABSOLUTE_LONG_X;
            uint16_t accumulator = cases[index].subtract
                                       ? (wide != 0U
                                              ? UINT16_C(0x8000)
                                              : UINT16_C(0x0080))
                                       : (wide != 0U
                                              ? UINT16_C(0x7fff)
                                              : UINT16_C(0x007f));
            uint16_t expected = cases[index].subtract
                                    ? (wide != 0U
                                           ? UINT16_C(0x7fff)
                                           : UINT16_C(0x007f))
                                    : (wide != 0U
                                           ? UINT16_C(0x8000)
                                           : UINT16_C(0x0080));
            uint8_t expected_flags = cases[index].subtract
                                         ? MIPSNES2_CPU_FLAG_V |
                                               MIPSNES2_CPU_FLAG_C
                                         : MIPSNES2_CPU_FLAG_N |
                                               MIPSNES2_CPU_FLAG_V;
            uint8_t width_flags = (uint8_t)(
                (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                (cases[index].wide_index ? 0U : MIPSNES2_CPU_FLAG_X));
            uint16_t pointer_address = 0U;
            mipsnes2_address base_address;
            mipsnes2_address target_address;
            uint16_t index_value = 0U;
            uint8_t *target_bank;
            uint16_t target_offset;
            uint64_t base_cycles;

            CHECK(reset_fixture(&fixture, UINT16_C(0x9850)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x00);
            cpu->emulation = false;
            cpu->a = wide != 0U
                         ? accumulator
                         : (uint16_t)(UINT16_C(0xab00) | accumulator);
            cpu->p = (uint8_t)(MIPSNES2_CPU_FLAG_I | width_flags |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_V |
                               MIPSNES2_CPU_FLAG_Z |
                               (cases[index].subtract
                                    ? MIPSNES2_CPU_FLAG_C
                                    : 0U));
            bank7e[0x9850] = cases[index].opcode;

            if (indexed) {
                index_value = cases[index].wide_index
                                  ? UINT16_C(0x0120)
                                  : UINT16_C(0x0020);
                if (cases[index].mode ==
                    ARITHMETIC_DIRECT_INDIRECT_LONG_Y) {
                    cpu->y = index_value;
                } else {
                    cpu->x = index_value;
                }
            }

            if (direct) {
                cpu->d = cases[index].direct_penalty
                             ? UINT16_C(0xfff0)
                             : UINT16_C(0x1200);
                bank7e[0x9851] = cases[index].direct_penalty
                                     ? UINT8_C(0x0f)
                                     : UINT8_C(0x34);
                pointer_address = cases[index].direct_penalty
                                      ? UINT16_C(0xffff)
                                      : UINT16_C(0x1234);
                if (indexed || cases[index].direct_penalty) {
                    base_address = UINT32_C(0x7ffff0);
                } else {
                    base_address = UINT32_C(0x7f2000);
                }
                bank00[pointer_address] = (uint8_t)base_address;
                bank00[(uint16_t)(pointer_address + UINT16_C(1))] =
                    (uint8_t)(base_address >> 8);
                bank00[(uint16_t)(pointer_address + UINT16_C(2))] =
                    (uint8_t)(base_address >> 16);
                base_cycles = UINT64_C(6) +
                    (cases[index].direct_penalty ? UINT64_C(1)
                                                 : UINT64_C(0));
            } else {
                base_address = cases[index].subtract && !indexed
                                   ? UINT32_C(0x7f2000)
                                   : UINT32_C(0x7ffff0);
                if (!indexed && !cases[index].subtract) {
                    base_address = UINT32_C(0x7fffff);
                }
                bank7e[0x9851] = (uint8_t)base_address;
                bank7e[0x9852] = (uint8_t)(base_address >> 8);
                bank7e[0x9853] = (uint8_t)(base_address >> 16);
                base_cycles = UINT64_C(5);
            }

            target_address = mipsnes2_address24(base_address + index_value);
            target_bank = target_address >= UINT32_C(0x800000)
                              ? bank80
                              : bank7f;
            target_offset = (uint16_t)target_address;
            target_bank[target_offset] = UINT8_C(0x01);
            if (target_address == UINT32_C(0x7fffff)) {
                bank80[0x0000] = UINT8_C(0x00);
            } else {
                target_bank[(uint16_t)(target_offset + UINT16_C(1))] =
                    UINT8_C(0x00);
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->a ==
                  (wide != 0U
                       ? expected
                       : (uint16_t)(UINT16_C(0xab00) | expected)));
            CHECK(cpu->p == (uint8_t)(MIPSNES2_CPU_FLAG_I |
                                      width_flags |
                                      expected_flags));
            CHECK(cpu->pc == (direct ? UINT16_C(0x9852)
                                     : UINT16_C(0x9854)));
            CHECK(cpu->instruction_cycles == base_cycles + wide);
            CHECK(cpu->dbr == UINT8_C(0x00));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->stopped);
            CHECK(!cpu->emulation);
        }
    }
    return 0;
}

static int test_compare_foundation(void)
{
    enum compare_target {
        COMPARE_ACCUMULATOR,
        COMPARE_X,
        COMPARE_Y
    };
    enum operand_mode {
        COMPARE_IMMEDIATE,
        COMPARE_DIRECT,
        COMPARE_ABSOLUTE
    };
    static const struct {
        uint8_t opcode;
        enum compare_target target;
        enum operand_mode mode;
    } cases[] = {
        {UINT8_C(0xc9), COMPARE_ACCUMULATOR, COMPARE_IMMEDIATE},
        {UINT8_C(0xc5), COMPARE_ACCUMULATOR, COMPARE_DIRECT},
        {UINT8_C(0xcd), COMPARE_ACCUMULATOR, COMPARE_ABSOLUTE},
        {UINT8_C(0xe0), COMPARE_X, COMPARE_IMMEDIATE},
        {UINT8_C(0xe4), COMPARE_X, COMPARE_DIRECT},
        {UINT8_C(0xec), COMPARE_X, COMPARE_ABSOLUTE},
        {UINT8_C(0xc0), COMPARE_Y, COMPARE_IMMEDIATE},
        {UINT8_C(0xc4), COMPARE_Y, COMPARE_DIRECT},
        {UINT8_C(0xcc), COMPARE_Y, COMPARE_ABSOLUTE}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_V;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t left;
            uint16_t operand;
            uint8_t result_flags;
            uint8_t width_flags;
            uint16_t expected_pc;
            uint64_t expected_cycles;

            if (cases[index].target == COMPARE_ACCUMULATOR) {
                left = wide != 0U ? UINT16_C(0x4444)
                                  : UINT16_C(0xab44);
                operand = wide != 0U ? UINT16_C(0x4444)
                                     : UINT16_C(0x0044);
                result_flags = MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C;
                width_flags = wide != 0U ? MIPSNES2_CPU_FLAG_X
                                         : MIPSNES2_CPU_FLAG_M;
            } else if (cases[index].target == COMPARE_X) {
                left = wide != 0U ? UINT16_C(0x1000)
                                  : UINT16_C(0x0010);
                operand = wide != 0U ? UINT16_C(0x2000)
                                     : UINT16_C(0x0020);
                result_flags = MIPSNES2_CPU_FLAG_N;
                width_flags = wide != 0U ? MIPSNES2_CPU_FLAG_M
                                         : MIPSNES2_CPU_FLAG_X;
            } else {
                left = wide != 0U ? UINT16_C(0x8000)
                                  : UINT16_C(0x0080);
                operand = UINT16_C(0x0001);
                result_flags = MIPSNES2_CPU_FLAG_C;
                width_flags = wide != 0U ? MIPSNES2_CPU_FLAG_M
                                         : MIPSNES2_CPU_FLAG_X;
            }

            CHECK(reset_fixture(&fixture, UINT16_C(0x9500)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = UINT16_C(0x2201);
            cpu->emulation = false;
            cpu->p = (uint8_t)(preserved | width_flags |
                               MIPSNES2_CPU_FLAG_N |
                               MIPSNES2_CPU_FLAG_Z |
                               MIPSNES2_CPU_FLAG_C);
            cpu->a = cases[index].target == COMPARE_ACCUMULATOR
                         ? left
                         : UINT16_C(0xabcd);
            cpu->x = cases[index].target == COMPARE_X
                         ? left
                         : UINT16_C(0x0030);
            cpu->y = cases[index].target == COMPARE_Y
                         ? left
                         : UINT16_C(0x0040);
            bank7e[0x9500] = cases[index].opcode;

            if (cases[index].mode == COMPARE_IMMEDIATE) {
                bank7e[0x9501] = (uint8_t)operand;
                bank7e[0x9502] = (uint8_t)(operand >> 8);
                expected_pc = (uint16_t)(wide != 0U ? 0x9503 : 0x9502);
                expected_cycles = UINT64_C(2) + wide;
            } else if (cases[index].mode == COMPARE_DIRECT) {
                bank7e[0x9501] = UINT8_C(0x34);
                bank00[0x2235] = (uint8_t)operand;
                bank00[0x2236] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9502);
                expected_cycles = UINT64_C(4) + wide;
            } else {
                bank7e[0x9501] = UINT8_C(0xff);
                bank7e[0x9502] = UINT8_C(0xff);
                bank7f[0xffff] = (uint8_t)operand;
                bank80[0x0000] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9503);
                expected_cycles = UINT64_C(4) + wide;
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->p == (uint8_t)(preserved | width_flags |
                                      result_flags));
            CHECK(cpu->a == (cases[index].target == COMPARE_ACCUMULATOR
                                 ? left
                                 : UINT16_C(0xabcd)));
            CHECK(cpu->x == (cases[index].target == COMPARE_X
                                 ? left
                                 : UINT16_C(0x0030)));
            CHECK(cpu->y == (cases[index].target == COMPARE_Y
                                 ? left
                                 : UINT16_C(0x0040)));
            CHECK(cpu->pc == expected_pc);
            CHECK(cpu->instruction_cycles == expected_cycles);
            CHECK(cpu->d == UINT16_C(0x2201));
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->emulation);
        }
    }
    return 0;
}

static int test_bit_foundation(void)
{
    enum bit_mode {
        BIT_IMMEDIATE,
        BIT_DIRECT,
        BIT_ABSOLUTE,
        BIT_DIRECT_X,
        BIT_ABSOLUTE_X
    };
    static const struct {
        uint8_t opcode;
        enum bit_mode mode;
        uint16_t operand8;
        uint16_t operand16;
        uint8_t initial_nv;
        uint8_t expected_nvz;
    } cases[] = {
        {UINT8_C(0x89), BIT_IMMEDIATE, UINT16_C(0x0080),
         UINT16_C(0x8000), MIPSNES2_CPU_FLAG_V,
         MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x24), BIT_DIRECT, UINT16_C(0x00c0),
         UINT16_C(0xc000), 0U, MIPSNES2_CPU_FLAG_N |
                                      MIPSNES2_CPU_FLAG_V |
                                      MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x2c), BIT_ABSOLUTE, UINT16_C(0x0040),
         UINT16_C(0x4000), MIPSNES2_CPU_FLAG_N,
         MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x34), BIT_DIRECT_X, UINT16_C(0x0080),
         UINT16_C(0x8000), MIPSNES2_CPU_FLAG_V,
         MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z},
        {UINT8_C(0x3c), BIT_ABSOLUTE_X, UINT16_C(0x000f),
         UINT16_C(0x000f), MIPSNES2_CPU_FLAG_N |
                                      MIPSNES2_CPU_FLAG_V,
         0U}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_C |
                              MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_X;
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;
    unsigned wide;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        for (wide = 0U; wide < 2U; ++wide) {
            uint16_t operand = wide != 0U ? cases[index].operand16
                                         : cases[index].operand8;
            uint16_t expected_pc;
            uint64_t expected_cycles;

            CHECK(reset_fixture(&fixture, UINT16_C(0x9600)) == 0);
            cpu->pbr = UINT8_C(0x7e);
            cpu->dbr = UINT8_C(0x7f);
            cpu->d = UINT16_C(0x1201);
            cpu->emulation = false;
            cpu->p = (uint8_t)(preserved |
                               (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                               cases[index].initial_nv |
                               MIPSNES2_CPU_FLAG_Z);
            cpu->a = wide != 0U ? UINT16_C(0x0f0f)
                                : UINT16_C(0xab0f);
            cpu->x = UINT16_C(0x0020);
            cpu->y = UINT16_C(0x0030);
            bank7e[0x9600] = cases[index].opcode;

            if (cases[index].mode == BIT_IMMEDIATE) {
                bank7e[0x9601] = (uint8_t)operand;
                bank7e[0x9602] = (uint8_t)(operand >> 8);
                expected_pc = (uint16_t)(wide != 0U ? 0x9603 : 0x9602);
                expected_cycles = UINT64_C(2) + wide;
            } else if (cases[index].mode == BIT_DIRECT) {
                bank7e[0x9601] = UINT8_C(0x34);
                bank00[0x1235] = (uint8_t)operand;
                bank00[0x1236] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9602);
                expected_cycles = UINT64_C(4) + wide;
            } else if (cases[index].mode == BIT_ABSOLUTE) {
                bank7e[0x9601] = UINT8_C(0xff);
                bank7e[0x9602] = UINT8_C(0xff);
                bank7f[0xffff] = (uint8_t)operand;
                bank80[0x0000] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9603);
                expected_cycles = UINT64_C(4) + wide;
            } else if (cases[index].mode == BIT_DIRECT_X) {
                bank7e[0x9601] = UINT8_C(0x30);
                bank00[0x1251] = (uint8_t)operand;
                bank00[0x1252] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9602);
                expected_cycles = UINT64_C(5) + wide;
            } else {
                bank7e[0x9601] = UINT8_C(0xf0);
                bank7e[0x9602] = UINT8_C(0xff);
                bank80[0x0010] = (uint8_t)operand;
                bank80[0x0011] = (uint8_t)(operand >> 8);
                expected_pc = UINT16_C(0x9603);
                expected_cycles = UINT64_C(5) + wide;
            }

            CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
            CHECK(cpu->p == (uint8_t)(preserved |
                                      (wide != 0U
                                           ? 0U
                                           : MIPSNES2_CPU_FLAG_M) |
                                      cases[index].expected_nvz));
            CHECK(cpu->a == (wide != 0U ? UINT16_C(0x0f0f)
                                        : UINT16_C(0xab0f)));
            CHECK(cpu->x == UINT16_C(0x0020));
            CHECK(cpu->y == UINT16_C(0x0030));
            CHECK(cpu->pc == expected_pc);
            CHECK(cpu->instruction_cycles == expected_cycles);
            CHECK(cpu->d == UINT16_C(0x1201));
            CHECK(cpu->dbr == UINT8_C(0x7f));
            CHECK(cpu->pbr == UINT8_C(0x7e));
            CHECK(!cpu->emulation);
        }
    }
    return 0;
}

static int test_control_flow_and_faults(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8000)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    bank7e[0x8000] = UINT8_C(0x80);
    bank7e[0x8001] = UINT8_C(0x02);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x8004));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x80fe);
    cpu->instruction_cycles = 0U;
    bank7e[0x80fe] = UINT8_C(0x80);
    bank7e[0x80ff] = UINT8_C(0xfe);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x80fe));
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->emulation = false;
    cpu->pc = UINT16_C(0x80fe);
    cpu->instruction_cycles = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x80fe));
    CHECK(cpu->instruction_cycles == UINT64_C(3));
    cpu->emulation = true;

    cpu->pc = UINT16_C(0xffff);
    cpu->instruction_cycles = 0U;
    bank7e[0xffff] = UINT8_C(0xea);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == 0U);
    CHECK(cpu->last_opcode_address == UINT32_C(0x7effff));
    CHECK(cpu->instruction_cycles == UINT64_C(2));

    return 0;
}

static int test_conditional_branches(void)
{
    static const struct {
        uint8_t opcode;
        uint8_t flag;
        bool branch_if_set;
    } cases[] = {
        {UINT8_C(0x10), MIPSNES2_CPU_FLAG_N, false}, /* BPL */
        {UINT8_C(0x30), MIPSNES2_CPU_FLAG_N, true},  /* BMI */
        {UINT8_C(0x50), MIPSNES2_CPU_FLAG_V, false}, /* BVC */
        {UINT8_C(0x70), MIPSNES2_CPU_FLAG_V, true},  /* BVS */
        {UINT8_C(0x90), MIPSNES2_CPU_FLAG_C, false}, /* BCC */
        {UINT8_C(0xb0), MIPSNES2_CPU_FLAG_C, true},  /* BCS */
        {UINT8_C(0xd0), MIPSNES2_CPU_FLAG_Z, false}, /* BNE */
        {UINT8_C(0xf0), MIPSNES2_CPU_FLAG_Z, true}   /* BEQ */
    };
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        uint8_t clear_status = (uint8_t)(UINT8_C(0xff) &
                                         (uint8_t)~cases[index].flag);
        uint8_t taken_status = cases[index].branch_if_set
                                   ? UINT8_C(0xff)
                                   : clear_status;
        uint8_t not_taken_status = cases[index].branch_if_set
                                       ? clear_status
                                       : UINT8_C(0xff);

        CHECK(reset_fixture(&fixture, UINT16_C(0x9000)) == 0);
        cpu->pbr = UINT8_C(0x7e);
        cpu->p = taken_status;
        bank7e[0x9000] = cases[index].opcode;
        bank7e[0x9001] = UINT8_C(0x02);
        CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
        CHECK(cpu->pc == UINT16_C(0x9004));
        CHECK(cpu->pbr == UINT8_C(0x7e));
        CHECK(cpu->p == taken_status);
        CHECK(cpu->instruction_cycles == UINT64_C(3));

        CHECK(reset_fixture(&fixture, UINT16_C(0x9000)) == 0);
        cpu->pbr = UINT8_C(0x7e);
        cpu->p = not_taken_status;
        bank7e[0x9000] = cases[index].opcode;
        bank7e[0x9001] = UINT8_C(0x02);
        CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
        CHECK(cpu->pc == UINT16_C(0x9002));
        CHECK(cpu->pbr == UINT8_C(0x7e));
        CHECK(cpu->p == not_taken_status);
        CHECK(cpu->instruction_cycles == UINT64_C(2));
    }

    CHECK(reset_fixture(&fixture, UINT16_C(0x9200)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_Z;
    bank7e[0x9200] = UINT8_C(0xd0);
    bank7e[0x9201] = UINT8_C(0xfe);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x9200));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x9300);
    cpu->instruction_cycles = 0U;
    bank7e[0x9300] = UINT8_C(0xd0);
    bank7e[0x9301] = UINT8_C(0x7f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x9381));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x9400);
    cpu->instruction_cycles = 0U;
    bank7e[0x9400] = UINT8_C(0xd0);
    bank7e[0x9401] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x9382));
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    return 0;
}

static int test_branch_timing_and_program_bank(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    uint8_t status;

    CHECK(reset_fixture(&fixture, UINT16_C(0x80fd)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
    status = cpu->p;
    bank7e[0x80fd] = UINT8_C(0x90);
    bank7e[0x80fe] = UINT8_C(0x01);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x8100));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->emulation = false;
    cpu->pc = UINT16_C(0x80fd);
    cpu->instruction_cycles = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x8100));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->emulation = true;
    cpu->p |= MIPSNES2_CPU_FLAG_C;
    status = cpu->p;
    cpu->pc = UINT16_C(0x80fd);
    cpu->instruction_cycles = 0U;
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x80ff));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(2));

    cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_Z;
    status = cpu->p;
    cpu->pc = UINT16_C(0xfffe);
    cpu->instruction_cycles = 0U;
    bank7e[0xfffe] = UINT8_C(0xd0);
    bank7e[0xffff] = UINT8_C(0xfe);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->last_opcode_address == UINT32_C(0x7efffe));
    CHECK(cpu->pc == UINT16_C(0xfffe));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    return 0;
}

static int test_branch_relative_long(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    uint8_t status;

    CHECK(reset_fixture(&fixture, UINT16_C(0x9300)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->p = UINT8_C(0xfd);
    status = cpu->p;
    bank7e[0x9300] = UINT8_C(0x82);
    bank7e[0x9301] = UINT8_C(0x34);
    bank7e[0x9302] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0xa537));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->emulation = false;
    cpu->pc = UINT16_C(0x9400);
    cpu->instruction_cycles = 0U;
    bank7e[0x9400] = UINT8_C(0x82);
    bank7e[0x9401] = UINT8_C(0xfc);
    bank7e[0x9402] = UINT8_C(0xff);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x93ff));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->emulation = true;
    cpu->pc = UINT16_C(0xfffd);
    cpu->instruction_cycles = 0U;
    bank7e[0xfffd] = UINT8_C(0x82);
    bank7e[0xfffe] = UINT8_C(0xfe);
    bank7e[0xffff] = UINT8_C(0xff);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->last_opcode_address == UINT32_C(0x7efffd));
    CHECK(cpu->pc == UINT16_C(0xfffe));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->pc = UINT16_C(0x1000);
    cpu->instruction_cycles = 0U;
    bank7e[0x1000] = UINT8_C(0x82);
    bank7e[0x1001] = UINT8_C(0xff);
    bank7e[0x1002] = UINT8_C(0x7f);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x9002));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->pc = UINT16_C(0x9000);
    cpu->instruction_cycles = 0U;
    bank7e[0x9000] = UINT8_C(0x82);
    bank7e[0x9001] = UINT8_C(0x00);
    bank7e[0x9002] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x1003));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    return 0;
}

static int test_rep_in_emulation(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8000)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    bank7e[0x8000] = UINT8_C(0xe2);
    bank7e[0x8001] = UINT8_C(0xc3);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
                     MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C)) ==
          (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
           MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C));
    bank7e[0x8002] = UINT8_C(0xc2);
    bank7e[0x8003] = UINT8_C(0xf3);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X)) ==
          (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
                     MIPSNES2_CPU_FLAG_Z | MIPSNES2_CPU_FLAG_C)) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(6));
    return 0;
}

static int test_stack_push_pull_emulation(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8000)) == 0);
    cpu->pbr = UINT8_C(0x7e);

    cpu->p = UINT8_C(0xfd);
    cpu->s = UINT16_C(0x01ff);
    bank7e[0x8000] = UINT8_C(0x08); /* PHP */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x01ff] == UINT8_C(0xfd));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->pc == UINT16_C(0x8001));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x8010);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x0100);
    cpu->d = UINT16_C(0x1234);
    bank7e[0x8010] = UINT8_C(0x0b); /* PHD */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x0100] == UINT8_C(0x12));
    CHECK(bank00[0x00ff] == UINT8_C(0x34));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->pc = UINT16_C(0x8020);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01ff);
    cpu->a = UINT16_C(0xab80);
    bank7e[0x8020] = UINT8_C(0x48); /* PHA */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x01ff] == UINT8_C(0x80));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x8030);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01ff);
    bank7e[0x8030] = UINT8_C(0x4b); /* PHK */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x01ff] == UINT8_C(0x7e));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x8040);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01ff);
    cpu->y = UINT16_C(0x005a);
    bank7e[0x8040] = UINT8_C(0x5a); /* PHY */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x01ff] == UINT8_C(0x5a));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x8050);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01ff);
    cpu->dbr = UINT8_C(0x80);
    bank7e[0x8050] = UINT8_C(0x8b); /* PHB */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x01ff] == UINT8_C(0x80));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x8060);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01ff);
    cpu->x = UINT16_C(0x0042);
    bank7e[0x8060] = UINT8_C(0xda); /* PHX */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x01ff] == UINT8_C(0x42));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(3));

    cpu->pc = UINT16_C(0x8070);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01fe);
    cpu->x = UINT16_C(0xabcd);
    cpu->y = UINT16_C(0xef12);
    bank00[0x01ff] = 0U;
    bank7e[0x8070] = UINT8_C(0x28); /* PLP */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->p == (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X));
    CHECK(cpu->x == UINT16_C(0x00cd));
    CHECK(cpu->y == UINT16_C(0x0012));
    CHECK(cpu->s == UINT16_C(0x01ff));
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->pc = UINT16_C(0x8080);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01ff);
    cpu->p = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
             MIPSNES2_CPU_FLAG_Z;
    bank00[0x0200] = UINT8_C(0x34);
    bank00[0x0201] = UINT8_C(0x80);
    bank7e[0x8080] = UINT8_C(0x2b); /* PLD */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->d == UINT16_C(0x8034));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) == 0U);
    CHECK(cpu->s == UINT16_C(0x0101));
    CHECK(cpu->instruction_cycles == UINT64_C(5));

    cpu->pc = UINT16_C(0x8090);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01fe);
    cpu->a = UINT16_C(0xabff);
    bank00[0x01ff] = 0U;
    bank7e[0x8090] = UINT8_C(0x68); /* PLA */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xab00));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) == 0U);
    CHECK(cpu->s == UINT16_C(0x01ff));
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->pc = UINT16_C(0x80a0);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01fe);
    bank00[0x01ff] = UINT8_C(0x80);
    bank7e[0x80a0] = UINT8_C(0x7a); /* PLY */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x0080));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->pc = UINT16_C(0x80b0);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01fe);
    bank00[0x01ff] = 0U;
    bank7e[0x80b0] = UINT8_C(0xab); /* PLB */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->dbr == 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->pc = UINT16_C(0x80c0);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01fe);
    bank00[0x01ff] = UINT8_C(0x7f);
    bank7e[0x80c0] = UINT8_C(0xfa); /* PLX */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == UINT16_C(0x007f));
    CHECK((cpu->p & (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z)) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(4));
    return 0;
}

static int test_stack_native_widths(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8200)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);

    cpu->s = 0U;
    cpu->a = UINT16_C(0x1234);
    bank7e[0x8200] = UINT8_C(0x48); /* PHA 16-bit */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x0000] == UINT8_C(0x12));
    CHECK(bank00[0xffff] == UINT8_C(0x34));
    CHECK(cpu->s == UINT16_C(0xfffe));
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->a = 0U;
    bank7e[0x8201] = UINT8_C(0x68); /* PLA 16-bit */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(cpu->s == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(9));

    cpu->s = UINT16_C(0x3000);
    cpu->x = UINT16_C(0x8001);
    cpu->y = UINT16_C(0x0000);
    bank7e[0x8202] = UINT8_C(0xda); /* PHX 16-bit */
    bank7e[0x8203] = UINT8_C(0x7a); /* PLY 16-bit */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x3000] == UINT8_C(0x80));
    CHECK(bank00[0x2fff] == UINT8_C(0x01));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x8001));
    CHECK(cpu->s == UINT16_C(0x3000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(18));

    cpu->s = UINT16_C(0x4000);
    cpu->y = UINT16_C(0x0000);
    cpu->x = UINT16_C(0xffff);
    bank7e[0x8204] = UINT8_C(0x5a); /* PHY 16-bit */
    bank7e[0x8205] = UINT8_C(0xfa); /* PLX 16-bit */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->x == 0U);
    CHECK(cpu->s == UINT16_C(0x4000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_Z) != 0U);
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) == 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(27));

    cpu->p |= MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X;
    cpu->s = UINT16_C(0x5000);
    cpu->a = UINT16_C(0xab80);
    bank7e[0x8206] = UINT8_C(0x48); /* PHA 8-bit */
    bank7e[0x8207] = UINT8_C(0x68); /* PLA 8-bit */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    cpu->a = UINT16_C(0xcd00);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->a == UINT16_C(0xcd80));
    CHECK(cpu->s == UINT16_C(0x5000));
    CHECK((cpu->p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu->instruction_cycles == UINT64_C(34));

    cpu->s = UINT16_C(0x6000);
    cpu->x = UINT16_C(0x007f);
    bank7e[0x8208] = UINT8_C(0xda); /* PHX 8-bit */
    bank7e[0x8209] = UINT8_C(0x7a); /* PLY 8-bit */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->y == UINT16_C(0x007f));
    CHECK(cpu->s == UINT16_C(0x6000));
    CHECK(cpu->instruction_cycles == UINT64_C(41));

    cpu->pc = UINT16_C(0x8210);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x7000);
    cpu->p = MIPSNES2_CPU_FLAG_N;
    bank7e[0x8210] = UINT8_C(0x08); /* PHP native */
    bank7e[0x8211] = UINT8_C(0x28); /* PLP native */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x7000] == MIPSNES2_CPU_FLAG_N);
    bank00[0x7000] = MIPSNES2_CPU_FLAG_X | MIPSNES2_CPU_FLAG_C;
    cpu->x = UINT16_C(0xabcd);
    cpu->y = UINT16_C(0xef12);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->p == (MIPSNES2_CPU_FLAG_X | MIPSNES2_CPU_FLAG_C));
    CHECK(cpu->x == UINT16_C(0x00cd));
    CHECK(cpu->y == UINT16_C(0x0012));
    CHECK(cpu->s == UINT16_C(0x7000));
    CHECK(cpu->instruction_cycles == UINT64_C(7));
    return 0;
}

static int test_effective_address_pushes(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8400)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->s = UINT16_C(0x0100);
    bank7e[0x8400] = UINT8_C(0xf4); /* PEA $1234 */
    bank7e[0x8401] = UINT8_C(0x34);
    bank7e[0x8402] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x0100] == UINT8_C(0x12));
    CHECK(bank00[0x00ff] == UINT8_C(0x34));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->pc == UINT16_C(0x8403));
    CHECK(cpu->instruction_cycles == UINT64_C(5));

    cpu->pc = UINT16_C(0x8500);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x0100);
    bank7e[0x8500] = UINT8_C(0x62); /* PER -4 */
    bank7e[0x8501] = UINT8_C(0xfc);
    bank7e[0x8502] = UINT8_C(0xff);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x0100] == UINT8_C(0x84));
    CHECK(bank00[0x00ff] == UINT8_C(0xff));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->pc == UINT16_C(0x8503));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->pc = UINT16_C(0x8600);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x0100);
    cpu->d = UINT16_C(0x0200);
    bank7e[0x8600] = UINT8_C(0xd4); /* PEI ($ff) */
    bank7e[0x8601] = UINT8_C(0xff);
    bank00[0x02ff] = UINT8_C(0x78);
    bank00[0x0200] = UINT8_C(0x56);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x0100] == UINT8_C(0x56));
    CHECK(bank00[0x00ff] == UINT8_C(0x78));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->emulation = false;
    cpu->p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    cpu->pc = UINT16_C(0x8700);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x4000);
    cpu->d = UINT16_C(0x1201);
    bank7e[0x8700] = UINT8_C(0xd4); /* PEI ($fe), DL penalty */
    bank7e[0x8701] = UINT8_C(0xfe);
    bank00[0x12ff] = UINT8_C(0xcd);
    bank00[0x1300] = UINT8_C(0xab);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank00[0x4000] == UINT8_C(0xab));
    CHECK(bank00[0x3fff] == UINT8_C(0xcd));
    CHECK(cpu->s == UINT16_C(0x3ffe));
    CHECK(cpu->instruction_cycles == UINT64_C(7));
    return 0;
}

static int test_subroutine_control_instructions(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8800)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    bank7e[0x8800] = UINT8_C(0x5c); /* JMP $80:3456 */
    bank7e[0x8801] = UINT8_C(0x56);
    bank7e[0x8802] = UINT8_C(0x34);
    bank7e[0x8803] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x3456));
    CHECK(cpu->pbr == UINT8_C(0x80));
    CHECK(cpu->instruction_cycles == UINT64_C(4));

    cpu->pbr = UINT8_C(0x7e);
    cpu->pc = UINT16_C(0x8900);
    cpu->instruction_cycles = 0U;
    bank7e[0x8900] = UINT8_C(0x6c); /* JMP ($ffff) */
    bank7e[0x8901] = UINT8_C(0xff);
    bank7e[0x8902] = UINT8_C(0xff);
    bank00[0xffff] = UINT8_C(0x34);
    bank00[0x0000] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x1234));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->instruction_cycles == UINT64_C(5));

    cpu->pc = UINT16_C(0x8a00);
    cpu->instruction_cycles = 0U;
    cpu->x = UINT16_C(0x0001);
    bank7e[0x8a00] = UINT8_C(0x7c); /* JMP ($fffe,X) */
    bank7e[0x8a01] = UINT8_C(0xfe);
    bank7e[0x8a02] = UINT8_C(0xff);
    bank7e[0xffff] = UINT8_C(0x78);
    bank7e[0x0000] = UINT8_C(0x56);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x5678));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->pc = UINT16_C(0x8b00);
    cpu->instruction_cycles = 0U;
    bank7e[0x8b00] = UINT8_C(0xdc); /* JMP [$fffe] */
    bank7e[0x8b01] = UINT8_C(0xfe);
    bank7e[0x8b02] = UINT8_C(0xff);
    bank00[0xfffe] = UINT8_C(0xbc);
    bank00[0xffff] = UINT8_C(0x9a);
    bank00[0x0000] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x9abc));
    CHECK(cpu->pbr == UINT8_C(0x80));
    CHECK(cpu->instruction_cycles == UINT64_C(6));

    cpu->pbr = UINT8_C(0x7e);
    cpu->pc = UINT16_C(0x8c00);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x0100);
    bank7e[0x8c00] = UINT8_C(0x20); /* JSR $9000 */
    bank7e[0x8c01] = 0U;
    bank7e[0x8c02] = UINT8_C(0x90);
    bank7e[0x9000] = UINT8_C(0x60); /* RTS */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x9000));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(bank00[0x0100] == UINT8_C(0x8c));
    CHECK(bank00[0x01ff] == UINT8_C(0x02));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(6));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x8c03));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->s == UINT16_C(0x0100));
    CHECK(cpu->instruction_cycles == UINT64_C(12));

    cpu->pc = UINT16_C(0x8d00);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01ff);
    bank7e[0x8d00] = UINT8_C(0x22); /* JSL $80:9100 */
    bank7e[0x8d01] = 0U;
    bank7e[0x8d02] = UINT8_C(0x91);
    bank7e[0x8d03] = UINT8_C(0x80);
    bank80[0x9100] = UINT8_C(0x6b); /* RTL */
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x9100));
    CHECK(cpu->pbr == UINT8_C(0x80));
    CHECK(bank00[0x01ff] == UINT8_C(0x7e));
    CHECK(bank00[0x01fe] == UINT8_C(0x8d));
    CHECK(bank00[0x01fd] == UINT8_C(0x03));
    CHECK(cpu->s == UINT16_C(0x01fc));
    CHECK(cpu->instruction_cycles == UINT64_C(8));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x8d04));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(cpu->s == UINT16_C(0x01ff));
    CHECK(cpu->instruction_cycles == UINT64_C(14));

    cpu->pc = UINT16_C(0x8e00);
    cpu->pbr = UINT8_C(0x7e);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x0100);
    cpu->x = UINT16_C(0x0001);
    bank7e[0x8e00] = UINT8_C(0xfc); /* JSR ($fffe,X) */
    bank7e[0x8e01] = UINT8_C(0xfe);
    bank7e[0x8e02] = UINT8_C(0xff);
    bank7e[0xffff] = UINT8_C(0x57);
    bank7e[0x0000] = UINT8_C(0x13);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x1357));
    CHECK(cpu->pbr == UINT8_C(0x7e));
    CHECK(bank00[0x0100] == UINT8_C(0x8e));
    CHECK(bank00[0x00ff] == UINT8_C(0x02));
    CHECK(cpu->s == UINT16_C(0x01fe));
    CHECK(cpu->instruction_cycles == UINT64_C(8));

    cpu->pc = UINT16_C(0x8f00);
    cpu->pbr = UINT8_C(0x7e);
    cpu->instruction_cycles = 0U;
    cpu->s = UINT16_C(0x01ff);
    bank7e[0x8f00] = UINT8_C(0x6b); /* RTL boundary exception */
    bank00[0x0200] = UINT8_C(0x34);
    bank00[0x0201] = UINT8_C(0x12);
    bank00[0x0202] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x1235));
    CHECK(cpu->pbr == UINT8_C(0x80));
    CHECK(cpu->s == UINT16_C(0x0102));
    CHECK(cpu->instruction_cycles == UINT64_C(6));
    return 0;
}

static int test_stack_bus_access_order_and_invalid_bus(void)
{
    trace_bus_context trace;
    mipsnes2_cpu cpu;

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x9000)) == 0);
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x01ff);
    bank7e[0x9000] = UINT8_C(0x22); /* JSL $80:3456 */
    bank7e[0x9001] = UINT8_C(0x56);
    bank7e[0x9002] = UINT8_C(0x34);
    bank7e[0x9003] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(trace.count == 7U);
    CHECK(!trace.accesses[0].write &&
          trace.accesses[0].address == UINT32_C(0x7e9000));
    CHECK(!trace.accesses[1].write &&
          trace.accesses[1].address == UINT32_C(0x7e9001));
    CHECK(!trace.accesses[2].write &&
          trace.accesses[2].address == UINT32_C(0x7e9002));
    CHECK(trace.accesses[3].write &&
          trace.accesses[3].address == UINT32_C(0x0001ff) &&
          trace.accesses[3].value == UINT8_C(0x7e));
    CHECK(!trace.accesses[4].write &&
          trace.accesses[4].address == UINT32_C(0x7e9003));
    CHECK(trace.accesses[5].write &&
          trace.accesses[5].address == UINT32_C(0x0001fe) &&
          trace.accesses[5].value == UINT8_C(0x90));
    CHECK(trace.accesses[6].write &&
          trace.accesses[6].address == UINT32_C(0x0001fd) &&
          trace.accesses[6].value == UINT8_C(0x03));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x9100)) == 0);
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x0100);
    cpu.x = UINT16_C(0x0001);
    bank7e[0x9100] = UINT8_C(0xfc); /* JSR ($fffe,X) */
    bank7e[0x9101] = UINT8_C(0xfe);
    bank7e[0x9102] = UINT8_C(0xff);
    bank7e[0xffff] = UINT8_C(0x34);
    bank7e[0x0000] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(trace.count == 7U);
    CHECK(!trace.accesses[0].write &&
          trace.accesses[0].address == UINT32_C(0x7e9100));
    CHECK(!trace.accesses[1].write &&
          trace.accesses[1].address == UINT32_C(0x7e9101));
    CHECK(trace.accesses[2].write &&
          trace.accesses[2].address == UINT32_C(0x000100) &&
          trace.accesses[2].value == UINT8_C(0x91));
    CHECK(trace.accesses[3].write &&
          trace.accesses[3].address == UINT32_C(0x0000ff) &&
          trace.accesses[3].value == UINT8_C(0x02));
    CHECK(!trace.accesses[4].write &&
          trace.accesses[4].address == UINT32_C(0x7e9102));
    CHECK(!trace.accesses[5].write &&
          trace.accesses[5].address == UINT32_C(0x7effff));
    CHECK(!trace.accesses[6].write &&
          trace.accesses[6].address == UINT32_C(0x7e0000));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x9200)) == 0);
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x01ff);
    cpu.bus.write8 = NULL;
    bank7e[0x9200] = UINT8_C(0x22);
    bank7e[0x9201] = UINT8_C(0x56);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_INVALID_BUS);
    CHECK(cpu.stopped);
    CHECK(cpu.stop_reason == MIPSNES2_CPU_INVALID_BUS);
    CHECK(cpu.pc == UINT16_C(0x9201));
    CHECK(cpu.s == UINT16_C(0x01ff));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 1U);
    CHECK(!trace.accesses[0].write &&
          trace.accesses[0].address == UINT32_C(0x7e9200));
    return 0;
}

static int test_software_interrupts_and_rti(void)
{
    trace_bus_context trace;
    mipsnes2_cpu cpu;
    uint8_t pushed_status;

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x8000)) == 0);
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x0100);
    cpu.p |= MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_C;
    pushed_status = cpu.p;
    bank7e[0x8000] = UINT8_C(0x00);
    bank7e[0x8001] = UINT8_C(0x5a);
    bank00[0xfffe] = UINT8_C(0x34);
    bank00[0xffff] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x01fd));
    CHECK(cpu.p == (uint8_t)((pushed_status |
                              MIPSNES2_CPU_FLAG_I) &
                             (uint8_t)~MIPSNES2_CPU_FLAG_D));
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 7U);
    CHECK(!trace.accesses[0].write &&
          trace.accesses[0].address == UINT32_C(0x7e8000));
    CHECK(!trace.accesses[1].write &&
          trace.accesses[1].address == UINT32_C(0x7e8001));
    CHECK(trace.accesses[2].write &&
          trace.accesses[2].address == UINT32_C(0x000100) &&
          trace.accesses[2].value == UINT8_C(0x80));
    CHECK(trace.accesses[3].write &&
          trace.accesses[3].address == UINT32_C(0x0001ff) &&
          trace.accesses[3].value == UINT8_C(0x02));
    CHECK(trace.accesses[4].write &&
          trace.accesses[4].address == UINT32_C(0x0001fe) &&
          trace.accesses[4].value == pushed_status);
    CHECK(!trace.accesses[5].write &&
          trace.accesses[5].address == UINT32_C(0x00fffe));
    CHECK(!trace.accesses[6].write &&
          trace.accesses[6].address == UINT32_C(0x00ffff));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x8100)) == 0);
    cpu.emulation = false;
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x2000);
    cpu.p = MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_C;
    pushed_status = cpu.p;
    bank7e[0x8100] = UINT8_C(0x02);
    bank7e[0x8101] = UINT8_C(0xa5);
    bank00[0xffe4] = UINT8_C(0x78);
    bank00[0xffe5] = UINT8_C(0x56);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x1ffc));
    CHECK(cpu.p == (MIPSNES2_CPU_FLAG_I | MIPSNES2_CPU_FLAG_C));
    CHECK(cpu.instruction_cycles == UINT64_C(8));
    CHECK(trace.count == 8U);
    CHECK(trace.accesses[2].write &&
          trace.accesses[2].address == UINT32_C(0x002000) &&
          trace.accesses[2].value == UINT8_C(0x7e));
    CHECK(trace.accesses[3].write &&
          trace.accesses[3].value == UINT8_C(0x81));
    CHECK(trace.accesses[4].write &&
          trace.accesses[4].value == UINT8_C(0x02));
    CHECK(trace.accesses[5].write &&
          trace.accesses[5].value == pushed_status);
    CHECK(trace.accesses[6].address == UINT32_C(0x00ffe4));
    CHECK(trace.accesses[7].address == UINT32_C(0x00ffe5));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x8150)) == 0);
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x01ff);
    bank7e[0x8150] = UINT8_C(0x02);
    bank7e[0x8151] = UINT8_C(0x11);
    bank00[0xfff4] = UINT8_C(0x44);
    bank00[0xfff5] = UINT8_C(0x33);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x3344));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x01fc));
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(bank00[0x01fd] ==
          (uint8_t)((MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
                     MIPSNES2_CPU_FLAG_I) &
                    (uint8_t)~MIPSNES2_CPU_FLAG_X));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x8180)) == 0);
    cpu.emulation = false;
    cpu.pbr = UINT8_C(0x80);
    cpu.s = UINT16_C(0x2100);
    bank80[0x8180] = UINT8_C(0x00);
    bank80[0x8181] = UINT8_C(0x22);
    bank00[0xffe6] = UINT8_C(0x88);
    bank00[0xffe7] = UINT8_C(0x77);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x7788));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x20fc));
    CHECK(cpu.instruction_cycles == UINT64_C(8));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x8200)) == 0);
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x01fc);
    cpu.x = UINT16_C(0xabcd);
    cpu.y = UINT16_C(0xef12);
    bank7e[0x8200] = UINT8_C(0x40);
    bank00[0x01fd] = MIPSNES2_CPU_FLAG_D;
    bank00[0x01fe] = UINT8_C(0x9a);
    bank00[0x01ff] = UINT8_C(0xbc);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0xbc9a));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.p == (MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_M |
                    MIPSNES2_CPU_FLAG_X));
    CHECK(cpu.s == UINT16_C(0x01ff));
    CHECK(cpu.x == UINT16_C(0x00cd));
    CHECK(cpu.y == UINT16_C(0x0012));
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 4U);

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x8300)) == 0);
    cpu.emulation = false;
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0xfffb);
    bank7e[0x8300] = UINT8_C(0x40);
    bank00[0xfffc] = MIPSNES2_CPU_FLAG_C;
    bank00[0xfffd] = UINT8_C(0xef);
    bank00[0xfffe] = UINT8_C(0xbe);
    bank00[0xffff] = UINT8_C(0x80);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0xbeef));
    CHECK(cpu.pbr == UINT8_C(0x80));
    CHECK(cpu.p == MIPSNES2_CPU_FLAG_C);
    CHECK(cpu.s == UINT16_C(0xffff));
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 5U);
    return 0;
}

static int test_wdm_and_block_moves(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    trace_bus_context trace;
    mipsnes2_cpu trace_cpu;
    uint8_t status;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8400)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->a = UINT16_C(0x1234);
    cpu->x = UINT16_C(0x0056);
    cpu->y = UINT16_C(0x0078);
    status = cpu->p;
    bank7e[0x8400] = UINT8_C(0x42);
    bank7e[0x8401] = UINT8_C(0xa5);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x8402));
    CHECK(cpu->a == UINT16_C(0x1234));
    CHECK(cpu->x == UINT16_C(0x0056));
    CHECK(cpu->y == UINT16_C(0x0078));
    CHECK(cpu->p == status);
    CHECK(cpu->instruction_cycles == UINT64_C(2));

    CHECK(reset_fixture(&fixture, UINT16_C(0x8480)) == 0);
    cpu->emulation = false;
    cpu->pbr = UINT8_C(0x7e);
    cpu->a = UINT16_C(0xabcd);
    cpu->p = MIPSNES2_CPU_FLAG_C | MIPSNES2_CPU_FLAG_V;
    bank7e[0x8480] = UINT8_C(0x42);
    bank7e[0x8481] = UINT8_C(0x3c);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->pc == UINT16_C(0x8482));
    CHECK(cpu->a == UINT16_C(0xabcd));
    CHECK(cpu->p == (MIPSNES2_CPU_FLAG_C | MIPSNES2_CPU_FLAG_V));
    CHECK(cpu->instruction_cycles == UINT64_C(2));

    CHECK(reset_fixture(&fixture, UINT16_C(0x8500)) == 0);
    cpu->emulation = false;
    cpu->pbr = UINT8_C(0x7e);
    cpu->p = 0U;
    cpu->a = UINT16_C(0x0001);
    cpu->x = UINT16_C(0xffff);
    cpu->y = UINT16_C(0x0001);
    bank7e[0x8500] = UINT8_C(0x54);
    bank7e[0x8501] = UINT8_C(0x7f);
    bank7e[0x8502] = UINT8_C(0x80);
    bank80[0xffff] = UINT8_C(0x11);
    bank80[0x0000] = UINT8_C(0x22);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x0001] == UINT8_C(0x11));
    CHECK(cpu->a == 0U);
    CHECK(cpu->x == 0U);
    CHECK(cpu->y == UINT16_C(0x0002));
    CHECK(cpu->pc == UINT16_C(0x8500));
    CHECK(cpu->dbr == UINT8_C(0x7f));
    CHECK(cpu->instruction_cycles == UINT64_C(7));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x0002] == UINT8_C(0x22));
    CHECK(cpu->a == UINT16_C(0xffff));
    CHECK(cpu->x == UINT16_C(0x0001));
    CHECK(cpu->y == UINT16_C(0x0003));
    CHECK(cpu->pc == UINT16_C(0x8503));
    CHECK(cpu->instruction_cycles == UINT64_C(14));

    CHECK(reset_fixture(&fixture, UINT16_C(0x8600)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->a = 0U;
    cpu->x = 0U;
    cpu->y = 0U;
    bank7e[0x8600] = UINT8_C(0x44);
    bank7e[0x8601] = UINT8_C(0x7f);
    bank7e[0x8602] = UINT8_C(0x80);
    bank80[0x0000] = UINT8_C(0x9c);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x0000] == UINT8_C(0x9c));
    CHECK(cpu->a == UINT16_C(0xffff));
    CHECK(cpu->x == UINT16_C(0x00ff));
    CHECK(cpu->y == UINT16_C(0x00ff));
    CHECK(cpu->pc == UINT16_C(0x8603));
    CHECK(cpu->instruction_cycles == UINT64_C(7));

    CHECK(reset_fixture(&fixture, UINT16_C(0x8680)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->a = 0U;
    cpu->x = UINT16_C(0x00ff);
    cpu->y = UINT16_C(0x00fe);
    bank7e[0x8680] = UINT8_C(0x54);
    bank7e[0x8681] = UINT8_C(0x7f);
    bank7e[0x8682] = UINT8_C(0x80);
    bank80[0x00ff] = UINT8_C(0x6d);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(bank7f[0x00fe] == UINT8_C(0x6d));
    CHECK(cpu->x == 0U);
    CHECK(cpu->y == UINT16_C(0x00ff));
    CHECK(cpu->a == UINT16_C(0xffff));

    CHECK(initialize_trace_cpu(&trace_cpu, &trace, UINT16_C(0x86c0)) == 0);
    trace_cpu.emulation = false;
    trace_cpu.pbr = UINT8_C(0x7e);
    trace_cpu.p = 0U;
    trace_cpu.a = 0U;
    trace_cpu.x = UINT16_C(0x1000);
    trace_cpu.y = UINT16_C(0x2000);
    bank7e[0x86c0] = UINT8_C(0x44);
    bank7e[0x86c1] = UINT8_C(0x7f);
    bank7e[0x86c2] = UINT8_C(0x80);
    bank80[0x1000] = UINT8_C(0x4a);
    CHECK(mipsnes2_cpu_step(&trace_cpu) == MIPSNES2_CPU_OK);
    CHECK(trace.count == 5U);
    CHECK(!trace.accesses[0].write &&
          trace.accesses[0].address == UINT32_C(0x7e86c0));
    CHECK(!trace.accesses[1].write &&
          trace.accesses[1].address == UINT32_C(0x7e86c1));
    CHECK(!trace.accesses[2].write &&
          trace.accesses[2].address == UINT32_C(0x7e86c2));
    CHECK(!trace.accesses[3].write &&
          trace.accesses[3].address == UINT32_C(0x801000) &&
          trace.accesses[3].value == UINT8_C(0x4a));
    CHECK(trace.accesses[4].write &&
          trace.accesses[4].address == UINT32_C(0x7f2000) &&
          trace.accesses[4].value == UINT8_C(0x4a));
    CHECK(trace_cpu.x == UINT16_C(0x0fff));
    CHECK(trace_cpu.y == UINT16_C(0x1fff));

    CHECK(initialize_trace_cpu(&trace_cpu, &trace, UINT16_C(0x8700)) == 0);
    trace_cpu.pbr = UINT8_C(0x7e);
    trace_cpu.bus.write8 = NULL;
    bank7e[0x8700] = UINT8_C(0x54);
    bank7e[0x8701] = UINT8_C(0x7f);
    CHECK(mipsnes2_cpu_step(&trace_cpu) == MIPSNES2_CPU_INVALID_BUS);
    CHECK(trace_cpu.stopped);
    CHECK(trace_cpu.pc == UINT16_C(0x8701));
    CHECK(trace_cpu.instruction_cycles == 0U);
    CHECK(trace.count == 1U);
    return 0;
}

static int test_wait_stop_and_interrupt_inputs(void)
{
    cpu_fixture fixture;
    mipsnes2_cpu *cpu = &fixture.cpu;
    trace_bus_context trace;
    mipsnes2_cpu native;
    uint64_t cycles;

    CHECK(reset_fixture(&fixture, UINT16_C(0x8800)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    bank7e[0x8800] = UINT8_C(0xcb);
    bank7e[0x8801] = UINT8_C(0xea);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->waiting);
    CHECK(cpu->pc == UINT16_C(0x8801));
    CHECK(cpu->instruction_cycles == UINT64_C(3));
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_WAITING);
    CHECK(cpu->pc == UINT16_C(0x8801));
    CHECK(cpu->instruction_cycles == UINT64_C(3));
    mipsnes2_cpu_set_irq(cpu, true);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu->waiting);
    CHECK(cpu->pc == UINT16_C(0x8802));
    CHECK(cpu->instruction_cycles == UINT64_C(5));
    mipsnes2_cpu_set_irq(cpu, false);

    CHECK(reset_fixture(&fixture, UINT16_C(0x8900)) == 0);
    cpu->pbr = UINT8_C(0x7e);
    cpu->p &= (uint8_t)~MIPSNES2_CPU_FLAG_I;
    cpu->s = UINT16_C(0x01ff);
    bank7e[0x8900] = UINT8_C(0xcb);
    bank00[0xfffe] = UINT8_C(0x00);
    bank00[0xffff] = UINT8_C(0x90);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    mipsnes2_cpu_set_irq(cpu, true);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu->waiting);
    CHECK(cpu->pc == UINT16_C(0x9000));
    CHECK(cpu->pbr == 0U);
    CHECK(cpu->s == UINT16_C(0x01fc));
    CHECK(cpu->instruction_cycles == UINT64_C(10));

    CHECK(reset_fixture(&fixture, UINT16_C(0x8980)) == 0);
    cpu->emulation = false;
    cpu->pbr = UINT8_C(0x7e);
    cpu->p = MIPSNES2_CPU_FLAG_I;
    cpu->s = UINT16_C(0x2000);
    bank7e[0x8980] = UINT8_C(0xcb);
    bank00[0xffea] = UINT8_C(0x45);
    bank00[0xffeb] = UINT8_C(0x23);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    mipsnes2_cpu_pulse_nmi(cpu);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu->waiting);
    CHECK(cpu->pc == UINT16_C(0x2345));
    CHECK(cpu->pbr == 0U);
    CHECK(cpu->s == UINT16_C(0x1ffc));
    CHECK(cpu->instruction_cycles == UINT64_C(11));

    CHECK(reset_fixture(&fixture, UINT16_C(0x8a00)) == 0);
    cpu->emulation = false;
    cpu->p = MIPSNES2_CPU_FLAG_I;
    cpu->pbr = UINT8_C(0x7e);
    bank7e[0x8a00] = UINT8_C(0xdb);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu->clock_stopped);
    CHECK(cpu->pc == UINT16_C(0x8a01));
    CHECK(cpu->instruction_cycles == UINT64_C(3));
    mipsnes2_cpu_set_irq(cpu, true);
    mipsnes2_cpu_pulse_abort(cpu);
    mipsnes2_cpu_pulse_nmi(cpu);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_CLOCK_STOPPED);
    CHECK(cpu->abort_pending);
    CHECK(cpu->pc == UINT16_C(0x8a01));
    CHECK(cpu->instruction_cycles == UINT64_C(3));
    bank00[0xfffc] = UINT8_C(0x34);
    bank00[0xfffd] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_reset(cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu->clock_stopped);
    CHECK(!cpu->waiting);
    CHECK(!cpu->abort_pending);
    CHECK(!cpu->nmi_pending);
    CHECK(cpu->pc == UINT16_C(0x1234));

    CHECK(initialize_trace_cpu(&native, &trace, UINT16_C(0x8b00)) == 0);
    native.emulation = false;
    native.pbr = UINT8_C(0x7e);
    native.s = UINT16_C(0x2000);
    native.p = MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_I;
    bank00[0xffea] = UINT8_C(0x78);
    bank00[0xffeb] = UINT8_C(0x56);
    mipsnes2_cpu_set_irq(&native, true);
    mipsnes2_cpu_pulse_nmi(&native);
    CHECK(mipsnes2_cpu_step(&native) == MIPSNES2_CPU_OK);
    CHECK(native.pc == UINT16_C(0x5678));
    CHECK(native.pbr == 0U);
    CHECK(native.s == UINT16_C(0x1ffc));
    CHECK(native.p == MIPSNES2_CPU_FLAG_I);
    CHECK(native.instruction_cycles == UINT64_C(8));
    CHECK(!native.nmi_pending);
    CHECK(trace.count == 6U);
    CHECK(trace.accesses[0].write &&
          trace.accesses[0].value == UINT8_C(0x7e));
    CHECK(trace.accesses[1].write &&
          trace.accesses[1].value == UINT8_C(0x8b));
    CHECK(trace.accesses[2].write &&
          trace.accesses[2].value == UINT8_C(0x00));
    CHECK(trace.accesses[3].write &&
          trace.accesses[3].value ==
              (MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_I));
    CHECK(trace.accesses[4].address == UINT32_C(0x00ffea));
    CHECK(trace.accesses[5].address == UINT32_C(0x00ffeb));

    cycles = native.instruction_cycles;
    native.p |= MIPSNES2_CPU_FLAG_I;
    native.pbr = UINT8_C(0x7e);
    native.pc = UINT16_C(0x8c00);
    bank7e[0x8c00] = UINT8_C(0xea);
    CHECK(mipsnes2_cpu_step(&native) == MIPSNES2_CPU_OK);
    CHECK(native.pc == UINT16_C(0x8c01));
    CHECK(native.instruction_cycles == cycles + UINT64_C(2));
    return 0;
}

static int test_abort_input_foundation(void)
{
    trace_bus_context trace;
    mipsnes2_cpu cpu;

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x9100)) == 0);
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x0100);
    cpu.p = MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
            MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
            MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_C;
    bank7e[0x9100] = UINT8_C(0xea);
    bank00[0xfff8] = UINT8_C(0x56);
    bank00[0xfff9] = UINT8_C(0x34);
    mipsnes2_cpu_pulse_abort(&cpu);
    CHECK(cpu.abort_pending);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu.abort_pending);
    CHECK(cpu.pc == UINT16_C(0x3456));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x01fd));
    CHECK(cpu.p == (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
                    MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
                    MIPSNES2_CPU_FLAG_I | MIPSNES2_CPU_FLAG_C));
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 5U);
    CHECK(trace.accesses[0].write &&
          trace.accesses[0].address == UINT32_C(0x000100) &&
          trace.accesses[0].value == UINT8_C(0x91));
    CHECK(trace.accesses[1].write &&
          trace.accesses[1].address == UINT32_C(0x0001ff) &&
          trace.accesses[1].value == UINT8_C(0x00));
    CHECK(trace.accesses[2].write &&
          trace.accesses[2].address == UINT32_C(0x0001fe) &&
          trace.accesses[2].value ==
              (MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_V |
               MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_D |
               MIPSNES2_CPU_FLAG_C));
    CHECK(!trace.accesses[3].write &&
          trace.accesses[3].address == UINT32_C(0x00fff8) &&
          trace.accesses[3].value == UINT8_C(0x56));
    CHECK(!trace.accesses[4].write &&
          trace.accesses[4].address == UINT32_C(0x00fff9) &&
          trace.accesses[4].value == UINT8_C(0x34));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x9200)) == 0);
    cpu.emulation = false;
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x2000);
    cpu.p = MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_D |
            MIPSNES2_CPU_FLAG_C;
    bank7e[0x9200] = UINT8_C(0xea);
    bank00[0xffe8] = UINT8_C(0x78);
    bank00[0xffe9] = UINT8_C(0x56);
    bank00[0xffea] = UINT8_C(0xbc);
    bank00[0xffeb] = UINT8_C(0x9a);
    mipsnes2_cpu_set_irq(&cpu, true);
    mipsnes2_cpu_pulse_nmi(&cpu);
    mipsnes2_cpu_pulse_abort(&cpu);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x1ffc));
    CHECK(cpu.p == (MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_I |
                    MIPSNES2_CPU_FLAG_C));
    CHECK(cpu.nmi_pending);
    CHECK(cpu.instruction_cycles == UINT64_C(8));
    CHECK(trace.count == 6U);
    CHECK(trace.accesses[0].write &&
          trace.accesses[0].address == UINT32_C(0x002000) &&
          trace.accesses[0].value == UINT8_C(0x7e));
    CHECK(trace.accesses[1].write &&
          trace.accesses[1].address == UINT32_C(0x001fff) &&
          trace.accesses[1].value == UINT8_C(0x92));
    CHECK(trace.accesses[2].write &&
          trace.accesses[2].address == UINT32_C(0x001ffe) &&
          trace.accesses[2].value == UINT8_C(0x00));
    CHECK(trace.accesses[3].write &&
          trace.accesses[3].address == UINT32_C(0x001ffd) &&
          trace.accesses[3].value ==
              (MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_D |
               MIPSNES2_CPU_FLAG_C));
    CHECK(!trace.accesses[4].write &&
          trace.accesses[4].address == UINT32_C(0x00ffe8));
    CHECK(!trace.accesses[5].write &&
          trace.accesses[5].address == UINT32_C(0x00ffe9));
    trace.count = 0U;
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu.nmi_pending);
    CHECK(cpu.pc == UINT16_C(0x9abc));
    CHECK(cpu.instruction_cycles == UINT64_C(16));
    CHECK(trace.count == 6U);
    CHECK(!trace.accesses[4].write &&
          trace.accesses[4].address == UINT32_C(0x00ffea));
    CHECK(!trace.accesses[5].write &&
          trace.accesses[5].address == UINT32_C(0x00ffeb));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x9300)) == 0);
    cpu.emulation = false;
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x2000);
    cpu.p = MIPSNES2_CPU_FLAG_I;
    bank7e[0x9300] = UINT8_C(0xcb);
    bank00[0xffea] = UINT8_C(0x34);
    bank00[0xffeb] = UINT8_C(0x12);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.waiting);
    CHECK(cpu.pc == UINT16_C(0x9301));
    trace.count = 0U;
    mipsnes2_cpu_pulse_abort(&cpu);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_WAITING);
    CHECK(cpu.waiting);
    CHECK(!cpu.abort_pending);
    CHECK(cpu.pc == UINT16_C(0x9300));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(trace.count == 0U);
    mipsnes2_cpu_pulse_nmi(&cpu);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(!cpu.waiting);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.s == UINT16_C(0x1ffc));
    CHECK(cpu.instruction_cycles == UINT64_C(11));
    CHECK(trace.count == 6U);
    CHECK(trace.accesses[0].write &&
          trace.accesses[0].value == UINT8_C(0x7e));
    CHECK(trace.accesses[1].write &&
          trace.accesses[1].value == UINT8_C(0x93));
    CHECK(trace.accesses[2].write &&
          trace.accesses[2].value == UINT8_C(0x00));

    CHECK(initialize_trace_cpu(&cpu, &trace, UINT16_C(0x9400)) == 0);
    cpu.pbr = UINT8_C(0x7e);
    cpu.bus.write8 = NULL;
    mipsnes2_cpu_pulse_abort(&cpu);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_INVALID_BUS);
    CHECK(cpu.stopped);
    CHECK(!cpu.abort_pending);
    CHECK(cpu.pc == UINT16_C(0x9400));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 0U);
    mipsnes2_cpu_pulse_abort(NULL);
    return 0;
}

int main(void)
{
    CHECK(test_bus() == 0);
    CHECK(test_reset() == 0);
    CHECK(test_emulation_mode_instructions() == 0);
    CHECK(test_width_and_mode_instructions() == 0);
    CHECK(test_absolute_loads() == 0);
    CHECK(test_absolute_indexed_loads_and_stores() == 0);
    CHECK(test_absolute_long_loads_and_stores() == 0);
    CHECK(test_direct_page_emulation_loads_and_stores() == 0);
    CHECK(test_direct_page_native_width_and_wrap() == 0);
    CHECK(test_direct_page_unaligned_penalty() == 0);
    CHECK(test_direct_indirect_emulation_pointer_rules() == 0);
    CHECK(test_direct_indirect_native_width_and_wrap() == 0);
    CHECK(test_direct_indirect_cycle_penalties() == 0);
    CHECK(test_register_transfers_emulation_width() == 0);
    CHECK(test_register_transfers_native_width() == 0);
    CHECK(test_register_transfers_mixed_widths() == 0);
    CHECK(test_special_transfers_emulation_mode() == 0);
    CHECK(test_special_transfers_native_mode() == 0);
    CHECK(test_stack_transfers_native_8_bit_index() == 0);
    CHECK(test_xba_width_independence() == 0);
    CHECK(test_status_control_instructions() == 0);
    CHECK(test_accumulator_increment_decrement() == 0);
    CHECK(test_index_increment_decrement() == 0);
    CHECK(test_logic_foundation() == 0);
    CHECK(test_accumulator_extended_addressing() == 0);
    CHECK(test_stack_relative_accumulator_alu() == 0);
    CHECK(test_stack_relative_accumulator_load_store() == 0);
    CHECK(test_binary_arithmetic_foundation() == 0);
    CHECK(test_decimal_arithmetic() == 0);
    CHECK(test_shift_rotate_family() == 0);
    CHECK(test_memory_increment_decrement() == 0);
    CHECK(test_memory_bit_and_store_zero() == 0);
    CHECK(test_stack_relative_binary_arithmetic() == 0);
    CHECK(test_direct_indirect_binary_arithmetic() == 0);
    CHECK(test_long_binary_arithmetic() == 0);
    CHECK(test_compare_foundation() == 0);
    CHECK(test_bit_foundation() == 0);
    CHECK(test_control_flow_and_faults() == 0);
    CHECK(test_conditional_branches() == 0);
    CHECK(test_branch_timing_and_program_bank() == 0);
    CHECK(test_branch_relative_long() == 0);
    CHECK(test_rep_in_emulation() == 0);
    CHECK(test_stack_push_pull_emulation() == 0);
    CHECK(test_stack_native_widths() == 0);
    CHECK(test_effective_address_pushes() == 0);
    CHECK(test_subroutine_control_instructions() == 0);
    CHECK(test_stack_bus_access_order_and_invalid_bus() == 0);
    CHECK(test_software_interrupts_and_rti() == 0);
    CHECK(test_wdm_and_block_moves() == 0);
    CHECK(test_wait_stop_and_interrupt_inputs() == 0);
    CHECK(test_abort_input_foundation() == 0);
    (void)puts("PASS: logical bus and all 256 65C816 opcode values");
    return 0;
}
