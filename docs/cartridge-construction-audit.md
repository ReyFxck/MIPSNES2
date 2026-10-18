# Cartridge preparation and construction audit

Date: 2026-10-18

## Scope

This milestone connects the existing image-analysis and executable-mapper layers
without adding filesystem code to the shared core. A platform or tool first loads a
complete file into caller-owned memory. The core then:

1. creates a clean/copier-prefixed image view;
2. performs conservative LoROM/HiROM/ExHiROM selection;
3. reports the exact header-declared SRAM requirement;
4. constructs a cartridge over caller-owned ROM and SRAM storage.

The source and SRAM buffers remain owned by the caller and must outlive the plan and
cartridge. No allocation, copy, filename, path, file handle or platform API enters
`core/`. PS2, PSP and host file/storage adapters therefore remain independent.

## API contract

`mipsnes2_cartridge_plan_create` returns the existing detailed image-analysis result
and changes its output only on success. The plan retains the image view, all three
candidate diagnostics, selected mapper and required SRAM byte count.

`mipsnes2_cartridge_construct` accepts an SRAM capacity at least as large as the
declared requirement. Extra capacity is not exposed to the SNES cartridge. Missing or
undersized required SRAM is rejected, a tampered/inconsistent plan is rejected, and
the destination cartridge remains unchanged on failure. Successful construction
still passes through the executable mapper's independent size/alignment validation.

This is a trust and lifetime contract for internal callers, not a serialized or
untrusted plan format. Platforms must not persist or mutate a plan.

## Generated verification fixtures

`tests/test_cartridge_prepare.c` uses only generated arrays and covers:

- null arguments, malformed image size and no-confident-header propagation;
- unchanged plan output after analysis failure;
- clean and required-512-byte-prefix LoROM sources;
- zero-SRAM construction and exact source/payload pointer retention;
- header-declared 8 KiB SRAM with missing, undersized and oversized capacities;
- unchanged cartridge output after construction failure;
- successful SRAM access through the executable LoROM mapper;
- selected 6 MiB ExHiROM construction and both physical ROM regions;
- invalid/tampered plan rejection.

The existing image, mapper and SNES-bus regression executables remain authoritative
for their narrower behavior. This integration test does not make file I/O or ROM
execution available in any platform entry point.

## Verification record

The following local checks passed with GCC 13.3.0 on 2026-10-18:

- `make clean && make test`;
- `make BUILD_DIR=build/release-local CC=gcc CFLAGS='-O2 -g -DNDEBUG' clean test`;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize-local CC=gcc CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' clean test`;
- `python3 tools/check_repository.py`;
- `git diff --check`.

Leak detection was disabled only for the local sanitizer runtime environment; ASan
and UBSan remained enabled. Local Clang, PS2SDK and PSPSDK were unavailable. GitHub
Actions passed for implementation commit
`cd55c07af49841751e67c750d1dceb9c70a8393f`:

- Host CI run `39165483492`: GCC and Clang release tests, ASan+UBSan tests,
  development artifacts and checksum validation succeeded;
- PS2 CI run `39165483445`: pinned toolchain installation, shared-core/scaffold
  cross-build, artifact upload and SHA-256 validation succeeded;
- PSP CI run `39165483460`: pinned toolchain installation, shared-core/scaffold
  cross-build, artifact upload and SHA-256 validation succeeded.

These cross-builds are not runtime tests. Real PS2, real PSP, PCSX2 and PPSSPP
runtime validation are not part of this interface-only milestone.

## Remaining work

- Platform-specific file acquisition, ownership and error reporting are absent.
- SRAM initialization policy and persistent save semantics are absent.
- The host executable still rejects ROM arguments because there is no runnable SNES
  machine.
- Special cartridge boards and enhancement hardware need separate construction data.
- No performance claim is made; mapper fast paths still require target profiling.
