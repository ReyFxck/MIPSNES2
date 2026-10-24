/* Deterministic PPU VRAM write-port fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/vram_ports.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t vram[MIPSNES2_PPU_VRAM_SIZE];

static int set_address(mipsnes2_ppu_vram_ports *ports, uint16_t address)
{
    CHECK(mipsnes2_ppu_vram_ports_write(ports, 0x2116, (uint8_t)address));
    CHECK(mipsnes2_ppu_vram_ports_write(ports, 0x2117,
                                       (uint8_t)(address >> 8)));
    return 0;
}

int main(void)
{
    mipsnes2_ppu_vram_ports ports;
    unsigned mode;

    CHECK(!mipsnes2_ppu_vram_ports_init(&ports, vram, sizeof(vram) - 1U));
    CHECK(mipsnes2_ppu_vram_ports_init(&ports, vram, sizeof(vram)));
    (void)memset(vram, 0U, sizeof(vram));
    CHECK(set_address(&ports, UINT16_C(0x1234)) == 0);
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2118, UINT8_C(0xa1)));
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2119, UINT8_C(0xb2)));
    CHECK(vram[0x2468] == UINT8_C(0xa1));
    CHECK(vram[0x246b] == UINT8_C(0xb2));
    CHECK(ports.word_address == UINT16_C(0x1235));

    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2115, UINT8_C(0x80)));
    CHECK(set_address(&ports, UINT16_C(0x7fff)) == 0);
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2118, UINT8_C(0x34)));
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2119, UINT8_C(0x56)));
    CHECK(ports.word_address == UINT16_C(0x8000));
    CHECK(vram[0xfffe] == UINT8_C(0x34));
    CHECK(vram[0xffff] == UINT8_C(0x56));
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2118, UINT8_C(0x78)));
    CHECK(vram[0] == UINT8_C(0x78));

    for (mode = 1U; mode <= 3U; ++mode) {
        unsigned group = 1U << (mode + 4U);

        CHECK(mipsnes2_ppu_vram_ports_write(
            &ports, 0x2115, (uint8_t)(UINT8_C(0x80) | (mode << 2))));
        CHECK(set_address(&ports, 1U) == 0);
        CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2119,
                                            (uint8_t)(0x90U + mode)));
        CHECK(vram[8U * 2U + 1U] == (uint8_t)(0x90U + mode));
        CHECK(set_address(&ports, (uint16_t)group) == 0);
        CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2118,
                                            (uint8_t)(0xa0U + mode)));
        CHECK(vram[2U] == (uint8_t)(0xa0U + mode));
    }

    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2115, UINT8_C(0x81)));
    CHECK(set_address(&ports, 0U) == 0);
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2119, 0U));
    CHECK(ports.word_address == 32U);
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2115, UINT8_C(0x82)));
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2119, 0U));
    CHECK(ports.word_address == 160U);
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2115, UINT8_C(0x83)));
    CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2119, 0U));
    CHECK(ports.word_address == 288U);
    CHECK(!mipsnes2_ppu_vram_ports_write(&ports, 0x2100, 0U));
    CHECK(mipsnes2_ppu_vram_ports_init(&ports, vram, sizeof(vram)));
    CHECK(ports.word_address == 0U && ports.vmain == 0U);
    CHECK(vram[0xfffe] == UINT8_C(0x34));
    {
        uint8_t value = UINT8_C(0x91);

        vram[0x20] = UINT8_C(0x12);
        vram[0x21] = UINT8_C(0x34);
        vram[0x22] = UINT8_C(0x56);
        vram[0x23] = UINT8_C(0x78);
        CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2115, UINT8_C(0x80)));
        CHECK(set_address(&ports, UINT16_C(0x0010)) == 0);
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x2139, &value));
        CHECK(value == 0U && ports.word_address == UINT16_C(0x0010));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x213a, &value));
        CHECK(value == 0U && ports.word_address == UINT16_C(0x0011));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x2139, &value));
        CHECK(value == UINT8_C(0x12));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x213a, &value));
        CHECK(value == UINT8_C(0x34) && ports.word_address == UINT16_C(0x0012));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x2139, &value));
        CHECK(value == UINT8_C(0x56));
        CHECK(set_address(&ports, UINT16_C(0x0020)) == 0);
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x2139, &value));
        CHECK(value == UINT8_C(0x56)); /* VMADD leaves the latch intact. */
        CHECK(!mipsnes2_ppu_vram_ports_read(&ports, 0x2138, &value));
        CHECK(!mipsnes2_ppu_vram_ports_read(&ports, 0x213a, NULL));

        CHECK(mipsnes2_ppu_vram_ports_init(&ports, vram, sizeof(vram)));
        CHECK(set_address(&ports, UINT16_C(0x0010)) == 0);
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x2139, &value));
        CHECK(value == 0U && ports.word_address == UINT16_C(0x0011));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x213a, &value));
        CHECK(value == UINT8_C(0x34));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x2139, &value));
        CHECK(value == UINT8_C(0x12) && ports.word_address == UINT16_C(0x0012));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x213a, &value));
        CHECK(value == UINT8_C(0x78));

        CHECK(mipsnes2_ppu_vram_ports_init(&ports, vram, sizeof(vram)));
        CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2115, UINT8_C(0x84)));
        vram[17] = UINT8_C(0xab);
        vram[16] = UINT8_C(0xcd);
        CHECK(set_address(&ports, 1U) == 0);
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x213a, &value));
        CHECK(value == 0U && ports.word_address == 2U);
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x2139, &value));
        CHECK(value == UINT8_C(0xcd));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x213a, &value));
        CHECK(value == UINT8_C(0xab));

        CHECK(mipsnes2_ppu_vram_ports_init(&ports, vram, sizeof(vram)));
        CHECK(mipsnes2_ppu_vram_ports_write(&ports, 0x2115, UINT8_C(0x80)));
        CHECK(set_address(&ports, UINT16_C(0xffff)) == 0);
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x213a, &value));
        CHECK(ports.word_address == 0U && ports.read_buffer == UINT16_C(0x5634));
        CHECK(mipsnes2_ppu_vram_ports_read(&ports, 0x2139, &value));
        CHECK(value == UINT8_C(0x34));
    }
    (void)puts("PPU VRAM ports: OK");
    return 0;
}
