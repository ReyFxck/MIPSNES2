# Host development target

The host is for compilation, unit/regression tests, future test-ROM execution,
fuzzing, debugging and tooling only. No GUI, SDL, desktop emulator UX or desktop-led
architecture is introduced. The binary identifies the CPU/bus foundation and rejects
arguments; it does not load ROMs.

Future host audio work is for deterministic offline sample tests and diagnostics
behind the common PCM contract. It remains independent of RFAuds2 and must be able to
validate S-DSP output without any physical PS2/SPU2 device.

Requirements: C99 GCC/Clang, GNU Make, Python 3.10+. No third-party C test framework.

```sh
make
make test
make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test
make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' test
make BUILD_DIR=build/clang CC=clang test
```

Use separate BUILD_DIR for changed compilers/flags; Make does not track flag changes.
The compiler's sanitizer runtime must be installed for sanitizer builds. Failures
must be investigated, not hidden. `make clean` removes the selected build directory.
`make check` runs only repository consistency. Tests are headless and fail with
nonzero exit status; they deliberately avoid NDEBUG-sensitive assertions.

Current coverage includes 24-bit address construction/masking across all banks and
offsets; logging callback/context/disabled behavior; host banner/argument rejection;
repository structure/boundaries/checklists; the reference bus; and the 223-opcode CPU
subset described in [65c816.md](65c816.md). CPU regressions include implemented
8/16-bit widths, flags, cycles, Direct Page/indexed/pointer wrapping and penalties,
regular-indirect DBR selection, long-indirect bank selection, absolute indexing and
linear 24-bit data crossing. Register-transfer regressions cover 8/16-bit and mixed
M/X destinations, high-byte preservation/clearing, zero extension, N/Z and cycles.
Special-transfer regressions add fixed-width C/D/S rules, emulation/native stack-high
behavior, X-selected TSX/TXS behavior and XBA byte/flag rules. They verify only the
implemented CPU/bus scope and remain independent of platform devices. Branch
regressions cover every conditional opcode plus BRA/BRL, taken and non-taken paths,
signed 8/16-bit offsets, PBR-local PC wrapping, flag preservation and exact native/
emulation cycle differences. Status-control regressions cover isolated D/I/V changes,
unrelated-state preservation and exact two-cycle behavior in both CPU modes.
Register increment/decrement regressions cover M/X-selected 8/16-bit wrapping, B
preservation, index zero extension, isolated N/Z changes and fixed two-cycle timing.
Logic/compare regressions cover all 23 base-group opcodes at 8/16-bit widths,
selected-width N/Z/C/V behavior, preserved operands/flags, Direct Page penalties,
indexed BIT timing and linear word reads across a bank boundary.
Extended accumulator-ALU regressions cover every non-stack-relative indexed, indirect
and long ORA/AND/EOR/CMP form at both M widths, Direct Page and index-width penalties,
page/bank carry, pointer-bank selection, emulation-mode Direct-X wrapping, result/
comparison flags and operand preservation.
Stack-relative accumulator-ALU regressions cover all eight ORA/AND/EOR/CMP forms at
both M widths, full 16-bit `S + offset` formation, bank-zero word/pointer wrapping,
DBR plus Y 24-bit carry, native/emulation behavior and fixed 4/7-cycle bases without
Direct Page, page-cross or X-width penalties. Stack-relative LDA/STA regressions add
8/16-bit reads and writes, B/N/Z/status behavior and bank-zero word wrap for their
four forms.
ADC/SBC regressions cover all thirty addressing forms at both M widths. They verify
carry/borrow inputs, unsigned carry/no-borrow, signed overflow, N/Z, 8-bit B
preservation, Direct Page and stack-relative bank-zero rules, DBR and long-pointer
bank selection, pre/post indexing, 24-bit carry, PC movement and exact width/DL/
indexed cycle additions.
Decimal arithmetic regressions exhaust 40,000 valid 8-bit packed-BCD ADC/SBC
operand/carry combinations, add targeted 16-bit and invalid-digit cases, and execute
all thirty addressing forms at both M widths with D=1. A separate read-only audit
compares result and C/V/N/Z behavior against 19,970 decimal immediate vectors from
a pinned MIT-licensed SingleStepTests revision.
Read-modify-write regressions execute all 36 ASL/LSR/ROL/ROR accumulator/memory,
memory INC/DEC, TSB/TRB and STZ opcodes at both M widths. They cover result and flag
isolation, B preservation, Direct Page bank/DL rules, DBR addressing, linear word
and indexed 24-bit carry, adjacent-byte preservation, aggregate cycles and a missing
write bus that stops before operand fetch. They do not claim bus-cycle dummy writes
or MLB signaling.
