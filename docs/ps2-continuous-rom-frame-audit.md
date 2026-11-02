# PS2 explicit-ROM frame continuation — 2026-11-02

## Scope and evidence

The 65C816 and SPC700 already execute all 256 opcode values. The immediate
runtime obstacle was the PS2 `--rom PATH` entry stopping after one frame in the
standalone build or eight frames with the optional audio build. The explicit ROM
path now continues across frame boundaries; the no-argument core and ROM-free
audio diagnostics retain their bounded behavior. No cartridge content is added.

The CPU opcode baseline is documented in [65c816.md](65c816.md) and
[spc700.md](spc700.md). The consulted primary instruction source is WDC's
[W65C816S datasheet, March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
sections 2.25 and Table 5-7 entry 22a. Its signal-level RESET sequence remains
open, as does the Ricoh 5A22 phase mapping; this block makes no CPU timing claim.
The existing H/V boundary contract is documented in
[frame-boundary-horizon-audit.md](frame-boundary-horizon-audit.md). No source from
another emulator was consulted or adapted.

## Implementation

- `platform/ps2/rom_frame.[ch]` computes each next exact H/V boundary, checks
  master-clock overflow and resumes the shared machine in bounded sequence
  slices. It stops at the caller's per-frame sequence cap on a pathological
  program, CPU stop or timing error without silently discarding in-flight state.
- The PS2 explicit-ROM entry resets its static capture before each frame,
  samples one port-0 pad word, and exits on connected Start+Select. GS upload
  follows emulated advancement; optional audio pumping still cannot advance it.
- Per-frame work is bounded, while the number of frames is governed by the user
  gesture or an error. Accumulated diagnostic counters saturate.
- All PS2 APIs, frame policy and buffers remain outside `core/`; PSP remains a
  separate backend.

## Verification and limits

`tests/test_machine_ppu_frame.c` uses an original synthetic LoROM (`BRA -2`),
first stops at a small sequence cap, resumes to the same frame horizon, then
checks three successive NTSC boundaries, short-line alternation and 224
reference line callbacks per frame. Existing chunk-equivalence/hash coverage
remains. No commercial ROM, external vector or test binary is imported.

Host tests and cross-builds establish only software behavior and compiler/link
portability. A PS2/PCSX2/NetherSX2 run of `--rom PATH`, button exit, actual
display, frame time and compatibility has not yet been observed. Missing PPU
modes, electrical CPU cycles, ROM selection and durable SRAM remain open.

## Local commands and results

| Check | Result |
|---|---|
| `make BUILD_DIR=build/rom-frame -j4 test` | Passed on GCC host after restoring execute bits on generated test executables |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/rom-frame-asan CC=gcc CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' -j4 test` | Passed on GCC host after restoring execute bits on generated test executables; LeakSanitizer disabled in this managed environment |
| `ASAN_OPTIONS=detect_leaks=0 build/rom-frame-asan/test-machine-ppu-frame` | Passed after the final controller-exit predicate change |
| `gcc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I. -c platform/ps2/rom_frame.c -o /tmp/mipsnes2-rom-frame-analyzer.o` | Passed |
| `python3 tools/check_repository.py` and `git diff --check` | Passed |

Clang, PS2SDK and PSPSDK are unavailable in this checkout; target cross-builds
must be read from actual CI runs on the published PR. No emulator or physical
console execution was performed in this block.

The implementation tree `5d663ba5e1335a16479a4e07176b4ca1fdfc9b6e`
was published as commit
[`bf87bda`](https://github.com/ReyFxck/MIPSNES2/commit/bf87bda6fe421e8e16140bcb7b925d9876fa9537)
on [PR #52](https://github.com/ReyFxck/MIPSNES2/pull/52). The PR targets
the existing #51 branch so its diff is limited to this frame block; neither
PR was merged in this task. Its actual workflow runs completed successfully:

- [Host CI 41529166778](https://github.com/ReyFxck/MIPSNES2/actions/runs/41529166778)
  — GCC and Clang release tests, sanitizers and artifact uploads passed;
- [PS2 CI 41529166766](https://github.com/ReyFxck/MIPSNES2/actions/runs/41529166766)
  — pinned cross-compile and upload passed; artifact
  [13180069608](https://github.com/ReyFxck/MIPSNES2/actions/runs/41529166766/artifacts/13180069608)
  contains `mipsnes2-ps2.elf` and checked `SHA256SUMS`;
- [PSP CI 41529166665](https://github.com/ReyFxck/MIPSNES2/actions/runs/41529166665)
  — pinned cross-compile, checksum and artifact upload passed.

The PS2 artifact ZIP digest is
`sha256:17d06e40eff9fc02298ec131334881fee433c1158b6fc6fabc6d12a771ef4a99`;
this is the archive digest, not the ELF digest. The ELF has not been run on an
emulator or physical PS2 in this milestone.
