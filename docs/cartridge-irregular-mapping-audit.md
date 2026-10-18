# Executable Irregular Base-ROM Mapping Audit

Date: 2026-10-18

Scope: bounded non-power-of-two ROM mirroring for the existing base LoROM/HiROM
mapper. This is not ExLoROM/ExHiROM, enhancement-chip, file-loading or universal
multi-chip board support.

## Evidence and design boundary

The Super Famicom Development Wiki's non-power-of-two ROM-size note describes a
larger ROM region first and a smaller final region repeated through the next
power-of-two address span, including the conventional `ABCC` and `ABCDEEEE`
patterns. SnesLab's ROM-header documentation independently describes checksum
mirroring for common non-power-of-two sizes and links the rule to Nintendo's
development manual. Both were inspected on 2026-10-18. No emulator implementation
was consulted or copied.

MIPSNES2 therefore accepts an executable base ROM only when all of the following are
true:

- the payload is between one mapper bank and 4 MiB;
- its length is aligned to 32 KiB for LoROM or 64 KiB for HiROM;
- it is a power of two, or decomposes into one largest power-of-two region followed
  by one smaller power-of-two tail;
- it needs no arbitrary zero padding or board-specific address decoder.

Initialization precomputes the first-region size and next power-of-two mirror span.
Reads preserve the first region once, repeat the smaller tail across the second half
and mirror the resulting span through the supported base mapping windows. ROM stays
read-only. SRAM behavior is unchanged.

This policy deliberately rejects padding-dependent and multi-stage irregular shapes.
It does not claim that every physical cartridge board follows this layout.

## Deterministic fixtures

`tests/test_cartridge.c` uses generated arrays only and verifies:

- 96 KiB LoROM as a 64 KiB first region plus repeated 32 KiB tail;
- 160 KiB LoROM as a 128 KiB first region plus a 32 KiB tail repeated four times;
- 192 KiB HiROM as a 128 KiB first region plus repeated 64 KiB tail;
- mirror-span wrap back to the first region;
- ROM write protection through a mirrored tail address;
- rejection of a 224 KiB padding-dependent shape;
- rejection of 80 KiB LoROM and 96 KiB HiROM bank misalignment.

`tests/test_cartridge_image.c` verifies that a conservatively selected 96 KiB LoROM
image can initialize the executable mapper and produces the expected precomputed
layout. No commercial ROM or externally copied fixture is included.

## Verification record

Local verification used GCC 13.3.0 on 2026-10-18:

| Command | Result |
|---|---|
| `make clean && make test` | Passed all host suites and repository checks |
| `make BUILD_DIR=build/release clean && make BUILD_DIR=build/release CFLAGS='-O2 -g -DNDEBUG' test` | Passed all host suites with test checks active under `NDEBUG` |
| `make BUILD_DIR=build/sanitize clean && ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test` | Passed ASan + UBSan; local leak detection was not claimed |
| `python3 tools/check_repository.py` | Passed required-file, license, link, checkbox, boundary and workflow-pin checks |
| `git diff --check` | Passed |

The cartridge suites reported `PASS: base LoROM/HiROM power-of-two and irregular
mapping` and `PASS: cartridge image checksum and base-mapper selection`. Local Clang
was unavailable. Local PS2 and PSP cross-builds were unavailable because `PS2SDK`
was unset and `psp-config` was absent.

GitHub Actions passed for implementation commit
`2d9f03bb51dbb4248685e91fdfa61f85df28ffd3`:

| Workflow | Run | Verified result |
|---|---:|---|
| Host CI | [39140080472](https://github.com/ReyFxck/MIPSNES2/actions/runs/39140080472) | GCC and Clang release tests, ASan+UBSan tests and artifact upload passed |
| PS2 CI | [39140080489](https://github.com/ReyFxck/MIPSNES2/actions/runs/39140080489) | Pinned toolchain install, scaffold cross-build, artifact and SHA-256 staging/upload passed |
| PSP CI | [39140080624](https://github.com/ReyFxck/MIPSNES2/actions/runs/39140080624) | Pinned toolchain install, scaffold cross-build, artifact and SHA-256 staging/upload passed |

These are compilation and deterministic host-test results. No PCSX2, PPSSPP, real
PS2 or real PSP runtime test was performed.

## Remaining work

- file loading and automatic executable-cartridge construction;
- extended/variant header resolution;
- padded or multi-stage irregular shapes backed by stronger board evidence;
- ExLoROM and ExHiROM;
- enhancement-chip and board-specific decoding;
- real-game and real-console validation.

No compatibility, performance or real-hardware claim is made.
