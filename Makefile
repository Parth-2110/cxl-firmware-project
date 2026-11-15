# CXL Firmware Client - Makefile
# Run inside the Ubuntu QEMU GUEST, from ~/cxl-firmware-client.
# Host-only QEMU launchers (~/open-cxl-lab.sh, ~/start-cxl-vm.sh)
# must be run separately in the PC1/WSL HOST terminal.

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2
BUILD   := build
IMAGE   ?= tests/firmware-images/test_fw.bin
FAULT   ?=

.PHONY: help check-env device build build-transfer build-info build-fault \
        build-tools build-all binaries fw-info fw-layout commands bg-status \
        transfer transfer-submit image-info fault-dry fault-submit fault-dry-all \
        quick full reports quick-report full-report test-collection \
        project-status files git-log

.DEFAULT_GOAL := help

help:
	@echo "CXL Firmware Client - available targets"
	@echo "Environment/device: make check-env | make device"
	@echo "Build:              make build | make binaries"
	@echo "Device queries:     make fw-info | make fw-layout | make commands | make bg-status"
	@echo "Transfer:           make transfer"
	@echo "                    make transfer-submit IMAGE=tests/firmware-images/test_fw.bin"
	@echo "Image details:      make image-info"
	@echo "Fault dry run:      make fault-dry FAULT=invalid-action"
	@echo "Fault submission:   make fault-submit FAULT=invalid-action"
	@echo "All fault dry runs: make fault-dry-all"
	@echo "Goal 5 tests:       make quick | make full"
	@echo "Reports:            make reports | make quick-report | make full-report"
	@echo "Project:            make project-status | make files | make git-log"
	@echo ""
	@echo "Host QEMU launchers and destructive process-interruption steps are intentionally separate."

check-env:
	@test -d .git || { echo "ERROR: Run make from the cxl-firmware-client repository root."; exit 1; }
	@test -x .venv/bin/python || { echo "ERROR: Python venv missing. Create it with: python3 -m venv .venv"; exit 1; }
	@test -f "$(IMAGE)" || { echo "ERROR: Firmware image not found: $(IMAGE)"; exit 1; }
	@echo "Repository: $$(pwd)"
	@echo "Python:     $$(.venv/bin/python --version)"
	@echo "Pytest:     $$(.venv/bin/python -m pytest --version)"
	@echo "Image:      $(IMAGE)"
	@echo "Environment checks passed."

device:
	@ls -l /dev/cxl/
	@ls -l /dev/cxl/mem0
	sudo cxl list
	@lspci -nn | grep -i -E 'CXL|memory' || true

build:
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) src/transfer_fw.c -o $(BUILD)/transfer_fw
	$(CC) $(CFLAGS) src/get_fw_info.c -o $(BUILD)/get_fw_info
	$(CC) $(CFLAGS) src/fault_inject.c -o $(BUILD)/fault_inject
	$(CC) $(CFLAGS) tools/check_bg_status.c -o $(BUILD)/check_bg_status
	$(CC) $(CFLAGS) tools/check_fw_layout.c -o $(BUILD)/check_fw_layout
	$(CC) $(CFLAGS) tools/query_commands.c -o $(BUILD)/query_commands
	$(CC) $(CFLAGS) tests/fault-injection/process_fault.c -o $(BUILD)/process_fault

build-transfer:
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) src/transfer_fw.c -o $(BUILD)/transfer_fw

build-info:
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) src/get_fw_info.c -o $(BUILD)/get_fw_info

build-fault:
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) src/fault_inject.c -o $(BUILD)/fault_inject

build-tools: build
	@echo "All client and helper utilities compiled."

build-all: build

binaries:
	@ls -lh $(BUILD)/

fw-info:
	sudo ./$(BUILD)/get_fw_info

fw-layout:
	sudo ./$(BUILD)/check_fw_layout

commands:
	sudo ./$(BUILD)/query_commands

bg-status:
	sudo ./$(BUILD)/check_bg_status

transfer:
	sudo ./$(BUILD)/transfer_fw

transfer-submit:
	@test -f "$(IMAGE)" || { echo "ERROR: Firmware image not found: $(IMAGE)"; exit 1; }
	sudo ./$(BUILD)/transfer_fw --submit "$(IMAGE)"

image-info:
	@ls -lh tests/firmware-images/
	@file "$(IMAGE)"
	@wc -c "$(IMAGE)"
	@sha256sum "$(IMAGE)"

fault-dry:
	@test -n "$(FAULT)" || { echo "Usage: make fault-dry FAULT=invalid-action"; exit 2; }
	./$(BUILD)/fault_inject --fault "$(FAULT)"

fault-submit:
	@test -n "$(FAULT)" || { echo "Usage: make fault-submit FAULT=invalid-action"; exit 2; }
	@echo "WARNING: submits a fault request. Use only on the intended QEMU test device, never production hardware."
	sudo ./$(BUILD)/fault_inject --fault "$(FAULT)" --submit

fault-dry-all:
	@set -e; \
	for fault in invalid-action invalid-slot out-of-range-offset bit-flip truncated-payload duplicate-offset continue-without-initiate end-without-initiate out-of-order-offset; do \
		echo ""; echo "===== Dry run: $$fault ====="; \
		./$(BUILD)/fault_inject --fault "$$fault"; \
	done

quick:
	./run-regression.sh quick

full:
	./run-regression.sh full

reports:
	@ls -lh reports/goal5/

quick-report:
	@cat reports/goal5/quick-results.txt

full-report:
	@cat reports/goal5/full-results.txt

test-collection:
	.venv/bin/python -m pytest --collect-only -q tests/regression

project-status:
	@echo "Branch: $$(git branch --show-current)"
	git status -sb

files:
	find src tools tests docs -maxdepth 3 -type f | sort

git-log:
	git log --oneline --decorate -5
