# PlayStation 2 — primary target

The EE initially performs SNES CPU, bus, scheduler, PPU, DMA/HDMA, SPC700, DSP and
cartridge work. This is not a generic MIPS port. PS2 receives architectural priority
when measured gains preserve correctness and isolation.

## Current scaffold and build

`platform/ps2/main.c` is a small stdout build probe using the shared logger. It prints
an honest foundation banner and returns. It does not initialize GS, input, IOP
services or audio. Console runtime remains unverified even when cross-compilation
succeeds. The exact verification record is in [bootstrap-audit.md](bootstrap-audit.md).

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
