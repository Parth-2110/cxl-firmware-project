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
	src/commands/fault_inject.c \
	tests/test_header.c \
	tests/test_transfer_request.c

OBJECTS := $(SOURCES:%.c=build/%.o)

SIZE ?= 8192
SLOT ?= 2
CAPACITY ?= 2048
TEST_FW := firmware/synthetic-test-8k.bin

.PHONY: all clean clean-test-firmware
.PHONY: query get-fw-info abort-fw-transfer fault-inject
.PHONY: test-header test-transfer-request test
.PHONY: transfer-dry-run test-firmware transfer-file-dry-run transfer-fw

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

build/%.o: %.c include/cxl_fw.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

query: $(TARGET)
	sudo ./$(TARGET) query

get-fw-info: $(TARGET)
	sudo ./$(TARGET) get-fw-info

abort-fw-transfer: $(TARGET)
	sudo ./$(TARGET) abort-fw-transfer

fault-inject: $(TARGET)
	sudo ./$(TARGET) fault-inject --all

test-header: $(TARGET)
	./$(TARGET) test-header

test-transfer-request: $(TARGET)
	./$(TARGET) test-transfer-request

test: test-header test-transfer-request

transfer-dry-run: $(TARGET)
	./$(TARGET) transfer-fw \
		--size $(SIZE) \
		--slot $(SLOT) \
		--payload-capacity $(CAPACITY) \
		--dry-run

test-firmware: $(TEST_FW)
	@echo "Synthetic firmware test image ready: $(TEST_FW)"
	@ls -lh $(TEST_FW)

$(TEST_FW):
	@mkdir -p $(dir $@)
	dd if=/dev/zero of=$@ bs=128 count=64 status=none

transfer-file-dry-run: $(TARGET) $(TEST_FW)
	./$(TARGET) transfer-fw \
		--file $(TEST_FW) \
		--slot $(SLOT) \
		--payload-capacity $(CAPACITY) \
		--dry-run

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

clean:
	rm -rf build

clean-test-firmware:
	rm -f $(TEST_FW)