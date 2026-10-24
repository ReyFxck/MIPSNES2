/* Deterministic OAM port fixture. See LICENSE for GPL-3.0 terms. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/ppu/oam_ports.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

static uint8_t oam[MIPSNES2_PPU_OAM_SIZE];

static int test_sprite_decode(void)
{
    static const uint8_t sizes[6][2] = {
        { 8U, 16U }, { 8U, 32U }, { 8U, 64U },
        { 16U, 32U }, { 16U, 64U }, { 32U, 64U }
    };
    mipsnes2_ppu_oam_ports ports;
    mipsnes2_ppu_oam_sprite sprite;
    uint8_t pixels = 0U;
    unsigned mode;

    (void)memset(oam, 0U, sizeof(oam));
    CHECK(mipsnes2_ppu_oam_ports_init(&ports, oam, sizeof(oam)));
    oam[0] = 0x34U;
    oam[1] = 0x56U;
    oam[2] = 0x78U;
    oam[3] = 0xfdU;
    oam[4] = 0xffU;
    oam[12] = 0x81U;
    oam[16] = 0x22U;
    oam[508] = 0xffU;
    oam[509] = 0xfeU;
    oam[510] = 0xffU;
    oam[511] = 0x41U;
    oam[512] = 0xc9U; /* OBJ 0: X; OBJ 1: large; OBJ 3: both. */
    oam[513] = 0x02U; /* OBJ 4: large. */
    oam[543] = 0xc0U; /* OBJ 127: both. */
    ports.byte_address = 0x2ffU;
    ports.low_latch = 0x9aU;
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2101, 0xa5U));
    CHECK(ports.obj_select == 0xa5U && ports.byte_address == 0x2ffU);

    CHECK(mipsnes2_ppu_oam_decode_sprite(&ports, 0U, &sprite));
    CHECK(sprite.x == 0x134U && sprite.y == 0x56U);
    CHECK(sprite.character == 0x178U && sprite.palette == 6U);
    CHECK(sprite.priority == 3U && sprite.flip_x && sprite.flip_y);
    CHECK(!sprite.large);
    CHECK(mipsnes2_ppu_oam_decode_sprite(&ports, 1U, &sprite));
    CHECK(sprite.x == 0xffU && sprite.large && sprite.character == 0U);
    CHECK(mipsnes2_ppu_oam_decode_sprite(&ports, 2U, &sprite));
    CHECK(sprite.x == 0U && !sprite.large);
    CHECK(mipsnes2_ppu_oam_decode_sprite(&ports, 3U, &sprite));
    CHECK(sprite.x == 0x181U && sprite.large);
    CHECK(mipsnes2_ppu_oam_decode_sprite(&ports, 4U, &sprite));
    CHECK(sprite.x == 0x22U && sprite.large);
    CHECK(mipsnes2_ppu_oam_decode_sprite(&ports, 127U, &sprite));
    CHECK(sprite.x == 0x1ffU && sprite.y == 0xfeU);
    CHECK(sprite.character == 0x1ffU && sprite.palette == 0U);
    CHECK(sprite.priority == 0U && sprite.flip_x && !sprite.flip_y);
    CHECK(sprite.large && ports.byte_address == 0x2ffU);
    CHECK(ports.low_latch == 0x9aU && ports.obj_select == 0xa5U);
    CHECK(!mipsnes2_ppu_oam_decode_sprite(&ports, 128U, &sprite));
    CHECK(!mipsnes2_ppu_oam_decode_sprite(&ports, 0U, NULL));
    CHECK(!mipsnes2_ppu_oam_decode_sprite(NULL, 0U, &sprite));

    for (mode = 0U; mode < 6U; ++mode) {
        CHECK(mipsnes2_ppu_oam_sprite_size((uint8_t)(mode << 5),
                                          false, &pixels));
        CHECK(pixels == sizes[mode][0]);
        CHECK(mipsnes2_ppu_oam_sprite_size((uint8_t)((mode << 5) | 0x1fU),
                                          true, &pixels));
        CHECK(pixels == sizes[mode][1]);
    }
    pixels = 0x5aU;
    CHECK(!mipsnes2_ppu_oam_sprite_size(0xc0U, false, &pixels));
    CHECK(!mipsnes2_ppu_oam_sprite_size(0xe0U, true, &pixels));
    CHECK(pixels == 0x5aU);
    CHECK(!mipsnes2_ppu_oam_sprite_size(0U, false, NULL));
    return 0;
}

static int test_sprite_tile_address(void)
{
    mipsnes2_ppu_oam_sprite sprite;
    uint16_t address = 0U;

    (void)memset(&sprite, 0, sizeof(sprite));
    sprite.character = 0x1feU;
    sprite.large = true;
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 0U, 0U,
                                           &address));
    CHECK(address == 0x1fe0U);
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 1U, 0U,
                                           &address));
    CHECK(address == 0x1ff0U);
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 2U, 0U,
                                           &address));
    CHECK(address == 0x1f00U);
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 0U, 1U,
                                           &address));
    CHECK(address == 0x10e0U);
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 3U, 3U,
                                           &address));
    CHECK(address == 0x1210U);
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 7U, 7U,
                                           &address));
    CHECK(address == 0x1650U);

    /* OBJSEL base and second-table selection both wrap at 32K words. */
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa3U, &sprite, 0U, 0U,
                                           &address));
    CHECK(address == 0x7fe0U);
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xbfU, &sprite, 0U, 0U,
                                           &address));
    CHECK(address == 0x2fe0U);
    sprite.character = 0xfeU;
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xbfU, &sprite, 0U, 0U,
                                           &address));
    CHECK(address == 0x6fe0U);

    sprite.large = false; /* mode 5 small is 32x32. */
    CHECK(mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 3U, 3U,
                                           &address));
    address = 0x55aaU;
    CHECK(!mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 4U, 0U,
                                            &address));
    CHECK(!mipsnes2_ppu_oam_sprite_tile_word(0xc0U, &sprite, 0U, 0U,
                                            &address));
    sprite.character = 0x200U;
    CHECK(!mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 0U, 0U,
                                            &address));
    CHECK(!mipsnes2_ppu_oam_sprite_tile_word(0xa0U, NULL, 0U, 0U,
                                            &address));
    CHECK(!mipsnes2_ppu_oam_sprite_tile_word(0xa0U, &sprite, 0U, 0U,
                                            NULL));
    CHECK(address == 0x55aaU);
    return 0;
}

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
    CHECK(!mipsnes2_ppu_oam_ports_write(&ports, 0x2105, 0U));
    CHECK(!mipsnes2_ppu_oam_ports_read(&ports, 0x2139, &value));
    CHECK(!mipsnes2_ppu_oam_ports_read(&ports, 0x2138, NULL));
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2102, 0x7f));
    CHECK(mipsnes2_ppu_oam_ports_write(&ports, 0x2103, 0U));
    CHECK(mipsnes2_ppu_oam_ports_read(&ports, 0x2138, &value));
    CHECK(ports.byte_address == 0xffU);
    CHECK(mipsnes2_ppu_oam_ports_vblank_reload(&ports, true));
    CHECK(ports.byte_address == 0xffU);
    CHECK(mipsnes2_ppu_oam_ports_vblank_reload(&ports, false));
    CHECK(ports.byte_address == 0xfeU && oam[0xfe] == 0x12U);
    CHECK(!mipsnes2_ppu_oam_ports_vblank_reload(NULL, false));
    CHECK(mipsnes2_ppu_oam_ports_init(&ports, oam, sizeof(oam)));
    CHECK(ports.byte_address == 0U && oam[0x1ff] == 0x78U);
    CHECK(test_sprite_decode() == 0);
    CHECK(test_sprite_tile_address() == 0);
    (void)puts("PPU OAM ports: OK");
    return 0;
}
