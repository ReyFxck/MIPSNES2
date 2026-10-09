# STATUS — canonical project state

Foundation only. No emulated subsystem, ROM execution, console runtime validation,
compatibility result or hardware benchmark exists. Read [bootstrap audit](docs/bootstrap-audit.md).

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

## Current blockers and diagnostic exceptions

No game-specific diagnostics or intentional emulation approximations exist.
Real consoles/emulators are not available in this session. SDK/CI availability and
remaining admin work are recorded in docs/bootstrap-audit.md and docs/github-setup.md.

## 65C816

- [ ] CPU state representation
- [ ] Reset sequence
- [ ] Instruction fetch
- [ ] Opcode dispatch
- [ ] Immediate addressing
- [ ] Absolute addressing
- [ ] Long addressing
- [ ] Direct Page addressing
- [ ] Stack-relative addressing
- [ ] Indexed addressing
- [ ] Indirect addressing
- [ ] Branch instructions
- [ ] Load/store instructions
- [ ] Arithmetic instructions
- [ ] Logic instructions
- [ ] Shift/rotate instructions
- [ ] Transfer instructions
- [ ] Stack instructions
- [ ] Status flag behavior
- [ ] M flag behavior
- [ ] X flag behavior
- [ ] Native mode
- [ ] Emulation mode
- [ ] Decimal arithmetic
- [ ] IRQ
- [ ] NMI
- [ ] RESET
- [ ] BRK
- [ ] COP
- [ ] WAI
- [ ] STP
- [ ] Bank wrapping
- [ ] Direct Page edge cases
- [ ] Timing model
- [ ] CPU automated tests
- [ ] CPU regression tests
- [ ] Complete interpreter integration

## Bus

- [ ] WRAM allocation and mapping
- [ ] Logical 24-bit access integration
- [ ] Low-bank WRAM mirrors
- [ ] ROM fast path
- [ ] SRAM windows
- [ ] PPU MMIO routing
- [ ] CPU I/O routing
- [ ] APU port routing
- [ ] DMA register routing
- [ ] Enhancement windows
- [ ] Open bus behavior
- [ ] Read side effects
- [ ] Write side effects
- [ ] Access timing
- [ ] Mapping boundaries and mirror tests
- [ ] Page/block mapping benchmark

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

- [ ] CPU unit harness
- [ ] Bus unit tests
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
