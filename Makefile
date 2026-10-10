# Host-only foundation build. Console SDK builds stay in platform/.
CC ?= cc
AR ?= ar
PYTHON ?= python3
BUILD_DIR ?= build/host
CPPFLAGS ?=
CFLAGS ?= -O2 -g
LDFLAGS ?=
LDLIBS ?=
WARNINGS = -std=c99 -Wall -Wextra -Wpedantic -Werror
INCLUDES = -I.
LIB = $(BUILD_DIR)/libmipsnes2-core.a
HOST = $(BUILD_DIR)/mipsnes2-host
TEST = $(BUILD_DIR)/test-contracts
TEST_NOLOG = $(BUILD_DIR)/test-contracts-nolog
TEST_CPU_BUS = $(BUILD_DIR)/test-cpu-bus
CORE_OBJS = $(BUILD_DIR)/log.o $(BUILD_DIR)/bus.o $(BUILD_DIR)/cpu.o

.PHONY: all test check clean
all: $(HOST)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/log.o: core/common/log.c core/common/log.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/bus.o: core/bus/bus.c core/bus/bus.h core/bus/interface.h core/common/address.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/cpu.o: core/cpu/65c816/cpu.c core/cpu/65c816/cpu.h core/bus/interface.h core/common/address.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(LIB): $(CORE_OBJS)
	$(AR) rcs $@ $^

$(HOST): platform/host/main.c core/common/log.h core/common/project.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST): tests/test_contracts.c core/common/address.h core/common/log.h core/bus/interface.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_NOLOG): tests/test_contracts.c core/common/log.c core/common/log.h core/common/address.h core/bus/interface.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) -DMIPSNES2_ENABLE_LOGGING=0 tests/test_contracts.c core/common/log.c $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_CPU_BUS): tests/test_cpu_bus.c core/bus/bus.h core/cpu/65c816/cpu.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

check:
	$(PYTHON) tools/check_repository.py

test: all $(TEST) $(TEST_NOLOG) $(TEST_CPU_BUS) check
	$(TEST)
	$(TEST_NOLOG)
	$(TEST_CPU_BUS)
	$(PYTHON) tests/test_host.py $(HOST)

clean:
	$(RM) -r $(BUILD_DIR)

-include $(BUILD_DIR)/log.d $(BUILD_DIR)/bus.d $(BUILD_DIR)/cpu.d
