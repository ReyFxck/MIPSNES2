Warning: truncated output (original token count: 31512)
Total output lines: 2291

# STATUS — canonical project state

Project foundation plus a 256-opcode 65C816, partial real SNES logical bus, base
LoROM/HiROM and Mode 25/35 ExHiROM mapping, explicit internal-header analysis and deterministic scheduler/
timing foundation with a partial non-interlaced H/V model and a platform-neutral
machine instance and an explicit-path PS2 ROM-file adapter. No runnable
SNES machine capable of running games, complete emulated subsystem,
console runtime validation,
compatibility result or hardware benchmark exists. Read
[bootstrap audit](docs/bootstrap-audit.md) and the CPU/bus evidence in
[docs/65c816-foundation-audit.md](docs/65c816-foundation-audit.md)
and [docs/snes-bus-timing-audit.md](docs/snes-bus-timing-audit.md), plus the
[base-cartridge audit](docs/cartridge-foundation-audit.md) and
[header-analysis audit](docs/cartridge-header-analysis-audit.md). Checksum/selection
and executable irregular-map evidence are recorded in
[docs/cartridge-mapper-selection-audit.md](docs/cartridge-mapper-selection-audit.md)
and [docs/cartridge-irregular-mapping-audit.md](docs/cartridge-irregular-mapping-audit.md).
Mode 25/35 evidence is in
[docs/cartridge-exhirom-audit.md](docs/cartridge-exhirom-audit.md). CPU-I/O and H/V
timing evidence is in
[docs/cpu-io-foundation-audit.md](docs/cpu-io-foundation-audit.md) and
[docs/hv-timing-foundation-audit.md](docs/hv-timing-foundation-audit.md). Stable-
register H/V timer-match evidence is in
[docs/hv-irq-foundation-audit.md](docs/hv-irq-foundation-audit.md), and shared
scheduler/H/V advancement evidence is in
[docs/master-timing-fanout-audit.md](docs/master-timing-fanout-audit.md), and atomic
CPU-I/O interrupt handoff evidence is in
[docs/interrupt-handoff-audit.md](docs/interrupt-handoff-audit.md). Ricoh 5A22 math
and controller evidence is in
[docs/5a22-math-register-audit.md](docs/5a22-math-register-audit.md) and
[docs/auto-joypad-audit.md](docs/auto-joypad-audit.md), with manual serial-port
evidence in [docs/manual-joypad-audit.md](docs/manual-joypad-audit.md).
Programmable I/O-port evidence is in
[docs/5a22-io-port-audit.md](docs/5a22-io-port-audit.md).
The counter-register foundation is scoped in
[docs/hv-counter-latch-audit.md](docs/hv-counter-latch-audit.md).
The rejected in-callback access-clock experiment is documented in
[docs/cpu-access-timing-postmortem.md](docs/cpu-access-timing-postmortem.md).
Boundary-visible 65C816 ABORT evidence is recorded in
[docs/65c816-abort-foundation-audit.md](docs/65c816-abort-foundation-audit.md).
The initial caller-owned CPU operation cursor is scoped in
[docs/65c816-cycle-contract-audit.md](docs/65c816-cycle-contract-audit.md), and the
interrupt-entry extension is recorded in
[docs/65c816-interrupt-cycle-audit.md](docs/65c816-interrupt-cycle-audit.md), and the
first resumable call/return pair is recorded in
[docs/65c816-subroutine-cycle-audit.md](docs/65c816-subroutine-cycle-audit.md), the
long pair in
[docs/65c816-long-subroutine-cycle-audit.md](docs/65c816-long-subroutine-cycle-audit.md),
and the indexed-indirect call in
[docs/65c816-indexed-subroutine-cycle-audit.md](docs/65c816-indexed-subroutine-cycle-audit.md).
The complete jump-family migration is recorded in
[docs/65c816-jump-cycle-audit.md](docs/65c816-jump-cycle-audit.md).
The complete resumable branch-family migration is recorded in
[docs/65c816-resumable-branch-cycle-audit.md](docs/65c816-resumable-branch-cycle-audit.md).
The resumable stack-family migration is recorded in
[docs/65c816-stack-cycle-audit.md](docs/65c816-stack-cycle-audit.md).
The resumable implied-family migration is recorded in
[docs/65c816-implied-cycle-audit.md](docs/65c816-implied-cycle-audit.md).
The resumable shift/rotate and memory RMW migration is recorded in
[docs/65c816-rmw-cycle-audit.md](docs/65c816-rmw-cycle-audit.md).
The complete resumable load/store-family migration is recorded in
[docs/65c816-load-store-cycle-audit.md](docs/65c816-load-store-cycle-audit.md).
Completion of the 256-opcode resumable cursor is recorded in
[docs/65c816-final-resumable-cycle-audit.md](docs/65c816-final-resumable-cycle-audit.md).
The resumable RESET-vector extension is recorded in
[docs/65c816-reset-cycle-audit.md](docs/65c816-reset-cycle-audit.md).
The scheduler-connected operation-timing bridge is recorded in
[docs/cpu-operation-timing-audit.md](docs/cpu-operation-timing-audit.md).
The bounded mid-instruction ABORT read-only slice is recorded in
[docs/65c816-mid-instruction-abort-slice-audit.md](docs/65c816-mid-instruction-abort-slice-audit.md).
The opt-in ABORT write-qualification and recovery mechanism is recorded in
[docs/65c816-abort-write-qualification-audit.md](docs/65c816-abort-write-qualification-audit.md).

The PS2 entry now shows a five-case, on-screen core diagnostic: original embedded
CPU instruction, timed horizon and ABORT write fixtures run through the shared
core, with stdout duplicates. Its host baseline and pinned PS2SDK cross-build
pass. User-supplied ARMSX2 and NetherSX2 Turnip screenshots show the v1 screen
and `5 PASS / 0 FAIL` in both emulators. ARMSX2 version/settings and on-device
ELF checksum are not yet recorded; no real-PS2 execution is claimed. The test
steps, screenshot hashes, upstream test-ROM provenance and limits are in
[docs/ps2-armsx2-diagnostic.md](docs/ps2-armsx2-diagnostic.md).

The new `core/machine` composes a caller-owned cartridge image, WRAM and SRAM
with CPU, bus, CPU-I/O, counter registers, H/V, interrupt lines and scheduler.
Its timed RESET and resumable instruction path executes a synthetic LoROM
program that writes WRAM, including a paused write and a power cycle retaining
WRAM/SRAM. `make test` passes on the host (2026-10-24); see
[machine integration audit](docs/machine-integration-audit.md). Pinned PS2 and PSP
cross-builds and host CI pass at `2d2a836`; target runtime behavior of that
revision remains unverified. At that revision there was no PPU
reference renderer, APU, DMA or game runtime.

The PS2 backend now accepts `--rom PATH` as an explicit file path on an already
available target filesystem, reads at most 8 MiB plus an optional copier prefix,
and feeds the bytes to the shared machine. It runs only a bounded NTSC CPU/H-V
slice and reports its result on the debug screen. The five-case diagnostic stays
the default when no path is passed. Host file-boundary and bounded-run fixtures
pass; PS2/PSP cross-builds and host CI also pass at `8261563`. Target runtime
and mounted-device/launcher arguments have not been checked.
See [PS2 ROM adapter audit](docs/ps2-rom-adapter-audit.md).

An optional caller-owned 64 KiB VRAM buffer now connects `$2115-$2119` write
ports to the shared machine and PS2 ROM path. Increment modes, address wrapping
and 32/64/128-word remapping pass deterministic host tests. Host CI and PS2/PSP
cross-builds pass at `f816a73`. Buffered `$2139/$213A` reads now return the
previous word and fetch before the selected increment, with a dummy first read
after VMADD changes; host fixtures cover both increment modes, remapping,
wrapping and machine bus routing. Active-display access and mixed read/write
behavior remain open. Host CI and PS2/PSP cross-builds pass at `de252cc`;
see the [read audit](docs/ppu-vram-read-audit.md) and
[write audit](docs/ppu-vram-write-audit.md). No picture is produced.

The shared CGRAM port subset supplies `$2121/$2122/$213B` over 512 caller-owned
bytes on the PS2 ROM path, with paired 15-bit writes, low/high reads, address
wrap and PPU2 open-bus bit 7. `make test` passes with deterministic port and
machine bus fixtures (2026-10-24, host C99 `cc`). PSP compiles the same core,
but has no ROM runtime. Active-display access, mixed read/write byte selection
and real hardware behavior remain unverified; see the
[CGRAM port audit](docs/ppu-cgram-port-audit.md). Implementation revision
`37b9ab6` passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052572567),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052572654) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40052572626).

The shared OAM port subset supplies `$2102-$2104/$2138` over 544
caller-owned bytes in the PS2 ROM path. Host `make test` passes on C99 `cc`
with table-boundary, mirror, pair-commit, address-reload and bus-routing
fixtures (2026-10-24). Revision `28ed5c1` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40076823004),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40076823013) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40076822995).
The PSP build includes the same core module. Active-display access,
sprite priority and rendering remain open. See the
[OAM port audit](docs/ppu-oam-port-audit.md).

The machine now schedules an OAM pointer reload at horizontal dot 10 of the
VBlank start line when forced blank is off. `$2100` bit 7 is tracked for this
purpose, and a recurring core scheduler event handles NTSC's alternating
short line and PAL's longer frame. Host fixtures cover the event boundary,
event-budget refusal, forced blank and PAL overscan. The exact hardware
reload clock, forced-blank release behavior, active-display invalidation and
sprite rotation effects remain unverified; see the OAM audit. Revision
`af3ff1e` passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40080224129),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40080224195) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40080224329).

The OAM address now also reloads when `$2100` bit 7 falls from 1 to 0 on
the first VBlank scanline. A clean host `make test` passes with mirrored-bank
write, repeated-write and next-line controls (2026-10-24, C99 `cc`).
Independent research differs on whether this transition can reload outside
the first VBlank line; the narrower behavior is implemented and the broader
claim remains unverified. Revision `fdffb76` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40084717479),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40084717451) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40084717441).

The shared OAM state now retains `$2101` OBJSEL and decodes any of the 128
physical sprite records without advancing the CPU port pointer. The descriptor
exposes raw nine-bit X, Y, nine-bit character, palette, BG priority, flip
flags and the large-size bit. A separate size lookup covers the six pairs
documented for `$2101` modes 0-5; modes 6-7 return unsupported. Host tests
cover low/high table packing including sprite 127, field extraction, pointer
non-mutation, all documented size pairs, invalid inputs and mirrored machine
routing. A clean C99 `cc` `make test` passes (2026-10-24). Revision
`e65cf63` passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40085196446),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40085196496) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40085196410).
This is descriptor decoding only: sprite
scanline eligibility, priority rotation effects, tile fetch and rendering
remain open. See the [OAM port audit](docs/ppu-oam-port-audit.md).

The shared OBJ tile-address helper now resolves an unflipped sprite-local
8×8 tile to its first VRAM word using `$2101` base/name selection. Row and
column wrap independently within the selected 16×16 character grid, and
the 32K-word VRAM address wraps. Deterministic host fixtures exercise
character `$1FE` across row/column edges, second-table and base selection,
VRAM wrap, sprite-size bounds and invalid inputs. A clean host C99 `cc`
`make test` passes (2026-10-24). Revision `920ae0c` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40085599497),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40085599465) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40085599449).
This helper does not fetch VRAM bytes or decide tile order after flips,
scanline limits, palette composition or rendering.

A separate platform-neutral 4bpp decoder now converts one 8×8 VRAM tile
to 64 row-major color indices. It is shared by future OBJ and BG paths,
and wired into both PS2 and PSP build graphs. Deterministic host fixtures
cover zero/full colors, each bitplane at distinct pixels, a mixed pixel,
VRAM-end wrap, invalid buffers and the OBJ tile-address handoff. A clean
host C99 `cc` `make test` passes (2026-10-24). Revision `6b23966`
passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40086023302),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40086023320) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40086023326).
The first clean run stopped on an unrelated zero-byte generated
`test-interrupt-lines` executable; rebuilding it and rerunning the full
suite passed. The cause of that local artifact failure is unconfirmed.
No palette lookup, transparency decision, flipping, scanline rendering or
target runtime is claimed. See the [4bpp tile audit](docs/ppu-tile4-audit.md).

The core now selects a bounded OBJ scanline range (32 sprites) and tile-fetch
list (34 slivers), in priority order and reverse fetch order respectively.
It handles 9-bit X clipping including X=`$100`, 8-bit Y wrap, square size
modes 0-5, flips and an explicit first-sprite index. Host fixtures cover
exact and overflow boundaries, list order, X/Y edges and invalid inputs.
A clean C99 `cc` `make test` passes (2026-10-24). Revision `25f0ae9`
passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40088629267),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40088629246) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40088629261).
The pure helper still accepts caller-supplied sprite-addressed Y. The machine
now schedules OBJ selection at HBlank start for the next visible line, retains
the bounded line and accumulates `$213E` range/time-over bits until the next
frame. Host fixtures cover exact event boundaries, disabled event budget,
both overflow bits, read-only status, PPU1 open-bus bit 5, forced-blank
retention and PAL frame clear. C99 `cc` `make test` passes (2026-10-24).
Revision `49904ce` passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40100944834),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40100944700) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40100944805).
This is a phase
approximation: exact intra-line assertion, forced-blank
behavior, active-display writes, screen output and hardware runtime remain open.
See the [OBJ scanline audit](docs/ppu-sprite-scanline-audit.md).

OBJ priority rotation now derives the first candidate from the current
internal OAM word address when `$2103` bit 7 is set. Host vectors cover
pointer advancement by reads, high-table wrap, VBlank reload and a rotated
33-sprite machine scanline; C99 `cc` `make test` passes (2026-10-24).
The exact active-display address-invalidation interaction and hardware
runtime remain unverified. Revision `f8155de` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101264633),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101264526) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101264612).

The shared OBJ compositor now reads selected 4bpp slivers from caller-owned
VRAM into a bounded 256-pixel line. It emits OBJ palette indices and priority,
uses color zero as transparent and applies sprite flip, clipping and
first-sprite precedence. Machine HBlank events populate this line when VRAM
is present. Synthetic host vectors cover overlap with a transparent top
pixel, palette/priority metadata, negative-X clipping, both flips, invalid
inputs and the machine handoff. C99 `cc` `make test` passes (2026-10-24).
Revision `b2de8a9` passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101533032),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101533044) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101533121).
There is no BG/OBJ
priority mixing, screen output, mid-line VRAM fetch timing or console-runtime
validation. See the [OBJ scanline audit](docs/ppu-sprite-scanline-audit.md).

The new pure BG 2bpp scanline helper reads 8x8/16x16 tilemaps from
caller-owned VRAM into a 256-pixel color/priority/opacity line. It accepts
explicit base/scroll/configuration values and covers the four 32x32 screen
layouts, flips, palettes, transparency and coordinate wrap. Synthetic host
fixtures pass with C99 `cc` (2026-10-24). It is not wired to `$2105-$2114`,
does not model display-time register changes, and has no BG/OBJ mixing or
output. Revision `5b9cfc6` passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101892040),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101892187) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40101892125). See the
[BG 2bpp audit](docs/ppu-bg2-scanline-audit.md).

At revision `b1fd6ef` the machine acquired partial BG register state for
`$2105`, `$2107-$210C` and `$210D-$2114`. Mode 0 BG1 rendered a 2bpp line
from caller-owned VRAM at the same provisional HBlank boundary as OBJ; the
other BG register sets then produced pure configurations without scheduled
pixels. Host
fixtures cover register base/size decode, shared scroll-write latches, mode
rejection and the machine event boundary. C99 `cc` `make test` passes
(2026-10-24). Revision `b1fd6ef` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102344749),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102344753) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102344720).
Scroll latch behavior follows a published hardware-research hypothesis and
needs real-console validation. Mosaic, BG enable/window selection, BG/OBJ
mixing, mid-line register effects and screen output remain open. See the
[BG register audit](docs/ppu-bg-register-audit.md).

The shared Mode 0 mixer now selects among all four BG lines, OBJ and backdrop
using the documented layer order, with `$212C` main-screen enables. The machine
prepares enabled BG1-4 lines and a 256-entry CGRAM-index/source line at the
provisional scanline event. Synthetic tests cover all four BG layers, OBJ
priority bands, transparent fall-through, disabled layers and machine BG/OBJ
priority inversion; C99 `cc` `make test` passes (2026-10-24). Revision
`91170f3` passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102722188),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102722237)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40102722289).

The same shared Mode 0 path now resolves CGRAM indices to a bounded BGR555
line from caller-owned palette bytes. `$2100` supplies brightness 0-15; each
5-bit channel is scaled by brightness/15 with downward integer rounding.
Backdrop uses CGRAM index zero, and forced blank leaves the stored color line
black at the scheduled line event. Pure fixtures cover color packing, high
bit masking, last palette entry, backdrop, brightness 0/8/15, forced blank,
invalid inputs and unchanged output on failure. Machine fixtures cover
BG/OBJ palette selection and brightness writes. Host C99 `cc` `make test`
passes (2026-10-24). Revision `ac3124c` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103159833),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103159758)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103159561).
This digital brightness model is not an analog DAC measurement. At that
revision, windows, sub-screen, color math, mid-line writes, exact fetch
timing, target runtime and video output remained open. See the
[Mode 0 mixer audit](docs/ppu-mode0-mix-audit.md).

Main-screen layer windows now retain `$2123-$212B/$212E` in the shared
machine. A pure 256-pixel mask applies inclusive edges, inversion and
OR/AND/XOR/XNOR to BG1-4/OBJ before Mode 0 priority selection. Synthetic
vectors cover all four operations, edge boundaries, inversion, disabled
windows, reversed edges, BG4/OBJ separation, masked fall-through and the
machine bus/event handoff. Host C99 `cc` `make clean && make test` passes
(2026-10-24). Revision `01fe1b8` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103587677),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103587679)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40103587649).
At that revision, sub-screen and color windows, mid-line updates and target
runtime remained open. See the
[window audit](docs/ppu-window-mask-audit.md).

The shared machine now prepares a separate Mode 0 sub-screen index/source
and raw BGR555 line from `$212D/$212F` with the same layer priority and
window rules. `$2132` retains independently selected 5-bit red/green/blue
fixed-color channels; the sub-screen backdrop uses this fixed color. This
raw line is an intermediate color-math source, not a displayed image.
Synthetic fixtures cover channel retention, independent main/sub layer
selection, masked sub-screen fallback, fixed-color backdrop and invalid
inputs. Host C99 `cc` `make clean && make test` passes (2026-10-24).
Revision `b258135` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40104037923),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40104037911)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40104037891).
Color math, color window, mid-line register changes and target runtime
remain open. See the
[sub-screen audit](docs/ppu-subscreen-audit.md).

The shared Mode 0 path now resolves an unattenuated main line, performs
`$2130/$2131` color addition or subtraction against the sub-screen or fixed
color, and applies `$2100` brightness afterward. Per-layer enables include
backdrop and OBJ palettes 4-7; half math is skipped for a sub-screen backdrop.
Pure fixtures cover channel packing, saturation, subtraction clamp, half
rounding, source gating, the backdrop exception, brightness order and invalid
inputs. A machine fixture covers register routing and a combined BG/fixed
color pixel. Host C99 `cc` `make clean && make test` passes (2026-10-24).
Revision `b7fa3f5` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40128208302),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40128208329)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40128208307).
At that revision color-window clipping, mid-line behavior, target runtime
and output remained open. See
the [color math audit](docs/ppu-color-math-audit.md).

The shared color-window pass now uses `$2125` high-nibble enable/inversion,
`$212B` color-window logic and `$2130` region bits. It can clip the main
pixel to black before math and suppress math in a separate region. Fixtures
cover inside/outside/never/always, XOR, disabled windows, half-math
suppression on clipped main pixels and the machine bus/event route. Host
C99 `cc` `make test` passes (2026-10-24). Revision `5e36e8b` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40128545747),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40128545742)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40128545745).
Mid-line changes, pseudo-hires, analog output and target runtime
remain open. See the [color-window audit](docs/ppu-color-window-audit.md).

The shared Mode 1 reference path now decodes BG1/BG2 in 4bpp and BG3 in
2bpp, selecting main/sub pixels with Mode 1 BG/OBJ priority. `$2105` bit 3
raises BG3 priority-1 pixels. Synthetic fixtures cover bitplanes, tilemap
boundaries, flip, transparency, register routing, priority/masks and a
scheduled machine line. Host C99 `cc` `make clean && make -j2 test` passes
(2026-10-24). Revision `7d1f09d` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129043810),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129043772)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129043850). There is no
PS2 or PSP screen output and no target runtime validation. See the
[Mode 1 audit](docs/ppu-mode1-audit.md).

An optional synchronous video-line callback now exposes the completed
256-pixel BGR555 reference line to a caller-owned backend. Synthetic machine
fixtures cover line order, pixel delivery, forced blank and copying before
buffer reuse. Host C99 `cc` `make clean && make -j2 test` passes
(2026-10-24). Revision `307643b` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129271327),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129271341)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129271384). The callback
does not present pixels on either console. See the
[video-line audit](docs/video-line-handoff-audit.md).

The PS2 ROM path now captures those lines into a bounded 256×256 RGBA32
texture buffer with deterministic BGR555 conversion and black padding.
Host fixtures cover packing, visible-row limits, duplicate lines and
invalid initialization; C99 `cc` `make clean && make -j2 test` passes
(2026-10-24). Revision `e003265` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129509895),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129509814)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129509884).
At that revision no GS upload or screen presentation was implemented. See the
[PS2 frame audit](docs/ps2-video-frame-audit.md).

The PS2 `--rom` path now submits a captured 256×256 RGBA32 image directly
to a GS framebuffer when at least one reference line exists. The host
conversion and machine-handoff tests pass. Revision `4ff9020` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129898305)
and [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129898337);
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40129898350)
also passed. Target runtime evidence is absent. This is a static, single
buffer without scaling or a continuous frame loop. No visual result in
ARMSX2/NetherSX2 or physical PS2 is claimed. See the
[GS audit](docs/ps2-gs-audit.md).

The shared DMA foundation now holds the eight `$43x0-$43xB` channel banks,
including the `$43xB/$43xF` alias, and routes them through the machine bus.
Pure helpers plan all eight B-bus port sequences, A-address increment/fix/
decrement and zero-size length. Synthetic fixtures cover boundaries,
mirroring, isolation and power-on; serial `make clean && make test` passes
(2026-10-24). Revision `795f103` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40130384836),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40130385004)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40130384830).
The shared DMA core now also has a bounded, resumable byte cursor. It visits
enabled channels in priority order, derives one A/B-bus data phase per byte,
and updates A1T/DAS after each callback. Deterministic synthetic tests cover
all eight modes, both directions, channel order, fixed/decrement/increment,
bank and B-port wrapping, pause/resume, zero count and register completion.
Serial `make clean && make test` passes on host C99 (2026-10-24).
Revision `7fc0d79` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40131063212),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40131063247)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40131063232).
The machine now consumes `$420B` requests after one extra CPU cycle and
suspends CPU operation admission while DMA advances through scheduler/H/V
horizons. One data slot transfers one byte through the shared A/B bus policy;
CPU access counters are not double-charged. Fixtures cover actual VRAM/OAM/
CGRAM ports, channel setup, resynchronization, interrupts and exact-once
resume. The byte effects commit at slot endpoints as an explicit reference
approximation. WRAM refresh, HDMA arbitration, electrical phases and DMA/math
overlap remain pending. See the [machine DMA audit](docs/dma-machine-audit.md).
Host C99 suite passed with `make -W tests/test_cpu_io.c test` (2026-10-24).
The preceding clean build produced a non-executable CPU-I/O test binary;
rebuilding that generated target resolved the local failure. The new DMA
fixture also passes AddressSanitizer/UBSan with `ASAN_OPTIONS=detect_leaks=0`;
LeakSanitizer cannot inspect this runtime's process tasks. Revision `71c22cf`
passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40132743907),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40132743906)
and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40132743939).

A broad source and verification review found no reproducible correctness,
memory-safety or undefined-behavior defect in the current bounded core/backend
scope. GCC 13.3 `-fanalyzer`, an `-O3 -DNDEBUG -fstrict-aliasing` full suite and
ASan/UBSan passed locally. A new deterministic differential test now executes all
256 opcode values from four valid states through both CPU entry points and compares
architectural state, cycle totals and exact ordered bus traces: 1,024/1,024 cases
pass. This is internal consistency evidence rather than hardware conformance, and no
target runtime was exercised. See the
[2026-10-24 code review](docs/code-review-2026-10-24.md).
Revision `f9dc49f` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40150351323),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40150351293) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40150351308).

The H/V-aware resumable bridge now stalls both CPU operations and general DMA
phases for the measured 40-master-clock WRAM refresh window. The first refresh
starts at H=538; absolute modulo-eight alignment produces H=538/H=534 on normal
lines and retains phase across the 1360-clock NTSC short line. Pending effects
remain exact-once across horizons, with completed refresh clocks reported
separately from CPU and DMA. Deterministic fixtures cover CPU and DMA crossings,
four boundary horizons, normal-line alternation and the NTSC short line. See the
[WRAM refresh audit](docs/wram-refresh-audit.md). Target runtime validation remains
pending.

Revision `0a3dc11` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151173570),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151173536) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151173541).

The shared core now also contains a pure HDMA table/scanline engine. `$420C`
stores its enable mask; frame initialization copies A1T to A2A and loads line
descriptors plus indirect pointers. Visible-line calls process channels 0 through
7 with all direct/indirect transfer modes, repeat/wait counters, pause/resume,
bank/port wrapping and zero termination. Deterministic tests cover those logical
effects. This layer is intentionally untimed; see the
[HDMA table audit](docs/hdma-table-audit.md).

Revision `dc56eb9` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151827396),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151827359) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40151827425).

The shared HDMA engine now has a resumable operation cursor yielding global and
per-channel overhead, table reads and A/B data slots. Its old atomic entry
points use the same implementation. Deterministic host tests cover operation
order/cost, stable peeks, stale-state rejection and pointer commit boundaries;
the full C99 `cc` host suite and repository checker pass (2026-10-25). The
cursor itself remains untimed: scheduler requests, refresh, CPU/DMA arbitration
and horizon retries are the next block. See the
[HDMA cursor audit](docs/hdma-cursor-audit.md). Revision `71d3232` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40175220242),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40175220257) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40175220251).
Subsequent reference review corrected an eight-clock-per-channel overcount in
frame initialization and removed an extra line-descriptor charge; the corrected
timing is covered in the following block.

The machine now schedules HDMA frame and visible-line requests after `$420C`
is enabled. The resumable bridge gives HDMA priority between general DMA slots,
advances its operation cursor through shared H/V and WRAM refresh, and commits
table/data effects at endpoints. Host fixtures cover real VRAM port writes,
repeated horizons, event-budget retry, DMA preemption on distinct channels,
refresh, A-bus exclusions and NTSC/PAL next-frame events. See the
[timed HDMA audit](docs/hdma-timed-audit.md). At revision `e18fe55`,
same-channel register collisions, VBlank deactivation and electrical phases
remained open. That revision
passed [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176249767),
but [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176249749)
and [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176249757)
failed at link because their object lists omitted `hdma.o`. Both Makefiles now
include it, and the repository checker compares target core objects with the
host list. Revision `8a0b915` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176428169),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176428148) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176428141).
Target runtime checks remain pending.

The general-DMA cursor now cancels channels selected by an admitted HDMA
operation at a DMA slot boundary. An interrupted channel retains its remaining
DAS count, while unrelated channels continue after HDMA. Pure cursor and timed
machine fixtures cover active/future cancellation, incomplete count retention
and a surviving channel. See the
[DMA/HDMA overlap audit](docs/dma-hdma-overlap-audit.md). Revision `0d1d64b`
passed [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176937420),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176937429) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40176937440).
Real-console behavior remains unverified.

The machine now schedules HDMA deactivation at H=0 of the first VBlank line
after `$420C` is enabled. It clears active/transfer masks without clearing
`$420C`; if an HDMA slot is already running, that slot finishes before the
cursor stops. Host fixtures cover NTSC and PAL VBlank starts, no same-frame
restart, and an eight-channel line that overruns VBlank. See the
[HDMA VBlank audit](docs/hdma-vblank-audit.md). CI and target runtime checks
for this change: revision `559eda5` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177443593),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177443514) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177443512).
Target runtime remains unverified.

The indirect HDMA cursor now treats a zero descriptor on the final selected
channel of a visible line as the low indirect-address byte, fetching only the
high byte from the table. Earlier channels still fetch both bytes. Pure
fixtures cover both channel positions and the eight-clock difference; a timed
machine fixture checks the resulting table pointer.
See the [zero-descriptor audit](docs/hdma-zero-descriptor-audit.md). Revision
`073b867` passed
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177771815),
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177771794) and
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177771800).
Real-console validation remains pending.

The shared machine now owns `$2133` SETINI and connects bit 2 to its
non-interlaced H/V state. Writes before V=225 select the current frame's
V=225/V=240 VBlank boundary; later writes latch for V=0 of the next frame.
OAM reload and HDMA deactivation probe V=225 and defer to V=240 when needed,
including `$420C` enabled during the overscan-only lines. Host fixtures cover
both boundaries, the next-frame latch and operation without optional PPU
memory. Exact late-line behavior, interlace/pseudo-hires bits and continuous
target display remain open. See the
[SETINI overscan audit](docs/ppu-setini-overscan-audit.md). Revision `a0a553a`
passed [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40199872417),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40199872441) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40199872450).
No target-runtime result is claimed.

The shared core now owns an eight-latch S-CPU/S-SMP communication mailbox:
four bytes in each direction. The CPU side maps `$2140-$217F` as four-address
mirrors; the future S-SMP side uses port indices 0-3. Pure fixtures cover every
direction, mirror, overwrite and invalid input, while the machine fixture covers
default bus routing and power-on reset. Visibility is immediate because no
S-SMP clock domain exists yet. See the
[APU port audit](docs/apu-io-port-audit.md). Revision `64e8f24` passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40200420524),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40200420538) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40200420504).
No target runtime or audio output is claimed.

The S-SMP now has a bounded local map over caller-owned 64 KiB ARAM, connected
to the machine's directional mailbox at `$F4-$F7`. `$F1` bit 7 selects a
caller-supplied 64-byte IPL read overlay, while writes still reach underlying
ARAM; bits 4/5 clear the S-SMP input-port pairs. `$F8/$F9` remain ordinary
ARAM. Unsupported DSP and test registers return failure instead of
silently acting as RAM. PS2 reserves ARAM; PSP links the shared object without
a runtime audio path. Synthetic host fixtures cover map boundaries, overlay
switching, missing IPL, input clear, output independence and machine routing.
`make test` and `make check` passed on host C99 `cc` (2026-10-25); pinned
PS2/PSP cross-build CI awaits publication. See the
[S-SMP map audit](docs/smp-memory-map-audit.md). No SPC700 instructions,
S-DSP, sound samples or target-runtime results are claimed.

The shared S-SMP now owns three-stage timers behind `$F1/$FA-$FF`. Timer 0/1
share each 128-cycle first-stage edge, timer 2 observes each 16-cycle edge and
the common prescaler remains free-running while disabled. Rising enables clear
stages 2/3 without resetting phase; targets accept live changes, zero means 256,
and four-bit outputs clear on read. The memory-map wrapper accepts explicit
S-SMP-cycle advancement. Pure fixtures cover boundaries, enable phase retention,
target wrap, output wrap and one-shot/chunked equivalence; map and machine tests
cover register routing and power-cycle reset. Host C99 verification is recorded
in the [timer audit](docs/smp-timer-audit.md). `make clean && make -j2 test`
passed with host C99 `cc` on 2026-10-25. An ASan/UBSan full-suite attempt hit
the previously observed local non-executable generated-test artifact at
`test-hv-timing`; force-rebuilding that target passed, followed by clean
sanitized timer, memory-map and machine fixtures. Real power-on values, exact
DSP-relative phase, TEST effects, register-edge races and a synchronized S-SMP
clock domain remain unverified. Revision `3a9329f` passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40237174159),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40237174284) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40237174281).
No audio or target-runtime result is claimed.

The caller-owned SPC700 execution foundation now represents A/X/Y/SP/PSW/PC and
fetches opcodes through the bounded S-SMP map, including IPL overlay selection and
16-bit PC wrap. Atomic dispatch implements `NOP` plus CLRP/SETP, CLRC/SETC, EI/DI,
CLRV and NOTC; their 2/3-cycle costs advance the timer block exactly once. All
other 247 byte values return an explicit unimplemented result without committing
programmer-visible registers or S-SMP time. Fixtures cover all 256 dispatch values,
every implemented PSW transition, mapped-fetch failure, cycle-total overflow, IPL
fetch and instruction/timer coupling. Exact per-cycle bus traffic, reset values,
interrupt behavior, operands and the remaining instructions are not claimed. See
the [SPC700 execution audit](docs/spc700-execution-foundation-audit.md). The complete
host suite passed after restoring the executable bit on the known local generated-
test artifact `test-cpu-io`; the focused ASan/UBSan fixture passed with leak checking
disabled because LeakSanitizer cannot inspect `/proc` in this environment. GCC
`-fanalyzer` passed. Clang, PS2SDK, `psp-config` and target runtime are unavailable
locally. Revision `e0313ab` passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40238640280),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40238640404) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40238640364).
No target-runtime result is claimed.

The atomic SPC700 dispatcher now also implements `MOV A/X/Y,#imm`, `MOV A,X`,
`MOV A,Y`, `MOV X,A`, `MOV Y,A`, `MOV X,SP` and `MOV SP,X`. The three immediate
forms fetch a second mapped byte with 16-bit PC wrap; an unavailable operand leaves
programmer-visible state, cycles and timers unchanged. All loads/transfers except
`MOV SP,X` derive N/Z from the transferred byte, while `MOV SP,X` preserves PSW.
Focused fixtures cover zero/positive/negative values for every immediate target,
all six register transfers, unaffected flags/registers, overflow before operand
fetch, unavailable IPL operands and MOV-to-timer cycle delivery. Eighteen opcodes
now execute and 238 fail explicitly. See the
[SPC700 MOV audit](docs/spc700-mov-foundation-audit.md). `make clean && make -j2
test` passed with host C99 `cc`; the focused ASan/UBSan fixture passed with leak
checking disabled for this environment, and GCC `-fanalyzer` passed. Clang,
PS2SDK, `psp-config` and target runtime are unavailable locally. Revision
`9d2930f` passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40239618720),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40239618753) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40239618714).
No target-runtime result is claimed.

The dispatcher now also implements `MOV A/X/Y,dp` and `MOV dp,A/X/Y`. PSW.P
selects page `$00xx` or `$01xx`, and each access uses the S-SMP map so page-zero
MMIO is not bypassed. Loads derive N/Z and cost three cycles; stores preserve PSW
and cost four. Fixtures cover both pages, all three registers, zero/positive/
negative loads, `$F4` communication-port routing versus `$01F4` ARAM, unavailable
`$F0` accesses and overflow before store mutation. Twenty-four opcodes now execute
and 232 fail explicitly. See the
[SPC700 direct-page MOV audit](docs/spc700-direct-page-mov-audit.md). A clean
`make -j2 test` passed with host C99 `cc`; focused ASan/UBSan with leak checking
disabled and GCC `-fanalyzer` also passed. Clang, PS2SDK, `psp-config` and target
runtime are unavailable locally. Revision `444eb36` passed
[host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40351899529),
[PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40351899482) and
[PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40351899443).
No target-runtime result is claimed.

The dispatcher now also implements `MOV A,dp+X`, `MOV X,dp+Y`, `MOV Y,dp+X`
and the matching `MOV dp+X,A`, `MOV dp+Y,X`, `MOV dp+X,Y` stores. The
operand-plus-index sum wraps to eight bits before PSW.P selects page `$00xx` or
`$01xx`; accesses still traverse the S-SMP map. Loads derive N/Z and cost four
cycles; stores preserve PSW and cost five. Fixtures cover both pages, forced
index wrap, all registers and load sign classes, `$F4` MMIO versus `$01F4` ARAM,
unavailable wrapped `$F0`, timer delivery and overflow before store mutation.
Thirty opcodes now execute and 226 fail explicitly. See the
[SPC700 indexed-direct-page MOV audit](docs/spc700-indexed-direct-page-mov-audit.md).
The complete host suite, focused ASan/UBSan with leak checking disabled, repository
checks and GCC `-fanalyzer` pass locally; the known generated-test executable-bit
issue required restoring that permission after the clean build. Clang, PS2SDK,
`psp-config` and target runtime are unavailable locally. Target CI is pending.
No target-runtime or audio result is claimed.

Next primary block: add absolute A/X/Y `MOV` load/store forms. Refining
DMA/HDMA electrical bus phases and post-HDMA CPU alignment remains
evidence-dependent. PS2 GS emulator and physical-console checks remain separate.

The existing bounded DMA cursor already keeps A1B fixed while incrementing,
decrementing or fixing the 16-bit A1T address, wraps A1T at `$0000/$FFFF`, and
wraps BBAD plus mode offsets within `$21xx`. Its synthetic fixture also proves
lowest-channel-first ordering across pauses. These two previously open status
items are therefore closed against the published `7fc0d79` PS2/PSP/host CI
evidence in the [bounded-transfer audit](docs/dma-bounded-transfer-audit.md);
no new behavior or target-runtime claim is introduced by this audit correction.

## State and evidence rules

- [ ] Not implemented or not verified
- [x] Implemented AND verified
- [ ] [IN PROGRESS] Work started but incomplete
- [ ] [BLOCKED] Blocked — reason must be included

These four lines are a notation legend, not project progress. Only the task rows
below carry project state. A check requires implementation, successful applicable
build/tests, no relevant placeholder, updated docs and target validation where
required. Parent work remains incomplete while required children are unfinished.

Verification gates for unchecked future tasks: hardware behavior requires sourced
semantics and edge-case regressions; subsystem integration requires deterministic
integration tests; console runtime and speed require the named real device;
optimization requires baseline, regression and repeat measurement on that target;
GitHub execution requires a successful run URL, artifact/release inspection where
applicable. Research/review tasks require a written evidence-based decision, including
an explicit justified rejection if not applicable. Administrative tasks require
inspection of the actual setting. Documents/structure require content/link audit.

## Foundation

- [ ] [IN PROGRESS] Phase 0 foundation fully verified — repository bootstrap verified; release/admin activation pending
- [x] Host build system
- [x] Minimal host target compiles
- [x] Minimal automated test runs
- [x] Logging contract and disabled configuration
- [x] Shared logical-address and bus interface contracts
- [x] PS2 scaffolding uses verified SDK conventions
- [x] PSP scaffolding uses verified SDK conventions
- [x] PS2 scaffold cross-compiles
- [x] PSP scaffold cross-compiles
- [x] GPL-3.0 LICENSE preserved byte-for-byte
- [x] Optional pinned XERJ first-party source/document search workflow verified

## Documentation

- [x] README.md
- [x] PROJECT_GOALS.md
- [x] ARCHITECTURE.md
- [x] STATUS.md
- [x] ROADMAP.md
- [x] PERFORMANCE.md
- [x] COMPATIBILITY.md
- [x] CONTRIBUTING.md
- [x] AGENTS.md
- [x] docs/65c816.md
- [x] docs/bus.md
- [x] docs/ppu.md
- [x] docs/spc700.md
- [x] docs/dsp.md
- [x] docs/apu.md
- [x] docs/dma-hdma.md
- [x] docs/scheduler.md
- [x] docs/cartridge.md
- [x] docs/enhancement-chips.md
- [x] docs/ps2.md
- [x] docs/psp.md
- [x] docs/host.md
- [x] docs/optimization.md
- [x] docs/testing.md
- [x] docs/debugging.md
- [x] docs/github-setup.md
- [x] docs/references.md — curated non-emulator hardware, independent-test and SDK sources
- [x] docs/xerj-workflow.md — host-only first-party indexing and provenance policy

## Repository Structure

- [x] core/
- [x] core/cpu/65c816/
- [x] core/bus/
- [x] core/ppu/
- [x] core/apu/spc700/
- [x] core/apu/dsp/
- [x] core/dma/
- [x] core/scheduler/
- [x] core/cartridge/
- [x] core/enhancement/
- [x] core/debug/
- [x] core/common/
- [x] platform/ps2/
- [x] platform/psp/
- [x] platform/host/
- [x] tests/
- [x] tools/

## Architecture Validation

- [x] Core has no PS2 API dependency
- [x] Core has no PSP API dependency
- [x] Host does not control architecture
- [x] Dependency direction documented
- [x] PS2 dedicated optimization policy documented
- [x] PSP dedicated optimization policy documented
- [x] No game-specific hacks
- [x] No copied emulator implementation
- [x] No fake completed subsystem
- [x] No invented SDK APIs

## GitHub Infrastructure

- [x] Host CI workflow file audited
- [x] PS2 workflow file audited
- [x] PSP workflow file audited
- [x] Pre-release workflow file audited
- [x] Artifact upload configuration audited
- [x] SHA-256 workflow support audited
- [x] GitHub Actions host CI
- [x] Automated host tests in GitHub Actions
- [x] PS2 CI build
- [x] PSP CI build
- [x] Build artifacts
- [x] SHA-256 generation
- [x] Dependency/toolchain caching
- [ ] [BLOCKED] Development pre-release workflow end-to-end — requires reviewed main and a manual release run
- [x] Issue templates
- [x] Pull request template
- [x] Dependabot configuration
- [ ] Dependabot service execution
- [x] GitHub setup documentation
- [ ] [BLOCKED] Main branch ruleset configured — owner/admin must configure after checks exist
- [ ] Labels created
- [ ] Milestones created
- [x] First full CI run verified
- [x] Bootstrap PR opened

## Verification snapshot

Local foundation audit: GCC 13.3.0 host default/release tests pass; ASan+UBSan pass
with local LeakSanitizer disabled because the runtime cannot inspect /proc tasks.
PS2 and PSP scaffolds cross-compile using pinned archives (both cross GCC 15.2.0).
Actionlint 1.7.12 and YAML parse checks pass. No console runtime has been tested.
Exact commands and every checkbox promotion are in docs/bootstrap-audit.md.
First full PR CI passed at e70bca9 (GCC/Clang including full sanitizers, PS2/PSP).
Node 24 Actions also passed at 21d568f, including GCC/Clang full sanitizers and
PS2/PSP builds, artifact/checksum generation and verified warm-cache reuse. Run URLs
and exact evidence are in docs/bootstrap-audit.md. Remaining release/admin work is
explicitly separated below and in docs/github-setup.md.

The 65C816 foundation audit on 2026-10-10 passed GCC host release tests, local
ASan+UBSan (with only LeakSanitizer disabled for the documented `/proc` runtime
restriction), and pinned PS2/PSP cross-builds including the new CPU/bus sources.
Local Clang was unavailable; GitHub's existing Clang/full-sanitizer job remains the
gate. Exact commands and limitations are in docs/65c816-foundation-audit.md.

The addressing/load-store audit on 2026-10-10 passed GCC host debug/release tests,
local ASan+UBSan with the same LeakSanitizer restriction, and pinned PS2/PSP
cross-builds. It verifies 27 opcodes and corrects implemented data accesses to linear
24-bit bank crossing. The first GitHub Actions execution passed Host GCC/Clang,
sanitizers and pinned PS2/PSP cross-build jobs. Exact local and CI evidence is in
docs/65c816-addressing-audit.md.

The Direct Page load/store audit on 2026-10-10 verifies 12 additional opcodes on
host GCC, including bank-zero selection, E/DL wrapping, 8/16-bit data and DL/width
cycle additions. Pinned local PS2/PSP cross-builds pass. The first GitHub Actions
execution passed Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP
cross-build, artifact and checksum jobs. Exact evidence is in
docs/65c816-direct-page-audit.md; this does not complete Direct Page addressing.

The Direct-indirect load/store milestone on 2026-10-10 verifies ten additional LDA/
STA opcodes on host GCC. Tests distinguish regular 16-bit pointer page wrapping from
the Direct-indirect-long linear pointer-byte exception, DBR from pointer-supplied data
banks, pre/post indexing, 8/16-bit data and documented cycle additions. GitHub Host
GCC/Clang release and sanitizer jobs plus pinned PS2/PSP cross-build, artifact and
checksum jobs passed for the implementation commit. Exact evidence is recorded in
docs/65c816-direct-indirect-audit.md; indirect addressing remains incomplete.

The A/X/Y register-transfer milestone on 2026-10-10 verifies TAX/TAY/TXA/TYA/TXY/TYX
on host GCC. Tests cover emulation/native and mixed M/X widths, destination high-byte
rules, full-source behavior, zero extension, N/Z, preserved status bits, PC movement
and two-cycle timing. Pinned PS2SDK/PSPSDK cross-builds compile the changed core and
pass. GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP cross-build,
artifact and checksum jobs passed for the implementation commit. Exact evidence and
limits are in docs/65c816-register-transfer-audit.md.

The special-transfer milestone on 2026-10-11 verifies TCS/TSC/TCD/TDC/TXS/TSX/XBA
on host GCC debug, release and ASan+UBSan builds. Tests cover fixed-width C/D/S
transfers, native/emulation stack-high rules, X-selected TSX/TXS behavior, XBA flags,
PC movement and exact base cycles. Pinned PS2SDK/PSPSDK cross-builds compile the
changed core and pass. GitHub Host GCC/Clang release and sanitizer jobs plus pinned
PS2/PSP cross-build, artifact and checksum jobs passed for the implementation commit.
Exact evidence and limits are in docs/65c816-special-transfer-audit.md.

The branch-family milestone on 2026-10-11 verifies BPL/BMI/BVC/BVS/BCC/BCS/BNE/BEQ,
BRA and BRL on host GCC debug, release and ASan+UBSan builds. Tests cover taken and
non-taken conditions, signed 8/16-bit offsets, unchanged status, PBR-local PC
wrapping, and exact 2/3/4-cycle behavior in native and emulation modes. Pinned
PS2SDK/PSPSDK cross-builds compile the changed core and pass. GitHub Host GCC/Clang
release and sanitizer jobs plus pinned PS2/PSP cross-build, artifact and checksum
jobs passed for the implementation commit. Exact evidence and validation limits are
in docs/65c816-branch-audit.md.

The implied status-control milestone on 2026-10-11 verifies CLD/SED, CLI/SEI and CLV
on host GCC debug, release and ASan+UBSan builds. Tests cover exact D/I/V changes,
unrelated-status and register preservation, one-byte PC movement, fixed two-cycle
timing and identical behavior in emulation/native modes. Pinned PS2SDK/PSPSDK
cross-builds compile the changed core and pass. Exact evidence and validation limits
are in docs/65c816-status-control-audit.md. GitHub Host GCC/Clang release and
sanitizer jobs plus pinned PS2/PSP cross-build, artifact and checksum jobs passed for
the implementation commit.

The accumulator/index increment-decrement milestone on 2026-10-11 verifies
INC A/DEC A, INX/DEX and INY/DEY on host GCC debug, release and ASan+UBSan builds.
Deterministic regressions cover emulation and native modes, M/X-selected 8/16-bit
wraparound, accumulator B preservation, 8-bit index zero extension, isolated N/Z
changes, one-byte PC movement and fixed two-cycle timing. Pinned PS2SDK/PSPSDK
cross-builds compile the changed core and pass. Exact evidence and validation limits
are in docs/65c816-increment-decrement-audit.md. GitHub Host GCC/Clang release and
sanitizer jobs plus pinned PS2/PSP cross-build, artifact and checksum jobs passed for
the implementation commit.

The logic/compare foundation milestone on 2026-10-11 adds 23 verified opcodes: base
immediate/Direct/absolute ORA, AND, EOR and CMP forms plus every CPX, CPY and BIT
form. Host GCC debug, release and ASan+UBSan tests cover 8/16-bit M/X selection,
N/Z/C/V isolation, preserved operands/B, Direct Page penalties, indexed BIT timing
and linear word reads across a bank boundary. Pinned PS2SDK/PSPSDK cross-builds
compile the 105-opcode core and produce nonempty target artifacts. Exact local
evidence and validation limits are in docs/65c816-logic-compare-audit.md. GitHub
Host GCC/Clang plus pinned PS2/PSP jobs passed, and downloaded artifacts/checksums
were inspected for the implementation commit.

The extended ORA/AND addressing milestone on 2026-10-12 adds 20 verified opcodes,
covering every indexed, indirect and long ORA/AND form that does not require the
still-unimplemented stack-relative address paths. Host GCC debug, release and
ASan+UBSan tests cover both M widths, N/Z and B preservation, Direct Page/index
penalties, regular versus long pointer banks, emulation Direct-X wrapping and 24-bit
page/bank carry. Pinned PS2SDK/PSPSDK cross-builds compile the 125-opcode core and
produce nonempty target artifacts. Exact local evidence and validation limits are
in docs/65c816-ora-and-audit.md. GitHub Host GCC/Clang plus pinned PS2/PSP jobs
passed for the implementation commit, and downloaded artifacts plus archive/internal
checksums were inspected.

The extended EOR/CMP addressing milestone on 2026-10-12 adds 20 verified opcodes,
covering every indexed, indirect and long EOR/CMP form that does not require the
still-unimplemented stack-relative address paths. Host GCC debug, release and
ASan+UBSan tests cover both M widths, EOR result/B behavior, CMP operand/N/Z/C
behavior, Direct Page/index penalties, regular versus long pointer banks, emulation
Direct-X wrapping and 24-bit page/bank carry. Pinned PS2SDK/PSPSDK cross-builds
compile the 145-opcode core and produce nonempty target artifacts. Local evidence
is recorded in docs/65c816-eor-cmp-audit.md. GitHub Host GCC/Clang plus pinned
PS2/PSP jobs passed for the implementation commit, and downloaded artifacts plus
archive/internal checksums were inspected.

The stack-relative accumulator-ALU milestone on 2026-10-12 adds eight verified
opcodes and completes the ORA/AND/EOR/CMP instruction families. Host GCC debug,
release and ASan+UBSan tests cover both M widths, full 16-bit `S + offset` formation,
bank-zero operand/pointer wrapping, DBR plus Y 24-bit carry, native/emulation
behavior, flags and exact 4/7-cycle bases without Direct Page, page-cross or X-width
penalties. Pinned PS2SDK/PSPSDK cross-builds compile the 153-opcode core and produce
nonempty target artifacts. Exact local evidence and the initial expired-temporary-
path setup failure are recorded in docs/65c816-stack-relative-audit.md. GitHub Host
GCC/Clang plus pinned PS2/PSP jobs passed for the implementation commit, and
downloaded artifacts plus archive/internal checksums were inspected. Stack-relative
addressing at that milestone remained in progress because ADC/SBC/LDA/STA forms were
not yet implemented; the following milestone advances that state.

The stack-relative accumulator load/store milestone on 2026-10-12 adds four verified
opcodes and completes the LDA/STA instruction families. Host GCC debug tests cover
both M widths, 8-bit B preservation, LDA N/Z isolation, STA status preservation,
bank-zero word wrap, DBR plus Y 24-bit carry, native/emulation behavior and exact
4/7-cycle bases without Direct Page, page-cross or X-width penalties. Host GCC
release and ASan+UBSan tests pass; pinned PS2SDK/PSPSDK cross-builds compile the
157-opcode core and produce nonempty target artifacts. Exact local evidence is in
docs/65c816-stack-relative-load-store-audit.md. GitHub Host GCC/Clang plus pinned
PS2/PSP jobs passed for the implementation commit, and downloaded artifacts plus
archive/internal checksums were inspected. Stack-relative addressing remains in
progress because ADC/SBC forms are not implemented.

The binary immediate arithmetic milestone on 2026-10-12 adds ADC `69` and SBC `E9`
for M-selected 8/16-bit binary operation. Host GCC debug, release and ASan+UBSan
tests cover carry input and borrow convention, carry out/no-borrow, signed overflow,
N/Z, wrap, 8-bit B preservation, operand length and exact 2/3-cycle timing. D=1 stops
before operand fetch with `MIPSNES2_CPU_UNIMPLEMENTED_DECIMAL_ARITHMETIC`; decimal
behavior is not approximated. Pinned PS2 and PSP cross-builds compile and produce
nonempty artifacts. GitHub Host GCC/Clang plus pinned PS2/PSP jobs passed for the
implementation commit, and downloaded artifacts plus archive/internal checksums were
inspected. Exact evidence is in docs/65c816-binary-arithmetic-audit.md. At that
milestone all other ADC/SBC forms and decimal arithmetic remained incomplete.

The binary Direct Page/absolute arithmetic milestone on 2026-10-15 adds ADC `65/6D`
and SBC `E5/ED`. Deterministic host tests cover both M widths, Direct Page bank-zero
word wrapping, aligned and `DL != 0` cycle bases, absolute DBR selection, linear
absolute word reads across a bank boundary, C/V/N/Z, B preservation, PC and exact
cycles. All six recognized ADC/SBC opcodes stop before operand/address fetch when
D=1. Host GCC debug, release and ASan+UBSan tests pass; freshly SHA-256-verified pinned
PS2/PSP toolchains cross-compile nonempty target artifacts. GitHub Host GCC/Clang plus
pinned PS2/PSP jobs passed for the implementation commit, and downloaded artifacts
plus archive/internal checksums were inspected. Exact evidence is in
docs/65c816-binary-direct-absolute-audit.md. Indexed, indirect, long, stack-relative
and decimal ADC/SBC remain incomplete.

The binary indexed arithmetic milestone on 2026-10-15 adds ADC `75/79/7D` and SBC
`F5/F9/FD`. Deterministic host tests cover both M widths, Direct-X bank-zero wrap and
DL timing, absolute-X/Y DBR selection, page crossing, 16-bit index timing, linear
24-bit bank carry, C/V/N/Z, B preservation, PC and exact cycles. All twelve recognized
ADC/SBC opcodes stop before operand/address fetch when D=1. Local host and cross-build
tests pass. GitHub Host GCC/Clang plus pinned PS2/PSP jobs passed for the implementation
commit, and downloaded artifacts plus archive/internal checksums were inspected. Exact
evidence is in docs/65c816-binary-indexed-audit.md. Indirect, long, stack-relative
and decimal ADC/SBC remain incomplete.

The binary stack-relative arithmetic milestone on 2026-10-15 adds ADC `63/73` and
SBC `E3/F3`, completing both stack-relative accumulator addressing forms across
LDA/STA/ORA/AND/EOR/CMP/ADC/SBC. Deterministic host tests cover both M widths, full
16-bit `S + offset`, bank-zero word/pointer wrap, DBR plus Y 24-bit carry, native and
emulation modes, C/V/N/Z, B preservation, PC and exact 4/7-cycle bases. All sixteen
recognized ADC/SBC opcodes stop before operand/address fetch when D=1. Local evidence
and GitHub Host GCC/Clang plus pinned PS2/PSP jobs pass. Downloaded artifacts and
archive/internal checksums were inspected. Exact evidence is in
docs/65c816-binary-stack-relative-audit.md. Direct-indirect, long and decimal ADC/SBC
remain incomplete.

The binary regular Direct-indirect arithmetic milestone on 2026-10-15 adds ADC
`61/71/72` and SBC `E1/F1/F2`. Deterministic host tests cover both M widths,
bank-zero pointer wrap, DBR selection, X pre-indexing, Y post-indexing and 24-bit
carry, aligned/nonzero-DL timing, the shared page-cross-or-16-bit-index cycle,
C/V/N/Z, B preservation, PC and exact cycles. All twenty-two recognized ADC/SBC
opcodes stop before operand/address fetch when D=1. Local evidence is recorded in
docs/65c816-binary-direct-indirect-audit.md. GitHub Host GCC/Clang plus pinned PS2/PSP
jobs pass for the corrected published implementation, and downloaded artifacts plus
archive/internal checksums were inspected. Direct-indirect-long, absolute-long and
decimal ADC/SBC remain incomplete.

The binary long arithmetic milestone on 2026-10-16 adds ADC `67/6F/77/7F` and SBC
`E7/EF/F7/FF`, completing every architectural ADC/SBC addressing form for binary
operation. Deterministic host tests cover both M widths, Direct-indirect-long pointer
bytes wrapping in bank zero, pointer-bank selection independent of DBR, absolute-long
operand fetch, full 24-bit indexed carry, C/V/N/Z, B preservation, PC and exact
cycles without page-cross or X-width penalties. All thirty ADC/SBC opcodes stop
before operand/address fetch when D=1. Local evidence and successful GitHub Host
GCC/Clang plus pinned PS2/PSP cross-builds are recorded in
docs/65c816-binary-long-audit.md; downloaded artifacts and all checksums were
inspected. Decimal ADC/SBC calculation remains incomplete.

The decimal arithmetic milestone on 2026-10-16 implements packed-BCD calculation
for all thirty ADC/SBC opcodes at both M widths without changing addressing or cycle
rules. Repository tests exhaust 40,000 valid 8-bit operand/carry combinations and
cover targeted 16-bit and invalid-digit cases. The arithmetic algorithm was also
audited against 19,970 decimal-mode immediate ADC/SBC cases from a pinned,
MIT-licensed SingleStepTests revision with zero C/V/N/Z/result mismatches. Local
evidence is in docs/65c816-decimal-arithmetic-audit.md; GitHub workflow evidence is
verified for Host GCC/Clang plus pinned PS2/PSP cross-builds, and all downloaded
artifact archives and internal checksums were inspected.

The memory read-modify-write and shift/rotate milestone on 2026-10-16 adds all
thirty-six ASL/LSR/ROL/R…1512 tokens truncated…it.md. No commercial ROM or runtime result is claimed.

The checksum/base-selection milestone on 2026-10-18 adds normalized whole-payload
checksums, unambiguous smaller-tail mirroring and conservative base LoROM/HiROM
selection. Map mode, base chipset, RESET vector, declared sizes, complement and actual
checksum must all agree; no-match and dual-valid ambiguity are explicit. Synthetic
tests cover each gate without title/opcode scoring or commercial data. Host GCC/Clang
release and sanitizer jobs plus pinned PS2/PSP scaffold cross-build, artifact and
checksum jobs passed for implementation commit
`b394209825d6c78ed2e08f86d7c142158e52ba6c`. Executable irregular boards, padded
ambiguous layouts, extended headers and special checksum conventions remain incomplete.
Exact evidence is in docs/cartridge-mapper-selection-audit.md. No console runtime,
compatibility or performance result is claimed.

The executable irregular-map milestone on 2026-10-18 extends the base mapper only to
bank-aligned images formed from one larger power-of-two region followed by one smaller
power-of-two tail. Initialization precomputes the mirror span; padded and board-specific
layouts are rejected. Generated host fixtures cover 96/160 KiB LoROM, 192 KiB HiROM,
tail repetition, span wrap, alignment/rejection and selected-header-to-mapper setup.
Local GCC default/release and ASan+UBSan tests pass. Local Clang and console SDKs were
unavailable. GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP
scaffold cross-build, artifact and checksum jobs passed for implementation commit
`2d9f03bb51dbb4248685e91fdfa61f85df28ffd3`. Exact evidence and limitations are in
docs/cartridge-irregular-mapping-audit.md. No console runtime, compatibility or
performance result is claimed.

The documented ExHiROM milestone on 2026-10-18 adds raw 16-byte registration fields,
Mode 25/35 classification at `$40FFC0`, conservative three-candidate selection and a
separate 4-8 MiB address transform with HiROM-style SRAM. Generated 6 MiB fixtures
cover first/extended regions, upper-half mirrors, bounded tail repetition, selection,
mapper initialization and CPU reset/NOP through the SNES bus. Local GCC default,
release and ASan+UBSan tests pass; local Clang and console SDKs are unavailable.
GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP scaffold
cross-build, artifact and checksum jobs passed for implementation commit
`9f333a97609857db808ec83a4f83f1a16429acdd`. Unofficial ExLoROM, physical cartridge
validation, special boards, file loading, persistence, console runtime, compatibility
and performance remain incomplete. Exact scope and evidence are in
docs/cartridge-exhirom-audit.md.

The cartridge-construction milestone on 2026-10-18 adds an allocation-free plan over
caller-owned file bytes, exposes exact header-declared SRAM requirements and binds a
validated mapper to caller-owned ROM/SRAM storage. Outputs remain unchanged on
failure and no file/path/platform API enters `core/`. Generated tests cover clean and
prefixed LoROM, missing/undersized/oversized SRAM capacity, failure transactionality,
tampered plans and selected 6 MiB ExHiROM construction. Local GCC default, release
and ASan+UBSan tests pass; local Clang and console SDKs are unavailable. GitHub Host
GCC/Clang release and sanitizer jobs plus pinned PS2/PSP scaffold cross-build,
artifact and checksum jobs passed for implementation commit
`cd55c07af49841751e67c750d1dceb9c70a8393f`. Platform file adapters, persistence
and ROM execution remain incomplete. Exact scope and evidence are in
docs/cartridge-construction-audit.md.

The SRAM-persistence-handoff milestone on 2026-10-18 keeps storage and filesystem
policy outside `core/` while exposing the mapper-visible caller-owned SRAM buffer,
dirty state and a revision token. Only value-changing writes advance the token;
acknowledging a stale revision cannot clear newer dirty data. Generated host tests
cover aliases, same-value writes, ROM/unmapped writes, stale and matching
acknowledgements, no-SRAM cartridges and deterministic revision wrap. Local GCC
default, release and ASan+UBSan suites pass; local Clang and console SDKs are
unavailable. GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP
scaffold cross-build, artifact and checksum jobs passed for implementation commit
`defebd3bf0ab741639e70a94e08a6fc65d18df0e`. Platform file loading, durable writes,
save scheduling and real-device validation remain incomplete. Exact scope is in
docs/cartridge-sram-persistence-audit.md.

The Ricoh 5A22 CPU-I/O foundation on 2026-10-18 adds caller-owned state for
NMITIMEN, the 9-bit H/V compare registers, RDNMI, TIMEUP and HVBJOY. It accepts
external blank transitions and timer-match signals, latches NMI edges, exposes the
timer IRQ level and is routed through the partial SNES bus without replacing the
fallback CPU-I/O interface. Deterministic tests cover reserved masks, open-bus bit
composition, mirror routing, enable transitions, read acknowledgements and blank/
joypad-busy status. GCC default, release and isolated ASan+UBSan host suites pass;
local Clang and console SDKs are unavailable. GitHub Host GCC/Clang release and
sanitizer jobs plus pinned PS2/PSP scaffold cross-build, artifact and checksum jobs
passed for implementation commit
`f0517d99ade3f1768fa803a8c3aa90bafab58370`. No H/V counter, PPU timing, automatic joypad poll,
external cartridge IRQ combination or exact TIMEUP match-window behavior is claimed.
Exact scope is in docs/cpu-io-foundation-audit.md.

The non-interlaced H/V timing foundation on 2026-10-19 adds caller-owned NTSC/PAL
master-clock phase, 262/312-line frames, HBlank/VBlank transitions, 224/239-line
VBlank selection, field alternation, the NTSC field-1 short scanline and the RDNMI/
NMI flag two master clocks after VBlank begins. The CPU-I/O input contract now keeps
VBlank status and NMI-flag assertion distinct. Deterministic tests cover exact frame
totals, boundaries, overscan, one-shot/chunked equivalence and overflow rejection.
GCC default, release and local ASan+UBSan suites pass; local Clang and console SDKs
are unavailable. GitHub Host GCC/Clang release and sanitizer jobs plus pinned PS2/PSP
scaffold cross-build, artifact and checksum jobs passed for implementation commit
`6ab26ca22c4d4ec838f513702b490a681196d610`. Interlace, PPU dot-counter latching,
dynamic SETINI and central-scheduler/CPU-boundary wiring remain incomplete. A later
milestone adds the stable-register H/V IRQ subset described below. Exact original
scope is in docs/hv-timing-foundation-audit.md.

The stable-register H/V timer-IRQ foundation on 2026-10-19 adds non-interlaced H,
V and HV match generation into the existing CPU-I/O TIMEUP latch. It covers every
valid HTIME value, 338/339 spill into the following line, V/HV positions including
the post-short-line offset, regional out-of-range values and the documented H=153
short/final-line suppressions. Exact mid-instruction register-write ordering,
multiple reprogrammed matches in one scanline, TIMEUP's narrow read-at-match exception,
interlace and automatic CPU interrupt-line delivery remain incomplete. Local GCC
default/release and ASan+UBSan verification suites pass. GitHub Host GCC/Clang plus
pinned PS2/PSP cross-build, artifact and checksum jobs passed for implementation
commit `9936fd24a2a8cfdd736d4fe7df707c11f0103e55`. Exact scope and run IDs are in
docs/hv-irq-foundation-audit.md. No console runtime or hardware result is claimed.

The shared master-clock fanout milestone on 2026-10-19 adds a caller-owned coordinator
that keeps the scheduler and non-interlaced H/V component on one checked timestamp.
It advances H/V transitions before general callbacks at the same time, preserves
domain alignment on event-budget exhaustion and lets the atomic CPU timing bridge
feed both from one instruction delta. Deterministic host fixtures cover mismatch and
error paths, same-time insertion, provisional tie ordering, chunking and repeated
NOP-to-H-IRQ advancement. The CPU remains atomic and CPU-I/O IRQ/NMI latches are not
yet wired automatically into CPU inputs. Local GCC default verification passes;
release and ASan+UBSan verification also pass. GitHub Host GCC/Clang plus pinned
PS2/PSP cross-build, artifact and checksum jobs passed for implementation commit
`f0c863d16c31140f8fd23352c9e68bffaf534c24`. Exact scope and run IDs are in
docs/master-timing-fanout-audit.md. No console runtime or hardware result is claimed.

The interrupt-line handoff milestone on 2026-10-19 adds caller-owned source
aggregation for internal timer, cartridge and expansion IRQ levels plus an NMI-edge
latch. The interrupt-aware H/V CPU bridge samples CPU-I/O before and after one atomic
step, preserving unrelated IRQ sources and transferring NMI/IRQ state into the
existing 65C816 input contract. Host fixtures cover TIMEUP acknowledgement with
cartridge IRQ retained, deassertion, post-step timer delivery, NMI vector entry and
simultaneous NMI-over-IRQ priority. Exact within-instruction sampling and complete
subsystem priority remain incomplete. Local default GCC verification passes;
release and ASan+UBSan verification also pass. GitHub Host GCC/Clang plus pinned
PS2/PSP cross-build, artifact and checksum jobs passed for implementation commit
`9ed34802ca8d436f70f0f9d8b96563184f65d3ab`. Exact scope and run IDs are in
docs/interrupt-handoff-audit.md. No console runtime or hardware result is claimed.

The Ricoh 5A22 math-register milestone on 2026-10-20 adds caller-owned `$4202-$4206`
operands/triggers and `$4214-$4217` result state. Unsigned multiply and divide commit
atomically after explicit 8- and 16-processor-cycle countdowns; division by zero
returns quotient `FFFF` and the dividend as remainder. The partial SNES bus advances
one math cycle per completed CPU bus cycle and advances aggregate internal cycles at
the existing atomic instruction boundary. Deterministic host tests cover old-result
visibility, exact completion boundaries, write-only/open-bus and read-only behavior,
division by zero, timing-overflow transactionality and an executed STA/NOP/LDA path.
Busy retrigger behavior and exact placement of currently aggregated internal cycles
remain unverified, so the broad task stays in progress. Local GCC default verification
plus release and ASan+UBSan verification pass; local LeakSanitizer is blocked by the
managed runtime's `/proc` restriction. GitHub Host GCC/Clang plus pinned PS2/PSP
cross-build, artifact and checksum jobs passed for implementation commit
`80ef8bd12755eff1ada6d442b0696d94966d8888`. Exact workflow evidence is recorded in
docs/5a22-math-register-audit.md. No console runtime or hardware result is claimed.

The automatic-joypad foundation on 2026-10-20 adds four caller-owned logical
controller inputs, snapshots them at a deterministic read start, keeps the previous
results visible while busy and commits `$4218-$421F` atomically after 4224 SNES
master clocks. The H/V component starts the operation at H=298 on the first VBlank
line for normal and overscan modes, and the shared master-clock coordinator preserves
those boundaries. Host tests cover disabled/late enable, disable while busy, four
result words, read-only writes, snapshot isolation, exact start/completion, regional
chunking and SNES-bus mirrors. Hardware research reports a phase that varies within
a window, and actual busy-time result bits are unreliable; the fixed start and
old-result visibility are documented deterministic approximations. Standard serial
`$4016/$4017` behavior is added by the following milestone; peripheral protocols and
real-console validation remain incomplete.
Local GCC default, release and ASan+UBSan verification pass; LeakSanitizer requires
disabling locally because the managed runtime blocks `/proc` task inspection. Local
Clang, PS2SDK and PSPSDK are unavailable. GitHub Host GCC/Clang release, sanitizer
and artifact jobs plus pinned PS2/PSP cross-build, artifact and checksum jobs passed
for implementation commit `3c56b6b1e3687b5eeacffebbab1ef3b1ed8a0f03`. Exact scope
and run IDs are in docs/auto-joypad-audit.md. No console runtime,
compatibility or performance result is claimed.

The manual-joypad foundation on 2026-10-20 adds standard-controller serial reads on
`$4016/$4017`. A `$4016` high-to-low strobe transition snapshots four caller-owned
16-bit lines; the two physical ports shift their paired lines independently in the
documented B-through-R order and then report caller-owned connectivity. Host tests
cover live high-strobe reads, falling-edge isolation, all bits on all four lines,
independent port clocks, post-word behavior, register return masks, SNES-bus mirrors
and deterministic coexistence with automatic polling. Auxiliary output wiring,
nonstandard peripherals and hardware contention between manual and automatic reads
remain incomplete. Local and GitHub verification evidence is recorded in
[docs/manual-joypad-audit.md](docs/manual-joypad-audit.md); no console runtime,
compatibility or performance result is claimed.

The programmable I/O-port foundation on 2026-10-20 adds `$4201` WRIO and `$4213`
RDIO. The caller owns external pin levels; reads combine them with the output latch
using open-collector wired-AND behavior. Combined bit-7 falling edges are retained
for one-shot machine handoff without making CPU-I/O depend on PPU code. Host tests
cover initialization, read/write behavior, external pulls, falling/rising/no-change
edges, acknowledgement and SNES-bus mirrors. Actual PPU H/V counter capture,
peripheral wiring, reset-versus-power retention and real-hardware validation remain
incomplete. Exact evidence is in [docs/5a22-io-port-audit.md](docs/5a22-io-port-audit.md).

The non-interlaced counter-register foundation on 2026-10-20 adds caller-owned
`$2137/$213C/$213D/$213F` state, low/high selectors, PPU2 open bus, region/version/
field status and WRIO/external-edge handoff through the optional SNES-bus adapter.
Clock-by-clock fixtures cover normal and NTSC short-line H positions, all valid H/V
values, bus mirrors and an executed atomic CPU read. The component samples the
supplied H/V timestamp; exact within-instruction capture, WRIO's one-dot relative
latency and contested held-low/repeated-latch behavior remain unverified. This adds
no renderer or PPU memory. Evidence and limitations are in
[docs/hv-counter-latch-audit.md](docs/hv-counter-latch-audit.md).

The 2026-10-20 in-callback access-clock experiment was reverted after review. It
could partially commit CPU, bus and scheduler state when event dispatch failed because
the instruction-atomic interpreter has no pause/resume contract. No timing task is
credited for that rejected design. The safe prerequisite and evidence are in
[docs/cpu-access-timing-postmortem.md](docs/cpu-access-timing-postmortem.md).

The 2026-10-23 operation-timing milestone adds an opt-in caller-owned bridge between
the complete CPU cursor and the scheduler/H/V domain. A 6/8/12-clock read/write or
6-clock internal operation can remain in flight across requested horizons and event-
budget failures; its CPU/bus effect commits exactly once only after the endpoint is
reached. Events at that endpoint are provisionally delivered before the commit.
FastROM cost, counter capacity and the pending descriptor are validated before an
effect. RESET uses an explicit mode because its two timed vector reads intentionally
leave `instruction_cycles` at zero. IRQ/NMI edges may be retained while an operation
is in flight, but CPU delivery remains at instruction boundaries. The legacy atomic
entry points retain their prior semantics. Deterministic tests cover per-clock
chunking, exact-endpoint ordering, 6/8/12 costs, budget exhaustion before a WRAM-port
write, exact-once resume, RESET, overflow/stale rejection and NMI-over-IRQ handoff.
This is operation-boundary scheduling, not electrical or bus-cycle-exact emulation;
dummy traffic, refresh, arbitration and full mid-instruction ABORT remain incomplete.
Evidence is in [docs/cpu-operation-timing-audit.md](docs/cpu-operation-timing-audit.md).

The 2026-10-23 read-only mid-instruction ABORT slice adds a timing-state pulse
after opcode fetch, a pre-fetch register checkpoint and deferred ABORT entry
after the instruction's external reads/time. The cursor refuses an in-flight
write pulse, and a later write fails closed before the effect; no MMIO rollback,
write qualification or active-state recovery is claimed. Tests cover exact
read accounting, opcode return PC, priority over NMI/IRQ and event-budget
retry. GCC host debug/O3/sanitizer suites and pinned PS2/PSP cross-builds pass;
console execution remains unavailable. Evidence and limits are in
[docs/65c816-mid-instruction-abort-slice-audit.md](docs/65c816-mid-instruction-abort-slice-audit.md).

The 2026-10-23 follow-up adds explicit per-write `EXTERNAL`/`INHIBIT`
qualification after a blocked ABORT write. The ordinary bus path is restored
after each cursor operation; an inhibited write consumes its timed bus cycle
without changing WRAM/MMIO/cartridge targets. Earlier accepted external writes
remain visible and are never rolled back. Early RMW and stack paths are covered;
late post-modify RMW pulses retain status at the pulse under WDC section 7.4's
exception. Electrical gate policy, RMW electrical phase, RTI/interrupt
exceptions and real-console measurements
remain open. Evidence is in
[docs/65c816-abort-write-qualification-audit.md](docs/65c816-abort-write-qualification-audit.md).

## Current blockers and diagnostic exceptions

No game-specific diagnostics exist. The reference bus has a deterministic unmapped
latch for tests; it is not claimed as complete SNES open-bus behavior. RESET retains
storage fields the hardware source labels uninitialized rather than claiming values.
Real consoles/emulators are not available in this session. SDK/CI availability and
remaining admin work are recorded in docs/bootstrap-audit.md and docs/github-setup.md.

## 65C816

- [x] CPU state representation
- [ ] [IN PROGRESS] Reset sequence — sourced programmer-visible invariants/vector implemented; signal and cycle sequence incomplete
- [x] Instruction fetch
- [x] Opcode dispatch — all 256 byte values have explicit behavior
- [x] Immediate addressing
- [x] Absolute addressing
- [x] Long addressing — data, ALU and JMP/JSL long forms verified
- [x] Direct Page addressing
- [x] Stack-relative addressing — complete LDA/STA/ORA/AND/EOR/CMP/ADC/SBC families
- [x] Indexed addressing
- [x] Indirect addressing — data/ALU, PEI and all indirect JMP/JSR forms verified
- [x] Branch instructions
- [x] Jump/subroutine call/return instructions
- [x] Load/store instructions — scalar families and MVP/MVN verified
- [x] Arithmetic instructions — complete ADC/SBC, compare and register/memory INC/DEC families
- [x] Logic instructions — complete ORA/AND/EOR/BIT/TSB/TRB families
- [x] Shift/rotate instructions — complete ASL/LSR/ROL/ROR accumulator and memory families
- [x] Transfer instructions
- [x] Stack instructions
- [x] Status flag behavior
- [x] M flag behavior
- [x] X flag behavior
- [x] Native mode instruction behavior
- [x] Emulation mode instruction behavior
- [x] Decimal arithmetic — all ADC/SBC forms verified at both M widths, including valid and invalid packed-BCD digits
- [x] IRQ CPU input and entry — SNES IRQ generation/timing remains Phase 4
- [x] NMI CPU input and entry — SNES NMI generation/timing remains Phase 4
- [ ] [IN PROGRESS] ABORT input and entry — boundary pulse, priority, vectors, stack/status and WAI no-wake restart behavior verified; read-only plus explicitly qualified write paths added, electrical sampling and RTI/interrupt exceptions pending
- [ ] [IN PROGRESS] RESET — vector and documented visible state subset only
- [x] BRK
- [x] COP
- [x] WAI
- [x] STP
- [x] Bank wrapping — program fetch/relative branches are bank-local; data/indexing is linear; Direct pointers remain in bank zero
- [x] Direct Page edge cases — E/DL wrap, long-pointer exception, unaligned-D carry and bank-zero wrap verified
- [ ] [IN PROGRESS] Timing model — the caller-owned operation bridge advances 6/8/12-clock accesses and 6-clock internal operations through scheduler/H/V horizons for all cursor paths including explicit RESET, WRAM refresh and HDMA priority; signal-level RESET, exact dummy/internal sequencing and electrical overlap phases are pending
- [x] CPU automated tests — all 256 opcode values and implemented behavior covered
- [ ] [IN PROGRESS] CPU regression tests — deterministic host suite exists; broader independent vectors/test ROMs pending
- [ ] [IN PROGRESS] Complete interpreter integration — opcode dispatch, all 256 resumable opcode sequences, boundary IRQ/NMI/ABORT entry, RESET vector reads and opt-in scheduler/5A22 cursor wiring exist; signal-level RESET, exact dummy cycles and full mid-instruction ABORT remain pending

### Verified 65C816 foundation subset

- [x] Caller-owned register/mode/bus/diagnostic state
- [x] Caller-owned peek/commit operation cursor for all 256 opcode values on a complete bus
- [x] Boundary `ABORT`, `NMI` and `IRQ` entry migrated to the same resumable cursor
- [x] Exact-once/stale-descriptor host tests for the resumable operation subset
- [x] Remaining 107 opcode remainders migrated from legacy-atomic execution
- [x] RESET vector/state transition migrated to the resumable cursor
- [x] Reset vector fetch from logical 00:FFFC-D
- [x] Pre-reset and invalid-bus diagnostic stops
- [x] Boundary-visible ABORT vectors, stack/status effects, priority and WAI restart-address behavior
- [x] BRK (`00`)
- [x] COP (`02`)
- [x] RTI (`40`)
- [x] WDM (`42`)
- [x] MVP (`44`)
- [x] MVN (`54`)
- [x] WAI (`CB`)
- [x] STP (`DB`)
- [x] PHP (`08`)
- [x] PHD (`0B`)
- [x] JSR absolute (`20`)
- [x] JSL absolute long (`22`)
- [x] PLP (`28`)
- [x] PLD (`2B`)
- [x] PHA (`48`)
- [x] PHK (`4B`)
- [x] PHY (`5A`)
- [x] JMP absolute long (`5C`)
- [x] RTS (`60`)
- [x] PER (`62`)
- [x] PLA (`68`)
- [x] RTL (`6B`)
- [x] JMP absolute indirect (`6C`)
- [x] PLY (`7A`)
- [x] JMP absolute-indexed-indirect (`7C`)
- [x] PHB (`8B`)
- [x] PLB (`AB`)
- [x] PEI direct (`D4`)
- [x] PHX (`DA`)
- [x] JMP absolute-indirect-long (`DC`)
- [x] PEA (`F4`)
- [x] PLX (`FA`)
- [x] JSR absolute-indexed-indirect (`FC`)
- [x] TSB direct (`04`)
- [x] ASL direct (`06`)
- [x] ASL accumulator (`0A`)
- [x] TSB absolute (`0C`)
- [x] ASL absolute (`0E`)
- [x] TRB direct (`14`)
- [x] ASL direct,X (`16`)
- [x] TRB absolute (`1C`)
- [x] ASL absolute,X (`1E`)
- [x] ROL direct (`26`)
- [x] ROL accumulator (`2A`)
- [x] ROL absolute (`2E`)
- [x] ROL direct,X (`36`)
- [x] ROL absolute,X (`3E`)
- [x] LSR direct (`46`)
- [x] LSR accumulator (`4A`)
- [x] LSR absolute (`4E`)
- [x] LSR direct,X (`56`)
- [x] LSR absolute,X (`5E`)
- [x] STZ direct (`64`)
- [x] ROR direct (`66`)
- [x] ROR accumulator (`6A`)
- [x] ROR absolute (`6E`)
- [x] STZ direct,X (`74`)
- [x] ROR direct,X (`76`)
- [x] ROR absolute,X (`7E`)
- [x] STZ absolute (`9C`)
- [x] STZ absolute,X (`9E`)
- [x] DEC direct (`C6`)
- [x] DEC absolute (`CE`)
- [x] DEC direct,X (`D6`)
- [x] DEC absolute,X (`DE`)
- [x] INC direct (`E6`)
- [x] INC absolute (`EE`)
- [x] INC direct,X (`F6`)
- [x] INC absolute,X (`FE`)
- [x] BPL (`10`)
- [x] TCS (`1B`)
- [x] NOP (`EA`)
- [x] LDA immediate (`A9`)
- [x] LDX immediate (`A2`)
- [x] LDY immediate (`A0`)
- [x] STA absolute (`8D`)
- [x] STX absolute (`8E`)
- [x] STY absolute (`8C`)
- [x] STA absolute long (`8F`)
- [x] STA absolute,Y (`99`)
- [x] STA absolute,X (`9D`)
- [x] STA absolute long,X (`9F`)
- [x] LDY absolute (`AC`)
- [x] LDA absolute (`AD`)
- [x] LDX absolute (`AE`)
- [x] LDA absolute long (`AF`)
- [x] LDA absolute,Y (`B9`)
- [x] LDY absolute,X (`BC`)
- [x] LDA absolute,X (`BD`)
- [x] LDX absolute,Y (`BE`)
- [x] LDA absolute long,X (`BF`)
- [x] CLC (`18`)
- [x] INC accumulator (`1A`)
- [x] BMI (`30`)
- [x] SEC (`38`)
- [x] DEC accumulator (`3A`)
- [x] TSC (`3B`)
- [x] REP (`C2`)
- [x] SEP (`E2`)
- [x] XCE (`FB`)
- [x] JMP absolute (`4C`)
- [x] BVC (`50`)
- [x] CLI (`58`)
- [x] TCD (`5B`)
- [x] BVS (`70`)
- [x] SEI (`78`)
- [x] TDC (`7B`)
- [x] BRA (`80`)
- [x] BRL (`82`)
- [x] STA direct-indexed-indirect (`81`)
- [x] STA stack-relative (`83`)
- [x] STY direct (`84`)
- [x] STA direct (`85`)
- [x] STX direct (`86`)
- [x] STA direct-indirect-long (`87`)
- [x] DEY (`88`)
- [x] TXA (`8A`)
- [x] BCC (`90`)
- [x] STA direct-indirect-indexed,Y (`91`)
- [x] STA direct-indirect (`92`)
- [x] STA stack-relative-indirect-indexed,Y (`93`)
- [x] STY direct,X (`94`)
- [x] STA direct,X (`95`)
- [x] STX direct,Y (`96`)
- [x] STA direct-indirect-long-indexed,Y (`97`)
- [x] TYA (`98`)
- [x] TXS (`9A`)
- [x] TXY (`9B`)
- [x] LDA direct-indexed-indirect (`A1`)
- [x] LDA stack-relative (`A3`)
- [x] LDY direct (`A4`)
- [x] LDA direct (`A5`)
- [x] LDX direct (`A6`)
- [x] LDA direct-indirect-long (`A7`)
- [x] TAY (`A8`)
- [x] TAX (`AA`)
- [x] BCS (`B0`)
- [x] LDA direct-indirect-indexed,Y (`B1`)
- [x] LDA direct-indirect (`B2`)
- [x] LDA stack-relative-indirect-indexed,Y (`B3`)
- [x] LDY direct,X (`B4`)
- [x] LDA direct,X (`B5`)
- [x] LDX direct,Y (`B6`)
- [x] LDA direct-indirect-long-indexed,Y (`B7`)
- [x] CLV (`B8`)
- [x] TSX (`BA`)
- [x] TYX (`BB`)
- [x] INY (`C8`)
- [x] DEX (`CA`)
- [x] BNE (`D0`)
- [x] CLD (`D8`)
- [x] INX (`E8`)
- [x] XBA (`EB`)
- [x] BEQ (`F0`)
- [x] SED (`F8`)
- [x] ORA direct-indexed-indirect (`01`)
- [x] ORA stack-relative (`03`)
- [x] ORA direct (`05`)
- [x] ORA direct-indirect-long (`07`)
- [x] ORA immediate (`09`)
- [x] ORA absolute (`0D`)
- [x] ORA absolute long (`0F`)
- [x] ORA direct-indirect-indexed,Y (`11`)
- [x] ORA direct-indirect (`12`)
- [x] ORA stack-relative-indirect-indexed,Y (`13`)
- [x] ORA direct,X (`15`)
- [x] ORA direct-indirect-long-indexed,Y (`17`)
- [x] ORA absolute,Y (`19`)
- [x] ORA absolute,X (`1D`)
- [x] ORA absolute long,X (`1F`)
- [x] AND direct-indexed-indirect (`21`)
- [x] AND stack-relative (`23`)
- [x] BIT direct (`24`)
- [x] AND direct (`25`)
- [x] AND direct-indirect-long (`27`)
- [x] AND immediate (`29`)
- [x] BIT absolute (`2C`)
- [x] AND absolute (`2D`)
- [x] AND absolute long (`2F`)
- [x] AND direct-indirect-indexed,Y (`31`)
- [x] AND direct-indirect (`32`)
- [x] AND stack-relative-indirect-indexed,Y (`33`)
- [x] BIT direct,X (`34`)
- [x] AND direct,X (`35`)
- [x] AND direct-indirect-long-indexed,Y (`37`)
- [x] AND absolute,Y (`39`)
- [x] BIT absolute,X (`3C`)
- [x] AND absolute,X (`3D`)
- [x] AND absolute long,X (`3F`)
- [x] EOR direct-indexed-indirect (`41`)
- [x] EOR stack-relative (`43`)
- [x] EOR direct (`45`)
- [x] EOR direct-indirect-long (`47`)
- [x] EOR immediate (`49`)
- [x] EOR absolute (`4D`)
- [x] EOR absolute long (`4F`)
- [x] EOR direct-indirect-indexed,Y (`51`)
- [x] EOR direct-indirect (`52`)
- [x] EOR stack-relative-indirect-indexed,Y (`53`)
- [x] EOR direct,X (`55`)
- [x] EOR direct-indirect-long-indexed,Y (`57`)
- [x] EOR absolute,Y (`59`)
- [x] EOR absolute,X (`5D`)
- [x] EOR absolute long,X (`5F`)
- [x] ADC direct-indexed-indirect, binary and decimal modes (`61`)
- [x] ADC stack-relative, binary and decimal modes (`63`)
- [x] ADC direct, binary and decimal modes (`65`)
- [x] ADC direct-indirect-long, binary and decimal modes (`67`)
- [x] ADC immediate, binary and decimal modes (`69`)
- [x] ADC absolute, binary and decimal modes (`6D`)
- [x] ADC absolute long, binary and decimal modes (`6F`)
- [x] ADC direct-indirect-indexed,Y, binary and decimal modes (`71`)
- [x] ADC direct-indirect, binary and decimal modes (`72`)
- [x] ADC stack-relative-indirect-indexed,Y, binary and decimal modes (`73`)
- [x] ADC direct,X, binary and decimal modes (`75`)
- [x] ADC direct-indirect-long-indexed,Y, binary and decimal modes (`77`)
- [x] ADC absolute,Y, binary and decimal modes (`79`)
- [x] ADC absolute,X, binary and decimal modes (`7D`)
- [x] ADC absolute long,X, binary and decimal modes (`7F`)
- [x] BIT immediate (`89`)
- [x] CPY immediate (`C0`)
- [x] CMP direct-indexed-indirect (`C1`)
- [x] CMP stack-relative (`C3`)
- [x] CPY direct (`C4`)
- [x] CMP direct (`C5`)
- [x] CMP direct-indirect-long (`C7`)
- [x] CMP immediate (`C9`)
- [x] CPY absolute (`CC`)
- [x] CMP absolute (`CD`)
- [x] CMP absolute long (`CF`)
- [x] CMP direct-indirect-indexed,Y (`D1`)
- [x] CMP direct-indirect (`D2`)
- [x] CMP stack-relative-indirect-indexed,Y (`D3`)
- [x] CMP direct,X (`D5`)
- [x] CMP direct-indirect-long-indexed,Y (`D7`)
- [x] CMP absolute,Y (`D9`)
- [x] CMP absolute,X (`DD`)
- [x] CMP absolute long,X (`DF`)
- [x] CPX immediate (`E0`)
- [x] SBC direct-indexed-indirect, binary and decimal modes (`E1`)
- [x] SBC stack-relative, binary and decimal modes (`E3`)
- [x] CPX direct (`E4`)
- [x] SBC direct, binary and decimal modes (`E5`)
- [x] SBC direct-indirect-long, binary and decimal modes (`E7`)
- [x] SBC immediate, binary and decimal modes (`E9`)
- [x] CPX absolute (`EC`)
- [x] SBC absolute, binary and decimal modes (`ED`)
- [x] SBC absolute long, binary and decimal modes (`EF`)
- [x] SBC direct-indirect-indexed,Y, binary and decimal modes (`F1`)
- [x] SBC direct-indirect, binary and decimal modes (`F2`)
- [x] SBC stack-relative-indirect-indexed,Y, binary and decimal modes (`F3`)
- [x] SBC direct,X, binary and decimal modes (`F5`)
- [x] SBC direct-indirect-long-indexed,Y, binary and decimal modes (`F7`)
- [x] SBC absolute,Y, binary and decimal modes (`F9`)
- [x] SBC absolute,X, binary and decimal modes (`FD`)
- [x] SBC absolute long,X, binary and decimal modes (`FF`)
- [x] Base-cycle accounting for the implemented subset
- [x] Linear 24-bit word access across bank boundaries
- [x] Absolute indexed page/write/X cycle rule for implemented opcodes
- [x] Direct Page bank-zero address formation for implemented opcodes
- [x] Emulation-mode aligned Direct Page wrapping for implemented opcodes
- [x] Direct Page DL and width cycle additions for implemented opcodes
- [x] Regular 16-bit Direct-indirect pointer wrapping for implemented opcodes
- [x] Direct-indirect-long linear pointer-byte exception for implemented opcodes
- [x] Indirect DBR versus long-pointer data-bank selection for implemented opcodes
- [x] Direct-indirect pre/post-index and cycle rules for implemented opcodes
- [x] Full-source and destination-selected M/X width for implemented transfers
- [x] Mixed M/X transfer preservation, truncation and zero extension
- [x] Register-transfer N/Z flags and two-cycle timing
- [x] Fixed-width C/D/S transfer behavior and 16-bit N/Z
- [x] Native/emulation TCS/TXS stack-high behavior
- [x] XBA byte exchange, 8-bit N/Z and three-cycle timing
- [x] Conditional branch taken/not-taken flag selection and status preservation
- [x] Signed 8-bit and 16-bit relative offsets from the advanced PC
- [x] Relative and relative-long PC wrapping within unchanged PBR
- [x] Conditional/BRA taken and emulation-only page-cross cycle additions
- [x] BRL fixed four-cycle timing
- [x] Implied D/I/V status-control isolation and two-cycle timing
- [x] Accumulator/index increment-decrement width, wrap, N/Z isolation and two-cycle timing
- [x] Base ORA/AND/EOR selected-width result, N/Z and B-preservation behavior
- [x] Non-stack ORA/AND indexed, indirect and long address/cycle behavior
- [x] Base CMP and complete CPX/CPY selected-width N/Z/C behavior
- [x] Non-stack EOR/CMP indexed, indirect and long address/cycle behavior
- [x] Stack-relative ORA/AND/EOR/CMP bank-zero address, pointer, DBR/Y and cycle behavior
- [x] Stack-relative LDA/STA bank-zero data wrap, pointer, DBR/Y, flag and cycle behavior
- [x] Binary immediate ADC/SBC carry, overflow, N/Z, B preservation and width behavior
- [x] Binary Direct Page/absolute ADC/SBC address, bank-wrap, flag, width and cycle behavior
- [x] Binary Direct-X/absolute-X/Y ADC/SBC address, page/index timing, bank-carry, flag, width and cycle behavior
- [x] Binary stack-relative ADC/SBC bank-zero data/pointer wrap, DBR/Y carry, flags, width and cycle behavior
- [x] Binary regular Direct-indirect ADC/SBC pointer wrap, DBR, pre/post-index, flag, width and cycle behavior
- [x] Binary long ADC/SBC pointer-bank, bank-zero wrap, 24-bit carry, flag, width and cycle behavior
- [x] Decimal ADC/SBC packed-BCD correction, C/V/N/Z, width, addressing and cycle behavior
- [x] Complete BIT selected-width Z, memory N/V and immediate N/V-preservation behavior
- [x] Complete ASL/LSR/ROL/ROR accumulator width, B preservation, C/N/Z and two-cycle behavior
- [x] Complete ASL/LSR/ROL/ROR memory Direct/Direct-X/absolute/absolute-X address, width, flag and cycle behavior
- [x] Complete memory INC/DEC wrap, N/Z isolation, address, width and cycle behavior
- [x] Complete TSB/TRB memory update, original-operand Z, address, width and cycle behavior
- [x] Complete STZ Direct/Direct-X/absolute/absolute-X width, address, flag-preservation and cycle behavior
- [x] Complete stack push/pull/effective-address instruction widths, byte order, flags and aggregate cycles
- [x] Ordinary emulation stack-page wrapping and documented linear multi-byte stack exceptions
- [x] Jump/subroutine target, return-address, PC/PBR and indirect-table bank behavior
- [x] JSL and JSR absolute-indexed-indirect meaningful bus callback order/count

## Bus

- [x] WRAM storage ownership and mapping — caller supplies verified 128 KiB storage
- [x] Logical 24-bit access integration
- [x] Low-bank WRAM mirrors
- [ ] ROM fast path
- [ ] [IN PROGRESS] SRAM windows — base LoROM/HiROM power-of-two mappings exist; broader boards absent
- [x] PPU MMIO routing — optional counter-register state with callback fallback; broader PPU registers remain unimplemented
- [x] CPU I/O routing — callback route plus bus-owned `$420D`; CPU-I/O registers remain incomplete
- [x] APU port routing — default machine-owned directional mailbox with explicit callback override
- [x] DMA register routing — callback route in the original bus foundation; shared channel register routing was added later
- [ ] Enhancement windows
- [ ] [IN PROGRESS] Open bus behavior — deterministic single-latch fallback only; hardware domains unverified
- [ ] [IN PROGRESS] Read side effects — `$2180` auto-increment exists; remaining SNES register effects absent
- [ ] [IN PROGRESS] Write side effects — WRAM ports and `$420D` exist; remaining SNES register effects absent
- [ ] [IN PROGRESS] Access timing — meaningful cursor accesses use 5A22 6/8/12 clocks, internal operations use 6 clocks and H/V-aware CPU/DMA/HDMA phases include shared WRAM refresh stalls; dummy traffic and electrical arbitration remain open
- [ ] [IN PROGRESS] Mapping boundaries and mirror tests — WRAM/MMIO, base LoROM/HiROM and Mode 25/35 ExHiROM boundaries verified; padded/ExLoROM/enhancement maps absent
- [ ] Page/block mapping benchmark

### Verified logical-bus foundation subset

- [x] Fixed-capacity caller-owned region table
- [x] Direct read-write and read-only regions
- [x] Separate MMIO callback slow path
- [x] Range, overlap and capacity validation
- [x] Deterministic unmapped fixture behavior
- [x] Partial SNES-map WRAM, mirror, port, MMIO-route and FastROM fixtures
- [x] Atomic compatibility and resumable CPU-operation/scheduler timing integration fixtures
- [x] Cartridge no-response preserves deterministic bus latch

## Cartridge

- [ ] [IN PROGRESS] ROM loading — core construction plus explicit-path PS2 file adapter verified on host; device setup/selection and PSP adapter absent
- [ ] [IN PROGRESS] ROM metadata — standard fields, registration prefix and conservative mapper evidence parsed; special variants/hardware resolution absent
- [ ] [IN PROGRESS] Header analysis — conservative LoROM/HiROM/ExHiROM selection verified; special variants absent
- [ ] [IN PROGRESS] LoROM — power-of-two and unambiguous two-region base ROM mapping, base SRAM and conservative detection verified; broader boards absent
- [ ] [IN PROGRESS] HiROM — power-of-two and unambiguous two-region base ROM mapping, base SRAM and conservative detection verified; broader boards absent
- [ ] ExLoROM — unofficial mapping remains research-only; no commercial-library requirement established
- [ ] [IN PROGRESS] ExHiROM — Mode 25/35 ROM/SRAM mapping and conservative selection verified; board/runtime validation absent
- [ ] [IN PROGRESS] SRAM — caller-owned mapper windows/read/write and revisioned dirty tracking verified; broader boards absent
- [ ] [IN PROGRESS] SRAM persistence — shared handoff exists; platform file backends and target validation absent
- [ ] [IN PROGRESS] Mapper tests — synthetic base/irregular/ExHiROM maps, checksum, selection, ambiguity and bus/CPU integration covered; padded/ExLoROM/enhancement execution absent
- [ ] [IN PROGRESS] Checksum handling where useful — mirrored base-image checksum verified; special-board checksum variants absent
- [ ] [IN PROGRESS] Truncated and malformed input rejection — mapper config, image granularity and candidate bounds covered; file I/O absent
- [x] Copier-header policy — explicit detect/reject/require for a single 512-byte prefix; other layouts rejected
- [ ] Region detection

### Verified cartridge foundation subset

- [x] Caller-owned read-only ROM and writable SRAM views
- [x] Cartridge slot mapped/unmapped response contract
- [x] Explicit base LoROM 32 KiB-bank wiring for supported sizes
- [x] Explicit base HiROM 64 KiB-bank wiring for supported sizes
- [x] Standard base LoROM/HiROM SRAM windows for supported sizes
- [x] ROM write protection and deterministic mirror fixtures
- [x] Cartridge/SNES-bus/CPU/scheduler integration fixture
- [x] Heap-free clean/copier-prefixed image view with source-offset retention
- [x] Explicit LoROM/HiROM internal-header candidate parsing
- [x] Raw title/map/chip/size/destination/developer/version/checksum/vector extraction
- [x] Overflow-safe declared ROM/SRAM size decoding
- [x] Checksum/complement relationship and base map-mode classification
- [x] Malformed image-size and out-of-range candidate rejection
- [x] Whole-payload checksum with candidate fields normalized and copier prefix excluded
- [x] Unambiguous two-region power-of-two checksum mirroring
- [x] Conservative base LoROM/HiROM evidence gates
- [x] Explicit no-confident-header and ambiguous-header results
- [x] Selection independent of title text and guessed reset opcode
- [x] Larger-first/smaller-tail base ROM mirroring for unambiguous layouts
- [x] Mapper-bank alignment and padding-dependent layout rejection
- [x] Header-selection to irregular-mapper initialization fixture
- [x] Raw 16-byte registration-prefix parsing without ASCII/title scoring
- [x] Mode 25/35 ExHiROM header classification at `$40FFC0`
- [x] ExHiROM first/extended region and upper-half mirror mapping
- [x] ExHiROM 6 MiB tail repetition and standard HiROM-style SRAM windows
- [x] Generic three-candidate selection with explicit ambiguity/no-match
- [x] Mapper executable-size/alignment confidence gate
- [x] Platform-neutral image-to-cartridge plan and caller-owned SRAM construction
- [x] Revisioned SRAM dirty-state and exact-acknowledgement handoff

Checksum/selection scope and local verification are recorded in
[cartridge-mapper-selection-audit.md](docs/cartridge-mapper-selection-audit.md).
Executable irregular-map scope and verification are recorded in
[cartridge-irregular-mapping-audit.md](docs/cartridge-irregular-mapping-audit.md).
Special checksum conventions, padded and board-specific irregular layouts, unofficial
ExLoROM and enhancement hardware remain unchecked.
Mode 25/35 ExHiROM scope and verification are recorded in
[cartridge-exhirom-audit.md](docs/cartridge-exhirom-audit.md). Unofficial ExLoROM,
special boards and enhancement hardware remain unchecked.
Preparation/construction scope and verification are recorded in
[cartridge-construction-audit.md](docs/cartridge-construction-audit.md). Platform file
I/O and allocation policy remain unchecked. Shared SRAM handoff scope is recorded in
[cartridge-sram-persistence-audit.md](docs/cartridge-sram-persistence-audit.md);
platform persistence remains incomplete.

## Scheduler

- [x] Master timestamp units — unsigned 64-bit SNES master clocks
- [x] Clock ratio representation — exact reduced rational with carried remainder
- [x] Event tie ordering — timestamp, explicit priority, insertion token
- [x] CPU event horizon — caller-owned in-flight operation state pauses at requested horizons or event-budget failures and resumes without restarting or duplicating the CPU/bus effect
- [x] Shared scheduler/H/V master-clock fanout — checked alignment, transactional H/V slices and provisional same-time ordering verified
- [ ] PPU event integration
- [ ] SPC700 synchronization
- [ ] DSP synchronization
- [ ] DMA bus ownership
- [ ] Enhancement scheduling
- [x] Determinism tests — identical scheduler replay traces verified
- [ ] [IN PROGRESS] Long-run clock drift tests — chunked ratio, H/V and short common-domain advancement verified; regional soak tests pending

## Interrupts and timers

- [ ] [IN PROGRESS] CPU I/O registers — interrupt/timer, math, `$4201/$4213` programmable-I/O and controller subsets implemented
- [ ] [IN PROGRESS] H/V counter model — non-interlaced phase, long-dot projection and counter-register subset verified; interlace and exact capture timing absent
- [ ] [IN PROGRESS] IRQ compare behavior — stable-register non-interlaced H/V matches and operation-boundary CPU delivery exist; exact write/match windows, interlace and electrical sampling incomplete
- [ ] [IN PROGRESS] NMI edge and enable behavior — gated edge/latch, non-interlaced VBlank timing and operation-boundary delivery exist; exact electrical sampling incomplete
- [ ] [IN PROGRESS] Interrupt acknowledgement — base RDNMI/TIMEUP reads exist; match-window/external IRQ cases absent
- [ ] [IN PROGRESS] Interrupt ordering — CPU NMI-over-IRQ priority and source aggregation verified; simultaneous subsystem timing pending
- [ ] [IN PROGRESS] CPU math register timing — 8/16-cycle completion and result semantics verified; busy collisions and exact internal-cycle placement pending
- [ ] [IN PROGRESS] Automatic joypad polling timing — 4224-clock window and result commit verified with a fixed deterministic start; hardware phase variation, busy-time partial data and manual-port contention pending
- [ ] SPC700 timer coordination
- [ ] [IN PROGRESS] Interrupt and timer integration tests — register/latch, bus/H/V timing, atomic compatibility and resumable operation-boundary input fixtures exist; exact electrical cases pending

### Verified CPU-I/O foundation subset

- [x] NMITIMEN enable/mode mask and disabled-IRQ acknowledgement
- [x] 9-bit HTIME/VTIME register storage
- [x] RDNMI/TIMEUP/HVBJOY read composition and side effects
- [x] NMI gated-edge and timer-IRQ latch handoff
- [x] Partial SNES-bus CPU-I/O state routing
- [x] Unsigned 8-bit multiply with 8-processor-cycle completion
- [x] Unsigned 16-by-8 divide with 16-processor-cycle completion
- [x] Divide-by-zero quotient/remainder behavior
- [x] Math-result read boundary and CPU-bus cycle integration
- [x] Four logical controller snapshots and `$4218-$421F` result registers
- [x] VBlank auto-joypad busy window and atomic completion
- [x] Standard manual joypad strobe and four serial data lines on `$4016/$4017`
- [x] Independent manual port clocks, 16-bit order and connected padding
- [x] WRIO/RDIO programmable I/O latch and wired-AND external levels
- [x] Programmable I/O bit-7 falling-edge handoff
- [ ] [IN PROGRESS] PPU H/V counter-latch consumption and register behavior — deterministic adapter exists; exact capture timing and held-low cases pending
- [ ] Auxiliary controller-output wiring and nonstandard peripheral protocols
- [ ] Busy-operation collision/retrigger hardware behavior

### Verified H/V foundation subset

- [x] Caller-owned non-interlaced NTSC/PAL master-clock state
- [x] HBlank and 224/239-line VBlank transition timing
- [x] Separate VBlank and delayed RDNMI/NMI-flag signals
- [x] NTSC field alternation and 1360-clock short scanline
- [x] PAL 312-line non-interlaced frame geometry
- [x] One-shot/chunked H/V advancement determinism
- [x] Normal-line long-dot and NTSC short-line PPU H projection
- [x] Counter low/high selectors and PPU2 open-bus subset
- [x] Counter field/region/version status and selector reset
- [x] Optional WRIO/external-edge to counter-register bus adapter

### H/V timer-IRQ foundation subset

- [x] H-IRQ match formula for every HTIME 0-339
- [x] HTIME 338/339 following-line spill
- [x] V-IRQ and HV-IRQ stable-register positions
- [x] Regional compare-range and H=153 exception behavior
- [x] TIMEUP latch/acknowledgement handoff from H/V timing

### Shared master-clock domain subset

- [x] Checked scheduler/H/V timestamp alignment and coordinated advancement
- [x] Deterministic H/V-before-general-callback ordering at equal timestamps
- [x] Event-budget exhaustion preserves aligned domain state
- [x] Atomic CPU-step delta advances scheduler and H/V together
- [x] Atomic CPU-I/O IRQ/NMI delivery into 65C816 inputs
- [x] Caller-owned CPU-operation state advances scheduler/H/V through requested horizons and event-budget retries with exact-once commit
- [ ] [IN PROGRESS] Resumable CPU cycle/micro-operation contract — caller-owned exact-once foundation and scheduler wiring are verified for all 256 opcode values, boundary IRQ/NMI/ABORT entry, explicit write-qualified ABORT subset and RESET; signal-level RESET, dummy cycles and full electrical ABORT remain pending

### Interrupt-line aggregation subset

- [x] Caller-owned independent IRQ source bitmask
- [x] CPU-I/O timer source update preserves cartridge/expansion IRQ sources
- [x] CPU-I/O NMI edge transfers once into the CPU input latch
- [x] Pre/post atomic-step input sampling contract
- [x] Resumable-operation IRQ/NMI retention with CPU delivery only at instruction boundaries
- [x] Simultaneous NMI/IRQ follows existing CPU NMI priority
- [ ] Exact within-instruction interrupt sampling

## DMA / HDMA

- [ ] [IN PROGRESS] DMA registers — channel banks, `$420B` start and `$420C` timed HDMA enable integrated; electrical overlap pending
- [ ] [IN PROGRESS] DMA transfer modes — timed general DMA and direct/indirect HDMA; electrical edge cases pending
- [ ] [IN PROGRESS] DMA timing — grace cycle, slots, setup, CPU resynchronization, HDMA priority and shared WRAM refresh; electrical phases pending
- [x] DMA source/destination wrapping — A1T wraps within fixed A1B; BBAD mode offsets wrap within `$21xx`
- [ ] [IN PROGRESS] HDMA initialization — frame request, table engine and scheduled VBlank deactivation implemented; revision edges pending
- [ ] [IN PROGRESS] HDMA scanline behavior — HBlank request and timed line engine implemented; electrical phase pending
- [ ] [IN PROGRESS] Indirect HDMA — timed pointer/table behavior and last-channel zero-descriptor shortening implemented; frame-initialization and electrical edges pending
- [ ] Transfer edge cases
- [ ] CPU interaction
- [x] Channel priority — lowest selected DMA/HDMA channel first with deterministic multi-channel fixtures
- [ ] [IN PROGRESS] DMA and HDMA overlap — same-channel cancellation at admitted slot boundaries implemented; electrical phases pending
- [ ] Tests

## PPU

- [ ] [IN PROGRESS] PPU register model — H/V counter, VRAM, CGRAM, OAM and Mode 0/1 BG register subsets
- [ ] [IN PROGRESS] VRAM — caller-owned bytes and CPU read/write ports; access windows and rendering absent
- [ ] [IN PROGRESS] CGRAM — 512 caller-owned bytes, CPU read/write ports and Mode 0 color lookup; access windows and video output absent
- [ ] [IN PROGRESS] OAM — 544 caller-owned bytes, CPU ports, VBlank pointer reload, sprite decode, rotation, scheduled scanline selection and 4bpp OBJ pixel line; final display absent
- [ ] [IN PROGRESS] Tile decode fixtures — 4bpp OBJ and pure 2bpp BG pixel fixtures tested; other PPU formats pending
- [ ] [IN PROGRESS] BG Mode 0 — four register-driven 2bpp lines, OBJ priority mixer, main/sub layer windows, CGRAM lookup, color math and color windows tested; output and timing validation absent
- [ ] [IN PROGRESS] BG Mode 1 — reference BG/OBJ scanline and color path; display and timing validation absent
- [ ] BG Mode 2
- [ ] BG Mode 3
- [ ] BG Mode 4
- [ ] BG Mode 5
- [ ] BG Mode 6
- [ ] Mode 7
- [ ] Sprites
- [ ] Sprite priorities
- [ ] BG priorities
- [ ] Window system
- [ ] Color math
- [ ] Brightness
- [ ] Mosaic
- [ ] Scrolling
- [ ] Offset-per-tile
- [ ] Hires
- [ ] Interlace
- [ ] Overscan
- [ ] [IN PROGRESS] Relevant latching behavior — deterministic H/V subset; exact access timing pending
- [ ] [IN PROGRESS] Relevant open-bus behavior — counter and CGRAM high-read PPU2 subset
- [ ] Scanline integration
- [ ] [IN PROGRESS] VRAM port increment/remap behavior — read/write paths host-verified; access-window/mixed-port behavior pending
- [ ] [IN PROGRESS] Sprite limits and overflow flags — 32 OBJ/34 sliver selection, scheduled sticky `$213E` and OBJ pixel line host tested; exact hardware phase absent
- [ ] Mid-frame register effects
- [ ] [IN PROGRESS] PPU tests — H/V counter, VRAM, CGRAM, OAM, timed OBJ status, 4bpp OBJ and pure 2bpp BG fixtures
- [ ] Reference renderer integration

## SPC700

- [x] Register state — caller-owned A/X/Y/SP/PSW/PC with no invented reset values
- [x] Instruction fetch — bounded S-SMP-map fetch, IPL overlay and 16-bit PC wrap verified
- [ ] [IN PROGRESS] Opcode dispatch — thirty implied/PSW/register/direct-page MOV opcodes execute; 226 values fail explicitly
- [ ] [IN PROGRESS] Addressing modes — implied/register, immediate, direct-page and indexed-direct-page forms verified; indirect/absolute forms absent
- [ ] [IN PROGRESS] Load/store — A/X/Y immediate/register, direct-page and indexed-direct-page MOV subset verified
- [ ] [IN PROGRESS] Arithmetic and flags — eight PSW-control operations verified; arithmetic absent
- [ ] Decimal adjust
- [ ] Logic and shifts
- [ ] Branches and calls
- [ ] Stack behavior
- [ ] Bit operations
- [ ] Multiply/divide
- [ ] Sleep and stop
- [ ] [IN PROGRESS] Instruction timing — implemented 2/3/4/5-cycle costs feed timers atomically; bus phases absent
- [ ] [IN PROGRESS] RAM and IPL mapping — bounded ARAM and optional IPL overlay verified; rest of S-SMP map/runtime pending
- [ ] IPL provenance decision
- [ ] [IN PROGRESS] I/O registers — `$F1/$F4-$FF` partial route exists; TEST and DSP registers remain
- [ ] [IN PROGRESS] Timers — three-stage 128/16-cycle counters, targets, enable edges and read-clear outputs verified with explicit S-SMP cycles; synchronized clock domain and electrical races pending
- [ ] [IN PROGRESS] Communication ports — four directional latches each way, CPU mirrors, S-SMP `$F4-$F7` and `$F1` clear verified; cross-clock timing pending
- [ ] [IN PROGRESS] Instruction tests — all 256 dispatch values, PSW controls and register/immediate/direct-page/indexed MOV covered
- [ ] [IN PROGRESS] Timing regressions — instruction/timer coupling, overflow and unavailable operand/target cases covered

## DSP

- [ ] Register state
- [ ] Eight voice state
- [ ] BRR decode
- [ ] BRR loop/end behavior
- [ ] Pitch progression
- [ ] Interpolation
- [ ] ADSR
- [ ] GAIN envelopes
- [ ] Key-on/key-off
- [ ] Noise generator
- [ ] Pitch modulation
- [ ] Voice volumes
- [ ] Main volumes
- [ ] Echo buffer
- [ ] FIR filter
- [ ] Feedback
- [ ] Clamping and signed arithmetic
- [ ] Sample timing
- [ ] DSP register side effects
- [ ] Golden sample tests

## APU and audio

- [ ] SPC700/DSP interconnect
- [ ] [IN PROGRESS] CPU/APU port synchronization — shared mailbox ownership exists; asynchronous clocks and race ordering pending
- [ ] Timer integration
- [ ] Sample generation pipeline
- [ ] Deterministic sample buffering
- [ ] Output sample format contract
- [ ] Platform-independent PCM output interface
- [ ] Resampling policy
- [ ] Underflow/overflow behavior
- [ ] Host offline audio tests
- [ ] [IN PROGRESS] Integrated APU regression tests — port, local memory-map and machine-route fixtures exist; executable S-SMP/DSP tests pending

## PS2 backend

- [x] On-screen five-case core diagnostic compiles with the pinned PS2SDK and passes its host baseline
- [x] User-captured ARMSX2 and NetherSX2 Turnip v1 diagnostic screens show five PASS results
- [ ] [IN PROGRESS] Record ARMSX2 version/settings and optional on-device ELF checksum for reproducibility
- [ ] SDK entry lifecycle runtime validation
- [ ] GS framebuffer presentation
- [ ] Scaling and aspect policy
- [ ] Video buffers and ownership
- [ ] GIF packet lifetime
- [ ] DMA cache coherency
- [ ] Pad input
- [ ] [IN PROGRESS] Storage and ROM selection — explicit PS2 path adapter exists; mount lifecycle and picker absent
- [ ] SRAM persistence
- [ ] SPU2 output
- [ ] Audio buffering
- [ ] Pin a reviewed RFAuds2 version/revision
- [ ] PS2-only RFAuds2 adapter behind platform audio interface
- [ ] RFAuds2 asynchronous accepted-tail/backpressure handling
- [ ] RFAuds2 FAT/Slim real-hardware and long-duration validation
- [ ] IOP service initialization
- [ ] Error reporting
- [ ] Memory budget enforcement
- [ ] Real PS2 boot test
- [ ] Real PS2 integrated runtime test

## PSP backend

- [ ] SDK entry lifecycle runtime validation
- [ ] GU framebuffer presentation
- [ ] Scaling and aspect policy
- [ ] Video buffers and ownership
- [ ] Cache coherency
- [ ] Controller input
- [ ] Storage and ROM selection
- [ ] SRAM persistence
- [ ] PSP audio output
- [ ] Audio buffering
- [ ] Exit and suspend lifecycle
- [ ] Clock policy
- [ ] Memory budget enforcement
- [ ] Real PSP boot test
- [ ] Real PSP integrated runtime test

## Host testing

- [x] CPU foundation unit harness
- [x] Reference bus unit tests
- [ ] Mapper fixtures
- [ ] PPU logic fixtures
- [ ] SPC700 fixtures
- [ ] DSP sample fixtures
- [x] Scheduler fixtures — event order/error paths, rational clocks, 5A22 classes/math and joypad timing, shared H/V fanout, IRQ-source aggregation and timed CPU boundaries
- [ ] DMA/HDMA fixtures
- [ ] Licensed test-ROM runner
- [ ] Deterministic replay harness
- [ ] Sanitizer regression coverage
- [ ] Malformed ROM fuzzing
- [ ] Serialization tests
- [ ] Reference/optimized differential harness

## Debugging

- [ ] CPU trace
- [ ] Register dump
- [ ] Memory trace
- [ ] Breakpoints
- [ ] Watchpoints
- [ ] PPU state dump
- [ ] Scanline log
- [ ] APU trace
- [ ] Scheduler events
- [ ] DMA trace
- [ ] Deterministic replay
- [ ] Trace filtering
- [ ] Release trace overhead verification

## Save states

- [ ] State stability review
- [ ] CPU serialization
- [ ] Bus serialization
- [ ] PPU serialization
- [ ] APU serialization
- [ ] Cartridge serialization
- [ ] Enhancement-chip serialization
- [ ] Scheduler serialization
- [ ] Versioned format
- [ ] Compatibility policy
- [ ] Malformed state rejection
- [ ] Regression tests

## PS2 optimization

- [ ] Real PS2 baseline profiling
- [ ] Identify hotspots
- [ ] EE optimization
- [ ] Memory layout review
- [ ] Cache review
- [ ] Scratchpad candidates
- [ ] MMI candidates
- [ ] DMA opportunities
- [ ] GS presentation optimization
- [ ] VU feasibility review
- [ ] VIF transfer cost review
- [ ] IOP offload feasibility
- [ ] Audio optimization
- [ ] Regression tests
- [ ] Real hardware benchmarks

## PSP optimization

- [ ] Real PSP baseline profiling
- [ ] Allegrex hotspot review
- [ ] Cache review
- [ ] Memory layout review
- [ ] MIPS optimization
- [ ] VFPU candidates
- [ ] GU optimization
- [ ] Audio optimization
- [ ] Buffering latency review
- [ ] Regression tests
- [ ] Real hardware benchmarks

## Compatibility campaign

- [ ] Legal test corpus inventory
- [ ] Region coverage matrix
- [ ] Base SNES integration suite
- [ ] Controller and input validation
- [ ] Commercial-game reproduction protocol
- [ ] PS2 compatibility reports
- [ ] PSP compatibility reports
- [ ] Hardware bug regression workflow validation
- [ ] Long-play stability
- [ ] Enhancement-chip game validation
- [ ] No-hack audit

## Performance validation

- [ ] PS2 timer calibration
- [ ] PSP timer calibration
- [ ] Repeatable input workloads
- [ ] Frame-time distribution reporting
- [ ] Memory high-water reporting
- [ ] Audio underrun measurement
- [ ] Real PS2 sustained full-speed validation
- [ ] Real PSP sustained full-speed validation
- [ ] Separate emulator and hardware records

## Possible dynarec

- [ ] Measure interpreter cost on real PS2
- [ ] Measure interpreter cost on real PSP
- [ ] Determine if dynarec is justified
- [ ] Research invalidation requirements
- [ ] Design IR if justified
- [ ] Design code cache
- [ ] PS2 MIPS emitter
- [ ] PSP MIPS emitter
- [ ] Correctness tests
- [ ] Interpreter fallback
- [ ] Performance validation

## Super FX

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Star Fox validation
- [ ] Yoshi's Island validation

## SA-1

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] 65C816 integration
- [ ] Internal RAM and bitmap modes
- [ ] DMA and arithmetic units
- [ ] Super Mario RPG validation

## DSP-1

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Command protocol
- [ ] Fixed-point arithmetic
- [ ] Reference vectors

## DSP-2

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Command protocol
- [ ] Fixed-point arithmetic
- [ ] Reference vectors

## DSP-3

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Command protocol
- [ ] Fixed-point arithmetic
- [ ] Reference vectors

## DSP-4

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Command protocol
- [ ] Fixed-point arithmetic
- [ ] Reference vectors

## CX4

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed

## S-DD1

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Decompression vectors
- [ ] Streaming boundaries

## SPC7110

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
- [ ] Decompression vectors
- [ ] Streaming boundaries
- [ ] RTC behavior and persistence

## OBC-1

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed

## Other commercially relevant hardware

- [ ] Architecture research
- [ ] Register state
- [ ] Command/instruction execution
- [ ] Memory mapping
- [ ] Bus integration
- [ ] Timing integration
- [ ] Interrupt/signaling behavior research
- [ ] Tests
- [ ] PS2 profiling
- [ ] PSP profiling
- [ ] Target-specific optimization if needed
