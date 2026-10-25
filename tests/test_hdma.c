/* Deterministic pure HDMA table and scanline tests. */
#include "core/dma/hdma.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    (void)fprintf(stderr, "FAIL:%d: %s\n", __LINE__, #x); return 1; \
} } while (0)

typedef struct {
    uint8_t bank_7e[65536];
    uint8_t bank_80[65536];
    mipsnes2_address reads[64];
    mipsnes2_address a[64];
    mipsnes2_address b[64];
    bool reverse[64];
    unsigned read_count;
    unsigned transfer_count;
} fixture;

static uint8_t table_read(void *context, mipsnes2_address address)
{
    fixture *f = context;
    uint8_t bank = (uint8_t)(address >> 16);

    f->reads[f->read_count++] = address;
    return bank == UINT8_C(0x7e) ? f->bank_7e[(uint16_t)address]
                                 : f->bank_80[(uint16_t)address];
}

static void transfer_byte(void *context, mipsnes2_address a_address,
                          mipsnes2_address b_address, bool b_to_a)
{
    fixture *f = context;
    unsigned index = f->transfer_count++;

    f->a[index] = a_address;
    f->b[index] = b_address;
    f->reverse[index] = b_to_a;
}

static void configure(uint8_t *channel, uint8_t control, uint8_t port,
                      uint16_t table, uint8_t bank)
{
    channel[0] = control;
    channel[1] = port;
    channel[2] = (uint8_t)table;
    channel[3] = (uint8_t)(table >> 8);
    channel[4] = bank;
}

static int test_direct_indirect_and_line_counters(void)
{
    mipsnes2_dma_registers registers = {0};
    mipsnes2_hdma_state state;
    mipsnes2_hdma_report report;
    fixture f;

    (void)memset(&f, 0, sizeof(f));
    mipsnes2_dma_registers_init(&registers);
    mipsnes2_hdma_state_init(&state);
    configure(registers.channel[0], 1U, UINT8_C(0x18),
              UINT16_C(0x1000), UINT8_C(0x7e));
    configure(registers.channel[1], UINT8_C(0xc0), UINT8_C(0x22),
              UINT16_C(0x2000), UINT8_C(0x80));
    registers.channel[1][7] = UINT8_C(0x7e);
    configure(registers.channel[2], 4U, UINT8_C(0xfc),
              UINT16_C(0x3000), UINT8_C(0x7e));
    f.bank_7e[0x1000] = UINT8_C(0x82);
    f.bank_80[0x2000] = 1U;
    f.bank_80[0x2001] = UINT8_C(0x34);
    f.bank_80[0x2002] = UINT8_C(0x12);
    f.bank_7e[0x3000] = 0U;

    CHECK(mipsnes2_hdma_frame_begin(&state, &registers, 7U,
                                    table_read, &f, &report));
    CHECK(report.table_bytes == 5U && report.data_bytes == 0U &&
          report.active_mask == 3U);
    CHECK(state.terminated_mask == 4U && state.transfer_mask == 3U);
    CHECK(registers.channel[0][8] == 1U &&
          registers.channel[0][9] == UINT8_C(0x10));
    CHECK(registers.channel[1][5] == UINT8_C(0x34) &&
          registers.channel[1][6] == UINT8_C(0x12));
    CHECK(f.reads[0] == UINT32_C(0x7e1000) &&
          f.reads[1] == UINT32_C(0x802000) &&
          f.reads[4] == UINT32_C(0x7e3000));

    /* Channel 0 repeats two direct bytes. Channel 1 performs one reverse
     * indirect byte, reloads a zero descriptor and terminates. */
    f.bank_80[0x2003] = 0U;
    f.bank_80[0x2004] = UINT8_C(0xaa);
    f.bank_80[0x2005] = UINT8_C(0xbb);
    CHECK(mipsnes2_hdma_run_scanline(&state, 3U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.channels_visited == 2U && report.data_bytes == 3U &&
          report.table_bytes == 2U && report.active_mask == 1U);
    CHECK(f.a[0] == UINT32_C(0x7e1001) && f.b[0] == UINT32_C(0x2118));
    CHECK(f.a[1] == UINT32_C(0x7e1002) && f.b[1] == UINT32_C(0x2119));
    CHECK(f.a[2] == UINT32_C(0x7e1234) && f.b[2] == UINT32_C(0x2122) &&
          f.reverse[2]);
    CHECK(registers.channel[0][10] == UINT8_C(0x81));
    CHECK(registers.channel[1][5] == 0U &&
          registers.channel[1][6] == UINT8_C(0xaa) &&
          registers.channel[1][8] == 5U &&
          registers.channel[1][9] == UINT8_C(0x20));

    /* Reloading a direct descriptor leaves A2A at its first data byte. */
    f.bank_7e[0x1005] = 3U;
    CHECK(mipsnes2_hdma_run_scanline(&state, 1U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.data_bytes == 2U && report.table_bytes == 1U &&
          registers.channel[0][8] == 6U &&
          registers.channel[0][9] == UINT8_C(0x10));
    CHECK(registers.channel[0][10] == 3U && state.transfer_mask == 1U);
    f.bank_7e[0x1008] = 0U;
    CHECK(mipsnes2_hdma_run_scanline(&state, 1U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.data_bytes == 2U && registers.channel[0][10] == 2U &&
          state.transfer_mask == 0U);
    CHECK(mipsnes2_hdma_run_scanline(&state, 1U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.data_bytes == 0U && registers.channel[0][10] == 1U);
    CHECK(mipsnes2_hdma_run_scanline(&state, 1U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.data_bytes == 0U && report.table_bytes == 1U &&
          report.active_mask == 0U && state.terminated_mask == 7U);
    return 0;
}

static int test_pause_manual_enable_and_128_lines(void)
{
    mipsnes2_dma_registers registers = {0};
    mipsnes2_hdma_state state;
    mipsnes2_hdma_report report;
    fixture f;

    (void)memset(&f, 0, sizeof(f));
    mipsnes2_dma_registers_init(&registers);
    mipsnes2_hdma_state_init(&state);
    configure(registers.channel[0], 0U, UINT8_C(0x30),
              UINT16_C(0x4000), UINT8_C(0x7e));
    f.bank_7e[0x4000] = UINT8_C(0x82);
    CHECK(mipsnes2_hdma_frame_begin(&state, &registers, 1U,
                                    table_read, &f, &report));
    CHECK(mipsnes2_hdma_run_scanline(&state, 0U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.channels_visited == 0U && report.data_bytes == 0U &&
          registers.channel[0][10] == UINT8_C(0x82));
    CHECK(mipsnes2_hdma_run_scanline(&state, 1U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.channels_visited == 1U && report.data_bytes == 1U &&
          f.a[0] == UINT32_C(0x7e4001));

    /* A channel enabled after frame initialization uses hand-programmed
     * A2A/NLTR. $80 decrements to $7f, so it transfers once and then waits. */
    configure(registers.channel[3], UINT8_C(0x84), UINT8_C(0xfe),
              UINT16_C(0x5000), UINT8_C(0x80));
    registers.channel[3][8] = 0xfeU;
    registers.channel[3][9] = 0xffU;
    registers.channel[3][10] = UINT8_C(0x80);
    CHECK(mipsnes2_hdma_run_scanline(&state, 8U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.channels_visited == 1U && report.data_bytes == 4U &&
          registers.channel[3][8] == 2U && registers.channel[3][9] == 0U);
    CHECK(f.a[1] == UINT32_C(0x80fffe) && f.a[2] == UINT32_C(0x80ffff) &&
          f.a[3] == UINT32_C(0x800000) && f.a[4] == UINT32_C(0x800001));
    CHECK(f.b[1] == UINT32_C(0x21fe) && f.b[2] == UINT32_C(0x21ff) &&
          f.b[3] == UINT32_C(0x2100) && f.b[4] == UINT32_C(0x2101));
    CHECK(f.reverse[1] && registers.channel[3][10] == UINT8_C(0x7f));
    CHECK(mipsnes2_hdma_run_scanline(&state, 8U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.channels_visited == 1U && report.data_bytes == 0U &&
          registers.channel[3][10] == UINT8_C(0x7e));

    configure(registers.channel[4], UINT8_C(0x41), UINT8_C(0x10),
              UINT16_C(0x6000), UINT8_C(0x80));
    registers.channel[4][5] = UINT8_C(0xff);
    registers.channel[4][6] = UINT8_C(0xff);
    registers.channel[4][7] = UINT8_C(0x7e);
    registers.channel[4][10] = 2U;
    CHECK(mipsnes2_hdma_run_scanline(&state, UINT8_C(0x10), table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.data_bytes == 2U && f.a[5] == UINT32_C(0x7effff) &&
          f.a[6] == UINT32_C(0x7e0000));
    CHECK(registers.channel[4][5] == 1U && registers.channel[4][6] == 0U);
    return 0;
}

static int test_all_transfer_modes(void)
{
    static const uint8_t counts[8] = {1U, 2U, 2U, 4U, 4U, 4U, 2U, 4U};
    static const uint8_t ports[8][4] = {
        {0U, 0U, 0U, 0U}, {0U, 1U, 0U, 0U},
        {0U, 0U, 0U, 0U}, {0U, 0U, 1U, 1U},
        {0U, 1U, 2U, 3U}, {0U, 1U, 0U, 1U},
        {0U, 0U, 0U, 0U}, {0U, 0U, 1U, 1U}
    };
    unsigned mode;

    for (mode = 0U; mode < 8U; ++mode) {
        mipsnes2_dma_registers registers = {0};
        mipsnes2_hdma_state state;
        mipsnes2_hdma_report report;
        fixture f;
        unsigned byte;

        (void)memset(&f, 0, sizeof(f));
        mipsnes2_dma_registers_init(&registers);
        mipsnes2_hdma_state_init(&state);
        configure(registers.channel[0], (uint8_t)mode, UINT8_C(0xfe),
                  UINT16_C(0x6000), UINT8_C(0x7e));
        f.bank_7e[0x6000] = 1U;
        f.bank_7e[0x6001 + counts[mode]] = 0U;
        CHECK(mipsnes2_hdma_frame_begin(&state, &registers, 1U,
                                        table_read, &f, &report));
        CHECK(mipsnes2_hdma_run_scanline(&state, 1U, table_read,
                                         transfer_byte, &f, &report));
        CHECK(report.data_bytes == counts[mode] &&
              f.transfer_count == counts[mode]);
        for (byte = 0U; byte < counts[mode]; ++byte) {
            CHECK(f.a[byte] == UINT32_C(0x7e6001) + byte);
            CHECK(f.b[byte] == (UINT32_C(0x2100) |
                  (uint8_t)(UINT8_C(0xfe) + ports[mode][byte])));
        }
    }
    return 0;
}

static int test_invalid_arguments(void)
{
    mipsnes2_dma_registers registers = {0};
    mipsnes2_hdma_state state;
    mipsnes2_hdma_report report;
    fixture f;

    (void)memset(&f, 0, sizeof(f));
    mipsnes2_hdma_state_init(&state);
    CHECK(!mipsnes2_hdma_frame_begin(&state, &registers, 0U,
                                     table_read, &f, &report));
    mipsnes2_dma_registers_init(&registers);
    CHECK(!mipsnes2_hdma_frame_begin(NULL, &registers, 0U,
                                     table_read, &f, &report));
    CHECK(!mipsnes2_hdma_frame_begin(&state, &registers, 0U,
                                     NULL, &f, &report));
    CHECK(mipsnes2_hdma_frame_begin(&state, &registers, 0U,
                                    table_read, &f, &report));
    CHECK(!mipsnes2_hdma_run_scanline(&state, 0U, NULL,
                                      transfer_byte, &f, &report));
    CHECK(!mipsnes2_hdma_run_scanline(&state, 0U, table_read,
                                      NULL, &f, &report));
    return 0;
}

static int test_resumable_cursor_operations(void)
{
    mipsnes2_dma_registers registers = {0};
    mipsnes2_hdma_state state;
    mipsnes2_hdma_cursor cursor;
    mipsnes2_hdma_operation operation;
    mipsnes2_hdma_operation repeat;
    mipsnes2_hdma_report report;
    fixture f;
    static const uint8_t frame_costs[] = {18U, 8U, 8U, 8U, 8U};
    static const mipsnes2_hdma_operation_kind frame_kinds[] = {
        MIPSNES2_HDMA_OPERATION_OVERHEAD,
        MIPSNES2_HDMA_OPERATION_TABLE_READ,
        MIPSNES2_HDMA_OPERATION_TABLE_READ,
        MIPSNES2_HDMA_OPERATION_TABLE_READ,
        MIPSNES2_HDMA_OPERATION_TABLE_READ
    };
    unsigned index;
    unsigned line_clocks = 18U + 8U + 8U;
    unsigned zero_cost_descriptors = 0U;

    (void)memset(&f, 0, sizeof(f));
    mipsnes2_dma_registers_init(&registers);
    mipsnes2_hdma_state_init(&state);
    mipsnes2_hdma_cursor_init(&cursor);
    configure(registers.channel[0], 1U, UINT8_C(0x18),
              UINT16_C(0x1000), UINT8_C(0x7e));
    configure(registers.channel[1], UINT8_C(0x40), UINT8_C(0x21),
              UINT16_C(0x2000), UINT8_C(0x80));
    registers.channel[1][7] = UINT8_C(0x7e);
    f.bank_7e[0x1000] = 1U;
    f.bank_80[0x2000] = 1U;
    f.bank_80[0x2001] = UINT8_C(0x34);
    f.bank_80[0x2002] = UINT8_C(0x12);
    CHECK(mipsnes2_hdma_cursor_begin_frame(&cursor, &state, &registers, 3U) ==
          MIPSNES2_HDMA_CURSOR_READY);
    CHECK(mipsnes2_hdma_cursor_begin_scanline(&cursor, &state, 3U) ==
          MIPSNES2_HDMA_CURSOR_INVALID_ARGUMENT);
    for (index = 0U; index < 5U; ++index) {
        uint8_t value = 0U;

        CHECK(mipsnes2_hdma_cursor_peek(&cursor, &operation) ==
              MIPSNES2_HDMA_CURSOR_READY);
        CHECK(operation.kind == frame_kinds[index] &&
              operation.master_clocks == frame_costs[index]);
        CHECK(mipsnes2_hdma_cursor_peek(&cursor, &repeat) ==
              MIPSNES2_HDMA_CURSOR_READY);
        CHECK(memcmp(&operation, &repeat, sizeof(operation)) == 0);
        if (index == 0U) {
            ++registers.channel[0][1];
            CHECK(mipsnes2_hdma_cursor_commit(&cursor, &operation, 0U) ==
                  MIPSNES2_HDMA_CURSOR_STALE_OPERATION);
            --registers.channel[0][1];
            CHECK(registers.channel[0][8] == UINT8_C(0xff));
        }
        if (operation.kind == MIPSNES2_HDMA_OPERATION_TABLE_READ) {
            uint8_t *channel = registers.channel[operation.channel];
            uint16_t old_pointer = (uint16_t)(channel[8] | channel[9] << 8);

            value = table_read(&f, operation.a_address);
            CHECK((uint16_t)(channel[8] | channel[9] << 8) == old_pointer);
        }
        CHECK(mipsnes2_hdma_cursor_commit(&cursor, &operation, value) ==
              (index == 4U ? MIPSNES2_HDMA_CURSOR_COMPLETE
                           : MIPSNES2_HDMA_CURSOR_READY));
    }
    CHECK(mipsnes2_hdma_cursor_report(&cursor, &report));
    CHECK(report.channels_visited == 2U && report.table_bytes == 4U &&
          report.active_mask == 3U && f.read_count == 4U);
    CHECK(registers.channel[1][5] == UINT8_C(0x34) &&
          registers.channel[1][6] == UINT8_C(0x12));
    CHECK(mipsnes2_hdma_cursor_begin_scanline(&cursor, &state, 3U) ==
          MIPSNES2_HDMA_CURSOR_READY);
    CHECK(mipsnes2_hdma_cursor_peek(&cursor, &operation) ==
          MIPSNES2_HDMA_CURSOR_READY);
    CHECK(operation.kind == MIPSNES2_HDMA_OPERATION_OVERHEAD &&
          operation.master_clocks == 18U);
    CHECK(mipsnes2_hdma_cursor_commit(&cursor, &operation, 0U) ==
          MIPSNES2_HDMA_CURSOR_READY);
    CHECK(mipsnes2_hdma_cursor_peek(&cursor, &operation) ==
          MIPSNES2_HDMA_CURSOR_READY);
    CHECK(operation.kind == MIPSNES2_HDMA_OPERATION_OVERHEAD &&
          operation.master_clocks == 8U);
    CHECK(mipsnes2_hdma_cursor_commit(&cursor, &operation, 0U) ==
          MIPSNES2_HDMA_CURSOR_READY);
    CHECK(mipsnes2_hdma_cursor_peek(&cursor, &operation) ==
          MIPSNES2_HDMA_CURSOR_READY);
    CHECK(operation.kind == MIPSNES2_HDMA_OPERATION_TRANSFER &&
          operation.a_address == UINT32_C(0x7e1001) &&
          operation.b_address == UINT32_C(0x2118));
    CHECK(f.transfer_count == 0U && registers.channel[0][8] == 1U);
    transfer_byte(&f, operation.a_address, operation.b_address,
                  operation.b_to_a);
    CHECK(mipsnes2_hdma_cursor_commit(&cursor, &operation, 0U) ==
          MIPSNES2_HDMA_CURSOR_READY);
    CHECK(f.transfer_count == 1U && registers.channel[0][8] == 2U);
    for (;;) {
        uint8_t value = 0U;
        mipsnes2_hdma_cursor_result result;

        CHECK(mipsnes2_hdma_cursor_peek(&cursor, &operation) ==
              MIPSNES2_HDMA_CURSOR_READY);
        line_clocks += operation.master_clocks;
        if (operation.kind == MIPSNES2_HDMA_OPERATION_TABLE_READ) {
            if (operation.master_clocks == 0U) {
                ++zero_cost_descriptors;
            }
            value = table_read(&f, operation.a_address);
        } else if (operation.kind == MIPSNES2_HDMA_OPERATION_TRANSFER) {
            transfer_byte(&f, operation.a_address, operation.b_address,
                          operation.b_to_a);
        }
        result = mipsnes2_hdma_cursor_commit(&cursor, &operation, value);
        if (result == MIPSNES2_HDMA_CURSOR_COMPLETE) {
            break;
        }
        CHECK(result == MIPSNES2_HDMA_CURSOR_READY);
    }
    CHECK(line_clocks == 66U && zero_cost_descriptors == 2U &&
          f.transfer_count == 3U);
    return 0;
}

static int test_vblank_deactivation(void)
{
    mipsnes2_dma_registers registers = {0};
    mipsnes2_hdma_state state;
    mipsnes2_hdma_report report;
    fixture f;

    (void)memset(&f, 0, sizeof(f));
    mipsnes2_dma_registers_init(&registers);
    mipsnes2_hdma_state_init(&state);
    CHECK(!mipsnes2_hdma_state_deactivate(NULL));
    CHECK(!mipsnes2_hdma_state_deactivate(&state));
    configure(registers.channel[0], 0U, UINT8_C(0x18),
              UINT16_C(0x1000), UINT8_C(0x7e));
    f.bank_7e[0x1000] = UINT8_C(0x80);
    CHECK(mipsnes2_hdma_frame_begin(&state, &registers, 1U,
                                    table_read, &f, &report));
    CHECK(state.active_mask == 1U && state.transfer_mask == 1U &&
          registers.channel[0][10] == UINT8_C(0x80));
    CHECK(mipsnes2_hdma_state_deactivate(&state));
    CHECK(state.active_mask == 0U && state.transfer_mask == 0U &&
          state.known_mask == 1U && state.terminated_mask == 0U &&
          registers.channel[0][10] == UINT8_C(0x80));
    CHECK(mipsnes2_hdma_run_scanline(&state, 1U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.active_mask == 0U && report.data_bytes == 0U &&
          f.transfer_count == 0U);
    CHECK(mipsnes2_hdma_frame_begin(&state, &registers, 1U,
                                    table_read, &f, &report));
    CHECK(state.active_mask == 1U);
    return 0;
}

static int test_zero_descriptor_indirect_last_channel(void)
{
    mipsnes2_dma_registers registers = {0};
    mipsnes2_hdma_state state;
    mipsnes2_hdma_report report;
    fixture f;

    (void)memset(&f, 0, sizeof(f));
    mipsnes2_dma_registers_init(&registers);
    mipsnes2_hdma_state_init(&state);
    configure(registers.channel[0], UINT8_C(0x40), UINT8_C(0x18),
              UINT16_C(0x1000), UINT8_C(0x7e));
    configure(registers.channel[1], UINT8_C(0x40), UINT8_C(0x22),
              UINT16_C(0x2000), UINT8_C(0x7e));
    registers.channel[0][7] = UINT8_C(0x7e);
    registers.channel[1][7] = UINT8_C(0x7e);
    f.bank_7e[0x1000] = 1U;
    f.bank_7e[0x1001] = 0U;
    f.bank_7e[0x1002] = UINT8_C(0x30);
    f.bank_7e[0x1003] = 0U;
    f.bank_7e[0x1004] = UINT8_C(0xaa);
    f.bank_7e[0x1005] = UINT8_C(0xbb);
    f.bank_7e[0x2000] = 1U;
    f.bank_7e[0x2001] = 0U;
    f.bank_7e[0x2002] = UINT8_C(0x40);
    f.bank_7e[0x2003] = 0U;
    f.bank_7e[0x2004] = UINT8_C(0xcc);
    f.bank_7e[0x2005] = UINT8_C(0xdd);
    CHECK(mipsnes2_hdma_frame_begin(&state, &registers, 3U,
                                    table_read, &f, &report));
    CHECK(report.table_bytes == 6U && state.active_mask == 3U);
    CHECK(mipsnes2_hdma_run_scanline(&state, 3U, table_read,
                                     transfer_byte, &f, &report));
    CHECK(report.table_bytes == 5U && report.data_bytes == 2U &&
          state.active_mask == 0U && state.terminated_mask == 3U);
    CHECK(registers.channel[0][5] == UINT8_C(0xaa) &&
          registers.channel[0][6] == UINT8_C(0xbb) &&
          registers.channel[0][8] == 6U);
    CHECK(registers.channel[1][5] == 0U &&
          registers.channel[1][6] == UINT8_C(0xcc) &&
          registers.channel[1][8] == 5U);
    CHECK(f.read_count == 11U &&
          f.reads[9] == UINT32_C(0x7e2003) &&
          f.reads[10] == UINT32_C(0x7e2004));
    return 0;
}

int main(void)
{
    CHECK(test_direct_indirect_and_line_counters() == 0);
    CHECK(test_pause_manual_enable_and_128_lines() == 0);
    CHECK(test_all_transfer_modes() == 0);
    CHECK(test_invalid_arguments() == 0);
    CHECK(test_resumable_cursor_operations() == 0);
    CHECK(test_vblank_deactivation() == 0);
    CHECK(test_zero_descriptor_indirect_last_channel() == 0);
    (void)puts("HDMA table and scanline engine: OK");
    return 0;
}
