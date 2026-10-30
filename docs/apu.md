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
volume. A separate `EON` sum now feeds the stereo ARAM echo ring, eight-tap FIR
and signed feedback path; signed echo volume joins the final clamped mix and
`FLG.MUTE` gates a deterministic DAC-facing sample. An optional caller-owned FIFO
now receives signed 16-bit left/right frames at the native 32 kHz cadence. The
bounded interface drops new frames and counts them when full, while an empty read
returns no frame; neither case changes emulated time. A separate bounded
integer-linear converter now drains already-produced frames at output rates greater
than or equal to 32 kHz. It keeps at most two source frames, emits only into
caller-bounded blocks and returns no fabricated frame on starvation. There is still
no verified audio-device output. The PS2-only build has a bounded RFAuds2 lifecycle
diagnostic, but no target execution or sustained-playback evidence.

## Design direction

Keep port synchronization and sample-buffer ownership explicit. Platform pacing must not alter
deterministic core state. The common boundary exposes typed emulated PCM without device
APIs. The common rate converter is a deterministic reference utility, not an audio
clock: it never runs the machine to satisfy a consumer. PS2 uses an optional PS2-only
RFAuds2 lifecycle/transport adapter; PSP and host remain independent.
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
- [ ] [IN PROGRESS] Sample generation pipeline — voices through echo, mute-gated final samples and native PCM handoff exist; exact DSP phases remain
- [x] Deterministic sample buffering — caller-owned bounded FIFO preserves order and diagnostics
- [x] Output sample format contract — typed signed 16-bit left/right frames at native 32 kHz
- [x] Platform-independent PCM output interface — bounded reads expose frames without device APIs
- [x] Resampling policy — integer-linear native-rate conversion with bounded state and deterministic signed rounding
- [x] Underflow/overflow behavior — empty reads return zero; full writes drop newest and count it
- [ ] [IN PROGRESS] Host offline audio tests — PCM value/order/wrap/chunk vectors exist; long hashes remain
- [ ] [IN PROGRESS] Integrated APU regression tests — clock chunking, CPU/timer/DSP cadence, PCM and machine execution are covered; exact phases remain

## Research and decisions

All 256 SPC700 opcode values have instruction-atomic semantics with documented
aggregate cycle totals, including word/control-flow/stack/bit/multiply/divide and
wait-state families. Nominal synchronized scheduling now exists. Exact bus phases,
interrupt/reset entry, cross-clock races, exact S-DSP microphases and PCM transport
do not.
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
ARAM echo, FIR/feedback and the mute-gated final sample are recorded in the
[S-DSP echo/output audit](sdsp-echo-output-audit.md). The native typed output
contract, bounded FIFO and machine attachment are recorded in the
[PCM buffer audit](pcm-buffer-audit.md). The transport-side rational phase,
integer interpolation and chunk/starvation contract are recorded in the
[PCM resampler audit](pcm-resampler-audit.md).
RFAuds2 is transport infrastructure only and must never become a core dependency or
SNES timing source. The initial audit covered the earlier code at
`f82e0967104fc96f37259e1abc124341eef6a76c`; the reviewed PS2-only integration now
pins stable source version `0.1.0` commit
`257ca54afb08b9c8c2197c03d96fd32d5b15b2bc`. Hardware references and independently
authored regression evidence remain mandatory for emulated behavior.
The PS2 coordinator embeds the pinned IRX, prebuffers only frames reported accepted,
and performs start/statistics/stop/flush/shutdown outside `core/`. Its bounded ROM
diagnostic advances predetermined machine horizons before pumping; no device result
can select an emulated deadline or request more samples.
