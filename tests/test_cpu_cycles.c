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
    mipsnes2_cpu_result cpu_result = MIPSNES2_CPU_OK;

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

static int test_legacy_atomic_fallback(void)
{
    trace_context trace;
    mipsnes2_cpu cpu;
    mipsnes2_cpu_cycle_state state;
    mipsnes2_cpu_operation operation;

    CHECK(initialize_cpu(&cpu, &trace, UINT16_C(0x8010)) == 0);
    bank7e[0x8010] = UINT8_C(0x18); /* CLC */
    cpu.p |= MIPSNES2_CPU_FLAG_C;
    CHECK(begin_instruction(&cpu, &state) == 0);
    CHECK(expect_operation(&cpu, &state, MIPSNES2_CPU_OPERATION_READ,
                           UINT32_C(0x7e8010), 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_READY,
                           MIPSNES2_CPU_NOT_READY) == 0);
    CHECK(cpu.instruction_cycles == UINT64_C(1));
    CHECK(expect_operation(&cpu, &state,
                           MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                           0U, 0U, &operation) == 0);
    CHECK(trace.count == 1U);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK((cpu.p & MIPSNES2_CPU_FLAG_C) == 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(2));
    CHECK(trace.count == 1U);

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
    CHECK(expect_operation(&cpu, &state,
                           MIPSNES2_CPU_OPERATION_LEGACY_ATOMIC,
                           0U, 0U, &operation) == 0);
    CHECK(commit_operation(&cpu, &state, &operation,
                           MIPSNES2_CPU_CYCLE_COMPLETE,
                           MIPSNES2_CPU_OK) == 0);
    CHECK(!cpu.nmi_pending);
    CHECK(cpu.pc == UINT16_C(0x1234));
    CHECK(cpu.pbr == 0U);
    CHECK(cpu.instruction_cycles == UINT64_C(7));
    CHECK(trace.count == 5U);
    return 0;
}

int main(void)
{
    CHECK(test_contract_validation_and_nop() == 0);
    CHECK(test_legacy_atomic_fallback() == 0);
    CHECK(test_lda_immediate_cycles() == 0);
    CHECK(test_sta_absolute_cycles() == 0);
    CHECK(test_rep_sep_internal_commit() == 0);
    CHECK(test_boundary_stale_on_interrupt() == 0);
    (void)puts("PASS: resumable 65C816 operation contract foundation");
    return 0;
}
