# core/cpu/65c816

Contains the caller-owned reference CPU state and an explicit 256-opcode reference
interpreter. Immediate, Direct Page, Direct-indexed, Direct-indirect,
absolute, absolute-indexed, absolute-long and stack-relative LDA/STA families plus the complete
register-transfer and branch families, implied single-flag status controls and
accumulator/index register increment-decrement operations, complete BIT/CPX/CPY
families and every ORA/AND/EOR/CMP form, including their two stack-relative modes,
plus every ADC/SBC addressing form in binary and packed-BCD decimal modes, every
ASL/LSR/ROL/ROR form, memory INC/DEC, TSB/TRB, STZ, every stack push/pull form and
every jump/subroutine call/return form, BRK/COP/RTI/WDM, MVP/MVN, WAI/STP and the
IRQ/NMI plus boundary-visible ABORT contract is covered. Read
[docs/65c816.md](../../../docs/65c816.md) for the exact implemented opcode list,
timing contract and known limits. Mid-instruction ABORT rollback/sampling,
signal-level reset/bus behavior, exact 5A22 timing and scheduler integration remain
incomplete; do not infer a complete SNES CPU subsystem.
