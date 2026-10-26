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
register/immediate/direct-page/indexed-direct-page/absolute MOV instructions,
PSW.P page selection, eight-bit indexed wrap, little-endian absolute operands,
16-bit operand wrap, N/Z transitions, mapped MMIO, unavailable
operands/targets, cycle overflow and
instruction-to-timer advancement. It does not claim a complete interpreter or
per-cycle bus sequencing.

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
