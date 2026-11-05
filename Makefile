CC ?= gcc
CPPFLAGS += -Iinclude -Isrc/fault_injection/protocol
CFLAGS ?= -O2
CFLAGS += -std=gnu11 -Wall -Wextra -Wpedantic
TARGET := build/cxl-fw-client
GENERATOR := build/generate-test-firmware
SOURCES := src/main.c src/core/utils.c src/core/transfer_request.c src/core/mailbox.c src/core/transfer_engine.c src/core/fault_log.c src/commands/query.c src/commands/get_fw_info.c src/commands/transfer_fw.c src/commands/abort_fw.c src/fault_injection/fault_inject.c src/fault_injection/protocol/fault_common.c src/fault_injection/protocol/fault_cases.c src/fault_injection/protocol/invalid_action.c src/fault_injection/protocol/invalid_slot.c src/fault_injection/protocol/out_of_sequence.c src/fault_injection/protocol/duplicate_offset.c src/fault_injection/protocol/skipped_offset.c src/fault_injection/protocol/out_of_range_offset.c src/fault_injection/protocol/bit_flipped_data.c src/fault_injection/protocol/truncated_payload.c src/fault_injection/protocol/corrupted_reserved_fields.c tests/test_header.c tests/test_transfer_request.c
OBJECTS := $(SOURCES:%.c=build/%.o)
SIZE ?= 8192
SLOT ?= 2
CAPACITY ?= 2048
TEST_FW := firmware/synthetic-test-8k.bin
.PHONY: all clean test test-header test-transfer-request query get-fw-info abort-fw-transfer transfer-dry-run transfer-file-dry-run test-firmware fault-dry-run fault-inject regression-live regression
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
query: $(TARGET)
	sudo ./$(TARGET) query
get-fw-info: $(TARGET)
	sudo ./$(TARGET) get-fw-info
abort-fw-transfer: $(TARGET)
	sudo ./$(TARGET) abort-fw-transfer
transfer-dry-run: $(TARGET)
	./$(TARGET) transfer-fw --size $(SIZE) --slot $(SLOT) --payload-capacity $(CAPACITY) --dry-run
$(TEST_FW): $(GENERATOR)
	./$(GENERATOR) $@ 8192
test-firmware: $(TEST_FW)
	ls -lh $(TEST_FW)
transfer-file-dry-run: $(TARGET) $(TEST_FW)
	./$(TARGET) transfer-fw --file $(TEST_FW) --slot $(SLOT) --payload-capacity $(CAPACITY) --dry-run
fault-dry-run: $(TARGET)
	./$(TARGET) fault-inject --all --dry-run
fault-inject: $(TARGET)
	sudo ./$(TARGET) fault-inject --all
test-header: $(TARGET)
	./$(TARGET) test-header
test-transfer-request: $(TARGET)
	./$(TARGET) test-transfer-request
test: test-header test-transfer-request
regression: test fault-dry-run
	@echo "Local regression checks complete. Device health, bootability and clean-update recovery remain pending Goal 5."
regression-live: $(TARGET)
	@echo "Live regression is not implemented yet; see docs/REGRESSION.md."
	@exit 2
clean:
	rm -rf build