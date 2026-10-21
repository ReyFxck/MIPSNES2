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

The CPU suite includes boundary-visible ABORT vectors, native/emulation stack layouts,
priority, WAI restart-address behavior and invalid-bus diagnostics. It does not claim
physical mid-instruction ABORT sampling or rollback.

`test-cpu-cycles` covers the caller-owned peek/commit cursor for opcode fetch and
`EA/A9/8D/C2/E2`, including repeated peek without effects, stale-operation rejection,
8/16-bit width, wrap and write order. All other opcode remainders and interrupt entry
remain explicitly legacy-atomic; the scheduler does not consume this cursor yet.
