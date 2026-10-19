# Tests

`make test` executes the address/logging contracts with logging enabled and disabled,
the logical-bus and 256-opcode CPU regressions, Ricoh 5A22 CPU-I/O register/latch
tests, deterministic scheduler/5A22 timing foundation tests, partial SNES WRAM/MMIO/timed-CPU integration tests, base LoROM/
HiROM power-of-two/irregular and ExHiROM mapping tests, copier-prefix/registration/
header analysis, whole-image mapper selection and caller-owned construction tests,
revisioned SRAM dirty-state/acknowledgement tests,
black-box host CLI smoke tests and repository
consistency checks.
See [testing policy](../docs/testing.md). No external test ROM is committed yet.
