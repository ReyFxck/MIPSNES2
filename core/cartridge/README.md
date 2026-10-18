# core/cartridge

Contains the caller-owned LoROM/HiROM/ExHiROM mapping foundation and a cartridge-slot
interface that can decline unmapped reads/writes. Platform code owns file loading and
SRAM persistence; the shared core receives validated byte buffers.

`image.c` provides a heap-free view over clean or 512-byte-prefixed image data,
parses LoROM/HiROM/ExHiROM internal-header candidates, computes their mirrored
whole-image checksum and conservatively selects one supported mapper only when every documented
evidence gate agrees. No-match and ambiguity are explicit results. Selection does not
initialize the executable mapper or trust a title/game identity as hardware truth.

The executable mappers accept power-of-two images and the documented larger
power-of-two region followed by a smaller power-of-two tail, repeating that tail to
the next power-of-two address span. Bank-misaligned or padding-dependent layouts are
rejected rather than guessed.

Documented Mode 25/35 ExHiROM is a separate mapper. It supports mapper-aligned images
larger than 4 MiB through 8 MiB, uses the `$40FFC0` header candidate and retains the
same bounded smaller-tail policy. Raw maker/game registration fields at the preceding
16 bytes are parsed for diagnostics but are not title/ASCII confidence heuristics.

This is not complete cartridge support. Special board/header variants,
board-specific irregular ROM layouts, unofficial ExLoROM and enhancement
chips and SRAM persistence remain open. See [cartridge documentation](../../docs/cartridge.md)
and [canonical status](../../STATUS.md).
