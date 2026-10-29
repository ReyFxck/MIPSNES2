/* RFAuds2 binding for the PS2 PCM adapter. See LICENSE for GPL-3.0 terms. */
#include "platform/ps2/rfauds2_transport.h"

#include <stddef.h>

#include <rfauds2/rfauds2.h>

typedef char mipsnes2_rfauds2_frame_size_check[
    (sizeof(mipsnes2_pcm_frame) == 2U * sizeof(s16)) ? 1 : -1];
typedef char mipsnes2_rfauds2_left_offset_check[
    (offsetof(mipsnes2_pcm_frame, left) == 0U) ? 1 : -1];
typedef char mipsnes2_rfauds2_right_offset_check[
    (offsetof(mipsnes2_pcm_frame, right) == sizeof(s16)) ? 1 : -1];
typedef char mipsnes2_rfauds2_rate_check[
    (RFAUDS2_OUTPUT_RATE == MIPSNES2_PS2_AUDIO_OUTPUT_RATE) ? 1 : -1];

static int rfauds2_launch(void *context,
                          const mipsnes2_pcm_frame *frames,
                          size_t frame_count)
{
    (void)context;
    if (frames == NULL || frame_count == 0U ||
        frame_count > RFAUDS2_ASYNC_MAX_FRAMES) {
        return -1;
    }
    return rfauds2_submit_s16_async(
        (const s16 *)frames, (u32)frame_count);
}

static int rfauds2_poll(void *context, size_t *accepted_frames)
{
    u32 accepted = 0U;
    int result;

    (void)context;
    if (accepted_frames == NULL) {
        return -1;
    }
    result = rfauds2_submit_poll(&accepted);
    if (result == 1) {
        *accepted_frames = accepted;
    }
    return result;
}

bool mipsnes2_ps2_rfauds2_audio_init(
    mipsnes2_ps2_audio_adapter *adapter, mipsnes2_pcm_frame *staging,
    size_t staging_capacity)
{
    if (staging_capacity > RFAUDS2_ASYNC_MAX_FRAMES) {
        return false;
    }
    return mipsnes2_ps2_audio_adapter_init(
        adapter, staging, staging_capacity, rfauds2_launch, rfauds2_poll,
        NULL);
}
