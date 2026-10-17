# 65C816 Complete Opcode Dispatch Audit

Date: 2026-10-17

## Scope

This milestone adds the eight previously absent W65C816 opcode values:

| Opcode | Instruction | Reference behavior added |
|---:|---|---|
| `00` | BRK | Signature fetch, mode-specific stack and vector entry |
| `02` | COP | Signature fetch, mode-specific stack and vector entry |
| `40` | RTI | Restore P/PC and native-mode PBR |
| `42` | WDM | Consume signature byte and preserve state |
| `44` | MVP | Descending, resumable byte transfer |
| `54` | MVN | Ascending, resumable byte transfer |
| `CB` | WAI | Wait state with IRQ/NMI/RESET release rules |
| `DB` | STP | Clock-stop state released only by RESET |

The same change adds caller-driven level IRQ and latched-edge NMI inputs so WAI and
RTI can be tested through real interrupt entry. It does not implement ABORT, Ricoh
5A22 interrupt-register generation, signal-level RESET, master-clock timing,
dummy/internal bus cycles, MLB, DMA arbitration or the central scheduler.

## Source and independent implementation

Behavior was independently implemented from Western Design Center's
[W65C816S Datasheet, revision March 13, 2024](https://www.westerndesigncenter.com/wdc/documentation/w65c816s.pdf).
The opcode/cycle table, interrupt vector table, interrupt bus sequences, processor
status table, block-move descriptions and WAI/STP sections define the expectations.
No emulator implementation was consulted or copied. No external ROM or vector corpus
is committed by this milestone.

## Contracts verified

- BRK/COP are two-byte instructions and stack the PC after the signature.
- Emulation entry pushes PCH/PCL/P with `01xx` wrapping; native entry first pushes PBR.
- BRK, COP, IRQ and NMI clear D, set I, force PBR to zero and use their mode-specific vectors.
- Emulation-mode BRK stacks bit 4 set; COP and hardware interrupt entry stack it clear.
- RTI restores P and PC, restores PBR only in native mode and reapplies E/M/X invariants.
- WDM consumes one signature byte and otherwise preserves processor state.
- MVP/MVN fetch destination then source bank, set DBR, update A/X/Y and consume seven cycles per byte.
- Block moves perform one byte per step and rewind PC until the 16-bit A count wraps to `FFFF`.
- WAI idles without bus or cycle progress, wakes for masked IRQ without vectoring, and vectors for NMI/unmasked IRQ.
- STP ignores IRQ/NMI and remains stopped until RESET.
- NMI has priority over an simultaneously asserted IRQ at the instruction boundary.
- A missing write callback stops BRK/COP/MVP/MVN before operand side effects.

## Local verification

The following checks passed in this worktree:

```text
make clean && make test
make BUILD_DIR=build/release CFLAGS='-O2 -DNDEBUG' test
ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test
```

Meaningful output:

```text
PASS: required files, license, identity, links, checkboxes, core boundaries and action pins
PASS: logical address and logging contracts (not SNES emulation)
PASS: logical bus and all 256 65C816 opcode values
PASS: host banner and unsupported argument rejection
```

The initial sanitizer execution compiled but LeakSanitizer failed because this
environment prevents its `/proc` thread inspection under ptrace. The clean rerun
disabled leak detection only; AddressSanitizer and UndefinedBehaviorSanitizer stayed
enabled and passed. Local Clang was unavailable. Local PS2 failed immediately with
`PS2SDK is unset`; local PSP failed because `psp-config` was unavailable. Pinned
GitHub Host/PS2/PSP workflow results will be added after they run for the final
commit. Cross-compilation is not runtime validation; no PCSX2, real PS2, PPSSPP or
real PSP result is claimed.

## Honest completion boundary

The byte decoder now has explicit behavior for all 256 W65C816 opcode values. This
completes opcode enumeration, not the entire SNES CPU subsystem. Phase 2 remains
open for ABORT, RESET/bus-cycle fidelity, independent validation and integration
with the future deterministic scheduler and Ricoh 5A22 timing model.
