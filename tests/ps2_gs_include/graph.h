#ifndef MIPSNES2_TEST_PS2_GRAPH_H
#define MIPSNES2_TEST_PS2_GRAPH_H

#include <graph_vram.h>

int graph_initialize(int fbp, int width, int height, int psm, int x, int y);
void graph_wait_vsync(void);
int graph_shutdown(void);

#endif
