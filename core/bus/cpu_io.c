/* MIPSNES2 Ricoh 5A22 CPU-I/O foundation. See LICENSE for GPL-3.0 terms. */
#include "core/bus/cpu_io.h"

#include <string.h>

enum {
    NMITIMEN_JOYPAD_ENABLE = 0x01,
    NMITIMEN_IRQ_MASK = 0x30,
    NMITIMEN_NMI_ENABLE = 0x80,
    NMITIMEN_IMPLEMENTED_MASK = 0xb1,
    MULTIPLY_CYCLES = 8,
    DIVIDE_CYCLES = 16
};

static void start_multiply(mipsnes2_cpu_io *io, uint8_t multiplier)
{
    io->wrmpyb = multiplier;
    io->pending_rddiv = multiplier;
    io->pending_rdmpy = (uint16_t)((uint16_t)io->wrmpya * multiplier);
    io->math_cycles_remaining = MULTIPLY_CYCLES;
    io->math_operation = MIPSNES2_CPU_IO_MATH_MULTIPLY;
}

static void start_divide(mipsnes2_cpu_io *io, uint8_t divisor)
{
    io->wrdivb = divisor;
    if (divisor == 0U) {
        io->pending_rddiv = UINT16_MAX;
        io->pending_rdmpy = io->wrdiv;
    } else {
        io->pending_rddiv = (uint16_t)(io->wrdiv / divisor);
        io->pending_rdmpy = (uint16_t)(io->wrdiv % divisor);
    }
    io->math_cycles_remaining = DIVIDE_CYCLES;
    io->math_operation = MIPSNES2_CPU_IO_MATH_DIVIDE;
}

static bool nmi_gate_active(const mipsnes2_cpu_io *io)
{
    return io->nmi_flag &&
           (io->nmitimen & NMITIMEN_NMI_ENABLE) != 0U;
}

static void latch_nmi_rising_edge(mipsnes2_cpu_io *io,
                                  bool was_active)
{
    if (!was_active && nmi_gate_active(io)) {
        io->nmi_edge_pending = true;
    }
}

static void latch_manual_joypads(mipsnes2_cpu_io *io)
{
    (void)memcpy(io->manual_joypad_latch, io->joypad_input,
                 sizeof(io->manual_joypad_latch));
    io->manual_joypad_connected_mask = io->joypad_connected_mask;
    io->manual_joypad_index[0] = 0U;
    io->manual_joypad_index[1] = 0U;
}

static uint8_t manual_joypad_bit(const mipsnes2_cpu_io *io,
                                 uint8_t line,
                                 uint8_t port)
{
    uint8_t index;

    if (io->joypad_strobe) {
        return (uint8_t)((io->joypad_input[line] >> 15) & UINT16_C(1));
    }
    index = io->manual_joypad_index[port];
    if (index < UINT8_C(16)) {
        return (uint8_t)((io->manual_joypad_latch[line] >>
                          (15U - index)) & UINT16_C(1));
    }
    return (io->manual_joypad_connected_mask & (uint8_t)(1U << line)) != 0U
               ? UINT8_C(1)
               : UINT8_C(0);
}

static uint8_t read_manual_joypad_port(mipsnes2_cpu_io *io,
                                       uint8_t port,
                                       uint8_t open_bus)
{
    uint8_t first_line = port;
    uint8_t second_line = (uint8_t)(port + 2U);
    uint8_t fixed = port == 0U ? (uint8_t)(open_bus & UINT8_C(0xfc))
                               : (uint8_t)((open_bus & UINT8_C(0xe0)) |
                                           UINT8_C(0x1c));
    uint8_t result = (uint8_t)(fixed |
                     manual_joypad_bit(io, first_line, port) |
                     (uint8_t)(manual_joypad_bit(io, second_line, port) << 1));

    if (!io->joypad_strobe && io->manual_joypad_index[port] < UINT8_C(16)) {
        io->manual_joypad_index[port]++;
    }
    return result;
}

void mipsnes2_cpu_io_init(mipsnes2_cpu_io *io, uint8_t cpu_version)
{
    if (io == NULL) {
        return;
    }
    (void)memset(io, 0, sizeof(*io));
    io->htime = UINT16_C(0x01ff);
    io->vtime = UINT16_C(0x01ff);
    io->cpu_version = cpu_version & UINT8_C(0x0f);
}

bool mipsnes2_cpu_io_read(mipsnes2_cpu_io *io,
                          uint16_t offset,
                          uint8_t open_bus,
                          uint8_t *value)
{
    uint8_t result;

    if (io == NULL || value == NULL) {
        return false;
    }
    switch (offset) {
    case 0x4016:
        result = read_manual_joypad_port(io, 0U, open_bus);
        break;
    case 0x4017:
        result = read_manual_joypad_port(io, 1U, open_bus);
        break;
    case 0x4200:
    case 0x4202:
    case 0x4203:
    case 0x4204:
    case 0x4205:
    case 0x4206:
    case 0x4207:
    case 0x4208:
    case 0x4209:
    case 0x420a:
        result = open_bus;
        break;
    case 0x4210:
        result = (uint8_t)((open_bus & UINT8_C(0x70)) |
                           io->cpu_version |
                           (io->nmi_flag ? UINT8_C(0x80) : 0U));
        io->nmi_flag = false;
        break;
    case 0x4211:
        result = (uint8_t)((open_bus & UINT8_C(0x7f)) |
                           (io->irq_flag ? UINT8_C(0x80) : 0U));
        io->irq_flag = false;
        break;
    case 0x4212:
        result = (uint8_t)((open_bus & UINT8_C(0x3e)) |
                           (io->vblank ? UINT8_C(0x80) : 0U) |
                           (io->hblank ? UINT8_C(0x40) : 0U) |
                           (io->auto_joypad_busy ? UINT8_C(0x01) : 0U));
        break;
    case 0x4214:
        result = (uint8_t)io->rddiv;
        break;
    case 0x4215:
        result = (uint8_t)(io->rddiv >> 8);
        break;
    case 0x4216:
        result = (uint8_t)io->rdmpy;
        break;
    case 0x4217:
        result = (uint8_t)(io->rdmpy >> 8);
        break;
    case 0x4218:
    case 0x4219:
    case 0x421a:
    case 0x421b:
    case 0x421c:
    case 0x421d:
    case 0x421e:
    case 0x421f: {
        uint16_t register_index = (uint16_t)(offset - UINT16_C(0x4218));
        uint16_t joypad = io->joypad_result[register_index >> 1];

        result = (register_index & UINT16_C(1)) == 0U
                     ? (uint8_t)joypad
                     : (uint8_t)(joypad >> 8);
        break;
    }
    default:
        return false;
    }
    *value = result;
    return true;
}

bool mipsnes2_cpu_io_write(mipsnes2_cpu_io *io,
                           uint16_t offset,
                           uint8_t value)
{
    bool nmi_was_active;

    if (io == NULL) {
        return false;
    }
    switch (offset) {
    case 0x4016: {
        bool next_strobe = (value & UINT8_C(1)) != 0U;

        io->joypad_output = value & UINT8_C(7);
        if (io->joypad_strobe && !next_strobe) {
            latch_manual_joypads(io);
        } else if (next_strobe) {
            io->manual_joypad_index[0] = 0U;
            io->manual_joypad_index[1] = 0U;
        }
        io->joypad_strobe = next_strobe;
        return true;
    }
    case 0x4200:
        nmi_was_active = nmi_gate_active(io);
        io->nmitimen = value & UINT8_C(NMITIMEN_IMPLEMENTED_MASK);
        if ((io->nmitimen & NMITIMEN_IRQ_MASK) == 0U) {
            io->irq_flag = false;
        }
        latch_nmi_rising_edge(io, nmi_was_active);
        return true;
    case 0x4202:
        io->wrmpya = value;
        return true;
    case 0x4203:
        start_multiply(io, value);
        return true;
    case 0x4204:
        io->wrdiv = (uint16_t)((io->wrdiv & UINT16_C(0xff00)) | value);
        return true;
    case 0x4205:
        io->wrdiv = (uint16_t)((io->wrdiv & UINT16_C(0x00ff)) |
                               ((uint16_t)value << 8));
        return true;
    case 0x4206:
        start_divide(io, value);
        return true;
    case 0x4207:
        io->htime = (uint16_t)((io->htime & UINT16_C(0x0100)) | value);
        return true;
    case 0x4208:
        io->htime = (uint16_t)((io->htime & UINT16_C(0x00ff)) |
                               (((uint16_t)value & UINT16_C(1)) << 8));
        return true;
    case 0x4209:
        io->vtime = (uint16_t)((io->vtime & UINT16_C(0x0100)) | value);
        return true;
    case 0x420a:
        io->vtime = (uint16_t)((io->vtime & UINT16_C(0x00ff)) |
                               (((uint16_t)value & UINT16_C(1)) << 8));
        return true;
    case 0x4210:
    case 0x4211:
    case 0x4212:
    case 0x4214:
    case 0x4215:
    case 0x4216:
    case 0x4217:
    case 0x4218:
    case 0x4219:
    case 0x421a:
    case 0x421b:
    case 0x421c:
    case 0x421d:
    case 0x421e:
    case 0x421f:
        return true;
    default:
        return false;
    }
}

void mipsnes2_cpu_io_set_vblank(mipsnes2_cpu_io *io, bool active)
{
    if (io == NULL || io->vblank == active) {
        return;
    }
    io->vblank = active;
    if (!active) {
        io->nmi_flag = false;
    }
}

void mipsnes2_cpu_io_set_hblank(mipsnes2_cpu_io *io, bool active)
{
    if (io != NULL) {
        io->hblank = active;
    }
}

void mipsnes2_cpu_io_signal_nmi_flag(mipsnes2_cpu_io *io)
{
    bool nmi_was_active;

    if (io == NULL) {
        return;
    }
    nmi_was_active = nmi_gate_active(io);
    io->nmi_flag = true;
    latch_nmi_rising_edge(io, nmi_was_active);
}

void mipsnes2_cpu_io_signal_timer_irq(mipsnes2_cpu_io *io)
{
    if (io != NULL && (io->nmitimen & NMITIMEN_IRQ_MASK) != 0U) {
        io->irq_flag = true;
    }
}

bool mipsnes2_cpu_io_take_nmi_edge(mipsnes2_cpu_io *io)
{
    bool pending;

    if (io == NULL) {
        return false;
    }
    pending = io->nmi_edge_pending;
    io->nmi_edge_pending = false;
    return pending;
}

bool mipsnes2_cpu_io_irq_asserted(const mipsnes2_cpu_io *io)
{
    return io != NULL && io->irq_flag;
}

void mipsnes2_cpu_io_advance_math_cycles(mipsnes2_cpu_io *io,
                                         uint64_t processor_cycles)
{
    if (io == NULL || io->math_cycles_remaining == 0U ||
        processor_cycles == 0U) {
        return;
    }
    if (processor_cycles < io->math_cycles_remaining) {
        io->math_cycles_remaining =
            (uint8_t)(io->math_cycles_remaining - processor_cycles);
        return;
    }
    io->rddiv = io->pending_rddiv;
    io->rdmpy = io->pending_rdmpy;
    io->math_cycles_remaining = 0U;
    io->math_operation = MIPSNES2_CPU_IO_MATH_IDLE;
}

bool mipsnes2_cpu_io_math_busy(const mipsnes2_cpu_io *io)
{
    return io != NULL && io->math_cycles_remaining != 0U;
}

bool mipsnes2_cpu_io_set_joypad_input(mipsnes2_cpu_io *io,
                                      uint8_t index,
                                      uint16_t value)
{
    if (io == NULL || index >= MIPSNES2_CPU_IO_JOYPAD_COUNT) {
        return false;
    }
    io->joypad_input[index] = value;
    return true;
}

bool mipsnes2_cpu_io_set_joypad_connected(mipsnes2_cpu_io *io,
                                          uint8_t index,
                                          bool connected)
{
    uint8_t mask;

    if (io == NULL || index >= MIPSNES2_CPU_IO_JOYPAD_COUNT) {
        return false;
    }
    mask = (uint8_t)(1U << index);
    if (connected) {
        io->joypad_connected_mask |= mask;
    } else {
        io->joypad_connected_mask &= (uint8_t)~mask;
    }
    return true;
}

bool mipsnes2_cpu_io_auto_joypad_enabled(const mipsnes2_cpu_io *io)
{
    return io != NULL &&
           (io->nmitimen & NMITIMEN_JOYPAD_ENABLE) != 0U;
}

bool mipsnes2_cpu_io_begin_auto_joypad_read(mipsnes2_cpu_io *io)
{
    if (!mipsnes2_cpu_io_auto_joypad_enabled(io) ||
        io->auto_joypad_busy) {
        return false;
    }
    (void)memcpy(io->pending_joypad_result, io->joypad_input,
                 sizeof(io->pending_joypad_result));
    io->auto_joypad_busy = true;
    return true;
}

bool mipsnes2_cpu_io_complete_auto_joypad_read(mipsnes2_cpu_io *io)
{
    if (io == NULL || !io->auto_joypad_busy) {
        return false;
    }
    (void)memcpy(io->joypad_result, io->pending_joypad_result,
                 sizeof(io->joypad_result));
    io->auto_joypad_busy = false;
    return true;
}

mipsnes2_cpu_io_irq_mode mipsnes2_cpu_io_irq_mode_get(
    const mipsnes2_cpu_io *io)
{
    if (io == NULL) {
        return MIPSNES2_CPU_IO_IRQ_DISABLED;
    }
    return (mipsnes2_cpu_io_irq_mode)((io->nmitimen &
                                      NMITIMEN_IRQ_MASK) >> 4);
}
