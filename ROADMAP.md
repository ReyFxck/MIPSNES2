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
- [x] PS2 embedded core diagnostic host baseline and cross-build
- [x] PS2 v1 diagnostic screen captured with five PASS results on ARMSX2 and NetherSX2 Turnip
- [ ] [IN PROGRESS] Record ARMSX2 version/settings and on-device ELF identity for reproducibility
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
- [x] Opcode dispatch — all 256 byte values
- [x] Immediate addressing
- [x] Absolute addressing
- [x] Long addressing — data, ALU and JMP/JSL long forms verified
- [x] Direct Page addressing
- [x] Stack-relative addressing — complete LDA/STA/ORA/AND/EOR/CMP/ADC/SBC families
- [x] Indexed addressing
- [x] Indirect addressing — data/ALU, PEI and all indirect JMP/JSR forms verified
- [x] Branch instructions
- [x] Jump/subroutine call/return instructions
- [x] Load/store instructions — scalar families and MVP/MVN
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
- [x] IRQ CPU input and entry
- [x] NMI CPU input and entry
- [ ] [IN PROGRESS] ABORT input and entry — boundary-visible and explicit per-write qualification subset verified; electrical gate sampling and RTI/interrupt exceptions pending
- [ ] [IN PROGRESS] RESET
- [x] BRK
- [x] COP
- [x] WAI
- [x] STP
- [x] Bank wrapping
- [x] Direct Page edge cases
- [ ] [IN PROGRESS] Timing model — resumable 5A22 operation timing is integrated; exact dummy/electrical sequencing pending
- [x] CPU automated tests for all 256 opcode values
- [ ] [IN PROGRESS] CPU regression tests — broader independent vectors/test ROMs pending
- [ ] [IN PROGRESS] Complete interpreter integration — all 256 opcode values, boundary IRQ/NMI/ABORT entry, RESET vector reads and scheduler/5A22 cursor wiring are resumable; signal-level RESET, exact dummy cycles and full mid-instruction ABORT remain pending

## Phase 3 — Bus and Cartridge

- [ ] Phase 3 exit gate verified

- [x] WRAM storage ownership and mapping — caller-owned 128 KiB
- [x] Logical 24-bit access integration
- [x] Low-bank WRAM mirrors
- [ ] ROM fast path
- [ ] [IN PROGRESS] SRAM windows — supported LoROM/HiROM/ExHiROM mappings verified
- [x] PPU MMIO routing — optional counter-register state and callback fallback
- [x] CPU I/O routing — callback route plus bus-owned `$420D`
- [x] APU port routing — callback route only
- [x] DMA register routing — callback route only
- [ ] Enhancement windows
- [ ] [IN PROGRESS] Open bus behavior — deterministic latch only
- [ ] [IN PROGRESS] Read side effects — WRAM data-port increment only
- [ ] [IN PROGRESS] Write side effects — WRAM ports and `$420D` only
- [ ] [IN PROGRESS] Access timing — meaningful accesses integrated; exact cycle placement/refresh/arbitration pending
- [ ] [IN PROGRESS] Mapping boundaries and mirror tests — WRAM/MMIO plus base and Mode 25/35 ExHiROM maps verified; padded/ExLoROM/enhancement pending
- [ ] Page/block mapping benchmark
- [ ] [IN PROGRESS] ROM loading — core construction and explicit PS2 path adapter exist; mounted-device/PSP adapters and selection remain
- [ ] [IN PROGRESS] ROM metadata — standard and 16-byte registration fields parsed; special variants pending
- [ ] [IN PROGRESS] Header analysis — conservative LoROM/HiROM/ExHiROM selection verified; special variants pending
- [ ] [IN PROGRESS] LoROM — power-of-two and unambiguous two-region base mapping plus conservative detection verified; broader boards pending
- [ ] [IN PROGRESS] HiROM — power-of-two and unambiguous two-region base mapping plus conservative detection verified; broader boards pending
- [ ] ExLoROM — unofficial mapping remains research-only
- [ ] [IN PROGRESS] ExHiROM — Mode 25/35 ROM/SRAM mapping and selection verified; board/runtime validation pending
- [ ] [IN PROGRESS] SRAM — mapper windows/read/write and revisioned dirty tracking verified; broader boards pending
- [ ] [IN PROGRESS] SRAM persistence — shared revision/acknowledgement handoff verified; platform file backends absent
- [ ] [IN PROGRESS] Mapper tests — synthetic base/irregular/ExHiROM map, checksum, selection and ambiguity suite exists; broader boards pending
- [ ] [IN PROGRESS] Checksum handling where useful — mirrored base-image checksum verified; special-board variants pending
- [ ] [IN PROGRESS] Truncated and malformed input rejection — mapper config and image/header bounds covered; file I/O absent
- [x] Copier-header policy — explicit detect/reject/require for a single 512-byte prefix; other layouts rejected
- [ ] Region detection
- [x] Platform-neutral image-to-cartridge construction contract
- [x] SRAM dirty/revision persistence handoff

## Phase 4 — Scheduler / IRQ / NMI / DMA / HDMA

- [ ] Phase 4 exit gate verified

- [x] Master timestamp units
- [x] Clock ratio representation
- [x] Event tie ordering
- [x] CPU event horizon — caller-owned in-flight operation state advances through requested/event horizons and resumes event-budget failures without duplicate CPU/bus effects
- [x] Shared scheduler/H/V master-clock fanout — checked alignment and provisional same-time ordering verified
- [x] Atomic CPU-I/O interrupt-line handoff — independent IRQ sources and NMI transfer verified
- [ ] [IN PROGRESS] Resumable CPU cycle/micro-operation contract — exact-once foundation plus scheduler wiring for all 256 opcode values, boundary IRQ/NMI/ABORT entry, write-qualified ABORT subset and explicit RESET; signal-level RESET, dummy cycles and electrical ABORT exceptions pending
- [ ] PPU event integration
- [ ] SPC700 synchronization
- [ ] DSP synchronization
- [ ] DMA bus ownership
- [ ] Enhancement scheduling
- [x] Determinism tests — scheduler foundation replay
- [ ] [IN PROGRESS] Long-run clock drift tests — exact ratio, H/V chunking and short common-domain runs verified; regional soak tests pending
- [ ] [IN PROGRESS] CPU I/O registers — interrupt/timer, math, programmable-I/O and controller subsets implemented
- [ ] [IN PROGRESS] H/V counter model — non-interlaced phase and counter-register subset verified; interlace and exact capture timing pending
- [ ] [IN PROGRESS] IRQ compare behavior — stable-register non-interlaced H/V matches and operation-boundary CPU delivery exist; exact write/match windows, interlace and electrical sampling pending
- [ ] [IN PROGRESS] NMI edge and enable behavior — non-interlaced VBlank/NMI timing and operation-boundary delivery exist; exact electrical sampling pending
- [ ] [IN PROGRESS] Interrupt acknowledgement — RDNMI/TIMEUP base side effects exist; match-window/external IRQ cases absent
- [ ] [IN PROGRESS] Interrupt ordering — CPU NMI-over-IRQ priority and independent IRQ-source aggregation verified; subsystem ties pending
- [ ] [IN PROGRESS] CPU math register timing — 8/16 processor-cycle completion and result semantics verified; busy collisions and exact internal-cycle placement pending
- [ ] [IN PROGRESS] Automatic joypad polling timing — deterministic 4224-clock foundation exists; variable hardware phase, partial busy reads and manual-port contention pending
- [x] Standard manual joypad serial foundation — `$4016/$4017` strobe, four lines, independent port clocks and post-word padding verified
- [x] Programmable I/O-port foundation — `$4201/$4213`, wired-AND external levels and bit-7 falling-edge handoff verified
- [x] Non-interlaced H/V counter-register foundation — dot projection, read selectors and deterministic WRIO bus adapter verified
- [ ] Exact counter capture latency and held-low/repeated-latch hardware validation
- [ ] SPC700 timer coordination
- [ ] [IN PROGRESS] Interrupt and timer integration tests — register/latch, bus/H/V timing, atomic compatibility and resumable operation-boundary delivery tests exist; exact electrical cases pending
- [x] 5A22 CPU-I/O register/latch foundation
- [x] Non-interlaced NTSC/PAL H/V timing foundation
- [x] Stable-register non-interlaced H/V timer-IRQ foundation
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
- [ ] [IN PROGRESS] VRAM ports — read/write buffering and remap exist; access gates/mixed-port behavior pending
- [ ] [IN PROGRESS] CGRAM ports — address, paired writes and reads; active-display and mixed-port behavior pending
- [ ] [IN PROGRESS] OAM ports — CPU read/write, scheduled VBlank reload, first-line forced-blank release, `$2101` storage and sprite descriptor decode; display access windows pending
- [ ] [IN PROGRESS] Tile decode fixtures — 4bpp planar OBJ-compatible tiles verified; 2bpp/8bpp and scanline output absent
- [ ] [IN PROGRESS] Reference scanline buffers — pure OBJ range/time selection exists; pixel buffer absent
- [ ] Basic Mode 0/1 output
- [ ] Scheduler scanline integration

## Phase 6 — PPU Compatibility

- [ ] Phase 6 exit gate verified

- [ ] PPU register model
- [ ] [IN PROGRESS] VRAM — storage and read/write-port subset; access gates and rendering absent
- [ ] [IN PROGRESS] CGRAM — storage and CPU port subset; access gates and rendering absent
- [ ] [IN PROGRESS] OAM — storage, CPU ports, VBlank reload, descriptor decode and tile-address lookup; sprite evaluation and rendering absent
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
- [ ] [IN PROGRESS] VRAM port increment/remap behavior — read/write sides host-verified; active-display and mixed-port behavior pending
- [ ] [IN PROGRESS] Sprite limits and overflow flags — pure 32 OBJ/34 sliver selection verified; `$213E` timing/latching absent
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

- [x] Machine ownership and lifecycle — stable caller-owned image/WRAM/SRAM and platform-neutral instance verified on host
- [ ] [IN PROGRESS] Cartridge reset path — timed CPU reset and power cycle verified; complete hardware reset behavior remains open
- [ ] [IN PROGRESS] CPU/bus/scheduler integration — ROM instruction/WRAM/H-V execution verified; remaining devices absent
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
