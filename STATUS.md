Warning: truncated output (original token count: 20407)
Total output lines: 1669

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
[docs/master-timing-fanout-audit.md](docs/master-timing-fanout-audit.md), and atomic
CPU-I/O interrupt handoff evidence is in
[docs/interrupt-handoff-audit.md](docs/interrupt-handoff-audit.md). Ricoh 5A22 math
and controller evidence is in
[docs/5a22-math-register-audit.md](docs/5a22-math-register-audit.md) and
[docs/auto-joypad-audit.md](docs/auto-joypad-audit.md), with manual serial-port
evidence in [docs/manual-joypad-audit.md](docs/manual-joypad-audit.md).
Programmable I/O-port evidence is in
[docs/5a22-io-port-audit.md](docs/5a22-io-port-audit.md).
The counter-register foundation is scoped in
[docs/hv-counter-latch-audit.md](docs/hv-counter-latch-audit.md).
The rejected in-callback access-clock experiment is documented in
[docs/cpu-access-timing-postmortem.md](docs/cpu-access-timing-postmortem.md).
Boundary-visible 65C816 ABORT evidence is recorded in
[docs/65c816-abort-foundation-audit.md](docs/65c816-abort-foundation-audit.md).
The initial caller-owned CPU operation cursor is scoped in
[docs/65c816-cycle-contract-audit.md](docs/65c816-cycle-contract-audit.md), and the
interrupt-entry extension is recorded in
[docs/65c816-interrupt-cycle-audit.md](docs/65c816-interrupt-cycle-audit.md).

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
- [x] Optional pinned XERJ first-party source/document search workflow verified

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
- [x] docs/xerj-workflow.md — host-only first-party indexing and provenance policy

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
`61/71/72` a…10407 tokens truncated…S] SRAM persistence — shared handoff exists; platform file backends and target validation absent
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

- [ ] [IN PROGRESS] CPU I/O registers — interrupt/timer, math, `$4201/$4213` programmable-I/O and controller subsets implemented
- [ ] [IN PROGRESS] H/V counter model — non-interlaced phase, long-dot projection and counter-register subset verified; interlace and exact capture timing absent
- [ ] [IN PROGRESS] IRQ compare behavior — stable-register non-interlaced H/V matches and atomic CPU delivery exist; exact write/match windows, interlace and within-instruction sampling incomplete
- [ ] [IN PROGRESS] NMI edge and enable behavior — gated edge/latch, non-interlaced VBlank timing and atomic CPU delivery exist; exact within-instruction sampling incomplete
- [ ] [IN PROGRESS] Interrupt acknowledgement — base RDNMI/TIMEUP reads exist; match-window/external IRQ cases absent
- [ ] [IN PROGRESS] Interrupt ordering — CPU NMI-over-IRQ priority and source aggregation verified; simultaneous subsystem timing pending
- [ ] [IN PROGRESS] CPU math register timing — 8/16-cycle completion and result semantics verified; busy collisions and exact internal-cycle placement pending
- [ ] [IN PROGRESS] Automatic joypad polling timing — 4224-clock window and result commit verified with a fixed deterministic start; hardware phase variation, busy-time partial data and manual-port contention pending
- [ ] SPC700 timer coordination
- [ ] [IN PROGRESS] Interrupt and timer integration tests — register/latch, bus/H/V timing and atomic CPU input delivery fixtures exist; exact cycle-boundary cases pending

### Verified CPU-I/O foundation subset

- [x] NMITIMEN enable/mode mask and disabled-IRQ acknowledgement
- [x] 9-bit HTIME/VTIME register storage
- [x] RDNMI/TIMEUP/HVBJOY read composition and side effects
- [x] NMI gated-edge and timer-IRQ latch handoff
- [x] Partial SNES-bus CPU-I/O state routing
- [x] Unsigned 8-bit multiply with 8-processor-cycle completion
- [x] Unsigned 16-by-8 divide with 16-processor-cycle completion
- [x] Divide-by-zero quotient/remainder behavior
- [x] Math-result read boundary and CPU-bus cycle integration
- [x] Four logical controller snapshots and `$4218-$421F` result registers
- [x] VBlank auto-joypad busy window and atomic completion
- [x] Standard manual joypad strobe and four serial data lines on `$4016/$4017`
- [x] Independent manual port clocks, 16-bit order and connected padding
- [x] WRIO/RDIO programmable I/O latch and wired-AND external levels
- [x] Programmable I/O bit-7 falling-edge handoff
- [ ] [IN PROGRESS] PPU H/V counter-latch consumption and register behavior — deterministic adapter exists; exact capture timing and held-low cases pending
- [ ] Auxiliary controller-output wiring and nonstandard peripheral protocols
- [ ] Busy-operation collision/retrigger hardware behavior

### Verified H/V foundation subset

- [x] Caller-owned non-interlaced NTSC/PAL master-clock state
- [x] HBlank and 224/239-line VBlank transition timing
- [x] Separate VBlank and delayed RDNMI/NMI-flag signals
- [x] NTSC field alternation and 1360-clock short scanline
- [x] PAL 312-line non-interlaced frame geometry
- [x] One-shot/chunked H/V advancement determinism
- [x] Normal-line long-dot and NTSC short-line PPU H projection
- [x] Counter low/high selectors and PPU2 open-bus subset
- [x] Counter field/region/version status and selector reset
- [x] Optional WRIO/external-edge to counter-register bus adapter

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
- [x] Atomic CPU-I/O IRQ/NMI delivery into 65C816 inputs
- [ ] [IN PROGRESS] Resumable CPU cycle/micro-operation contract — caller-owned exact-once foundation verified for seven opcodes and boundary IRQ/NMI/ABORT entry; remaining 249 opcodes, reset, dummy cycles, mid-instruction ABORT and scheduler integration pending

### Interrupt-line aggregation subset

- [x] Caller-owned independent IRQ source bitmask
- [x] CPU-I/O timer source update preserves cartridge/expansion IRQ sources
- [x] CPU-I/O NMI edge transfers once into the CPU input latch
- [x] Pre/post atomic-step input sampling contract
- [x] Simultaneous NMI/IRQ follows existing CPU NMI priority
- [ ] Exact within-instruction interrupt sampling

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

- [ ] [IN PROGRESS] PPU register model — H/V counter subset only
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
- [ ] [IN PROGRESS] Relevant latching behavior — deterministic H/V subset; exact access timing pending
- [ ] [IN PROGRESS] Relevant open-bus behavior — counter-register PPU2 subset only
- [ ] Scanline integration
- [ ] VRAM port increment/remap behavior
- [ ] Sprite limits and overflow flags
- [ ] Mid-frame register effects
- [ ] [IN PROGRESS] PPU tests — H/V counter fixtures only
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
- [x] Scheduler fixtures — event order/error paths, rational clocks, 5A22 classes/math and joypad timing, shared H/V fanout, IRQ-source aggregation and timed CPU boundaries
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
