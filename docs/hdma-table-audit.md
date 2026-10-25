# HDMA table and scanline engine audit

Date: 2026-10-25

## Scope

The shared core now owns the logical HDMA table state independently of PS2 and
PSP backends. `$420C` writes store the eight-channel enable mask in the SNES bus.
The new pure engine initializes enabled channels at a frame boundary and applies
one caller-requested visible scanline in channel order.

Frame initialization copies A1T (`$43x2/3`) to A2A (`$43x8/9`), reads the first
line descriptor and loads DAS (`$43x5/6`) for indirect channels. Scanline
processing supports all eight transfer-mode port sequences, direct and indirect
tables, both transfer directions, 16-bit pointer wrapping, repeat/wait line
counters, enable-bit pause/resume, manual mid-frame setup and zero-descriptor
termination. A terminated channel cannot restart until the next frame begin.

The behavior follows the published register descriptions for `$420C` and
`$43x0-$43xA` in
[Anomie's Register Doc](https://raw.githubusercontent.com/gilligan/snesdev/master/docs/snes_registers.txt)
and the independently described table algorithm in the
[Super Famicom Development Wiki](https://wiki.superfamicom.org/dma-and-hdma).
No source code from another emulator was consulted, copied or indexed.

## Separation and timing boundary

`core/dma/hdma.c` is an untimed state machine. Its callbacks describe table
reads and A/B data phases; they do not advance the CPU, scheduler or H/V domain.
No backend owns SNES HDMA policy. The next scheduler block must request frame
initialization near V=0/H=6, request transfers near dot 278 on visible lines,
pause the CPU at an operation boundary, give HDMA priority over general DMA and
charge the documented overhead/read/data phases through the existing refresh
arbiter.

At this foundation revision, the less common zero-descriptor/last-active-channel
indirect-address shortening was not implemented. It was subsequently added to
the shared cursor; see the
[HDMA zero-descriptor audit](hdma-zero-descriptor-audit.md). Register mutation
or reentry from callbacks is outside this API.

## Deterministic verification

- [x] Frame begin copies A1T to A2A and loads direct/indirect descriptors in
  channel priority order.
- [x] Direct repeat and wait entries update A2A and NLTR over successive lines.
- [x] Indirect data uses DASB:DAS, advances DAS and reloads its pointer.
- [x] All transfer modes use their 1/2/4-byte B-port sequence, including `$21FF`
  wrap to `$2100`.
- [x] Direct and indirect A offsets wrap within their fixed bank.
- [x] Clearing/re-enabling `$420C` pauses and resumes state; a manually prepared
  mid-frame channel starts at the next requested scanline.
- [x] `$80` line-counter decrement behavior and zero-descriptor termination are
  covered.
- [x] Power-on clears the bus enable mask.
- [x] The full deterministic host, optimized and ASan/UBSan suites pass.
- [x] GCC static analysis passes with `-fanalyzer`.

## Completion criterion

This foundation is complete when the pure engine and `$420C` routing pass the
host suite and PS2/PSP/host CI. The parent HDMA timing milestone remains open
until scheduler arbitration, horizon-resumable phase timing and general-DMA
overlap have deterministic coverage.

Published revision `dc56eb9` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151827396),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151827359) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151827425).
