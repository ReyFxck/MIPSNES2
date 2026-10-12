# core/cpu/65c816

Contains the caller-owned reference CPU state and a deliberately partial 105-opcode
interpreter subset. Immediate, Direct Page, Direct-indexed, Direct-indirect,
absolute, absolute-indexed and absolute-long load/store foundations plus the complete
register-transfer and branch families, implied single-flag status controls and
accumulator/index register increment-decrement operations, complete BIT/CPX/CPY
families and base immediate/Direct/absolute ORA/AND/EOR/CMP forms are covered. Read
[docs/65c816.md](../../../docs/65c816.md) for the exact implemented opcode list,
timing contract and known limits. Do not infer complete 65C816 support.
