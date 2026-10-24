# Partial machine integration, 2026-10-24

`core/machine/machine.[ch]` composes the existing shared modules. It accepts an
image buffer, 128 KiB WRAM and header-declared SRAM supplied by the caller, and
constructs the validated cartridge without storing a platform path or SDK handle.
Its address must stay stable while initialized. Power-on resets CPU, CPU-I/O,
counter registers, H/V, interrupt aggregation, scheduler and bus; retained WRAM
and SRAM remain the caller's responsibility. It requests a timed RESET vector
sequence. `resume` advances one sequence/instruction, pauses at a master-clock
horizon or event budget, and preserves the operation cursor for the next call.

The original synthetic 64 KiB LoROM in `tests/test_machine.c` contains
`LDA #$5A; STA $0000; BRA -2` and a locally calculated internal-header
checksum. It is generated in memory by the test; no external ROM or emulator
source was imported. Host validation: `make test` passes, including reset across
a horizon, delayed WRAM write commit, instruction loop, H/V and bus clock
alignment, retained WRAM/SRAM across power-on, PAL selection and malformed
configuration/SRAM capacity rejection. The underlying 256-opcode and scheduler
regressions also pass. SDKs for local PS2 and PSP cross-builds are absent from
this workspace. At revision `2d2a836668441f64006b5899b6abfd0c2b913027`,
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40050336298),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40050336313) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40050336316)
completed successfully. Target runtime was not verified for this revision.

The CPU-I/O and PPU counter revisions initialized here are deterministic
foundation defaults (2 and 3); full console revision selection is outstanding.
Power-on clears scheduled events and does not reset external PPU/APU/DMA callback
implementations. The latter have no complete implementations yet. No ROM-file
adapter, renderer, sample generator or licensed test-ROM runner exists. These
limits prevent claiming a playable emulator or hardware-correct reset.
