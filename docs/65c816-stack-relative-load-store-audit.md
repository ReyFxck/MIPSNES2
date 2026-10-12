# 65C816 stack-relative LDA/STA audit

Date: 2026-10-12
Branch: `feature/65c816-stack-relative-load-store`
Stacked base: `feature/65c816-stack-relative-alu` commit
`55f8dff14da4240b22d40fd04eed678e8362f546`

## Scope

This increment adds exactly four opcodes to the platform-independent reference C
interpreter:

- STA stack-relative `83` and stack-relative-indirect-indexed `93`;
- LDA stack-relative `A3` and stack-relative-indirect-indexed `B3`.

The verified interpreter subset grows from 153 to 157 opcodes. LDA and STA now have
every architectural opcode represented. Stack-relative addressing remains incomplete:
ADC `63`/`73` and SBC `E3`/`F3` are outside this milestone. Stack push/pull
instructions, broader arithmetic, interrupts, bus mapping, cartridge, PPU, APU,
DMA/HDMA, enhancement chips, dynarec, assembly optimization and RFAuds2 runtime
integration are also outside scope.

## Behavior source

Behavior was independently implemented from the Western Design Center
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf):

- sections 2.4, 2.5, 2.7, 2.8 and 2.11 for A, DBR, Y, flags and S;
- sections 3.5.23 and 3.5.24 for `offset,S` and `(offset,S),Y` address formation;
- Table 3-1 and timing entries 23/24 for two-byte length and 4/7-cycle bases;
- Tables 5-4 and 5-5 for opcode assignments, operations and status effects;
- timing note 1 for the additional M=0 data-width cycle;
- caveat 7.1 for the bank-zero stack address range.

The unsigned 8-bit offset is added to the full 16-bit S value and wraps modulo 16
bits in bank zero. Direct 16-bit stack-relative loads and stores keep both data bytes
in bank zero, including the `00:FFFF` to `00:0000` wrap. The indirect-indexed form
reads a little-endian 16-bit pointer from bank zero, combines it with DBR, then adds
Y across the full 24-bit address space. Its final data access is linear. Neither mode
receives a Direct Page, page-cross or X-width cycle penalty. LDA updates only N/Z at
the selected M width and preserves B in 8-bit mode; STA does not change status. No
emulator implementation or external ROM was copied or committed.

## Verification coverage

Deterministic host tests cover:

- all four new opcodes in native 8-bit and 16-bit accumulator modes;
- LDA selected-width N/Z behavior and 8-bit B preservation;
- STA selected-width writes, exact status preservation and unchanged accumulator;
- two-byte PC advancement and exact 4/7-cycle bases plus the M=0 width cycle;
- full 16-bit `S + offset` formation and wrap;
- direct 16-bit data wrap from `00:FFFF` to `00:0000` for reads and writes;
- bank-zero pointer wrap, DBR selection and Y carry from `7F:FFF0` to `80:0010`;
- emulation-mode `01F0 + 30 = 0220`, without Direct Page-style page wrapping;
- absence of Direct Page, page-cross and X-width cycle additions.

Local environment: GCC 13.3.0, GNU Make 4.3 and Python 3.12.14. Clang is not
installed locally, so the GitHub Clang job remains required. Console cross-builds
reuse the repository-pinned PS2 toolchain archive v2.0.0 and PSP toolchain archive
v20261001; both report cross GCC 15.2.0.

| Command | Result |
|---|---|
| `make clean && make test` | Passed repository checks, contracts, all 157 implemented CPU opcodes and host smoke test |
| `make BUILD_DIR=build/host-release CFLAGS='-O2 -DNDEBUG' clean` then the matching `make ... test` | Passed release host build and full test suite |
| `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/host-sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test` after a clean sanitizer build | Passed ASan+UBSan host build and full test suite; LeakSanitizer remains disabled in this restricted environment |
| `make clean && make all` in `platform/ps2` with pinned `PS2DEV`/`PS2SDK` | Passed; produced a 1,396,652-byte `mipsnes2-ps2.elf` |
| `make clean && make all` in `platform/psp` with pinned `PSPDEV` | Passed; produced a 183,474-byte `EBOOT.PBP` |
| `python3 tools/check_repository.py` | Passed required-file, license, identity, link, checkbox, boundary and action-pin checks |

GitHub Actions execution and artifact inspection remain pending until the branch is
published. Their results will be recorded before the pull request is described as
fully verified.

## Validation limits

Host unit tests do not establish complete CPU correctness. No PCSX2, real PS2,
PPSSPP or real PSP runtime test is claimed. Cross-compilation is not execution. No
performance or commercial-game compatibility result exists. The broad addressing,
timing, regression and complete-interpreter tasks remain in progress.
