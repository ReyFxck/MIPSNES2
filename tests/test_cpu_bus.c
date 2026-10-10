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
    CHECK(test_control_flow_and_faults() == 0);
    CHECK(test_rep_in_emulation() == 0);
    (void)puts("PASS: logical bus and 65C816 direct-indirect load/store subset");
    return 0;
}
