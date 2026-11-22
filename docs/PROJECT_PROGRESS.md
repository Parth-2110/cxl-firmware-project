# PROJECT PROGRESS

## Project Overview

Build an in-band fault-injection and validation harness for the CXL CCI mailbox firmware-update sequence, running against a QEMU-emulated CXL Type 3 device.

The project implements an independent userspace firmware-management client, protocol-level fault injection, QEMU process-level fault injection, and an automated testing and reporting workflow.

---

## Goal 1 — Environment & Protocol Literacy

**Status: Completed**

- Established a working QEMU, Linux and CXL environment.
- Booted a QEMU guest with a CXL Type 3 device.
- Verified kernel binding and availability of `/dev/cxl/mem0`.
- Studied the Linux CXL mailbox implementation and relevant kernel code.
- Examined QEMU's CXL mailbox implementation.
- Understood the userspace-to-kernel-to-QEMU mailbox command flow.
- Studied the firmware-management sequence, including Get FW Info, Transfer FW and Activate FW.

**Outcome:** Established the environment and protocol understanding required for independent implementation and testing.

---

## Goal 2 — Reference Study via libcxlmi

**Status: Completed**

- Built libcxlmi from source.
- Studied its command construction and CXL management implementation.
- Used it as a reference for understanding well-formed CXL management operations.
- Examined firmware-management command flow and request structure.

libcxlmi served as a reference during development; the final client does not depend on it at runtime.

**Outcome:** Established a reference understanding of CXL management operations before implementing an independent client.

---

## Goal 3 — In-Band Firmware Update, Built from Scratch

**Status: Completed**

Developed an independent C userspace client for direct communication with the CXL memory device using the Linux `CXL_MEM_SEND_COMMAND` ioctl.

### Implemented

- Supported-command discovery.
- Get FW Info.
- Transfer FW request construction.
- INITIATE, CONTINUE and END transfer actions.
- Firmware-transfer planning and dry-run functionality.
- Firmware-transfer execution.
- Firmware-transfer abort.
- Synthetic firmware image generation.
- Local request/header tests.

The client independently constructs and sends CXL mailbox requests rather than relying on cxl-cli's firmware-loader abstraction or libcxlmi.

**Outcome:** Implemented the independent userspace firmware-management client used as the basis for the fault-injection tests.

---

## Goal 4 — Fault Injection

**Status: Completed**

Implemented protocol-level and process-level fault injection against the QEMU-emulated CXL Type 3 device.

### Protocol-Level Faults

Implemented nine fault scenarios:

| Fault | Description |
|---|---|
| Invalid action | Injects an invalid Transfer FW action |
| Invalid slot | Targets an invalid or inappropriate firmware slot |
| Out of sequence | Sends Transfer FW actions in an invalid sequence |
| Duplicate offset | Repeats a transfer offset |
| Skipped offset | Introduces a gap in transfer offsets |
| Out-of-range offset | Sends a transfer request outside permitted bounds |
| Bit-flipped data | Modifies firmware-transfer data |
| Truncated payload | Sends a shortened payload |
| Corrupted reserved fields | Modifies reserved fields in the request |

### Process-Level Faults

Implemented QEMU process termination at controlled firmware-transfer checkpoints.

| Test | Injection Point |
|---|---|
| PF1 | Before INITIATE |
| PF2 | After INITIATE |
| PF3-1 | After CONTINUE 1 |
| PF3-2 | After CONTINUE 2 |
| PF4 | After final CONTINUE, before END |

The controller handles QEMU termination, restart, guest connectivity verification and CXL device availability checks.

### Logging

- Protocol-level results recorded in CSV.
- Process-level events recorded in JSONL.
- Separate QEMU output logs.
- Test run identifiers for distinguishing individual executions.

**Outcome:** Implemented the fault catalogue and its execution mechanisms against the independent firmware-management client.

---

## Goal 5 — Validation, Regression Suite & Reporting

**Status: Completed**

Automated the execution of the testing workflows and consolidated their results into repeatable commands and reports.

### Implemented

- Local client tests.
- Protocol-level fault-injection execution.
- Process-level fault-injection execution.
- Single-command execution of the process-level fault suite.
- Test-result logging.
- Protocol-level report generation.
- Process-level report generation.
- PASS/FAIL result tracking.
- Run-specific identification and logging.

The process-level controller can execute the defined checkpoints sequentially through a single command, handling the QEMU lifecycle and test execution.

### Reporting and Results

- Protocol fault results: CSV.
- Process fault controller events: JSONL.
- QEMU execution output: separate log files.
- Protocol test report: generated Markdown report.
- Process test report: generated from the recorded test execution.

**Outcome:** Established a repeatable testing workflow with consolidated execution, result recording and report generation.

---

## Project Achievements

- Established a QEMU-emulated CXL Type 3 testing environment.
- Studied the CXL firmware-management and CCI mailbox flow.
- Built libcxlmi as a reference.
- Developed an independent C userspace firmware-management client.
- Implemented firmware-transfer request construction and execution.
- Implemented protocol-level fault injection.
- Implemented QEMU process-level fault injection.
- Automated fault-test execution.
- Implemented logging and report generation.
- Documented the environment, client, fault tests, execution procedures and results.