# core/cartridge

Contains the caller-owned base LoROM/HiROM mapping foundation and a cartridge-slot
interface that can decline unmapped reads/writes. Platform code owns file loading and
SRAM persistence; the shared core receives validated byte buffers.

`image.c` provides a heap-free view over clean or 512-byte-prefixed image data,
parses LoROM/HiROM internal-header candidates, computes their mirrored whole-image
checksum and conservatively selects one supported base map only when every documented
evidence gate agrees. No-match and ambiguity are explicit results. Selection does not
initialize the executable mapper or trust a title/game identity as hardware truth.

The executable base mapper accepts power-of-two images and the documented larger
power-of-two region followed by a smaller power-of-two tail, repeating that tail to
the next power-of-two address span. Bank-misaligned or padding-dependent layouts are
rejected rather than guessed.

This is not complete cartridge support. Extended/variant header analysis,
board-specific irregular ROM layouts, ExLoROM/ExHiROM, enhancement
chips and SRAM persistence remain open. See [cartridge documentation](../../docs/cartridge.md)
and [canonical status](../../STATUS.md).
