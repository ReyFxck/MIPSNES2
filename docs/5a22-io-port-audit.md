# Ricoh 5A22 programmable I/O-port audit

Date: 2026-10-20

## Verified scope

This milestone adds the caller-owned programmable I/O output latch at `$4201` WRIO
and combined input read at `$4213` RDIO. Both the output latch and external input
levels initialize to `FF`. RDIO returns their bitwise AND, modeling open-collector
lines where either the console or an attached device can pull a line low.

The combined controller-port-2 bit-7 line produces a pending event only on a high-to-
low transition, whether caused by WRIO or by the caller's external input. The event
is consumed through an explicit one-shot API. This preserves dependency direction:
CPU-I/O reports a line event, while future machine wiring decides when a PPU counter
latch consumes it.

Reads of write-only WRIO return the supplied CPU open-bus value. Writes to read-only
RDIO are accepted without modifying state, matching the existing read-only register
policy.

## Evidence and boundaries

Nintendo's *SNES Development Manual*, Book I page 2-14-1 describes the programmable
I/O port and pages 2-28-1/2-28-7 define WRIO/RDIO. Independent SNESdev and fullsnes
hardware research describe the open-collector behavior, `FF` power-on output and the
bit-7 external H/V-latch connection. References are linked from
[references.md](references.md). No emulator source or implementation was consulted
or copied.

Initialization represents the documented component power-on value; MIPSNES2 does not
yet distinguish machine power-on from signal-level reset retention. External levels
are an electrical/logical machine input, not a PS2, PSP or host API.

## Verification

`tests/test_cpu_io.c` covers initialization, invalid/null access, write-only/open-bus
reads, read-only writes, output storage, external pulls, wired-AND reads and bit-7
falling/rising/repeated transitions with one-shot acknowledgement. `tests/test_snes_bus.c`
covers low/high-bank routing without fallback leakage.

Local verification so far:

- `make clean && make -j2 test` — passed with GCC after updating superseded
  unimplemented-register expectations.
- `make BUILD_DIR=build/release-io-port CC=gcc CFLAGS='-O2 -g -DNDEBUG'
  clean test` — passed.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-io-port CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' clean test` — passed with ASan+UBSan;
  local leak detection remains disabled for the documented `/proc` restriction.
- `python3 tools/check_repository.py` — passed.

GitHub cross-build evidence will be recorded only after those workflows finish.
Local Clang, PS2SDK and PSPSDK remain unavailable.

## Explicitly incomplete

- wiring the pending edge into future PPU H/V counter capture;
- `$2137/$213C/$213D/$213F` counter-latch and status behavior;
- controller and accessory use of bits 6-7 beyond caller-owned levels;
- Super Famicom Box behavior on otherwise unconnected bits;
- power-on versus reset retention at machine level;
- save-state serialization and real-hardware validation.

No PPU, DMA/HDMA, SPC700, S-DSP, RFAuds2 runtime, enhancement chip, dynarec or
platform-specific optimization was added. No compatibility or performance claim
follows from this milestone.
