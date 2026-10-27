# Tests

`make test` executes the address/logging contracts with logging enabled and disabled,
the logical-bus and 256-opcode CPU regressions, the initial exact-once/resumable CPU
operation-contract regressions, Ricoh 5A22 CPU-I/O register/latch
tests, deterministic scheduler/5A22 timing foundation tests, partial SNES WRAM/MMIO/timed-CPU integration tests, base LoROM/
HiROM power-of-two/irregular and ExHiROM mapping tests, copier-prefix/registration/
header analysis, whole-image mapper selection and caller-owned construction tests,
revisioned SRAM dirty-state/acknowledgement tests,
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
equivalence. `test-apu-timing` verifies nominal master-to-S-SMP conversion, one-shot/
chunked equivalence, atomic SPC700 admission and credit, timer coordination,
SLEEP continuation and overflow handling. The four-point API is only the input to
future Gaussian interpolation; these events are not PCM output.

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
