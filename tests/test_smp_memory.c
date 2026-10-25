/* Synthetic S-SMP memory-map fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/apu/spc700/memory.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t ram[MIPSNES2_SMP_RAM_SIZE];
static uint8_t synthetic_ipl[MIPSNES2_SMP_IPL_SIZE];

int main(void)
{
    mipsnes2_apu_io_ports ports;
    mipsnes2_smp_memory memory;
    uint8_t value = UINT8_C(0xee);
    unsigned i;

    (void)memset(ram, UINT8_C(0xa5), sizeof(ram));
    for (i = 0U; i < MIPSNES2_SMP_IPL_SIZE; ++i) {
        synthetic_ipl[i] = (uint8_t)(i ^ UINT8_C(0x5a));
    }
    mipsnes2_apu_io_ports_init(&ports);
    CHECK(!mipsnes2_smp_memory_init(NULL, ram, sizeof(ram), &ports,
                                    synthetic_ipl, sizeof(synthetic_ipl)));
    CHECK(!mipsnes2_smp_memory_init(&memory, NULL, sizeof(ram), &ports,
                                    synthetic_ipl, sizeof(synthetic_ipl)));
    CHECK(!mipsnes2_smp_memory_init(&memory, ram, sizeof(ram) - 1U, &ports,
                                    synthetic_ipl, sizeof(synthetic_ipl)));
    CHECK(!mipsnes2_smp_memory_init(&memory, ram, sizeof(ram), NULL,
                                    synthetic_ipl, sizeof(synthetic_ipl)));
    CHECK(!mipsnes2_smp_memory_init(&memory, ram, sizeof(ram), &ports,
                                    synthetic_ipl, sizeof(synthetic_ipl) - 1U));
    CHECK(!mipsnes2_smp_memory_init(&memory, ram, sizeof(ram), &ports,
                                    NULL, sizeof(synthetic_ipl)));
    CHECK(mipsnes2_smp_memory_init(&memory, ram, sizeof(ram), &ports,
                                   synthetic_ipl, sizeof(synthetic_ipl)));
    CHECK(memory.ipl_enabled && ram[0] == UINT8_C(0xa5));

    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x0000), 0U));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00ef), 1U));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x0100), 2U));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0xffbf), 3U));
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0x0000), &value));
    CHECK(value == 0U);
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0x00ef), &value));
    CHECK(value == 1U);
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0x0100), &value));
    CHECK(value == 2U);
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0xffbf), &value));
    CHECK(value == 3U);

    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0xffc0), &value));
    CHECK(value == synthetic_ipl[0]);
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0xffc0),
                                    UINT8_C(0x39)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0xffff),
                                    UINT8_C(0x7b)));
    CHECK(ram[0xffc0] == UINT8_C(0x39) && ram[0xffff] == UINT8_C(0x7b));
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0xffff), &value));
    CHECK(value == synthetic_ipl[63]);
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1), 0U));
    CHECK(!memory.ipl_enabled);
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0xffc0), &value));
    CHECK(value == UINT8_C(0x39));
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0xffff), &value));
    CHECK(value == UINT8_C(0x7b));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x80)));
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0xffc0), &value));
    CHECK(value == synthetic_ipl[0]);

    for (i = 0U; i < MIPSNES2_APU_IO_PORT_COUNT; ++i) {
        CHECK(mipsnes2_apu_io_ports_cpu_write(
            &ports, (uint16_t)(UINT16_C(0x2140) + i),
            (uint8_t)(UINT8_C(0x10) + i)));
        CHECK(mipsnes2_smp_memory_read(
            &memory, (uint16_t)(UINT16_C(0x00f4) + i), &value));
        CHECK(value == (uint8_t)(UINT8_C(0x10) + i));
        CHECK(mipsnes2_smp_memory_write(
            &memory, (uint16_t)(UINT16_C(0x00f4) + i),
            (uint8_t)(UINT8_C(0x80) + i)));
        CHECK(ram[0xf4U + i] == (uint8_t)(UINT8_C(0x80) + i));
        CHECK(mipsnes2_apu_io_ports_cpu_read(
            &ports, (uint16_t)(UINT16_C(0x2140) + i), &value));
        CHECK(value == (uint8_t)(UINT8_C(0x80) + i));
    }
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0x90)));
    CHECK(ports.cpu_to_apu[0] == 0U && ports.cpu_to_apu[1] == 0U);
    CHECK(ports.cpu_to_apu[2] == UINT8_C(0x12) &&
          ports.cpu_to_apu[3] == UINT8_C(0x13));
    CHECK(ports.apu_to_cpu[0] == UINT8_C(0x80));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                    UINT8_C(0xa0)));
    CHECK(ports.cpu_to_apu[2] == 0U && ports.cpu_to_apu[3] == 0U);
    CHECK(ports.apu_to_cpu[3] == UINT8_C(0x83));

    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f8),
                                    UINT8_C(0x24)));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f9),
                                    UINT8_C(0x68)));
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0x00f8), &value));
    CHECK(value == UINT8_C(0x24));
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0x00f9), &value));
    CHECK(value == UINT8_C(0x68));

    for (i = 0xf0U; i <= 0xffU; ++i) {
        if (i == 0xf1U || (i >= 0xf4U && i <= 0xf9U)) {
            continue;
        }
        value = UINT8_C(0xee);
        CHECK(!mipsnes2_smp_memory_read(&memory, (uint16_t)i, &value));
        CHECK(value == UINT8_C(0xee));
        CHECK(!mipsnes2_smp_memory_write(&memory, (uint16_t)i,
                                         UINT8_C(0x77)));
        CHECK(ram[i] == UINT8_C(0xa5));
    }
    CHECK(!mipsnes2_smp_memory_read(&memory, UINT16_C(0x00f1), &value));
    CHECK(!mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1),
                                     UINT8_C(0x11)));
    CHECK(memory.ipl_enabled && ram[0xf1] == UINT8_C(0xa0));
    CHECK(!mipsnes2_smp_memory_read(&memory, 0U, NULL));
    CHECK(!mipsnes2_smp_memory_read(NULL, 0U, &value));
    CHECK(!mipsnes2_smp_memory_write(NULL, 0U, 0U));

    CHECK(mipsnes2_smp_memory_reset(&memory));
    CHECK(memory.ipl_enabled && ram[0xffc0] == UINT8_C(0x39));
    CHECK(mipsnes2_smp_memory_init(&memory, ram, sizeof(ram), &ports,
                                   NULL, 0U));
    value = UINT8_C(0xee);
    CHECK(!mipsnes2_smp_memory_read(&memory, UINT16_C(0xffc0), &value));
    CHECK(value == UINT8_C(0xee));
    CHECK(mipsnes2_smp_memory_write(&memory, UINT16_C(0x00f1), 0U));
    CHECK(mipsnes2_smp_memory_read(&memory, UINT16_C(0xffc0), &value));
    CHECK(value == UINT8_C(0x39));

    (void)puts("S-SMP memory map: OK");
    return 0;
}
