/* MIPSNES2 S-DSP register/timing foundation. See LICENSE for GPL-3.0 terms. */
#include "core/apu/dsp/dsp.h"

#include <limits.h>
#include <string.h>

enum {
    DSP_ENVELOPE_COUNTER_PERIOD = 0x7800,
    DSP_ENVELOPE_MAX = 0x07ff
};

static const uint16_t envelope_counter_rates[32] = {
    0U, 2048U, 1536U, 1280U, 1024U, 768U, 640U, 512U,
    384U, 320U, 256U, 192U, 160U, 128U, 96U, 80U,
    64U, 48U, 40U, 32U, 24U, 20U, 16U, 12U,
    10U, 8U, 6U, 5U, 4U, 3U, 2U, 1U
};

static const uint16_t envelope_counter_offsets[32] = {
    0U, 0U, 1040U, 536U, 0U, 1040U, 536U, 0U,
    1040U, 536U, 0U, 1040U, 536U, 0U, 1040U, 536U,
    0U, 1040U, 536U, 0U, 1040U, 536U, 0U, 1040U,
    536U, 0U, 1040U, 536U, 0U, 1040U, 0U, 0U
};

/* 512 coefficients transcribed from the hardware-extracted SnesLab/fullsnes
 * documentation. FNV-1a over little-endian 16-bit entries: b1216dfae1423abe. */
static const uint16_t gaussian_coefficients[512] = {
    UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000),
    UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000), UINT16_C(0x000),
    UINT16_C(0x001), UINT16_C(0x001), UINT16_C(0x001), UINT16_C(0x001), UINT16_C(0x001), UINT16_C(0x001), UINT16_C(0x001), UINT16_C(0x001),
    UINT16_C(0x001), UINT16_C(0x001), UINT16_C(0x001), UINT16_C(0x002), UINT16_C(0x002), UINT16_C(0x002), UINT16_C(0x002), UINT16_C(0x002),
    UINT16_C(0x002), UINT16_C(0x002), UINT16_C(0x003), UINT16_C(0x003), UINT16_C(0x003), UINT16_C(0x003), UINT16_C(0x003), UINT16_C(0x004),
    UINT16_C(0x004), UINT16_C(0x004), UINT16_C(0x004), UINT16_C(0x004), UINT16_C(0x005), UINT16_C(0x005), UINT16_C(0x005), UINT16_C(0x005),
    UINT16_C(0x006), UINT16_C(0x006), UINT16_C(0x006), UINT16_C(0x006), UINT16_C(0x007), UINT16_C(0x007), UINT16_C(0x007), UINT16_C(0x008),
    UINT16_C(0x008), UINT16_C(0x008), UINT16_C(0x009), UINT16_C(0x009), UINT16_C(0x009), UINT16_C(0x00a), UINT16_C(0x00a), UINT16_C(0x00a),
    UINT16_C(0x00b), UINT16_C(0x00b), UINT16_C(0x00b), UINT16_C(0x00c), UINT16_C(0x00c), UINT16_C(0x00d), UINT16_C(0x00d), UINT16_C(0x00e),
    UINT16_C(0x00e), UINT16_C(0x00f), UINT16_C(0x00f), UINT16_C(0x00f), UINT16_C(0x010), UINT16_C(0x010), UINT16_C(0x011), UINT16_C(0x011),
    UINT16_C(0x012), UINT16_C(0x013), UINT16_C(0x013), UINT16_C(0x014), UINT16_C(0x014), UINT16_C(0x015), UINT16_C(0x015), UINT16_C(0x016),
    UINT16_C(0x017), UINT16_C(0x017), UINT16_C(0x018), UINT16_C(0x018), UINT16_C(0x019), UINT16_C(0x01a), UINT16_C(0x01b), UINT16_C(0x01b),
    UINT16_C(0x01c), UINT16_C(0x01d), UINT16_C(0x01d), UINT16_C(0x01e), UINT16_C(0x01f), UINT16_C(0x020), UINT16_C(0x020), UINT16_C(0x021),
    UINT16_C(0x022), UINT16_C(0x023), UINT16_C(0x024), UINT16_C(0x024), UINT16_C(0x025), UINT16_C(0x026), UINT16_C(0x027), UINT16_C(0x028),
    UINT16_C(0x029), UINT16_C(0x02a), UINT16_C(0x02b), UINT16_C(0x02c), UINT16_C(0x02d), UINT16_C(0x02e), UINT16_C(0x02f), UINT16_C(0x030),
    UINT16_C(0x031), UINT16_C(0x032), UINT16_C(0x033), UINT16_C(0x034), UINT16_C(0x035), UINT16_C(0x036), UINT16_C(0x037), UINT16_C(0x038),
    UINT16_C(0x03a), UINT16_C(0x03b), UINT16_C(0x03c), UINT16_C(0x03d), UINT16_C(0x03e), UINT16_C(0x040), UINT16_C(0x041), UINT16_C(0x042),
    UINT16_C(0x043), UINT16_C(0x045), UINT16_C(0x046), UINT16_C(0x047), UINT16_C(0x049), UINT16_C(0x04a), UINT16_C(0x04c), UINT16_C(0x04d),
    UINT16_C(0x04e), UINT16_C(0x050), UINT16_C(0x051), UINT16_C(0x053), UINT16_C(0x054), UINT16_C(0x056), UINT16_C(0x057), UINT16_C(0x059),
    UINT16_C(0x05a), UINT16_C(0x05c), UINT16_C(0x05e), UINT16_C(0x05f), UINT16_C(0x061), UINT16_C(0x063), UINT16_C(0x064), UINT16_C(0x066),
    UINT16_C(0x068), UINT16_C(0x06a), UINT16_C(0x06b), UINT16_C(0x06d), UINT16_C(0x06f), UINT16_C(0x071), UINT16_C(0x073), UINT16_C(0x075),
    UINT16_C(0x076), UINT16_C(0x078), UINT16_C(0x07a), UINT16_C(0x07c), UINT16_C(0x07e), UINT16_C(0x080), UINT16_C(0x082), UINT16_C(0x084),
    UINT16_C(0x086), UINT16_C(0x089), UINT16_C(0x08b), UINT16_C(0x08d), UINT16_C(0x08f), UINT16_C(0x091), UINT16_C(0x093), UINT16_C(0x096),
    UINT16_C(0x098), UINT16_C(0x09a), UINT16_C(0x09c), UINT16_C(0x09f), UINT16_C(0x0a1), UINT16_C(0x0a3), UINT16_C(0x0a6), UINT16_C(0x0a8),
    UINT16_C(0x0ab), UINT16_C(0x0ad), UINT16_C(0x0af), UINT16_C(0x0b2), UINT16_C(0x0b4), UINT16_C(0x0b7), UINT16_C(0x0ba), UINT16_C(0x0bc),
    UINT16_C(0x0bf), UINT16_C(0x0c1), UINT16_C(0x0c4), UINT16_C(0x0c7), UINT16_C(0x0c9), UINT16_C(0x0cc), UINT16_C(0x0cf), UINT16_C(0x0d2),
    UINT16_C(0x0d4), UINT16_C(0x0d7), UINT16_C(0x0da), UINT16_C(0x0dd), UINT16_C(0x0e0), UINT16_C(0x0e3), UINT16_C(0x0e6), UINT16_C(0x0e9),
    UINT16_C(0x0ec), UINT16_C(0x0ef), UINT16_C(0x0f2), UINT16_C(0x0f5), UINT16_C(0x0f8), UINT16_C(0x0fb), UINT16_C(0x0fe), UINT16_C(0x101),
    UINT16_C(0x104), UINT16_C(0x107), UINT16_C(0x10b), UINT16_C(0x10e), UINT16_C(0x111), UINT16_C(0x114), UINT16_C(0x118), UINT16_C(0x11b),
    UINT16_C(0x11e), UINT16_C(0x122), UINT16_C(0x125), UINT16_C(0x129), UINT16_C(0x12c), UINT16_C(0x130), UINT16_C(0x133), UINT16_C(0x137),
    UINT16_C(0x13a), UINT16_C(0x13e), UINT16_C(0x141), UINT16_C(0x145), UINT16_C(0x148), UINT16_C(0x14c), UINT16_C(0x150), UINT16_C(0x153),
    UINT16_C(0x157), UINT16_C(0x15b), UINT16_C(0x15f), UINT16_C(0x162), UINT16_C(0x166), UINT16_C(0x16a), UINT16_C(0x16e), UINT16_C(0x172),
    UINT16_C(0x176), UINT16_C(0x17a), UINT16_C(0x17d), UINT16_C(0x181), UINT16_C(0x185), UINT16_C(0x189), UINT16_C(0x18d), UINT16_C(0x191),
    UINT16_C(0x195), UINT16_C(0x19a), UINT16_C(0x19e), UINT16_C(0x1a2), UINT16_C(0x1a6), UINT16_C(0x1aa), UINT16_C(0x1ae), UINT16_C(0x1b2),
    UINT16_C(0x1b7), UINT16_C(0x1bb), UINT16_C(0x1bf), UINT16_C(0x1c3), UINT16_C(0x1c8), UINT16_C(0x1cc), UINT16_C(0x1d0), UINT16_C(0x1d5),
    UINT16_C(0x1d9), UINT16_C(0x1dd), UINT16_C(0x1e2), UINT16_C(0x1e6), UINT16_C(0x1eb), UINT16_C(0x1ef), UINT16_C(0x1f3), UINT16_C(0x1f8),
    UINT16_C(0x1fc), UINT16_C(0x201), UINT16_C(0x205), UINT16_C(0x20a), UINT16_C(0x20f), UINT16_C(0x213), UINT16_C(0x218), UINT16_C(0x21c),
    UINT16_C(0x221), UINT16_C(0x226), UINT16_C(0x22a), UINT16_C(0x22f), UINT16_C(0x233), UINT16_C(0x238), UINT16_C(0x23d), UINT16_C(0x241),
    UINT16_C(0x246), UINT16_C(0x24b), UINT16_C(0x250), UINT16_C(0x254), UINT16_C(0x259), UINT16_C(0x25e), UINT16_C(0x263), UINT16_C(0x267),
    UINT16_C(0x26c), UINT16_C(0x271), UINT16_C(0x276), UINT16_C(0x27b), UINT16_C(0x280), UINT16_C(0x284), UINT16_C(0x289), UINT16_C(0x28e),
    UINT16_C(0x293), UINT16_C(0x298), UINT16_C(0x29d), UINT16_C(0x2a2), UINT16_C(0x2a6), UINT16_C(0x2ab), UINT16_C(0x2b0), UINT16_C(0x2b5),
    UINT16_C(0x2ba), UINT16_C(0x2bf), UINT16_C(0x2c4), UINT16_C(0x2c9), UINT16_C(0x2ce), UINT16_C(0x2d3), UINT16_C(0x2d8), UINT16_C(0x2dc),
    UINT16_C(0x2e1), UINT16_C(0x2e6), UINT16_C(0x2eb), UINT16_C(0x2f0), UINT16_C(0x2f5), UINT16_C(0x2fa), UINT16_C(0x2ff), UINT16_C(0x304),
    UINT16_C(0x309), UINT16_C(0x30e), UINT16_C(0x313), UINT16_C(0x318), UINT16_C(0x31d), UINT16_C(0x322), UINT16_C(0x326), UINT16_C(0x32b),
    UINT16_C(0x330), UINT16_C(0x335), UINT16_C(0x33a), UINT16_C(0x33f), UINT16_C(0x344), UINT16_C(0x349), UINT16_C(0x34e), UINT16_C(0x353),
    UINT16_C(0x357), UINT16_C(0x35c), UINT16_C(0x361), UINT16_C(0x366), UINT16_C(0x36b), UINT16_C(0x370), UINT16_C(0x374), UINT16_C(0x379),
    UINT16_C(0x37e), UINT16_C(0x383), UINT16_C(0x388), UINT16_C(0x38c), UINT16_C(0x391), UINT16_C(0x396), UINT16_C(0x39b), UINT16_C(0x39f),
    UINT16_C(0x3a4), UINT16_C(0x3a9), UINT16_C(0x3ad), UINT16_C(0x3b2), UINT16_C(0x3b7), UINT16_C(0x3bb), UINT16_C(0x3c0), UINT16_C(0x3c5),
    UINT16_C(0x3c9), UINT16_C(0x3ce), UINT16_C(0x3d2), UINT16_C(0x3d7), UINT16_C(0x3dc), UINT16_C(0x3e0), UINT16_C(0x3e5), UINT16_C(0x3e9),
    UINT16_C(0x3ed), UINT16_C(0x3f2), UINT16_C(0x3f6), UINT16_C(0x3fb), UINT16_C(0x3ff), UINT16_C(0x403), UINT16_C(0x408), UINT16_C(0x40c),
    UINT16_C(0x410), UINT16_C(0x415), UINT16_C(0x419), UINT16_C(0x41d), UINT16_C(0x421), UINT16_C(0x425), UINT16_C(0x42a), UINT16_C(0x42e),
    UINT16_C(0x432), UINT16_C(0x436), UINT16_C(0x43a), UINT16_C(0x43e), UINT16_C(0x442), UINT16_C(0x446), UINT16_C(0x44a), UINT16_C(0x44e),
    UINT16_C(0x452), UINT16_C(0x455), UINT16_C(0x459), UINT16_C(0x45d), UINT16_C(0x461), UINT16_C(0x465), UINT16_C(0x468), UINT16_C(0x46c),
    UINT16_C(0x470), UINT16_C(0x473), UINT16_C(0x477), UINT16_C(0x47a), UINT16_C(0x47e), UINT16_C(0x481), UINT16_C(0x485), UINT16_C(0x488),
    UINT16_C(0x48c), UINT16_C(0x48f), UINT16_C(0x492), UINT16_C(0x496), UINT16_C(0x499), UINT16_C(0x49c), UINT16_C(0x49f), UINT16_C(0x4a2),
    UINT16_C(0x4a6), UINT16_C(0x4a9), UINT16_C(0x4ac), UINT16_C(0x4af), UINT16_C(0x4b2), UINT16_C(0x4b5), UINT16_C(0x4b7), UINT16_C(0x4ba),
    UINT16_C(0x4bd), UINT16_C(0x4c0), UINT16_C(0x4c3), UINT16_C(0x4c5), UINT16_C(0x4c8), UINT16_C(0x4cb), UINT16_C(0x4cd), UINT16_C(0x4d0),
    UINT16_C(0x4d2), UINT16_C(0x4d5), UINT16_C(0x4d7), UINT16_C(0x4d9), UINT16_C(0x4dc), UINT16_C(0x4de), UINT16_C(0x4e0), UINT16_C(0x4e3),
    UINT16_C(0x4e5), UINT16_C(0x4e7), UINT16_C(0x4e9), UINT16_C(0x4eb), UINT16_C(0x4ed), UINT16_C(0x4ef), UINT16_C(0x4f1), UINT16_C(0x4f3),
    UINT16_C(0x4f5), UINT16_C(0x4f6), UINT16_C(0x4f8), UINT16_C(0x4fa), UINT16_C(0x4fb), UINT16_C(0x4fd), UINT16_C(0x4ff), UINT16_C(0x500),
    UINT16_C(0x502), UINT16_C(0x503), UINT16_C(0x504), UINT16_C(0x506), UINT16_C(0x507), UINT16_C(0x508), UINT16_C(0x50a), UINT16_C(0x50b),
    UINT16_C(0x50c), UINT16_C(0x50d), UINT16_C(0x50e), UINT16_C(0x50f), UINT16_C(0x510), UINT16_C(0x511), UINT16_C(0x511), UINT16_C(0x512),
    UINT16_C(0x513), UINT16_C(0x514), UINT16_C(0x514), UINT16_C(0x515), UINT16_C(0x516), UINT16_C(0x516), UINT16_C(0x517), UINT16_C(0x517),
    UINT16_C(0x517), UINT16_C(0x518), UINT16_C(0x518), UINT16_C(0x518), UINT16_C(0x518), UINT16_C(0x518), UINT16_C(0x519), UINT16_C(0x519)
};


static int32_t arithmetic_shift_right(int32_t value, unsigned int amount)
{
    uint32_t magnitude;
    uint32_t bias;

    if (amount == 0U || value >= 0) {
        return value >> amount;
    }
    magnitude = (uint32_t)(-(value + 1)) + UINT32_C(1);
    bias = (UINT32_C(1) << amount) - UINT32_C(1);
    return -(int32_t)((magnitude + bias) >> amount);
}

static int16_t clamp_signed16(int32_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

static int16_t wrap_signed16(int32_t value)
{
    uint16_t wrapped = (uint16_t)((uint32_t)value & UINT32_C(0xffff));

    return wrapped <= (uint16_t)INT16_MAX
               ? (int16_t)wrapped
               : (int16_t)((int32_t)wrapped - INT32_C(0x10000));
}

static int16_t interpolate_gaussian(const int16_t samples[4],
                                    uint8_t fraction)
{
    int32_t partial;
    int32_t newest;

    partial = arithmetic_shift_right(
        (int32_t)gaussian_coefficients[UINT8_C(0xff) - fraction] *
            (int32_t)samples[0],
        10U);
    partial += arithmetic_shift_right(
        (int32_t)gaussian_coefficients[UINT16_C(0x1ff) - fraction] *
            (int32_t)samples[1],
        10U);
    partial += arithmetic_shift_right(
        (int32_t)gaussian_coefficients[UINT16_C(0x100) + fraction] *
            (int32_t)samples[2],
        10U);
    partial = wrap_signed16(partial);
    newest = arithmetic_shift_right(
        (int32_t)gaussian_coefficients[fraction] *
            (int32_t)samples[3],
        10U);
    return (int16_t)arithmetic_shift_right(
        clamp_signed16(partial + newest), 1U);
}

static int16_t clamp_and_wrap_brr(int32_t value)
{
    uint16_t wrapped;

    if (value > INT16_MAX) {
        value = INT16_MAX;
    } else if (value < INT16_MIN) {
        value = INT16_MIN;
    }
    wrapped = (uint16_t)value & UINT16_C(0x7fff);
    if ((wrapped & UINT16_C(0x4000)) != 0U) {
        return (int16_t)((int32_t)wrapped - INT32_C(0x8000));
    }
    return (int16_t)wrapped;
}

static int16_t decode_brr_sample(uint8_t raw, uint8_t shift,
                                 uint8_t filter, int16_t *previous1,
                                 int16_t *previous2)
{
    int32_t history1 = *previous1;
    int32_t history2 = *previous2;
    int32_t nibble = raw < UINT8_C(8) ? (int32_t)raw
                                      : (int32_t)raw - INT32_C(16);
    int32_t sample;
    int16_t result;

    if (shift <= 12U) {
        sample = arithmetic_shift_right(
            nibble * (INT32_C(1) << shift), 1U);
    } else {
        sample = nibble < 0 ? INT32_C(-2048) : 0;
    }
    if (filter == 1U) {
        sample += history1 + arithmetic_shift_right(-history1, 4U);
    } else if (filter == 2U) {
        sample += (INT32_C(2) * history1) +
                  arithmetic_shift_right(INT32_C(-3) * history1, 5U) -
                  history2 + arithmetic_shift_right(history2, 4U);
    } else if (filter == 3U) {
        sample += (INT32_C(2) * history1) +
                  arithmetic_shift_right(INT32_C(-13) * history1, 6U) -
                  history2 + arithmetic_shift_right(
                      INT32_C(3) * history2, 4U);
    }
    result = clamp_and_wrap_brr(sample);
    *previous2 = *previous1;
    *previous1 = result;
    return result;
}

static uint16_t read_ram16(const mipsnes2_dsp *dsp, uint16_t address)
{
    uint16_t next = (uint16_t)(address + UINT16_C(1));

    return (uint16_t)((uint16_t)dsp->ram[address] |
                      ((uint16_t)dsp->ram[next] << 8U));
}

static void write_ram16(mipsnes2_dsp *dsp, uint16_t address, int16_t value)
{
    uint16_t next = (uint16_t)(address + UINT16_C(1));
    uint16_t bits = (uint16_t)value;

    dsp->ram[address] = (uint8_t)bits;
    dsp->ram[next] = (uint8_t)(bits >> 8U);
}

static int16_t filter_echo_fir(
    const int16_t samples[MIPSNES2_DSP_FIR_TAP_COUNT],
    const int8_t coefficients[MIPSNES2_DSP_FIR_TAP_COUNT])
{
    int32_t partial = 0;
    int32_t newest;
    unsigned int tap;

    for (tap = 0U; tap < MIPSNES2_DSP_FIR_TAP_COUNT - 1U; ++tap) {
        partial += arithmetic_shift_right(
            (int32_t)samples[tap] * (int32_t)coefficients[tap], 6U);
    }
    partial = wrap_signed16(partial);
    newest = arithmetic_shift_right(
        (int32_t)samples[MIPSNES2_DSP_FIR_TAP_COUNT - 1U] *
            (int32_t)coefficients[MIPSNES2_DSP_FIR_TAP_COUNT - 1U],
        6U);
    return (int16_t)(arithmetic_shift_right(
        clamp_signed16(partial + newest), 1U) * INT32_C(2));
}

static uint16_t voice_pitch(const mipsnes2_dsp *dsp, unsigned int index)
{
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);

    return (uint16_t)(((uint16_t)dsp->registers[
                           base + MIPSNES2_DSP_VOICE_REGISTER_PITCH_HIGH]
                       << 8U) |
                      dsp->registers[
                          base + MIPSNES2_DSP_VOICE_REGISTER_PITCH_LOW]) &
           UINT16_C(0x3fff);
}

static uint16_t effective_voice_pitch(const mipsnes2_dsp *dsp,
                                      unsigned int index)
{
    uint16_t base_pitch = voice_pitch(dsp, index);
    uint8_t bit = (uint8_t)(UINT8_C(1) << index);
    int32_t pitch = base_pitch;

    if (index != 0U &&
        (dsp->registers[MIPSNES2_DSP_REGISTER_PMON] & bit) != 0U &&
        (dsp->registers[MIPSNES2_DSP_REGISTER_NON] & bit) == 0U &&
        dsp->voices[index - 1U].output_sample_valid) {
        int32_t modulation = arithmetic_shift_right(
            dsp->voices[index - 1U].output_sample, 5U);

        pitch += arithmetic_shift_right(
            modulation * (int32_t)base_pitch, 10U);
    }
    return (uint16_t)pitch;
}

static void advance_envelope_counter(mipsnes2_dsp *dsp, uint64_t samples)
{
    uint16_t delta = (uint16_t)(
        samples % (uint64_t)DSP_ENVELOPE_COUNTER_PERIOD);

    dsp->envelope_counter = (uint16_t)(
        (dsp->envelope_counter + DSP_ENVELOPE_COUNTER_PERIOD - delta) %
        DSP_ENVELOPE_COUNTER_PERIOD);
}

static bool envelope_counter_event(const mipsnes2_dsp *dsp, uint8_t rate)
{
    uint16_t period = envelope_counter_rates[rate];

    return period != 0U &&
           ((uint32_t)dsp->envelope_counter +
            envelope_counter_offsets[rate]) % period == 0U;
}

static uint64_t counter_event_count(uint16_t counter, uint64_t samples,
                                    uint8_t rate)
{
    uint16_t period = envelope_counter_rates[rate];
    uint16_t first;

    if (period == 0U || samples == 0U) {
        return 0U;
    }
    first = (uint16_t)(((uint32_t)counter +
                        envelope_counter_offsets[rate]) % period);
    if (first == 0U) {
        first = period;
    }
    if (samples < first) {
        return 0U;
    }
    return UINT64_C(1) +
           (samples - (uint64_t)first) / (uint64_t)period;
}

static uint16_t step_noise_lfsr(uint16_t value)
{
    uint16_t feedback = (uint16_t)(
        ((value ^ (value >> 1U)) & UINT16_C(1)) << 14U);

    return (uint16_t)((value >> 1U) | feedback);
}

static int16_t current_noise_sample(const mipsnes2_dsp *dsp)
{
    uint16_t value = dsp->noise_lfsr;

    return (int16_t)((value & UINT16_C(0x4000)) != 0U
                         ? (int32_t)value - INT32_C(0x8000)
                         : (int32_t)value);
}

static uint16_t apply_noise_transform(const uint16_t transform[15],
                                      uint16_t value)
{
    uint16_t result = 0U;
    unsigned int bit;

    for (bit = 0U; bit < 15U; ++bit) {
        if ((value & (uint16_t)(UINT16_C(1) << bit)) != 0U) {
            result ^= transform[bit];
        }
    }
    return result;
}

static void advance_noise_lfsr(mipsnes2_dsp *dsp, uint64_t events)
{
    uint16_t transform[15];
    uint16_t squared[15];
    uint16_t value = dsp->noise_lfsr;
    unsigned int bit;

    if (events <= UINT64_C(64)) {
        while (events != 0U) {
            value = step_noise_lfsr(value);
            --events;
        }
        dsp->noise_lfsr = value;
        return;
    }
    for (bit = 0U; bit < 15U; ++bit) {
        transform[bit] = step_noise_lfsr(
            (uint16_t)(UINT16_C(1) << bit));
    }
    while (events != 0U) {
        if ((events & UINT64_C(1)) != 0U) {
            value = apply_noise_transform(transform, value);
        }
        for (bit = 0U; bit < 15U; ++bit) {
            squared[bit] = apply_noise_transform(
                transform, transform[bit]);
        }
        (void)memcpy(transform, squared, sizeof(transform));
        events >>= 1U;
    }
    dsp->noise_lfsr = value;
}

static void advance_global_state(mipsnes2_dsp *dsp, uint64_t samples)
{
    uint8_t rate = dsp->internal_flags & UINT8_C(0x1f);
    uint64_t noise_events = counter_event_count(
        dsp->envelope_counter, samples, rate);

    advance_envelope_counter(dsp, samples);
    advance_noise_lfsr(dsp, noise_events);
}

static uint16_t clamp_envelope(int32_t value)
{
    if (value < 0) {
        return 0U;
    }
    if (value > DSP_ENVELOPE_MAX) {
        return DSP_ENVELOPE_MAX;
    }
    return (uint16_t)value;
}

static void update_envelope_register(mipsnes2_dsp *dsp,
                                     unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);

    dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_ENVX] =
        (uint8_t)(voice->envelope >> 4U);
}

static int32_t signed_register_value(uint8_t value)
{
    return value < UINT8_C(0x80) ? (int32_t)value
                                 : (int32_t)value - INT32_C(0x100);
}

static bool fill_voice_interpolation_input(
    const mipsnes2_dsp_voice *voice,
    mipsnes2_dsp_interpolation_input *input)
{
    uint8_t sample_index;
    unsigned int index;

    if (!voice->brr_stream_active || voice->decoded_groups < 3U) {
        return false;
    }
    sample_index = (uint8_t)(voice->interpolation_index >> 12U);
    for (index = 0U; index < 4U; ++index) {
        input->samples[index] = voice->brr_ring[
            (voice->ring_offset + sample_index + index) %
            MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT];
    }
    input->sample_index = sample_index;
    input->fraction = (uint8_t)(voice->interpolation_index >> 4U);
    return true;
}

static void update_voice_output(mipsnes2_dsp *dsp, unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t bit = (uint8_t)(UINT8_C(1) << index);
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);
    bool noise_selected =
        (dsp->registers[MIPSNES2_DSP_REGISTER_NON] & bit) != 0U;
    int32_t adjusted;

    voice->output_uses_noise = noise_selected;
    if (noise_selected) {
        voice->source_sample = current_noise_sample(dsp);
    } else {
        mipsnes2_dsp_interpolation_input input;

        if (!fill_voice_interpolation_input(voice, &input)) {
            if (voice->brr_stream_active && voice->key_on_delay != 0U) {
                voice->source_sample = 0;
            } else {
                voice->output_sample_valid = false;
                return;
            }
        } else {
            voice->source_sample = interpolate_gaussian(
                input.samples, input.fraction);
        }
    }
    /* BRR and noise sources are signed 15-bit values. The S-DSP keeps the
     * post-envelope sample left-aligned in 16 bits and clears its low bit. */
    adjusted = arithmetic_shift_right(
        (int32_t)voice->source_sample * (int32_t)voice->envelope, 10U);
    adjusted = arithmetic_shift_right(adjusted, 1U) * INT32_C(2);
    voice->output_sample = (int16_t)adjusted;
    voice->left_sample = wrap_signed16(arithmetic_shift_right(
        (int32_t)voice->output_sample * signed_register_value(
            dsp->registers[
                base + MIPSNES2_DSP_VOICE_REGISTER_VOLUME_LEFT]),
        7U));
    voice->right_sample = wrap_signed16(arithmetic_shift_right(
        (int32_t)voice->output_sample * signed_register_value(
            dsp->registers[
                base + MIPSNES2_DSP_VOICE_REGISTER_VOLUME_RIGHT]),
        7U));
    voice->output_sample_valid = true;
    dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_OUTX] =
        (uint8_t)((uint16_t)voice->output_sample >> 8U);
}

static void update_resolved_dry_mix(mipsnes2_dsp *dsp)
{
    int32_t left = 0;
    int32_t right = 0;
    uint8_t mask = 0U;
    unsigned int index;

    /* Saturation is observable, so preserve the documented Voice 0-to-7
     * accumulation order and clamp after every individual addition. */
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        const mipsnes2_dsp_voice *voice = &dsp->voices[index];

        if (!voice->output_sample_valid) {
            continue;
        }
        left = clamp_signed16(left + (int32_t)voice->left_sample);
        right = clamp_signed16(right + (int32_t)voice->right_sample);
        mask |= (uint8_t)(UINT8_C(1) << index);
    }
    dsp->dry_mix.voice_mix.left = (int16_t)left;
    dsp->dry_mix.voice_mix.right = (int16_t)right;
    dsp->dry_mix.main_sample.left = wrap_signed16(arithmetic_shift_right(
        left * signed_register_value(
            dsp->registers[MIPSNES2_DSP_REGISTER_MAIN_VOLUME_LEFT]),
        7U));
    dsp->dry_mix.main_sample.right = wrap_signed16(arithmetic_shift_right(
        right * signed_register_value(
            dsp->registers[MIPSNES2_DSP_REGISTER_MAIN_VOLUME_RIGHT]),
        7U));
    dsp->dry_mix.resolved_voice_mask = mask;
}

static void update_echo_voice_mix(mipsnes2_dsp *dsp)
{
    int32_t left = 0;
    int32_t right = 0;
    uint8_t mask = 0U;
    uint8_t enabled = dsp->registers[MIPSNES2_DSP_REGISTER_EON];
    unsigned int index;

    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        const mipsnes2_dsp_voice *voice = &dsp->voices[index];
        uint8_t bit = (uint8_t)(UINT8_C(1) << index);

        if (!voice->output_sample_valid || (enabled & bit) == 0U) {
            continue;
        }
        left = clamp_signed16(left + (int32_t)voice->left_sample);
        right = clamp_signed16(right + (int32_t)voice->right_sample);
        mask |= bit;
    }
    dsp->echo.voice_mix.left = (int16_t)left;
    dsp->echo.voice_mix.right = (int16_t)right;
    dsp->echo.resolved_voice_mask = mask;
}

static void update_final_output(mipsnes2_dsp *dsp)
{
    int32_t left;
    int32_t right;

    dsp->output.echo_sample.left = wrap_signed16(arithmetic_shift_right(
        (int32_t)dsp->echo.fir_sample.left * signed_register_value(
            dsp->registers[MIPSNES2_DSP_REGISTER_ECHO_VOLUME_LEFT]),
        7U));
    dsp->output.echo_sample.right = wrap_signed16(arithmetic_shift_right(
        (int32_t)dsp->echo.fir_sample.right * signed_register_value(
            dsp->registers[MIPSNES2_DSP_REGISTER_ECHO_VOLUME_RIGHT]),
        7U));
    left = (int32_t)dsp->dry_mix.main_sample.left +
           (int32_t)dsp->output.echo_sample.left;
    right = (int32_t)dsp->dry_mix.main_sample.right +
            (int32_t)dsp->output.echo_sample.right;
    dsp->output.mixed_sample.left = clamp_signed16(left);
    dsp->output.mixed_sample.right = clamp_signed16(right);
    dsp->output.muted = (dsp->internal_flags & UINT8_C(0x40)) != 0U;
    if (dsp->output.muted) {
        dsp->output.dac_sample.left = 0;
        dsp->output.dac_sample.right = 0;
    } else {
        dsp->output.dac_sample = dsp->output.mixed_sample;
    }
}

static void update_echo_and_output(mipsnes2_dsp *dsp)
{
    int16_t ordered_left[MIPSNES2_DSP_FIR_TAP_COUNT];
    int16_t ordered_right[MIPSNES2_DSP_FIR_TAP_COUNT];
    int8_t coefficients[MIPSNES2_DSP_FIR_TAP_COUNT];
    uint8_t current_history = dsp->echo.history_index;
    unsigned int tap;
    int32_t left;
    int32_t right;

    dsp->echo.ram_valid = dsp->ram != NULL &&
                          dsp->ram_size >= MIPSNES2_DSP_ARAM_SIZE;
    dsp->echo.write_enabled =
        (dsp->internal_flags & UINT8_C(0x20)) == 0U;
    update_echo_voice_mix(dsp);
    if (!dsp->echo.ram_valid) {
        update_final_output(dsp);
        return;
    }

    if (dsp->echo.offset == 0U) {
        dsp->echo.length = (uint16_t)(
            (uint16_t)(dsp->registers[MIPSNES2_DSP_REGISTER_EDL] &
                       UINT8_C(0x0f)) << 11U);
        if (dsp->echo.length == 0U) {
            dsp->echo.length = UINT16_C(4);
        }
    }
    dsp->echo.address = (uint16_t)(dsp->echo.base_address +
                                   dsp->echo.offset);
    dsp->echo.history_left[current_history] = (int16_t)
        arithmetic_shift_right(wrap_signed16((int32_t)read_ram16(
            dsp, dsp->echo.address)), 1U);
    dsp->echo.history_right[current_history] = (int16_t)
        arithmetic_shift_right(wrap_signed16((int32_t)read_ram16(
            dsp, (uint16_t)(dsp->echo.address + UINT16_C(2)))), 1U);

    for (tap = 0U; tap < MIPSNES2_DSP_FIR_TAP_COUNT; ++tap) {
        uint8_t history = (uint8_t)(
            (current_history + 1U + tap) &
            (MIPSNES2_DSP_FIR_TAP_COUNT - 1U));
        uint8_t coefficient_register = (uint8_t)(
            MIPSNES2_DSP_REGISTER_FIR_BASE +
            (tap * MIPSNES2_DSP_VOICE_REGISTER_STRIDE));

        ordered_left[tap] = dsp->echo.history_left[history];
        ordered_right[tap] = dsp->echo.history_right[history];
        coefficients[tap] = (int8_t)signed_register_value(
            dsp->registers[coefficient_register]);
    }
    dsp->echo.fir_sample.left = filter_echo_fir(
        ordered_left, coefficients);
    dsp->echo.fir_sample.right = filter_echo_fir(
        ordered_right, coefficients);
    update_final_output(dsp);

    dsp->echo.feedback_sample.left = wrap_signed16(arithmetic_shift_right(
        (int32_t)dsp->echo.fir_sample.left * signed_register_value(
            dsp->registers[MIPSNES2_DSP_REGISTER_ECHO_FEEDBACK]),
        7U));
    dsp->echo.feedback_sample.right = wrap_signed16(arithmetic_shift_right(
        (int32_t)dsp->echo.fir_sample.right * signed_register_value(
            dsp->registers[MIPSNES2_DSP_REGISTER_ECHO_FEEDBACK]),
        7U));
    left = clamp_signed16((int32_t)dsp->echo.voice_mix.left +
                          (int32_t)dsp->echo.feedback_sample.left);
    right = clamp_signed16((int32_t)dsp->echo.voice_mix.right +
                           (int32_t)dsp->echo.feedback_sample.right);
    dsp->echo.write_sample.left = (int16_t)(
        arithmetic_shift_right(left, 1U) * INT32_C(2));
    dsp->echo.write_sample.right = (int16_t)(
        arithmetic_shift_right(right, 1U) * INT32_C(2));
    if (dsp->echo.write_enabled) {
        write_ram16(dsp, dsp->echo.address, dsp->echo.write_sample.left);
        write_ram16(dsp, (uint16_t)(dsp->echo.address + UINT16_C(2)),
                    dsp->echo.write_sample.right);
    }

    dsp->echo.offset = (uint16_t)(dsp->echo.offset + UINT16_C(4));
    if (dsp->echo.offset >= dsp->echo.length) {
        dsp->echo.offset = 0U;
    }
    dsp->echo.base_address = (uint16_t)(
        (uint16_t)dsp->registers[MIPSNES2_DSP_REGISTER_ESA] << 8U);
    dsp->echo.history_index = (uint8_t)(
        (current_history + 1U) & (MIPSNES2_DSP_FIR_TAP_COUNT - 1U));
}

static void refresh_idle_noise_outputs(mipsnes2_dsp *dsp)
{
    unsigned int index;
    int16_t source = current_noise_sample(dsp);

    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        mipsnes2_dsp_voice *voice = &dsp->voices[index];
        uint8_t bit = (uint8_t)(UINT8_C(1) << index);
        uint8_t base = (uint8_t)(
            index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);

        if ((dsp->registers[MIPSNES2_DSP_REGISTER_NON] & bit) != 0U) {
            voice->source_sample = source;
            voice->output_sample = 0;
            voice->left_sample = 0;
            voice->right_sample = 0;
            voice->output_sample_valid = true;
            voice->output_uses_noise = true;
            dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_OUTX] = 0U;
        }
    }
    update_resolved_dry_mix(dsp);
    update_echo_and_output(dsp);
}

static void release_envelope(mipsnes2_dsp *dsp, unsigned int index,
                             bool clear)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];

    voice->envelope_state = MIPSNES2_DSP_ENVELOPE_RELEASE;
    if (clear) {
        voice->envelope = 0U;
        voice->envelope_calculation = 0;
        update_envelope_register(dsp, index);
    }
}

static void update_voice_envelope(mipsnes2_dsp *dsp, unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);
    uint8_t adsr1 = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_ADSR1];
    uint8_t adsr2 = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_ADSR2];
    uint8_t gain = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_GAIN];
    int32_t next = voice->envelope;
    uint8_t rate = 31U;
    bool direct = false;

    if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_RELEASE) {
        next -= INT32_C(8);
    } else if ((adsr1 & UINT8_C(0x80)) == 0U) {
        if ((gain & UINT8_C(0x80)) == 0U) {
            next = (int32_t)(gain & UINT8_C(0x7f)) << 4U;
            direct = true;
        } else {
            uint8_t mode = (gain >> 5U) & UINT8_C(3);

            rate = gain & UINT8_C(0x1f);
            if (mode == 0U) {
                next -= INT32_C(32);
            } else if (mode == 1U) {
                next -= arithmetic_shift_right(next - INT32_C(1), 8U) +
                        INT32_C(1);
            } else if (mode == 2U) {
                next += INT32_C(32);
            } else {
                next += voice->envelope_calculation < INT32_C(0x600)
                            ? INT32_C(32) : INT32_C(8);
            }
        }
    } else if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_ATTACK) {
        uint8_t attack = adsr1 & UINT8_C(0x0f);

        rate = (uint8_t)((attack << 1U) | UINT8_C(1));
        next += attack == UINT8_C(0x0f) ? INT32_C(1024)
                                                : INT32_C(32);
    } else {
        if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_DECAY) {
            rate = (uint8_t)(UINT8_C(16) +
                             (((adsr1 >> 4U) & UINT8_C(7)) << 1U));
        } else {
            rate = adsr2 & UINT8_C(0x1f);
        }
        next -= arithmetic_shift_right(next - INT32_C(1), 8U) +
                INT32_C(1);
    }

    if (direct ||
        voice->envelope_state == MIPSNES2_DSP_ENVELOPE_RELEASE ||
        envelope_counter_event(dsp, rate)) {
        voice->envelope = clamp_envelope(next);
    }
    if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_DECAY &&
        ((uint32_t)next >> 8U) == (uint32_t)(adsr2 >> 5U)) {
        voice->envelope_state = MIPSNES2_DSP_ENVELOPE_SUSTAIN;
    } else if (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_ATTACK &&
               (next < 0 || next > DSP_ENVELOPE_MAX)) {
        voice->envelope_state = MIPSNES2_DSP_ENVELOPE_DECAY;
    }
    voice->envelope_calculation = next;
    update_envelope_register(dsp, index);
}

static void refresh_loop_address(mipsnes2_dsp *dsp, unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);
    uint16_t directory;

    voice->source_number = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_SRCN];
    directory = (uint16_t)(((uint16_t)dsp->registers[
                                MIPSNES2_DSP_REGISTER_DIR] << 8U) +
                           ((uint16_t)voice->source_number * UINT16_C(4)));
    voice->directory_address = directory;
    voice->source_loop_address = read_ram16(
        dsp, (uint16_t)(directory + UINT16_C(2)));
}

static void decode_voice_brr_group(mipsnes2_dsp *dsp, unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t shift;
    uint8_t filter;
    unsigned int byte_index;

    if (!voice->brr_stream_active) {
        return;
    }
    if (voice->brr_group_index == 0U) {
        voice->brr_header = dsp->ram[voice->brr_address];
        if ((voice->brr_header & UINT8_C(3)) == UINT8_C(1)) {
            voice->state = MIPSNES2_DSP_VOICE_RELEASE;
            release_envelope(dsp, index, true);
        }
    }
    shift = voice->brr_header >> 4U;
    filter = (voice->brr_header >> 2U) & UINT8_C(3);
    for (byte_index = 0U; byte_index < 2U; ++byte_index) {
        uint16_t address = (uint16_t)(
            voice->brr_address + UINT16_C(1) +
            ((uint16_t)voice->brr_group_index * UINT16_C(2)) +
            (uint16_t)byte_index);
        uint8_t packed = dsp->ram[address];
        uint8_t destination = (uint8_t)(
            voice->ring_write_offset + (byte_index * 2U));

        voice->brr_ring[destination] = decode_brr_sample(
            packed >> 4U, shift, filter, &voice->brr_previous1,
            &voice->brr_previous2);
        voice->brr_ring[destination + 1U] = decode_brr_sample(
            packed & UINT8_C(0x0f), shift, filter,
            &voice->brr_previous1, &voice->brr_previous2);
    }
    voice->ring_write_offset = (uint8_t)(
        (voice->ring_write_offset + MIPSNES2_DSP_BRR_GROUP_SAMPLE_COUNT) %
        MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT);
    ++voice->brr_group_index;
    if (voice->decoded_groups != UINT64_MAX) {
        ++voice->decoded_groups;
    }
    if (voice->brr_group_index ==
        MIPSNES2_DSP_BRR_SAMPLE_COUNT /
        MIPSNES2_DSP_BRR_GROUP_SAMPLE_COUNT) {
        voice->brr_group_index = 0U;
        if ((voice->brr_header & UINT8_C(1)) != 0U) {
            dsp->registers[MIPSNES2_DSP_REGISTER_ENDX] |=
                (uint8_t)(UINT8_C(1) << index);
            refresh_loop_address(dsp, index);
            voice->brr_address = voice->source_loop_address;
        } else {
            voice->brr_address = (uint16_t)(
                voice->brr_address + MIPSNES2_DSP_BRR_BLOCK_SIZE);
        }
    }
}

static void begin_key_on(mipsnes2_dsp *dsp, unsigned int index)
{
    mipsnes2_dsp_voice *voice = &dsp->voices[index];
    uint8_t base = (uint8_t)(index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE);
    uint16_t directory;

    voice->source_number = dsp->registers[
        base + MIPSNES2_DSP_VOICE_REGISTER_SRCN];
    directory = (uint16_t)(((uint16_t)dsp->registers[
                                MIPSNES2_DSP_REGISTER_DIR] << 8U) +
                           ((uint16_t)voice->source_number * UINT16_C(4)));
    voice->directory_address = directory;
    voice->directory_loaded = dsp->ram != NULL &&
                              dsp->ram_size >= MIPSNES2_DSP_ARAM_SIZE;
    if (voice->directory_loaded) {
        voice->source_start_address = read_ram16(dsp, directory);
        voice->source_loop_address = read_ram16(
            dsp, (uint16_t)(directory + UINT16_C(2)));
    } else {
        voice->source_start_address = 0U;
        voice->source_loop_address = 0U;
    }
    voice->brr_address = voice->source_start_address;
    voice->interpolation_index = 0U;
    voice->pitch = voice_pitch(dsp, index);
    voice->decoded_groups = 0U;
    voice->envelope = 0U;
    voice->envelope_calculation = 0;
    voice->output_sample = 0;
    voice->left_sample = 0;
    voice->right_sample = 0;
    voice->output_sample_valid =
        ((dsp->registers[MIPSNES2_DSP_REGISTER_NON] &
          (uint8_t)(UINT8_C(1) << index)) != 0U) ||
        voice->directory_loaded;
    voice->output_uses_noise =
        (dsp->registers[MIPSNES2_DSP_REGISTER_NON] &
         (uint8_t)(UINT8_C(1) << index)) != 0U;
    voice->source_sample = voice->output_sample_valid
                               ? current_noise_sample(dsp) : 0;
    voice->envelope_state = MIPSNES2_DSP_ENVELOPE_ATTACK;
    voice->brr_group_index = 0U;
    voice->brr_header = 0U;
    voice->ring_offset = 0U;
    voice->ring_write_offset = 0U;
    voice->brr_previous1 = voice->brr_ring[
        MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT - 1U];
    voice->brr_previous2 = voice->brr_ring[
        MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT - 2U];
    voice->brr_stream_active = voice->directory_loaded;
    voice->key_on_delay = MIPSNES2_DSP_KEY_ON_DELAY_SAMPLES;
    voice->state = MIPSNES2_DSP_VOICE_KEY_ON_DELAY;
    dsp->registers[MIPSNES2_DSP_REGISTER_ENDX] &=
        (uint8_t)~(UINT8_C(1) << index);
    dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_ENVX] = 0U;
    dsp->registers[base + MIPSNES2_DSP_VOICE_REGISTER_OUTX] = 0U;
}

static bool event_work_pending(const mipsnes2_dsp *dsp)
{
    unsigned int index;

    /* With ARAM attached the echo pointer, FIR history and optional writes
     * advance every output sample even when all voices are idle. */
    if (dsp->pcm != NULL ||
        (dsp->ram != NULL && dsp->ram_size >= MIPSNES2_DSP_ARAM_SIZE) ||
        dsp->pending_key_on != 0U) {
        return true;
    }
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        const mipsnes2_dsp_voice *voice = &dsp->voices[index];
        uint8_t bit = (uint8_t)(UINT8_C(1) << index);
        bool noise_selected =
            (dsp->registers[MIPSNES2_DSP_REGISTER_NON] & bit) != 0U;

        if (voice->key_on_delay != 0U ||
            voice->output_uses_noise != noise_selected ||
            (voice->output_sample_valid && voice->envelope == 0U &&
             voice->output_sample != 0) ||
            voice->state == MIPSNES2_DSP_VOICE_ACTIVE ||
            (voice->envelope_state == MIPSNES2_DSP_ENVELOPE_RELEASE &&
             voice->envelope != 0U) ||
            (voice->brr_stream_active &&
             (voice->interpolation_index >= UINT16_C(0x4000) ||
              voice_pitch(dsp, index) != 0U)) ||
            ((dsp->internal_flags & UINT8_C(0x80)) != 0U &&
             voice->state != MIPSNES2_DSP_VOICE_WAITING) ||
            ((dsp->registers[MIPSNES2_DSP_REGISTER_KOFF] & bit) != 0U &&
             voice->state != MIPSNES2_DSP_VOICE_WAITING &&
             voice->state != MIPSNES2_DSP_VOICE_RELEASE)) {
            return true;
        }
    }
    return false;
}

static void service_sample_event(mipsnes2_dsp *dsp)
{
    unsigned int index;
    bool poll_keys = (dsp->sample_count & UINT64_C(1)) == 0U;

    advance_global_state(dsp, 1U);
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        mipsnes2_dsp_voice *voice = &dsp->voices[index];
        bool update_envelope = false;

        if (voice->key_on_delay != 0U) {
            if (voice->brr_stream_active && voice->key_on_delay <= 4U &&
                voice->key_on_delay >= 2U) {
                decode_voice_brr_group(dsp, index);
            }
            --voice->key_on_delay;
            update_envelope = voice->key_on_delay == 0U;
            if (voice->key_on_delay == 0U &&
                voice->state == MIPSNES2_DSP_VOICE_KEY_ON_DELAY) {
                voice->state = MIPSNES2_DSP_VOICE_ACTIVE;
            }
            update_voice_output(dsp, index);
        } else if (voice->brr_stream_active) {
            uint32_t position;

            if (voice->interpolation_index >= UINT16_C(0x4000)) {
                voice->ring_offset = (uint8_t)(
                    (voice->ring_offset +
                     MIPSNES2_DSP_BRR_GROUP_SAMPLE_COUNT) %
                    MIPSNES2_DSP_BRR_RING_SAMPLE_COUNT);
                decode_voice_brr_group(dsp, index);
                voice->interpolation_index &= UINT16_C(0x3fff);
            }
            update_voice_output(dsp, index);
            voice->pitch = effective_voice_pitch(dsp, index);
            position = (uint32_t)voice->interpolation_index + voice->pitch;
            if (position > UINT16_C(0x7fff)) {
                position = UINT16_C(0x7fff);
            }
            voice->interpolation_index = (uint16_t)position;
            update_envelope = voice->state == MIPSNES2_DSP_VOICE_ACTIVE ||
                              voice->state == MIPSNES2_DSP_VOICE_RELEASE;
        } else {
            update_voice_output(dsp, index);
            update_envelope = voice->state == MIPSNES2_DSP_VOICE_ACTIVE ||
                              voice->state == MIPSNES2_DSP_VOICE_RELEASE;
        }
        if ((dsp->internal_flags & UINT8_C(0x80)) != 0U &&
            voice->state != MIPSNES2_DSP_VOICE_WAITING) {
            voice->state = MIPSNES2_DSP_VOICE_WAITING;
            release_envelope(dsp, index, true);
            voice->source_sample = 0;
            voice->output_sample = 0;
            voice->left_sample = 0;
            voice->right_sample = 0;
            voice->output_sample_valid =
                (dsp->registers[MIPSNES2_DSP_REGISTER_NON] &
                 (uint8_t)(UINT8_C(1) << index)) != 0U ||
                voice->brr_stream_active;
            voice->output_uses_noise =
                (dsp->registers[MIPSNES2_DSP_REGISTER_NON] &
                 (uint8_t)(UINT8_C(1) << index)) != 0U;
            dsp->registers[
                (index * MIPSNES2_DSP_VOICE_REGISTER_STRIDE) +
                MIPSNES2_DSP_VOICE_REGISTER_OUTX] = 0U;
        } else if (update_envelope) {
            update_voice_envelope(dsp, index);
        }
    }
    update_resolved_dry_mix(dsp);
    update_echo_and_output(dsp);
    if (dsp->pcm != NULL) {
        mipsnes2_pcm_frame frame;

        frame.left = dsp->output.dac_sample.left;
        frame.right = dsp->output.dac_sample.right;
        (void)mipsnes2_pcm_buffer_push(dsp->pcm, frame);
    }
    if (!poll_keys) {
        return;
    }
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        uint8_t bit = (uint8_t)(UINT8_C(1) << index);

        if ((dsp->registers[MIPSNES2_DSP_REGISTER_KOFF] & bit) != 0U &&
            dsp->voices[index].state != MIPSNES2_DSP_VOICE_WAITING) {
            dsp->voices[index].state = MIPSNES2_DSP_VOICE_RELEASE;
            release_envelope(dsp, index, false);
        }
    }
    for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
        if ((dsp->pending_key_on & (UINT8_C(1) << index)) != 0U) {
            begin_key_on(dsp, index);
        }
    }
    dsp->pending_key_on = 0U;
}

void mipsnes2_dsp_init(mipsnes2_dsp *dsp)
{
    if (dsp != NULL) {
        unsigned int index;

        (void)memset(dsp, 0, sizeof(*dsp));
        /* The internal reset/mute/echo-disable state is distinct from the
         * readable register shadow, whose electrical reset value is not
         * claimed by this foundation. */
        dsp->internal_flags = UINT8_C(0xe0);
        dsp->noise_lfsr = UINT16_C(0x4000);
        dsp->echo.length = UINT16_C(4);
        dsp->output.muted = true;
        for (index = 0U; index < MIPSNES2_DSP_VOICE_COUNT; ++index) {
            dsp->voices[index].envelope_state =
                MIPSNES2_DSP_ENVELOPE_RELEASE;
        }
        dsp->initialized = true;
    }
}

bool mipsnes2_dsp_attach_ram(mipsnes2_dsp *dsp, uint8_t *ram,
                             size_t ram_size)
{
    if (dsp == NULL || !dsp->initialized || ram == NULL ||
        ram_size < MIPSNES2_DSP_ARAM_SIZE) {
        return false;
    }
    dsp->ram = ram;
    dsp->ram_size = ram_size;
    dsp->echo.ram_valid = true;
    return true;
}

bool mipsnes2_dsp_attach_pcm(mipsnes2_dsp *dsp,
                             mipsnes2_pcm_buffer *pcm)
{
    if (dsp == NULL || !dsp->initialized || pcm == NULL ||
        !pcm->initialized) {
        return false;
    }
    dsp->pcm = pcm;
    return true;
}

void mipsnes2_dsp_detach_pcm(mipsnes2_dsp *dsp)
{
    if (dsp != NULL && dsp->initialized) {
        dsp->pcm = NULL;
    }
}

bool mipsnes2_dsp_write_address(mipsnes2_dsp *dsp, uint8_t address)
{
    if (dsp == NULL || !dsp->initialized) {
        return false;
    }
    dsp->address = address;
    return true;
}

bool mipsnes2_dsp_read_address(const mipsnes2_dsp *dsp, uint8_t *address)
{
    if (dsp == NULL || !dsp->initialized || address == NULL) {
        return false;
    }
    *address = dsp->address;
    return true;
}

bool mipsnes2_dsp_write_data(mipsnes2_dsp *dsp, uint8_t value)
{
    uint8_t reg;

    if (dsp == NULL || !dsp->initialized) {
        return false;
    }
    /* Address bit 7 inhibits S-DSP writes but does not change the latch. */
    if ((dsp->address & UINT8_C(0x80)) != 0U) {
        return true;
    }
    reg = dsp->address & UINT8_C(0x7f);
    if (reg == MIPSNES2_DSP_REGISTER_ENDX) {
        /* Any CPU write acknowledges and clears all voice-end bits. */
        dsp->registers[reg] = 0U;
        return true;
    }
    dsp->registers[reg] = value;
    if (reg == MIPSNES2_DSP_REGISTER_KON) {
        /* KON is sampled at 16 kHz. Multiple writes before a poll collapse to
         * the most recently written mask. */
        dsp->pending_key_on = value;
    } else if (reg == MIPSNES2_DSP_REGISTER_FLG) {
        dsp->internal_flags = value;
    }
    return true;
}

bool mipsnes2_dsp_read_data(const mipsnes2_dsp *dsp, uint8_t *value)
{
    if (dsp == NULL || !dsp->initialized || value == NULL) {
        return false;
    }
    *value = dsp->registers[dsp->address & UINT8_C(0x7f)];
    return true;
}

bool mipsnes2_dsp_brr_decode_block(const uint8_t *encoded,
                                   size_t encoded_size,
                                   int16_t previous1, int16_t previous2,
                                   mipsnes2_dsp_brr_block *decoded)
{
    mipsnes2_dsp_brr_block result;
    unsigned int index;

    if (encoded == NULL || decoded == NULL ||
        encoded_size < MIPSNES2_DSP_BRR_BLOCK_SIZE) {
        return false;
    }
    (void)memset(&result, 0, sizeof(result));
    result.header = encoded[0];
    result.shift = encoded[0] >> 4U;
    result.filter = (encoded[0] >> 2U) & UINT8_C(3);
    result.loop = (encoded[0] & UINT8_C(2)) != 0U;
    result.end = (encoded[0] & UINT8_C(1)) != 0U;
    for (index = 0U; index < MIPSNES2_DSP_BRR_SAMPLE_COUNT; ++index) {
        uint8_t packed = encoded[1U + (index / 2U)];
        uint8_t raw = (index & 1U) == 0U ? packed >> 4U
                                         : packed & UINT8_C(0x0f);

        result.samples[index] = decode_brr_sample(
            raw, result.shift, result.filter, &previous1, &previous2);
    }
    result.previous1 = previous1;
    result.previous2 = previous2;
    *decoded = result;
    return true;
}

bool mipsnes2_dsp_voice_interpolation_input(
    const mipsnes2_dsp *dsp, size_t voice_index,
    mipsnes2_dsp_interpolation_input *input)
{
    const mipsnes2_dsp_voice *voice;

    if (dsp == NULL || !dsp->initialized ||
        voice_index >= MIPSNES2_DSP_VOICE_COUNT || input == NULL) {
        return false;
    }
    voice = &dsp->voices[voice_index];
    if (voice->key_on_delay != 0U) {
        return false;
    }
    return fill_voice_interpolation_input(voice, input);
}

bool mipsnes2_dsp_gaussian_coefficient(size_t index,
                                       uint16_t *coefficient)
{
    if (index >= sizeof(gaussian_coefficients) /
                     sizeof(gaussian_coefficients[0]) ||
        coefficient == NULL) {
        return false;
    }
    *coefficient = gaussian_coefficients[index];
    return true;
}

bool mipsnes2_dsp_gaussian_interpolate(
    const mipsnes2_dsp_interpolation_input *input, int16_t *sample)
{
    unsigned int index;

    if (input == NULL || sample == NULL ||
        input->sample_index >=
            (MIPSNES2_DSP_BRR_GROUP_SAMPLE_COUNT * 2U)) {
        return false;
    }
    for (index = 0U; index < 4U; ++index) {
        if (input->samples[index] < INT16_C(-16384) ||
            input->samples[index] > INT16_C(16383)) {
            return false;
        }
    }
    *sample = interpolate_gaussian(input->samples, input->fraction);
    return true;
}

bool mipsnes2_dsp_fir(
    const int16_t samples[MIPSNES2_DSP_FIR_TAP_COUNT],
    const int8_t coefficients[MIPSNES2_DSP_FIR_TAP_COUNT],
    int16_t *sample)
{
    unsigned int index;

    if (samples == NULL || coefficients == NULL || sample == NULL) {
        return false;
    }
    for (index = 0U; index < MIPSNES2_DSP_FIR_TAP_COUNT; ++index) {
        if (samples[index] < INT16_C(-16384) ||
            samples[index] > INT16_C(16383)) {
            return false;
        }
    }
    *sample = filter_echo_fir(samples, coefficients);
    return true;
}

bool mipsnes2_dsp_voice_noise_input(const mipsnes2_dsp *dsp,
                                    size_t voice_index, int16_t *sample)
{
    if (dsp == NULL || !dsp->initialized ||
        voice_index >= MIPSNES2_DSP_VOICE_COUNT || sample == NULL ||
        (dsp->registers[MIPSNES2_DSP_REGISTER_NON] &
         (uint8_t)(UINT8_C(1) << voice_index)) == 0U) {
        return false;
    }
    *sample = current_noise_sample(dsp);
    return true;
}

bool mipsnes2_dsp_voice_output_sample(const mipsnes2_dsp *dsp,
                                      size_t voice_index, int16_t *sample)
{
    if (dsp == NULL || !dsp->initialized ||
        voice_index >= MIPSNES2_DSP_VOICE_COUNT || sample == NULL ||
        !dsp->voices[voice_index].output_sample_valid) {
        return false;
    }
    *sample = dsp->voices[voice_index].output_sample;
    return true;
}

bool mipsnes2_dsp_voice_stereo_sample(const mipsnes2_dsp *dsp,
                                      size_t voice_index,
                                      mipsnes2_dsp_stereo_sample *sample)
{
    if (dsp == NULL || !dsp->initialized ||
        voice_index >= MIPSNES2_DSP_VOICE_COUNT || sample == NULL ||
        !dsp->voices[voice_index].output_sample_valid) {
        return false;
    }
    sample->left = dsp->voices[voice_index].left_sample;
    sample->right = dsp->voices[voice_index].right_sample;
    return true;
}

bool mipsnes2_dsp_resolved_dry_mix(const mipsnes2_dsp *dsp,
                                   mipsnes2_dsp_dry_mix *mix)
{
    if (dsp == NULL || !dsp->initialized || mix == NULL) {
        return false;
    }
    *mix = dsp->dry_mix;
    return true;
}

bool mipsnes2_dsp_resolved_echo(const mipsnes2_dsp *dsp,
                                mipsnes2_dsp_echo *echo)
{
    if (dsp == NULL || !dsp->initialized || echo == NULL) {
        return false;
    }
    *echo = dsp->echo;
    return true;
}

bool mipsnes2_dsp_resolved_output(const mipsnes2_dsp *dsp,
                                  mipsnes2_dsp_output *output)
{
    if (dsp == NULL || !dsp->initialized || output == NULL) {
        return false;
    }
    *output = dsp->output;
    return true;
}

bool mipsnes2_dsp_can_advance(const mipsnes2_dsp *dsp,
                              uint64_t smp_cycles)
{
    uint64_t samples;
    uint64_t whole;
    uint8_t remainder;
    uint16_t phase;

    if (dsp == NULL || !dsp->initialized ||
        dsp->smp_cycles > UINT64_MAX - smp_cycles) {
        return false;
    }
    whole = smp_cycles / MIPSNES2_DSP_SAMPLE_CYCLES;
    remainder = (uint8_t)(smp_cycles % MIPSNES2_DSP_SAMPLE_CYCLES);
    phase = (uint16_t)dsp->sample_phase + remainder;
    samples = whole + (phase / MIPSNES2_DSP_SAMPLE_CYCLES);
    return dsp->sample_count <= UINT64_MAX - samples;
}

bool mipsnes2_dsp_advance(mipsnes2_dsp *dsp, uint64_t smp_cycles)
{
    uint64_t samples;
    uint64_t whole;
    uint8_t remainder;
    uint16_t phase;

    if (!mipsnes2_dsp_can_advance(dsp, smp_cycles)) {
        return false;
    }
    whole = smp_cycles / MIPSNES2_DSP_SAMPLE_CYCLES;
    remainder = (uint8_t)(smp_cycles % MIPSNES2_DSP_SAMPLE_CYCLES);
    phase = (uint16_t)dsp->sample_phase + remainder;
    samples = whole + (phase / MIPSNES2_DSP_SAMPLE_CYCLES);
    dsp->sample_phase = (uint8_t)(phase % MIPSNES2_DSP_SAMPLE_CYCLES);
    dsp->smp_cycles += smp_cycles;
    while (samples != 0U && event_work_pending(dsp)) {
        service_sample_event(dsp);
        ++dsp->sample_count;
        --samples;
    }
    advance_global_state(dsp, samples);
    if (samples != 0U) {
        refresh_idle_noise_outputs(dsp);
    }
    dsp->sample_count += samples;
    return true;
}
