# core/dma

The shared core owns eight `$43x0-$43xB` channel register banks, the
`$43xB/$43xF` alias, mode/address helpers, and a bounded untimed byte-transfer
cursor with a caller-supplied single-phase A/B-bus callback. The resumable
CPU bridge services `$420B` and applies shared bus restrictions and timed
CPU suspension. Refresh and HDMA remain pending. See the
[canonical status](../../STATUS.md) and [machine DMA audit](../../docs/dma-machine-audit.md).
