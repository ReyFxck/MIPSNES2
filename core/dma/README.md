# core/dma

The shared core owns eight `$43x0-$43xB` channel register banks, the
`$43xB/$43xF` alias, mode/address helpers, and a bounded untimed byte-transfer
cursor with a caller-supplied single-phase A/B-bus callback. `$420B` does not
start DMA; CPU stalls, DMA bus policy, timing and HDMA are pending. See the
[canonical status](../../STATUS.md) and
[DMA audit](../../docs/dma-register-foundation-audit.md).
