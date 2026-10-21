# 65C816 resumable interrupt-entry audit

Date: 2026-10-21

## Scope

This milestone extends the caller-owned peek/commit cursor to software-interrupt
opcodes `BRK` (`00`) and `COP` (`02`) and to boundary-accepted `ABORT`, `NMI` and
`IRQ` entry. It does not connect the cursor to the scheduler or claim a complete
electrical bus-cycle model.

For `BRK` and `COP`, the cursor exposes the opcode read, signature-byte read, native
PBR stack write when applicable, PCH/PCL/status stack writes and low/high vector
reads. Hardware entries expose the stack and vector accesses plus two explicit
internal operations that preserve the established seven-cycle emulation and
eight-cycle native totals. Those internal-operation positions are a provisional
software decomposition: no unsourced dummy bus reads or writes were invented.

The accepted interrupt cause, entry PC/PBR, mode, pushed status and vector are
latched before the first effect. Later input changes therefore cannot redirect a
partially committed entry. `ABORT` remains boundary-visible only; physical
mid-instruction sampling, restart and write suppression are not claimed.

## Evidence

Instruction behavior, vectors, stack layout and aggregate cycle totals are
implemented from the [Western Design Center W65C816S Datasheet, revision March 13,
2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf), especially
sections 2.13, 7.4, 7.13 and 7.19, Tables 5-2 and 5-3, and Table 5-7 entry 22a.
Existing MIPSNES2 interrupt behavior and invariants remain those documented in
[65c816.md](65c816.md) and
[the ABORT audit](65c816-abort-foundation-audit.md). The datasheet
does not establish the exact placement of every internal or dummy cycle represented
by this software cursor. No emulator implementation was consulted or copied.

## Verified properties

- side-effect-free repeated peek and exact-once commit for every new operation;
- stale boundary-control descriptor rejection when an interrupt arrives;
- `ABORT` over `NMI` over mask-eligible `IRQ` boundary priority;
- retained lower-priority NMI after a simultaneous accepted `ABORT`;
- accepted cause and entry state remain stable throughout a partial entry;
- emulation/native stack byte order, PBR handling and stack-pointer decrement;
- BRK-only emulation pushed-status break bit and hardware/COP clear behavior;
- interrupt-disable set, decimal clear and native PBR clear at the established point;
- emulation/native vectors and low-before-high vector fetch;
- no opcode read before hardware entry and one signature read for BRK/COP;
- seven/eight processor-cycle aggregate totals and matching access traces;
- deterministic invalid-bus failure before any stack or vector access when the write
  callback required by an accepted entry is missing;
- legacy public `mipsnes2_cpu_step` results preserved through the same cursor driver.

## Deliberate limits

The other 249 opcode remainders and reset remain legacy-atomic. Dummy reads/writes,
signal-level activity, exact electrical phase placement, RESET-signal sequencing,
mid-instruction `ABORT`, general cancellation/checkpoint recovery and DMA/HDMA bus
ownership remain open. The central timed-step bridge still executes a whole CPU
boundary and must not dispatch scheduler events from a bus callback.

This block is complete when deterministic host tests prove every property above,
the full debug/release/sanitizer suites and static analyzer pass, repository checks
pass, and the shared core compiles in the pinned PS2 and PSP CI toolchains.

## Verification

- `make BUILD_DIR=build/interrupt-dev -j2 test` — passed all 14 C test executables,
  the host smoke test and repository checks.
- `make BUILD_DIR=build/interrupt-release CC=gcc CFLAGS='-O2 -g -DNDEBUG' -j2 test`
  — passed.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/interrupt-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test` — passed with ASan and UBSan;
  leak detection alone was disabled for the managed environment.
- `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c
  core/cpu/65c816/cpu.c -o /tmp/mipsnes2-interrupt-cpu.o` — passed.
- `make -C platform/ps2` — not run to compilation locally: `PS2SDK` is unset.
- `make -C platform/psp` — not run to compilation locally: `psp-config` is
  unavailable.

Clang, PS2SDK and PSPSDK are unavailable locally. GitHub Host, PS2 and PSP workflow
results for this exact change are pending. No real SNES, PS2, PSP, PCSX2 or PPSSPP
test and no performance measurement was performed.
