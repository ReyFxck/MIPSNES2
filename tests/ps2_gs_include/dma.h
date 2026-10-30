#ifndef MIPSNES2_TEST_PS2_DMA_H
#define MIPSNES2_TEST_PS2_DMA_H

#define DMA_CHANNEL_GIF 2

int dma_channel_initialize(int channel, void *handler, int flags);
void dma_channel_fast_waits(int channel);
int dma_channel_send_chain(
    int channel, void *data, int qwc, int flags, int spr);
void dma_wait_fast(void);
int dma_channel_shutdown(int channel, int flags);

#endif
