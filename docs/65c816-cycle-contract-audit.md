# 65C816 resumable operation-contract foundation audit

Date: 2026-10-21

## Scope

This milestone introduces a caller-owned peek/commit cursor without changing the
verified 256-value opcode decode. The initial resumable instruction subset is exactly:

| Opcode | Instruction | Exposed operations |
|---:|---|---|
| `EA` | `NOP` | opcode read, internal cycle |
| `A9` | `LDA #` | opcode read, one or two M-selected operand reads |
| `8D` | `STA abs` | opcode read, two address reads, one or two M-selected writes |
| `C2` | `REP #` | opcode read, mask read, internal status commit |
| `E2` | `SEP #` | opcode read, mask read, internal status commit |

The cursor describes zero-time boundary control, logical 24-bit reads, writes,
internal cycles and an explicit legacy-atomic remainder. Peek has no bus or CPU
effect. Commit rejects a changed descriptor, then performs one described operation.
`mipsnes2_cpu_step` drives the cursor until completion so existing callers and all
opcode regressions continue through the same public instruction API.

## Evidence

Instruction behavior and aggregate cycle counts are independently implemented from the
[Western Design Center W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
especially the programming model/status definitions, instruction descriptions and
Table 5-7 opcode, length and cycle entries. The datasheet does not by itself prove
every internal signal phase used by this software cursor; those positions remain a
provisional, testable sequencing choice. The established M/E invariants and
logical-address rules remain those documented in [65c816.md](65c816.md). No emulator
implementation was consulted or copied.

## Verified properties

- caller-owned cursor initialization, begin, repeated peek and one-operation commit;
- stale descriptor rejection before a bus or CPU effect;
- logical PBR:PC opcode/operand addresses and 16-bit PC wrap within PBR;
- DBR-selected absolute stores and linear 24-bit high-byte crossing;
- 8-bit accumulator B preservation and 16-bit immediate assembly;
- N/Z changes for `LDA`, status preservation for `STA`, and REP/SEP E/M/X invariants;
- deterministic software commit points for those effects, without claiming the
  unresolved electrical phase as hardware-verified;
- low-before-high 16-bit writes with one callback per committed byte;
- processor-cycle totals matching the existing aggregate contract;
- a newly arrived boundary NMI invalidating a previously peeked control descriptor;
- explicit `LEGACY_ATOMIC` fallback with unchanged aggregate behavior.

## Deliberate limits

This is not a complete cycle-level interpreter. The other 251 opcode remainders,
interrupt entry, reset, dummy reads/writes, signal-level cycles and undocumented
activity remain atomic or absent. The central timed-step bridge still executes a
whole instruction and does not consume the cursor. Active state must be retained and
resumed without interleaving another CPU step; general cancellation/checkpoint
recovery is not implemented. Exact internal/dummy-cycle placement remains subject to
stronger documentation and hardware tests. No mid-instruction ABORT claim is made.

Meaningful-access scheduler dispatch must remain disabled until every reachable path
can pause before an effect, resume once, and propagate event-budget/callback failure
without losing the cursor or duplicating bus activity. DMA/HDMA ownership and bus
arbitration remain future work.

## Verification

- `make BUILD_DIR=build/cycle-dev -j2 test` — passed all 14 C test executables,
  the host smoke test and repository checks.
- `make BUILD_DIR=build/cycle-default -j2 test` — passed with GCC 13.3.0.
- `make BUILD_DIR=build/cycle-release CC=gcc CFLAGS='-O2 -g -DNDEBUG' -j2 test`
  — passed.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/cycle-sanitize CC=gcc
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test` — passed with ASan and UBSan;
  leak detection alone was disabled for the managed environment.
- `python3 tools/check_repository.py` and `git diff --check` — passed.
- `make -C platform/ps2` — not run to compilation: `PS2SDK` is unset.
- `make -C platform/psp` — not run to compilation: `psp-config` is unavailable.

GitHub PR #49 initial review head
`d6c0832f064c44f611de52b2fb7e4d97f7f25421` used exact source tree
`88c739c5cda66c0aadb6a96947957c2e0c9f81d1` and passed:

- Host CI run `39674430063`: GCC and Clang release plus sanitizer jobs;
- PS2 CI run `39674429865`: pinned-toolchain scaffold cross-build, artifact and checksum;
- PSP CI run `39674429960`: pinned-toolchain scaffold cross-build, artifact and checksum.

Clang, PS2SDK and PSPSDK are unavailable locally. GitHub Host, PS2 and PSP workflow
results above were inspected before the documentation-only audit follow-up. No real
SNES, PS2, PSP, PCSX2 or PPSSPP test and no performance measurement was performed.
`LICENSE` remains byte-for-byte
GPL-3.0 with SHA-256
`3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.
