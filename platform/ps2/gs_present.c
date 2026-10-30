/* PS2 GS presentation of captured reference frames. GPL-3.0. */
#include "platform/ps2/gs_present.h"
#include "platform/ps2/video_frame.h"

#include <dma.h>
#include <draw.h>
#include <graph.h>
#include <gs_psm.h>
#include <kernel.h>
#include <packet.h>

static void clear_presenter(mipsnes2_ps2_gs_presenter *presenter)
{
    presenter->packet = NULL;
    presenter->framebuffer_address = -1;
    presenter->initialized = false;
}

bool mipsnes2_ps2_gs_presenter_init(
    mipsnes2_ps2_gs_presenter *presenter)
{
    packet_t *packet;
    int address;

    if (presenter == NULL || presenter->initialized) {
        return false;
    }
    clear_presenter(presenter);
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
    if (dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0) < 0) {
        graph_vram_free(address);
        packet_free(packet);
        return false;
    }
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
    if (graph_initialize(address, MIPSNES2_PS2_VIDEO_WIDTH,
                         MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT,
                         GS_PSM_32, 0, 0) != 0) {
        (void)dma_channel_shutdown(DMA_CHANNEL_GIF, 0);
        graph_vram_free(address);
        packet_free(packet);
        return false;
    }
    presenter->packet = packet;
    presenter->framebuffer_address = address;
    presenter->initialized = true;
    return true;
}

bool mipsnes2_ps2_gs_presenter_present(
    mipsnes2_ps2_gs_presenter *presenter,
    const uint32_t *pixels, size_t pixel_count)
{
    packet_t *packet;
    qword_t *q;

    if (presenter == NULL || !presenter->initialized ||
        presenter->packet == NULL || presenter->framebuffer_address < 0 ||
        pixels == NULL ||
        pixel_count < MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS ||
        ((uintptr_t)pixels & 15U) != 0U) {
        return false;
    }
    packet = presenter->packet;
    packet_reset(packet);
    q = packet->data;
    q = draw_texture_transfer(q, (void *)pixels,
                              MIPSNES2_PS2_VIDEO_WIDTH,
                              MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT,
                              GS_PSM_32, presenter->framebuffer_address,
                              MIPSNES2_PS2_VIDEO_WIDTH);
    q = draw_texture_flush(q);
    /* The DMA chain references the texture separately from its packet.
     * PS2SDK only flushes the packet in dma_channel_send_chain(). */
    FlushCache(0);
    if (dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data,
                               q - packet->data, 0, 0) < 0) {
        return false;
    }
    dma_wait_fast();
    graph_wait_vsync();
    return true;
}

void mipsnes2_ps2_gs_presenter_end(
    mipsnes2_ps2_gs_presenter *presenter)
{
    packet_t *packet;
    int address;

    if (presenter == NULL || !presenter->initialized) {
        return;
    }
    packet = presenter->packet;
    address = presenter->framebuffer_address;
    dma_wait_fast();
    graph_vram_free(address);
    packet_free(packet);
    (void)graph_shutdown();
    (void)dma_channel_shutdown(DMA_CHANNEL_GIF, 0);
    clear_presenter(presenter);
}
