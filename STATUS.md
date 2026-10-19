# STATUS — canonical project state

Project foundation plus a 256-opcode 65C816, partial real SNES logical bus, base
LoROM/HiROM and Mode 25/35 ExHiROM mapping, explicit internal-header analysis and deterministic scheduler/
timing foundation with a partial non-interlaced H/V model. No ROM loader or runnable
SNES machine, complete emulated subsystem,
console runtime validation,
compatibility result or hardware benchmark exists. Read
[bootstrap audit](docs/bootstrap-audit.md) and the CPU/bus evidence in
[docs/65c816-foundation-audit.md](docs/65c816-foundation-audit.md)
and [docs/snes-bus-timing-audit.md](docs/snes-bus-timing-audit.md), plus the
[base-cartridge audit](docs/cartridge-foundation-audit.md) and
[header-analysis audit](docs/cartridge-header-analysis-audit.md). Checksum/selection
and executable irregular-map evidence are recorded in
[docs/cartridge-mapper-selection-audit.md](docs/cartridge-mapper-selection-audit.md)
and [docs/cartridge-irregular-mapping-audit.md](docs/cartridge-irregular-mapping-audit.md).
Mode 25/35 evidence is in
[docs/cartridge-exhirom-audit.md](docs/cartridge-exhirom-audit.md). CPU-I/O and H/V
timing evidence is in
[docs/cpu-io-foundation-audit.md](docs/cpu-io-foundation-audit.md) and
[docs/hv-timing-foundation-audit.md](docs/hv-timing-foundation-audit.md). Stable-
register H/V timer-match evidence is in
[docs/hv-irq-foundation-audit.md](docs/hv-irq-foundation-audit.md), and shared
scheduler/H/V advancement evidence is in
[docs/master-timing-fanout-audit.md](docs/master-timing-fanout-audit.md).

## State and evidence rules

- [ ] Not implemented or not verified
- [x] Implemented AND verified
- [ ] [IN PROGRESS] Work started but incomplete
- [ ] [BLOCKED] Blocked — reason must be included

These four lines are a notation legend, not project progress. Only the task rows
below carry project state. A check requires implementation, successful applicable
build/tests, no relevant placeholder, updated docs and target validation where
required. Parent work remains incomplete while required children are unfinished.

Verification gates for unchecked future tasks: hardware behavior requires sourced
semantics and edge-case regressions; subsystem integration requires deterministic
integration tests; console runtime and speed require the named real device;
optimization requires baseline, regression and repeat measurement on that target;
GitHub execution requires a successful run URL, artifact/release inspection where
applicable. Research/review tasks require a written evidence-based decision, including
an explicit justified rejection if not applicable. Administrative tasks require
inspection of the actual setting. Documents/structure require content/link audit.

## Foundation

- [ ] [IN PROGRESS] Phase 0 foundation fully verified — repository bootstrap verified; release/admin activation pending
- [x] Host build system
- [x] Minimal host target compiles
- [x] Minimal automated test runs
- [x] Logging contract and disabled configuration
- [x] Shared logical-address and bus interface contracts
- [x] PS2 scaffolding uses verified SDK conventions
- [x] PSP scaffolding uses verified SDK conventions
- [x] PS2 scaffold cross-compiles
- [x] PSP scaffold cross-compiles
- [x] GPL-3.0 LICENSE preserved byte-for-byte

## Documentation

- [x] README.md
- [x] PROJECT_GOALS.md
- [x] ARCHITECTURE.md
- [x] STATUS.md
- [x] ROADMAP.md
- [x] PERFORMANCE.md
- [x] COMPATIBILITY.md
- [x] CONTRIBUTING.md
- [x] AGENTS.md
- [x] docs/65c816.md
- [x] docs/bus.md
- [x] docs/ppu.md
- [x] docs/spc700.md
- [x] docs/dsp.md
- [x] docs/apu.md
- [x] docs/dma-hdma.md
- [x] docs/scheduler.md
- [x] docs/cartridge.md
- [x] docs/enhancement-chips.md
- [x] docs/ps2.md
- [x] docs/psp.md
- [x] docs/host.md
- [x] docs/optimization.md
- [x] docs/testing.md
- [x] docs/debugging.md
- [x] docs/github-setup.md
- [x] docs/references.md — curated non-emulator hardware, independent-test and SDK sources

## Repository Structure

- [x] core/
- [x] core/cpu/65c816/
- [x] core/bus/
- [x] core/ppu/
- [x] core/apu/spc700/
- [x] core/apu/dsp/
- [x] core/dma/
- [x] core/scheduler/
- [x] core/cartridge/
- [x] core/enhancement/
- [x] core/debug/
- [x] core/common/
- [x] platform/ps2/
- [x] platform/psp/
- [x] platform/host/
- [x] tests/
- [x] tools/

## Architecture Validation

- [x] Core has no PS2 API dependency
- [x] Core has no PSP API dependency
- [x] Host does not control architecture
- [x] Dependency direction documented
- [x] PS2 dedicated optimization policy documented
- [x] PSP dedicated optimization policy documented
- [x] No game-specific hacks
- [x] No copied emulator implementation
- [x] No fake completed subsystem
- [x] No invented SDK APIs

## GitHub Infrastructure

- [x] Host CI workflow file audited
- [x] PS2 workflow file audited
- [x] PSP workflow file audited
- [x] Pre-release workflow file audited
- [x] Artifact upload configuration audited
- [x] SHA-256 workflow support audited
- [x] GitHub Actions host CI
- [x] Automated host tests in GitHub Actions
- [x] PS2 CI build
- [x] PSP CI build
- [x] Build artifacts
- [x] SHA-256 generation
- [x] Dependency/toolchain caching
- [ ] [BLOCKED] Development pre-release workflow end-to-end — requires reviewed main and a manual release run
- [x] Issue templates
- [x] Pull request template
- [x] Dependabot configuration
- [ ] Dependabot service execution
- [x] GitHub setup documentation
- [ ] [BLOCKED] Main branch ruleset configured — owner/admin must configure after checks exist
- [ ] Labels created
- [ ] Milestones created
- [x] First full CI run verified
- [x] Bootstrap PR opened

## Verification snapshot

Local foundation audit: GCC 13.3.0 host default/release tests pass; ASan+UBSan pass
with local LeakSanitizer disabled because the runtime cannot inspect /proc tasks.
PS2 and PSP scaffolds cross-compile using pinned archives (both cross GCC 15.2.0).
Actionlint 1.7.12 and YAML parse checks pass. No console runtime has been tested.
Exact commands and every checkbox promotion are in docs/bootstrap-audit.md.
First full PR CI passed at e70bca9 (GCC/Clang including full sanitizers, PS2/PSP).
Node 24 Actions also passed at 21d568f, including GCC/Clang full sanitizers and
PS2/PSP builds, artifact/checksum generation and verified warm-cache reuse. Run URLs
and exact evidence are in docs/bootstrap-audit.md. Remaining release/admin work is
explicitly separated below and in docs/github-setup.md.

The 65C816 foundation audit on 2026-10-10 passed GCC host release tests, local
ASan+UBSan (with only LeakSanitizer disabled for the documented `/proc` runtime
restriction), and pinned PS2/PSP cross-builds including the new CPU/bus sources.
Local Clang was unavailable; GitHub's existing Clang/full-sanitizer job remains the
gate. Exact commands and limitations are in docs/65c816-foundation-audit.md.

The addressing/load-store audit on 2026-10-10 passed GCC host debug/release tests,
local ASan+UBSan with the same LeakSanitizer restriction, and pinned PS2/PSP
cross-builds. It verifies 27 opcodes and corrects implemented data accesses to linear
24-bit bank crossing. The first GitHub Actions execution passed Host GCC/Clang,
sanitizers and pinned PS2/PSP cross-build jobs. Exact local and CI evidence is in
docs/65c816-addressing-audit.md.

The Direct Page load/store audit on 2026-10-10 verifies 12 additional opcodes on
host GCC, including bank-zero selection, E/DL wrapping, 8/16-bit data and DL/width
cycle additions. Pinned local PS2/PSP cross-builds pass. The first GitHub Actions
execution passed Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP
cross-build, artifact and checksum jobs. Exact evidence is in
docs/65c816-direct-page-audit.md; this does not complete Direct Page addressing.

The Direct-indirect load/store milestone on 2026-10-10 verifies ten additional LDA/
STA opcodes on host GCC. Tests distinguish regular 16-bit pointer page wrapping from
the Direct-indirect-long linear pointer-byte exception, DBR from pointer-supplied data
banks, pre/post indexing, 8/16-bit data and documented cycle additions. GitHub Host
GCC/Clang release and sanitizer jobs plus pinned PS2/PSP cross-build, artifact and
checksum jobs passed for the implementation commit. Exact evidence is recorded in
docs/65c816-direct-indirect-audit.md; indirect addressing remains incomplete.

The A/X/Y register-transfer milestone on 2026-10-10 verifies TAX/TAY/TXA/TYA/TXY/TYX
on host GCC. Tests cover emulation/native and mixed M/X widths, destination high-byte
rules, full-source behavior, zero extension, N/Z, preserved status bits, PC movement
and two-cycle timing. Pinned PS2SDK/PSPSDK cross-builds compile the changed core and
pass. GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP cross-build,
artifact and checksum jobs passed for the implementation commit. Exact evidence and
limits are in docs/65c816-register-transfer-audit.md.

The special-transfer milestone on 2026-10-11 verifies TCS/TSC/TCD/TDC/TXS/TSX/XBA
on host GCC debug, release and ASan+UBSan builds. Tests cover fixed-width C/D/S
transfers, native/emulation stack-high rules, X-selected TSX/TXS behavior, XBA flags,
PC movement and exact base cycles. Pinned PS2SDK/PSPSDK cross-builds compile the
changed core and pass. GitHub Host GCC/Clang release and sanitizer jobs plus pinned
PS2/PSP cross-build, artifact and checksum jobs passed for the implementation commit.
Exact evidence and limits are in docs/65c816-special-transfer-audit.md.

The branch-family milestone on 2026-10-11 verifies BPL/BMI/BVC/BVS/BCC/BCS/BNE/BEQ,
BRA and BRL on host GCC debug, release and ASan+UBSan builds. Tests cover taken and
non-taken conditions, signed 8/16-bit offsets, unchanged status, PBR-local PC
wrapping, and exact 2/3/4-cycle behavior in native and emulation modes. Pinned
PS2SDK/PSPSDK cross-builds compile the changed core and pass. GitHub Host GCC/Clang
release and sanitizer jobs plus pinned PS2/PSP cross-build, artifact and checksum
jobs passed for the implementation commit. Exact evidence and validation limits are
in docs/65c816-branch-audit.md.

The implied status-control milestone on 2026-10-11 verifies CLD/SED, CLI/SEI and CLV
on host GCC debug, release and ASan+UBSan builds. Tests cover exact D/I/V changes,
unrelated-status and register preservation, one-byte PC movement, fixed two-cycle
timing and identical behavior in emulation/native modes. Pinned PS2SDK/PSPSDK
cross-builds compile the changed core and pass. Exact evidence and validation limits
are in docs/65c816-status-control-audit.md. GitHub Host GCC/Clang release and
sanitizer jobs plus pinned PS2/PSP cross-build, artifact and checksum jobs passed for
the implementation commit.

The accumulator/index increment-decrement milestone on 2026-10-11 verifies
INC A/DEC A, INX/DEX and INY/DEY on host GCC debug, release and ASan+UBSan builds.
Deterministic regressions cover emulation and native modes, M/X-selected 8/16-bit
wraparound, accumulator B preservation, 8-bit index zero extension, isolated N/Z
changes, one-byte PC movement and fixed two-cycle timing. Pinned PS2SDK/PSPSDK
cross-builds compile the changed core and pass. Exact evidence and validation limits
are in docs/65c816-increment-decrement-audit.md. GitHub Host GCC/Clang release and
sanitizer jobs plus pinned PS2/PSP cross-build, artifact and checksum jobs passed for
the implementation commit.

The logic/compare foundation milestone on 2026-10-11 adds 23 verified opcodes: base
immediate/Direct/absolute ORA, AND, EOR and CMP forms plus every CPX, CPY and BIT
form. Host GCC debug, release and ASan+UBSan tests cover 8/16-bit M/X selection,
N/Z/C/V isolation, preserved operands/B, Direct Page penalties, indexed BIT timing
and linear word reads across a bank boundary. Pinned PS2SDK/PSPSDK cross-builds
compile the 105-opcode core and produce nonempty target artifacts. Exact local
evidence and validation limits are in docs/65c816-logic-compare-audit.md. GitHub
Host GCC/Clang plus pinned PS2/PSP jobs passed, and downloaded artifacts/checksums
were inspected for the implementation commit.

The extended ORA/AND addressing milestone on 2026-10-12 adds 20 verified opcodes,
covering every indexed, indirect and long ORA/AND form that does not require the
still-unimplemented stack-relative address paths. Host GCC debug, release and
ASan+UBSan tests cover both M widths, N/Z and B preservation, Direct Page/index
penalties, regular versus long pointer banks, emulation Direct-X wrapping and 24-bit
page/bank carry. Pinned PS2SDK/PSPSDK cross-builds compile the 125-opcode core and
produce nonempty target artifacts. Exact local evidence and validation limits are
in docs/65c816-ora-and-audit.md. GitHub Host GCC/Clang plus pinned PS2/PSP jobs
passed for the implementation commit, and downloaded artifacts plus archive/internal
checksums were inspected.

The extended EOR/CMP addressing milestone on 2026-10-12 adds 20 verified opcodes,
covering every indexed, indirect and long EOR/CMP form that does not require the
still-unimplemented stack-relative address paths. Host GCC debug, release and
ASan+UBSan tests cover both M widths, EOR result/B behavior, CMP operand/N/Z/C
behavior, Direct Page/index penalties, regular versus long pointer banks, emulation
Direct-X wrapping and 24-bit page/bank carry. Pinned PS2SDK/PSPSDK cross-builds
compile the 145-opcode core and produce nonempty target artifacts. Local evidence
is recorded in docs/65c816-eor-cmp-audit.md. GitHub Host GCC/Clang plus pinned
PS2/PSP jobs passed for the implementation commit, and downloaded artifacts plus
archive/internal checksums were inspected.

The stack-relative accumulator-ALU milestone on 2026-10-12 adds eight verified
opcodes and completes the ORA/AND/EOR/CMP instruction families. Host GCC debug,
release and ASan+UBSan tests cover both M widths, full 16-bit `S + offset` formation,
bank-zero operand/pointer wrapping, DBR plus Y 24-bit carry, native/emulation
behavior, flags and exact 4/7-cycle bases without Direct Page, page-cross or X-width
penalties. Pinned PS2SDK/PSPSDK cross-builds compile the 153-opcode core and produce
nonempty target artifacts. Exact local evidence and the initial expired-temporary-
path setup failure are recorded in docs/65c816-stack-relative-audit.md. GitHub Host
GCC/Clang plus pinned PS2/PSP jobs passed for the implementation commit, and
downloaded artifacts plus archive/internal checksums were inspected. Stack-relative
addressing at that milestone remained in progress because ADC/SBC/LDA/STA forms were
not yet implemented; the following milestone advances that state.

The stack-relative accumulator load/store milestone on 2026-10-12 adds four verified
opcodes and completes the LDA/STA instruction families. Host GCC debug tests cover
both M widths, 8-bit B preservation, LDA N/Z isolation, STA status preservation,
bank-zero word wrap, DBR plus Y 24-bit carry, native/emulation behavior and exact
4/7-cycle bases without Direct Page, page-cross or X-width penalties. Host GCC
release and ASan+UBSan tests pass; pinned PS2SDK/PSPSDK cross-builds compile the
157-opcode core and produce nonempty target artifacts. Exact local evidence is in
docs/65c816-stack-relative-load-store-audit.md. GitHub Host GCC/Clang plus pinned
PS2/PSP jobs passed for the implementation commit, and downloaded artifacts plus
archive/internal checksums were inspected. Stack-relative addressing remains in
progress because ADC/SBC forms are not implemented.

The binary immediate arithmetic milestone on 2026-10-12 adds ADC `69` and SBC `E9`
for M-selected 8/16-bit binary operation. Host GCC debug, release and ASan+UBSan
tests cover carry input and borrow convention, carry out/no-borrow, signed overflow,
N/Z, wrap, 8-bit B preservation, operand length and exact 2/3-cycle timing. D=1 stops
before operand fetch with `MIPSNES2_CPU_UNIMPLEMENTED_DECIMAL_ARITHMETIC`; decimal
behavior is not approximated. Pinned PS2 and PSP cross-builds compile and produce
nonempty artifacts. GitHub Host GCC/Clang plus pinned PS2/PSP jobs passed for the
implementation commit, and downloaded artifacts plus archive/internal checksums were
inspected. Exact evidence is in docs/65c816-binary-arithmetic-audit.md. At that
milestone all other ADC/SBC forms and decimal arithmetic remained incomplete.

The binary Direct Page/absolute arithmetic milestone on 2026-10-15 adds ADC `65/6D`
and SBC `E5/ED`. Deterministic host tests cover both M widths, Direct Page bank-zero
word wrapping, aligned and `DL != 0` cycle bases, absolute DBR selection, linear
absolute word reads across a bank boundary, C/V/N/Z, B preservation, PC and exact
cycles. All six recognized ADC/SBC opcodes stop before operand/address fetch when
D=1. Host GCC debug, release and ASan+UBSan tests pass; freshly SHA-256-verified pinned
PS2/PSP toolchains cross-compile nonempty target artifacts. GitHub Host GCC/Clang plus
pinned PS2/PSP jobs passed for the implementation commit, and downloaded artifacts
plus archive/internal checksums were inspected. Exact evidence is in
docs/65c816-binary-direct-absolute-audit.md. Indexed, indirect, long, stack-relative
and decimal ADC/SBC remain incomplete.

The binary indexed arithmetic milestone on 2026-10-15 adds ADC `75/79/7D` and SBC
`F5/F9/FD`. Deterministic host tests cover both M widths, Direct-X bank-zero wrap and
DL timing, absolute-X/Y DBR selection, page crossing, 16-bit index timing, linear
24-bit bank carry, C/V/N/Z, B preservation, PC and exact cycles. All twelve recognized
ADC/SBC opcodes stop before operand/address fetch when D=1. Local host and cross-build
tests pass. GitHub Host GCC/Clang plus pinned PS2/PSP jobs passed for the implementation
commit, and downloaded artifacts plus archive/internal checksums were inspected. Exact
evidence is in docs/65c816-binary-indexed-audit.md. Indirect, long, stack-relative
and decimal ADC/SBC remain incomplete.

The binary stack-relative arithmetic milestone on 2026-10-15 adds ADC `63/73` and
SBC `E3/F3`, completing both stack-relative accumulator addressing forms across
LDA/STA/ORA/AND/EOR/CMP/ADC/SBC. Deterministic host tests cover both M widths, full
16-bit `S + offset`, bank-zero word/pointer wrap, DBR plus Y 24-bit carry, native and
emulation modes, C/V/N/Z, B preservation, PC and exact 4/7-cycle bases. All sixteen
recognized ADC/SBC opcodes stop before operand/address fetch when D=1. Local evidence
and GitHub Host GCC/Clang plus pinned PS2/PSP jobs pass. Downloaded artifacts and
archive/internal checksums were inspected. Exact evidence is in
docs/65c816-binary-stack-relative-audit.md. Direct-indirect, long and decimal ADC/SBC
remain incomplete.

The binary regular Direct-indirect arithmetic milestone on 2026-10-15 adds ADC
`61/71/72` and SBC `E1/F1/F2`. Deterministic host tests cover both M widths,
bank-zero pointer wrap, DBR selection, X pre-indexing, Y post-indexing and 24-bit
carry, aligned/nonzero-DL timing, the shared page-cross-or-16-bit-index cycle,
C/V/N/Z, B preservation, PC and exact cycles. All twenty-two recognized ADC/SBC
opcodes stop before operand/address fetch when D=1. Local evidence is recorded in
docs/65c816-binary-direct-indirect-audit.md. GitHub Host GCC/Clang plus pinned PS2/PSP
jobs pass for the corrected published implementation, and downloaded artifacts plus
archive/internal checksums were inspected. Direct-indirect-long, absolute-long and
decimal ADC/SBC remain incomplete.

The binary long arithmetic milestone on 2026-10-16 adds ADC `67/6F/77/7F` and SBC
`E7/EF/F7/FF`, completing every architectural ADC/SBC addressing form for binary
operation. Deterministic host tests cover both M widths, Direct-indirect-long pointer
bytes wrapping in bank zero, pointer-bank selection independent of DBR, absolute-long
operand fetch, full 24-bit indexed carry, C/V/N/Z, B preservation, PC and exact
cycles without page-cross or X-width penalties. All thirty ADC/SBC opcodes stop
before operand/address fetch when D=1. Local evidence and successful GitHub Host
GCC/Clang plus pinned PS2/PSP cross-builds are recorded in
docs/65c816-binary-long-audit.md; downloaded artifacts and all checksums were
inspected. Decimal ADC/SBC calculation remains incomplete.

The decimal arithmetic milestone on 2026-10-16 implements packed-BCD calculation
for all thirty ADC/SBC opcodes at both M widths without changing addressing or cycle
rules. Repository tests exhaust 40,000 valid 8-bit operand/carry combinations and
cover targeted 16-bit and invalid-digit cases. The arithmetic algorithm was also
audited against 19,970 decimal-mode immediate ADC/SBC cases from a pinned,
MIT-licensed SingleStepTests revision with zero C/V/N/Z/result mismatches. Local
evidence is in docs/65c816-decimal-arithmetic-audit.md; GitHub workflow evidence is
verified for Host GCC/Clang plus pinned PS2/PSP cross-builds, and all downloaded
artifact archives and internal checksums were inspected.

The memory read-modify-write and shift/rotate milestone on 2026-10-16 adds all
thirty-six ASL/LSR/ROL/ROR accumulator/memory, memory INC/DEC, TSB/TRB and STZ
opcodes. Host regressions cover both M widths, C/N/Z isolation, TSB/TRB overlap
semantics, B and unrelated-state preservation, Direct Page bank/DL rules, absolute
and indexed 24-bit carry, 16-bit memory access, exact aggregate cycles and missing
write-bus diagnostics. W65C816 bus-cycle dummy writes and lock signaling remain
outside the current instruction-level timing contract. Local evidence is in
docs/65c816-memory-rmw-shifts-audit.md. GitHub Host GCC/Clang and pinned PS2/PSP
cross-build workflows passed for implementation head
`7649664d488687436daeac1a41969cd8af402610`; artifact metadata/digests were inspected,
but archive download inspection was blocked by repeated HTTP 502 responses.

The stack push/pull and subroutine-control milestone on 2026-10-17 adds 25 verified
opcodes and brings the reference interpreter to 248 dispatch entries. Deterministic
host regressions cover every stack/status/register push and pull, PEA/PEI/PER, every
direct/long/indirect JMP/JSR form, JSL, RTS and RTL in emulation and native modes.
They verify M/X-selected 8/16-bit widths, byte order, flags, PC/PBR and return-address
rules, ordinary `01xx` stack wrapping versus WDC's linear multi-byte exceptions,
Direct Page pointer wrapping and DL timing, aggregate cycles, meaningful JSL/JSR
`(absolute,X)` callback order/count and missing-write-bus stops before operand fetch.
Local GCC debug/release and ASan+UBSan tests pass; local PS2SDK/PSPSDK are unavailable,
so GitHub's pinned cross-build workflows are the platform build gate. GitHub Host
GCC/Clang plus pinned PS2/PSP workflows passed for implementation head
`182c31f40a536b99044a389fade89633c2ab4014`. Exact local and CI evidence, initial
resolved documentation-order failures and limitations are recorded in
docs/65c816-stack-control-audit.md. No console runtime or hardware result is claimed.

The final opcode-dispatch milestone on 2026-10-17 adds BRK, COP, RTI, WDM, MVP,
MVN, WAI and STP, bringing the reference interpreter to all 256 opcode values. It
also adds caller-driven IRQ/NMI inputs required to test wait and return behavior.
Host regressions verify emulation/native vectors and stack layouts, signature-byte
PC behavior, flags, meaningful bus order/count, aggregate cycles, native/8-bit-index
block moves, WAI wake rules, NMI priority and STP reset-only release. This does not
complete ABORT, signal-level RESET/bus cycles, Ricoh 5A22 timing or scheduler
integration. GitHub Host plus pinned PS2/PSP workflows passed for implementation
commit `74890c559b428643c787fda71358d2065bcd030f`. Exact evidence is in
docs/65c816-complete-opcodes-audit.md.

The scheduler foundation milestone on 2026-10-17 adds caller-owned 64-bit master-
clock time, a bounded deterministic event table, explicit priority/insertion tie
ordering, cancellation, event horizons and exact rational conversion. A separately
sourced Ricoh 5A22 classifier identifies 6/8/12-master-clock logical-address access
classes and FastROM-capable windows. Host GCC debug/release and ASan+UBSan tests pass,
including repeated traces, long chunked-ratio equivalence, timing boundaries and
scheduled IRQ/NMI delivery to WAI. The CPU still lacks per-bus-cycle timing, refresh
stalls and arbitration, so the broad timing/event-horizon tasks remain in progress.
GitHub Host GCC/Clang plus sanitizers and pinned PS2/PSP cross-build workflows passed
for implementation commit `e1dc763ad81601bdcfbc44885eee08b2f770dd0d`.
Exact local/CI evidence and limitations are in docs/scheduler-foundation-audit.md.

The partial SNES bus/timing milestone on 2026-10-17 adds caller-owned 128 KiB WRAM,
low-bank mirrors, WRAM ports, subsystem callback routes, `$420D` FastROM state and
address-dependent clocks for meaningful CPU accesses. An atomic timing bridge
advances the scheduler and exposes mismatch/overflow errors. Host tests cover map
boundaries, port side effects, FastROM, mixed-speed instructions and NMI delivery.
Exact dummy/internal cycle placement, WRAM refresh, DMA/HDMA arbitration, cartridge
board coverage and mid-instruction event pausing remain incomplete. GitHub Host
GCC/Clang plus sanitizers and pinned PS2/PSP cross-build workflows passed for implementation
commit `161aeb647fabce2b2e7ff7667d06b95204829daa`. Exact local/CI evidence and limits
are in docs/snes-bus-timing-audit.md. No console runtime or performance result is
claimed.

The base-cartridge milestone on 2026-10-17 adds explicit caller-owned LoROM/HiROM
ROM/SRAM views, standard base windows, deterministic power-of-two mirrors and a slot
response contract that preserves the bus latch for undecoded reads. Synthetic host
tests cover map boundaries, SRAM aliases, ROM write protection, invalid configuration
and reset/NOP fetch through cartridge, bus, CPU and scheduler. Automatic mapper
selection, file loading, irregular/multi-chip boards, Ex mappings, persistence and
enhancement hardware remain incomplete. GitHub Host GCC/Clang plus sanitizers and
pinned PS2/PSP cross-build workflows passed for implementation commit
`f5be6dec1b4fc9ebdf96564fb1736c6fcdcad032`. Exact evidence and limitations are in
docs/cartridge-foundation-audit.md. No commercial ROM, console runtime, compatibility
or performance result is claimed.

The cartridge image-analysis milestone on 2026-10-18 adds immutable views over clean
or 512-byte-prefixed payloads and caller-selected LoROM/HiROM internal-header parsing.
It extracts raw standard fields, safely decodes declared sizes, classifies only base
map modes and checks the checksum/complement relationship. Synthetic tests cover all
prefix policies, malformed sizes, candidate bounds, fields and hostile exponents.
Automatic candidate selection, extended headers, full checksum calculation and
hardware resolution remain incomplete. GitHub Host GCC/Clang plus sanitizers and
pinned PS2/PSP cross-build workflows passed for implementation commit
`add0e83ae5c5aee1680794ea420314cac3aa2b57`. Exact evidence and limitations are in
docs/cartridge-header-analysis-audit.md. No commercial ROM or runtime result is claimed.

The checksum/base-selection milestone on 2026-10-18 adds normalized whole-payload
checksums, unambiguous smaller-tail mirroring and conservative base LoROM/HiROM
selection. Map mode, base chipset, RESET vector, declared sizes, complement and actual
checksum must all agree; no-match and dual-valid ambiguity are explicit. Synthetic
tests cover each gate without title/opcode scoring or commercial data. Host GCC/Clang
release and sanitizer jobs plus pinned PS2/PSP scaffold cross-build, artifact and
checksum jobs passed for implementation commit
`b394209825d6c78ed2e08f86d7c142158e52ba6c`. Executable irregular boards, padded
ambiguous layouts, extended headers and special checksum conventions remain incomplete.
Exact evidence is in docs/cartridge-mapper-selection-audit.md. No console runtime,
compatibility or performance result is claimed.

The executable irregular-map milestone on 2026-10-18 extends the base mapper only to
bank-aligned images formed from one larger power-of-two region followed by one smaller
power-of-two tail. Initialization precomputes the mirror span; padded and board-specific
layouts are rejected. Generated host fixtures cover 96/160 KiB LoROM, 192 KiB HiROM,
tail repetition, span wrap, alignment/rejection and selected-header-to-mapper setup.
Local GCC default/release and ASan+UBSan tests pass. Local Clang and console SDKs were
unavailable. GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP
scaffold cross-build, artifact and checksum jobs passed for implementation commit
`2d9f03bb51dbb4248685e91fdfa61f85df28ffd3`. Exact evidence and limitations are in
docs/cartridge-irregular-mapping-audit.md. No console runtime, compatibility or
performance result is claimed.

The documented ExHiROM milestone on 2026-10-18 adds raw 16-byte registration fields,
Mode 25/35 classification at `$40FFC0`, conservative three-candidate selection and a
separate 4-8 MiB address transform with HiROM-style SRAM. Generated 6 MiB fixtures
cover first/extended regions, upper-half mirrors, bounded tail repetition, selection,
mapper initialization and CPU reset/NOP through the SNES bus. Local GCC default,
release and ASan+UBSan tests pass; local Clang and console SDKs are unavailable.
GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP scaffold
cross-build, artifact and checksum jobs passed for implementation commit
`9f333a97609857db808ec83a4f83f1a16429acdd`. Unofficial ExLoROM, physical cartridge
validation, special boards, file loading, persistence, console runtime, compatibility
and performance remain incomplete. Exact scope and evidence are in
docs/cartridge-exhirom-audit.md.

The cartridge-construction milestone on 2026-10-18 adds an allocation-free plan over
caller-owned file bytes, exposes exact header-declared SRAM requirements and binds a
validated mapper to caller-owned ROM/SRAM storage. Outputs remain unchanged on
failure and no file/path/platform API enters `core/`. Generated tests cover clean and
prefixed LoROM, missing/undersized/oversized SRAM capacity, failure transactionality,
tampered plans and selected 6 MiB ExHiROM construction. Local GCC default, release
and ASan+UBSan tests pass; local Clang and console SDKs are unavailable. GitHub Host
GCC/Clang release and sanitizer jobs plus pinned PS2/PSP scaffold cross-build,
artifact and checksum jobs passed for implementation commit
`cd55c07af49841751e67c750d1dceb9c70a8393f`. Platform file adapters, persistence
and ROM execution remain incomplete. Exact scope and evidence are in
docs/cartridge-construction-audit.md.

The SRAM-persistence-handoff milestone on 2026-10-18 keeps storage and filesystem
policy outside `core/` while exposing the mapper-visible caller-owned SRAM buffer,
dirty state and a revision token. Only value-changing writes advance the token;
acknowledging a stale revision cannot clear newer dirty data. Generated host tests
cover aliases, same-value writes, ROM/unmapped writes, stale and matching
acknowledgements, no-SRAM cartridges and deterministic revision wrap. Local GCC
default, release and ASan+UBSan suites pass; local Clang and console SDKs are
unavailable. GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP
scaffold cross-build, artifact and checksum jobs passed for implementation commit
`defebd3bf0ab741639e70a94e08a6fc65d18df0e`. Platform file loading, durable writes,
save scheduling and real-device validation remain incomplete. Exact scope is in
docs/cartridge-sram-persistence-audit.md.

The Ricoh 5A22 CPU-I/O foundation on 2026-10-18 adds caller-owned state for
NMITIMEN, the 9-bit H/V compare registers, RDNMI, TIMEUP and HVBJOY. It accepts
external blank transitions and timer-match signals, latches NMI edges, exposes the
timer IRQ level and is routed through the partial SNES bus without replacing the
fallback CPU-I/O interface. Deterministic tests cover reserved masks, open-bus bit
composition, mirror routing, enable transitions, read acknowledgements and blank/
joypad-busy status. GCC default, release and isolated ASan+UBSan host suites pass;
local Clang and console SDKs are unavailable. GitHub Host GCC/Clang release and
sanitizer jobs plus pinned PS2/PSP scaffold cross-build, artifact and checksum jobs
passed for implementation commit
`f0517d99ade3f1768fa803a8c3aa90bafab58370`. No H/V counter, PPU timing, automatic joypad poll,
external cartridge IRQ combination or exact TIMEUP match-window behavior is claimed.
Exact scope is in docs/cpu-io-foundation-audit.md.

The non-interlaced H/V timing foundation on 2026-10-19 adds caller-owned NTSC/PAL
master-clock phase, 262/312-line frames, HBlank/VBlank transitions, 224/239-line
VBlank selection, field alternation, the NTSC field-1 short scanline and the RDNMI/
NMI flag two master clocks after VBlank begins. The CPU-I/O input contract now keeps
VBlank status and NMI-flag assertion distinct. Deterministic tests cover exact frame
totals, boundaries, overscan, one-shot/chunked equivalence and overflow rejection.
GCC default, release and local ASan+UBSan suites pass; local Clang and console SDKs
are unavailable. GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP
scaffold cross-build, artifact and checksum jobs passed for implementation commit
`6ab26ca22c4d4ec838f513702b490a681196d610`. Interlace, PPU dot-counter latching,
dynamic SETINI and central-scheduler/CPU-boundary wiring remain incomplete. A later
milestone adds the stable-register H/V IRQ subset described below. Exact original
scope is in docs/hv-timing-foundation-audit.md.

The stable-register H/V timer-IRQ foundation on 2026-10-19 adds non-interlaced H,
V and HV match generation into the existing CPU-I/O TIMEUP latch. It covers every
valid HTIME value, 338/339 spill into the following line, V/HV positions including
the post-short-line offset, regional out-of-range values and the documented H=153
short/final-line suppressions. Exact mid-instruction register-write ordering,
multiple reprogrammed matches in one scanline, TIMEUP's narrow read-at-match exception,
interlace and automatic CPU interrupt-line delivery remain incomplete. Local GCC
default/release and ASan+UBSan verification suites pass. GitHub Host GCC/Clang plus
pinned PS2/PSP cross-build, artifact and checksum jobs passed for implementation
commit `9936fd24a2a8cfdd736d4fe7df707c11f0103e55`. Exact scope and run IDs are in
docs/hv-irq-foundation-audit.md. No console runtime or hardware result is claimed.

The shared master-clock fanout milestone on 2026-10-19 adds a caller-owned coordinator
that keeps the scheduler and non-interlaced H/V component on one checked timestamp.
It advances H/V transitions before general callbacks at the same time, preserves
domain alignment on event-budget exhaustion and lets the atomic CPU timing bridge
feed both from one instruction delta. Deterministic host fixtures cover mismatch and
error paths, same-time insertion, provisional tie ordering, chunking and repeated
NOP-to-H-IRQ advancement. The CPU remains atomic and CPU-I/O IRQ/NMI latches are not
yet wired automatically into CPU inputs. Local GCC default verification passes;
release and ASan+UBSan verification also pass. GitHub Host GCC/Clang plus pinned
PS2/PSP cross-build, artifact and checksum jobs passed for implementation commit
`f0c863d16c31140f8fd23352c9e68bffaf534c24`. Exact scope and run IDs are in
docs/master-timing-fanout-audit.md. No console runtime or hardware result is claimed.

## Current blockers and diagnostic exceptions

No game-specific diagnostics exist. The reference bus has a deterministic unmapped
latch for tests; it is not claimed as complete SNES open-bus behavior. RESET retains
storage fields the hardware source labels uninitialized rather than claiming values.
Real consoles/emulators are not available in this session. SDK/CI availability and
remaining admin work are recorded in docs/bootstrap-audit.md and docs/github-setup.md.

## 65C816

- [x] CPU state representation
- [ ] [IN PROGRESS] Reset sequence — sourced programmer-visible invariants/vector implemented; signal and cycle sequence incomplete
- [x] Instruction fetch
- [x] Opcode dispatch — all 256 byte values have explicit behavior
- [x] Immediate addressing
- [x] Absolute addressing
- [x] Long addressing — data, ALU and JMP/JSL long forms verified
- [x] Direct Page addressing
- [x] Stack-relative addressing — complete LDA/STA/ORA/AND/EOR/CMP/ADC/SBC families
- [x] Indexed addressing
- [x] Indirect addressing — data/ALU, PEI and all indirect JMP/JSR forms verified
- [x] Branch instructions
- [x] Jump/subroutine call/return instructions
- [x] Load/store instructions — scalar families and MVP/MVN verified
- [x] Arithmetic instructions — complete ADC/SBC, compare and register/memory INC/DEC families
- [x] Logic instructions — complete ORA/AND/EOR/BIT/TSB/TRB families
- [x] Shift/rotate instructions — complete ASL/LSR/ROL/ROR accumulator and memory families
- [x] Transfer instructions
- [x] Stack instructions
- [x] Status flag behavior
- [x] M flag behavior
- [x] X flag behavior
- [x] Native mode instruction behavior
- [x] Emulation mode instruction behavior
- [x] Decimal arithmetic — all ADC/SBC forms verified at both M widths, including valid and invalid packed-BCD digits
- [x] IRQ CPU input and entry — SNES IRQ generation/timing remains Phase 4
- [x] NMI CPU input and entry — SNES NMI generation/timing remains Phase 4
- [ ] ABORT input and entry
- [ ] [IN PROGRESS] RESET — vector and documented visible state subset only
- [x] BRK
- [x] COP
- [x] WAI
- [x] STP
- [x] Bank wrapping — program fetch/relative branches are bank-local; data/indexing is linear; Direct pointers remain in bank zero
- [x] Direct Page edge cases — E/DL wrap, long-pointer exception, unaligned-D carry and bank-zero wrap verified
- [ ] [IN PROGRESS] Timing model — aggregate opcode timing plus meaningful-access 5A22 clocks integrated; exact dummy/internal sequencing, refresh and arbitration pending
- [x] CPU automated tests — all 256 opcode values and implemented behavior covered
- [ ] [IN PROGRESS] CPU regression tests — deterministic host suite exists; broader independent vectors/test ROMs pending
- [ ] [IN PROGRESS] Complete interpreter integration — opcode dispatch and atomic scheduler/5A22 bridge exist; exact cycle boundaries and ABORT pending

### Verified 65C816 foundation subset

- [x] Caller-owned register/mode/bus/diagnostic state
- [x] Reset vector fetch from logical 00:FFFC-D
- [x] Pre-reset and invalid-bus diagnostic stops
- [x] BRK (`00`)
- [x] COP (`02`)
- [x] RTI (`40`)
- [x] WDM (`42`)
- [x] MVP (`44`)
- [x] MVN (`54`)
- [x] WAI (`CB`)
- [x] STP (`DB`)
- [x] PHP (`08`)
- [x] PHD (`0B`)
- [x] JSR absolute (`20`)
- [x] JSL absolute long (`22`)
- [x] PLP (`28`)
- [x] PLD (`2B`)
- [x] PHA (`48`)
- [x] PHK (`4B`)
- [x] PHY (`5A`)
- [x] JMP absolute long (`5C`)
- [x] RTS (`60`)
- [x] PER (`62`)
- [x] PLA (`68`)
- [x] RTL (`6B`)
- [x] JMP absolute indirect (`6C`)
- [x] PLY (`7A`)
- [x] JMP absolute-indexed-indirect (`7C`)
- [x] PHB (`8B`)
- [x] PLB (`AB`)
- [x] PEI direct (`D4`)
- [x] PHX (`DA`)
- [x] JMP absolute-indirect-long (`DC`)
- [x] PEA (`F4`)
- [x] PLX (`FA`)
- [x] JSR absolute-indexed-indirect (`FC`)
- [x] TSB direct (`04`)
- [x] ASL direct (`06`)
- [x] ASL accumulator (`0A`)
- [x] TSB absolute (`0C`)
- [x] ASL absolute (`0E`)
- [x] TRB direct (`14`)
- [x] ASL direct,X (`16`)
- [x] TRB absolute (`1C`)
- [x] ASL absolute,X (`1E`)
- [x] ROL direct (`26`)
- [x] ROL accumulator (`2A`)
- [x] ROL absolute (`2E`)
- [x] ROL direct,X (`36`)
- [x] ROL absolute,X (`3E`)
- [x] LSR direct (`46`)
- [x] LSR accumulator (`4A`)
- [x] LSR absolute (`4E`)
- [x] LSR direct,X (`56`)
- [x] LSR absolute,X (`5E`)
- [x] STZ direct (`64`)
- [x] ROR direct (`66`)
- [x] ROR accumulator (`6A`)
- [x] ROR absolute (`6E`)
- [x] STZ direct,X (`74`)
- [x] ROR direct,X (`76`)
- [x] ROR absolute,X (`7E`)
- [x] STZ absolute (`9C`)
- [x] STZ absolute,X (`9E`)
- [x] DEC direct (`C6`)
- [x] DEC absolute (`CE`)
- [x] DEC direct,X (`D6`)
- [x] DEC absolute,X (`DE`)
- [x] INC direct (`E6`)
- [x] INC absolute (`EE`)
- [x] INC direct,X (`F6`)
- [x] INC absolute,X (`FE`)
- [x] BPL (`10`)
- [x] TCS (`1B`)
- [x] NOP (`EA`)
- [x] LDA immediate (`A9`)
- [x] LDX immediate (`A2`)
- [x] LDY immediate (`A0`)
- [x] STA absolute (`8D`)
- [x] STX absolute (`8E`)
- [x] STY absolute (`8C`)
- [x] STA absolute long (`8F`)
- [x] STA absolute,Y (`99`)
- [x] STA absolute,X (`9D`)
- [x] STA absolute long,X (`9F`)
- [x] LDY absolute (`AC`)
- [x] LDA absolute (`AD`)
- [x] LDX absolute (`AE`)
- [x] LDA absolute long (`AF`)
- [x] LDA absolute,Y (`B9`)
- [x] LDY absolute,X (`BC`)
- [x] LDA absolute,X (`BD`)
- [x] LDX absolute,Y (`BE`)
- [x] LDA absolute long,X (`BF`)
- [x] CLC (`18`)
- [x] INC accumulator (`1A`)
- [x] BMI (`30`)
- [x] SEC (`38`)
- [x] DEC accumulator (`3A`)
- [x] TSC (`3B`)
- [x] REP (`C2`)
- [x] SEP (`E2`)
- [x] XCE (`FB`)
- [x] JMP absolute (`4C`)
- [x] BVC (`50`)
- [x] CLI (`58`)
- [x] TCD (`5B`)
- [x] BVS (`70`)
- [x] SEI (`78`)
- [x] TDC (`7B`)
- [x] BRA (`80`)
- [x] BRL (`82`)
- [x] STA direct-indexed-indirect (`81`)
- [x] STA stack-relative (`83`)
- [x] STY direct (`84`)
- [x] STA direct (`85`)
- [x] STX direct (`86`)
- [x] STA direct-indirect-long (`87`)
- [x] DEY (`88`)
- [x] TXA (`8A`)
- [x] BCC (`90`)
- [x] STA direct-indirect-indexed,Y (`91`)
- [x] STA direct-indirect (`92`)
- [x] STA stack-relative-indirect-indexed,Y (`93`)
- [x] STY direct,X (`94`)
- [x] STA direct,X (`95`)
- [x] STX direct,Y (`96`)
- [x] STA direct-indirect-long-indexed,Y (`97`)
- [x] TYA (`98`)
- [x] TXS (`9A`)
- [x] TXY (`9B`)
- [x] LDA direct-indexed-indirect (`A1`)
- [x] LDA stack-relative (`A3`)
- [x] LDY direct (`A4`)
- [x] LDA direct (`A5`)
- [x] LDX direct (`A6`)
- [x] LDA direct-indirect-long (`A7`)
- [x] TAY (`A8`)
- [x] TAX (`AA`)
- [x] BCS (`B0`)
- [x] LDA direct-indirect-indexed,Y (`B1`)
- [x] LDA direct-indirect (`B2`)
- [x] LDA stack-relative-indirect-indexed,Y (`B3`)
- [x] LDY direct,X (`B4`)
- [x] LDA direct,X (`B5`)
- [x] LDX direct,Y (`B6`)
- [x] LDA direct-indirect-long-indexed,Y (`B7`)
- [x] CLV (`B8`)
- [x] TSX (`BA`)
- [x] TYX (`BB`)
- [x] INY (`C8`)
- [x] DEX (`CA`)
- [x] BNE (`D0`)
- [x] CLD (`D8`)
- [x] INX (`E8`)
- [x] XBA (`EB`)
- [x] BEQ (`F0`)
- [x] SED (`F8`)
- [x] ORA direct-indexed-indirect (`01`)
- [x] ORA stack-relative (`03`)
- [x] ORA direct (`05`)
- [x] ORA direct-indirect-long (`07`)
- [x] ORA immediate (`09`)
- [x] ORA absolute (`0D`)
- [x] ORA absolute long (`0F`)
- [x] ORA direct-indirect-indexed,Y (`11`)
- [x] ORA direct-indirect (`12`)
- [x] ORA stack-relative-indirect-indexed,Y (`13`)
- [x] ORA direct,X (`15`)
- [x] ORA direct-indirect-long-indexed,Y (`17`)
- [x] ORA absolute,Y (`19`)
- [x] ORA absolute,X (`1D`)
- [x] ORA absolute long,X (`1F`)
- [x] AND direct-indexed-indirect (`21`)
- [x] AND stack-relative (`23`)
- [x] BIT direct (`24`)
- [x] AND direct (`25`)
- [x] AND direct-indirect-long (`27`)
- [x] AND immediate (`29`)
- [x] BIT absolute (`2C`)
- [x] AND absolute (`2D`)
- [x] AND absolute long (`2F`)
- [x] AND direct-indirect-indexed,Y (`31`)
- [x] AND direct-indirect (`32`)
- [x] AND stack-relative-indirect-indexed,Y (`33`)
- [x] BIT direct,X (`34`)
- [x] AND direct,X (`35`)
- [x] AND direct-indirect-long-indexed,Y (`37`)
- [x] AND absolute,Y (`39`)
- [x] BIT absolute,X (`3C`)
- [x] AND absolute,X (`3D`)
- [x] AND absolute long,X (`3F`)
- [x] EOR direct-indexed-indirect (`41`)
- [x] EOR stack-relative (`43`)
- [x] EOR direct (`45`)
- [x] EOR direct-indirect-long (`47`)
- [x] EOR immediate (`49`)
- [x] EOR absolute (`4D`)
- [x] EOR absolute long (`4F`)
- [x] EOR direct-indirect-indexed,Y (`51`)
- [x] EOR direct-indirect (`52`)
- [x] EOR stack-relative-indirect-indexed,Y (`53`)
- [x] EOR direct,X (`55`)
- [x] EOR direct-indirect-long-indexed,Y (`57`)
- [x] EOR absolute,Y (`59`)
- [x] EOR absolute,X (`5D`)
- [x] EOR absolute long,X (`5F`)
- [x] ADC direct-indexed-indirect, binary and decimal modes (`61`)
- [x] ADC stack-relative, binary and decimal modes (`63`)
- [x] ADC direct, binary and decimal modes (`65`)
- [x] ADC direct-indirect-long, binary and decimal modes (`67`)
- [x] ADC immediate, binary and decimal modes (`69`)
- [x] ADC absolute, binary and decimal modes (`6D`)
- [x] ADC absolute long, binary and decimal modes (`6F`)
- [x] ADC direct-indirect-indexed,Y, binary and decimal modes (`71`)
- [x] ADC direct-indirect, binary and decimal modes (`72`)
- [x] ADC stack-relative-indirect-indexed,Y, binary and decimal modes (`73`)
- [x] ADC direct,X, binary and decimal modes (`75`)
- [x] ADC direct-indirect-long-indexed,Y, binary and decimal modes (`77`)
- [x] ADC absolute,Y, binary and decimal modes (`79`)
- [x] ADC absolute,X, binary and decimal modes (`7D`)
- [x] ADC absolute long,X, binary and decimal modes (`7F`)
- [x] BIT immediate (`89`)
- [x] CPY immediate (`C0`)
- [x] CMP direct-indexed-indirect (`C1`)
- [x] CMP stack-relative (`C3`)
- [x] CPY direct (`C4`)
- [x] CMP direct (`C5`)
- [x] CMP direct-indirect-long (`C7`)
- [x] CMP immediate (`C9`)
- [x] CPY absolute (`CC`)
- [x] CMP absolute (`CD`)
- [x] CMP absolute long (`CF`)
- [x] CMP direct-indirect-indexed,Y (`D1`)
- [x] CMP direct-indirect (`D2`)
- [x] CMP stack-relative-indirect-indexed,Y (`D3`)
- [x] CMP direct,X (`D5`)
- [x] CMP direct-indirect-long-indexed,Y (`D7`)
- [x] CMP absolute,Y (`D9`)
- [x] CMP absolute,X (`DD`)
- [x] CMP absolute long,X (`DF`)
- [x] CPX immediate (`E0`)
- [x] SBC direct-indexed-indirect, binary and decimal modes (`E1`)
- [x] SBC stack-relative, binary and decimal modes (`E3`)
- [x] CPX direct (`E4`)
- [x] SBC direct, binary and decimal modes (`E5`)
- [x] SBC direct-indirect-long, binary and decimal modes (`E7`)
- [x] SBC immediate, binary and decimal modes (`E9`)
- [x] CPX absolute (`EC`)
- [x] SBC absolute, binary and decimal modes (`ED`)
- [x] SBC absolute long, binary and decimal modes (`EF`)
- [x] SBC direct-indirect-indexed,Y, binary and decimal modes (`F1`)
- [x] SBC direct-indirect, binary and decimal modes (`F2`)
- [x] SBC stack-relative-indirect-indexed,Y, binary and decimal modes (`F3`)
- [x] SBC direct,X, binary and decimal modes (`F5`)
- [x] SBC direct-indirect-long-indexed,Y, binary and decimal modes (`F7`)
- [x] SBC absolute,Y, binary and decimal modes (`F9`)
- [x] SBC absolute,X, binary and decimal modes (`FD`)
- [x] SBC absolute long,X, binary and decimal modes (`FF`)
- [x] Base-cycle accounting for the implemented subset
- [x] Linear 24-bit word access across bank boundaries
- [x] Absolute indexed page/write/X cycle rule for implemented opcodes
- [x] Direct Page bank-zero address formation for implemented opcodes
- [x] Emulation-mode aligned Direct Page wrapping for implemented opcodes
- [x] Direct Page DL and width cycle additions for implemented opcodes
- [x] Regular 16-bit Direct-indirect pointer wrapping for implemented opcodes
- [x] Direct-indirect-long linear pointer-byte exception for implemented opcodes
- [x] Indirect DBR versus long-pointer data-bank selection for implemented opcodes
- [x] Direct-indirect pre/post-index and cycle rules for implemented opcodes
- [x] Full-source and destination-selected M/X width for implemented transfers
- [x] Mixed M/X transfer preservation, truncation and zero extension
- [x] Register-transfer N/Z flags and two-cycle timing
- [x] Fixed-width C/D/S transfer behavior and 16-bit N/Z
- [x] Native/emulation TCS/TXS stack-high behavior
- [x] XBA byte exchange, 8-bit N/Z and three-cycle timing
- [x] Conditional branch taken/not-taken flag selection and status preservation
- [x] Signed 8-bit and 16-bit relative offsets from the advanced PC
- [x] Relative and relative-long PC wrapping within unchanged PBR
- [x] Conditional/BRA taken and emulation-only page-cross cycle additions
- [x] BRL fixed four-cycle timing
- [x] Implied D/I/V status-control isolation and two-cycle timing
- [x] Accumulator/index increment-decrement width, wrap, N/Z isolation and two-cycle timing
- [x] Base ORA/AND/EOR selected-width result, N/Z and B-preservation behavior
- [x] Non-stack ORA/AND indexed, indirect and long address/cycle behavior
- [x] Base CMP and complete CPX/CPY selected-width N/Z/C behavior
- [x] Non-stack EOR/CMP indexed, indirect and long address/cycle behavior
- [x] Stack-relative ORA/AND/EOR/CMP bank-zero address, pointer, DBR/Y and cycle behavior
- [x] Stack-relative LDA/STA bank-zero data wrap, pointer, DBR/Y, flag and cycle behavior
- [x] Binary immediate ADC/SBC carry, overflow, N/Z, B preservation and width behavior
- [x] Binary Direct Page/absolute ADC/SBC address, bank-wrap, flag, width and cycle behavior
- [x] Binary Direct-X/absolute-X/Y ADC/SBC address, page/index timing, bank-carry, flag, width and cycle behavior
- [x] Binary stack-relative ADC/SBC bank-zero data/pointer wrap, DBR/Y carry, flags, width and cycle behavior
- [x] Binary regular Direct-indirect ADC/SBC pointer wrap, DBR, pre/post-index, flag, width and cycle behavior
- [x] Binary long ADC/SBC pointer-bank, bank-zero wrap, 24-bit carry, flag, width and cycle behavior
- [x] Decimal ADC/SBC packed-BCD correction, C/V/N/Z, width, addressing and cycle behavior
- [x] Complete BIT selected-width Z, memory N/V and immediate N/V-preservation behavior
- [x] Complete ASL/LSR/ROL/ROR accumulator width, B preservation, C/N/Z and two-cycle behavior
- [x] Complete ASL/LSR/ROL/ROR memory Direct/Direct-X/absolute/absolute-X address, width, flag and cycle behavior
- [x] Complete memory INC/DEC wrap, N/Z isolation, address, width and cycle behavior
- [x] Complete TSB/TRB memory update, original-operand Z, address, width and cycle behavior
- [x] Complete STZ Direct/Direct-X/absolute/absolute-X width, address, flag-preservation and cycle behavior
- [x] Complete stack push/pull/effective-address instruction widths, byte order, flags and aggregate cycles
- [x] Ordinary emulation stack-page wrapping and documented linear multi-byte stack exceptions
- [x] Jump/subroutine target, return-address, PC/PBR and indirect-table bank behavior
- [x] JSL and JSR absolute-indexed-indirect meaningful bus callback order/count

## Bus

- [x] WRAM storage ownership and mapping — caller supplies verified 128 KiB storage
- [x] Logical 24-bit access integration
- [x] Low-bank WRAM mirrors
- [ ] ROM fast path
- [ ] [IN PROGRESS] SRAM windows — base LoROM/HiROM power-of-two mappings exist; broader boards absent
- [x] PPU MMIO routing — callback route only; PPU registers remain unimplemented
- [x] CPU I/O routing — callback route plus bus-owned `$420D`; CPU-I/O registers remain incomplete
- [x] APU port routing — callback route only; APU ports remain unimplemented
- [x] DMA register routing — callback route only; DMA registers remain unimplemented
- [ ] Enhancement windows
- [ ] [IN PROGRESS] Open bus behavior — deterministic single-latch fallback only; hardware domains unverified
- [ ] [IN PROGRESS] Read side effects — `$2180` auto-increment exists; remaining SNES register effects absent
- [ ] [IN PROGRESS] Write side effects — WRAM ports and `$420D` exist; remaining SNES register effects absent
- [ ] [IN PROGRESS] Access timing — meaningful accesses use 5A22 6/8/12 clocks and atomic CPU steps advance the scheduler; dummy/internal placement, refresh and arbitration absent
- [ ] [IN PROGRESS] Mapping boundaries and mirror tests — WRAM/MMIO, base LoROM/HiROM and Mode 25/35 ExHiROM boundaries verified; padded/ExLoROM/enhancement maps absent
- [ ] Page/block mapping benchmark

### Verified logical-bus foundation subset

- [x] Fixed-capacity caller-owned region table
- [x] Direct read-write and read-only regions
- [x] Separate MMIO callback slow path
- [x] Range, overlap and capacity validation
- [x] Deterministic unmapped fixture behavior
- [x] Partial SNES-map WRAM, mirror, port, MMIO-route and FastROM fixtures
- [x] Atomic CPU/scheduler timing integration fixtures
- [x] Cartridge no-response preserves deterministic bus latch

## Cartridge

- [ ] [IN PROGRESS] ROM loading — platform-neutral analysis/construction contract verified; platform file adapters absent
- [ ] [IN PROGRESS] ROM metadata — standard fields, registration prefix and conservative mapper evidence parsed; special variants/hardware resolution absent
- [ ] [IN PROGRESS] Header analysis — conservative LoROM/HiROM/ExHiROM selection verified; special variants absent
- [ ] [IN PROGRESS] LoROM — power-of-two and unambiguous two-region base ROM mapping, base SRAM and conservative detection verified; broader boards absent
- [ ] [IN PROGRESS] HiROM — power-of-two and unambiguous two-region base ROM mapping, base SRAM and conservative detection verified; broader boards absent
- [ ] ExLoROM — unofficial mapping remains research-only; no commercial-library requirement established
- [ ] [IN PROGRESS] ExHiROM — Mode 25/35 ROM/SRAM mapping and conservative selection verified; board/runtime validation absent
- [ ] [IN PROGRESS] SRAM — caller-owned mapper windows/read/write and revisioned dirty tracking verified; broader boards absent
- [ ] [IN PROGRESS] SRAM persistence — shared handoff exists; platform file backends and target validation absent
- [ ] [IN PROGRESS] Mapper tests — synthetic base/irregular/ExHiROM maps, checksum, selection, ambiguity and bus/CPU integration covered; padded/ExLoROM/enhancement execution absent
- [ ] [IN PROGRESS] Checksum handling where useful — mirrored base-image checksum verified; special-board checksum variants absent
- [ ] [IN PROGRESS] Truncated and malformed input rejection — mapper config, image granularity and candidate bounds covered; file I/O absent
- [x] Copier-header policy — explicit detect/reject/require for a single 512-byte prefix; other layouts rejected
- [ ] Region detection

### Verified cartridge foundation subset

- [x] Caller-owned read-only ROM and writable SRAM views
- [x] Cartridge slot mapped/unmapped response contract
- [x] Explicit base LoROM 32 KiB-bank wiring for supported sizes
- [x] Explicit base HiROM 64 KiB-bank wiring for supported sizes
- [x] Standard base LoROM/HiROM SRAM windows for supported sizes
- [x] ROM write protection and deterministic mirror fixtures
- [x] Cartridge/SNES-bus/CPU/scheduler integration fixture
- [x] Heap-free clean/copier-prefixed image view with source-offset retention
- [x] Explicit LoROM/HiROM internal-header candidate parsing
- [x] Raw title/map/chip/size/destination/developer/version/checksum/vector extraction
- [x] Overflow-safe declared ROM/SRAM size decoding
- [x] Checksum/complement relationship and base map-mode classification
- [x] Malformed image-size and out-of-range candidate rejection
- [x] Whole-payload checksum with candidate fields normalized and copier prefix excluded
- [x] Unambiguous two-region power-of-two checksum mirroring
- [x] Conservative base LoROM/HiROM evidence gates
- [x] Explicit no-confident-header and ambiguous-header results
- [x] Selection independent of title text and guessed reset opcode
- [x] Larger-first/smaller-tail base ROM mirroring for unambiguous layouts
- [x] Mapper-bank alignment and padding-dependent layout rejection
- [x] Header-selection to irregular-mapper initialization fixture
- [x] Raw 16-byte registration-prefix parsing without ASCII/title scoring
- [x] Mode 25/35 ExHiROM header classification at `$40FFC0`
- [x] ExHiROM first/extended region and upper-half mirror mapping
- [x] ExHiROM 6 MiB tail repetition and standard HiROM-style SRAM windows
- [x] Generic three-candidate selection with explicit ambiguity/no-match
- [x] Mapper executable-size/alignment confidence gate
- [x] Platform-neutral image-to-cartridge plan and caller-owned SRAM construction
- [x] Revisioned SRAM dirty-state and exact-acknowledgement handoff

Checksum/selection scope and local verification are recorded in
[cartridge-mapper-selection-audit.md](docs/cartridge-mapper-selection-audit.md).
Executable irregular-map scope and verification are recorded in
[cartridge-irregular-mapping-audit.md](docs/cartridge-irregular-mapping-audit.md).
Special checksum conventions, padded and board-specific irregular layouts, unofficial
ExLoROM and enhancement hardware remain unchecked.
Mode 25/35 ExHiROM scope and verification are recorded in
[cartridge-exhirom-audit.md](docs/cartridge-exhirom-audit.md). Unofficial ExLoROM,
special boards and enhancement hardware remain unchecked.
Preparation/construction scope and verification are recorded in
[cartridge-construction-audit.md](docs/cartridge-construction-audit.md). Platform file
I/O and allocation policy remain unchecked. Shared SRAM handoff scope is recorded in
[cartridge-sram-persistence-audit.md](docs/cartridge-sram-persistence-audit.md);
platform persistence remains incomplete.

## Scheduler

- [x] Master timestamp units — unsigned 64-bit SNES master clocks
- [x] Clock ratio representation — exact reduced rational with carried remainder
- [x] Event tie ordering — timestamp, explicit priority, insertion token
- [ ] [IN PROGRESS] CPU event horizon — atomic timed CPU steps exist; mid-instruction bus-cycle pausing pending
- [x] Shared scheduler/H/V master-clock fanout — checked alignment, transactional H/V slices and provisional same-time ordering verified
- [ ] PPU event integration
- [ ] SPC700 synchronization
- [ ] DSP synchronization
- [ ] DMA bus ownership
- [ ] Enhancement scheduling
- [x] Determinism tests — identical scheduler replay traces verified
- [ ] [IN PROGRESS] Long-run clock drift tests — chunked ratio, H/V and short common-domain advancement verified; regional soak tests pending

## Interrupts and timers

- [ ] [IN PROGRESS] CPU I/O registers — `$4200`, `$4207-$420A` and `$4210-$4212` subset implemented
- [ ] [IN PROGRESS] H/V counter model — non-interlaced NTSC/PAL master-clock phase, blanking and NTSC short line verified; interlace/dot latches absent
- [ ] [IN PROGRESS] IRQ compare behavior — stable-register non-interlaced H/V matches exist; exact write/match windows, interlace and scheduler/CPU delivery incomplete
- [ ] [IN PROGRESS] NMI edge and enable behavior — gated edge/latch and non-interlaced VBlank/NMI timing exist; central scheduler/CPU delivery incomplete
- [ ] [IN PROGRESS] Interrupt acknowledgement — base RDNMI/TIMEUP reads exist; match-window/external IRQ cases absent
- [ ] Pending interrupt ordering
- [ ] CPU math register timing
- [ ] Automatic joypad polling timing
- [ ] SPC700 timer coordination
- [ ] [IN PROGRESS] Interrupt and timer integration tests — register/latch, SNES-bus routing, H/V compare and atomic common-domain fixtures exist; CPU input delivery pending

### Verified CPU-I/O foundation subset

- [x] NMITIMEN enable/mode mask and disabled-IRQ acknowledgement
- [x] 9-bit HTIME/VTIME register storage
- [x] RDNMI/TIMEUP/HVBJOY read composition and side effects
- [x] NMI gated-edge and timer-IRQ latch handoff
- [x] Partial SNES-bus CPU-I/O state routing

### Verified H/V foundation subset

- [x] Caller-owned non-interlaced NTSC/PAL master-clock state
- [x] HBlank and 224/239-line VBlank transition timing
- [x] Separate VBlank and delayed RDNMI/NMI-flag signals
- [x] NTSC field alternation and 1360-clock short scanline
- [x] PAL 312-line non-interlaced frame geometry
- [x] One-shot/chunked H/V advancement determinism

### H/V timer-IRQ foundation subset

- [x] H-IRQ match formula for every HTIME 0-339
- [x] HTIME 338/339 following-line spill
- [x] V-IRQ and HV-IRQ stable-register positions
- [x] Regional compare-range and H=153 exception behavior
- [x] TIMEUP latch/acknowledgement handoff from H/V timing

### Shared master-clock domain subset

- [x] Checked scheduler/H/V timestamp alignment and coordinated advancement
- [x] Deterministic H/V-before-general-callback ordering at equal timestamps
- [x] Event-budget exhaustion preserves aligned domain state
- [x] Atomic CPU-step delta advances scheduler and H/V together
- [ ] Automatic CPU-I/O IRQ/NMI delivery into 65C816 inputs

## DMA / HDMA

- [ ] DMA registers
- [ ] DMA transfer modes
- [ ] DMA timing
- [ ] DMA source/destination wrapping
- [ ] HDMA initialization
- [ ] HDMA scanline behavior
- [ ] Indirect HDMA
- [ ] Transfer edge cases
- [ ] CPU interaction
- [ ] Channel priority
- [ ] DMA and HDMA overlap
- [ ] Tests

## PPU

- [ ] PPU register model
- [ ] VRAM
- [ ] CGRAM
- [ ] OAM
- [ ] BG Mode 0
- [ ] BG Mode 1
- [ ] BG Mode 2
- [ ] BG Mode 3
- [ ] BG Mode 4
- [ ] BG Mode 5
- [ ] BG Mode 6
- [ ] Mode 7
- [ ] Sprites
- [ ] Sprite priorities
- [ ] BG priorities
- [ ] Window system
- [ ] Color math
- [ ] Brightness
- [ ] Mosaic
- [ ] Scrolling
- [ ] Offset-per-tile
- [ ] Hires
- [ ] Interlace
- [ ] Overscan
- [ ] Relevant latching behavior
- [ ] Relevant open-bus behavior
- [ ] Scanline integration
- [ ] VRAM port increment/remap behavior
- [ ] Sprite limits and overflow flags
- [ ] Mid-frame register effects
- [ ] PPU tests
- [ ] Reference renderer integration

## SPC700

- [ ] Register state
- [ ] Instruction fetch
- [ ] Opcode dispatch
- [ ] Addressing modes
- [ ] Load/store
- [ ] Arithmetic and flags
- [ ] Decimal adjust
- [ ] Logic and shifts
- [ ] Branches and calls
- [ ] Stack behavior
- [ ] Bit operations
- [ ] Multiply/divide
- [ ] Sleep and stop
- [ ] Instruction timing
- [ ] RAM and IPL mapping
- [ ] IPL provenance decision
- [ ] I/O registers
- [ ] Timers
- [ ] Communication ports
- [ ] Instruction tests
- [ ] Timing regressions

## DSP

- [ ] Register state
- [ ] Eight voice state
- [ ] BRR decode
- [ ] BRR loop/end behavior
- [ ] Pitch progression
- [ ] Interpolation
- [ ] ADSR
- [ ] GAIN envelopes
- [ ] Key-on/key-off
- [ ] Noise generator
- [ ] Pitch modulation
- [ ] Voice volumes
- [ ] Main volumes
- [ ] Echo buffer
- [ ] FIR filter
- [ ] Feedback
- [ ] Clamping and signed arithmetic
- [ ] Sample timing
- [ ] DSP register side effects
- [ ] Golden sample tests

## APU and audio

- [ ] SPC700/DSP interconnect
- [ ] CPU/APU port synchronization
- [ ] Timer integration
- [ ] Sample generation pipeline
- [ ] Deterministic sample buffering
- [ ] Output sample format contract
- [ ] Platform-independent PCM output interface
- [ ] Resampling policy
- [ ] Underflow/overflow behavior
- [ ] Host offline audio tests
- [ ] Integrated APU regression tests

## PS2 backend

- [ ] SDK entry lifecycle runtime validation
- [ ] GS framebuffer presentation
- [ ] Scaling and aspect policy
- [ ] Video buffers and ownership
- [ ] GIF packet lifetime
- [ ] DMA cache coherency
- [ ] Pad input
- [ ] Storage and ROM selection
- [ ] SRAM persistence
- [ ] SPU2 output
- [ ] Audio buffering
- [ ] Pin a reviewed RFAuds2 version/revision
- [ ] PS2-only RFAuds2 adapter behind platform audio interface
- [ ] RFAuds2 asynchronous accepted-tail/backpressure handling
- [ ] RFAuds2 FAT/Slim real-hardware and long-duration validation
- [ ] IOP service initialization
- [ ] Error reporting
- [ ] Memory budget enforcement
- [ ] Real PS2 boot test
- [ ] Real PS2 integrated runtime test

## PSP backend

- [ ] SDK entry lifecycle runtime validation
- [ ] GU framebuffer presentation
- [ ] Scaling and aspect policy
- [ ] Video buffers and ownership
- [ ] Cache coherency
- [ ] Controller input
- [ ] Storage and ROM selection
- [ ] SRAM persistence
- [ ] PSP audio output
- [ ] Audio buffering
- [ ] Exit and suspend lifecycle
- [ ] Clock policy
- [ ] Memory budget enforcement
- [ ] Real PSP boot test
- [ ] Real PSP integrated runtime test

## Host testing

- [x] CPU foundation unit harness
- [x] Reference bus unit tests
- [ ] Mapper fixtures
- [ ] PPU logic fixtures
- [ ] SPC700 fixtures
- [ ] DSP sample fixtures
- [x] Scheduler fixtures — event order/error paths, rational clocks, 5A22 classes, shared H/V fanout, timed CPU steps and IRQ/NMI boundary
- [ ] DMA/HDMA fixtures
- [ ] Licensed test-ROM runner
- [ ] Deterministic replay harness
- [ ] Sanitizer regression coverage
- [ ] Malformed ROM fuzzing
- [ ] Serialization tests
- [ ] Reference/optimized differential harness

## Debugging

- [ ] CPU trace
- [ ] Register dump
- [ ] Memory trace
- [ ] Breakpoints
- [ ] Watchpoints
- [ ] PPU state dump
- [ ] Scanline log
- [ ] APU trace
- [ ] Scheduler events
- [ ] DMA trace
- [ ] Deterministic replay
- [ ] Trace filtering
- [ ] Release trace overhead verification

## Save states

- [ ] State stability review
- [ ] CPU serialization
- [ ] Bus serialization
- [ ] PPU serialization
- [ ] APU serialization
- [ ] Cartridge serialization
- [ ] Enhancement-chip serialization
- [ ] Scheduler serialization
- [ ] Versioned format
- [ ] Compatibility policy
- [ ] Malformed state rejection
- [ ] Regression tests

## PS2 optimization

- [ ] Real PS2 baseline profiling
- [ ] Identify hotspots
- [ ] EE optimization
- [ ] Memory layout review
- [ ] Cache review
- [ ] Scratchpad candidates
- [ ] MMI candidates
- [ ] DMA opportunities
- [ ] GS presentation optimization
- [ ] VU feasibility review
- [ ] VIF transfer cost review
- [ ] IOP offload feasibility
- [ ] Audio optimization
- [ ] Regression tests
- [ ] Real hardware benchmarks

## PSP optimization

- [ ] Real PSP baseline profiling
- [ ] Allegrex hotspot review
- [ ] Cache review
- [ ] Memory layout review
- [ ] MIPS optimization
- [ ] VFPU candidates
- [ ] GU optimization
- [ ] Audio optimization
- [ ] Buffering latency review
- [ ] Regression tests
- [ ] Real hardware benchmarks

## Compatibility campaign

- [ ] Legal test corpus inventory
- [ ] Region coverage matrix
- [ ] Base SNES integration suite
- [ ] Controller and input validation
- [ ] Commercial-game reproduction protocol
- [ ] PS2 compatibility reports
- [ ] PSP compatibility reports
- [ ] Hardware bug regression workflow validation
- [ ] Long-play stability
- [ ] Enhancement-chip game validation
- [ ] No-hack audit

## Performance validation

- [ ] PS2 timer calibration
- [ ] PSP timer calibration
- [ ] Repeatable input workloads
- [ ] Frame-time distribution reporting
- [ ] Memory high-water reporting
- [ ] Audio underrun measurement
- [ ] Real PS2 sustained full-speed validation
- [ ] Real PSP sustained full-speed validation
- [ ] Separate emulator and hardware records

## Possible dynarec

- [ ] Measure interpreter cost on real PS2
- [ ] Measure interpreter cost on real PSP
- [ ] Determine if dynarec is justified
- [ ] Research invalidation requirements
- [ ] Design IR if justified
- [ ] Design code cache
- [ ] PS2 MIPS emitter
- [ ] PSP MIPS emitter
- [ ] Correctness tests
- [ ] Interpreter fallback
- [ ] Performance validation

## Super FX

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Star Fox validation
- [ ] Yoshi's Island validation

## SA-1

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] 65C816 integration
- [ ] Internal RAM and bitmap modes
- [ ] DMA and arithmetic units
- [ ] Super Mario RPG validation

## DSP-1

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Command protocol
- [ ] Fixed-point arithmetic
- [ ] Reference vectors

## DSP-2

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Command protocol
- [ ] Fixed-point arithmetic
- [ ] Reference vectors

## DSP-3

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Command protocol
- [ ] Fixed-point arithmetic
- [ ] Reference vectors

## DSP-4

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Command protocol
- [ ] Fixed-point arithmetic
- [ ] Reference vectors

## CX4

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed

## S-DD1

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Decompression vectors
- [ ] Streaming boundaries

## SPC7110

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Decompression vectors
- [ ] Streaming boundaries
- [ ] RTC behavior and persistence

## OBC-1

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed

## Other commercially relevant hardware

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
