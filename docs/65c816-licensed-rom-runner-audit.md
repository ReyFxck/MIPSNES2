# Pinned 65C816 Test-ROM Runner and First Failure Audit

Date: 2026-11-03

## Scope

This block adds a host-development-only, bounded ROM runner over the same
`mipsnes2_machine` used by the console backends. It also adds a reproducible fetch
and provenance check for gilyon's MIT-licensed `snes-tests` release `v1.4`, exact
revision `5ecdf555da920f0bd7b157542141965a8120186d`.

The generic runner accepts a file, a frame limit and optional WRAM/VRAM inspection
ranges. It has no test-ROM title, CRC or hash branches and does not interpret a
game. A separate test adapter uses the suite's documented WRAM test number and its
tilemap text to report `Success`, `Failed` or an incomplete run. No upstream ROM,
source, generated assembly or test-description file is committed.

## Asset provenance and preparation

The fetcher validates all content before use:

- release archive `snes-tests.zip`:
  `c3a39e15a9f3789fe143e54dfa035c988204576eafacf97df8ff3546a917c5e9`;
- upstream `cputest-basic.sfc`:
  `d479bde706b9e16d76c0dfe98e2b2d3f5348f9dcb911e6a36b898f4201fc98bb`;
- upstream `cputest-full.sfc`:
  `f61de78d346a68c166d67472a414e5d207368889d37b977d1338f9221ca6400e`;
- generated basic/full descriptions:
  `cc5fca504344d771f7b7a5e65c51e4485dbcbaba378edb88df381d6c9819e289`
  and `abbd5b5afdf3a669cf16859bd2e509938e1eff1a1ac4c0deef7217c0ee35f60b`;
- upstream MIT license:
  `8d22899b4e22435aefc1865666f889402750a1fffdf8957ce87f03f2d6d40fec`.

The released ROMs intentionally contain a `$0000/$FFFF` checksum placeholder.
MIPSNES2's conservative mapper selector requires the calculated pair, so the
fetcher changes only those four header bytes in its ignored build output. Prepared
basic/full hashes are respectively
`793a4425569649314b200d6bcdc21ea8e69684f9b99ad0a41ba6a4a526d28c3b`
and
`b5d0559118df83c1138550d46154eaa0a810013b4adaaa2ce52374759dca6e98`.
Executable test bytes are unchanged, and the generated `PROVENANCE-*.json` records
the transformation.

## Independent result and correction

The basic ROM completed all 1,107 cases through the shared machine and displayed
`Success`, ending at test `$0452` after the bounded 140-frame run.

The full ROM first stopped deterministically at test `$0027`, `ADC ($F7,X)`, with
`E=1`, `D=$011A`, `X=$00EE`. Its independently generated fixture requires the low
pointer byte from `$02FF` and the high byte from `$0200`: with a nonzero Direct Page
low byte, the initial `D + operand + X` calculation is linear, but the high-byte
increment wraps in the selected page. MIPSNES2 incorrectly fetched the high byte
from `$0300`.

The correction is an addressing-mode rule, not a ROM exception. Aggregate and
resumable `(direct,X)` paths now share the E-mode high-byte wrap while other regular
Direct-indirect modes retain their existing DL-dependent behavior and long pointers
remain linear. Synthetic tests cover the exact `$02FF -> $0200` access order,
result, cycle count and both CPU entry points.

After the fix, the full ROM passes `$0027` and reaches the next deterministic
failure at `$0103`, `COP #$DB`, an emulation-mode stack/vector case. That remaining
failure defines the next CPU-conformance frontier; the full suite is not claimed to
pass.

## Verification and limits

The following local gates passed on 2026-11-03:

- `build/analyzer/test-cpu-bus` and `build/analyzer/test-cpu-cycles` after a
  `-fanalyzer` build of those tests and `mipsnes2-rom-runner`;
- `make clean && make test` with the default GCC toolchain;
- `make licensed-rom-test`, completing 1,107/1,107 basic cases;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/sanitize clean test
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined'`;
- `python3 tools/check_repository.py` and `git diff --check`.

The managed filesystem initially created only `test-ps2-pad-input` in the sanitizer
build without its user execute bit. After setting that bit on the ignored build
artifact, the identical suite passed with no ASan/UBSan finding; this was not a
source or test failure. Leak inspection remained disabled because the managed
environment denies the required `/proc` access. No local PS2SDK or PSP toolchain was
available, so the pinned PS2/PSP Actions cross-builds are required publication
evidence rather than being inferred from workflow files.

This is host test infrastructure, not a desktop emulator frontend. No PS2, PSP,
PCSX2/NetherSX2 or PPSSPP runtime was performed by this runner block. No target
performance or commercial-ROM compatibility claim is made.
