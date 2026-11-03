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

After the first fix, the full ROM passed `$0027` and reached `$0103`, `COP #$DB`,
with `E=1`, `S=$0100` and input P logically `$3B` after E-mode invariants. The WDC
W65C816S datasheet section 2.8 specifies bit 4 set in the status stacked by an
emulation-mode interrupt; Tables 5-2/5-3 and Table 5-7 entry 22j establish the COP
vector, PCH/PCL/P order and seven-cycle emulation entry. The aggregate and resumable
paths had incorrectly reserved the set bit for BRK and cleared it for COP.

The generic correction classifies both BRK and COP as software interrupts when
forming the emulation-mode stacked status; hardware IRQ/NMI/ABORT still clear bit 4.
Exact aggregate and resumable regressions start at `$7E:8000` with `S=$0100`, read
`COP #$DB`, write `$80` to `$0100`, `$02` to `$01FF` and `$3B` to `$01FE`, fetch
`$FFF4/$FFF5`, finish with `S=$01FD`, live P `$37` and seven cycles. No ROM identity
affects the implementation.

With that correction the full ROM passed `$0103` and reached `$03C4`, `PEI ($FF)`,
with E=1, DL=0 and the Direct pointer low byte at `$02FF`. The existing generic
Direct-word helper wrapped the high-byte read within the `$02xx` page to `$0200`.
WDC section 7.2 explicitly makes PEI an exception: its high-byte read advances
linearly to `$0300`. This is separate from section 7.1, under which PEI's two stack
writes already advance linearly from `$0100` to `$00FF` before restoring `SH=01`.

Aggregate and resumable PEI paths now share the instruction-specific pointer-next
rule. Exact regressions place a conflicting sentinel at `$0200`, require reads from
`$02FF/$0300`, writes to `$0100/$00FF`, preserved registers/status and six cycles.
The full ROM then reached `$03D9`, `PLB`, with E=1 and S=`$01FF`. WDC documents the
generic increment-before-read operation but omits this exception from caveat 7.1.
The pinned test README and Avery Lee's Altirra Hardware Reference independently
record that 65C816-added stack operations including PLB may temporarily read above
page one before `SH=01` is restored. PLB therefore reads `$0200`, not `$0100`, and
finishes with S=`$0100`.

Aggregate and resumable PLB paths now form the read address from linear S+1 and
reapply E-mode invariants only after the transfer. Exact regressions place a
conflicting sentinel at `$0100`, require the `$0200` read, DBR=`$3D`, preserved
A/D/P, normalized X/Y and four cycles. Ordinary PLA/PLX/PLY/PLP pulls retain their
page-one wrapping. With this correction the full ROM reports `Success` at `$0649`
and completes all 1,610 cases. No ROM name, hash or test number affects core
behavior.

## Verification and limits

The following local gates passed on 2026-11-03:

- `build/plb-analyzer/test-cpu-bus` and `build/plb-analyzer/test-cpu-cycles` after a
  `-fanalyzer` build of those tests and `mipsnes2-rom-runner`;
- `make clean && make test` with the default GCC toolchain;
- `make BUILD_DIR=build/plb-conformance licensed-rom-test`, completing 1,107/1,107
  basic and 1,610/1,610 full cases;
- `ASAN_OPTIONS=detect_leaks=0 make BUILD_DIR=build/plb-sanitize clean test
  CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
  LDFLAGS='-fsanitize=address,undefined'`;
- `python3 tools/check_repository.py` and `git diff --check`.

The managed filesystem created sanitizer `test-machine-ppu-frame` and
`test-ps2-rom-file` without their user execute bit. After setting that bit on those
ignored build artifacts, the identical suite passed with no ASan/UBSan finding;
this was not a source or test failure. Leak inspection remained disabled because
the managed environment denies the required `/proc` access. No local PS2SDK or PSP
toolchain was available, so the pinned PS2/PSP Actions cross-builds are required
publication evidence rather than being inferred from workflow files.

Published implementation `a8bb91e` passed the inspected
[host](https://github.com/ReyFxck/MIPSNES2/actions/runs/41649717078),
[PS2](https://github.com/ReyFxck/MIPSNES2/actions/runs/41649717556) and
[PSP](https://github.com/ReyFxck/MIPSNES2/actions/runs/41649717624) workflows.
The host matrix passed release and sanitizer tests under GCC and Clang; the GCC
licensed-ROM step recorded prepared SHA-256
`793a4425569649314b200d6bcdc21ea8e69684f9b99ad0a41ba6a4a526d28c3b`,
`Success` at `$0452` and 1,107 completed tests. Its two artifacts are nonempty and
the GCC checksum log includes both `mipsnes2-host` and `mipsnes2-rom-runner`. The
console job logs show the changed core cross-compiled by the pinned PS2/PSP
toolchains, checksum-verified staging and nonempty artifact uploads.

Published COP correction `4f42ed0` passed the inspected
[host](https://github.com/ReyFxck/MIPSNES2/actions/runs/41705022870),
[PS2](https://github.com/ReyFxck/MIPSNES2/actions/runs/41705022710) and
[PSP](https://github.com/ReyFxck/MIPSNES2/actions/runs/41705022800) workflows. Host
GCC and Clang again passed release and sanitizer suites; the GCC licensed-ROM step
completed 1,107/1,107 basic cases and checksum-verified both host executables. PS2
compiled the changed CPU into a checksum-verified `mipsnes2-ps2.elf` and uploaded a
952,101-byte artifact. PSP compiled the same core into a checksum-verified
`EBOOT.PBP` and uploaded a 167,596-byte artifact. This does not add PS2/PSP runtime
evidence.

Published PEI correction `2605abe` passed the inspected
[host](https://github.com/ReyFxck/MIPSNES2/actions/runs/41707926087),
[PS2](https://github.com/ReyFxck/MIPSNES2/actions/runs/41707926242) and
[PSP](https://github.com/ReyFxck/MIPSNES2/actions/runs/41707925706) workflows. Host
GCC and Clang passed release and sanitizer suites; the GCC licensed-ROM step again
completed 1,107/1,107 basic cases and checksum-verified both host executables. PS2
and PSP compiled the changed CPU, checksum-verified `mipsnes2-ps2.elf` and
`EBOOT.PBP`, and uploaded 952,452-byte and 167,643-byte artifacts respectively.
This is cross-build and host-conformance evidence, not target execution.

This is host test infrastructure, not a desktop emulator frontend. No PS2, PSP,
PCSX2/NetherSX2 or PPSSPP runtime was performed by this runner block. No target
performance or commercial-ROM compatibility claim is made.
