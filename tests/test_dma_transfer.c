/* Synthetic DMA address/mode fixture. See LICENSE for GPL-3.0. */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/dma/transfer.h"

#define CHECK(value) do { if (!(value)) { \
    (void)fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
    return 1; } } while (0)

typedef struct {
    mipsnes2_address a[16];
    mipsnes2_address b[16];
    bool reverse[16];
    uint32_t count;
    uint8_t source[16];
    uint8_t destination[16];
} transfer_capture;

static void capture_byte(void *context, mipsnes2_address a,
                         mipsnes2_address b, bool reverse)
{
    transfer_capture *capture = context;
    uint32_t index = capture->count++;

    if (index < 16U) {
        capture->a[index] = a;
        capture->b[index] = b;
        capture->reverse[index] = reverse;
        if (reverse) {
            capture->source[index] = capture->destination[index];
        } else {
            capture->destination[index] = capture->source[index];
        }
    }
}

static int check_bounded_transfers(void)
{
    mipsnes2_dma_registers registers = {0};
    mipsnes2_dma_cursor cursor = {0};
    transfer_capture capture = {0};
    uint32_t done = UINT32_MAX;
    unsigned i;

    mipsnes2_dma_registers_init(&registers);
    /* Channel 0 crosses $FFFF without changing the A bank. Channel 2
     * follows it, and BBAD+mode-offset wraps within the B-bus low byte. */
    registers.channel[0][0] = 4U;
    registers.channel[0][1] = UINT8_C(0xfe);
    registers.channel[0][2] = UINT8_C(0xfe);
    registers.channel[0][3] = UINT8_C(0xff);
    registers.channel[0][4] = UINT8_C(0x7e);
    registers.channel[0][5] = 4U;
    registers.channel[0][6] = 0U;
    registers.channel[2][0] = UINT8_C(0x91); /* reverse, decrement, mode 1 */
    registers.channel[2][1] = UINT8_C(0x80);
    registers.channel[2][2] = 0U;
    registers.channel[2][3] = 0U;
    registers.channel[2][4] = UINT8_C(0x12);
    registers.channel[2][5] = 2U;
    registers.channel[2][6] = 0U;
    for (i = 0U; i < 16U; ++i) {
        capture.source[i] = (uint8_t)(i + 1U);
        capture.destination[i] = (uint8_t)(0xa0U + i);
    }
    CHECK(mipsnes2_dma_cursor_begin(&cursor, &registers, 5U));
    CHECK(mipsnes2_dma_cursor_run(&cursor, 0U, capture_byte, &capture,
                                  &done) == MIPSNES2_DMA_RUN_PAUSED);
    CHECK(done == 0U && capture.count == 0U);
    CHECK(mipsnes2_dma_cursor_run(&cursor, 3U, capture_byte, &capture,
                                  &done) == MIPSNES2_DMA_RUN_PAUSED);
    CHECK(done == 3U && capture.count == 3U);
    CHECK(capture.a[0] == UINT32_C(0x7efffe));
    CHECK(capture.a[1] == UINT32_C(0x7effff));
    CHECK(capture.a[2] == UINT32_C(0x7e0000));
    CHECK(capture.b[0] == UINT32_C(0x21fe));
    CHECK(capture.b[1] == UINT32_C(0x21ff));
    CHECK(capture.b[2] == UINT32_C(0x2100));
    CHECK(capture.destination[0] == 1U && capture.destination[2] == 3U);
    CHECK(registers.channel[0][2] == 1U && registers.channel[0][3] == 0U);
    CHECK(registers.channel[0][5] == 1U);
    CHECK(registers.channel[2][2] == 0U && registers.channel[2][5] == 2U);
    CHECK(mipsnes2_dma_cursor_run(&cursor, 8U, capture_byte, &capture,
                                  &done) == MIPSNES2_DMA_RUN_COMPLETE);
    CHECK(done == 3U && capture.count == 6U);
    CHECK(capture.a[3] == UINT32_C(0x7e0001));
    CHECK(capture.b[3] == UINT32_C(0x2101));
    CHECK(capture.a[4] == UINT32_C(0x120000));
    CHECK(capture.a[5] == UINT32_C(0x12ffff));
    CHECK(capture.b[4] == UINT32_C(0x2180));
    CHECK(capture.b[5] == UINT32_C(0x2181));
    CHECK(capture.reverse[4] && capture.reverse[5]);
    CHECK(capture.source[4] == UINT8_C(0xa4));
    CHECK(registers.channel[0][5] == 0U && registers.channel[0][6] == 0U);
    CHECK(registers.channel[2][2] == UINT8_C(0xfe));
    CHECK(registers.channel[2][3] == UINT8_C(0xff));
    CHECK(registers.channel[2][5] == 0U);
    CHECK(mipsnes2_dma_cursor_run(&cursor, 8U, capture_byte, &capture,
                                  &done) == MIPSNES2_DMA_RUN_COMPLETE);
    CHECK(done == 0U && capture.count == 6U);
    return 0;
}

static int check_zero_count_and_modes(void)
{
    mipsnes2_dma_registers registers = {0};
    mipsnes2_dma_cursor cursor = {0};
    transfer_capture capture = {0};
    uint32_t done;
    unsigned mode;

    mipsnes2_dma_registers_init(&registers);
    for (mode = 0U; mode < 8U; ++mode) {
        (void)memset(&capture, 0, sizeof(capture));
        registers.channel[7][0] = (uint8_t)(mode | 8U); /* fixed A */
        registers.channel[7][1] = UINT8_C(0x30);
        registers.channel[7][2] = UINT8_C(0x34);
        registers.channel[7][3] = UINT8_C(0x12);
        registers.channel[7][4] = UINT8_C(0x56);
        registers.channel[7][5] = 8U;
        registers.channel[7][6] = 0U;
        CHECK(mipsnes2_dma_cursor_begin(&cursor, &registers,
                                        UINT8_C(0x80)));
        CHECK(mipsnes2_dma_cursor_run(&cursor, 8U, capture_byte, &capture,
                                      &done) == MIPSNES2_DMA_RUN_COMPLETE);
        CHECK(done == 8U);
        CHECK(capture.a[0] == UINT32_C(0x561234));
        CHECK(capture.a[7] == UINT32_C(0x561234));
        CHECK(capture.b[0] == UINT32_C(0x2130));
        CHECK(capture.b[7] ==
              UINT32_C(0x2130) +
                  (mode == 1U || mode == 3U || mode == 4U || mode == 5U ||
                   mode == 7U ? (mode == 4U ? 3U : 1U) : 0U));
    }
    (void)memset(&capture, 0, sizeof(capture));
    registers.channel[7][5] = 0U;
    registers.channel[7][6] = 0U;
    CHECK(mipsnes2_dma_cursor_begin(&cursor, &registers, UINT8_C(0x80)));
    CHECK(mipsnes2_dma_cursor_run(&cursor, UINT32_C(65535), capture_byte,
                                  &capture, &done) == MIPSNES2_DMA_RUN_PAUSED);
    CHECK(done == UINT32_C(65535));
    CHECK(registers.channel[7][5] == 1U && registers.channel[7][6] == 0U);
    CHECK(mipsnes2_dma_cursor_run(&cursor, 1U, capture_byte, &capture,
                                  &done) == MIPSNES2_DMA_RUN_COMPLETE);
    CHECK(done == 1U && capture.count == UINT32_C(65536));
    CHECK(registers.channel[7][5] == 0U && registers.channel[7][6] == 0U);
    CHECK(!mipsnes2_dma_cursor_begin(NULL, &registers, 1U));
    CHECK(mipsnes2_dma_cursor_run(NULL, 1U, capture_byte, &capture,
                                  &done) == MIPSNES2_DMA_RUN_INVALID_ARGUMENT);
    return 0;
}

int main(void)
{
    static const uint8_t expected[8][8] = {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 0, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 1, 1, 0, 0, 1, 1 },
        { 0, 1, 2, 3, 0, 1, 2, 3 },
        { 0, 1, 0, 1, 0, 1, 0, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 1, 1, 0, 0, 1, 1 }
    };
    uint8_t offset = UINT8_C(0x5a);
    unsigned mode;
    unsigned index;

    for (mode = 0U; mode < 8U; ++mode) {
        for (index = 0U; index < 8U; ++index) {
            CHECK(mipsnes2_dma_port_offset((uint8_t)mode, index, &offset));
            CHECK(offset == expected[mode][index]);
        }
    }
    CHECK(mipsnes2_dma_port_offset(4U, UINT32_MAX, &offset));
    CHECK(offset == 3U);
    CHECK(!mipsnes2_dma_port_offset(8U, 0U, &offset));
    CHECK(offset == 3U);
    CHECK(!mipsnes2_dma_port_offset(0U, 0U, NULL));
    CHECK(mipsnes2_dma_next_a_offset(UINT16_C(0xffff), 0U) == 0U);
    CHECK(mipsnes2_dma_next_a_offset(0U, 16U) == UINT16_C(0xffff));
    CHECK(mipsnes2_dma_next_a_offset(0U, 8U) == 0U);
    CHECK(mipsnes2_dma_next_a_offset(0U, 24U) == 0U);
    CHECK(mipsnes2_dma_transfer_length(0U) == UINT32_C(65536));
    CHECK(mipsnes2_dma_transfer_length(UINT16_C(0xffff)) ==
          UINT32_C(65535));
    CHECK(check_bounded_transfers() == 0);
    CHECK(check_zero_count_and_modes() == 0);
    (void)puts("DMA transfer address plan and bounded cursor: OK");
    return 0;
}
