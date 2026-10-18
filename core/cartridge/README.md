# core/cartridge

Contains the caller-owned base LoROM/HiROM mapping foundation and a cartridge-slot
interface that can decline unmapped reads/writes. Platform code owns file loading and
SRAM persistence; the shared core receives validated byte buffers.

`image.c` provides a heap-free view over clean or 512-byte-prefixed image data and
parses an explicitly selected LoROM/HiROM internal-header candidate. It does not
automatically select a mapper or trust metadata as hardware truth.

This is not complete cartridge support. Automatic mapper selection, extended/variant
header analysis, non-power-of-two ROM boards, ExLoROM/ExHiROM, enhancement
chips and SRAM persistence remain open. See [cartridge documentation](../../docs/cartridge.md)
and [canonical status](../../STATUS.md).
