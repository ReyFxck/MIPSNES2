# APU and audio

## Scope and current state

APU integration owns SPC700/DSP coordination, timers, CPU communication and generation
of final emulated samples. Platform code owns delivery buffers and device APIs. The
logical S-SMP timer block and a caller-owned instruction-atomic SPC700 interpreter
covering all 256 opcode values now run under a nominal rational machine-to-S-SMP
clock. The same clock advances the timers and a first S-DSP register/sample-event
foundation. There is still no voice synthesis, PCM buffer or audio pipeline.

## Design direction

Keep port synchronization and sample-buffer ownership explicit. Platform pacing must not alter
deterministic core state. The common boundary will expose emulated PCM without device
APIs. PS2 will prefer a PS2-only RFAuds2 adapter; PSP and host remain independent.
The SPU2/RFAuds2 output rate must never drive SPC700/S-DSP time. Decide output format,
resampling and underrun policy from correctness and target latency measurements.

## Verification criteria

Test bidirectional communication, event ordering, long-run sample counts, buffer
limits and deterministic sample hashes without requiring physical output. Console
audio tests must include underruns and sustained playback; host offline tests do not
prove RFAuds2/SPU2 or PSP delivery.

## Work breakdown

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Document
implementation evidence before promotion.

- [ ] [IN PROGRESS] SPC700/DSP interconnect — `$F2/$F3` register route and common cycle domain exist; voice effects and exact polling remain
- [ ] [IN PROGRESS] CPU/APU port synchronization — shared mailbox is clocked at atomic boundaries; electrical races remain
- [x] Timer integration — machine APU clock advances the existing S-SMP timers
- [ ] Sample generation pipeline
- [ ] Deterministic sample buffering
- [ ] Output sample format contract
- [ ] Platform-independent PCM output interface
- [ ] Resampling policy
- [ ] Underflow/overflow behavior
- [ ] Host offline audio tests
- [ ] [IN PROGRESS] Integrated APU regression tests — clock chunking, CPU/timer/DSP cadence and machine execution covered; PCM vectors pending

## Research and decisions

All 256 SPC700 opcode values have instruction-atomic semantics with documented
aggregate cycle totals, including word/control-flow/stack/bit/multiply/divide and
wait-state families. Nominal synchronized scheduling now exists. Exact bus phases,
interrupt/reset entry, cross-clock races and most S-DSP semantics do not.
See the [APU clock/DSP register audit](apu-clock-dsp-register-audit.md).
RFAuds2 is transport infrastructure only
and must never become a core dependency or SNES timing source. The inspected candidate
was RFAuds2 `0.1.0-alpha.1` at revision
`f82e0967104fc96f37259e1abc124341eef6a76c`; an eventual dependency still requires an
explicit reviewed pin. Before emulation implementation, record hardware references
and independently authored regression evidence.
