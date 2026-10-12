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

    cpu->pc = UINT16_C(0x8100);
    cpu->instruction_cycles = 0U;
    bank7e[0x8100] = UINT8_C(0x02);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_UNIMPLEMENTED_OPCODE);
    CHECK(cpu->stopped);
    CHECK(cpu->stop_reason == MIPSNES2_CPU_UNIMPLEMENTED_OPCODE);
    CHECK(cpu->last_opcode == UINT8_C(0x02));
    CHECK(cpu->last_opcode_address == UINT32_C(0x7e8100));
    CHECK(cpu->pc == UINT16_C(0x8101));
    CHECK(cpu->instruction_cycles == 0U);
    CHECK(mipsnes2_cpu_step(cpu) == MIPSNES2_CPU_UNIMPLEMENTED_OPCODE);
    CHECK(cpu->pc == UINT16_C(0x8101));
    CHECK(cpu->instruction_cycles == 0U);
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
    CHECK(test_compare_foundation() == 0);
    CHECK(test_bit_foundation() == 0);
    CHECK(test_control_flow_and_faults() == 0);
    CHECK(test_conditional_branches() == 0);
    CHECK(test_branch_timing_and_program_bank() == 0);
    CHECK(test_branch_relative_long() == 0);
    CHECK(test_rep_in_emulation() == 0);
    (void)puts("PASS: logical bus and 65C816 logic/compare subset");
    return 0;
}
