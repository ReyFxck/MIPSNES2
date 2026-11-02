# ROM-free PS2 audio lifecycle diagnostic

Date: 2026-11-02. Scope: optional PS2 RFAuds2 build, launcher entry and host
mock; no S-DSP algorithm or console timing change.

## First Android result and bind follow-up

The user supplied a NetherSX2 Turnip screenshot and emulator log on 2026-11-02.
The ELF was loaded (emulator CRC `913F8406`, entry `0x00100F58`) and reached the
on-screen diagnostic. It printed `init=7`, `device=-2000`, `calls=0`,
`accepted=0`, `started=0`, `result=INCOMPLETE`. The screenshot labels the
emulator NetherSX2 v2.2n-4248 (Patched) on a Motorola fogos. No on-device ELF
SHA-256, complete settings, physical-PS2 result or audible output was supplied.
This is a real emulator failure at RPC binding, before PCM submission.

At the pinned RFAuds2 `0.1.0` revision, `src/ee/client.c` returns `-2000`
only when the RPC server remains absent after 5,000 bind attempts. That path
occurs after `SifExecModuleBuffer` returned a module ID and accepted the IRX
start result. The code does not prove that the IRX's RPC thread remains alive or
that the emulator implements every relevant IOP behavior. The PS2SDK
`loadfile.h` documents `SifExecModuleBuffer` and `SifSearchModuleByName`; these
diagnostics concern module loading, not the SNES core.

The follow-up build records the first device result and makes up to two delayed
`rfauds2_bind()` calls without reloading the IRX. Each attempt is bounded by the
pinned client. It also prints the IOP module search result. If binding later
succeeds, normal configuration and the original fixed PCM flow continue; if it
stays absent, `INCOMPLETE` and `-2000` remain visible. A successful build or a
retry is not evidence that this specific emulator fault was repaired.

Follow-up verification on the local feature tree: `make BUILD_DIR=build/audio-bind
-j2 test` passed; the focused RFAuds2 fixture passed under ASan/UBSan; GCC
`-fanalyzer` passed on the diagnostic, runtime and fixture;
`tools/check_repository.py` and `git diff --check` passed. A clean sanitizer
build initially produced one unrelated non-executable test file (`test-ps2-pad-input`)
and stopped at shell `Permission denied`. Its ELF bytes were present; restoring
only that generated file's execute bit and rerunning the full ASan/UBSan suite
passed. This is a local artifact-permission anomaly, not an observed test assertion.
The follow-up implementation tree passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41485559468),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41485559662)
and [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41485559447).
The PS2 workflow uploaded artifact `13162271771`; its `SHA256SUMS` entry and
independent extraction hash agree on ELF SHA-256
`af8eed4ec399a85eb6c9ccbac60233d5a71bf3739337fcc084cada5e31f0353c`.
The Android retry result remains pending.

## Why this entry exists

An Android emulator library can launch an ELF without a command line. The prior
audio lifecycle was reachable only through `--rom PATH`, making its target check
depend on launcher argument forwarding, a filesystem path and a supported ROM.
The optional audio-enabled ELF now opens the lifecycle diagnostic by default;
`--audio-diag` also selects it explicitly. `--core-diag` preserves the five-case
CPU screen, and `--rom PATH` retains the existing machine diagnostic. The normal
build still defaults to the five-case screen.

The diagnostic writes alternating signed extrema into a caller-owned 2,048-frame
32 kHz FIFO. It uses the existing 960-frame 48 kHz staging adapter, 1,536-frame
accepted prebuffer threshold, 43 ms configured latency and a limit of 32 pump
attempts. It then asks for one statistics snapshot and one ordered end. It does
not run the machine, use a ROM or request emulated time from the device. A pending
poll can invoke the PS2 caller's one-second pause after every fourth attempt,
up to seven times; the pause is outside emulated time. A pending
RPC after the bounded attempts leaves the static storage live and prints `BUSY`;
shutdown cannot safely reclaim a transport-owned request. `PASS` requires start,
successful statistics and successful shutdown. Queued frames and underruns are
observations, not a quality or latency verdict.

## Target capture procedure

1. Build the optional ELF with the exact RFAuds2 pin documented in `docs/ps2.md`,
   compute `sha256sum platform/ps2/mipsnes2-ps2.elf`, and retain the build commit.
2. On the Android PS2 emulator, record app version/build, BIOS region and settings
   (EE cycle rate, cycle skipping, game fixes, fast-forward). Open the ELF from
   its library without arguments and start without a save state.
3. Capture the full diagnostic screen and, if available, export its stdout log.
   Note `Init`, `Accepted`, `started`, `Stats`, `End`, `bound` and `RESULT`.
4. A black screen, pending RPC, nonzero result or crash needs the exact emulator
   log and settings before assigning a cause. This capture cannot establish
   physical SPU2 behavior, audible polarity, sustained streaming or PS2 speed.

## Verification and limits

`tests/test_ps2_rfauds2_runtime.c` drives the same ROM-free runner through a
synthetic RFAuds2 API. It checks four-call accepted prebuffer/start, diagnostics
and ordered stop/flush/shutdown, plus a permanently pending RPC (32 attempts,
busy snapshot/end without shutdown), init failure cleanup and undersized storage.
The follow-up adds synthetic first-bind-timeout/rebind-success and persistent-
timeout cases, asserting no duplicate IRX load and the reported attempt count.
The source waveform, state and buffers are fixed; there is no heap allocation.
Local verification on the feature tree (host `cc`, C99, 2026-11-02):

- `make BUILD_DIR=build/audio-diag-test -j2 build/audio-diag-test/test-ps2-rfauds2-runtime`
  and the resulting executable: pass.
- `make BUILD_DIR=build/verify-audio-diag -j2 test`: full clean host suite and
  repository checker pass.
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/verify-audio-diag-san
  CFLAGS='-O1 -g -std=c99 -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined' -j2 test`: full clean suite passes.
- `cc -std=c99 -Wall -Wextra -Wpedantic -Werror -fanalyzer` compiled both
  `platform/ps2/audio_diagnostic.c` and its test independently; pass.
- `python3 tools/check_repository.py` and `git diff --check`: pass.

No local PS2/PSP cross compiler or Android emulator is present in this executor.
PR #51's implementation tree passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41481996434)
(GCC and Clang release/sanitizer jobs),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41481996433)
(pinned RFAuds2 IRX/client, cross-link, checksum and artifact upload) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41481996438)
(scaffold cross-link and artifact upload). The PS2 workflow artifact ID
`13160575689` was listed as present and unexpired with ZIP digest
`sha256:dc0b12115b07be07d534096a959002ca37ba60af4d1bfd6c20f237bcf47b735d`.
That is the archive digest; read the artifact's `SHA256SUMS` for the ELF checksum
used in an Android capture. No emulator or hardware result is claimed until a
captured run with checksum is added.

Next primary milestone: resolve or isolate the RFAuds2 RPC bind failure in
NetherSX2 through the new module/rebind diagnostic, with emulator settings,
on-device ELF checksum, screen and log.
