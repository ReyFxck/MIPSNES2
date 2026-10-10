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
    CHECK(bank7f[0x0000] == UINT8_C(0x12));
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
    CHECK(test_control_flow_and_faults() == 0);
    CHECK(test_rep_in_emulation() == 0);
    (void)puts("PASS: logical bus and 65C816 foundation");
    return 0;
}
