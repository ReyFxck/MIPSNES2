# APU and audio

## Scope and current state

APU integration owns SPC700/DSP coordination, timers, CPU communication and generation
of final emulated samples. Platform code owns delivery buffers and device APIs. No
audio pipeline exists.

## Design direction

Define rational clock relationship to the main scheduler, port synchronization and
sample buffer ownership before implementation. Platform pacing must not alter
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

Planning checklist only; [STATUS.md](../STATUS.md) is canonical. Every item below
remains unimplemented or unverified; document implementation evidence before promotion.

- [ ] SPC700/DSP interconnect
- [ ] CPU/APU port synchronization
- [ ] Timer integration
- [ ] Sample generation pipeline
- [ ] Deterministic sample buffering
- [ ] Output sample format contract
- [ ] Platform-independent PCM output interface
- [ ] Resampling policy
- [ ] Underflow/overflow behavior
- [ ] Host offline audio tests
- [ ] Integrated APU regression tests

## Research and decisions

No SPC700/S-DSP semantics are implemented. RFAuds2 is transport infrastructure only
and must never become a core dependency or SNES timing source. The inspected candidate
was RFAuds2 `0.1.0-alpha.1` at revision
`f82e0967104fc96f37259e1abc124341eef6a76c`; an eventual dependency still requires an
explicit reviewed pin. Before emulation implementation, record hardware references
and independently authored regression evidence.
