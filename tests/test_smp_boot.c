/* Deterministic clean-room S-SMP boot protocol tests. */
#include <stdio.h>
#include <string.h>

#include "core/apu/spc700/boot.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "FAIL %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static uint8_t ram[MIPSNES2_SMP_RAM_SIZE];

static int write_port(mipsnes2_smp_boot *boot, unsigned port,
                      uint8_t value, mipsnes2_smp_boot_event *event)
{
    CHECK(mipsnes2_smp_boot_cpu_write(
        boot, (uint16_t)(UINT16_C(0x2140) + port), value, event));
    return 0;
}

static int test_upload_and_start(void)
{
    mipsnes2_apu_io_ports ports;
    mipsnes2_smp_memory memory;
    mipsnes2_smp_boot boot;
    mipsnes2_smp_boot_event event;
    uint8_t value;

    (void)memset(ram, UINT8_C(0x5a), sizeof(ram));
    mipsnes2_apu_io_ports_init(&ports);
    CHECK(mipsnes2_smp_memory_init(
        &memory, ram, sizeof(ram), &ports, NULL, 0U));
    CHECK(mipsnes2_smp_boot_init(&boot, &memory));
    CHECK(ram[0x0000] == 0U && ram[0x00ef] == 0U &&
          ram[0x00f0] == UINT8_C(0x5a));
    CHECK(mipsnes2_apu_io_ports_cpu_read(
        &ports, UINT16_C(0x2140), &value));
    CHECK(value == UINT8_C(0xaa));
    CHECK(mipsnes2_apu_io_ports_cpu_read(
        &ports, UINT16_C(0x2141), &value));
    CHECK(value == UINT8_C(0xbb));

    CHECK(write_port(&boot, 2U, UINT8_C(0x00), &event) == 0);
    CHECK(write_port(&boot, 3U, UINT8_C(0x02), &event) == 0);
    CHECK(write_port(&boot, 1U, UINT8_C(0x01), &event) == 0);
    CHECK(write_port(&boot, 0U, UINT8_C(0xcc), &event) == 0);
    CHECK(!event.start_cpu &&
          boot.state == MIPSNES2_SMP_BOOT_WAIT_FIRST_BYTE);
    CHECK(mipsnes2_apu_io_ports_cpu_read(
        &ports, UINT16_C(0x2140), &value));
    CHECK(value == UINT8_C(0xcc));

    CHECK(write_port(&boot, 1U, UINT8_C(0xe8), &event) == 0);
    CHECK(write_port(&boot, 0U, 0U, &event) == 0);
    CHECK(ram[0x0200] == UINT8_C(0xe8));
    CHECK(write_port(&boot, 1U, UINT8_C(0xef), &event) == 0);
    CHECK(write_port(&boot, 0U, UINT8_C(0x01), &event) == 0);
    CHECK(ram[0x0201] == UINT8_C(0xef));

    /* A stale/backward signal is ignored; a forward signal terminates. */
    CHECK(write_port(&boot, 0U, UINT8_C(0x82), &event) == 0);
    CHECK(boot.destination == UINT16_C(0x0202));
    CHECK(write_port(&boot, 2U, UINT8_C(0x00), &event) == 0);
    CHECK(write_port(&boot, 3U, UINT8_C(0x02), &event) == 0);
    CHECK(write_port(&boot, 1U, 0U, &event) == 0);
    CHECK(write_port(&boot, 0U, UINT8_C(0x03), &event) == 0);
    CHECK(event.start_cpu);
    CHECK(event.registers.pc == UINT16_C(0x0200));
    CHECK(event.registers.a == 0U && event.registers.x == 0U &&
          event.registers.y == 0U && event.registers.sp == UINT8_C(0xef) &&
          event.registers.psw == MIPSNES2_SPC700_FLAG_Z);
    CHECK(boot.state == MIPSNES2_SMP_BOOT_COMPLETE);
    return 0;
}

static int test_blocks_wrap_reset_and_failure(void)
{
    mipsnes2_apu_io_ports ports;
    mipsnes2_smp_memory memory;
    mipsnes2_smp_boot boot;
    mipsnes2_smp_boot_event event;
    uint8_t value;

    (void)memset(ram, UINT8_C(0xa5), sizeof(ram));
    mipsnes2_apu_io_ports_init(&ports);
    CHECK(mipsnes2_smp_memory_init(
        &memory, ram, sizeof(ram), &ports, NULL, 0U));
    CHECK(mipsnes2_smp_boot_init(&boot, &memory));
    CHECK(write_port(&boot, 2U, UINT8_C(0xfe), &event) == 0);
    CHECK(write_port(&boot, 3U, UINT8_C(0xff), &event) == 0);
    CHECK(write_port(&boot, 1U, UINT8_C(1), &event) == 0);
    CHECK(write_port(&boot, 0U, UINT8_C(0xcc), &event) == 0);
    CHECK(write_port(&boot, 1U, UINT8_C(0x11), &event) == 0);
    CHECK(write_port(&boot, 0U, 0U, &event) == 0);
    CHECK(write_port(&boot, 1U, UINT8_C(0x22), &event) == 0);
    CHECK(write_port(&boot, 0U, UINT8_C(1), &event) == 0);
    CHECK(ram[0xfffe] == UINT8_C(0x11) &&
          ram[0xffff] == UINT8_C(0x22) && boot.destination == 0U);

    CHECK(write_port(&boot, 2U, UINT8_C(0x00), &event) == 0);
    CHECK(write_port(&boot, 3U, UINT8_C(0x03), &event) == 0);
    CHECK(write_port(&boot, 1U, UINT8_C(1), &event) == 0);
    CHECK(write_port(&boot, 0U, UINT8_C(4), &event) == 0);
    CHECK(boot.state == MIPSNES2_SMP_BOOT_WAIT_FIRST_BYTE);
    CHECK(write_port(&boot, 1U, UINT8_C(0x33), &event) == 0);
    CHECK(write_port(&boot, 0U, 0U, &event) == 0);
    CHECK(ram[0x0300] == UINT8_C(0x33));

    CHECK(mipsnes2_smp_boot_reset(&boot));
    CHECK(boot.state == MIPSNES2_SMP_BOOT_WAIT_COMMAND);
    CHECK(mipsnes2_apu_io_ports_cpu_read(
        &ports, UINT16_C(0x2140), &value));
    CHECK(value == UINT8_C(0xaa));

    /* TEST at $F0 is still explicitly unsupported by the memory map. */
    CHECK(write_port(&boot, 2U, UINT8_C(0xf0), &event) == 0);
    CHECK(write_port(&boot, 3U, 0U, &event) == 0);
    CHECK(write_port(&boot, 1U, UINT8_C(1), &event) == 0);
    CHECK(write_port(&boot, 0U, UINT8_C(0xcc), &event) == 0);
    CHECK(write_port(&boot, 1U, UINT8_C(0x77), &event) == 0);
    CHECK(!mipsnes2_smp_boot_cpu_write(
        &boot, UINT16_C(0x2140), 0U, &event));
    CHECK(boot.state == MIPSNES2_SMP_BOOT_FAILED);
    return 0;
}

int main(void)
{
    CHECK(test_upload_and_start() == 0);
    CHECK(test_blocks_wrap_reset_and_failure() == 0);
    (void)puts("clean-room S-SMP boot protocol: OK");
    return 0;
}
