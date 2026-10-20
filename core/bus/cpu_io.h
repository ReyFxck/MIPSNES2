/* MIPSNES2 Ricoh 5A22 CPU-I/O foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_CPU_IO_H
#define MIPSNES2_CPU_IO_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MIPSNES2_CPU_IO_IRQ_DISABLED = 0,
    MIPSNES2_CPU_IO_IRQ_H = 1,
    MIPSNES2_CPU_IO_IRQ_V = 2,
    MIPSNES2_CPU_IO_IRQ_HV = 3
} mipsnes2_cpu_io_irq_mode;

typedef enum {
    MIPSNES2_CPU_IO_MATH_IDLE = 0,
    MIPSNES2_CPU_IO_MATH_MULTIPLY,
    MIPSNES2_CPU_IO_MATH_DIVIDE
} mipsnes2_cpu_io_math_operation;

typedef struct {
    uint16_t htime;
    uint16_t vtime;
    uint16_t wrdiv;
    uint16_t rddiv;
    uint16_t rdmpy;
    uint16_t pending_rddiv;
    uint16_t pending_rdmpy;
    uint8_t nmitimen;
    uint8_t cpu_version;
    uint8_t wrmpya;
    uint8_t wrmpyb;
    uint8_t wrdivb;
    uint8_t math_cycles_remaining;
    mipsnes2_cpu_io_math_operation math_operation;
    bool nmi_flag;
    bool irq_flag;
    bool vblank;
    bool hblank;
    bool auto_joypad_busy;
    bool nmi_edge_pending;
} mipsnes2_cpu_io;

void mipsnes2_cpu_io_init(mipsnes2_cpu_io *io, uint8_t cpu_version);
bool mipsnes2_cpu_io_read(mipsnes2_cpu_io *io,
                          uint16_t offset,
                          uint8_t open_bus,
                          uint8_t *value);
bool mipsnes2_cpu_io_write(mipsnes2_cpu_io *io,
                           uint16_t offset,
                           uint8_t value);
void mipsnes2_cpu_io_set_vblank(mipsnes2_cpu_io *io, bool active);
void mipsnes2_cpu_io_set_hblank(mipsnes2_cpu_io *io, bool active);
void mipsnes2_cpu_io_set_auto_joypad_busy(mipsnes2_cpu_io *io,
                                          bool active);
void mipsnes2_cpu_io_signal_nmi_flag(mipsnes2_cpu_io *io);
void mipsnes2_cpu_io_signal_timer_irq(mipsnes2_cpu_io *io);
bool mipsnes2_cpu_io_take_nmi_edge(mipsnes2_cpu_io *io);
bool mipsnes2_cpu_io_irq_asserted(const mipsnes2_cpu_io *io);
void mipsnes2_cpu_io_advance_math_cycles(mipsnes2_cpu_io *io,
                                         uint64_t processor_cycles);
bool mipsnes2_cpu_io_math_busy(const mipsnes2_cpu_io *io);
mipsnes2_cpu_io_irq_mode mipsnes2_cpu_io_irq_mode_get(
    const mipsnes2_cpu_io *io);

#endif
