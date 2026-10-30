#ifndef MIPSNES2_TEST_PS2_GRAPH_VRAM_H
#define MIPSNES2_TEST_PS2_GRAPH_VRAM_H

#define GRAPH_ALIGN_PAGE 2048

int graph_vram_allocate(int width, int height, int psm, int alignment);
void graph_vram_free(int address);

#endif
