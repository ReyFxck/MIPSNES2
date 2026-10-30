# Machine APU/PCM integration audit

Date: 2026-10-29  
Scope: deterministic machine-clocked SPC700 register programming through S-DSP
sample generation into the caller-owned PCM FIFO

## Integrated path

`tests/test_machine_apu_pcm.c` uses only synthetic, repository-owned bytes. A
valid minimal LoROM keeps the 5A22 executing a branch loop. A 64-byte synthetic
IPL starts the SPC700 at `$FFC0` and uses implemented `MOV dp,#imm` instructions
to write S-DSP address/data ports `$F2/$F3`. It configures the noise clock,
`NON`, signed voice and main volumes, direct GAIN and `KON`, then enters `SLEEP`.

The normal `mipsnes2_machine_run_until` path advances the scheduler to absolute
master-clock horizons. `mipsnes2_machine_resume` catches the APU clock up to that
same time, executes the SPC700 program, advances the S-DSP at its fixed cadence
and deposits native signed-16 stereo frames in the machine-owned bounded FIFO.
No platform device, resampler, wall clock or consumer request can advance this
path.

## Deterministic evidence

The fixture runs the complete setup twice to the same absolute horizon of
1,500,019 master clocks:

1. regular increments of 50,000 master clocks;
2. repeating increments of 1, 31, 4,096, 49,999, 777, 32,000, 17 and 45,000
   master clocks.

Every increment stays below the 128-frame FIFO capacity and is drained only
after the machine reaches the requested horizon. Both schedules must produce:

- 2,234 enqueued and consumed PCM frames with zero drops;
- the canonical little-endian 64-bit FNV-1a stream hash
  `412f0b445f6c4610`;
- identical S-SMP cycles, S-DSP sample count/phase, noise/envelope state and
  final non-zero DAC pair;
- an SPC700 in `SLEEP` with the expected `FLG`, `NON` and direct-GAIN registers;
- APU master-clock time exactly equal to scheduler time and the fixed horizon.

The hash is a regression sentinel over behavior constrained by the existing
short instruction, timing, DSP and PCM vectors. It is not a recording from a
real SNES and does not close exact instruction bus phases, S-DSP microphases or
cross-clock race ordering.

## Verification and limits

The focused fixture, complete host suite, ASan+UBSan, GCC `-fanalyzer`, repository
checks and diff checks are the local evidence for this slice. Published Host,
PS2 and PSP workflow results are recorded in `STATUS.md` after completion.
Cross-building the shared core does not prove PS2/PSP runtime audio.

The Phase 9 APU sample-integration wire is now host verified. Exact S-DSP phases,
target-device pacing/latency and sustained audible output remain separate work.

Next primary milestone: capture the existing bounded RFAuds2 lifecycle diagnostic
on the named Android PS2 emulator with version/settings and ELF checksum.
