/* PS2 GS presentation of one captured reference frame. GPL-3.0. */
#include "platform/ps2/gs_present.h"
#include "platform/ps2/video_frame.h"

#include <dma.h>
#include <draw.h>
#include <graph.h>
#include <gs_psm.h>
#include <kernel.h>
#include <packet.h>

bool mipsnes2_ps2_gs_present(const uint32_t *pixels, size_t pixel_count)
{
    packet_t *packet;
    qword_t *q;
    int address;

    if (pixels == NULL ||
        pixel_count < MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS ||
        ((uintptr_t)pixels & 15U) != 0U) {
        return false;
    }
    packet = packet_init(50, PACKET_NORMAL);
    if (packet == NULL) {
        return false;
    }
    address = graph_vram_allocate(MIPSNES2_PS2_VIDEO_WIDTH,
                                  MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT,
                                  GS_PSM_32, GRAPH_ALIGN_PAGE);
    if (address < 0) {
        packet_free(packet);
        return false;
    }
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
    if (graph_initialize(address, MIPSNES2_PS2_VIDEO_WIDTH,
                         MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT,
                         GS_PSM_32, 0, 0) != 0) {
        packet_free(packet);
        return false;
    }
    q = packet->data;
    q = draw_texture_transfer(q, (void *)pixels,
                              MIPSNES2_PS2_VIDEO_WIDTH,
                              MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT,
                              GS_PSM_32, address,
                              MIPSNES2_PS2_VIDEO_WIDTH);
    q = draw_texture_flush(q);
    /* The DMA chain references the texture separately from its packet.
     * PS2SDK only flushes the packet in dma_channel_send_chain(). */
    FlushCache(0);
    if (dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data,
                               q - packet->data, 0, 0) < 0) {
        packet_free(packet);
        return false;
    }
    dma_wait_fast();
    graph_wait_vsync();
    packet_free(packet);
    return true;
}
