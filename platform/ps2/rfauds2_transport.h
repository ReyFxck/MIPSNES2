/* RFAuds2 binding for the PS2 PCM adapter. See LICENSE for GPL-3.0 terms. */
#ifndef MIPSNES2_PS2_RFAUDS2_TRANSPORT_H
#define MIPSNES2_PS2_RFAUDS2_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>

#include "platform/ps2/audio_adapter.h"

/* The caller still owns RFAuds2 module load/bind/start/shutdown lifecycle. */
bool mipsnes2_ps2_rfauds2_audio_init(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_frame *staging,
    size_t staging_capacity);

#endif
