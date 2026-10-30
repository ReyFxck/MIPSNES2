/* Host lifecycle fixture for the PS2 GS presenter. GPL-3.0. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dma.h>
#include <draw.h>
#include <graph.h>
#include <gs_psm.h>
#include <packet.h>

#include "platform/ps2/gs_present.h"
#include "platform/ps2/video_frame.h"

static void check(bool value, int line, const char *expression)
{
    if (!value) {
        (void)fprintf(stderr, "line %d: %s\n", line, expression);
        exit(1);
    }
}

#define CHECK(value) check((value), __LINE__, #value)

static qword_t packet_data[64];
static packet_t fake_packet;
static uint32_t pixels[MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS]
    __attribute__((aligned(64)));
static unsigned char unaligned_pixels[sizeof(pixels) + 1U];

static bool fail_packet;
static int allocate_result;
static int dma_init_result;
static int graph_init_result;
static int send_result;
static unsigned packet_init_calls;
static unsigned packet_reset_calls;
static unsigned packet_free_calls;
static unsigned allocate_calls;
static unsigned vram_free_calls;
static unsigned dma_init_calls;
static unsigned dma_fast_calls;
static unsigned dma_send_calls;
static unsigned dma_wait_calls;
static unsigned dma_shutdown_calls;
static unsigned graph_init_calls;
static unsigned graph_vsync_calls;
static unsigned graph_shutdown_calls;
static unsigned transfer_calls;
static unsigned texture_flush_calls;
static unsigned cache_flush_calls;
static int last_freed_address;
static int last_transfer_address;
static int last_send_qwc;

static void reset_fakes(void)
{
    fail_packet = false;
    allocate_result = 2048;
    dma_init_result = 0;
    graph_init_result = 0;
    send_result = 0;
    packet_init_calls = 0U;
    packet_reset_calls = 0U;
    packet_free_calls = 0U;
    allocate_calls = 0U;
    vram_free_calls = 0U;
    dma_init_calls = 0U;
    dma_fast_calls = 0U;
    dma_send_calls = 0U;
    dma_wait_calls = 0U;
    dma_shutdown_calls = 0U;
    graph_init_calls = 0U;
    graph_vsync_calls = 0U;
    graph_shutdown_calls = 0U;
    transfer_calls = 0U;
    texture_flush_calls = 0U;
    cache_flush_calls = 0U;
    last_freed_address = -1;
    last_transfer_address = -1;
    last_send_qwc = -1;
    (void)memset(&fake_packet, 0, sizeof(fake_packet));
    fake_packet.data = packet_data;
}

packet_t *packet_init(int qwords, int type)
{
    ++packet_init_calls;
    fake_packet.qwords = (uint32_t)qwords;
    fake_packet.type = (uint16_t)type;
    return fail_packet ? NULL : &fake_packet;
}

void packet_reset(packet_t *packet)
{
    ++packet_reset_calls;
    packet->qwc = 0U;
    (void)memset(packet->data, 0, sizeof(packet_data));
}

void packet_free(packet_t *packet)
{
    CHECK(packet == &fake_packet);
    ++packet_free_calls;
}

int graph_vram_allocate(int width, int height, int psm, int alignment)
{
    CHECK(width == MIPSNES2_PS2_VIDEO_WIDTH);
    CHECK(height == MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT);
    CHECK(psm == GS_PSM_32 && alignment == GRAPH_ALIGN_PAGE);
    ++allocate_calls;
    return allocate_result;
}

void graph_vram_free(int address)
{
    ++vram_free_calls;
    last_freed_address = address;
}

int dma_channel_initialize(int channel, void *handler, int flags)
{
    CHECK(channel == DMA_CHANNEL_GIF && handler == NULL && flags == 0);
    ++dma_init_calls;
    return dma_init_result;
}

void dma_channel_fast_waits(int channel)
{
    CHECK(channel == DMA_CHANNEL_GIF);
    ++dma_fast_calls;
}

int dma_channel_send_chain(
    int channel, void *data, int qwc, int flags, int spr)
{
    CHECK(channel == DMA_CHANNEL_GIF && data == packet_data);
    CHECK(flags == 0 && spr == 0);
    ++dma_send_calls;
    last_send_qwc = qwc;
    return send_result;
}

void dma_wait_fast(void)
{
    ++dma_wait_calls;
}

int dma_channel_shutdown(int channel, int flags)
{
    CHECK(channel == DMA_CHANNEL_GIF && flags == 0);
    ++dma_shutdown_calls;
    return 0;
}

int graph_initialize(int fbp, int width, int height, int psm, int x, int y)
{
    CHECK(fbp == allocate_result);
    CHECK(width == MIPSNES2_PS2_VIDEO_WIDTH);
    CHECK(height == MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT);
    CHECK(psm == GS_PSM_32 && x == 0 && y == 0);
    ++graph_init_calls;
    return graph_init_result;
}

void graph_wait_vsync(void)
{
    ++graph_vsync_calls;
}

int graph_shutdown(void)
{
    ++graph_shutdown_calls;
    return 0;
}

qword_t *draw_texture_transfer(
    qword_t *q, void *src, int width, int height, int psm,
    int dest, int dest_width)
{
    CHECK(q == packet_data && src == pixels);
    CHECK(width == MIPSNES2_PS2_VIDEO_WIDTH);
    CHECK(height == MIPSNES2_PS2_VIDEO_TEXTURE_HEIGHT);
    CHECK(psm == GS_PSM_32 && dest_width == MIPSNES2_PS2_VIDEO_WIDTH);
    ++transfer_calls;
    last_transfer_address = dest;
    return q + 4;
}

qword_t *draw_texture_flush(qword_t *q)
{
    CHECK(q == packet_data + 4);
    ++texture_flush_calls;
    return q + 1;
}

void FlushCache(int operation)
{
    CHECK(operation == 0);
    ++cache_flush_calls;
}

int main(void)
{
    mipsnes2_ps2_gs_presenter presenter;
    const uint32_t *misaligned =
        (const uint32_t *)(const void *)(unaligned_pixels + 1U);

    reset_fakes();
    (void)memset(&presenter, 0, sizeof(presenter));
    CHECK(!mipsnes2_ps2_gs_presenter_init(NULL));
    fail_packet = true;
    CHECK(!mipsnes2_ps2_gs_presenter_init(&presenter));
    CHECK(presenter.packet == NULL && presenter.framebuffer_address == -1);

    reset_fakes();
    (void)memset(&presenter, 0, sizeof(presenter));
    allocate_result = -1;
    CHECK(!mipsnes2_ps2_gs_presenter_init(&presenter));
    CHECK(packet_free_calls == 1U && dma_init_calls == 0U);

    reset_fakes();
    (void)memset(&presenter, 0, sizeof(presenter));
    dma_init_result = -1;
    CHECK(!mipsnes2_ps2_gs_presenter_init(&presenter));
    CHECK(vram_free_calls == 1U && last_freed_address == 2048);
    CHECK(packet_free_calls == 1U && graph_init_calls == 0U);

    reset_fakes();
    (void)memset(&presenter, 0, sizeof(presenter));
    graph_init_result = -1;
    CHECK(!mipsnes2_ps2_gs_presenter_init(&presenter));
    CHECK(dma_shutdown_calls == 1U && vram_free_calls == 1U);
    CHECK(packet_free_calls == 1U && !presenter.initialized);

    reset_fakes();
    (void)memset(&presenter, 0, sizeof(presenter));
    CHECK(mipsnes2_ps2_gs_presenter_init(&presenter));
    CHECK(presenter.initialized && presenter.packet == &fake_packet);
    CHECK(presenter.framebuffer_address == 2048);
    CHECK(packet_init_calls == 1U && allocate_calls == 1U);
    CHECK(dma_init_calls == 1U && dma_fast_calls == 1U);
    CHECK(graph_init_calls == 1U);
    CHECK(!mipsnes2_ps2_gs_presenter_init(&presenter));
    CHECK(!mipsnes2_ps2_gs_presenter_present(NULL, pixels,
          MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS));
    CHECK(!mipsnes2_ps2_gs_presenter_present(
          &presenter, NULL, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS));
    CHECK(!mipsnes2_ps2_gs_presenter_present(
          &presenter, pixels, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS - 1U));
    CHECK(!mipsnes2_ps2_gs_presenter_present(
          &presenter, misaligned, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS));

    send_result = -1;
    CHECK(!mipsnes2_ps2_gs_presenter_present(
          &presenter, pixels, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS));
    CHECK(packet_reset_calls == 1U && dma_send_calls == 1U);
    CHECK(dma_wait_calls == 0U && graph_vsync_calls == 0U);

    send_result = 0;
    CHECK(mipsnes2_ps2_gs_presenter_present(
          &presenter, pixels, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS));
    CHECK(mipsnes2_ps2_gs_presenter_present(
          &presenter, pixels, MIPSNES2_PS2_VIDEO_TEXTURE_PIXELS));
    CHECK(packet_reset_calls == 3U && transfer_calls == 3U);
    CHECK(texture_flush_calls == 3U && cache_flush_calls == 3U);
    CHECK(dma_send_calls == 3U && last_send_qwc == 5);
    CHECK(dma_wait_calls == 2U && graph_vsync_calls == 2U);
    CHECK(last_transfer_address == 2048);
    CHECK(packet_init_calls == 1U && allocate_calls == 1U);

    mipsnes2_ps2_gs_presenter_end(&presenter);
    CHECK(!presenter.initialized && presenter.packet == NULL);
    CHECK(presenter.framebuffer_address == -1);
    CHECK(dma_wait_calls == 3U && vram_free_calls == 1U);
    CHECK(packet_free_calls == 1U && graph_shutdown_calls == 1U);
    CHECK(dma_shutdown_calls == 1U);
    mipsnes2_ps2_gs_presenter_end(&presenter);
    CHECK(packet_free_calls == 1U && graph_shutdown_calls == 1U);

    (void)puts("PS2 persistent GS presenter lifecycle: OK");
    return 0;
}
