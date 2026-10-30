/* Minimal host-test declarations for the documented PS2SDK libpad API. */
#ifndef MIPSNES2_TEST_LIBPAD_H
#define MIPSNES2_TEST_LIBPAD_H

#define PAD_LEFT 0x0080
#define PAD_DOWN 0x0040
#define PAD_RIGHT 0x0020
#define PAD_UP 0x0010
#define PAD_START 0x0008
#define PAD_SELECT 0x0001
#define PAD_SQUARE 0x8000
#define PAD_CROSS 0x4000
#define PAD_CIRCLE 0x2000
#define PAD_TRIANGLE 0x1000
#define PAD_R1 0x0800
#define PAD_L1 0x0400

#define PAD_STATE_DISCONN 0x00
#define PAD_STATE_FINDCTP1 0x02
#define PAD_STATE_STABLE 0x06
#define PAD_STATE_ERROR 0x07

struct padButtonStatus {
    unsigned char ok;
    unsigned char mode;
    unsigned short btns;
};

int padInit(int mode);
int padEnd(void);
int padPortOpen(int port, int slot, void *pad_area);
int padPortClose(int port, int slot);
unsigned char padRead(int port, int slot, struct padButtonStatus *data);
int padGetState(int port, int slot);

#endif
