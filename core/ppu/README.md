# core/ppu

The shared core has non-interlaced H/V counter registers, caller-owned
VRAM/CGRAM/OAM CPU ports, OAM sprite descriptor and tile-address helpers,
pure sprite scanline selection and a 4bpp tile decoder. These are PPU
foundations, not a renderer.
Display access windows, scanline evaluation, pixel composition and
platform presentation remain open.
See the [canonical status](../../STATUS.md) and subsystem design documentation.
Do not replace this scope marker with success-returning stubs.
