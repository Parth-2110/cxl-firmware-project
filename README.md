
# CXL Firmware Management and Fault-Injection Framework

A Linux userspace framework for interacting with a QEMU-emulated CXL Type 3 memory device through the Linux CXL mailbox interface.

The project implements selected CXL firmware-management commands and provides protocol-level and QEMU process-level fault-injection tests for the `TRANSFER_FW` command.

It is designed to study firmware-update request handling, inject controlled faults, observe mailbox responses, and validate QEMU process restart behaviour.

## Project Objectives

1. Understand CXL firmware-management commands and the CCI mailbox interface.
2. Implement an independent C userspace client using `CXL_MEM_SEND_COMMAND`.
3. Implement firmware-transfer request construction and execution.
4. Inject protocol-level faults into `TRANSFER_FW` requests.
5. Inject process-level faults by terminating QEMU at controlled firmware-transfer checkpoints.
6. Record test outcomes using CSV and JSONL logs.

## Features

### Firmware-management client

- Query supported CXL commands.
- Retrieve firmware slot information.
- Plan firmware transfers without sending commands.
- Execute firmware transfers to an inactive slot.
- Abort an in-progress firmware transfer.
- Generate synthetic firmware images for testing.

### Protocol-level fault injection

The client injects controlled malformed or unusual `TRANSFER_FW` requests:

- Invalid action
- Invalid slot
- Out-of-sequence request
- Duplicate offset
- Skipped offset
- Out-of-range offset
- Bit-flipped data
- Truncated payload
- Corrupted reserved fields

Results are recorded in an append-only CSV log.

### Process-level fault injection

A Python controller running on the Arch host terminates QEMU at controlled points in the firmware-transfer sequence.

Test scenarios:

- PF1: Before INITIATE
- PF2: After INITIATE
- PF3-1: After CONTINUE 1
- PF3-2: After CONTINUE 2
- PF4: After the final CONTINUE, before END

The controller restarts QEMU and verifies SSH availability and the reappearance of `/dev/cxl/mem0`.

Results are recorded in host-side JSONL logs. QEMU output is recorded separately.

## Architecture

The project uses two operating-system environments.

| Component | Operating system | Responsibility |
|---|---|---|
| QEMU emulator | Arch Linux host | Runs the CXL Type 3 virtual device |
| Process-fault controller | Arch Linux host | Controls QEMU termination, restart and process-fault logging |
| Ubuntu guest | Ubuntu Linux | Runs the CXL userspace client |
| Protocol fault injection | Ubuntu guest | Sends malformed CXL mailbox requests |
| Protocol CSV logger | Ubuntu guest | Records expected and actual mailbox results |

## Requirements

### Arch Linux host

- Arch Linux or a compatible Linux distribution.
- QEMU built with CXL support.
- KVM support.
- Python 3.
- QEMU overlay disk containing the configured Ubuntu guest.
- SSH server enabled inside the guest.
- Host-side networking configured for guest SSH and checkpoint communication.

### Ubuntu guest

- Linux kernel with CXL memory-device support.
- `CONFIG_CXL_MEM_RAW_COMMANDS` enabled.
- `CXL_MEM_SEND_COMMAND` ioctl support.
- A QEMU-emulated CXL Type 3 device exposed as `/dev/cxl/mem0`.
- GCC.
- GNU Make.
- Python is not required for the C client.

## Repository Setup

Clone the repository and switch to the project branch:

```bash
git clone https://github.com/Parth-2110/cxl-firmware-project.git
cd cxl-firmware-project
git checkout aarya/cxl-userspace-client
```

### Build inside Ubuntu guest

```bash
make
```

Generate the default synthetic firmware image:

```bash
make test-firmware
```

Run local unit tests:

```bash
make test
```

For the full client command reference, see [Client Usage](docs/CLIENT_USAGE.md).

## Documentation

| Document | Description |
|---|---|
| [Client Usage](docs/CLIENT_USAGE.md) | Build and execute firmware-management commands |
| [Protocol Fault Testing](docs/PROTOCOL_FAULT_TESTING.md) | Protocol fault scenarios, commands, expected results and CSV logging |
| [Process Fault Testing](docs/PROCESS_FAULT_TESTING.md) | Arch host controller setup, PF1–PF4 execution and JSONL logging |
| [Test Results](docs/TEST_RESULTS.md) | Completed tests, observations and project limitations |

## Logging

### Protocol-level logs

Generated inside the Ubuntu guest:

```text
logs/protocol-fault-injection.csv
```

Contains run IDs, timestamps, fault cases, expected and actual mailbox return values, ioctl status and verdicts.

### Process-level logs

Generated on the Arch host:

```text
~/Projects/qemu/process-fault-logs/
```

Contains one JSONL controller event log per run and a separate QEMU stdout/stderr log.

## Scope and Limitations

- The project focuses on CXL firmware information and firmware transfer operations.
- Firmware activation is out of scope.
- Synthetic firmware images are test data, not vendor-signed production firmware.
- Protocol fault results describe QEMU's observed mailbox behaviour.
- Process-level validation establishes checkpoint reception, QEMU termination, QEMU restart, SSH recovery and CXL device availability.
- The project does not establish firmware-transfer resumption, firmware-state persistence, firmware correctness or bootability.

## Project Status

The implementation and the defined protocol-level and process-level test scenarios have been completed against the configured QEMU environment.

See [Test Results](docs/TEST_RESULTS.md) for the recorded outcomes and limitations.