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
The Android retry result was pending at this point; the next section records it.

## Second Android capture and loader boundary

The follow-up NetherSX2 Turnip screenshot shows `first: -2000`, `retry: 2`,
`device: -2000`, `IOP module: 0`, no pump calls and `INCOMPLETE`. The pasted
stdout line has the first build's field layout, so it cannot be identified as
the follow-up run; the screenshot itself has the new fields. The second bind
attempts did not expose an RPC server. The value `0` from a name lookup alone
does not establish whether the IRX was retained, because the loader's module
ID, its `_start` result and a lookup immediately after loading were not
recorded. No new performance or audible-output result follows from this image.

The next diagnostic executes the embedded IRX once at the launcher boundary,
records `SifExecModuleBuffer`'s module ID and `_start` result, and records the
name lookup before loading, immediately after loading and after the bounded
bind/PCM attempt. It binds through the existing runtime's loaded-module path,
so this observation does not trigger a second load. A failed load or start is
reported as `INCOMPLETE` without binding. The displayed `INT_MIN` start value
means the loader did not write a start result; it is not an IOP error code.
An ID and start value of zero still need interpretation alongside the three
search results and RPC outcome. The PS2SDK loadfile API returns the module ID
separately from the `_start` result; its search wrapper returns the IOP loader's
ID field or a transport error. See `docs/references.md` for the SDK source.

Local verification of this loader probe: `make BUILD_DIR=build/audio-load-verify
-j2 build/audio-load-verify/test-ps2-rfauds2-runtime` and its executable passed;
`make BUILD_DIR=build/audio-load-full -j2 test` passed the clean host suite and
repository checks. The full ASan/UBSan suite with `ASAN_OPTIONS=detect_leaks=0`,
`BUILD_DIR=build/audio-load-san`, `CFLAGS='-O1 -g -std=c99
-fsanitize=address,undefined -fno-omit-frame-pointer'` and matching `LDFLAGS`
passed after the generated `test-ppu-window-mask` ELF lacked execute permission;
only that generated file was made executable, then the entire suite was rerun.
GCC `-fanalyzer` compiled the changed diagnostic and fixture, and
`tools/check_repository.py` plus `git diff --check` passed. Android capture of
this revision is pending.
The implementation tree passed
[Host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41493138888),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41493138639)
and [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41493138633).
The uploaded PS2 artifact `13164862861` contains a PS2 ELF with SHA-256
`58e50f0d50bd06f8789168701c39f86aaeaf3e57b21897d682bd7b04fbfa3eda`,
confirmed by extracting and hashing it against the artifact's `SHA256SUMS`.
An emulator capture of this ELF was pending at this point; the next section
records its diagnostic layout and result.

## Third Android capture and buffer-loader correction

The user ran the v3 ELF in NetherSX2 Turnip. Its screen showed `IRX load:
282624`, `start: 0`, `IOP name: 0 > 0 > 0`, `first: -2000`, two failed
bind-only retries and no PCM pump. The start result of zero is not sufficient
proof of a resident IRX when the module ID looks like an IOP address and the
RPC server never appears. The capture does not contain an on-device checksum
or IOP debug log.

The pinned PS2SDK's SBV documentation states that ROM LOADFILE may lack the
`LoadModuleBuffer` RPC used by `SifExecModuleBuffer`, and that
`sbv_patch_enable_lmb()` prepares this service before an EE-embedded IRX load.
The MIPSNES2 optional launcher had not called it. The PS2-only launcher now
prepares LOADFILE once after SIF RPC initialization and before either its ROM
or ROM-free embedded-IRX path. A nonzero patch result skips the IRX load and is
reported on screen/stdout; it is not mistaken for a successful module start.
The diagnostic still records load/start/search results when the patch succeeds.
The host fixture mocks the SDK calls to verify patch-before-load order and
that a patch failure prevents a load. This is a reasoned correction of the
initialization sequence, pending another target capture; it does not yet prove
RPC or audible playback on NetherSX2 or a physical PS2.

Local checks: `make BUILD_DIR=build/lmb-full -j2 test` and the focused
`test-ps2-iop-module-loader` passed. The full ASan/UBSan suite in
`build/lmb-san` passed after the generated `test-ppu-oam` ELF was found without
execute permission; only that generated file was made executable and the full
suite rerun. GCC `-fanalyzer` compiled the loader and its fixture;
`tools/check_repository.py` and `git diff --check` passed. PS2/PSP compilers
are unavailable locally, so the optional SDK link and ELF require CI.
The first PS2 CI attempt failed at link because the integration requested
`-lsbv`; the pinned PS2SDK `ee/sbv/Makefile` names this archive
`libpatches.a`. The link was corrected to `-lpatches` without changing the
SBV implementation or vendoring it. This attempt did not produce a new ELF.
The corrected implementation tree passed
[Host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41502163448),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41502163364) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/41502163536).
PS2 artifact `13169126880` was extracted and independently hashed; its ELF
matches `SHA256SUMS` at
`cc569d603dd7d10e1298bad36c702d7167510aa6bdcb7fc14c53ea30da7f0dcc`.
The corrected executable was subsequently run in NetherSX2; see the result
below. Physical-console execution remains unavailable.

## Corrected ELF on NetherSX2

The user supplied a screenshot of the no-argument diagnostic on NetherSX2
v2.2n-4248 (Patched), Motorola fogos, after the SBV correction. Its exact
on-screen values are `LMB patch: 0`, `IRX load: 25`, `start: 0`,
`IOP name: 2 > 0 > 52`, `Init: 0`, `first: 0`, `retry: 0`,
`Pump: 5 (9 calls)`, `Accepted: 1920`, `started: 1`,
`Stats: 0`, `queued: 896`, `underruns: 0`, `End: 0`,
`device: 0`, `bound: 0`, and `RESULT: PASS`.

This is direct emulator evidence that the embedded IRX loaded and the RPC
lifecycle accepted synthetic PCM, reached the prebuffer/start threshold,
returned statistics, and completed shutdown. The name-search values are
reported verbatim; their `2 > 0 > 52` sequence does not establish a stable
module identity. The screenshot matches the corrected ELF's field layout,
but no on-device SHA-256 was supplied. It does not establish audible output,
the SNES SPC700/S-DSP signal, sustained streaming, device latency, real PS2
behavior, or full-speed performance. No commercial ROM was used.

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
used in an Android capture. A screenshot can establish only the values it
displays; an on-device checksum is still needed for exact byte identity.

Next primary milestone: bounded continuous SNES PCM delivery through the PS2
32-to-48 kHz/RFAuds2 path without device-driven emulated time.
