# core/cpu/65c816

Contains the caller-owned reference CPU state and a deliberately partial 159-opcode
interpreter subset. Immediate, Direct Page, Direct-indexed, Direct-indirect,
absolute, absolute-indexed, absolute-long and stack-relative LDA/STA families plus the complete
register-transfer and branch families, implied single-flag status controls and
accumulator/index register increment-decrement operations, complete BIT/CPX/CPY
families and every ORA/AND/EOR/CMP form, including their two stack-relative modes,
plus binary immediate ADC/SBC are covered. Decimal ADC/SBC stops explicitly rather
than falling back to binary arithmetic. Read
[docs/65c816.md](../../../docs/65c816.md) for the exact implemented opcode list,
timing contract and known limits. Do not infer complete 65C816 support.
