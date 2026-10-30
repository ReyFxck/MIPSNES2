#ifndef MIPSNES2_TEST_PS2_DRAW_H
#define MIPSNES2_TEST_PS2_DRAW_H

#include <tamtypes.h>

qword_t *draw_texture_transfer(
    qword_t *q, void *src, int width, int height, int psm,
    int dest, int dest_width);
qword_t *draw_texture_flush(qword_t *q);

#endif
