# Roadmap

[STATUS.md](STATUS.md) is canonical task state. This file tracks phase exit gates;
unchecked phase gates are intentional until all required work has evidence. Do not
infer implementation from this planning document. GitHub milestones are recommended
with these names; none are claimed created. Platform build probes happen in Phase 0;
real backend implementation is integrated incrementally before Phase 9.

Each phase requires relevant tests, documentation and status updates. Hardware
behavior gates require correctness regressions; performance gates require the named
real target. Parent gates remain unchecked while required children remain incomplete.
Optional advanced work requires a measured go/no-go decision, not automatic adoption.

## Phase 0 — Foundation

- [ ] Phase 0 exit gate verified

- [x] Repository structure
- [x] Documentation
- [x] AGENTS.md
- [x] STATUS.md
- [x] Host build
- [x] Test framework
- [x] Logging
- [x] Core/platform boundary
- [x] PS2 build scaffolding
- [x] PSP build scaffolding
- [ ] GitHub infrastructure

## Phase 1 — 65C816 Foundation

- [x] Phase 1 exit gate verified

- [x] Register and flag ownership
- [x] Logical bus fixture
- [x] Reset vector sequence
- [x] Instruction fetch harness
- [x] Emulation-mode reset invariants
- [x] First instruction stepping tests

## Phase 2 — Complete 65C816 Interpreter

- [ ] Phase 2 exit gate verified

- [x] CPU state representation
- [ ] [IN PROGRESS] Reset sequence
- [x] Instruction fetch
- [ ] [IN PROGRESS] Opcode dispatch
- [ ] [IN PROGRESS] Immediate addressing
- [ ] [IN PROGRESS] Absolute addressing
- [ ] [IN PROGRESS] Long addressing
- [ ] [IN PROGRESS] Direct Page addressing
- [ ] Stack-relative addressing
- [ ] [IN PROGRESS] Indexed addressing
- [ ] [IN PROGRESS] Indirect addressing
- [x] Branch instructions
- [ ] [IN PROGRESS] Load/store instructions
- [ ] [IN PROGRESS] Arithmetic instructions — register INC/DEC, base CMP and complete CPX/CPY subsets only
- [ ] [IN PROGRESS] Logic instructions — base ORA/AND/EOR forms and complete BIT family only
- [ ] Shift/rotate instructions
- [x] Transfer instructions
- [ ] Stack instructions
- [ ] [IN PROGRESS] Status flag behavior
- [ ] [IN PROGRESS] M flag behavior
- [ ] [IN PROGRESS] X flag behavior
- [ ] [IN PROGRESS] Native mode
- [ ] [IN PROGRESS] Emulation mode
- [ ] Decimal arithmetic
- [ ] IRQ
- [ ] NMI
- [ ] [IN PROGRESS] RESET
- [ ] BRK
- [ ] COP
- [ ] WAI
- [ ] STP
- [ ] [IN PROGRESS] Bank wrapping
- [ ] [IN PROGRESS] Direct Page edge cases
- [ ] [IN PROGRESS] Timing model
- [ ] [IN PROGRESS] CPU automated tests
- [ ] CPU regression tests
- [ ] Complete interpreter integration

## Phase 3 — Bus and Cartridge

- [ ] Phase 3 exit gate verified

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

## Phase 4 — Scheduler / IRQ / NMI / DMA / HDMA

- [ ] Phase 4 exit gate verified

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

## Phase 5 — PPU Foundation

- [ ] Phase 5 exit gate verified

- [ ] PPU state ownership
- [ ] Register reads/writes
- [ ] VRAM ports
- [ ] CGRAM ports
- [ ] OAM ports
- [ ] Tile decode fixtures
- [ ] Reference scanline buffers
- [ ] Basic Mode 0/1 output
- [ ] Scheduler scanline integration

## Phase 6 — PPU Compatibility

- [ ] Phase 6 exit gate verified

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

## Phase 7 — SPC700

- [ ] Phase 7 exit gate verified

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

## Phase 8 — DSP / Audio

- [ ] Phase 8 exit gate verified

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

## Phase 9 — Base SNES Integration

- [ ] Phase 9 exit gate verified

- [ ] Machine ownership and lifecycle
- [ ] Cartridge reset path
- [ ] CPU/bus/scheduler integration
- [ ] PPU event integration
- [ ] APU sample integration
- [ ] Input integration
- [ ] SRAM persistence integration
- [ ] Licensed test-ROM execution
- [ ] Real PS2 runtime smoke
- [ ] Real PSP runtime smoke
- [ ] Base regression suite

## Phase 10 — PS2 Performance Pass

- [ ] Phase 10 exit gate verified

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

## Phase 11 — PSP Performance Pass

- [ ] Phase 11 exit gate verified

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

## Phase 12 — Enhancement Chips

- [ ] Phase 12 exit gate verified

### Super FX

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

### SA-1

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

### DSP-1

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

### DSP-2

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

### DSP-3

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

### DSP-4

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

### CX4

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

### S-DD1

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

### SPC7110

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

### OBC-1

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

### Other commercially relevant hardware

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

## Phase 13 — Compatibility Campaign

- [ ] Phase 13 exit gate verified

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

## Phase 14 — Advanced Optimization

- [ ] Phase 14 exit gate verified

- [ ] Profile evidence review
- [ ] Dynarec go/no-go decision
- [ ] Aggressive MIPS candidates
- [ ] Specialized rendering kernels
- [ ] Additional parallelism cost analysis
- [ ] Reference-path regressions
- [ ] Real PS2 comparisons
- [ ] Real PSP comparisons

## Deferred state format

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

## Dynarec evidence gates

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
