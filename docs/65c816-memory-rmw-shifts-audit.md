# 65C816 Memory RMW and Shift/Rotate Audit

Date: 2026-10-16

Branch: `feature/65c816-memory-rmw-shifts`

Pull request: [#24](https://github.com/ReyFxck/MIPSNES2/pull/24)

Integration target: `main` (the cumulative reviewed stack through this milestone).

Stacked base: pull request #23, remote commit
`ce9d96b43bec22be91bfa4460bb1232bc3861cc3`

## Scope

This milestone adds exactly 36 opcodes to the reference C interpreter:

- ASL, LSR, ROL and ROR in accumulator, Direct, Direct-X, absolute and absolute-X
  forms;
- memory INC and DEC in Direct, Direct-X, absolute and absolute-X forms;
- TSB and TRB in Direct and absolute forms;
- STZ in Direct, Direct-X, absolute and absolute-X forms.

The dispatch grows from 187 to 223 unique opcodes. It does not add stack
instructions, block moves, interrupts, indirect control flow, DMA, PPU or APU work.

## Independent behavior basis

The implementation was written from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf),
not from another emulator. The relevant evidence is:

- Table 5-5 for each instruction's N, V, M, X, D, I, Z and C effects;
- Table 5-7 for the opcode/addressing matrix and Direct read-modify-write entries;
- Table 3-1 notes 3 through 5 for M-width, Direct low-byte and
  read-modify-write cycle additions;
- Table 5-7 notes 1, 2 and 17 for Direct Page formation, DL timing and the
  read-modify-write memory sequence;
- caveat 7.5 for the optional indexed cycle rule, which does not apply as a
  separate variable penalty to these fixed-cycle indexed RMW forms.

## Implemented behavior

- All data widths are selected by M without duplicating the interpreter.
- Accumulator shifts preserve B when M=1.
- ASL/LSR/ROL/ROR set C from the shifted-out bit and N/Z from the result.
- Memory INC/DEC update only N/Z.
- TSB/TRB derive Z from `A AND original memory` before modifying memory.
- STZ preserves all flags.
- Direct and Direct-X remain in bank zero and apply the existing E/DL address and
  DL-cycle rules.
- Absolute forms use DBR; absolute-X adds the architectural index across the full
  logical 24-bit data address.
- M=0 memory operations use two little-endian bytes and preserve the adjacent byte
  in M=1.
- Aggregate instruction cycles follow the WDC base table and documented additions.
- A missing write callback produces `MIPSNES2_CPU_INVALID_BUS`, does not fetch the
  operand after the opcode and adds no completed-instruction cycles.

## Automated coverage

`tests/test_cpu_bus.c` executes every new opcode at M=1 and M=0. Targeted cases
cover carry-in/out for shifts and rotates, zero/negative results, unrelated flags,
accumulator B preservation, INC/DEC wrap, TSB/TRB overlap and non-overlap, STZ flag
preservation, Direct Page DL timing, DBR selection, linear word bank crossing,
absolute-X 24-bit carry, fixed indexed timing with X=0, adjacent-byte preservation,
16-bit memory writes and missing-write-bus diagnostics.

## Local verification

Environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14.

- `make clean && make test` — passed; `test-cpu-bus` reported the 223-opcode subset.
- `make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' clean && make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test` — passed.
- `make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test` — passed; leak detection was disabled because this managed runtime cannot expose the required process inspection.
- `python3 tools/check_repository.py && git diff --check` — passed.
- A source audit counted 223 dispatch cases and 223 unique opcode values.
- Changed source/tests contained no TODO, FIXME, placeholder, platform include,
  RFAuds2 dependency or game-specific ROM-name/CRC branch.
- Local Clang — unavailable (`clang` is absent); GitHub Clang remains required.
- `make -C platform/ps2 clean && make -C platform/ps2` — unavailable because
  `PS2SDK` is unset; it stopped before compilation.
- `make -C platform/psp clean && make -C platform/psp` — unavailable because
  `psp-config` is absent; it stopped before compilation.

## GitHub Actions

Implementation head: `7649664d488687436daeac1a41969cd8af402610`.

- Host CI run `38978604614` — passed GCC/Clang release, sanitizer, tests and artifact jobs.
- PS2 CI run `38978604587` — passed the pinned PS2SDK cross-build and artifact job.
- PSP CI run `38978604610` — passed the pinned PSPSDK cross-build and artifact job.

GitHub reported these artifact records:

- Host Clang `12079862204`, 17,777-byte archive, SHA-256 `e8d45899429bd4b04b3be9c9c260c62a237df16c904a22f8993d7421dfe04c83`.
- Host GCC `12079842304`, 18,300-byte archive, SHA-256 `41243398ca63f24b3b842c184cbc7c4d4415d70e29d5ebc3f8a9050c7b0f0bb9`.
- PS2 `12079803122`, 473,052-byte archive, SHA-256 `edf96d3646ccee2852469d728e314e4a3ac3554a6f8fc8a516b4246f4a55f5b6`.
- PSP `12079443942`, 98,982-byte archive, SHA-256 `18b159764555f934a04ad53c79f689bffaf36a59909df572a74b31007f1ab2d1`.

The artifact records and GitHub-reported digests were inspected, but repeated HTTP
502 responses prevented downloading the ZIPs in this session. Therefore their
internal `SHA256SUMS` files and executable/package sizes are not claimed as inspected.
Cross-build success is compilation evidence only, not console execution.

After PR #24 was retargeted to `main`, integration run `38979028919` failed both
Host jobs in the repository checker because the automatic merge result retained a
newer `main` identity statement but lost the separately required exact SNES9X-port
warning. PS2 run `38979029070` and PSP run `38979028956` passed, but the change was
not merged. The branch was reconciled to preserve both identity statements; a new
full workflow matrix is required before integration.

## Deliberate limits

The reference bus exposes final reads and writes, not the W65C816's complete
cycle-by-cycle read-modify-write sequence. Dummy/modify writes, VDA/VPA behavior and
MLB lock signaling are not modeled. `instruction_cycles` is aggregate W65C816
processor timing, not Ricoh 5A22 master timing. No console runtime or performance
measurement is claimed.
