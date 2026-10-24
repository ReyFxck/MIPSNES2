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
runs at most one NTSC H/V frame or 100,000 CPU sequences. It prints CPU and timing
status; it cannot draw SNES frames, produce audio or persist SRAM. It does not
mount a USB device or provide a file picker. The original five-case diagnostic
still runs without arguments; it is not a game or the final GS/video backend.
The new ROM path has no console runtime evidence. The default diagnostic has
user-captured emulator results, while physical PS2 validation remains open.
The original scaffold record is in
[bootstrap-audit.md](bootstrap-audit.md); see the
[ARMSX2 diagnostic guide](ps2-armsx2-diagnostic.md) for the mobile test.

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
No emulator implementation was copied. Scaffolding calls only standard C functions.

## Preferred future PCM backend: RFAuds2

[ReyFxck/RFAuds2](https://github.com/ReyFxck/RFAuds2) is the preferred future PS2
PCM transport, provided continued testing shows it is suitable. The decision was
recorded after inspecting RFAuds2 `0.1.0-alpha.1` at revision
`f82e0967104fc96f37259e1abc124341eef6a76c` on 2026-10-10, including its README,
API/architecture material and emulator-integration guidance.

RFAuds2 is pre-1.0/experimental. Its current repository reports deterministic host
regressions and PS2 cross-build coverage, plus audible testing in NetherSX2 inherited
from its earlier audio path. Physical FAT/Slim PS2 validation and long emulator stress
remain required. These facts make it a preferred candidate, not a completed MIPSNES2
integration or a production-readiness claim.

The required boundary is:

```text
SNES SPC700/S-DSP -> common emulated PCM contract -> PS2 adapter -> RFAuds2 -> IOP/SPU2
```

The emulated SPC700/S-DSP owns SNES audio timing. RFAuds2 only transports/resamples
final PCM for the PS2 device; its 48 kHz SPU2 boundary must never set the emulated
clock. PSP keeps a dedicated PSP audio backend, and host keeps offline/testing output.
S-DSP/sample generation must remain testable without RFAuds2 or physical SPU2.

Future integration must pin a reviewed RFAuds2 version or revision, define the
platform audio interface and buffer ownership, retain/retry unaccepted asynchronous
tails, and validate underrun/backpressure behavior on real PS2 hardware. Do not copy
RFAuds2 source into unrelated `core/` directories. No runtime dependency is added by
the current milestone.

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
- [ ] Pin a reviewed RFAuds2 version/revision
- [ ] PS2-only audio adapter for the common PCM contract
- [ ] RFAuds2 asynchronous tail/backpressure handling
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
