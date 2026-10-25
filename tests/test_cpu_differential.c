/* Deterministic atomic/resumable CPU consistency test. See LICENSE. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/cpu/65c816/cpu.h"

enum { MAX_OVERRIDES = 64, MAX_TRACE = 128 };

typedef struct {
    bool write;
    mipsnes2_address address;
    uint8_t value;
} traced_access;

typedef struct {
    mipsnes2_address address[MAX_OVERRIDES];
    uint8_t value[MAX_OVERRIDES];
    unsigned count;
    traced_access trace[MAX_TRACE];
    unsigned trace_count;
    uint32_t seed;
} memory;

static uint8_t base_value(uint32_t seed, mipsnes2_address address)
{
    uint32_t mixed = (uint32_t)address ^ seed ^ UINT32_C(0x9e3779b9);

    mixed ^= mixed >> 16;
    mixed *= UINT32_C(0x7feb352d);
    mixed ^= mixed >> 15;
    return (uint8_t)(mixed ^ (mixed >> 8));
}

static uint8_t memory_read(void *opaque, mipsnes2_address address)
{
    memory *state = opaque;
    unsigned index;
    uint8_t value;

    address = mipsnes2_address24(address);
    value = base_value(state->seed, address);
    for (index = state->count; index != 0U; --index) {
        if (state->address[index - 1U] == address) {
            value = state->value[index - 1U];
            break;
        }
    }
    if (state->trace_count < MAX_TRACE) {
        state->trace[state->trace_count].write = false;
        state->trace[state->trace_count].address = address;
        state->trace[state->trace_count].value = value;
    }
    ++state->trace_count;
    return value;
}

static void memory_write(void *opaque, mipsnes2_address address, uint8_t value)
{
    memory *state = opaque;

    address = mipsnes2_address24(address);
    if (state->trace_count < MAX_TRACE) {
        state->trace[state->trace_count].write = true;
        state->trace[state->trace_count].address = address;
        state->trace[state->trace_count].value = value;
    }
    ++state->trace_count;
    if (state->count < MAX_OVERRIDES) {
        state->address[state->count] = address;
        state->value[state->count] = value;
        ++state->count;
    }
}

static void memory_put(memory *state, mipsnes2_address address, uint8_t value)
{
    state->address[state->count] = mipsnes2_address24(address);
    state->value[state->count] = value;
    ++state->count;
}

static bool cpu_equal(const mipsnes2_cpu *left, const mipsnes2_cpu *right)
{
    return left->a == right->a && left->x == right->x &&
           left->y == right->y && left->s == right->s &&
           left->d == right->d && left->pc == right->pc &&
           left->dbr == right->dbr && left->pbr == right->pbr &&
           left->p == right->p && left->emulation == right->emulation &&
           left->instruction_cycles == right->instruction_cycles &&
           left->waiting == right->waiting &&
           left->clock_stopped == right->clock_stopped &&
           left->irq_level == right->irq_level &&
           left->abort_pending == right->abort_pending &&
           left->nmi_pending == right->nmi_pending &&
           left->stopped == right->stopped &&
           left->stop_reason == right->stop_reason &&
           left->last_opcode == right->last_opcode &&
           left->last_opcode_address == right->last_opcode_address;
}

static bool trace_equal(const memory *left, const memory *right)
{
    unsigned index;

    if (left->trace_count != right->trace_count ||
        left->trace_count > MAX_TRACE) {
        return false;
    }
    for (index = 0U; index < left->trace_count; ++index) {
        if (left->trace[index].write != right->trace[index].write ||
            left->trace[index].address != right->trace[index].address ||
            left->trace[index].value != right->trace[index].value) {
            return false;
        }
    }
    return true;
}

static int run_case(unsigned opcode, unsigned variant)
{
    memory atomic_memory = {0};
    memory resumable_memory = {0};
    mipsnes2_bus_interface atomic_bus = {
        &atomic_memory, memory_read, memory_write
    };
    mipsnes2_bus_interface resumable_bus = {
        &resumable_memory, memory_read, memory_write
    };
    mipsnes2_cpu atomic_cpu;
    mipsnes2_cpu resumable_cpu;
    mipsnes2_cpu_cycle_state cursor;
    mipsnes2_cpu_operation operation;
    mipsnes2_cpu_result atomic_result;
    mipsnes2_cpu_result resumable_result = MIPSNES2_CPU_NOT_READY;
    mipsnes2_cpu_cycle_result cycle_result;
    uint32_t seed = UINT32_C(0x13579bdf) ^
                    opcode * UINT32_C(0x01020305) ^ variant;
    mipsnes2_address opcode_address;
    unsigned steps;

    atomic_memory.seed = seed;
    resumable_memory.seed = seed;
    mipsnes2_cpu_init(&atomic_cpu, atomic_bus);
    mipsnes2_cpu_init(&resumable_cpu, resumable_bus);
    atomic_cpu.a = resumable_cpu.a = (uint16_t)(seed ^ (seed >> 16));
    atomic_cpu.x = resumable_cpu.x = (uint16_t)(seed * 3U);
    atomic_cpu.y = resumable_cpu.y = (uint16_t)(seed * 5U);
    atomic_cpu.d = resumable_cpu.d = (uint16_t)(seed >> 3);
    atomic_cpu.pc = resumable_cpu.pc =
        (uint16_t)(UINT16_C(0x8000) + variant * 16U);
    atomic_cpu.pbr = resumable_cpu.pbr = (uint8_t)(0x20U + variant);
    atomic_cpu.dbr = resumable_cpu.dbr = (uint8_t)(0x70U + variant);
    atomic_cpu.emulation = resumable_cpu.emulation = (variant & 1U) != 0U;
    atomic_cpu.p = resumable_cpu.p = (uint8_t)(seed >> 8);
    if (atomic_cpu.emulation) {
        atomic_cpu.p = resumable_cpu.p =
            (uint8_t)(atomic_cpu.p | MIPSNES2_CPU_FLAG_M |
                      MIPSNES2_CPU_FLAG_X);
        atomic_cpu.s = resumable_cpu.s =
            (uint16_t)(UINT16_C(0x0100) | (seed & UINT32_C(0xff)));
        atomic_cpu.x = resumable_cpu.x = (uint8_t)atomic_cpu.x;
        atomic_cpu.y = resumable_cpu.y = (uint8_t)atomic_cpu.y;
    } else {
        atomic_cpu.s = resumable_cpu.s = (uint16_t)(seed >> 5);
        if ((atomic_cpu.p & MIPSNES2_CPU_FLAG_X) != 0U) {
            atomic_cpu.x = resumable_cpu.x = (uint8_t)atomic_cpu.x;
            atomic_cpu.y = resumable_cpu.y = (uint8_t)atomic_cpu.y;
        }
    }

    opcode_address = mipsnes2_address_make(atomic_cpu.pbr, atomic_cpu.pc);
    memory_put(&atomic_memory, opcode_address, (uint8_t)opcode);
    memory_put(&resumable_memory, opcode_address, (uint8_t)opcode);
    memory_put(&atomic_memory, opcode_address + 1U, (uint8_t)(seed >> 8));
    memory_put(&resumable_memory, opcode_address + 1U,
               (uint8_t)(seed >> 8));
    memory_put(&atomic_memory, opcode_address + 2U, (uint8_t)(seed >> 16));
    memory_put(&resumable_memory, opcode_address + 2U,
               (uint8_t)(seed >> 16));
    memory_put(&atomic_memory, opcode_address + 3U, (uint8_t)(seed >> 24));
    memory_put(&resumable_memory, opcode_address + 3U,
               (uint8_t)(seed >> 24));
    atomic_memory.trace_count = 0U;
    resumable_memory.trace_count = 0U;

    atomic_result = mipsnes2_cpu_step(&atomic_cpu);
    mipsnes2_cpu_cycle_state_init(&cursor);
    if (mipsnes2_cpu_cycle_begin(&cursor) != MIPSNES2_CPU_CYCLE_READY) {
        return 1;
    }
    for (steps = 0U; steps < 80U; ++steps) {
        if (mipsnes2_cpu_cycle_peek(
                &resumable_cpu, &cursor, &operation) !=
            MIPSNES2_CPU_CYCLE_READY) {
            return 1;
        }
        cycle_result = mipsnes2_cpu_cycle_commit(
            &resumable_cpu, &cursor, &operation, &resumable_result);
        if (cycle_result == MIPSNES2_CPU_CYCLE_COMPLETE) {
            break;
        }
        if (cycle_result != MIPSNES2_CPU_CYCLE_READY) {
            return 1;
        }
    }

    if (steps == 80U || atomic_result != resumable_result ||
        !cpu_equal(&atomic_cpu, &resumable_cpu) ||
        !trace_equal(&atomic_memory, &resumable_memory)) {
        (void)fprintf(stderr,
            "opcode %02x variant %u differs: result %d/%d, "
            "steps %u, accesses %u/%u\n",
            opcode, variant, atomic_result, resumable_result, steps,
            atomic_memory.trace_count, resumable_memory.trace_count);
        return 1;
    }
    return 0;
}

int main(void)
{
    unsigned opcode;
    unsigned variant;

    for (opcode = 0U; opcode < 256U; ++opcode) {
        for (variant = 0U; variant < 4U; ++variant) {
            if (run_case(opcode, variant) != 0) {
                return 1;
            }
        }
    }
    (void)puts("PASS: atomic/resumable CPU agreement (1024 cases)");
    return 0;
}
