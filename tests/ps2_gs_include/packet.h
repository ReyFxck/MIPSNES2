#ifndef MIPSNES2_TEST_PS2_PACKET_H
#define MIPSNES2_TEST_PS2_PACKET_H

#include <stdint.h>

#include <tamtypes.h>

#define PACKET_NORMAL 0

typedef struct {
    uint32_t qwords;
    uint16_t qwc;
    uint16_t type;
    qword_t *data;
} packet_t;

packet_t *packet_init(int qwords, int type);
void packet_reset(packet_t *packet);
void packet_free(packet_t *packet);

#endif
