# core/cpu/65c816

Contains the caller-owned reference CPU state and a deliberately partial 39-opcode
interpreter subset. Immediate, Direct Page, Direct-indexed, absolute,
absolute-indexed and absolute-long load/store foundations are covered. Read
[docs/65c816.md](../../../docs/65c816.md) for the exact implemented opcode list,
timing contract and known limits. Do not infer complete 65C816 support.
