# core/cartridge

Contains the caller-owned base LoROM/HiROM mapping foundation and a cartridge-slot
interface that can decline unmapped reads/writes. Platform code owns file loading and
SRAM persistence; the shared core receives validated byte buffers.

This is not complete cartridge support. Header analysis, automatic mapper selection,
copier-header handling, non-power-of-two ROM boards, ExLoROM/ExHiROM, enhancement
chips and SRAM persistence remain open. See [cartridge documentation](../../docs/cartridge.md)
and [canonical status](../../STATUS.md).
