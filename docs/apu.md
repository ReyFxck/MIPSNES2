# APU and audio

## Scope and current state

APU integration owns SPC700/DSP coordination, timers, CPU communication and generation
of final emulated samples. Platform code owns delivery buffers and device APIs. The
logical S-SMP timer block and a caller-owned instruction-atomic SPC700 interpreter
covering all 256 opcode values now run under a nominal rational machine-to-S-SMP
clock. The same clock advances the timers and a first S-DSP register/sample-event
foundation. Eight voice-control records, sample-granularity key polling, source
directory latching and a pure BRR block decoder now exist. Per-voice BRR
group streaming, a twelve-sample ring, unmodulated pitch progression and the
four-point interpolation input are connected. The published 512-entry Gaussian
table resolves BRR output, and sample-level `PMON` modulates voices 1 through 7
from the preceding post-envelope sample. The shared rate counter now drives
11-bit ADSR, all GAIN modes and release, plus the shared 15-bit noise LFSR selected
per voice by `NON`. Resolved BRR/noise voices apply the envelope as left-aligned
signed-15 output, update `OUTX` and expose signed left/right voice-volume samples.
Those voices are mixed in order with per-addition saturation and signed main
volume. There is still no echo, mute/DAC, PCM buffer or audio output.

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

- [ ] [IN PROGRESS] SPC700/DSP interconnect — `$F2/$F3`, ARAM source directories, common time and sample-granularity key polling exist; exact DSP microsteps remain
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
The first voice-control and decoder slice is recorded in the
[S-DSP voice/BRR audit](sdsp-voice-brr-foundation-audit.md).
The connected source traversal and pitch input are recorded in the
[S-DSP BRR stream/pitch audit](sdsp-brr-stream-pitch-audit.md).
The next state-only audio layer is recorded in the
[S-DSP envelope audit](sdsp-envelope-audit.md).
The shared noise source is recorded in the
[S-DSP noise audit](sdsp-noise-audit.md).
Its resolved per-voice path is recorded in the
[S-DSP noise voice-output audit](sdsp-noise-voice-output-audit.md).
The resolved ordered/main-volume path is recorded in the
[S-DSP dry-mix audit](sdsp-dry-mix-audit.md).
Gaussian BRR output, corrected left-aligned sample scale and sample-level PMON
are recorded in the [S-DSP Gaussian/PMON audit](sdsp-gaussian-pmon-audit.md).
RFAuds2 is transport infrastructure only
and must never become a core dependency or SNES timing source. The inspected candidate
was RFAuds2 `0.1.0-alpha.1` at revision
`f82e0967104fc96f37259e1abc124341eef6a76c`; an eventual dependency still requires an
explicit reviewed pin. Before emulation implementation, record hardware references
and independently authored regression evidence.
