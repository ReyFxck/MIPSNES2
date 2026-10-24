# core/dma

The shared core owns eight `$43x0-$43xB` channel register banks, the
`$43xB/$43xF` alias, and pure general-purpose DMA mode/address helpers.
No bytes move yet; `$420B` does not start DMA, and CPU stalls, DMA timing and
HDMA are pending. See the [canonical status](../../STATUS.md) and
[DMA audit](../../docs/dma-register-foundation-audit.md).
