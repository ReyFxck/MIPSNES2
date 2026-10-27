# S-DSP envelope progression audit

Date: 2026-10-27

## Scope

This change adds deterministic envelope state to each portable S-DSP voice. It
does not turn that state into an audio sample. The implemented boundary is:

- a shared counter initialized to zero and counting through `$77FF..$0000`;
- all 32 rate divisors, including the three documented phase-offset families;
- an 11-bit unsigned envelope and retained pre-clamp calculation per voice;
- Attack, Decay, Sustain and Release states;
- normal and fast ADSR attack, exponential decay and exponential sustain;
- Direct GAIN plus timed linear decrease, exponential decrease, linear increase
  and bent increase;
- release by eight units per sample with saturation at zero;
- key-on hold through preload, key-off release, soft-reset zero and immediate
  zero for an end-without-loop BRR header; and
- `ENVX` updates from the high seven envelope bits.

There is no interpolated source sample to multiply by the envelope yet, so this
is observable voice state rather than PCM generation.

## Evidence and arithmetic decisions

The manufacturer [SFC Sound Manual](https://wiki.superfamicom.org/sfc-sound-manual)
defines the ADSR register fields, approximate timing curves, Direct GAIN and the
four timed GAIN shapes. It also specifies that key-off decays in ADSR and GAIN
modes and that `ENVX` exposes an unsigned seven-bit current value.

[Anomie's S-DSP hardware research, revision 1212](https://gist.github.com/nyanpasu64/a0d916ce6924912a7116682bf778e9a0)
supplies the integer state equations and global-counter alignment: 32 rates,
periods from infinity through every sample and offsets 0/536/1040 for rate
families. The reference-C foundation uses the documented countdown/modulo form
because it makes rate switching and chunk invariance explicit. Skipped idle
events advance that counter arithmetically rather than losing phase.

Envelope changes are calculated every serviced sample. A due counter event
commits the clamped 11-bit result; Direct GAIN and Release commit every sample.
The pre-clamp value is retained even when a rate event is not due, which preserves
the documented bent-increase threshold and Attack/Decay state-transition behavior.
Negative right shifts use the same explicit arithmetic helper as BRR decode.

Key-on starts Attack at zero. The first envelope calculation occurs on the fifth
post-key-on event while pitch is still held, matching the existing three-group
BRR preload schedule. `KOFF` selects Release without cancelling BRR transport.
`FLG.RES` selects Release, zeros the envelope and `OUTX`, and keeps the separate
source decoder alive. An end-without-loop BRR header also selects Release and
zeros the envelope immediately.

Exact per-voice S3c timing differs across the physical 32-step loop, including
whether a voice observes the global-counter update before or after its envelope
step. This foundation intentionally applies one documented sample-level phase to
all voices; microphase-accurate register visibility remains open.

## Executable evidence

`tests/test_dsp.c` covers:

- counter initialization, idle bulk advancement and irregular chunk equivalence;
- Direct GAIN beginning on the fifth post-key-on event;
- linear decrease, exponential decrease, linear increase and both bent-increase
  slopes at rate 31;
- a due event for every finite rate row plus rate 2 on a non-due sample;
- rate zero as an infinite/no-commit rate while retaining the hidden result;
- fast ADSR Attack, clamp to `$7FF`, Decay entry and Sustain entry;
- a due Decay update at rate 16 followed by Sustain transition;
- key-off Release, eight-unit steps and saturation at zero;
- soft-reset zeroing of envelope, `ENVX` and `OUTX`; and
- envelope/counter state in one-shot versus irregular BRR-stream chunks.

## Validation

| Check | Result |
|---|---|
| `make clean && make -j2 test` | Pass: full host suite and repository checker |
| Focused GCC ASan/UBSan DSP/APU/S-SMP/machine tests | Pass with leak detection disabled for the traced environment |
| GCC `-fanalyzer` on `dsp.c` and `test_dsp.c` | Pass with warnings promoted to errors |
| `git diff --check` | Pass |

Pinned host/PS2/PSP cross-build workflows remain the publication gate and do not
claim physical-console audio behavior.

## Deliberate limits

The four-point interpolation input still has no independently sourced Gaussian
coefficient set or signed multiply/round sequence. The table printed in the
available research document is explicitly attributed there to another emulator,
so MIPSNES2 does not import it. Envelope multiplication, `OUTX` sample values,
pitch modulation, noise, voice/main volume, mixing, echo/FIR, PCM buffering,
resampling and device output remain absent. Exact 32-step voice microphases also
remain unclaimed.
