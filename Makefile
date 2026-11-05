CC ?= gcc

CPPFLAGS += -Iinclude -Isrc/fault_injection/protocol
CFLAGS ?= -O2
CFLAGS += -std=gnu11 -Wall -Wextra -Wpedantic

TARGET := build/cxl-fw-client
GENERATOR := build/generate-test-firmware

SOURCES := \
	src/main.c \
	src/core/utils.c \
	src/core/transfer_request.c \
	src/core/mailbox.c \
	src/core/transfer_engine.c \
	src/core/fault_log.c \
	src/commands/query.c \
	src/commands/get_fw_info.c \
	src/commands/transfer_fw.c \
	src/commands/abort_fw.c \
	src/fault_injection/fault_inject.c \
	src/fault_injection/protocol/fault_common.c \
	src/fault_injection/protocol/fault_cases.c \
	src/fault_injection/protocol/invalid_action.c \
	src/fault_injection/protocol/invalid_slot.c \
	src/fault_injection/protocol/out_of_sequence.c \
	src/fault_injection/protocol/duplicate_offset.c \
	src/fault_injection/protocol/skipped_offset.c \
	src/fault_injection/protocol/out_of_range_offset.c \
	src/fault_injection/protocol/bit_flipped_data.c \
	src/fault_injection/protocol/truncated_payload.c \
	src/fault_injection/protocol/corrupted_reserved_fields.c \
	tests/test_header.c \
	tests/test_transfer_request.c

OBJECTS := $(SOURCES:%.c=build/%.o)

# Transfer configuration
SIZE ?= 8192
SLOT ?= 2
CAPACITY ?= 2048
FILE ?= firmware/synthetic-test-8k.bin

# Synthetic firmware generation
OUTPUT ?= firmware/synthetic-custom.bin
IMAGE_SIZE ?= 8192

# Default test firmware
TEST_FW := firmware/synthetic-test-8k.bin

.PHONY: all clean \
	test test-header test-transfer-request \
	query get-fw-info abort-fw-transfer \
	transfer-dry-run transfer-file-dry-run transfer-fw \
	test-firmware generate-firmware \
	fault-dry-run fault-inject \
	regression regression-live

# --------------------------------------------------
# Build
# --------------------------------------------------

all: $(TARGET) $(GENERATOR)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

build/%.o: %.c include/cxl_fw.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(GENERATOR): tests/generate_test_firmware.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf build

# --------------------------------------------------
# Device commands
# Run inside the Ubuntu guest OS
# --------------------------------------------------

query: $(TARGET)
	sudo ./$(TARGET) query

get-fw-info: $(TARGET)
	sudo ./$(TARGET) get-fw-info

abort-fw-transfer: $(TARGET)
	sudo ./$(TARGET) abort-fw-transfer

# --------------------------------------------------
# Firmware transfer
# --------------------------------------------------

# Size-only transfer planner
transfer-dry-run: $(TARGET)
	./$(TARGET) transfer-fw \
		--size $(SIZE) \
		--slot $(SLOT) \
		--payload-capacity $(CAPACITY) \
		--dry-run

# Generate the default 8 KiB synthetic firmware image
$(TEST_FW): $(GENERATOR)
	@mkdir -p $(dir $@)
	./$(GENERATOR) $@ 8192

test-firmware: $(TEST_FW)
	ls -lh $(TEST_FW)

# File-backed transfer planner
# Default FILE is the generated 8 KiB test image.
# A custom FILE must already exist.
transfer-file-dry-run: $(TARGET) $(GENERATOR)
	@if [ "$(FILE)" = "$(TEST_FW)" ] && [ ! -f "$(FILE)" ]; then \
		./$(GENERATOR) "$(FILE)" 8192; \
	fi
	@test -f "$(FILE)" || { \
		echo "Firmware file not found: $(FILE)"; \
		exit 2; \
	}
	./$(TARGET) transfer-fw \
		--file "$(FILE)" \
		--slot $(SLOT) \
		--payload-capacity $(CAPACITY) \
		--dry-run

# Actual firmware transfer
# Requires FILE, SLOT and CAPACITY.
# The client still requires typing TRANSFER.
transfer-fw: $(TARGET)
	@test -n "$(FILE)" || { echo "FILE is required."; exit 2; }
	@test -n "$(SLOT)" || { echo "SLOT is required."; exit 2; }
	@test -n "$(CAPACITY)" || { echo "CAPACITY is required."; exit 2; }
	@test -f "$(FILE)" || { echo "Firmware file not found: $(FILE)"; exit 2; }
	sudo ./$(TARGET) transfer-fw \
		--file "$(FILE)" \
		--slot $(SLOT) \
		--payload-capacity $(CAPACITY) \
		--execute

# --------------------------------------------------
# Synthetic firmware generation
# --------------------------------------------------

# Generate a custom synthetic firmware image.
# Example:
# make generate-firmware OUTPUT=firmware/synthetic-16k.bin IMAGE_SIZE=16384
generate-firmware: $(GENERATOR)
	@test -n "$(OUTPUT)" || { echo "OUTPUT is required."; exit 2; }
	@test -n "$(IMAGE_SIZE)" || { echo "IMAGE_SIZE is required."; exit 2; }
	@mkdir -p $(dir $(OUTPUT))
	./$(GENERATOR) "$(OUTPUT)" $(IMAGE_SIZE)
	ls -lh "$(OUTPUT)"

# --------------------------------------------------
# Fault injection
# --------------------------------------------------

fault-dry-run: $(TARGET)
	./$(TARGET) fault-inject --all --dry-run

fault-inject: $(TARGET)
	sudo ./$(TARGET) fault-inject --all

# --------------------------------------------------
# Tests
# --------------------------------------------------

test-header: $(TARGET)
	./$(TARGET) test-header

test-transfer-request: $(TARGET)
	./$(TARGET) test-transfer-request

test: test-header test-transfer-request

# --------------------------------------------------
# Regression
# --------------------------------------------------

regression: test fault-dry-run
	@echo "Local regression checks complete."
	@echo "Device health, bootability and clean-update recovery remain pending Goal 5."

regression-live: $(TARGET)
	@echo "Live regression is not implemented yet; see docs/REGRESSION.md."
	@exit 2