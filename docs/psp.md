# PSP — independent secondary target

PSP shares useful SNES semantics, not blindly the PS2 low-level implementation.
Allegrex, cache, memory, MIPS, VFPU, GU and audio need independent profiling and
implementation choices. Host PC and PS2 measurements do not establish PSP gains.

## Current scaffold and build

`platform/psp/main.c` supplies SDK module metadata, a user main thread, shared logging
and `sceKernelExitGame`. It is a build probe, not an emulator/menu. Runtime lifecycle,
exit handling, GU, controller and audio integration remain future work.

Linux x86-64 toolchain pin: pspdev **v20261001**, exact URL/SHA-256 in
[tools/toolchains.json](../tools/toolchains.json). No floating latest release is used.

```sh
python3 tools/setup_toolchain.py psp /tmp/mipsnes2-sdks "$HOME/.cache/mipsnes2-toolchains"
export PSPDEV=/tmp/mipsnes2-sdks/pspdev
export PATH="$PSPDEV/bin:$PATH"
make -C platform/psp
```

The makefile obtains PSPSDK with `psp-config --pspsdk-path`, includes
`$(PSPSDK)/lib/build.mak`, enables `BUILD_PRX=1`, and packages `EXTRA_TARGETS=EBOOT.PBP`
with `PSP_EBOOT_TITLE`. Outputs include ELF, PRX and EBOOT.PBP. These are foundation
outputs; successful packaging does not mean emulator functionality or runtime safety.
Use a fresh install destination for SDK updates.

Upstream conventions inspected on 2026-10-09:

- [PRX makefile](https://github.com/pspdev/pspsdk/blob/e2b2b313d4175823a263c2c6cf175311e824eb77/src/samples/template/prx_template/Makefile.sample)
- [Module/thread declarations](https://github.com/pspdev/pspsdk/blob/e2b2b313d4175823a263c2c6cf175311e824eb77/src/user/pspmoduleinfo.h)
- [Exit API declaration](https://github.com/pspdev/pspsdk/blob/e2b2b313d4175823a263c2c6cf175311e824eb77/src/user/psploadexec.h)
- [PBP build rules](https://github.com/pspdev/pspsdk/blob/e2b2b313d4175823a263c2c6cf175311e824eb77/src/base/build.mak)
- [Pinned binary release](https://github.com/pspdev/pspdev/releases/tag/v20261001)

Source inspection revision and actual archive digest are deliberately separate.
Only SDK API/build conventions were consulted, not emulator implementations.

## Dedicated optimization candidates

No candidate below is implemented or measured.

- [ ] Allegrex dispatch, compact state and branch reduction
- [ ] Cache-aware data and instruction layout
- [ ] Memory budget and aligned buffer review
- [ ] Allegrex-specific MIPS for measured hotspots
- [ ] VFPU suitable pixel/conversion/audio/filter/vector workloads
- [ ] GU final framebuffer presentation, scaling, overlays and suitable filters
- [ ] Platform-specific video buffering and coherency
- [ ] PSP audio output, buffering and latency/underrun review
- [ ] Independent profiling and repeatable real-PSP benchmarks
- [ ] Reference/optimized regression comparisons

Do not force scalar 65C816 execution onto VFPU or couple core PPU to GU. Device audio
receives emulated samples; SNES DSP remains emulated. Explicit clock policy and
suspend/exit behavior need implementation and real-device tests later.

Successful optimizations: none. Rejected optimizations: none measured. Record both
in [optimization log](optimization.md) and [PERFORMANCE.md](../PERFORMANCE.md).
PPSSPP is useful for development, not authoritative for final performance.
