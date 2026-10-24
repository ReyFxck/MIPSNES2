/* Deterministic PPU CGRAM port fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/cgram_ports.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t cgram[MIPSNES2_PPU_CGRAM_SIZE];

int main(void)
{
    mipsnes2_ppu_cgram_ports ports;
    uint8_t value = 0U;

    CHECK(!mipsnes2_ppu_cgram_ports_init(&ports, cgram,
                                        sizeof(cgram) - 1U));
    CHECK(mipsnes2_ppu_cgram_ports_init(&ports, cgram, sizeof(cgram)));
    (void)memset(cgram, 0U, sizeof(cgram));
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2121, 0xff));
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2122, 0x34));
    CHECK(cgram[0x1fe] == 0U && ports.address == 0xffU);
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2122, 0xd2));
    CHECK(cgram[0x1fe] == 0x34U && cgram[0x1ff] == 0x52U);
    CHECK(ports.address == 0U && !ports.high_phase);
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2122, 0x78));
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2122, 0x56));
    CHECK(cgram[0] == 0x78U && cgram[1] == 0x56U);

    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2122, 0xaa));
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2121, 0xff));
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2122, 0x91));
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2122, 0x8f));
    CHECK(cgram[0x1fe] == 0x91U && cgram[0x1ff] == 0x0fU);
    CHECK(mipsnes2_ppu_cgram_ports_write(&ports, 0x2121, 0xff));
    CHECK(mipsnes2_ppu_cgram_ports_read(&ports, 0x213b, 0x80, &value));
    CHECK(value == 0x91U && ports.address == 0xffU);
    CHECK(mipsnes2_ppu_cgram_ports_read(&ports, 0x213b, 0x80, &value));
    CHECK(value == 0x8fU && ports.address == 0U);
    CHECK(mipsnes2_ppu_cgram_ports_read(&ports, 0x213b, 0x80, &value));
    CHECK(value == 0x78U);
    CHECK(mipsnes2_ppu_cgram_ports_read(&ports, 0x213b, 0U, &value));
    CHECK(value == 0x56U && ports.address == 1U);
    CHECK(!mipsnes2_ppu_cgram_ports_read(&ports, 0x213a, 0U, &value));
    CHECK(!mipsnes2_ppu_cgram_ports_read(&ports, 0x213b, 0U, NULL));
    CHECK(!mipsnes2_ppu_cgram_ports_write(&ports, 0x2120, 0U));

    CHECK(mipsnes2_ppu_cgram_ports_init(&ports, cgram, sizeof(cgram)));
    CHECK(ports.address == 0U && !ports.high_phase);
    CHECK(cgram[0x1fe] == 0x91U && cgram[0x1ff] == 0x0fU);
    (void)puts("PPU CGRAM ports: OK");
    return 0;
}
