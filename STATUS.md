# STATUS — canonical project state

Project foundation plus a partial 82-opcode 65C816/reference-bus milestone. No ROM execution,
complete emulated subsystem, console runtime validation, compatibility result or
hardware benchmark exists. Read [bootstrap audit](docs/bootstrap-audit.md) and the
CPU/bus evidence in [docs/65c816-foundation-audit.md](docs/65c816-foundation-audit.md).

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
are in docs/65c816-increment-decrement-audit.md; GitHub CI remains required for the
published commit.

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
- [ ] [IN PROGRESS] Opcode dispatch — 82-opcode subset only
- [ ] [IN PROGRESS] Immediate addressing — LDA/LDX/LDY plus REP/SEP implemented only
- [ ] [IN PROGRESS] Absolute addressing — JMP plus LDA/LDX/LDY and STA/STX/STY implemented
- [ ] [IN PROGRESS] Long addressing — LDA/STA absolute long and absolute long,X implemented only
- [ ] [IN PROGRESS] Direct Page addressing — direct/indexed loads/stores plus ten LDA/STA indirect forms only
- [ ] Stack-relative addressing
- [ ] [IN PROGRESS] Indexed addressing — direct, absolute, long and Direct-indirect load/store subset only
- [ ] [IN PROGRESS] Indirect addressing — LDA/STA `(direct,X)`, `(direct)`, `(direct),Y`, `[direct]` and `[direct],Y` only
- [x] Branch instructions
- [ ] [IN PROGRESS] Load/store instructions — immediate, direct/indirect, absolute, indexed and long subset only
- [ ] [IN PROGRESS] Arithmetic instructions — accumulator/index register increment-decrement subset only
- [ ] Logic instructions
- [ ] Shift/rotate instructions
- [x] Transfer instructions
- [ ] Stack instructions
- [ ] [IN PROGRESS] Status flag behavior — implied C/D/I/V controls and implemented load/transfer/register-arithmetic N/Z plus REP/SEP/XCE subset verified
- [ ] [IN PROGRESS] M flag behavior — implemented accumulator loads/stores, transfers and register INC/DEC verified at both widths
- [ ] [IN PROGRESS] X flag behavior — implemented index loads/stores, transfers, register INC/DEC, indexed cycles and high-byte clearing verified
- [ ] [IN PROGRESS] Native mode — XCE entry, implemented widths/transfers, branches, status controls and register INC/DEC verified
- [ ] [IN PROGRESS] Emulation mode — reset/REP/XCE invariants, widths, transfers, Direct/indirect pointer wrapping, branches, status controls and register INC/DEC verified for implemented subset
- [ ] Decimal arithmetic
- [ ] IRQ
- [ ] NMI
- [ ] [IN PROGRESS] RESET — vector and documented visible state subset only
- [ ] BRK
- [ ] COP
- [ ] WAI
- [ ] STP
- [ ] [IN PROGRESS] Bank wrapping — program fetch/relative branches are bank-local; data/indexing is linear; Direct pointers remain in bank zero
- [ ] [IN PROGRESS] Direct Page edge cases — E/DL wrap, long-pointer exception, unaligned-D carry and bank-zero wrap verified for implemented subset
- [ ] [IN PROGRESS] Timing model — width, Direct DL, indexed read/write, complete branch/transfer, status-control and register INC/DEC timing verified for implemented subset only
- [ ] [IN PROGRESS] CPU automated tests — all 82 implemented opcodes and addressing behavior covered; complete CPU pending
- [ ] CPU regression tests
- [ ] Complete interpreter integration

### Verified 65C816 foundation subset

- [x] Caller-owned register/mode/bus/diagnostic state
- [x] Reset vector fetch from logical 00:FFFC-D
- [x] Pre-reset and unimplemented-opcode diagnostic stops
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
- [x] STY direct (`84`)
- [x] STA direct (`85`)
- [x] STX direct (`86`)
- [x] STA direct-indirect-long (`87`)
- [x] DEY (`88`)
- [x] TXA (`8A`)
- [x] BCC (`90`)
- [x] STA direct-indirect-indexed,Y (`91`)
- [x] STA direct-indirect (`92`)
- [x] STY direct,X (`94`)
- [x] STA direct,X (`95`)
- [x] STX direct,Y (`96`)
- [x] STA direct-indirect-long-indexed,Y (`97`)
- [x] TYA (`98`)
- [x] TXS (`9A`)
- [x] TXY (`9B`)
- [x] LDA direct-indexed-indirect (`A1`)
- [x] LDY direct (`A4`)
- [x] LDA direct (`A5`)
- [x] LDX direct (`A6`)
- [x] LDA direct-indirect-long (`A7`)
- [x] TAY (`A8`)
- [x] TAX (`AA`)
- [x] BCS (`B0`)
- [x] LDA direct-indirect-indexed,Y (`B1`)
- [x] LDA direct-indirect (`B2`)
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

## Bus

- [ ] WRAM allocation and mapping
- [x] Logical 24-bit access integration
- [ ] Low-bank WRAM mirrors
- [ ] ROM fast path
- [ ] SRAM windows
- [ ] PPU MMIO routing
- [ ] CPU I/O routing
- [ ] APU port routing
- [ ] DMA register routing
- [ ] Enhancement windows
- [ ] [IN PROGRESS] Open bus behavior — deterministic single-latch fallback only; hardware domains unverified
- [ ] [IN PROGRESS] Read side effects — direct/MMIO dispatch exists; SNES register effects absent
- [ ] [IN PROGRESS] Write side effects — direct/MMIO dispatch exists; SNES register effects absent
- [ ] Access timing
- [ ] [IN PROGRESS] Mapping boundaries and mirror tests — reference-region boundaries tested; SNES mirrors absent
- [ ] Page/block mapping benchmark

### Verified logical-bus foundation subset

- [x] Fixed-capacity caller-owned region table
- [x] Direct read-write and read-only regions
- [x] Separate MMIO callback slow path
- [x] Range, overlap and capacity validation
- [x] Deterministic unmapped fixture behavior

## Cartridge

- [ ] ROM loading
- [ ] ROM metadata
- [ ] Header analysis
- [ ] LoROM
- [ ] HiROM
- [ ] ExLoROM
- [ ] ExHiROM
- [ ] SRAM
- [ ] SRAM persistence
- [ ] Mapper tests
- [ ] Checksum handling where useful
- [ ] Truncated and malformed input rejection
- [ ] Copier-header policy
- [ ] Region detection

## Scheduler

- [ ] Master timestamp units
- [ ] Clock ratio representation
- [ ] Event tie ordering
- [ ] CPU event horizon
- [ ] PPU event integration
- [ ] SPC700 synchronization
- [ ] DSP synchronization
- [ ] DMA bus ownership
- [ ] Enhancement scheduling
- [ ] Determinism tests
- [ ] Long-run clock drift tests

## Interrupts and timers

- [ ] CPU I/O registers
- [ ] H/V counter model
- [ ] IRQ compare behavior
- [ ] NMI edge and enable behavior
- [ ] Interrupt acknowledgement
- [ ] Pending interrupt ordering
- [ ] CPU math register timing
- [ ] Automatic joypad polling timing
- [ ] SPC700 timer coordination
- [ ] Interrupt and timer integration tests

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
- [ ] Scheduler fixtures
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
