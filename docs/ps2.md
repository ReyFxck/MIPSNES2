# PlayStation 2 — primary target

The EE initially performs SNES CPU, bus, scheduler, PPU, DMA/HDMA, SPC700, DSP and
cartridge work. This is not a generic MIPS port. PS2 receives architectural priority
when measured gains preserve correctness and isolation.

## Current diagnostic and build

`platform/ps2/main.c` displays five deterministic shared-core checks by default
using the PS2SDK debug screen and logs the same results to stdout. Its original embedded
fixtures exercise CPU instructions, scheduler horizons and explicit ABORT write
qualification; `tests/test_ps2_diagnostic.c` runs the identical suite on the host.
The optional `--rom PATH` entry reads an image through `fopen` from a filesystem
that the launcher has already made available, constructs the shared machine and
runs one bounded reference frame in the standalone build. The RFAuds2-enabled
diagnostic instead makes eight bounded calls toward fixed absolute NTSC frame horizons, each with a
100,000-sequence bound. It prints CPU and timing
status and captures bounded reference lines in a static RGBA32 texture. A persistent
single-buffer GS presenter may upload one capture after each fixed horizon; its
vertical-sync wait occurs only after machine advancement and cannot set emulated
time. This path
also attempts a nonblocking port-0 `libpad` sample before each fixed frame horizon
and injects the translated standard-pad word into the machine. It has no console
runtime validation, continuous gameplay loop, verified input/audio or SRAM
persistence. It does not
mount a USB device or provide a file picker. The original five-case diagnostic
still runs without arguments; it is not a game or the final GS/video backend.
The new ROM path has no console runtime evidence. The default diagnostic has
user-captured emulator results, while physical PS2 validation remains open.
The original scaffold record is in
[bootstrap-audit.md](bootstrap-audit.md); see the
[ARMSX2 diagnostic guide](ps2-armsx2-diagnostic.md) for the mobile test.
See the [PS2 frame audit](ps2-video-frame-audit.md) for the capture format and
its tested limits, and the [GS audit](ps2-gs-audit.md) for the unverified
persistent presenter.

The SDK's own [EE file-reading sample](https://github.com/ps2dev/ps2sdk/blob/2c670453980fcc3fe46ead6399b730b12c8556eb/ee/rpc/audsrv/samples/playwav/playwav.c)
uses `sceSifInitRpc`, `fopen("host:...", "rb")` and `fread`. This adapter uses
standard C file calls only; device registration remains a separate backend task.

Linux x86-64 toolchain pin: ps2dev **v2.0.0**, archive SHA-256 and URL in
[tools/toolchains.json](../tools/toolchains.json). Despite `latest` in the asset
filename, the release tag and content digest are fixed.

```sh
python3 tools/setup_toolchain.py ps2 /tmp/mipsnes2-sdks "$HOME/.cache/mipsnes2-toolchains"
export PS2DEV=/tmp/mipsnes2-sdks/ps2dev
export PS2SDK="$PS2DEV/ps2sdk"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PATH"
make -C platform/ps2
```

Use a fresh install root when updating; do not overlay SDK versions. Build output is
`platform/ps2/mipsnes2-ps2.elf`. The installer verifies cached archives too.

Verified conventions: `EE_BIN`, `EE_OBJS`, `EE_LIBS`, `EE_INCS`, `EE_CFLAGS`,
`$(PS2SDK)/samples/Makefile.pref` and `Makefile.eeglobal`, and the
`mips64r5900el-ps2-elf-` compiler prefix. Sources inspected on 2026-10-09:

- [PS2SDK sample Makefile](https://github.com/ps2dev/ps2sdk/blob/2c670453980fcc3fe46ead6399b730b12c8556eb/ee/debug/samples/helloworld/Makefile.sample)
- [EE global rules](https://github.com/ps2dev/ps2sdk/blob/2c670453980fcc3fe46ead6399b730b12c8556eb/samples/Makefile.eeglobal_sample)
- [Tool prefix definitions](https://github.com/ps2dev/ps2sdk/blob/2c670453980fcc3fe46ead6399b730b12c8556eb/samples/Makefile.pref_sample)
- [Pinned binary release](https://github.com/ps2dev/ps2dev/releases/tag/v2.0.0)

The inspected upstream source revision documents conventions; the archive digest
identifies the actual SDK/toolchain used for builds. These are distinct records.
No emulator implementation was copied. The original scaffold uses standard C
functions; the isolated target adapters use only the documented PS2SDK APIs named
in their audits.

## Pad input

The PS2 ROM diagnostic owns one fixed 256-byte, 64-byte-aligned `libpad` DMA area.
It uses the PS2SDK's documented `SifLoadModule`, `padInit`, `padPortOpen`,
`padGetState` and `padRead` sequence without blocking the emulator on controller
readiness. Stable port-0 data is translated as Cross/Square/Circle/Triangle to SNES
B/Y/A/X, direct directions and Start/Select, and L1/R1 to SNES L/R. L2/R2, stick
buttons and analog axes are intentionally unused for the standard-pad profile.
Unavailable reads inject a disconnected zero word rather than retaining stale input.

Host tests cover the complete active-low digital translation. Pinned PS2SDK
cross-builds prove only API/link compatibility; ARMSX2/NetherSX2 and physical PS2
runtime behavior, hotplug recovery, continuous gameplay polling, second port,
multitap and PSP mapping remain open. See the
[PS2 pad-input audit](ps2-pad-input-audit.md).

## PCM backend: RFAuds2

[ReyFxck/RFAuds2](https://github.com/ReyFxck/RFAuds2) is the stable, functional PS2
PCM transport selected for the optional backend. The decision was
recorded after inspecting RFAuds2 `0.1.0-alpha.1` at revision
`f82e0967104fc96f37259e1abc124341eef6a76c` on 2026-10-10, including its README,
API/architecture material and emulator-integration guidance. The later
`fix/comprehensive-hardening` work was corrected, re-reviewed and merged by
[RFAuds2 PR #2](https://github.com/ReyFxck/RFAuds2/pull/2). Its `0.1.0-alpha.2`
`main` merge commit `38cd7ae09a1ad646c9009e5ff38f675ecc8ee584` supplied the reviewed
code base; see the [branch review](rfauds2-branch-review.md). The current exact
integration pin is stable source version `0.1.0` at
`257ca54afb08b9c8c2197c03d96fd32d5b15b2bc`.

RFAuds2's supported 48 kHz stereo S16 path is stable and functional. Its repository
has deterministic converter, mixer, transport, refill and lifecycle regressions plus
PS2 cross-build coverage. Physical FAT/Slim validation and long emulator stress remain
release-quality evidence gaps; MIPSNES2 does not convert build success into a target
runtime claim.

The required boundary is:

```text
SNES SPC700/S-DSP -> common emulated PCM contract -> PS2 adapter -> RFAuds2 -> IOP/SPU2
```

The emulated SPC700/S-DSP owns SNES audio timing. The common bounded reference
converter can drain native 32 kHz frames into 48 kHz caller blocks without running
the machine. The PS2-only adapter borrows fixed staging storage and makes at most one
asynchronous launch or poll per pump. Launch copies the request; poll later reports
pending or the accepted prefix. The adapter retains zero/partial/error tails before
it consumes more source PCM. Starvation and backpressure are reported and counted;
neither can request more emulation work. RFAuds2 only transports final PCM for the PS2 device;
its 48 kHz SPU2 boundary must never set the emulated clock. PSP keeps a dedicated
PSP audio backend, and host keeps offline/testing output.
S-DSP/sample generation must remain testable without RFAuds2 or physical SPU2.

`platform/ps2/rfauds2_transport.c` is the thin mapping to the pinned
`rfauds2_submit_s16_async`/`rfauds2_submit_poll` API. It is enabled only with
`MIPSNES2_RFAUDS2=1` and an external `RFAUDS2_ROOT` at the exact pinned commit;
the Makefile rejects another checkout. The PS2 CI builds that exact external source,
including its IRX against PS2SDK source commit
`4996c6f63d42d2cf5d68dfa2f7bb74b9e3c653f5`. PS2SDK `bin2c` embeds that IRX as a
generated, ignored C build artifact before the optional binding is cross-linked;
CI builds the host tool from that same PS2SDK source checkout instead of assuming
the binary toolchain archive installed it.
`platform/ps2/rfauds2_runtime.c` coordinates init, latency/volume/stat reset,
accepted-frame prebuffering, start, statistics, stop, flush and shutdown with fixed
caller-owned storage. The ROM diagnostic advances eight predetermined emulated
frame horizons regardless of device results; pumps and bounded final polls cannot
advance the machine. Do not copy RFAuds2 source into unrelated `core/` directories.
Target execution, continuous streaming and real-hardware underrun/backpressure
validation remain. See the
[PS2 audio-adapter audit](ps2-audio-adapter-audit.md).

## Dedicated optimization candidates

No optimization below has been implemented or benchmarked. These are research gates.

- [ ] EE compact state, branch reduction and software pipelining after profiling
- [ ] Cache-aware layout and reduced memory traffic
- [ ] Alignment/aliasing contracts and bounded preallocation
- [ ] MIPS assembly for demonstrated hotspots with a tested C reference
- [ ] MMI packed pixel, conversion, integer/color or suitable DSP kernels
- [ ] Scratchpad hot state/tables/scanline buffers with transfer-cost accounting
- [ ] GS final presentation, scaling, overlays and suitable filtering
- [ ] GIF packet organization and lifetime contracts
- [ ] Host DMA framebuffer/audio/bulk transfers with coherency and stall measurements
- [ ] VIF feasibility for appropriate vector workloads
- [ ] VU0 candidate vector kernels and synchronization-cost review
- [ ] VU1 candidate vector kernels and synchronization-cost review
- [ ] SPU2 final audio buffering and underrun measurement
- [x] Pin stable RFAuds2 `0.1.0` revision `257ca54afb08b9c8c2197c03d96fd32d5b15b2bc`
- [ ] [IN PROGRESS] PS2-only audio adapter for the common PCM contract — fixed-storage lifecycle diagnostic is connected; target execution and continuous delivery remain
- [ ] [IN PROGRESS] RFAuds2 asynchronous tail/backpressure handling — launch/poll and ordered retry are host verified; target validation remains
- [ ] RFAuds2 FAT/Slim PS2 and long-duration validation
- [ ] IOP platform services, storage and peripherals
- [ ] Real-hardware timer calibration and subsystem profiling
- [ ] End-to-end regression and sustained frame-time measurements

Do not force arbitrary SNES PPU operations onto GS or pretend SPU2 replaces SNES
DSP. VUs are vector processors, not generic replacement CPUs. Timing-critical SNES
emulation stays off IOP by default; any offload must justify communication cost.
SNES DMA/HDMA remains a separate emulated subsystem from PS2 host DMA.

Successful optimizations: none. Rejected optimizations: none measured. Record both
in [optimization log](optimization.md) and [PERFORMANCE.md](../PERFORMANCE.md).
PCSX2 may aid development but cannot establish real PS2 full-speed operation.
