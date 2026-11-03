
CC ?= gcc
CPPFLAGS += -Iinclude
CFLAGS ?= -O2
CFLAGS += -std=gnu11 -Wall -Wextra -Wpedantic

TARGET := build/cxl-fw-client

SOURCES := \
	src/main.c \
	src/core/utils.c \
	src/core/transfer_request.c \
	src/core/mailbox.c \
	src/commands/query.c \
	src/commands/get_fw_info.c \
	src/commands/transfer_fw.c \
	src/commands/abort_fw.c \
	tests/test_header.c \
	tests/test_transfer_request.c

OBJECTS := $(SOURCES:%.c=build/%.o)

# --------------------------------------------------
# Configurable defaults
# --------------------------------------------------

SIZE ?= 8192
SLOT ?= 2
CAPACITY ?= 2048

# Synthetic firmware test image
TEST_FW := firmware/synthetic-test-8k.bin

# --------------------------------------------------
# Phony targets
# --------------------------------------------------

.PHONY: all clean clean-test-firmware
.PHONY: query get-fw-info abort-fw-transfer
.PHONY: test-header test-transfer-request test
.PHONY: transfer-dry-run test-firmware
.PHONY: transfer-file-dry-run transfer-fw

# --------------------------------------------------
# Build
# --------------------------------------------------

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

build/%.o: %.c include/cxl_fw.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# --------------------------------------------------
# Device commands (require sudo)
# --------------------------------------------------

query: $(TARGET)
	sudo ./$(TARGET) query

get-fw-info: $(TARGET)
	sudo ./$(TARGET) get-fw-info

abort-fw-transfer: $(TARGET)
	sudo ./$(TARGET) abort-fw-transfer

# --------------------------------------------------
# Local tests (no device access)
# --------------------------------------------------

test-header: $(TARGET)
	./$(TARGET) test-header

test-transfer-request: $(TARGET)
	./$(TARGET) test-transfer-request

test: test-header test-transfer-request

# --------------------------------------------------
# Transfer planner: size-only dry-run
# --------------------------------------------------

transfer-dry-run: $(TARGET)
	./$(TARGET) transfer-fw \
		--size $(SIZE) \
		--slot $(SLOT) \
		--payload-capacity $(CAPACITY) \
		--dry-run

# --------------------------------------------------
# Synthetic firmware image generation
# --------------------------------------------------

test-firmware: $(TEST_FW)
	@echo "Synthetic firmware image ready: $(TEST_FW)"
	@ls -lh $(TEST_FW)

$(TEST_FW):
	@mkdir -p $(dir $@)
	dd if=/dev/zero of=$@ bs=128 count=64 status=none

# --------------------------------------------------
# Transfer planner: file-based dry-run
# --------------------------------------------------

transfer-file-dry-run: $(TARGET) $(TEST_FW)
	./$(TARGET) transfer-fw \
		--file $(TEST_FW) \
		--slot $(SLOT) \
		--payload-capacity $(CAPACITY) \
		--dry-run

# --------------------------------------------------
# Actual firmware transfer (synthetic test image)
# --------------------------------------------------

transfer-fw: $(TARGET) $(TEST_FW)
	@echo "WARNING: Actual firmware transfer will be attempted."
	@echo "Image: $(TEST_FW)"
	@echo "Target slot: $(SLOT)"
	@echo "Use only a disposable QEMU guest/device."
	sudo ./$(TARGET) transfer-fw \
		--file $(TEST_FW) \
		--slot $(SLOT) \
		--payload-capacity $(CAPACITY) \
		--execute

# --------------------------------------------------
# Cleanup
# --------------------------------------------------

clean:
	rm -rf build

clean-test-firmware:
	rm -f $(TEST_FW)
