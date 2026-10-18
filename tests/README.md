# Tests

`make test` executes the address/logging contracts with logging enabled and disabled,
the logical-bus and 256-opcode CPU regressions, deterministic scheduler/5A22 timing
foundation tests, partial SNES WRAM/MMIO/timed-CPU integration tests, base LoROM/
HiROM power-of-two/irregular and ExHiROM mapping tests, copier-prefix/registration/
header analysis, whole-image mapper selection tests, black-box host CLI smoke tests and repository
consistency checks.
See [testing policy](../docs/testing.md). No external test ROM is committed yet.
