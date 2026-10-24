# VRAM write-port foundation — 2026-10-24

`core/ppu/vram_ports.[ch]` owns `$2115-$2119` write-port state over a
caller-owned 64 KiB byte buffer. The full-graphic remap reorders address bits
within 32, 64 or 128-word groups; increment steps are 1, 32 or 128 words and
occur after the selected low or high byte. The logical word address is 16-bit,
while physical memory contains 32K words. `core/machine` routes these writes
from the CPU bus, preserving optional other-register callbacks; the PS2 path
supplies the VRAM buffer. No other emulator source or ROM was used.

Manufacturer reference: Nintendo *SNES Development Manual*, Book I,
[PPU register page 2-27-6 and programming cautions 24.3–24.5](https://floating.muncher.se/bot/manual/book1_text.pdf).
These sections specify VMAIN's increment/timing settings, VRAM size and access
windows. The manual scan's full-graphic table is difficult to extract as text;
the address-bit permutations were cross-checked with the independent
[SNESdev PPU register documentation](https://snes.nesdev.org/wiki/PPU_registers).
The source is a hardware manual and hardware research, not emulator source.

`tests/test_ppu_vram.c` covers low/high increment timing, physical wrap, all
three full-graphic groups, 32/128-word steps, unknown-register routing and
storage retention across port reset. `tests/test_machine.c` verifies bus-to-VRAM
composition and rejects a short VRAM buffer. `make test` passes on the host;
target build and runtime evidence for this revision will be recorded after CI.

This does not implement `$2139/$213A` reads, internal read buffering,
forced-blank/VBlank/HBlank access gates, DMA/HDMA, PPU fetch contention or any
pixel rendering. Writes currently update storage whenever the CPU bus accesses
the port. This is an explicit partial register model, not a claim of hardware
correctness for active-display writes or a playable game.
