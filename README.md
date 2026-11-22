# CXL Firmware Management & Fault-Injection Framework

An in-band firmware-management and fault-injection framework for a QEMU-emulated CXL Type 3 memory device.

The project implements an independent Linux userspace client for CXL firmware-management operations and a fault-injection harness for examining firmware-transfer behaviour under protocol-level corruption and QEMU process interruptions.

## Problem Statement

Build an in-band fault-injection and validation harness for the CXL CCI mailbox firmware-update sequence, running entirely against a QEMU-emulated CXL Type 3 device.

The framework deliberately introduces malformed firmware-transfer requests and process-level interruptions to examine how the emulated device handles abnormal firmware-update sequences.

## Objectives

- Understand the CXL firmware-management and CCI mailbox workflow.
- Study libcxlmi as a reference implementation.
- Build an independent in-band firmware-management client.
- Inject protocol-level faults into firmware-transfer requests.
- Simulate process-level interruptions during firmware transfer.
- Automate test execution, logging and report generation.

---

## Features

### Firmware-Management Client

Independent C userspace client communicating through the Linux CXL ioctl interface.

- Query supported CXL commands.
- Retrieve firmware information.
- Construct firmware-transfer requests.
- Plan firmware transfers.
- Execute firmware transfers.
- Abort firmware transfers.
- Generate synthetic firmware images.

### Protocol-Level Fault Injection

Nine fault scenarios:

- Invalid action
- Invalid slot
- Out-of-sequence request
- Duplicate offset
- Skipped offset
- Out-of-range offset
- Bit-flipped data
- Truncated payload
- Corrupted reserved fields

Test results are recorded in CSV format, including expected and actual mailbox return values, ioctl status and verdicts.

### Process-Level Fault Injection

Controlled QEMU termination during firmware-transfer execution.

| Test | Injection Point |
|---|---|
| PF1 | Before INITIATE |
| PF2 | After INITIATE |
| PF3-1 | After CONTINUE 1 |
| PF3-2 | After CONTINUE 2 |
| PF4 | After final CONTINUE, before END |

The host controller handles QEMU termination, restart, guest connectivity verification and CXL device availability checks.

### Automated Testing & Reporting

- Local unit tests.
- Protocol-level fault-injection execution.
- Process-level fault-injection suite.
- Sequential execution of all process-level checkpoints.
- CSV and JSONL logging.
- Run-specific test identification.
- Automated test report generation.

---

## Architecture

The framework uses two operating-system environments.

| Component | OS | Responsibility |
|---|---|---|
| QEMU emulator | Arch Linux host | Runs the CXL Type 3 virtual device |
| Process-fault controller | Arch Linux host | Controls QEMU interruption, restart and logging |
| Firmware-management client | Ubuntu guest | Communicates with the CXL device |
| Protocol fault injection | Ubuntu guest | Sends deliberately modified mailbox requests |
| Protocol logger | Ubuntu guest | Records test results in CSV |

The guest communicates with the emulated CXL device through the Linux CXL subsystem and `CXL_MEM_SEND_COMMAND`.

---

## Requirements

### Arch Linux Host

- Linux
- QEMU with CXL support
- KVM
- Python 3
- Configured Ubuntu guest disk
- SSH connectivity to the guest

### Ubuntu Guest

- Linux kernel with CXL memory-device support
- `CONFIG_CXL_MEM_RAW_COMMANDS` enabled
- CXL raw mailbox ioctl support
- GCC
- GNU Make
- QEMU-emulated CXL device exposed as `/dev/cxl/mem0`

---

## Setup

Clone the repository:

```bash
git clone https://github.com/Parth-2110/cxl-firmware-project.git
cd cxl-firmware-project
git checkout aarya/cxl-userspace-client
```

Build the client inside the Ubuntu guest:

```bash
make
```

Generate the synthetic firmware test image:

```bash
make test-firmware
```

Run local tests:

```bash
make test
```

---

## Client Usage

Commands are executed inside the **Ubuntu guest**, from the project directory.

### Query supported commands

```bash
make query
```

### Get firmware information

```bash
make get-fw-info
```

### Generate a transfer plan

```bash
make transfer-dry-run SIZE=8192 SLOT=2 CAPACITY=2048
```

### Execute firmware transfer

```bash
make transfer-fw FILE=firmware/synthetic-test-8k.bin SLOT=2 CAPACITY=2048
```

### Abort firmware transfer

```bash
make abort-fw-transfer
```

---

## Protocol-Level Fault Injection

Inside the Ubuntu guest:

Display the fault catalogue:

```bash
make fault-dry-run
```

Execute the protocol fault-injection suite:

```bash
make fault-inject
```

The client requests explicit confirmation before executing the fault cases.

Results are recorded in:

```text
logs/protocol-fault-injection.csv
```

The CSV contains run identifiers, timestamps, fault cases, request steps, expected and actual mailbox returns, ioctl status, verdicts and notes.

---

## Process-Level Fault Injection

Process-level fault injection is executed from the **Arch Linux host**.

Run all defined process-level checkpoints:

```bash
python3 tools/host/cxl_process_fault_controller.py --all
```

Individual checkpoints can also be executed:

```bash
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF1
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF2
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF3 --continue-index 1
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF3 --continue-index 2
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF4
```

The controller performs the QEMU lifecycle management and post-restart checks.

---

## Logging & Reports

### Protocol-Level Logs

Location inside the Ubuntu guest:

```text
logs/protocol-fault-injection.csv
```

### Process-Level Logs

Location on the Arch host:

```text
~/Projects/qemu/process-fault-logs/
```

Includes:

- Controller event logs in JSONL.
- Separate QEMU output logs.
- Test lifecycle events.
- Checkpoint and restart records.

### Protocol-Level Report

Generated using:

```bash
python3 tools/reports/generate_protocol_report.py
```

### Process-Level Report

Generated using the process-level report generator with the recorded suite ID.

---

## Documentation

| Document | Description |
|---|---|
| [`CORE_PROJECT_CONFIGURATION.md`](docs/CORE_PROJECT_CONFIGURATION.md) | Environment, QEMU configuration, CXL setup and controller configuration |
| [`CLIENT_USAGE.md`](docs/CLIENT_USAGE.md) | Client build, commands and firmware-management operations |
| [`PROTOCOL_FAULT_TESTING.md`](docs/PROTOCOL_FAULT_TESTING.md) | Protocol fault catalogue, execution, logging and reports |
| [`PROCESS_FAULT_TESTING.md`](docs/PROCESS_FAULT_TESTING.md) | QEMU process fault injection, checkpoints and controller operation |
| [`PROJECT_PROGRESS.md`](docs/PROJECT_PROGRESS.md) | Progress and achievements organized around the original five goals |

---

## Project Results

The project establishes a working CXL firmware-management client and fault-injection framework against a QEMU-emulated CXL Type 3 device.

It combines independent mailbox command construction, protocol-level fault injection, controlled QEMU process interruption, automated test execution, logging and reporting into a reproducible testing environment.