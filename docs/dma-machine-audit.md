# CPU-triggered general-purpose DMA integration

`$420B` writes in system banks now queue a channel mask when shared DMA
registers are installed. The resumable CPU bridge allows one further timed
CPU operation, then pauses before the next timed operation. Zero-time CPU
control operations do not consume that grace cycle. The queued transfer can
therefore span an instruction boundary: `STA $420B; NOP` fetches NOP before
the pause. Writes of zero queue nothing. Machine power-on clears requests
and paused DMA state along with the CPU cursor.

## Bus effects

`mipsnes2_snes_bus_dma_byte` performs one A/B data phase without charging CPU
accesses, CPU master clocks or CPU arithmetic cycles. The timing bridge
invokes the existing DMA cursor for exactly one byte at each admitted data
endpoint. This preserves channel order, A1T/DAS updates and mode position.
Both directions reach the existing WRAM, cartridge and PPU/APU routes.

In system banks, A-bus accesses to `$2100-$21FF`, `$420B/$420C` and
`$4300-$437F` are blocked. Reads use the existing deterministic open-bus
latch; writes have no register effect. A WRAM address on bus A suppresses
bus-B `$2180-$2183` access and its address increment. Reverse conflicting
`$2180` reads supply the fallback latch to the A destination. Cartridge
banks whose low offsets resemble I/O are not blocked by these system-bank
rules. Electrical open-bus decay and chip revisions remain unverified.

## Timing and explicit limits

The reference bridge implements Anomie's Timing Doc revision 1126 DMA
sequence: positive alignment to the next eight-clock boundary since machine
power-on, eight clocks of whole-transfer setup, eight clocks per channel,
eight clocks per byte, and positive resynchronization to the next CPU
operation's 6/8/12-clock class. FastROM does not change byte-slot cost.
H/V and scheduled events advance during the pause. At equal timestamps,
events run before the byte commits; event-budget exhaustion and caller
horizons retain the pending phase without repeating a byte. CPU state and
DMA registers are checked against the admitted phase snapshot.

The eight-clock byte's read/write subphase is not established by the
reference. **The implementation commits both effects at the slot endpoint**
and puts whole-transfer setup before the first channel. This is an explicit
reference approximation, not a verified electrical phase model. WRAM refresh
pauses, HDMA priority/overlap, special `$4000-$41FF` behavior, DMA/arithmetic
overlap and exact post-DMA interrupt latency remain pending. Interrupt edges
are retained during the pause and delivered using the existing CPU sequence
boundary policy. The legacy atomic CPU helper does not service DMA; use the
resumable machine path. No real PS2/PSP or Android emulator run is claimed.

`report.master_clocks` includes committed CPU clocks and DMA stall clocks;
`dma_master_clocks` and `dma_bytes` expose the DMA contribution. Bus access
and processor-cycle counters remain CPU-only. As with pending CPU operations,
a partly elapsed DMA phase has not yet been charged to the report.

## Deterministic checks

- [x] Original synthetic `STA $420B; NOP` fixture verifies the grace fetch and CPU freeze.
- [x] Byte endpoint, repeated horizon and exhausted event budget do not duplicate effects.
- [x] Chunked and uninterrupted execution agree on data, clocks and register completion.
- [x] 6/8/12-clock CPU resynchronization, FastROM, second-channel setup and reverse transfer.
- [x] Blocked A-bus I/O, mirrored WRAM conflict and legal ROM-to-WRAM-port side effects.
- [x] Interrupt retention, stale CPU rejection and timestamp-overflow preflight.
- [x] A machine fixture transfers twelve bytes to the actual VRAM, OAM and CGRAM port implementations.
- [ ] WRAM refresh and HDMA arbitration verification.
- [ ] Sub-byte electrical phase and real-console validation.

Completion of this foundation requires these synthetic tests plus host and
PS2/PSP builds. It does not complete the parent DMA timing milestone. Build
commands and CI results are recorded in [STATUS.md](../STATUS.md).

## Sources consulted on 2026-10-24

- [Nintendo SNES Development Manual, Book I](https://floating.muncher.se/bot/manual/book1_text.pdf), DMA chapter and CPU clock chapter 21 (internal page 2-21-1).
- [Anomie's Timing Doc, revision 1126, 2007-04-21](https://raw.githubusercontent.com/gilligan/snesdev/master/docs/timing.txt), DMA section: one-cycle grace, alignment, per-channel/byte costs, CPU resynchronization and unknown intra-slot phase.
- [Anomie's Register Doc, revision 1157, 2007-07-12](https://raw.githubusercontent.com/gilligan/snesdev/master/docs/snes_registers.txt), `$2180-$2183` and DMA sections: WRAM conflicts and A/B restrictions.

Only hardware descriptions and independent research were consulted. No source
code from another emulator was used or indexed.
