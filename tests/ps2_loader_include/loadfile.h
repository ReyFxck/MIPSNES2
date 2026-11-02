#ifndef MIPSNES2_TEST_PS2_LOADFILE_H
#define MIPSNES2_TEST_PS2_LOADFILE_H
#include <stdint.h>
int SifSearchModuleByName(const char *name);
int SifExecModuleBuffer(void *ptr, uint32_t size, uint32_t arg_len,
                        const char *args, int *mod_res);
#endif
