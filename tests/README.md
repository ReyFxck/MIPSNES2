# Tests

`make test` executes the address/logging contracts with logging enabled and disabled,
the logical-bus and 256-opcode CPU regressions, the initial exact-once/resumable CPU
operation-contract regressions, Ricoh 5A22 CPU-I/O register/latch
tests, deterministic scheduler/5A22 timing foundation tests, partial SNES WRAM/MMIO/timed-CPU integration tests, base LoROM/
HiROM power-of-two/irregular and ExHiROM mapping tests, copier-prefix/registration/
header analysis, whole-image mapper selection and caller-owned construction tests,
revisioned SRAM dirty-state/acknowledgement tests,
machine-level CPU-to-SRAM persistence-handoff tests,
PS2 active-low digital-pad translation tests,
black-box host CLI smoke tests and repository
consistency checks.
See [testing policy](../docs/testing.md). No external test ROM is committed yet.

`test-spc700-cpu` exercises caller-owned SPC700 registers, mapped opcode fetch,
IPL selection, PC/operand wrap, all 256 dispatch byte values, the initial PSW and
register/immediate/direct-page/absolute/indexed/indirect and memory-to-memory MOV
instructions, PSW.P page selection, eight-bit direct-page/pointer wrap, little-endian absolute operands, 16-bit
operand/effective-address wrap, N/Z transitions, mapped MMIO, unavailable
operands/targets, cycle overflow and
instruction-to-timer advancement. It does not claim a complete interpreter or
per-cycle bus sequencing.

`test-dsp` verifies the 128-byte S-DSP register/address boundary, inhibited
writes, `ENDX` clear, internal `FLG` state, 32-S-SMP-cycle events, eight-voice
key polling/source-directory state and exact standalone BRR shift/filter/clamp
vectors. It also checks three-group key-on preload, twelve-sample ring rotation,
14-bit pitch/fraction progression, end/loop traversal, live loop-directory refresh,
release/reset decode continuation, ARAM address wrap and one-shot/chunked stream
equivalence. Envelope fixtures cover the global rate/offset phase, five-event
key-on hold, fast ADSR attack, decay/sustain transitions, Direct GAIN, all four
timed GAIN curves, saturating release, reset zeroing and `ENVX` updates.
Noise fixtures cover the `$4000` seed, signed-15 conversion, `NON` sharing,
rate-zero hold, fast/slow/offset counter events, a million-step scalar/bulk
equivalence check and irregular clock chunking while BRR transport continues.
Resolved-source fixtures cover the five empty key-on samples, left-aligned
signed-15 envelope output, low-bit clearing, negative rounding, full-range
`OUTX`, signed/inverted left-right voice volumes, Release ordering and reset
zeroing. Gaussian fixtures hash and inspect all 512 coefficients, validate every
phase quartet, pin independent interpolation/overflow vectors and drive a BRR
voice through envelope, volume and dry mixing. PMON fixtures cover positive and
negative modulation, staged negative rounding, target-`NON` suppression, ignored
voice-0 bit and the `$7fff` position cap. Dry-mix fixtures cover Voice-0-to-7
accumulation, saturation after each addition, all eight voice slots, explicit
inclusion masks, signed main-volume rounding/wrap, reset zeroing and chunk
equivalence. Echo fixtures validate pure eight-tap FIR arithmetic, first-seven
wrap, newest-tap clamp, low-bit clearing, `EON` send selection, one-sample `ESA`
application, `EDL=0` and delayed relatching, 16-bit ARAM wrap, signed `EVOL`/`EFB`,
write protection, feedback writes, mute continuation and irregular clock chunks.
`test-apu-timing` verifies nominal master-to-S-SMP conversion, one-shot/
chunked equivalence, atomic SPC700 admission and credit, timer coordination,
SLEEP continuation, overflow handling and non-mutating DSP/ARAM preflight. BRR and noise voices now reach
per-voice stereo state, sample-level pitch modulation, ordered dry/echo paths and
a mute-gated final sample, but there is no PCM queue or device output.

`test-spc700-alu` verifies the exact 100-opcode OR/AND/EOR/CMP/ADC/SBC,
ASL/ROL/LSR/ROR and byte INC/DEC block across every documented addressing form.
It includes 54 accumulator and 18 direct-memory ALU cases, all 28 shift/INC/DEC
forms, exact lengths/cycles, P-selected mapped memory, MMIO failure/rollback,
timer coupling and all 262,144 byte ADC/SBC operand/carry combinations.

`test-spc700-complete` independently counts the remaining 106 opcodes, executes
all 256 base dispatch paths and verifies bit, branch, call/return, stack, word,
compare, multiply/divide, decimal-adjust and wait-state behavior. It exhausts all
65,536 MUL inputs, every X/Y pair with six A samples for DIV, all A/H/C decimal
adjust inputs and cycle-overflow boundaries. This completes opcode semantics, not
per-cycle bus sequencing, interrupt entry or audio synthesis. The later APU bridge
reuses the atomic dispatcher under a deterministic clock.

The CPU suite includes boundary-visible ABORT vectors, native/emulation stack layouts,
priority, WAI restart-address behavior and invalid-bus diagnostics. It does not claim
physical mid-instruction ABORT sampling or rollback.

`test-cpu-cycles` covers the caller-owned peek/commit cursor for opcode fetch,
106 opcodes and boundary `ABORT`/`NMI`/`IRQ` entry, including repeated peek without
effects, stale-operation rejection, 8/16-bit width, wrapping and exact meaningful
access/internal operation order. Its RMW matrix covers all thirty-six accumulator/
memory shifts, rotates, INC/DEC, TSB/TRB and STZ opcodes at both M widths. The other
150 opcode remainders and reset remain explicitly legacy-atomic; the scheduler does
not consume this cursor yet.
