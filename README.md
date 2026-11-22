# CXL Firmware Management & Fault-Injection Framework

An in-band fault-injection and validation harness for the CXL CCI mailbox firmware-update sequence, operating against a QEMU-emulated CXL Type 3 memory device.

The project implements an independent userspace firmware-management client and a fault-injection framework for evaluating firmware-update transaction behaviour under malformed mailbox requests and simulated process interruptions.

## Problem Statement

Firmware updates are sensitive to malformed requests, interrupted transactions, and unexpected system failures.

Existing firmware-update tools generally execute valid, kernel-managed update sequences without exposing interfaces for deliberately injecting malformed or out-of-sequence mailbox requests. Similarly, existing RAS validation approaches focus primarily on hardware error reporting rather than firmware-update transaction handling.

This project provides a controlled environment for injecting faults into the CXL firmware-update sequence and observing the resulting behaviour.

## Objectives

- Understand the CXL firmware-management and CCI mailbox workflow.
- Study libcxlmi as a reference implementation.
- Develop an independent in-band firmware-management client.
- Inject protocol-level faults into firmware-transfer requests.
- Simulate QEMU process interruptions during firmware transfer.
- Automate testing, result collection and report generation.

## Features

### Independent Firmware-Management Client

A C userspace client implementing direct CXL mailbox communication through the Linux CXL ioctl interface.

Supports:

- Supported-command discovery.
- Get FW Info.
- Transfer FW request construction.
- INITIATE, CONTINUE and END actions.
- Firmware-transfer planning.
- Firmware-transfer execution.
- Transfer abort.

### Protocol-Level Fault Injection

Deliberately modifies firmware-transfer requests to examine mailbox handling under abnormal inputs.

Implemented fault categories:

- Invalid action
- Invalid slot
- Out-of-sequence requests
- Duplicate offsets
- Skipped offsets
- Out-of-range offsets
- Bit-flipped data
- Truncated payloads
- Corrupted reserved fields

### Process-Level Fault Injection

Simulates abrupt QEMU termination at selected firmware-transfer checkpoints, followed by QEMU restart and guest/device availability checks.

### Automated Testing & Reporting

- Local client tests.
- Protocol-level fault testing.
- Process-level fault testing.
- Sequential test execution.
- CSV and JSONL logging.
- Test-run identification.
- Automated Markdown report generation.

---

## Architecture

The framework operates across two Linux environments.

| Component | Environment | Responsibility |
|---|---|---|
| QEMU CXL device | Arch Linux host | Emulates the CXL Type 3 device |
| Process-fault controller | Arch Linux host | Controls QEMU interruption, restart and logging |
| Firmware-management client | Ubuntu guest | Communicates with the emulated CXL device |
| Protocol fault injection | Ubuntu guest | Sends modified mailbox requests |
| Test logging | Host and guest | Records test execution and outcomes |

The client communicates with the CXL device through the Linux CXL subsystem using `CXL_MEM_SEND_COMMAND`.

---

## Requirements

### Host Environment

- Linux
- QEMU with CXL support
- KVM
- Python 3

### Guest Environment

- Linux kernel with CXL memory-device support
- `CONFIG_CXL_MEM_RAW_COMMANDS` enabled
- CXL raw mailbox ioctl support
- GCC
- GNU Make
- QEMU-emulated CXL Type 3 device accessible through `/dev/cxl/mem0`

For complete environment configuration and system-specific details, see **[Core Project Configuration](docs/CORE_PROJECT_CONFIGURATION.md)**.

---

## Testing Framework

### Protocol-Level Testing

The protocol fault-injection suite examines the mailbox responses to deliberately modified firmware-transfer requests.

Results include expected and actual mailbox return values, ioctl status and test verdicts.

See **[Protocol Fault Testing](docs/PROTOCOL_FAULT_TESTING.md)** for the fault catalogue, test methodology, logging and reporting details.

### Process-Level Testing

The process fault-injection controller terminates QEMU at defined points during firmware-transfer execution and evaluates the restart process and guest/device availability.

See **[Process Fault Testing](docs/PROCESS_FAULT_TESTING.md)** for checkpoint definitions, controller configuration, test execution and logging.

---

## Documentation

| Document | Description |
|---|---|
| [Core Project Configuration](docs/CORE_PROJECT_CONFIGURATION.md) | Host and guest setup, QEMU configuration, CXL topology and environment-specific settings |
| [Client Usage](docs/CLIENT_USAGE.md) | Client commands, firmware operations, build instructions and usage |
| [Protocol Fault Testing](docs/PROTOCOL_FAULT_TESTING.md) | Protocol-level fault catalogue, testing and result logging |
| [Process Fault Testing](docs/PROCESS_FAULT_TESTING.md) | QEMU process-level faults, checkpoints and controller operation |
| [Project Progress](docs/PROJECT_PROGRESS.md) | Achievements organized around the original five project goals |

---

## Project Outcome

The project combines an independent CXL firmware-management client, protocol-level fault injection, process-level fault injection, automated test execution, and structured logging and reporting into a unified framework operating against a QEMU-emulated CXL Type 3 device.