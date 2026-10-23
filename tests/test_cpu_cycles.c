/* MIPSNES2 resumable 65C816 operation tests. See LICENSE for GPL-3.0 terms. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/cpu/65c816/cpu.h"

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "failure at line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

enum { TRACE_CAPACITY = 32 };

typedef struct {
    bool write;
    mipsnes2_address address;
    uint8_t value;
} traced_access;

typedef struct {
    traced_access accesses[TRACE_CAPACITY];
    size_t count;
} trace_context;

static uint8_t bank00[UINT32_C(0x10000)];
static uint8_t bank7e[UINT32_C(0x10000)];
static uint8_t bank7f[UINT32_C(0x10000)];

static uint8_t *address_bank(mipsnes2_address address)
{
    switch ((uint8_t)(address >> 16)) {
    case 0x00:
        return bank00;
    case 0x7e:
        return bank7e;
    case 0x7f:
        return bank7f;
    default:
        return NULL;
    }
}

static void record_access(trace_context *trace, bool write,
                          mipsnes2_address address, uint8_t value)
{
    if (trace->count < TRACE_CAPACITY) {
        traced_access *access = &trace->accesses[trace->count];

        access->write = write;
        access->address = mipsnes2_address24(address);
        access->value = value;
    }
    trace->count++;
}

static uint8_t trace_read(void *opaque, mipsnes2_address address)
{
    trace_context *trace = opaque;
    uint8_t *bank = address_bank(address);
    uint8_t value = bank != NULL ? bank[(uint16_t)address] : UINT8_C(0xa5);

    record_access(trace, false, address, value);
    return value;
}

static void trace_write(void *opaque, mipsnes2_address address, uint8_t value)
{
    trace_context *trace = opaque;
    uint8_t *bank = address_bank(address);

    record_access(trace, true, address, value);
    if (bank != NULL) {
        bank[(uint16_t)address] = value;
    }
}

static int initialize_cpu(mipsnes2_cpu *cpu, trace_context *trace,
                          uint16_t reset_pc)
{
    mipsnes2_bus_interface interface;

    (void)memset(bank00, 0, sizeof(bank00));
    (void)memset(bank7e, 0, sizeof(bank7e));
    (void)memset(bank7f, 0, sizeof(bank7f));
    (void)memset(trace, 0, sizeof(*trace));
    bank00[0xfffc] = (uint8_t)reset_pc;
    bank00[0xfffd] = (uint8_t)(reset_pc >> 8);
    interface.context = trace;
    interface.read8 = trace_read;
    interface.write8 = trace_write;
    mipsnes2_cpu_init(cpu, interface);
    CHECK(mipsnes2_cpu_reset(cpu) == MIPSNES2_CPU_OK);
    trace->count = 0U;
    cpu->pbr = UINT8_C(0x7e);
    return 0;
}

static int expect_operation(const mipsnes2_cpu *cpu,
                            const mipsnes2_cpu_cycle_state *state,
                            mipsnes2_cpu_operation_kind kind,
                            mipsnes2_address address, uint8_t value,
                            mipsnes2_cpu_operation *operation)
{
    CHECK(mipsnes2_cpu_cycle_peek(cpu, state, operation) ==
          MIPSNES2_CPU_CYCLE_READY);
    CHECK(operation->kind == kind);
    CHECK(operation->address == mipsnes2_address24(address));
    CHECK(operation->value == value);
    return 0;
}

static int commit_operation(mipsnes2_cpu *cpu,
                            mipsnes2_cpu_cycle_state *state,
                            const mipsnes2_cpu_operation *operation,
                            mipsnes2_cpu_cycle_result expected,
                            mipsnes2_cpu_result expected_cpu)
{
    mipsnes2_cpu_operation repeated;
    mipsnes2_cpu_result cpu_result = MIPSNES2_CPU_OK;

    if (expected != MIPSNES2_CPU_CYCLE_STALE_OPERATION) {
        CHECK(mipsnes2_cpu_cycle_peek(cpu, state, &repeated) ==
              MIPSNES2_CPU_CYCLE_READY);
        CHECK(repeated.kind == operation->kind);
        CHECK(repeated.address == operation->address);
        CHECK(repeated.value == operation->value);
    }
    CHECK(mipsnes2_cpu_cycle_commit(cpu, state, operation, &cpu_result) ==
          expected);
    CHECK(cpu_result == expected_cpu);
    return 0;
}

static int begin_instruction(mipsnes2_cpu *cpu,
                             mipsnes2_cpu_cycle_state *state)
{
    mipsnes2_cpu_operation operation;

    mipsnes2_cpu_cycle_state_init(state);
    CHECK(mipsnes2_cpu_cycle_begin(state) == MIPSNES2_CPU_CYCLE_READY);
    CHECK(expect_operation(cpu, state, MIPSNES2_CPU_OPERATION_CONTROL,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(cpu, state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    return 0;
}

typedef struct {
    mipsnes2_cpu_operation_kind kind;
    mipsnes2_address address;
    uint8_t value;
} expected_operation;

static void append_expected_operation(expected_operation *operations,
                                      size_t *count,
                                      mipsnes2_cpu_operation_kind kind,
                                      mipsnes2_address address,
                                      uint8_t value)
{
    operations[*count].kind = kind;
    operations[*count].address = address;
    operations[*count].value = value;
    (*count)++;
}

static int run_expected_instruction(mipsnes2_cpu *cpu,
                                    const expected_operation *expected,
                                    size_t count)
{
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    size_t index;

    CHECK(begin_instruction(cpu, &state) == 0);
    for (index = 0U; index < count; index++) {
        bool last = index + 1U == count;

        CHECK(expect_operation(cpu, &state, expected[index].kind,
                               expected[index].address,
                               expected[index].value, &operation) == 0);
        CHECK(commit_operation(
                  cpu, &state, &operation,
                  last ? MIPSNES2_CPU_CYCLE_COMPLETE
                       : MIPSNES2_CPU_CYCLE_READY,
                  last ? MIPSNES2_CPU_OK : MIPSNES2_CPU_NOT_READY) == 0);
    }
    return 0;
}

static int test_contract_validation_and_nop(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;
    mipsnes2_cpu_result cpu_result;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8000)) == 0);
    bank7e[0x8000] = UINT8_C(0xea);
    mipsnes2_cpu_cycle_state_init(&state);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
          MIPSNES2_CPU_CYCLE_INVALID_STATE);
    CHECK(mipsnes2_cpu_cycle_begin(NULL) ==
          MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_cycle_begin(&state) == MIPSNES2_CPU_CYCLE_READY);
    CHECK(mipsnes2_cpu_cycle_begin(&state) ==
          MIPSNES2_CPU_CYCLE_INVALID_STATE);
    CHECK(mipsnes2_cpu_cycle_peek(NULL, &state, &operation) ==
          MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, NULL, &operation) ==
          MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, NULL) ==
          MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT);

    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_CONTROL,
                           0U, 0U, &operation) == 0);
    CHECK(trace.count == 0U);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_CONTROL,
                           0U, 0U, &stale) == 0);
    CHECK(trace.count == 0U);
    stale.kind = MIPSNES2_CPU_OPERATION_INTERNAL;
    cpu_result = MIPSNES2_CPU_OK;
    CHECK(mipsnes2_cpu_cycle_commit(&cpu, &state, &stale, &cpu_result) ==
          MIPSNES2_CPU_CYCLE_STALE_OPERATION);
    CHECK(cpu_result == MIPSNES2_CPU_NOT_READY);
    CHECK(trace.count == 0U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);

    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8000), 0U, &operation) == 0);
    stale = operation;
    stale.address++;
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(trace.count == 0U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(trace.count == 1U);
    CHECK(cpu.pc == UINT16_C(0x8001));
    CHECK(cpu.instruction_cycles == UINT64_C(1));
    CHECK(cpu.last_opcode == UINT8_C(0xea));
    CHECK(cpu.last_opcode_address == UINT32_C(0x7e8000));

    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 0U, &operation) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 0U, &stale) == 0);
    CHECK(trace.count == 1U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.instruction_cycles == UINT64_C(2));
    CHECK(trace.count == 1U);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
          MIPSNES2_CPU_CYCLE_COMPLETE);
    CHECK(mipsnes2_cpu_cycle_commit(NULL, &state, &operation, &cpu_result) ==
          MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_cycle_commit(&cpu, NULL, &operation, &cpu_result) ==
          MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_cycle_commit(&cpu, &state, NULL, &cpu_result) ==
          MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT);
    CHECK(mipsnes2_cpu_cycle_commit(&cpu, &state, &operation, NULL) ==
          MIPSNES2_CPU_CYCLE_INVALID_ARGUMENT);
    return 0;
}

static int test_resumable_ora_and_missing_write_fallback(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8010)) == 0);
    bank7e[0x8010] = UINT8_C(0x09); /* ORA immediate */
    bank7e[0x8011] = UINT8_C(0x80);
    cpu.a = UINT16_C(0xab01);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8010), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.instruction_cycles == UINT64_C(1));
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8011), 0U, &operation) == 0);
    CHECK(trace.count == 1U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.a == UINT16_C(0xab81));
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(2));
    CHECK(trace.count == 2U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8020)) == 0);
    bank7e[0x8020] = UINT8_C(0x8d); /* STA absolute */
    bank7e[0x8021] = UINT8_C(0x34);
    bank7e[0x8022] = UINT8_C(0x12);
    cpu.bus.write8 = NULL;
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_INVALID_BUS);
    CHECK(cpu.stopped);
    CHECK(cpu.pc == UINT16_C(0x8021));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 1U);
    CHECK(!trace.accesses[0].write &&
          trace.accesses[0].address == UINT32_C(0x7e8020));
    return 0;
}

static int test_all_opcodes_have_resumable_sequences(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    uint16_t opcode;

    for (opcode = 0U; opcode <= UINT8_MAX; ++opcode) {
        mipsnes2_cpu_cycle_state state;
        mipsnes2_cpu_operation operation;
        mipsnes2_cpu_result cpu_result = MIPSNES2_CPU_NOT_READY;
        mipsnes2_cpu_cycle_result cycle_result;
        unsigned operation_count = 0U;

        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x4000)) == 0);
        bank7e[0x4000] = (uint8_t)opcode;
        bank7e[0x4001] = 0U;
        bank7e[0x4002] = 0U;
        bank7e[0x4003] = 0U;
        CHECK(begin_instruction(&cpu, &state) == 0);

        do {
            CHECK(operation_count++ < 16U);
            CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
                  MIPSNES2_CPU_CYCLE_READY);
            CHECK(operation.kind != MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC);
            cycle_result = mipsnes2_cpu_cycle_commit(
                &cpu, &state, &operation, &cpu_result);
            CHECK(cycle_result == MIPSNES2_CPU_CYCLE_READY ||
                  cycle_result == MIPSNES2_CPU_CYCLE_COMPLETE);
        } while (cycle_result == MIPSNES2_CPU_CYCLE_READY);

        CHECK(cpu_result == MIPSNES2_CPU_OK);
    }
    return 0;
}

static int test_final_opcode_sequences(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    expected_operation expected[8];
    size_t count;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xa000)) == 0);
    bank7e[0xa000] = UINT8_C(0x42); /* WDM */
    bank7e[0xa001] = UINT8_C(0xa5);
    count = 0U;
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea000), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea001), 0U);
    CHECK(run_expected_instruction(&cpu, expected, count) == 0);
    CHECK(cpu.pc == UINT16_C(0xa002));
    CHECK(cpu.instruction_cycles == UINT64_C(2));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xa010)) == 0);
    bank7e[0xa010] = UINT8_C(0xcb); /* WAI */
    count = 0U;
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea010), 0U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U);
    CHECK(run_expected_instruction(&cpu, expected, count) == 0);
    CHECK(cpu.waiting);
    CHECK(cpu.instruction_cycles == UINT64_C(3));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xa020)) == 0);
    bank7e[0xa020] = UINT8_C(0xdb); /* STP */
    expected[0].address = UINT32_C(0x7ea020);
    CHECK(run_expected_instruction(&cpu, expected, count) == 0);
    CHECK(cpu.clock_stopped);
    CHECK(cpu.instruction_cycles == UINT64_C(3));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xa030)) == 0);
    bank7e[0xa030] = UINT8_C(0x40); /* RTI, emulation mode */
    cpu.s = UINT16_C(0x01fc);
    bank00[0x01fd] = MIPSNES2_CPU_FLAG_C;
    bank00[0x01fe] = UINT8_C(0x34);
    bank00[0x01ff] = UINT8_C(0x12);
    count = 0U;
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea030), 0U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x0001fd), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x0001fe), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x0001ff), 0U);
    CHECK(run_expected_instruction(&cpu, expected, count) == 0);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.s == UINT16_C(0x01ff));
    CHECK(cpu.p == (MIPSNES2_CPU_FLAG_C | MIPSNES2_CPU_FLAG_M |
                    MIPSNES2_CPU_FLAG_X));
    CHECK(cpu.instruction_cycles == UINT64_C(6));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xa040)) == 0);
    bank7e[0xa040] = UINT8_C(0x40); /* RTI, native mode */
    cpu.emulation = false;
    cpu.s = UINT16_C(0x01fb);
    bank00[0x01fc] = MIPSNES2_CPU_FLAG_D;
    bank00[0x01fd] = UINT8_C(0x78);
    bank00[0x01fe] = UINT8_C(0x56);
    bank00[0x01ff] = UINT8_C(0x7f);
    count = 0U;
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea040), 0U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x0001fc), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x0001fd), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x0001fe), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x0001ff), 0U);
    CHECK(run_expected_instruction(&cpu, expected, count) == 0);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == UINT8_C(0x7f));
    CHECK(cpu.s == UINT16_C(0x01ff));
    CHECK(cpu.p == MIPSNES2_CPU_FLAG_D);
    CHECK(cpu.instruction_cycles == UINT64_C(7));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xa050)) == 0);
    bank7e[0xa050] = UINT8_C(0x44); /* MVP */
    bank7e[0xa051] = UINT8_C(0x7f);
    bank7e[0xa052] = UINT8_C(0x7e);
    bank7e[0x0023] = UINT8_C(0x5a);
    cpu.a = 0U;
    cpu.x = UINT16_C(0x0023);
    cpu.y = UINT16_C(0x0067);
    count = 0U;
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea050), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea051), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea052), 0U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7e0023), 0U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_WRITE,
                              UINT32_C(0x7f0067), UINT8_C(0x5a));
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U);
    CHECK(run_expected_instruction(&cpu, expected, count) == 0);
    CHECK(bank7f[0x0067] == UINT8_C(0x5a));
    CHECK(cpu.a == UINT16_C(0xffff));
    CHECK(cpu.x == UINT16_C(0x0022));
    CHECK(cpu.y == UINT16_C(0x0066));
    CHECK(cpu.dbr == UINT8_C(0x7f));
    CHECK(cpu.pc == UINT16_C(0xa053));
    CHECK(cpu.instruction_cycles == UINT64_C(7));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xa060)) == 0);
    bank7e[0xa060] = UINT8_C(0x54); /* MVN */
    bank7e[0xa061] = UINT8_C(0x7f);
    bank7e[0xa062] = UINT8_C(0x7e);
    bank7e[0x0123] = UINT8_C(0xc3);
    cpu.emulation = false;
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_X;
    cpu.a = UINT16_C(0x0001);
    cpu.x = UINT16_C(0x0123);
    cpu.y = UINT16_C(0x4567);
    count = 0U;
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea060), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea061), 0U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7ea062), 0U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U);
    append_expected_operation(expected, &count, MIPSNES2_CPU_OPERATION_READ,
                              UINT32_C(0x7e0123), 0U);
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_WRITE,
                              UINT32_C(0x7f4567), UINT8_C(0xc3));
    append_expected_operation(expected, &count,
                              MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U);
    CHECK(run_expected_instruction(&cpu, expected, count) == 0);
    CHECK(bank7f[0x4567] == UINT8_C(0xc3));
    CHECK(cpu.a == 0U);
    CHECK(cpu.x == UINT16_C(0x0124));
    CHECK(cpu.y == UINT16_C(0x4568));
    CHECK(cpu.pc == UINT16_C(0xa060));
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    return 0;
}

typedef enum {
    RMW_CYCLE_DIRECT,
    RMW_CYCLE_DIRECT_X,
    RMW_CYCLE_ABSOLUTE,
    RMW_CYCLE_ABSOLUTE_X
} rmw_cycle_addressing;

typedef enum {
    RMW_CYCLE_ASL,
    RMW_CYCLE_ROL,
    RMW_CYCLE_LSR,
    RMW_CYCLE_ROR,
    RMW_CYCLE_DEC,
    RMW_CYCLE_INC,
    RMW_CYCLE_TSB,
    RMW_CYCLE_TRB,
    RMW_CYCLE_STZ
} rmw_cycle_effect;

static int test_accumulator_shift_cycles(void)
{
    static const struct {
        uint8_t opcode;
        rmw_cycle_effect effect;
    } cases[] = {
        {UINT8_C(0x0a), RMW_CYCLE_ASL},
        {UINT8_C(0x2a), RMW_CYCLE_ROL},
        {UINT8_C(0x4a), RMW_CYCLE_LSR},
        {UINT8_C(0x6a), RMW_CYCLE_ROR}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_V;
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            expected_operation expected[2];
            uint16_t input;
            uint16_t result;
            uint8_t result_flags;
            bool carry_in = cases[index].effect == RMW_CYCLE_ROL ||
                            cases[index].effect == RMW_CYCLE_ROR;

            switch (cases[index].effect) {
            case RMW_CYCLE_ASL:
                input = wide != 0U ? UINT16_C(0x8001)
                                   : UINT16_C(0x0081);
                result = UINT16_C(0x0002);
                result_flags = MIPSNES2_CPU_FLAG_C;
                break;
            case RMW_CYCLE_ROL:
                input = wide != 0U ? UINT16_C(0x8000)
                                   : UINT16_C(0x0080);
                result = UINT16_C(0x0001);
                result_flags = MIPSNES2_CPU_FLAG_C;
                break;
            case RMW_CYCLE_LSR:
                input = UINT16_C(0x0001);
                result = 0U;
                result_flags = MIPSNES2_CPU_FLAG_C |
                               MIPSNES2_CPU_FLAG_Z;
                break;
            case RMW_CYCLE_ROR:
                input = UINT16_C(0x0001);
                result = wide != 0U ? UINT16_C(0x8000)
                                    : UINT16_C(0x0080);
                result_flags = MIPSNES2_CPU_FLAG_C |
                               MIPSNES2_CPU_FLAG_N;
                break;
            default:
                return 1;
            }

            CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9800)) == 0);
            cpu.emulation = false;
            cpu.a = wide != 0U
                        ? input
                        : (uint16_t)(UINT16_C(0xab00) | input);
            cpu.p = (uint8_t)(preserved |
                              (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                              MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z |
                              (carry_in ? MIPSNES2_CPU_FLAG_C : 0U));
            bank7e[0x9800] = cases[index].opcode;
            expected[0] = (expected_operation){
                MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9800), 0U};
            expected[1] = (expected_operation){
                MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U};

            CHECK(run_expected_instruction(&cpu, expected, 2U) == 0);
            CHECK(cpu.a == (wide != 0U
                                ? result
                                : (uint16_t)(UINT16_C(0xab00) | result)));
            CHECK(cpu.p ==
                  (uint8_t)(preserved |
                            (wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M) |
                            result_flags));
            CHECK(cpu.pc == UINT16_C(0x9801));
            CHECK(cpu.instruction_cycles == UINT64_C(2));
            CHECK(trace.count == 1U);
        }
    }
    return 0;
}

static int test_memory_rmw_cycles(void)
{
    static const struct {
        uint8_t opcode;
        rmw_cycle_addressing addressing;
        rmw_cycle_effect effect;
    } cases[] = {
        {UINT8_C(0x04), RMW_CYCLE_DIRECT, RMW_CYCLE_TSB},
        {UINT8_C(0x06), RMW_CYCLE_DIRECT, RMW_CYCLE_ASL},
        {UINT8_C(0x0c), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_TSB},
        {UINT8_C(0x0e), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_ASL},
        {UINT8_C(0x14), RMW_CYCLE_DIRECT, RMW_CYCLE_TRB},
        {UINT8_C(0x16), RMW_CYCLE_DIRECT_X, RMW_CYCLE_ASL},
        {UINT8_C(0x1c), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_TRB},
        {UINT8_C(0x1e), RMW_CYCLE_ABSOLUTE_X, RMW_CYCLE_ASL},
        {UINT8_C(0x26), RMW_CYCLE_DIRECT, RMW_CYCLE_ROL},
        {UINT8_C(0x2e), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_ROL},
        {UINT8_C(0x36), RMW_CYCLE_DIRECT_X, RMW_CYCLE_ROL},
        {UINT8_C(0x3e), RMW_CYCLE_ABSOLUTE_X, RMW_CYCLE_ROL},
        {UINT8_C(0x46), RMW_CYCLE_DIRECT, RMW_CYCLE_LSR},
        {UINT8_C(0x4e), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_LSR},
        {UINT8_C(0x56), RMW_CYCLE_DIRECT_X, RMW_CYCLE_LSR},
        {UINT8_C(0x5e), RMW_CYCLE_ABSOLUTE_X, RMW_CYCLE_LSR},
        {UINT8_C(0x64), RMW_CYCLE_DIRECT, RMW_CYCLE_STZ},
        {UINT8_C(0x66), RMW_CYCLE_DIRECT, RMW_CYCLE_ROR},
        {UINT8_C(0x6e), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_ROR},
        {UINT8_C(0x74), RMW_CYCLE_DIRECT_X, RMW_CYCLE_STZ},
        {UINT8_C(0x76), RMW_CYCLE_DIRECT_X, RMW_CYCLE_ROR},
        {UINT8_C(0x7e), RMW_CYCLE_ABSOLUTE_X, RMW_CYCLE_ROR},
        {UINT8_C(0x9c), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_STZ},
        {UINT8_C(0x9e), RMW_CYCLE_ABSOLUTE_X, RMW_CYCLE_STZ},
        {UINT8_C(0xc6), RMW_CYCLE_DIRECT, RMW_CYCLE_DEC},
        {UINT8_C(0xce), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_DEC},
        {UINT8_C(0xd6), RMW_CYCLE_DIRECT_X, RMW_CYCLE_DEC},
        {UINT8_C(0xde), RMW_CYCLE_ABSOLUTE_X, RMW_CYCLE_DEC},
        {UINT8_C(0xe6), RMW_CYCLE_DIRECT, RMW_CYCLE_INC},
        {UINT8_C(0xee), RMW_CYCLE_ABSOLUTE, RMW_CYCLE_INC},
        {UINT8_C(0xf6), RMW_CYCLE_DIRECT_X, RMW_CYCLE_INC},
        {UINT8_C(0xfe), RMW_CYCLE_ABSOLUTE_X, RMW_CYCLE_INC}
    };
    const uint8_t preserved = MIPSNES2_CPU_FLAG_I |
                              MIPSNES2_CPU_FLAG_D |
                              MIPSNES2_CPU_FLAG_X |
                              MIPSNES2_CPU_FLAG_V;
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            expected_operation expected[10];
            size_t count = 0U;
            bool direct = cases[index].addressing == RMW_CYCLE_DIRECT ||
                          cases[index].addressing == RMW_CYCLE_DIRECT_X;
            bool indexed = cases[index].addressing == RMW_CYCLE_DIRECT_X ||
                           cases[index].addressing == RMW_CYCLE_ABSOLUTE_X;
            bool store_zero = cases[index].effect == RMW_CYCLE_STZ;
            mipsnes2_address target = direct
                                          ? (indexed ? UINT32_C(0x001231)
                                                     : UINT32_C(0x001211))
                                          : (indexed ? UINT32_C(0x7e2020)
                                                     : UINT32_C(0x7e2000));
            mipsnes2_address high_address = target + UINT32_C(1);
            uint8_t *low;
            uint8_t *high;
            uint16_t input;
            uint16_t result;
            uint16_t accumulator = wide != 0U ? UINT16_C(0x1234)
                                               : UINT16_C(0xab34);
            uint8_t initial_status;
            uint8_t expected_status;
            uint8_t width_flag = wide != 0U ? 0U : MIPSNES2_CPU_FLAG_M;

            switch (cases[index].effect) {
            case RMW_CYCLE_ASL:
                input = wide != 0U ? UINT16_C(0x8001)
                                   : UINT16_C(0x0081);
                result = UINT16_C(0x0002);
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_N |
                                           MIPSNES2_CPU_FLAG_Z);
                expected_status = (uint8_t)(preserved | width_flag |
                                            MIPSNES2_CPU_FLAG_C);
                break;
            case RMW_CYCLE_ROL:
                input = wide != 0U ? UINT16_C(0x8000)
                                   : UINT16_C(0x0080);
                result = UINT16_C(0x0001);
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_N |
                                           MIPSNES2_CPU_FLAG_Z |
                                           MIPSNES2_CPU_FLAG_C);
                expected_status = (uint8_t)(preserved | width_flag |
                                            MIPSNES2_CPU_FLAG_C);
                break;
            case RMW_CYCLE_LSR:
                input = UINT16_C(0x0001);
                result = 0U;
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_N);
                expected_status = (uint8_t)(preserved | width_flag |
                                            MIPSNES2_CPU_FLAG_C |
                                            MIPSNES2_CPU_FLAG_Z);
                break;
            case RMW_CYCLE_ROR:
                input = UINT16_C(0x0001);
                result = wide != 0U ? UINT16_C(0x8000)
                                    : UINT16_C(0x0080);
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_Z |
                                           MIPSNES2_CPU_FLAG_C);
                expected_status = (uint8_t)(preserved | width_flag |
                                            MIPSNES2_CPU_FLAG_N |
                                            MIPSNES2_CPU_FLAG_C);
                break;
            case RMW_CYCLE_DEC:
                input = 0U;
                result = wide != 0U ? UINT16_MAX : UINT8_MAX;
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_N |
                                           MIPSNES2_CPU_FLAG_Z |
                                           MIPSNES2_CPU_FLAG_C);
                expected_status = (uint8_t)(preserved | width_flag |
                                            MIPSNES2_CPU_FLAG_N |
                                            MIPSNES2_CPU_FLAG_C);
                break;
            case RMW_CYCLE_INC:
                input = wide != 0U ? UINT16_MAX : UINT8_MAX;
                result = 0U;
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_N |
                                           MIPSNES2_CPU_FLAG_C);
                expected_status = (uint8_t)(preserved | width_flag |
                                            MIPSNES2_CPU_FLAG_Z |
                                            MIPSNES2_CPU_FLAG_C);
                break;
            case RMW_CYCLE_TSB:
                accumulator = wide != 0U ? UINT16_C(0x00ff)
                                         : UINT16_C(0xab0f);
                input = wide != 0U ? UINT16_C(0xff00)
                                   : UINT16_C(0x00f0);
                result = wide != 0U ? UINT16_MAX : UINT8_MAX;
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_N |
                                           MIPSNES2_CPU_FLAG_C);
                expected_status = (uint8_t)(initial_status |
                                            MIPSNES2_CPU_FLAG_Z);
                break;
            case RMW_CYCLE_TRB:
                accumulator = wide != 0U ? UINT16_C(0x00ff)
                                         : UINT16_C(0xab0f);
                input = wide != 0U ? UINT16_MAX : UINT8_MAX;
                result = wide != 0U ? UINT16_C(0xff00)
                                    : UINT16_C(0x00f0);
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_N |
                                           MIPSNES2_CPU_FLAG_Z |
                                           MIPSNES2_CPU_FLAG_C);
                expected_status = (uint8_t)(initial_status &
                                            (uint8_t)~MIPSNES2_CPU_FLAG_Z);
                break;
            case RMW_CYCLE_STZ:
                input = UINT16_C(0xbbaa);
                result = 0U;
                initial_status = (uint8_t)(preserved | width_flag |
                                           MIPSNES2_CPU_FLAG_N |
                                           MIPSNES2_CPU_FLAG_Z |
                                           MIPSNES2_CPU_FLAG_C);
                expected_status = initial_status;
                break;
            default:
                return 1;
            }
            CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9800)) == 0);
            cpu.emulation = false;
            cpu.d = UINT16_C(0x1201);
            cpu.dbr = UINT8_C(0x7e);
            cpu.x = UINT16_C(0x0020);
            cpu.a = accumulator;
            cpu.p = initial_status;
            bank7e[0x9800] = cases[index].opcode;
            bank7e[0x9801] = direct ? UINT8_C(0x10) : UINT8_C(0x00);
            bank7e[0x9802] = UINT8_C(0x20);
            low = &address_bank(target)[(uint16_t)target];
            high = &address_bank(high_address)[(uint16_t)high_address];
            *low = (uint8_t)input;
            *high = wide != 0U ? (uint8_t)(input >> 8) : UINT8_C(0x5a);

            append_expected_operation(
                expected, &count, MIPSNES2_CPU_OPERATION_READ,
                UINT32_C(0x7e9800), 0U);
            append_expected_operation(
                expected, &count, MIPSNES2_CPU_OPERATION_READ,
                UINT32_C(0x7e9801), 0U);
            if (!direct) {
                append_expected_operation(
                    expected, &count, MIPSNES2_CPU_OPERATION_READ,
                    UINT32_C(0x7e9802), 0U);
            } else {
                append_expected_operation(
                    expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                    0U, 1U);
            }
            if (indexed) {
                append_expected_operation(
                    expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                    0U, 2U);
            }
            if (!store_zero) {
                append_expected_operation(
                    expected, &count, MIPSNES2_CPU_OPERATION_READ,
                    target, 0U);
                if (wide != 0U) {
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_READ,
                        high_address, 0U);
                }
                append_expected_operation(
                    expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                    0U, 3U);
            }
            append_expected_operation(
                expected, &count, MIPSNES2_CPU_OPERATION_WRITE,
                target, (uint8_t)result);
            if (wide != 0U) {
                append_expected_operation(
                    expected, &count, MIPSNES2_CPU_OPERATION_WRITE,
                    high_address, (uint8_t)(result >> 8));
            }

            CHECK(run_expected_instruction(&cpu, expected, count) == 0);
            CHECK(*low == (uint8_t)result);
            CHECK(*high == (wide != 0U ? (uint8_t)(result >> 8)
                                       : UINT8_C(0x5a)));
            CHECK(cpu.a == accumulator);
            CHECK(cpu.p == expected_status);
            CHECK(cpu.pc == (direct ? UINT16_C(0x9802)
                                    : UINT16_C(0x9803)));
            CHECK(cpu.instruction_cycles == count);
            CHECK(cpu.last_opcode == cases[index].opcode);
            CHECK(!cpu.stopped);
        }
    }
    return 0;
}

static int test_memory_rmw_missing_write_preflight(void)
{
    static const uint8_t opcodes[] = {
        UINT8_C(0x04), UINT8_C(0x06), UINT8_C(0x0c), UINT8_C(0x0e),
        UINT8_C(0x14), UINT8_C(0x16), UINT8_C(0x1c), UINT8_C(0x1e),
        UINT8_C(0x26), UINT8_C(0x2e), UINT8_C(0x36), UINT8_C(0x3e),
        UINT8_C(0x46), UINT8_C(0x4e), UINT8_C(0x56), UINT8_C(0x5e),
        UINT8_C(0x64), UINT8_C(0x66), UINT8_C(0x6e), UINT8_C(0x74),
        UINT8_C(0x76), UINT8_C(0x7e), UINT8_C(0x9c), UINT8_C(0x9e),
        UINT8_C(0xc6), UINT8_C(0xce), UINT8_C(0xd6), UINT8_C(0xde),
        UINT8_C(0xe6), UINT8_C(0xee), UINT8_C(0xf6), UINT8_C(0xfe)
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(opcodes) / sizeof(opcodes[0]); ++index) {
        mipsnes2_cpu_cycle_state state;
        mipsnes2_cpu_operation operation;

        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9810)) == 0);
        bank7e[0x9810] = opcodes[index];
        cpu.bus.write8 = NULL;
        CHECK(begin_instruction(&cpu, &state) == 0);
        CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                               UINT32_C(0x7e9810), 0U, &operation) == 0);
        CHECK(commit_operation(&cpu, &state, &operation,
                               MIPSNES2_CPU_CYCLE_READY,
                               MIPSNES2_CPU_NOT_READY) == 0);
        CHECK(expect_operation(&cpu, &state,
                               MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                               0U, 0U, &operation) == 0);
        CHECK(commit_operation(&cpu, &state, &operation,
                               MIPSNES2_CPU_CYCLE_COMPLETE,
                               MIPSNES2_CPU_INVALID_BUS) == 0);
        CHECK(cpu.stopped);
        CHECK(cpu.stop_reason == MIPSNES2_CPU_INVALID_BUS);
        CHECK(cpu.pc == UINT16_C(0x9811));
        CHECK(cpu.instruction_cycles == 0U);
        CHECK(trace.count == 1U);
    }
    return 0;
}

static int test_memory_rmw_stale_internal(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9820)) == 0);
    cpu.emulation = false;
    cpu.d = UINT16_C(0x1201);
    cpu.x = UINT16_C(0x0020);
    bank7e[0x9820] = UINT8_C(0x16); /* ASL direct,X */
    bank7e[0x9821] = UINT8_C(0x10);
    bank00[0x1231] = UINT8_C(0x40);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9820), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9821), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &stale) == 0);
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(bank00[0x1231] == UINT8_C(0x40));
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    return 0;
}

typedef enum {
    LOAD_STORE_TEST_IMMEDIATE,
    LOAD_STORE_TEST_DIRECT,
    LOAD_STORE_TEST_DIRECT_X,
    LOAD_STORE_TEST_DIRECT_Y,
    LOAD_STORE_TEST_DIRECT_INDIRECT,
    LOAD_STORE_TEST_DIRECT_INDEXED_INDIRECT_X,
    LOAD_STORE_TEST_DIRECT_INDIRECT_Y,
    LOAD_STORE_TEST_DIRECT_INDIRECT_LONG,
    LOAD_STORE_TEST_DIRECT_INDIRECT_LONG_Y,
    LOAD_STORE_TEST_STACK_RELATIVE,
    LOAD_STORE_TEST_STACK_RELATIVE_INDIRECT_Y,
    LOAD_STORE_TEST_ABSOLUTE,
    LOAD_STORE_TEST_ABSOLUTE_X,
    LOAD_STORE_TEST_ABSOLUTE_Y,
    LOAD_STORE_TEST_ABSOLUTE_LONG,
    LOAD_STORE_TEST_ABSOLUTE_LONG_X
} load_store_test_mode;

typedef enum {
    LOAD_STORE_TEST_A,
    LOAD_STORE_TEST_X,
    LOAD_STORE_TEST_Y
} load_store_test_register;

static uint16_t load_store_test_register_value(
    const mipsnes2_cpu *cpu, load_store_test_register target)
{
    switch (target) {
    case LOAD_STORE_TEST_X:
        return cpu->x;
    case LOAD_STORE_TEST_Y:
        return cpu->y;
    case LOAD_STORE_TEST_A:
    default:
        return cpu->a;
    }
}

static int test_load_store_family_cycles(void)
{
    static const struct {
        uint8_t opcode;
        load_store_test_mode mode;
        load_store_test_register target;
        bool store;
    } cases[] = {
        {UINT8_C(0x81), LOAD_STORE_TEST_DIRECT_INDEXED_INDIRECT_X,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x83), LOAD_STORE_TEST_STACK_RELATIVE,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x84), LOAD_STORE_TEST_DIRECT, LOAD_STORE_TEST_Y, true},
        {UINT8_C(0x85), LOAD_STORE_TEST_DIRECT, LOAD_STORE_TEST_A, true},
        {UINT8_C(0x86), LOAD_STORE_TEST_DIRECT, LOAD_STORE_TEST_X, true},
        {UINT8_C(0x87), LOAD_STORE_TEST_DIRECT_INDIRECT_LONG,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x8c), LOAD_STORE_TEST_ABSOLUTE, LOAD_STORE_TEST_Y, true},
        {UINT8_C(0x8d), LOAD_STORE_TEST_ABSOLUTE, LOAD_STORE_TEST_A, true},
        {UINT8_C(0x8e), LOAD_STORE_TEST_ABSOLUTE, LOAD_STORE_TEST_X, true},
        {UINT8_C(0x8f), LOAD_STORE_TEST_ABSOLUTE_LONG,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x91), LOAD_STORE_TEST_DIRECT_INDIRECT_Y,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x92), LOAD_STORE_TEST_DIRECT_INDIRECT,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x93), LOAD_STORE_TEST_STACK_RELATIVE_INDIRECT_Y,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x94), LOAD_STORE_TEST_DIRECT_X, LOAD_STORE_TEST_Y, true},
        {UINT8_C(0x95), LOAD_STORE_TEST_DIRECT_X, LOAD_STORE_TEST_A, true},
        {UINT8_C(0x96), LOAD_STORE_TEST_DIRECT_Y, LOAD_STORE_TEST_X, true},
        {UINT8_C(0x97), LOAD_STORE_TEST_DIRECT_INDIRECT_LONG_Y,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x99), LOAD_STORE_TEST_ABSOLUTE_Y,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x9d), LOAD_STORE_TEST_ABSOLUTE_X,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0x9f), LOAD_STORE_TEST_ABSOLUTE_LONG_X,
         LOAD_STORE_TEST_A, true},
        {UINT8_C(0xa0), LOAD_STORE_TEST_IMMEDIATE, LOAD_STORE_TEST_Y, false},
        {UINT8_C(0xa1), LOAD_STORE_TEST_DIRECT_INDEXED_INDIRECT_X,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xa2), LOAD_STORE_TEST_IMMEDIATE, LOAD_STORE_TEST_X, false},
        {UINT8_C(0xa3), LOAD_STORE_TEST_STACK_RELATIVE,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xa4), LOAD_STORE_TEST_DIRECT, LOAD_STORE_TEST_Y, false},
        {UINT8_C(0xa5), LOAD_STORE_TEST_DIRECT, LOAD_STORE_TEST_A, false},
        {UINT8_C(0xa6), LOAD_STORE_TEST_DIRECT, LOAD_STORE_TEST_X, false},
        {UINT8_C(0xa7), LOAD_STORE_TEST_DIRECT_INDIRECT_LONG,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xa9), LOAD_STORE_TEST_IMMEDIATE, LOAD_STORE_TEST_A, false},
        {UINT8_C(0xac), LOAD_STORE_TEST_ABSOLUTE, LOAD_STORE_TEST_Y, false},
        {UINT8_C(0xad), LOAD_STORE_TEST_ABSOLUTE, LOAD_STORE_TEST_A, false},
        {UINT8_C(0xae), LOAD_STORE_TEST_ABSOLUTE, LOAD_STORE_TEST_X, false},
        {UINT8_C(0xaf), LOAD_STORE_TEST_ABSOLUTE_LONG,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xb1), LOAD_STORE_TEST_DIRECT_INDIRECT_Y,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xb2), LOAD_STORE_TEST_DIRECT_INDIRECT,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xb3), LOAD_STORE_TEST_STACK_RELATIVE_INDIRECT_Y,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xb4), LOAD_STORE_TEST_DIRECT_X, LOAD_STORE_TEST_Y, false},
        {UINT8_C(0xb5), LOAD_STORE_TEST_DIRECT_X, LOAD_STORE_TEST_A, false},
        {UINT8_C(0xb6), LOAD_STORE_TEST_DIRECT_Y, LOAD_STORE_TEST_X, false},
        {UINT8_C(0xb7), LOAD_STORE_TEST_DIRECT_INDIRECT_LONG_Y,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xb9), LOAD_STORE_TEST_ABSOLUTE_Y,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xbc), LOAD_STORE_TEST_ABSOLUTE_X,
         LOAD_STORE_TEST_Y, false},
        {UINT8_C(0xbd), LOAD_STORE_TEST_ABSOLUTE_X,
         LOAD_STORE_TEST_A, false},
        {UINT8_C(0xbe), LOAD_STORE_TEST_ABSOLUTE_Y,
         LOAD_STORE_TEST_X, false},
        {UINT8_C(0xbf), LOAD_STORE_TEST_ABSOLUTE_LONG_X,
         LOAD_STORE_TEST_A, false}
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t case_index;

    for (case_index = 0U;
         case_index < sizeof(cases) / sizeof(cases[0]); ++case_index) {
        unsigned wide;

        for (wide = 0U; wide < 2U; ++wide) {
            const uint16_t start_pc = UINT16_C(0x8600);
            const uint8_t operand = UINT8_C(0x10);
            expected_operation expected[16];
            size_t count = 0U;
            size_t bus_count = 0U;
            mipsnes2_address pointer = 0U;
            mipsnes2_address target = 0U;
            uint16_t source;
            uint8_t initial_status;
            bool index_cycle = cases[case_index].store || wide != 0U;

            CHECK(initialize_cpu(&cpu, &trace, start_pc) == 0);
            cpu.emulation = false;
            initial_status = (uint8_t)(MIPSNES2_CPU_FLAG_C |
                                       MIPSNES2_CPU_FLAG_I |
                                       MIPSNES2_CPU_FLAG_D |
                                       MIPSNES2_CPU_FLAG_V |
                                       (wide != 0U
                                            ? 0U
                                            : MIPSNES2_CPU_FLAG_M |
                                                  MIPSNES2_CPU_FLAG_X));
            cpu.p = initial_status;
            cpu.dbr = UINT8_C(0x7f);
            cpu.d = UINT16_C(0x1201);
            cpu.s = UINT16_C(0x1f00);
            cpu.a = UINT16_C(0x5a42);
            cpu.x = UINT16_C(0x0020);
            cpu.y = UINT16_C(0x0030);
            bank7e[start_pc] = cases[case_index].opcode;
            bank7e[(uint16_t)(start_pc + UINT16_C(1))] = operand;
            bank7e[(uint16_t)(start_pc + UINT16_C(2))] = UINT8_C(0x20);
            bank7e[(uint16_t)(start_pc + UINT16_C(3))] = UINT8_C(0x7e);

            append_expected_operation(expected, &count,
                                      MIPSNES2_CPU_OPERATION_READ,
                                      UINT32_C(0x7e8600), 0U);
            if (cases[case_index].mode == LOAD_STORE_TEST_IMMEDIATE) {
                bank7e[(uint16_t)(start_pc + UINT16_C(1))] = UINT8_C(0x80);
                bank7e[(uint16_t)(start_pc + UINT16_C(2))] = UINT8_C(0x81);
                append_expected_operation(expected, &count,
                                          MIPSNES2_CPU_OPERATION_READ,
                                          UINT32_C(0x7e8601), 0U);
                if (wide != 0U) {
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              UINT32_C(0x7e8602), 0U);
                }
            } else {
                append_expected_operation(expected, &count,
                                          MIPSNES2_CPU_OPERATION_READ,
                                          UINT32_C(0x7e8601), 0U);
                switch (cases[case_index].mode) {
                case LOAD_STORE_TEST_DIRECT:
                    target = UINT32_C(0x001211);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(1));
                    break;
                case LOAD_STORE_TEST_DIRECT_X:
                    target = UINT32_C(0x001231);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(1));
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(3));
                    break;
                case LOAD_STORE_TEST_DIRECT_Y:
                    target = UINT32_C(0x001241);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(1));
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(3));
                    break;
                case LOAD_STORE_TEST_DIRECT_INDIRECT:
                case LOAD_STORE_TEST_DIRECT_INDIRECT_Y:
                    pointer = UINT32_C(0x001211);
                    target = cases[case_index].mode ==
                                     LOAD_STORE_TEST_DIRECT_INDIRECT_Y
                                 ? UINT32_C(0x7f3030)
                                 : UINT32_C(0x7f3000);
                    bank00[(uint16_t)pointer] = UINT8_C(0x00);
                    bank00[(uint16_t)(pointer + UINT32_C(1))] =
                        UINT8_C(0x30);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(1));
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer, 0U);
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer + UINT32_C(1), 0U);
                    if (cases[case_index].mode ==
                            LOAD_STORE_TEST_DIRECT_INDIRECT_Y &&
                        index_cycle) {
                        append_expected_operation(
                            expected, &count,
                            MIPSNES2_CPU_OPERATION_INTERNAL, 0U,
                            UINT8_C(3));
                    }
                    break;
                case LOAD_STORE_TEST_DIRECT_INDEXED_INDIRECT_X:
                    pointer = UINT32_C(0x001231);
                    target = UINT32_C(0x7f3000);
                    bank00[(uint16_t)pointer] = UINT8_C(0x00);
                    bank00[(uint16_t)(pointer + UINT32_C(1))] =
                        UINT8_C(0x30);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(1));
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(2));
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer, 0U);
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer + UINT32_C(1), 0U);
                    break;
                case LOAD_STORE_TEST_DIRECT_INDIRECT_LONG:
                case LOAD_STORE_TEST_DIRECT_INDIRECT_LONG_Y:
                    pointer = UINT32_C(0x001211);
                    target = cases[case_index].mode ==
                                     LOAD_STORE_TEST_DIRECT_INDIRECT_LONG_Y
                                 ? UINT32_C(0x7e4030)
                                 : UINT32_C(0x7e4000);
                    bank00[(uint16_t)pointer] = UINT8_C(0x00);
                    bank00[(uint16_t)(pointer + UINT32_C(1))] =
                        UINT8_C(0x40);
                    bank00[(uint16_t)(pointer + UINT32_C(2))] =
                        UINT8_C(0x7e);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(1));
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer, 0U);
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer + UINT32_C(1), 0U);
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer + UINT32_C(2), 0U);
                    break;
                case LOAD_STORE_TEST_STACK_RELATIVE:
                    target = UINT32_C(0x001f10);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(2));
                    break;
                case LOAD_STORE_TEST_STACK_RELATIVE_INDIRECT_Y:
                    pointer = UINT32_C(0x001f10);
                    target = UINT32_C(0x7f5030);
                    bank00[(uint16_t)pointer] = UINT8_C(0x00);
                    bank00[(uint16_t)(pointer + UINT32_C(1))] =
                        UINT8_C(0x50);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(2));
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer, 0U);
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              pointer + UINT32_C(1), 0U);
                    append_expected_operation(
                        expected, &count, MIPSNES2_CPU_OPERATION_INTERNAL,
                        0U, UINT8_C(3));
                    break;
                case LOAD_STORE_TEST_ABSOLUTE:
                case LOAD_STORE_TEST_ABSOLUTE_X:
                case LOAD_STORE_TEST_ABSOLUTE_Y:
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              UINT32_C(0x7e8602), 0U);
                    target = UINT32_C(0x7f2010);
                    if (cases[case_index].mode ==
                        LOAD_STORE_TEST_ABSOLUTE_X) {
                        target += UINT32_C(0x20);
                    } else if (cases[case_index].mode ==
                               LOAD_STORE_TEST_ABSOLUTE_Y) {
                        target += UINT32_C(0x30);
                    }
                    if (cases[case_index].mode !=
                            LOAD_STORE_TEST_ABSOLUTE &&
                        index_cycle) {
                        append_expected_operation(
                            expected, &count,
                            MIPSNES2_CPU_OPERATION_INTERNAL, 0U,
                            UINT8_C(3));
                    }
                    break;
                case LOAD_STORE_TEST_ABSOLUTE_LONG:
                case LOAD_STORE_TEST_ABSOLUTE_LONG_X:
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              UINT32_C(0x7e8602), 0U);
                    append_expected_operation(expected, &count,
                                              MIPSNES2_CPU_OPERATION_READ,
                                              UINT32_C(0x7e8603), 0U);
                    target = UINT32_C(0x7e2010);
                    if (cases[case_index].mode ==
                        LOAD_STORE_TEST_ABSOLUTE_LONG_X) {
                        target += UINT32_C(0x20);
                    }
                    break;
                case LOAD_STORE_TEST_IMMEDIATE:
                default:
                    break;
                }

                source = load_store_test_register_value(
                    &cpu, cases[case_index].target);
                if (!cases[case_index].store) {
                    uint8_t *data_bank = address_bank(target);

                    CHECK(data_bank != NULL);
                    data_bank[(uint16_t)target] = UINT8_C(0x80);
                    data_bank[(uint16_t)(target + UINT32_C(1))] =
                        UINT8_C(0x81);
                }
                append_expected_operation(
                    expected, &count,
                    cases[case_index].store
                        ? MIPSNES2_CPU_OPERATION_WRITE
                        : MIPSNES2_CPU_OPERATION_READ,
                    target,
                    cases[case_index].store ? (uint8_t)source : 0U);
                if (wide != 0U) {
                    append_expected_operation(
                        expected, &count,
                        cases[case_index].store
                            ? MIPSNES2_CPU_OPERATION_WRITE
                            : MIPSNES2_CPU_OPERATION_READ,
                        target + UINT32_C(1),
                        cases[case_index].store
                            ? (uint8_t)(source >> 8)
                            : 0U);
                }
            }

            {
                size_t operation_index;

                for (operation_index = 0U; operation_index < count;
                     ++operation_index) {
                    if (expected[operation_index].kind !=
                        MIPSNES2_CPU_OPERATION_INTERNAL) {
                        bus_count++;
                    }
                }
            }
            CHECK(run_expected_instruction(&cpu, expected, count) == 0);
            CHECK(cpu.instruction_cycles == (uint64_t)count);
            CHECK(trace.count == bus_count);
            if (cases[case_index].store) {
                uint8_t *data_bank = address_bank(target);

                source = load_store_test_register_value(
                    &cpu, cases[case_index].target);
                CHECK(data_bank != NULL);
                CHECK(data_bank[(uint16_t)target] == (uint8_t)source);
                if (wide != 0U) {
                    CHECK(data_bank[(uint16_t)(target + UINT32_C(1))] ==
                          (uint8_t)(source >> 8));
                }
                CHECK(cpu.p == initial_status);
            } else {
                uint16_t expected_value = wide != 0U
                                              ? UINT16_C(0x8180)
                                              : UINT16_C(0x0080);
                uint16_t register_value = load_store_test_register_value(
                    &cpu, cases[case_index].target);

                if (cases[case_index].target == LOAD_STORE_TEST_A &&
                    wide == 0U) {
                    expected_value |= UINT16_C(0x5a00);
                }
                CHECK(register_value == expected_value);
                CHECK(cpu.p == (uint8_t)(initial_status |
                                         MIPSNES2_CPU_FLAG_N));
            }
        }
    }
    return 0;
}

static int test_load_store_missing_write_preflight(void)
{
    static const uint8_t opcodes[] = {
        UINT8_C(0x81), UINT8_C(0x83), UINT8_C(0x84), UINT8_C(0x85),
        UINT8_C(0x86), UINT8_C(0x87), UINT8_C(0x8c), UINT8_C(0x8d),
        UINT8_C(0x8e), UINT8_C(0x8f), UINT8_C(0x91), UINT8_C(0x92),
        UINT8_C(0x93), UINT8_C(0x94), UINT8_C(0x95), UINT8_C(0x96),
        UINT8_C(0x97), UINT8_C(0x99), UINT8_C(0x9d), UINT8_C(0x9f)
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(opcodes) / sizeof(opcodes[0]); ++index) {
        mipsnes2_cpu_cycle_state state;
        mipsnes2_cpu_operation operation;

        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8680)) == 0);
        bank7e[0x8680] = opcodes[index];
        bank7e[0x8681] = UINT8_C(0x10);
        cpu.bus.write8 = NULL;
        CHECK(begin_instruction(&cpu, &state) == 0);
        CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                               UINT32_C(0x7e8680), 0U, &operation) == 0);
        CHECK(commit_operation(&cpu, &state, &operation,
                               MIPSNES2_CPU_CYCLE_READY,
                               MIPSNES2_CPU_NOT_READY) == 0);
        CHECK(expect_operation(&cpu, &state,
                               MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                               0U, 0U, &operation) == 0);
        CHECK(commit_operation(&cpu, &state, &operation,
                               MIPSNES2_CPU_CYCLE_COMPLETE,
                               MIPSNES2_CPU_INVALID_BUS) == 0);
        CHECK(cpu.stopped);
        CHECK(cpu.pc == UINT16_C(0x8681));
        CHECK(cpu.instruction_cycles == 0U);
        CHECK(trace.count == 1U);
    }
    return 0;
}

static int test_load_store_stale_internal(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x86c0)) == 0);
    cpu.emulation = false;
    cpu.d = UINT16_C(0x1201);
    cpu.x = UINT16_C(0x0020);
    bank7e[0x86c0] = UINT8_C(0xb5); /* LDA direct,X */
    bank7e[0x86c1] = UINT8_C(0x10);
    bank00[0x1231] = UINT8_C(0x80);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e86c0), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e86c1), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &stale) == 0);
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 3U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(trace.count == 2U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    return 0;
}

static int test_lda_immediate_cycles(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8100)) == 0);
    bank7e[0x8100] = UINT8_C(0xa9);
    bank7e[0x8101] = UINT8_C(0x80);
    cpu.a = UINT16_C(0x5a00);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8100), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8101), 0U, &operation) == 0);
    CHECK(cpu.a == UINT16_C(0x5a00));
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.a == UINT16_C(0x5a80));
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_Z) == 0U);
    CHECK(cpu.pc == UINT16_C(0x8102));
    CHECK(cpu.instruction_cycles == UINT64_C(2));
    CHECK(trace.count == 2U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xffff)) == 0);
    cpu.emulation = false;
    cpu.p = 0U;
    bank7e[0xffff] = UINT8_C(0xa9);
    bank7e[0x0000] = UINT8_C(0x00);
    bank7e[0x0001] = UINT8_C(0x80);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7effff), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e0000), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.a == 0U);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e0001), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.a == UINT16_C(0x8000));
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu.pc == UINT16_C(0x0002));
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(trace.count == 3U);
    return 0;
}

static int test_sta_absolute_cycles(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation repeated;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8200)) == 0);
    bank7e[0x8200] = UINT8_C(0x8d);
    bank7e[0x8201] = UINT8_C(0x34);
    bank7e[0x8202] = UINT8_C(0x12);
    cpu.dbr = UINT8_C(0x7f);
    cpu.a = UINT16_C(0xabcd);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8200), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8201), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8202), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x7f1234), UINT8_C(0xcd),
                           &operation) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x7f1234), UINT8_C(0xcd),
                           &repeated) == 0);
    CHECK(trace.count == 3U);
    CHECK(bank7f[0x1234] == 0U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(bank7f[0x1234] == UINT8_C(0xcd));
    CHECK(trace.count == 4U);
    CHECK(cpu.instruction_cycles == UINT64_C(4));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8300)) == 0);
    cpu.emulation = false;
    cpu.p = 0U;
    cpu.dbr = UINT8_C(0x7e);
    cpu.a = UINT16_C(0xbeef);
    bank7e[0x8300] = UINT8_C(0x8d);
    bank7e[0x8301] = UINT8_C(0xff);
    bank7e[0x8302] = UINT8_C(0xff);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8300), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8301), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8302), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x7effff), UINT8_C(0xef),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x7f0000), UINT8_C(0xbe),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(bank7e[0xffff] == UINT8_C(0xef));
    CHECK(bank7f[0x0000] == UINT8_C(0xbe));
    CHECK(trace.count == 5U);
    CHECK(cpu.instruction_cycles == UINT64_C(5));
    return 0;
}

static int test_rep_sep_internal_commit(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8400)) == 0);
    cpu.emulation = false;
    cpu.p = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
            MIPSNES2_CPU_FLAG_C;
    cpu.x = UINT16_C(0x12ab);
    cpu.y = UINT16_C(0x34cd);
    bank7e[0x8400] = UINT8_C(0xc2);
    bank7e[0x8401] = UINT8_C(0x30);
    bank7e[0x8402] = UINT8_C(0xe2);
    bank7e[0x8403] = UINT8_C(0x30);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8400), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8401), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK((cpu.p & (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X)) ==
          (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X));
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK((cpu.p & (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X)) == 0U);
    CHECK(cpu.x == UINT16_C(0x12ab));
    CHECK(cpu.y == UINT16_C(0x34cd));
    CHECK(cpu.instruction_cycles == UINT64_C(3));

    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8402), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8403), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK((cpu.p & (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X)) ==
          (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X));
    CHECK(cpu.x == UINT16_C(0x00ab));
    CHECK(cpu.y == UINT16_C(0x00cd));
    CHECK(cpu.instruction_cycles == UINT64_C(6));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8500)) == 0);
    bank7e[0x8500] = UINT8_C(0xc2);
    bank7e[0x8501] = UINT8_C(0x30);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK((cpu.p & (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X)) ==
          (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X));
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    return 0;
}

static int test_software_interrupt_cycles(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation repeated;
    uint8_t pushed_status;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8000)) == 0);
    cpu.s = UINT16_C(0x0100);
    cpu.p |= MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_C;
    pushed_status = cpu.p;
    bank7e[0x8000] = UINT8_C(0x00); /* BRK */
    bank7e[0x8001] = UINT8_C(0x5a);
    bank00[0xfffe] = UINT8_C(0x34);
    bank00[0xffff] = UINT8_C(0x12);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8000), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8001), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x000100), UINT8_C(0x80),
                           &operation) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x000100), UINT8_C(0x80),
                           &repeated) == 0);
    CHECK(trace.count == 2U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0001ff), UINT8_C(0x02),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0001fe), pushed_status,
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00fffe), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00ffff), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x01fd));
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 7U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8100)) == 0);
    cpu.emulation = false;
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x2000);
    cpu.p = MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_C;
    pushed_status = cpu.p;
    bank7e[0x8100] = UINT8_C(0x02); /* COP */
    bank7e[0x8101] = UINT8_C(0xa5);
    bank00[0xffe4] = UINT8_C(0x78);
    bank00[0xffe5] = UINT8_C(0x56);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8100), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8101), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x002000), UINT8_C(0x7e),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x001fff), UINT8_C(0x81),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x001ffe), UINT8_C(0x02),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x001ffd), pushed_status,
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00ffe4), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00ffe5), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x1ffc));
    CHECK(cpu.p == (MIPSNES2_CPU_FLAG_I | MIPSNES2_CPU_FLAG_C));
    CHECK(cpu.instruction_cycles == UINT64_C(8));
    CHECK(trace.count == 8U);
    return 0;
}

static int test_hardware_interrupt_cycles(void)
{
    static const mipsnes2_cpu_operation_kind irq_kinds[] = {
        MIPSNES2_CPU_OPERATION_INTERNAL,
        MIPSNES2_CPU_OPERATION_INTERNAL,
        MIPSNES2_CPU_OPERATION_WRITE,
        MIPSNES2_CPU_OPERATION_WRITE,
        MIPSNES2_CPU_OPERATION_WRITE,
        MIPSNES2_CPU_OPERATION_READ,
        MIPSNES2_CPU_OPERATION_READ
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    size_t step;
    uint8_t pushed_status;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8600)) == 0);
    cpu.s = UINT16_C(0x0100);
    cpu.p |= MIPSNES2_CPU_FLAG_D | MIPSNES2_CPU_FLAG_C;
    pushed_status = (uint8_t)(cpu.p & (uint8_t)~MIPSNES2_CPU_FLAG_X);
    bank00[0xfffa] = UINT8_C(0x34);
    bank00[0xfffb] = UINT8_C(0x12);
    mipsnes2_cpu_pulse_nmi(&cpu);
    mipsnes2_cpu_cycle_state_init(&state);
    CHECK(mipsnes2_cpu_cycle_begin(&state) == MIPSNES2_CPU_CYCLE_READY);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
          MIPSNES2_CPU_CYCLE_READY);
    CHECK(operation.kind == MIPSNES2_CPU_OPERATION_CONTROL);
    CHECK(operation.value != 0U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(!cpu.nmi_pending);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(trace.count == 0U);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x000100), UINT8_C(0x86),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0001ff), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0001fe), pushed_status,
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00fffa), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00fffb), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x01fd));
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 5U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9200)) == 0);
    cpu.emulation = false;
    cpu.pbr = UINT8_C(0x7e);
    cpu.s = UINT16_C(0x2000);
    cpu.p = MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_D |
            MIPSNES2_CPU_FLAG_C;
    pushed_status = cpu.p;
    bank00[0xffe8] = UINT8_C(0x78);
    bank00[0xffe9] = UINT8_C(0x56);
    mipsnes2_cpu_set_irq(&cpu, true);
    mipsnes2_cpu_pulse_nmi(&cpu);
    mipsnes2_cpu_pulse_abort(&cpu);
    mipsnes2_cpu_cycle_state_init(&state);
    CHECK(mipsnes2_cpu_cycle_begin(&state) == MIPSNES2_CPU_CYCLE_READY);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
          MIPSNES2_CPU_CYCLE_READY);
    CHECK(operation.kind == MIPSNES2_CPU_OPERATION_CONTROL);
    CHECK(operation.value != 0U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(!cpu.abort_pending);
    CHECK(cpu.nmi_pending);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x002000), UINT8_C(0x7e),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x001fff), UINT8_C(0x92),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x001ffe), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x001ffd), pushed_status,
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00ffe8), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00ffe9), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x1ffc));
    CHECK(cpu.p == (MIPSNES2_CPU_FLAG_V | MIPSNES2_CPU_FLAG_I |
                    MIPSNES2_CPU_FLAG_C));
    CHECK(cpu.nmi_pending);
    CHECK(cpu.instruction_cycles == UINT64_C(8));
    CHECK(trace.count == 6U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9300)) == 0);
    cpu.s = UINT16_C(0x0100);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_I;
    pushed_status = (uint8_t)(cpu.p & (uint8_t)~MIPSNES2_CPU_FLAG_X);
    bank00[0xfffe] = UINT8_C(0xbc);
    bank00[0xffff] = UINT8_C(0x9a);
    mipsnes2_cpu_set_irq(&cpu, true);
    mipsnes2_cpu_cycle_state_init(&state);
    CHECK(mipsnes2_cpu_cycle_begin(&state) == MIPSNES2_CPU_CYCLE_READY);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
          MIPSNES2_CPU_CYCLE_READY);
    CHECK(operation.kind == MIPSNES2_CPU_OPERATION_CONTROL);
    CHECK(operation.value != 0U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    for (step = 0U; step < sizeof(irq_kinds) / sizeof(irq_kinds[0]); ++step) {
        CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
              MIPSNES2_CPU_CYCLE_READY);
        CHECK(operation.kind == irq_kinds[step]);
        CHECK(commit_operation(
                  &cpu, &state, &operation,
                  step + 1U == sizeof(irq_kinds) / sizeof(irq_kinds[0])
                      ? MIPSNES2_CPU_CYCLE_COMPLETE
                      : MIPSNES2_CPU_CYCLE_READY,
                  step + 1U == sizeof(irq_kinds) / sizeof(irq_kinds[0])
                      ? MIPSNES2_CPU_OK
                      : MIPSNES2_CPU_NOT_READY) == 0);
    }
    CHECK(cpu.pc == UINT16_C(0x9abc));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.s == UINT16_C(0x01fd));
    CHECK(cpu.irq_level);
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 5U);
    CHECK(trace.accesses[0].write);
    CHECK(trace.accesses[0].address == UINT32_C(0x000100));
    CHECK(trace.accesses[0].value == UINT8_C(0x93));
    CHECK(trace.accesses[1].write);
    CHECK(trace.accesses[1].address == UINT32_C(0x0001ff));
    CHECK(trace.accesses[1].value == 0U);
    CHECK(trace.accesses[2].write);
    CHECK(trace.accesses[2].address == UINT32_C(0x0001fe));
    CHECK(trace.accesses[2].value == pushed_status);
    CHECK(!trace.accesses[3].write);
    CHECK(trace.accesses[3].address == UINT32_C(0x00fffe));
    CHECK(!trace.accesses[4].write);
    CHECK(trace.accesses[4].address == UINT32_C(0x00ffff));
    return 0;
}

static int test_boundary_stale_on_interrupt(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_result cpu_result;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8600)) == 0);
    bank00[0xfffa] = UINT8_C(0x34);
    bank00[0xfffb] = UINT8_C(0x12);
    mipsnes2_cpu_cycle_state_init(&state);
    CHECK(mipsnes2_cpu_cycle_begin(&state) == MIPSNES2_CPU_CYCLE_READY);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_CONTROL,
                           0U, 0U, &operation) == 0);
    mipsnes2_cpu_pulse_nmi(&cpu);
    cpu_result = MIPSNES2_CPU_OK;
    CHECK(mipsnes2_cpu_cycle_commit(&cpu, &state, &operation, &cpu_result) ==
          MIPSNES2_CPU_CYCLE_STALE_OPERATION);
    CHECK(cpu_result == MIPSNES2_CPU_NOT_READY);
    CHECK(cpu.nmi_pending);
    CHECK(trace.count == 0U);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
          MIPSNES2_CPU_CYCLE_READY);
    CHECK(operation.kind == MIPSNES2_CPU_OPERATION_CONTROL);
    CHECK(operation.value != 0U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(!cpu.nmi_pending);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x000100), UINT8_C(0x86),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0001ff), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0001fe),
                           MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_I,
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00fffa), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x00fffb), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 5U);
    return 0;
}

static int test_interrupt_missing_write_preflight(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8700)) == 0);
    cpu.bus.write8 = NULL;
    mipsnes2_cpu_pulse_abort(&cpu);
    mipsnes2_cpu_cycle_state_init(&state);
    CHECK(mipsnes2_cpu_cycle_begin(&state) == MIPSNES2_CPU_CYCLE_READY);
    CHECK(mipsnes2_cpu_cycle_peek(&cpu, &state, &operation) ==
          MIPSNES2_CPU_CYCLE_READY);
    CHECK(operation.kind == MIPSNES2_CPU_OPERATION_CONTROL);
    CHECK(operation.value != 0U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_INVALID_BUS) == 0);
    CHECK(cpu.stopped);
    CHECK(cpu.stop_reason == MIPSNES2_CPU_INVALID_BUS);
    CHECK(!cpu.abort_pending);
    CHECK(cpu.pc == UINT16_C(0x8700));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 0U);
    return 0;
}

static int test_jsr_absolute_cycles(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8a00)) == 0);
    cpu.s = UINT16_C(0x0100);
    bank7e[0x8a00] = UINT8_C(0x20);
    bank7e[0x8a01] = UINT8_C(0x34);
    bank7e[0x8a02] = UINT8_C(0x12);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8a00), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8a01), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8a02), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x000100), UINT8_C(0x8a),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0001ff), UINT8_C(0x02),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.s == UINT16_C(0x01fe));
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 5U);
    CHECK(trace.accesses[3].write);
    CHECK(trace.accesses[3].address == UINT32_C(0x000100));
    CHECK(trace.accesses[3].value == UINT8_C(0x8a));
    CHECK(trace.accesses[4].write);
    CHECK(trace.accesses[4].address == UINT32_C(0x0001ff));
    CHECK(trace.accesses[4].value == UINT8_C(0x02));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xfffe)) == 0);
    cpu.emulation = false;
    cpu.s = 0U;
    bank7e[0xfffe] = UINT8_C(0x20);
    bank7e[0xffff] = UINT8_C(0x78);
    bank7e[0x0000] = UINT8_C(0x56);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.s == UINT16_C(0xfffe));
    CHECK(bank00[0x0000] == 0U);
    CHECK(bank00[0xffff] == 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 5U);
    CHECK(trace.accesses[3].write);
    CHECK(trace.accesses[3].address == UINT32_C(0x000000));
    CHECK(trace.accesses[3].value == 0U);
    CHECK(trace.accesses[4].write);
    CHECK(trace.accesses[4].address == UINT32_C(0x00ffff));
    CHECK(trace.accesses[4].value == 0U);
    return 0;
}

static int test_stack_push_cycles(void)
{
    static const uint8_t opcodes[] = {
        UINT8_C(0x08), UINT8_C(0x0b), UINT8_C(0x48), UINT8_C(0x4b),
        UINT8_C(0x5a), UINT8_C(0x8b), UINT8_C(0xda)
    };
    static const uint8_t wide_opcodes[] = {
        UINT8_C(0x48), UINT8_C(0x5a), UINT8_C(0xda)
    };
    expected_operation expected[4];
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(opcodes) / sizeof(opcodes[0]); index++) {
        uint16_t value = 0U;
        bool wide = opcodes[index] == UINT8_C(0x0b);

        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8000)) == 0);
        cpu.s = wide ? UINT16_C(0x0100) : UINT16_C(0x01ff);
        switch (opcodes[index]) {
        case 0x08:
            cpu.p = UINT8_C(0xfd);
            value = cpu.p;
            break;
        case 0x0b:
            cpu.d = UINT16_C(0x1234);
            value = cpu.d;
            break;
        case 0x48:
            cpu.a = UINT16_C(0xab80);
            value = (uint8_t)cpu.a;
            break;
        case 0x4b:
            value = cpu.pbr;
            break;
        case 0x5a:
            cpu.y = UINT16_C(0x005a);
            value = cpu.y;
            break;
        case 0x8b:
            cpu.dbr = UINT8_C(0x80);
            value = cpu.dbr;
            break;
        case 0xda:
            cpu.x = UINT16_C(0x0042);
            value = cpu.x;
            break;
        default:
            CHECK(false);
        }
        bank7e[0x8000] = opcodes[index];
        expected[0] = (expected_operation){
            MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8000), 0U
        };
        expected[1] = (expected_operation){
            MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U
        };
        if (wide) {
            expected[2] = (expected_operation){
                MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x000100),
                (uint8_t)(value >> 8)
            };
            expected[3] = (expected_operation){
                MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x0000ff),
                (uint8_t)value
            };
        } else {
            expected[2] = (expected_operation){
                MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x0001ff),
                (uint8_t)value
            };
        }
        CHECK(run_expected_instruction(&cpu, expected, wide ? 4U : 3U) == 0);
        CHECK(cpu.pc == UINT16_C(0x8001));
        CHECK(cpu.s == UINT16_C(0x01fe));
        CHECK(cpu.instruction_cycles == (wide ? UINT64_C(4) : UINT64_C(3)));
        CHECK(trace.count == (wide ? 3U : 2U));
    }

    for (index = 0U;
         index < sizeof(wide_opcodes) / sizeof(wide_opcodes[0]); index++) {
        uint16_t value = (uint16_t)(UINT16_C(0x8100) + index);

        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8100)) == 0);
        cpu.emulation = false;
        cpu.p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
        cpu.s = 0U;
        if (wide_opcodes[index] == UINT8_C(0x48)) {
            cpu.a = value;
        } else if (wide_opcodes[index] == UINT8_C(0x5a)) {
            cpu.y = value;
        } else {
            cpu.x = value;
        }
        bank7e[0x8100] = wide_opcodes[index];
        expected[0] = (expected_operation){
            MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8100), 0U
        };
        expected[1] = (expected_operation){
            MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U
        };
        expected[2] = (expected_operation){
            MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x000000),
            (uint8_t)(value >> 8)
        };
        expected[3] = (expected_operation){
            MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x00ffff),
            (uint8_t)value
        };
        CHECK(run_expected_instruction(&cpu, expected, 4U) == 0);
        CHECK(cpu.s == UINT16_C(0xfffe));
        CHECK(cpu.instruction_cycles == UINT64_C(4));
        CHECK(trace.count == 3U);
    }
    return 0;
}

static int test_stack_pull_cycles(void)
{
    static const uint8_t opcodes[] = {
        UINT8_C(0x28), UINT8_C(0x2b), UINT8_C(0x68),
        UINT8_C(0x7a), UINT8_C(0xab), UINT8_C(0xfa)
    };
    static const uint8_t wide_opcodes[] = {
        UINT8_C(0x68), UINT8_C(0x7a), UINT8_C(0xfa)
    };
    expected_operation expected[5];
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(opcodes) / sizeof(opcodes[0]); index++) {
        bool wide = opcodes[index] == UINT8_C(0x2b);

        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8200)) == 0);
        cpu.s = wide ? UINT16_C(0x01ff) : UINT16_C(0x01fe);
        cpu.p = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
                MIPSNES2_CPU_FLAG_Z;
        cpu.a = UINT16_C(0xabff);
        cpu.x = UINT16_C(0xabcd);
        cpu.y = UINT16_C(0xef12);
        bank7e[0x8200] = opcodes[index];
        bank00[wide ? 0x0200 : 0x01ff] =
            opcodes[index] == UINT8_C(0x7a) ? UINT8_C(0x80) :
            opcodes[index] == UINT8_C(0xfa) ? UINT8_C(0x7f) :
            opcodes[index] == UINT8_C(0x2b) ? UINT8_C(0x34) : 0U;
        if (wide) {
            bank00[0x0201] = UINT8_C(0x80);
        }
        expected[0] = (expected_operation){
            MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8200), 0U
        };
        expected[1] = (expected_operation){
            MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U
        };
        expected[2] = (expected_operation){
            MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U
        };
        expected[3] = (expected_operation){
            MIPSNES2_CPU_OPERATION_READ,
            wide ? UINT32_C(0x000200) : UINT32_C(0x0001ff), 0U
        };
        if (wide) {
            expected[4] = (expected_operation){
                MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x000201), 0U
            };
        }
        CHECK(run_expected_instruction(&cpu, expected, wide ? 5U : 4U) == 0);
        CHECK(cpu.pc == UINT16_C(0x8201));
        CHECK(cpu.instruction_cycles == (wide ? UINT64_C(5) : UINT64_C(4)));
        CHECK(trace.count == (wide ? 3U : 2U));
        switch (opcodes[index]) {
        case 0x28:
            CHECK(cpu.p == (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X));
            CHECK(cpu.x == UINT16_C(0x00cd));
            CHECK(cpu.y == UINT16_C(0x0012));
            break;
        case 0x2b:
            CHECK(cpu.d == UINT16_C(0x8034));
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
            CHECK(cpu.s == UINT16_C(0x0101));
            break;
        case 0x68:
            CHECK(cpu.a == UINT16_C(0xab00));
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_Z) != 0U);
            break;
        case 0x7a:
            CHECK(cpu.y == UINT16_C(0x0080));
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
            break;
        case 0xab:
            CHECK(cpu.dbr == 0U);
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_Z) != 0U);
            break;
        case 0xfa:
            CHECK(cpu.x == UINT16_C(0x007f));
            CHECK((cpu.p & (MIPSNES2_CPU_FLAG_N |
                            MIPSNES2_CPU_FLAG_Z)) == 0U);
            break;
        default:
            CHECK(false);
        }
    }

    for (index = 0U;
         index < sizeof(wide_opcodes) / sizeof(wide_opcodes[0]); index++) {
        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8300)) == 0);
        cpu.emulation = false;
        cpu.p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X |
                            MIPSNES2_CPU_FLAG_N | MIPSNES2_CPU_FLAG_Z);
        cpu.s = UINT16_C(0xfffe);
        bank7e[0x8300] = wide_opcodes[index];
        bank00[0xffff] = UINT8_C(0x01);
        bank00[0x0000] = UINT8_C(0x80);
        expected[0] = (expected_operation){
            MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8300), 0U
        };
        expected[1] = (expected_operation){
            MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U
        };
        expected[2] = (expected_operation){
            MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U
        };
        expected[3] = (expected_operation){
            MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x00ffff), 0U
        };
        expected[4] = (expected_operation){
            MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x000000), 0U
        };
        CHECK(run_expected_instruction(&cpu, expected, 5U) == 0);
        CHECK(cpu.s == 0U);
        CHECK(cpu.instruction_cycles == UINT64_C(5));
        CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
        if (wide_opcodes[index] == UINT8_C(0x68)) {
            CHECK(cpu.a == UINT16_C(0x8001));
        } else if (wide_opcodes[index] == UINT8_C(0x7a)) {
            CHECK(cpu.y == UINT16_C(0x8001));
        } else {
            CHECK(cpu.x == UINT16_C(0x8001));
        }
    }
    return 0;
}

static int test_stack_pull_stale_internal(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;
    uint64_t cycles_before;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8380)) == 0);
    cpu.s = UINT16_C(0x01fe);
    bank7e[0x8380] = UINT8_C(0x28);
    bank00[0x01ff] = UINT8_C(0xff);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8380), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    stale = operation;
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    cycles_before = cpu.instruction_cycles;
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.instruction_cycles == cycles_before);
    CHECK(cpu.s == UINT16_C(0x01fe));
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x0001ff), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.s == UINT16_C(0x01ff));
    CHECK(cpu.instruction_cycles == UINT64_C(4));
    CHECK(trace.count == 2U);
    return 0;
}

static int test_effective_address_push_cycles(void)
{
    static const expected_operation pea[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8400), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8401), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8402), 0U },
        { MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x000100), UINT8_C(0x12) },
        { MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x0000ff), UINT8_C(0x34) }
    };
    static const expected_operation per[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8500), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8501), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8502), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U },
        { MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x000100), UINT8_C(0x84) },
        { MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x0000ff), UINT8_C(0xff) }
    };
    static const expected_operation pei_aligned[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8600), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8601), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x0002ff), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x000200), 0U },
        { MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x000100), UINT8_C(0x56) },
        { MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x0000ff), UINT8_C(0x78) }
    };
    static const expected_operation pei_unaligned[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8700), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8701), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x0012ff), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x001300), 0U },
        { MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x004000), UINT8_C(0xab) },
        { MIPSNES2_CPU_OPERATION_WRITE, UINT32_C(0x003fff), UINT8_C(0xcd) }
    };
    trace_context trace;
    mipsnes2_cpu cpu;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8400)) == 0);
    cpu.s = UINT16_C(0x0100);
    bank7e[0x8400] = UINT8_C(0xf4);
    bank7e[0x8401] = UINT8_C(0x34);
    bank7e[0x8402] = UINT8_C(0x12);
    CHECK(run_expected_instruction(&cpu, pea, sizeof(pea) / sizeof(pea[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x8403));
    CHECK(cpu.s == UINT16_C(0x01fe));
    CHECK(cpu.instruction_cycles == UINT64_C(5));
    CHECK(trace.count == 5U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8500)) == 0);
    cpu.s = UINT16_C(0x0100);
    bank7e[0x8500] = UINT8_C(0x62);
    bank7e[0x8501] = UINT8_C(0xfc);
    bank7e[0x8502] = UINT8_C(0xff);
    CHECK(run_expected_instruction(&cpu, per, sizeof(per) / sizeof(per[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x8503));
    CHECK(cpu.s == UINT16_C(0x01fe));
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 5U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8600)) == 0);
    cpu.s = UINT16_C(0x0100);
    cpu.d = UINT16_C(0x0200);
    bank7e[0x8600] = UINT8_C(0xd4);
    bank7e[0x8601] = UINT8_C(0xff);
    bank00[0x02ff] = UINT8_C(0x78);
    bank00[0x0200] = UINT8_C(0x56);
    CHECK(run_expected_instruction(
              &cpu, pei_aligned,
              sizeof(pei_aligned) / sizeof(pei_aligned[0])) == 0);
    CHECK(cpu.s == UINT16_C(0x01fe));
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 6U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8700)) == 0);
    cpu.emulation = false;
    cpu.p &= (uint8_t)~(MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X);
    cpu.s = UINT16_C(0x4000);
    cpu.d = UINT16_C(0x1201);
    bank7e[0x8700] = UINT8_C(0xd4);
    bank7e[0x8701] = UINT8_C(0xfe);
    bank00[0x12ff] = UINT8_C(0xcd);
    bank00[0x1300] = UINT8_C(0xab);
    CHECK(run_expected_instruction(
              &cpu, pei_unaligned,
              sizeof(pei_unaligned) / sizeof(pei_unaligned[0])) == 0);
    CHECK(cpu.s == UINT16_C(0x3ffe));
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 6U);
    return 0;
}

static int test_stack_missing_write_fallback(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8800)) == 0);
    cpu.bus.write8 = NULL;
    cpu.s = UINT16_C(0x01ff);
    cpu.a = UINT16_C(0x0080);
    bank7e[0x8800] = UINT8_C(0x48);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8800), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state,
                           MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_INVALID_BUS) == 0);
    CHECK(cpu.stopped);
    CHECK(cpu.s == UINT16_C(0x01ff));
    CHECK(cpu.pc == UINT16_C(0x8801));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 1U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8810)) == 0);
    cpu.bus.write8 = NULL;
    cpu.s = UINT16_C(0x0100);
    bank7e[0x8810] = UINT8_C(0xf4);
    bank7e[0x8811] = UINT8_C(0x34);
    bank7e[0x8812] = UINT8_C(0x12);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8810), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state,
                           MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_INVALID_BUS) == 0);
    CHECK(cpu.stopped);
    CHECK(cpu.s == UINT16_C(0x0100));
    CHECK(cpu.pc == UINT16_C(0x8811));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 1U);
    return 0;
}

static int run_implied_instruction(mipsnes2_cpu *cpu, uint16_t pc,
                                   uint8_t opcode)
{
    expected_operation expected[3];
    size_t count = opcode == UINT8_C(0xeb) ? 3U : 2U;

    bank7e[pc] = opcode;
    expected[0] = (expected_operation){
        MIPSNES2_CPU_OPERATION_READ,
        mipsnes2_address_make(UINT8_C(0x7e), pc), 0U
    };
    expected[1] = (expected_operation){
        MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U
    };
    expected[2] = (expected_operation){
        MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U
    };
    return run_expected_instruction(cpu, expected, count);
}

static int test_implied_status_cycles(void)
{
    static const struct {
        uint8_t opcode;
        uint8_t initial;
        uint8_t expected;
    } cases[] = {
        { UINT8_C(0x18), UINT8_C(0xff), UINT8_C(0xfe) },
        { UINT8_C(0x38), UINT8_C(0x30), UINT8_C(0x31) },
        { UINT8_C(0x58), UINT8_C(0xff), UINT8_C(0xfb) },
        { UINT8_C(0x78), UINT8_C(0x30), UINT8_C(0x34) },
        { UINT8_C(0xb8), UINT8_C(0xff), UINT8_C(0xbf) },
        { UINT8_C(0xd8), UINT8_C(0xff), UINT8_C(0xf7) },
        { UINT8_C(0xf8), UINT8_C(0x30), UINT8_C(0x38) }
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8900)) == 0);
        cpu.p = cases[index].initial;
        CHECK(run_implied_instruction(&cpu, UINT16_C(0x8900),
                                      cases[index].opcode) == 0);
        CHECK(cpu.p == cases[index].expected);
        CHECK(cpu.pc == UINT16_C(0x8901));
        CHECK(cpu.instruction_cycles == UINT64_C(2));
        CHECK(trace.count == 1U);
    }

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8910)) == 0);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
    CHECK(run_implied_instruction(&cpu, UINT16_C(0x8910),
                                  UINT8_C(0xfb)) == 0);
    CHECK(!cpu.emulation);
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_C) != 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(2));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8920)) == 0);
    cpu.emulation = false;
    cpu.p = MIPSNES2_CPU_FLAG_C;
    cpu.s = UINT16_C(0xabcd);
    cpu.x = UINT16_C(0x1234);
    cpu.y = UINT16_C(0x5678);
    CHECK(run_implied_instruction(&cpu, UINT16_C(0x8920),
                                  UINT8_C(0xfb)) == 0);
    CHECK(cpu.emulation);
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_C) == 0U);
    CHECK((cpu.p & (MIPSNES2_CPU_FLAG_M |
                    MIPSNES2_CPU_FLAG_X)) ==
          (MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X));
    CHECK(cpu.s == UINT16_C(0x01cd));
    CHECK(cpu.x == UINT16_C(0x0034));
    CHECK(cpu.y == UINT16_C(0x0078));
    CHECK(cpu.instruction_cycles == UINT64_C(2));
    return 0;
}

static int test_implied_transfer_cycles(void)
{
    static const uint8_t opcodes[] = {
        UINT8_C(0x1b), UINT8_C(0x3b), UINT8_C(0x5b), UINT8_C(0x7b),
        UINT8_C(0x8a), UINT8_C(0x98), UINT8_C(0x9a), UINT8_C(0x9b),
        UINT8_C(0xa8), UINT8_C(0xaa), UINT8_C(0xba), UINT8_C(0xbb)
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(opcodes) / sizeof(opcodes[0]); index++) {
        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8930)) == 0);
        cpu.emulation = false;
        cpu.p = 0U;
        switch (opcodes[index]) {
        case 0x1b:
            cpu.a = UINT16_C(0x8001);
            break;
        case 0x3b:
            cpu.s = UINT16_C(0x8001);
            break;
        case 0x5b:
            cpu.a = 0U;
            cpu.d = UINT16_C(0xffff);
            break;
        case 0x7b:
            cpu.d = UINT16_C(0x8001);
            break;
        case 0x8a:
            cpu.x = UINT16_C(0x8001);
            break;
        case 0x98:
            cpu.y = 0U;
            cpu.a = UINT16_C(0xffff);
            break;
        case 0x9a:
            cpu.x = UINT16_C(0x4567);
            break;
        case 0x9b:
            cpu.x = UINT16_C(0x8001);
            break;
        case 0xa8:
            cpu.a = 0U;
            cpu.y = UINT16_C(0xffff);
            break;
        case 0xaa:
            cpu.a = UINT16_C(0x8001);
            break;
        case 0xba:
            cpu.s = UINT16_C(0x8001);
            break;
        case 0xbb:
            cpu.y = 0U;
            cpu.x = UINT16_C(0xffff);
            break;
        default:
            CHECK(false);
        }
        CHECK(run_implied_instruction(&cpu, UINT16_C(0x8930),
                                      opcodes[index]) == 0);
        CHECK(cpu.instruction_cycles == UINT64_C(2));
        CHECK(trace.count == 1U);
        switch (opcodes[index]) {
        case 0x1b:
            CHECK(cpu.s == UINT16_C(0x8001));
            CHECK(cpu.p == 0U);
            break;
        case 0x3b:
            CHECK(cpu.a == UINT16_C(0x8001));
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_N);
            break;
        case 0x5b:
            CHECK(cpu.d == 0U);
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_Z);
            break;
        case 0x7b:
            CHECK(cpu.a == UINT16_C(0x8001));
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_N);
            break;
        case 0x8a:
            CHECK(cpu.a == UINT16_C(0x8001));
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_N);
            break;
        case 0xaa:
            CHECK(cpu.x == UINT16_C(0x8001));
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_N);
            break;
        case 0x98:
            CHECK(cpu.a == 0U);
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_Z);
            break;
        case 0x9a:
            CHECK(cpu.s == UINT16_C(0x4567));
            CHECK(cpu.p == 0U);
            break;
        case 0x9b:
            CHECK(cpu.y == UINT16_C(0x8001));
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_N);
            break;
        case 0xa8:
            CHECK(cpu.y == 0U);
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_Z);
            break;
        case 0xba:
            CHECK(cpu.x == UINT16_C(0x8001));
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_N);
            break;
        case 0xbb:
            CHECK(cpu.x == 0U);
            CHECK(cpu.p == MIPSNES2_CPU_FLAG_Z);
            break;
        default:
            CHECK(false);
        }
    }

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8940)) == 0);
    cpu.emulation = false;
    cpu.p = MIPSNES2_CPU_FLAG_M;
    cpu.a = UINT16_C(0x8000);
    CHECK(run_implied_instruction(&cpu, UINT16_C(0x8940),
                                  UINT8_C(0xaa)) == 0);
    CHECK(cpu.x == UINT16_C(0x8000));
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8950)) == 0);
    cpu.emulation = false;
    cpu.p = MIPSNES2_CPU_FLAG_X;
    cpu.a = UINT16_C(0x1280);
    CHECK(run_implied_instruction(&cpu, UINT16_C(0x8950),
                                  UINT8_C(0xaa)) == 0);
    CHECK(cpu.x == UINT16_C(0x0080));
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
    return 0;
}

static int test_implied_increment_cycles(void)
{
    static const uint8_t opcodes[] = {
        UINT8_C(0x1a), UINT8_C(0x3a), UINT8_C(0x88),
        UINT8_C(0xc8), UINT8_C(0xca), UINT8_C(0xe8)
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(opcodes) / sizeof(opcodes[0]); index++) {
        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8960)) == 0);
        cpu.p = MIPSNES2_CPU_FLAG_M | MIPSNES2_CPU_FLAG_X;
        cpu.a = opcodes[index] == UINT8_C(0x1a)
                    ? UINT16_C(0xabff) : UINT16_C(0xab00);
        cpu.x = opcodes[index] == UINT8_C(0xe8)
                    ? UINT16_C(0x007f) : 0U;
        cpu.y = opcodes[index] == UINT8_C(0x88)
                    ? UINT16_C(0x0001) : UINT16_C(0x00ff);
        CHECK(run_implied_instruction(&cpu, UINT16_C(0x8960),
                                      opcodes[index]) == 0);
        CHECK(cpu.instruction_cycles == UINT64_C(2));
        CHECK(trace.count == 1U);
        switch (opcodes[index]) {
        case 0x1a:
            CHECK(cpu.a == UINT16_C(0xab00));
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_Z) != 0U);
            break;
        case 0x3a:
            CHECK(cpu.a == UINT16_C(0xabff));
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
            break;
        case 0x88:
            CHECK(cpu.y == 0U);
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_Z) != 0U);
            break;
        case 0xc8:
            CHECK(cpu.y == 0U);
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_Z) != 0U);
            break;
        case 0xca:
            CHECK(cpu.x == UINT16_C(0x00ff));
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
            break;
        case 0xe8:
            CHECK(cpu.x == UINT16_C(0x0080));
            CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
            break;
        default:
            CHECK(false);
        }
    }

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8970)) == 0);
    cpu.emulation = false;
    cpu.p = 0U;
    cpu.a = UINT16_C(0x7fff);
    CHECK(run_implied_instruction(&cpu, UINT16_C(0x8970),
                                  UINT8_C(0x1a)) == 0);
    CHECK(cpu.a == UINT16_C(0x8000));
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8980)) == 0);
    cpu.emulation = false;
    cpu.p = 0U;
    cpu.x = UINT16_C(0xffff);
    CHECK(run_implied_instruction(&cpu, UINT16_C(0x8980),
                                  UINT8_C(0xe8)) == 0);
    CHECK(cpu.x == 0U);
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_Z) != 0U);
    return 0;
}

static int test_xba_stale_internal(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;
    uint64_t cycles_before;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8990)) == 0);
    cpu.a = UINT16_C(0x8012);
    bank7e[0x8990] = UINT8_C(0xeb);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8990), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    stale = operation;
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.a == UINT16_C(0x8012));
    cycles_before = cpu.instruction_cycles;
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.instruction_cycles == cycles_before);
    CHECK(cpu.a == UINT16_C(0x8012));
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.a == UINT16_C(0x1280));
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_N) != 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(trace.count == 1U);
    return 0;
}

static int test_conditional_branch_cycles(void)
{
    static const struct {
        uint8_t opcode;
        uint8_t flag;
        bool branch_if_set;
    } cases[] = {
        { UINT8_C(0x10), MIPSNES2_CPU_FLAG_N, false }, /* BPL */
        { UINT8_C(0x30), MIPSNES2_CPU_FLAG_N, true },  /* BMI */
        { UINT8_C(0x50), MIPSNES2_CPU_FLAG_V, false }, /* BVC */
        { UINT8_C(0x70), MIPSNES2_CPU_FLAG_V, true },  /* BVS */
        { UINT8_C(0x90), MIPSNES2_CPU_FLAG_C, false }, /* BCC */
        { UINT8_C(0xb0), MIPSNES2_CPU_FLAG_C, true },  /* BCS */
        { UINT8_C(0xd0), MIPSNES2_CPU_FLAG_Z, false }, /* BNE */
        { UINT8_C(0xf0), MIPSNES2_CPU_FLAG_Z, true }   /* BEQ */
    };
    static const expected_operation taken[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9000), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9001), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U }
    };
    static const expected_operation not_taken[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9000), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9001), 0U }
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); index++) {
        uint8_t status;

        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9000)) == 0);
        status = cases[index].branch_if_set
                     ? (uint8_t)(cpu.p | cases[index].flag)
                     : (uint8_t)(cpu.p & (uint8_t)~cases[index].flag);
        cpu.p = status;
        bank7e[0x9000] = cases[index].opcode;
        bank7e[0x9001] = UINT8_C(0x02);
        CHECK(run_expected_instruction(
                  &cpu, taken, sizeof(taken) / sizeof(taken[0])) == 0);
        CHECK(cpu.pc == UINT16_C(0x9004));
        CHECK(cpu.pbr == UINT8_C(0x7e));
        CHECK(cpu.p == status);
        CHECK(cpu.instruction_cycles == UINT64_C(3));
        CHECK(trace.count == 2U);

        CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9000)) == 0);
        status = cases[index].branch_if_set
                     ? (uint8_t)(cpu.p & (uint8_t)~cases[index].flag)
                     : (uint8_t)(cpu.p | cases[index].flag);
        cpu.p = status;
        bank7e[0x9000] = cases[index].opcode;
        bank7e[0x9001] = UINT8_C(0x02);
        CHECK(run_expected_instruction(
                  &cpu, not_taken,
                  sizeof(not_taken) / sizeof(not_taken[0])) == 0);
        CHECK(cpu.pc == UINT16_C(0x9002));
        CHECK(cpu.pbr == UINT8_C(0x7e));
        CHECK(cpu.p == status);
        CHECK(cpu.instruction_cycles == UINT64_C(2));
        CHECK(trace.count == 2U);
    }
    return 0;
}

static int test_branch_boundary_cycles(void)
{
    static const expected_operation native_cross[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e80fd), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e80fe), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U }
    };
    static const expected_operation not_taken_cross[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e80fd), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e80fe), 0U }
    };
    static const expected_operation wrapping_branch[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7efffe), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7effff), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 2U }
    };
    static const expected_operation bra[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9200), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9201), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U }
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;
    uint64_t cycles_before;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x80fd)) == 0);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
    bank7e[0x80fd] = UINT8_C(0x90);
    bank7e[0x80fe] = UINT8_C(0x01);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e80fd), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e80fe), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    stale = operation;
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.pc == UINT16_C(0x80ff));
    cycles_before = cpu.instruction_cycles;
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.instruction_cycles == cycles_before);
    CHECK(cpu.pc == UINT16_C(0x80ff));
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x8100));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.instruction_cycles == UINT64_C(4));
    CHECK(trace.count == 2U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x80fd)) == 0);
    cpu.emulation = false;
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_C;
    bank7e[0x80fd] = UINT8_C(0x90);
    bank7e[0x80fe] = UINT8_C(0x01);
    CHECK(run_expected_instruction(
              &cpu, native_cross,
              sizeof(native_cross) / sizeof(native_cross[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x8100));
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(trace.count == 2U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x80fd)) == 0);
    cpu.p |= MIPSNES2_CPU_FLAG_C;
    bank7e[0x80fd] = UINT8_C(0x90);
    bank7e[0x80fe] = UINT8_C(0x01);
    CHECK(run_expected_instruction(
              &cpu, not_taken_cross,
              sizeof(not_taken_cross) / sizeof(not_taken_cross[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x80ff));
    CHECK(cpu.instruction_cycles == UINT64_C(2));
    CHECK(trace.count == 2U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xfffe)) == 0);
    cpu.p &= (uint8_t)~MIPSNES2_CPU_FLAG_Z;
    bank7e[0xfffe] = UINT8_C(0xd0);
    bank7e[0xffff] = UINT8_C(0xfe);
    CHECK(run_expected_instruction(
              &cpu, wrapping_branch,
              sizeof(wrapping_branch) / sizeof(wrapping_branch[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0xfffe));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.instruction_cycles == UINT64_C(4));
    CHECK(trace.count == 2U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9200)) == 0);
    bank7e[0x9200] = UINT8_C(0x80);
    bank7e[0x9201] = UINT8_C(0xfe);
    CHECK(run_expected_instruction(
              &cpu, bra, sizeof(bra) / sizeof(bra[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x9200));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(trace.count == 2U);
    return 0;
}

static int test_branch_long_cycles(void)
{
    static const expected_operation positive[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e0000), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e0001), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e0002), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U }
    };
    static const expected_operation negative[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8100), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8101), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e8102), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U }
    };
    static const expected_operation operand_wrap[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7efffd), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7efffe), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7effff), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U }
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    uint8_t status;

    CHECK(initialize_cpu(&cpu, &trace, 0U) == 0);
    status = cpu.p;
    bank7e[0x0000] = UINT8_C(0x82);
    bank7e[0x0001] = UINT8_C(0xff);
    bank7e[0x0002] = UINT8_C(0x7f);
    CHECK(run_expected_instruction(
              &cpu, positive, sizeof(positive) / sizeof(positive[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x8002));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.p == status);
    CHECK(cpu.instruction_cycles == UINT64_C(4));
    CHECK(trace.count == 3U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8100)) == 0);
    status = cpu.p;
    bank7e[0x8100] = UINT8_C(0x82);
    bank7e[0x8101] = 0U;
    bank7e[0x8102] = UINT8_C(0x80);
    CHECK(run_expected_instruction(
              &cpu, negative, sizeof(negative) / sizeof(negative[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x0103));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.p == status);
    CHECK(cpu.instruction_cycles == UINT64_C(4));
    CHECK(trace.count == 3U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xfffd)) == 0);
    bank7e[0xfffd] = UINT8_C(0x82);
    bank7e[0xfffe] = UINT8_C(0xfe);
    bank7e[0xffff] = UINT8_C(0xff);
    CHECK(run_expected_instruction(
              &cpu, operand_wrap,
              sizeof(operand_wrap) / sizeof(operand_wrap[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0xfffe));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.instruction_cycles == UINT64_C(4));
    CHECK(trace.count == 3U);
    return 0;
}

static int test_jump_direct_cycles(void)
{
    static const expected_operation long_jump[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7efffd), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7efffe), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7effff), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e0000), 0U }
    };
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xfffe)) == 0);
    bank7e[0xfffe] = UINT8_C(0x4c);
    bank7e[0xffff] = UINT8_C(0x34);
    bank7e[0x0000] = UINT8_C(0x12);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7efffe), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7effff), 0U, &operation) == 0);
    stale = operation;
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e0000), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.instruction_cycles == UINT64_C(3));
    CHECK(trace.count == 3U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xfffd)) == 0);
    bank7e[0xfffd] = UINT8_C(0x5c);
    bank7e[0xfffe] = UINT8_C(0x78);
    bank7e[0xffff] = UINT8_C(0x56);
    bank7e[0x0000] = UINT8_C(0x7f);
    CHECK(run_expected_instruction(
              &cpu, long_jump,
              sizeof(long_jump) / sizeof(long_jump[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == UINT8_C(0x7f));
    CHECK(cpu.instruction_cycles == UINT64_C(4));
    CHECK(trace.count == 4U);
    return 0;
}

static int test_jump_indirect_cycles(void)
{
    static const expected_operation indirect_jump[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9900), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9901), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9902), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x00ffff), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x000000), 0U }
    };
    static const expected_operation indexed_jump[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9a00), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9a01), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9a02), 0U },
        { MIPSNES2_CPU_OPERATION_INTERNAL, 0U, 1U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7effff), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e0000), 0U }
    };
    static const expected_operation indirect_long_jump[] = {
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9b00), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9b01), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x7e9b02), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x00fffe), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x00ffff), 0U },
        { MIPSNES2_CPU_OPERATION_READ, UINT32_C(0x000000), 0U }
    };
    trace_context trace;
    mipsnes2_cpu cpu;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9900)) == 0);
    bank7e[0x9900] = UINT8_C(0x6c);
    bank7e[0x9901] = UINT8_C(0xff);
    bank7e[0x9902] = UINT8_C(0xff);
    bank00[0xffff] = UINT8_C(0x34);
    bank00[0x0000] = UINT8_C(0x12);
    CHECK(run_expected_instruction(
              &cpu, indirect_jump,
              sizeof(indirect_jump) / sizeof(indirect_jump[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.instruction_cycles == UINT64_C(5));
    CHECK(trace.count == 5U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9a00)) == 0);
    cpu.x = UINT16_C(0x0001);
    bank7e[0x9a00] = UINT8_C(0x7c);
    bank7e[0x9a01] = UINT8_C(0xfe);
    bank7e[0x9a02] = UINT8_C(0xff);
    bank7e[0xffff] = UINT8_C(0x78);
    bank7e[0x0000] = UINT8_C(0x56);
    CHECK(run_expected_instruction(
              &cpu, indexed_jump,
              sizeof(indexed_jump) / sizeof(indexed_jump[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 5U);

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9b00)) == 0);
    bank7e[0x9b00] = UINT8_C(0xdc);
    bank7e[0x9b01] = UINT8_C(0xfe);
    bank7e[0x9b02] = UINT8_C(0xff);
    bank00[0xfffe] = UINT8_C(0xbc);
    bank00[0xffff] = UINT8_C(0x9a);
    bank00[0x0000] = UINT8_C(0x7f);
    CHECK(run_expected_instruction(
              &cpu, indirect_long_jump,
              sizeof(indirect_long_jump) /
                  sizeof(indirect_long_jump[0])) == 0);
    CHECK(cpu.pc == UINT16_C(0x9abc));
    CHECK(cpu.pbr == UINT8_C(0x7f));
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 6U);
    return 0;
}

static int test_rts_cycles(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9000)) == 0);
    cpu.s = UINT16_C(0x01fe);
    bank7e[0x9000] = UINT8_C(0x60);
    bank00[0x01ff] = UINT8_C(0xfe);
    bank00[0x0100] = UINT8_C(0x8a);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9000), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    stale = operation;
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.instruction_cycles == UINT64_C(2));
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x0001ff), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x000100), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 3U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x8aff));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.s == UINT16_C(0x0100));
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 3U);
    CHECK(!trace.accesses[1].write);
    CHECK(trace.accesses[1].address == UINT32_C(0x0001ff));
    CHECK(!trace.accesses[2].write);
    CHECK(trace.accesses[2].address == UINT32_C(0x000100));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9100)) == 0);
    cpu.emulation = false;
    cpu.s = UINT16_C(0xfffe);
    bank7e[0x9100] = UINT8_C(0x60);
    bank00[0xffff] = UINT8_C(0xff);
    bank00[0x0000] = UINT8_C(0x7f);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x8000));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.s == 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 3U);
    CHECK(trace.accesses[1].address == UINT32_C(0x00ffff));
    CHECK(trace.accesses[2].address == UINT32_C(0x000000));
    return 0;
}

static int test_jsr_missing_write_fallback(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9200)) == 0);
    cpu.s = UINT16_C(0x0100);
    cpu.bus.write8 = NULL;
    bank7e[0x9200] = UINT8_C(0x20);
    bank7e[0x9201] = UINT8_C(0x34);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9200), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state,
                           MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_INVALID_BUS) == 0);
    CHECK(cpu.stopped);
    CHECK(cpu.stop_reason == MIPSNES2_CPU_INVALID_BUS);
    CHECK(cpu.pc == UINT16_C(0x9201));
    CHECK(cpu.s == UINT16_C(0x0100));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 1U);
    return 0;
}

static int test_jsr_indexed_indirect_cycles(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9700)) == 0);
    cpu.s = UINT16_C(0x0100);
    cpu.x = UINT16_C(0x0001);
    bank7e[0x9700] = UINT8_C(0xfc);
    bank7e[0x9701] = UINT8_C(0xfe);
    bank7e[0x9702] = UINT8_C(0xff);
    bank7e[0xffff] = UINT8_C(0x34);
    bank7e[0x0000] = UINT8_C(0x12);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9700), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9701), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x000100), UINT8_C(0x97),
                           &operation) == 0);
    stale = operation;
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.s == UINT16_C(0x00ff));
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0000ff), UINT8_C(0x02),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.s == UINT16_C(0x01fe));
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9702), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7effff), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e0000), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.s == UINT16_C(0x01fe));
    CHECK(cpu.instruction_cycles == UINT64_C(8));
    CHECK(trace.count == 7U);
    CHECK(trace.accesses[2].write);
    CHECK(trace.accesses[2].address == UINT32_C(0x000100));
    CHECK(trace.accesses[2].value == UINT8_C(0x97));
    CHECK(trace.accesses[3].write);
    CHECK(trace.accesses[3].address == UINT32_C(0x0000ff));
    CHECK(trace.accesses[3].value == UINT8_C(0x02));
    CHECK(trace.accesses[4].address == UINT32_C(0x7e9702));
    CHECK(trace.accesses[5].address == UINT32_C(0x7effff));
    CHECK(trace.accesses[6].address == UINT32_C(0x7e0000));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xffff)) == 0);
    cpu.emulation = false;
    cpu.s = 0U;
    cpu.x = 0U;
    bank7e[0xffff] = UINT8_C(0xfc);
    bank7e[0x0000] = UINT8_C(0x00);
    bank7e[0x0001] = UINT8_C(0x20);
    bank7e[0x2000] = UINT8_C(0x78);
    bank7e[0x2001] = UINT8_C(0x56);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == UINT8_C(0x7e));
    CHECK(cpu.s == UINT16_C(0xfffe));
    CHECK(cpu.instruction_cycles == UINT64_C(8));
    CHECK(trace.count == 7U);
    CHECK(trace.accesses[1].address == UINT32_C(0x7e0000));
    CHECK(trace.accesses[2].write);
    CHECK(trace.accesses[2].address == UINT32_C(0x000000));
    CHECK(trace.accesses[2].value == 0U);
    CHECK(trace.accesses[3].write);
    CHECK(trace.accesses[3].address == UINT32_C(0x00ffff));
    CHECK(trace.accesses[3].value == UINT8_C(0x01));
    CHECK(trace.accesses[4].address == UINT32_C(0x7e0001));
    CHECK(trace.accesses[5].address == UINT32_C(0x7e2000));
    CHECK(trace.accesses[6].address == UINT32_C(0x7e2001));
    return 0;
}

static int test_jsr_indexed_missing_write_fallback(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9800)) == 0);
    cpu.s = UINT16_C(0x0100);
    cpu.bus.write8 = NULL;
    bank7e[0x9800] = UINT8_C(0xfc);
    bank7e[0x9801] = UINT8_C(0x34);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9800), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state,
                           MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_INVALID_BUS) == 0);
    CHECK(cpu.stopped);
    CHECK(cpu.stop_reason == MIPSNES2_CPU_INVALID_BUS);
    CHECK(cpu.pc == UINT16_C(0x9801));
    CHECK(cpu.s == UINT16_C(0x0100));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 1U);
    return 0;
}

static int test_jsl_absolute_long_cycles(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9300)) == 0);
    cpu.s = UINT16_C(0x0100);
    bank7e[0x9300] = UINT8_C(0x22);
    bank7e[0x9301] = UINT8_C(0x78);
    bank7e[0x9302] = UINT8_C(0x56);
    bank7e[0x9303] = UINT8_C(0x7f);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9300), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9301), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9302), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x000100), UINT8_C(0x7e),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.s == UINT16_C(0x00ff));
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9303), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0000ff), UINT8_C(0x93),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_WRITE,
                           UINT32_C(0x0000fe), UINT8_C(0x03),
                           &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0x5678));
    CHECK(cpu.pbr == UINT8_C(0x7f));
    CHECK(cpu.s == UINT16_C(0x01fd));
    CHECK(cpu.instruction_cycles == UINT64_C(8));
    CHECK(trace.count == 7U);
    CHECK(trace.accesses[3].write);
    CHECK(trace.accesses[3].address == UINT32_C(0x000100));
    CHECK(trace.accesses[4].address == UINT32_C(0x7e9303));
    CHECK(trace.accesses[5].address == UINT32_C(0x0000ff));
    CHECK(trace.accesses[6].address == UINT32_C(0x0000fe));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0xfffd)) == 0);
    cpu.emulation = false;
    cpu.s = 0U;
    bank7e[0xfffd] = UINT8_C(0x22);
    bank7e[0xfffe] = UINT8_C(0x34);
    bank7e[0xffff] = UINT8_C(0x12);
    bank7e[0x0000] = UINT8_C(0x7f);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == UINT8_C(0x7f));
    CHECK(cpu.s == UINT16_C(0xfffd));
    CHECK(cpu.instruction_cycles == UINT64_C(8));
    CHECK(trace.count == 7U);
    CHECK(trace.accesses[3].address == UINT32_C(0x000000));
    CHECK(trace.accesses[3].value == UINT8_C(0x7e));
    CHECK(trace.accesses[4].address == UINT32_C(0x7e0000));
    CHECK(trace.accesses[5].address == UINT32_C(0x00ffff));
    CHECK(trace.accesses[5].value == 0U);
    CHECK(trace.accesses[6].address == UINT32_C(0x00fffe));
    CHECK(trace.accesses[6].value == 0U);
    return 0;
}

static int test_rtl_cycles(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_operation stale;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9400)) == 0);
    cpu.s = UINT16_C(0x01ff);
    bank7e[0x9400] = UINT8_C(0x6b);
    bank00[0x0200] = UINT8_C(0xfe);
    bank00[0x0201] = UINT8_C(0xff);
    bank00[0x0202] = UINT8_C(0x7f);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9400), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 1U, &operation) == 0);
    stale = operation;
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(commit_operation(&cpu, &state, &stale,
                           MIPSNES2_CPU_CYCLE_STALE_OPERATION,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_INTERNAL,
                           0U, 2U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x000200), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x000201), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x000202), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(cpu.pc == UINT16_C(0xffff));
    CHECK(cpu.pbr == UINT8_C(0x7f));
    CHECK(cpu.s == UINT16_C(0x0102));
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 4U);
    CHECK(trace.accesses[1].address == UINT32_C(0x000200));
    CHECK(trace.accesses[2].address == UINT32_C(0x000201));
    CHECK(trace.accesses[3].address == UINT32_C(0x000202));

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9500)) == 0);
    cpu.emulation = false;
    cpu.s = UINT16_C(0xfffd);
    bank7e[0x9500] = UINT8_C(0x6b);
    bank00[0xfffe] = UINT8_C(0xff);
    bank00[0xffff] = UINT8_C(0xff);
    bank00[0x0000] = UINT8_C(0x7f);
    CHECK(mipsnes2_cpu_step(&cpu) == MIPSNES2_CPU_OK);
    CHECK(cpu.pc == 0U);
    CHECK(cpu.pbr == UINT8_C(0x7f));
    CHECK(cpu.s == 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(6));
    CHECK(trace.count == 4U);
    CHECK(trace.accesses[1].address == UINT32_C(0x00fffe));
    CHECK(trace.accesses[2].address == UINT32_C(0x00ffff));
    CHECK(trace.accesses[3].address == UINT32_C(0x000000));
    return 0;
}

static int test_jsl_missing_write_fallback(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x9600)) == 0);
    cpu.s = UINT16_C(0x0100);
    cpu.bus.write8 = NULL;
    bank7e[0x9600] = UINT8_C(0x22);
    bank7e[0x9601] = UINT8_C(0x34);
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e9600), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(expect_operation(&cpu, &state,
                           MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_INVALID_BUS) == 0);
    CHECK(cpu.stopped);
    CHECK(cpu.stop_reason == MIPSNES2_CPU_INVALID_BUS);
    CHECK(cpu.pc == UINT16_C(0x9601));
    CHECK(cpu.s == UINT16_C(0x0100));
    CHECK(cpu.instruction_cycles == 0U);
    CHECK(trace.count == 1U);
    return 0;
}

int main(void)
{
    CHECK(test_contract_validation_and_nop() == 0);
    CHECK(test_resumable_ora_and_missing_write_fallback() == 0);
    CHECK(test_all_opcodes_have_resumable_sequences() == 0);
    CHECK(test_final_opcode_sequences() == 0);
    CHECK(test_accumulator_shift_cycles() == 0);
    CHECK(test_memory_rmw_cycles() == 0);
    CHECK(test_memory_rmw_missing_write_preflight() == 0);
    CHECK(test_memory_rmw_stale_internal() == 0);
    CHECK(test_load_store_family_cycles() == 0);
    CHECK(test_load_store_missing_write_preflight() == 0);
    CHECK(test_load_store_stale_internal() == 0);
    CHECK(test_lda_immediate_cycles() == 0);
    CHECK(test_sta_absolute_cycles() == 0);
    CHECK(test_rep_sep_internal_commit() == 0);
    CHECK(test_software_interrupt_cycles() == 0);
    CHECK(test_hardware_interrupt_cycles() == 0);
    CHECK(test_boundary_stale_on_interrupt() == 0);
    CHECK(test_interrupt_missing_write_preflight() == 0);
    CHECK(test_stack_push_cycles() == 0);
    CHECK(test_stack_pull_cycles() == 0);
    CHECK(test_stack_pull_stale_internal() == 0);
    CHECK(test_effective_address_push_cycles() == 0);
    CHECK(test_stack_missing_write_fallback() == 0);
    CHECK(test_implied_status_cycles() == 0);
    CHECK(test_implied_transfer_cycles() == 0);
    CHECK(test_implied_increment_cycles() == 0);
    CHECK(test_xba_stale_internal() == 0);
    CHECK(test_conditional_branch_cycles() == 0);
    CHECK(test_branch_boundary_cycles() == 0);
    CHECK(test_branch_long_cycles() == 0);
    CHECK(test_jump_direct_cycles() == 0);
    CHECK(test_jump_indirect_cycles() == 0);
    CHECK(test_jsr_absolute_cycles() == 0);
    CHECK(test_rts_cycles() == 0);
    CHECK(test_jsr_missing_write_fallback() == 0);
    CHECK(test_jsr_indexed_indirect_cycles() == 0);
    CHECK(test_jsr_indexed_missing_write_fallback() == 0);
    CHECK(test_jsl_absolute_long_cycles() == 0);
    CHECK(test_rtl_cycles() == 0);
    CHECK(test_jsl_missing_write_fallback() == 0);
    (void)puts("PASS: resumable 65C816 operation, control-flow and interrupt contract");
    return 0;
}
