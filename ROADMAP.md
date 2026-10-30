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
- [x] APU port routing — default machine-owned directional mailbox with explicit callback override
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
- [ ] [IN PROGRESS] PPU event integration — recurring HBlank frames and SETINI-dependent VBlank/OAM/HDMA cutoffs are host verified; exact late-line behavior remains
- [ ] [IN PROGRESS] SPC700 synchronization — nominal rational budget and atomic admission integrated; exact phases/races pending
- [ ] [IN PROGRESS] DSP synchronization — common cycle domain and sample-event cadence integrated; pipeline phases pending
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
- [x] SPC700 timer coordination — integrated APU clock advances timer stages
- [ ] [IN PROGRESS] Interrupt and timer integration tests — register/latch, bus/H/V timing, atomic compatibility and resumable operation-boundary delivery tests exist; exact electrical cases pending
- [x] 5A22 CPU-I/O register/latch foundation
- [x] Non-interlaced NTSC/PAL H/V timing foundation
- [x] Stable-register non-interlaced H/V timer-IRQ foundation
- [ ] [IN PROGRESS] DMA registers — channel banks and DMA/HDMA enable paths; electrical overlap pending
- [ ] [IN PROGRESS] DMA transfer modes — timed general DMA and HDMA machine paths; electrical edges pending
- [ ] [IN PROGRESS] DMA timing — general DMA slots/resynchronization, HDMA priority and shared refresh; electrical edges pending
- [x] DMA source/destination wrapping — fixed-bank A1T and `$21xx` BBAD wrapping verified
- [ ] [IN PROGRESS] HDMA initialization — V=0 request and VBlank deactivation; revision edges pending
- [ ] [IN PROGRESS] HDMA scanline behavior — visible-line request; exact subphases pending
- [ ] [IN PROGRESS] Indirect HDMA — timed pointers and last-channel zero-descriptor shortening; frame-initialization and electrical edges pending
- [ ] Transfer edge cases
- [ ] CPU interaction
- [x] Channel priority — lowest selected channel first verified for DMA and HDMA
- [ ] [IN PROGRESS] DMA and HDMA overlap — same-channel cancellation implemented; electrical phases pending
- [ ] Tests

## Phase 5 — PPU Foundation

- [ ] Phase 5 exit gate verified

- [ ] PPU state ownership
- [ ] [IN PROGRESS] Register reads/writes — shared machine-owned display, BG, window, color and SETINI write subsets exist; complete read/write/open-bus behavior remains
- [ ] [IN PROGRESS] VRAM ports — read/write buffering and remap exist; access gates/mixed-port behavior pending
- [ ] [IN PROGRESS] CGRAM ports — address, paired writes and reads; active-display and mixed-port behavior pending
- [ ] [IN PROGRESS] OAM ports — CPU read/write, scheduled VBlank reload, first-line forced-blank release, `$2101` storage, sprite descriptor decode and priority rotation; display access windows pending
- [ ] [IN PROGRESS] Tile decode fixtures — 4bpp OBJ and BG1 2bpp scanlines verified; 8bpp and final output absent
- [ ] [IN PROGRESS] Reference scanline buffers — scheduled OBJ, four Mode 0 or three Mode 1 BG lines, main/sub priority and layer/color windows, CGRAM lookup, fixed-color backdrop, color math and optional line handoff exist; PS2 has unverified single-frame GS submission but no continuous display
- [ ] Basic Mode 0/1 output
- [ ] Scheduler scanline integration

## Phase 6 — PPU Compatibility

- [ ] Phase 6 exit gate verified

- [ ] PPU register model
- [ ] [IN PROGRESS] VRAM — storage and read/write-port subset; access gates and rendering absent
- [ ] [IN PROGRESS] CGRAM — storage, CPU port subset and Mode 0 color lookup; access gates and video output absent
- [ ] [IN PROGRESS] OAM — storage, CPU ports, VBlank reload, descriptor decode, tile-address lookup and scheduled OBJ selection; exact evaluation phase and rendering absent
- [ ] [IN PROGRESS] BG Mode 0 — four register-driven 2bpp scanlines, main/sub layer and color windows, OBJ priority mixer and BGR555 color math exist; display and timing validation absent
- [ ] [IN PROGRESS] BG Mode 1 — reference 4bpp BG1/BG2, 2bpp BG3, priority and shared color path; display absent
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
- [ ] [IN PROGRESS] Overscan — `$2133` bit 2 drives VBlank, OAM reload and HDMA deactivation before V=225; renderer sizing and exact late-line effects remain
- [ ] [IN PROGRESS] Relevant latching behavior — late SETINI overscan writes latch for the next frame; other register latches and electrical timing remain
- [ ] Relevant open-bus behavior
- [x] Scanline integration — recurring machine events deliver two ordered reference frames with a chunk-independent pixel hash
- [ ] [IN PROGRESS] VRAM port increment/remap behavior — read/write sides host-verified; active-display and mixed-port behavior pending
- [ ] [IN PROGRESS] Sprite limits and overflow flags — scheduled 32 OBJ/34 sliver selection and sticky `$213E` verified on host; hardware phase and pixels absent
- [ ] [IN PROGRESS] Mid-frame register effects — bounded SETINI frame cutoff plus existing scanline-driven BG/window state; exact dot-level effects remain
- [ ] PPU tests
- [ ] Reference renderer integration

## Phase 7 — SPC700

- [ ] Phase 7 exit gate verified

- [x] Register state — caller-owned A/X/Y/SP/PSW/PC
- [x] Instruction fetch — bounded S-SMP map, IPL overlay and PC wrap
- [x] Opcode dispatch — all 256 opcode values implemented atomically
- [x] Addressing modes — all architectural operand and address forms represented
- [x] Load/store — byte and word register/memory MOV families
- [x] Arithmetic and flags — byte/word arithmetic, compare, decimal adjust, multiply and divide
- [x] Decimal adjust
- [x] Byte logic and shifts — OR/AND/EOR plus ASL/ROL/LSR/ROR addressing families
- [x] Branches and calls
- [x] Stack behavior
- [x] Bit operations
- [x] Multiply/divide
- [x] Sleep and stop
- [ ] [IN PROGRESS] Instruction timing — documented aggregate 2-12-cycle costs and taken-branch deltas advance timers atomically; exact bus phases remain
- [ ] [IN PROGRESS] RAM and IPL mapping — caller-owned 64 KiB ARAM, optional IPL read overlay, write-through and DSP route tested; TEST/reset policy and hardware validation pending
- [ ] IPL provenance decision
- [ ] [IN PROGRESS] I/O registers — `$F1-$FF` timer/port/DSP routes exist except `$F0` TEST and `$F1` read behavior
- [ ] [IN PROGRESS] Timers — three-stage counters run from machine-derived S-SMP cycles; electrical races pending
- [ ] [IN PROGRESS] Communication ports — four directional latches each way, CPU mirrors, S-SMP `$F4-$F7` and `$F1` input-pair clear verified; cross-clock timing pending
- [x] Instruction tests — every opcode, family matrices and exhaustive byte ADC/SBC, MUL/DIV and decimal-adjust coverage
- [ ] [IN PROGRESS] Timing regressions — aggregate instruction/timer, taken-branch, overflow and missing-operand coverage; per-cycle phases remain

## Phase 8 — DSP / Audio

- [ ] Phase 8 exit gate verified

- [ ] [IN PROGRESS] Register state — 128-byte shadow and `$F2/$F3` address/data path exist; full side effects pending
- [ ] [IN PROGRESS] Eight voice state — BRR/noise, Gaussian, envelope, PMON, dry mixing and `EON` send selection exist; exact microphases remain
- [ ] [IN PROGRESS] BRR decode — exact block arithmetic and four-sample per-voice streaming exist; exact DSP microphases remain
- [ ] [IN PROGRESS] BRR loop/end behavior — completed end blocks set `ENDX` and jump through the current directory loop pointer; exact phase/envelope effects remain
- [ ] [IN PROGRESS] Pitch progression — raw and PMON-adjusted 2.12 steps plus the documented cap exist; exact phases remain
- [ ] [IN PROGRESS] Interpolation — 512 coefficients, four-point wrap/clamp math and BRR output exist; exact microphases remain
- [ ] [IN PROGRESS] ADSR — 11-bit state and shared counter drive resolved BRR/noise samples; exact microphases remain
- [ ] [IN PROGRESS] GAIN envelopes — Direct and four timed modes plus release progression exist; exact microphases remain
- [ ] [IN PROGRESS] Key-on/key-off — alternating-sample polling, five-event preparation and release decay exist; exact microsteps remain
- [ ] [IN PROGRESS] Noise generator — shared LFSR/rates, `NON`, envelope/`OUTX`, stereo and dry mixing exist; exact microphases remain
- [ ] [IN PROGRESS] Pitch modulation — preceding-voice output, staged signed shifts, voice-0 ignore, target-`NON` suppression and cap exist; exact fetch phases remain
- [ ] [IN PROGRESS] Voice volumes — signed `VxVOLL`/`VxVOLR` feed BRR/noise into the resolved ordered mixer; exact phases remain
- [ ] [IN PROGRESS] Main volumes — signed dry output is integrated with echo volume and final clamping; exact fetch phases remain
- [x] Echo buffer — sample-level `ESA`/`EDL` ARAM ring, stereo wrap and protected writes are host verified
- [x] FIR filter — signed eight-tap history, partial wrap, final clamp and low-bit clear are host verified
- [x] Feedback — ordered `EON` send, signed `EFB` and protected left-aligned writes are host verified
- [ ] [IN PROGRESS] Clamping and signed arithmetic — BRR, dry, FIR, feedback and final-mix rules are explicit; post-DAC polarity remains
- [ ] [IN PROGRESS] Sample timing — deterministic sample values and one event per 32 S-SMP cycles exist; exact pipeline phases remain
- [ ] [IN PROGRESS] DSP register side effects — write inhibition, `ENDX`, `FLG` reset/mute/echo gate and `ESA`/`EDL` latching exist; exact phases remain
- [ ] [IN PROGRESS] Golden sample tests — Gaussian, PMON, dry, echo/FIR/feedback, mute, chunk and buffered PCM vectors exist; exact phases remain
- [ ] [IN PROGRESS] SPC700/DSP interconnect — `$F2/$F3`, ARAM source latches, common time and sample-granularity key polling exist; exact microsteps pending
- [ ] [IN PROGRESS] CPU/APU port synchronization — shared mailbox ownership exists; asynchronous clocks and race ordering pending
- [x] Timer integration — machine APU clock advances S-SMP timers
- [ ] [IN PROGRESS] Sample generation pipeline — voices through echo, mute-gated final samples and native PCM handoff exist; exact phases remain
- [x] Deterministic sample buffering — bounded caller-owned FIFO
- [x] Output sample format contract — typed signed 16-bit stereo at native 32 kHz
- [x] Platform-independent PCM output interface — bounded typed reads without device APIs
- [x] Resampling policy — deterministic integer-linear conversion from native 32 kHz to bounded caller blocks at rates at or above 32 kHz
- [x] Underflow/overflow behavior — empty read and drop-newest/count-full semantics are explicit
- [ ] [IN PROGRESS] PS2 48 kHz transport contract — bounded asynchronous launch/poll, accepted-tail retry and fixed-storage RFAuds2 lifecycle are host verified; target/SPU2 validation remains
- [x] Host offline audio tests — bounded vectors plus a 65,536-frame PCM/ARAM hash regression pass across regular and irregular cycle chunks
- [ ] [IN PROGRESS] Integrated APU regression tests — executable clock/CPU/timer/DSP/PCM and machine fixtures exist; exact phases remain

## Phase 9 — Base SNES Integration

- [ ] Phase 9 exit gate verified

- [x] Machine ownership and lifecycle — stable caller-owned image/WRAM/SRAM and platform-neutral instance verified on host
- [ ] [IN PROGRESS] Cartridge reset path — timed CPU reset and power cycle verified; complete hardware reset behavior remains open
- [ ] [IN PROGRESS] CPU/bus/scheduler integration — ROM instruction/WRAM/H-V execution verified; remaining devices absent
- [x] PPU event integration — two recurring machine frames deliver 448 ordered reference lines with a chunk-independent pixel hash
- [x] APU sample integration — machine-clocked SPC700 register programming reaches deterministic non-zero DSP PCM with chunk-independent long hash
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
- [ ] [IN PROGRESS] Audio transport — pinned RFAuds2 binding, bounded 48 kHz contract and lifecycle diagnostic exist; target validation, continuous streaming and optimization remain
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
