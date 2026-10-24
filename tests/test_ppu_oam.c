/* Deterministic OAM port fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/oam_ports.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t oam[MIPSNES2_PPU_OAM_SIZE];

int main(void)
{
    mipsnes2_ppu_oam_ports ports;
    uint8_t value = 0U;
    unsigned i;

    CHECK(!mipsnes2_ppu_oam_ports_init(&ports, oam, sizeof(oam) - 1U));
    CHECK(mipsnes2_ppu_oam_ports_init(&ports, oam, sizeof(oam)));
    (void)memset(oam, 0U, sizeof(oam));
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2102, 0x7f));
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2103, 0x80));
    CHECK(ports.priority_rotation && ports.byte_address == 0xfeU);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0x12));
    CHECK(oam[0xfe] == 0U && ports.byte_address == 0xffU);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0x34));
    CHECK(oam[0xfe] == 0x12U && oam[0xff] == 0x34U);

    /* Either address register reloads from both saved address bits. */
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2102, 0xff));
    CHECK(ports.byte_address == 0x1feU);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0x56));
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0x78));
    CHECK(oam[0x1fe] == 0x56U && oam[0x1ff] == 0x78U);
    CHECK(ports.byte_address == 0x200U);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0x9a));
    CHECK(oam[512] == 0x9aU);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2103, 0x01));
    CHECK(!ports.priority_rotation && ports.byte_address == 0x3feU);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0xbc));
    CHECK(oam[542] == 0xbcU);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0xde));
    CHECK(oam[543] == 0xdeU && ports.byte_address == 0U);

    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2102, 0x10));
    CHECK(ports.byte_address == 0x220U);
    CHECK(mipsnes2_ppu_oam_ports_read(&ports, 0x2138, &value));
    CHECK(value == 0x9aU && ports.byte_address == 0x221U);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2103, 0U));
    CHECK(ports.byte_address == 0x20U);

    /* A read at the odd low-table byte skips commit; the later odd write
     * commits the old latch alongside the new high byte. */
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0xa1));
    CHECK(mipsnes2_ppu_oam_ports_read(&ports, 0x2138, &value));
    CHECK(value == 0U && oam[0x20] == 0U);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0xa2));
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, 0xa3));
    CHECK(oam[0x22] == 0xa2U && oam[0x23] == 0xa3U);
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2102, 0x10));
    CHECK(mipsnes2_ppu_oam_ports_read(&ports, 0x2138, &value));
    CHECK(value == 0U);

    /* Every high-table word address aliases one of 32 physical bytes. */
    for (i = 0U; i < 16U; ++i) {
        CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2102, (uint8_t)i));
        CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2103, 1U));
        CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, (uint8_t)i));
        CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2104, (uint8_t)(i + 0x80U)));
    }
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2102, 0x10));
    CHECK(mipsnes2_ppu_oam_ports_read(&ports, 0x2138, &value));
    CHECK(value == 0U);
    CHECK(mipsnes2_ppu_oam_ports_read(&ports, 0x2138, &value));
    CHECK(value == 0x80U);
    CHECK(!mipsnes2_ppu_oam_ports_write(&ports, 0x2101, 0U));
    CHECK(!mipsnes2_ppu_oam_ports_read(&ports, 0x2139, &value));
    CHECK(!mipsnes2_ppu_oam_ports_read(&ports, 0x2138, NULL));
    CHECK(mipsnes2_ppu_oam_ports_init(&ports, oam, sizeof(oam)));
    CHECK(ports.byte_address == 0U && oam[0x1ff] == 0x78U);
    (void)puts("PPU OAM ports: OK");
    return 0;
}
