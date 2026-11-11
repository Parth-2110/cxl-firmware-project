
# CXL Firmware Management — Test Results

## 1. Overview

This document records the protocol-level and process-level fault-injection tests performed against the QEMU-emulated CXL Type 3 device.

The results reflect the configured QEMU environment and the observed test runs.

## 2. Local Unit Tests

| Test | Result |
|---|---|
| Transfer header tests | PASS |
| Transfer request tests | PASS |

Both test suites completed successfully using:

```bash
make test
```

## 3. Protocol-Level Fault Injection

The protocol-level suite contains nine fault cases.

| Fault case | Expected mailbox return | Observed behaviour |
|---|---:|---|
| Invalid action | `0x02` | Expected rejection |
| Invalid slot | `0x0B` | Expected rejection |
| Out of sequence | `0x02` | Expected rejection |
| Duplicate offset | `0x00` | Accepted by QEMU |
| Skipped offset | `0x00` | Accepted by QEMU |
| Out of range offset | `0x02` | Expected rejection |
| Bit-flipped data | `0x00` | Accepted by QEMU |
| Truncated payload | `0x00` | Accepted by QEMU |
| Corrupted reserved fields | `0x00` | Accepted by QEMU |

The actual mailbox return values are compared against the expected values.

The CSV log records each test step, expected and actual mailbox return values, ioctl status and verdict.

Default log:

```text
logs/protocol-fault-injection.csv
```

A PASS indicates that the observed mailbox return matched the expected value. It does not establish that the resulting firmware image is valid.

## 4. Process-Level Fault Injection

Five process-fault scenarios were executed.

| Test | Injection point | QEMU SIGKILL | Restart | SSH restored | CXL device restored |
|---|---|---|---|---|---|
| PF1 | Before INITIATE | PASS | PASS | PASS | PASS |
| PF2 | After INITIATE | PASS | PASS | PASS | PASS |
| PF3-1 | After CONTINUE 1 | PASS | PASS | PASS | PASS |
| PF3-2 | After CONTINUE 2 | PASS | PASS | PASS | PASS |
| PF4 | After final CONTINUE, before END | PASS | PASS | PASS | PASS |

For each test:

- The controller received the expected checkpoint.
- QEMU was terminated using SIGKILL.
- QEMU was restarted.
- SSH became available again.
- `/dev/cxl/mem0` reappeared.
- The controller recorded test completion.

Process-fault logs are stored on the Arch host:

```text
~/Projects/qemu/process-fault-logs/
```

Each run has a separate JSONL event log and a corresponding QEMU output log.

## 5. Observations

### Protocol-level observations

QEMU accepted several unusual requests, including duplicate offsets, skipped offsets, bit-flipped data, truncated payloads and corrupted reserved fields.

These results describe the current QEMU implementation. They should not be interpreted as evidence that such requests are valid under every CXL device implementation.

### Process-level observations

The process-fault tests demonstrated that QEMU could be abruptly terminated and restarted at the selected transfer checkpoints.

The guest became reachable again over SSH, and its CXL memory device node reappeared.

## 6. Limitations

The following behaviours were not established by these tests:

- Resumption of an interrupted firmware transfer.
- Persistence of firmware-transfer state across QEMU restart.
- Correctness of a transferred firmware image.
- Firmware activation.
- Firmware-slot bootability.

The QEMU Type 3 device uses a volatile memory backend. CCI firmware-transfer state is reset when QEMU restarts.

Firmware activation was outside the project scope.

## 7. Overall Result

The implemented client, protocol fault-injection suite and process-fault controller were exercised against the configured QEMU environment.

The defined process-level acceptance criteria were met for PF1, PF2, PF3-1, PF3-2 and PF4.

The protocol-level suite recorded expected and actual mailbox results, including cases where QEMU accepted unusual requests.

The project does not claim firmware recovery, activation or bootability validation.