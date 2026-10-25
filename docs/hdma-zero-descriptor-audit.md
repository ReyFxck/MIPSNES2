# Indirect HDMA zero-descriptor audit

Date: 2026-10-25

## Scope and basis

The [Super Famicom Development Wiki HDMA algorithm](https://wiki.superfamicom.org/dma-and-hdma)
describes an exception when a zero line descriptor terminates the last active
HDMA channel of a scanline: the zero byte supplies the low indirect-address
byte, and the table supplies only the high byte. Thus A2A advances one fewer
byte and the line consumes eight fewer master clocks than a two-byte pointer
reload. Earlier channels on the same scanline still read both pointer bytes.

The platform-neutral operation cursor implements this for visible-line
descriptor reloads. It uses the selected channel mask to distinguish the
last channel, sets DAS low to zero and yields only the high-byte read. Frame
initialization still loads both pointer bytes, following the separate
start-of-frame description; the source does not establish whether that case
has the same exception. Hardware revisions and exact electrical subphases
remain unverified. No platform or game-specific branch was introduced.

## Deterministic evidence

- [x] Pure two-channel table fixture terminates both indirect channels on
  zero descriptors. The first reads two pointer bytes; the last reads one.
  A2A and DAS differ as specified, with no extra table read.
- [x] Resumable cursor fixture observes a 66-clock line instead of the prior
  74-clock model when its last selected channel terminates indirectly.
- [x] Timed machine fixture retains the data transfer and observes the
  one-byte-shorter A2A after the zero descriptor.
- [x] Revision `073b867` passed
  [PS2 CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177771815),
  [PSP CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177771794)
  and [host CI](https://github.com/ReyFxck/MIPSNES2/actions/runs/40177771800).
- [ ] Real-console comparison and frame-initialization edge behavior.

All fixtures use generated in-memory data. No source code from another
emulator was used or indexed.

On 2026-10-25, the full C99 `cc` host suite, `-O3` suite, ASan/UBSan suite
with leak detection disabled, GCC `-fanalyzer` host build and repository
checker passed. Leak detection is unavailable in this execution environment.
Physical target runtime remains unverified.
