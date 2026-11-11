# Protocol-Level Fault Injection

All commands in this document are executed inside the **Ubuntu guest OS**, from `~/fw-management`.

## 1. Overview

Protocol-level fault injection tests the QEMU CXL Type 3 device's handling of deliberately unusual or malformed `TRANSFER_FW` mailbox requests.

The fault-injection suite uses the same request-building and mailbox-submission infrastructure as the normal firmware-transfer client, while independently constructing the request mutations and sequences.

The suite does not activate firmware.

## 2. Fault Catalogue

| Test | Injected condition | Expected mailbox return |
|---|---|---:|
| `invalid_action` | Invalid action byte (`0xFF`) | `0x02` |
| `invalid_slot` | FULL request to active slot | `0x0B` |
| `out_of_sequence` | CONTINUE without INITIATE | `0x02` |
| `duplicate_offset` | Repeated offset | `0x00` |
| `skipped_offset` | Offset gap | `0x00` |
| `out_of_range_offset` | Request exceeds 32 MiB limit | `0x02` |
| `bit_flipped_data` | First data byte modified | `0x00` |
| `truncated_payload` | Short data section | `0x00` |
| `corrupted_reserved_fields` | Reserved bytes set to nonzero values | `0x00` |

The expected values above reflect the current QEMU test configuration and observed behaviour.

A PASS means that the actual mailbox return matched the expected return. It does not necessarily mean the device rejected an unusual request.

## 3. Dry Run

```bash
make fault-dry-run
```

Displays the fault catalogue without opening the CXL device or issuing ioctls.

## 4. Live Fault Injection

```bash
make fault-inject
```

The client:

1. Opens `/dev/cxl/mem0`.
2. Retrieves firmware slot information.
3. Identifies an inactive slot.
4. Reads the device's `payload_max`.
5. Performs initial ABORT cleanup.
6. Requests explicit confirmation.
7. Executes all protocol fault cases.
8. Performs ABORT cleanup after each case.
9. Prints the final PASS/FAIL summary.

Required confirmation:

```text
INJECT
```

Use a disposable QEMU guest.

## 5. Logging

Default log:

```text
logs/protocol-fault-injection.csv
```

The CSV is append-only. Existing records are retained when another run is executed.

Each run has a unique run ID and timestamp.

### Recorded fields

| Field | Description |
|---|---|
| `run_id` | Identifier for the execution |
| `timestamp` | Time of the event |
| `test` | Fault case name |
| `step` | Request or cleanup step |
| `expected_mailbox_return` | Expected mailbox return value |
| `actual_mailbox_return` | Actual mailbox return value |
| `ioctl_status` | Whether the ioctl succeeded |
| `verdict` | PASS or FAIL |
| `note` | Additional information |

### Custom log path

The client supports overriding the log path:

```bash
sudo ./build/cxl-fw-client fault-inject --all --log logs/custom-run.csv
```

## 6. Understanding the Results

The suite compares the actual mailbox return value with the expected value for each test.

- PASS: Actual mailbox return matches the expected value.
- FAIL: Actual mailbox return differs from the expected value or the ioctl fails.

Some malformed requests are intentionally expected to be accepted by the current QEMU implementation. These observations are recorded rather than treated as proof of firmware validity.

The suite does not verify firmware image correctness, activation or bootability.

## 7. Relevant Source Files

```text
src/
├── core/
│   ├── mailbox.c
│   ├── transfer_engine.c
│   ├── transfer_request.c
│   └── fault_log.c
└── fault_injection/
    ├── fault_inject.c
    └── protocol/
        ├── fault_cases.c
        ├── fault_common.c
        ├── fault_cases.h
        └── individual fault implementations
```