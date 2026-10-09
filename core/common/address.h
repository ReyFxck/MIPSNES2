/* MIPSNES2 foundation. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_ADDRESS_H
#define MIPSNES2_ADDRESS_H
#include <stdint.h>
/* Logical SNES addresses only; these are never native memory pointers. */
typedef uint32_t mipsnes2_address;
static inline mipsnes2_address mipsnes2_address24(uint32_t value)
{
    return value & UINT32_C(0x00ffffff);
}
static inline mipsnes2_address mipsnes2_address_make(uint8_t bank, uint16_t offset)
{
    return ((uint32_t)bank << 16) | (uint32_t)offset;
}
#endif
