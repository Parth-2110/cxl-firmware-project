# Protocol-Level Fault Injection

All commands in this document are executed inside the **Ubuntu guest OS**, from `~/fw-management`.

## 1. Overview

Protocol-level fault injection evaluates how the QEMU-emulated CXL Type 3 device handles deliberately malformed or unusual `TRANSFER_FW` mailbox requests.

The suite uses the firmware-transfer request construction and mailbox submission infrastructure of the client, while independently modifying request fields or sequences to introduce faults.

The suite does not activate firmware.

---

## 2. Fault Catalogue

| Test | Injected condition | Expected mailbox return |
|---|---|---:|
| `invalid_action` | Invalid action byte (`0xFF`) | `0x02` |
| `invalid_slot` | FULL request to active slot | `0x0B` |
| `out_of_sequence` | CONTINUE without INITIATE | `0x02` |
| `duplicate_offset` | Repeated offset | `0x00` |
| `skipped_offset` | Offset gap | `0x00` |
| `out_of_range_offset` | Request exceeds 32 MiB firmware limit | `0x02` |
| `bit_flipped_data` | First payload byte modified | `0x00` |
| `truncated_payload` | Short data section | `0x00` |
| `corrupted_reserved_fields` | Reserved bytes set to nonzero values | `0x00` |

The expected values reflect the current QEMU test configuration and observed behaviour.

A PASS means the actual mailbox return matched the expected return. It does **not** necessarily mean the device rejected the injected fault.

---

## 3. Dry Run

Execute:

```bash
make fault-dry-run
```

This displays the registered fault cases without opening the CXL device or issuing mailbox ioctls.

---

## 4. Execute Protocol Fault Tests

Execute the entire protocol fault suite:

```bash
make fault-inject
```

The client performs the following:

1. Opens `/dev/cxl/mem0`.
2. Retrieves firmware slot information.
3. Identifies an inactive firmware slot.
4. Reads the device's payload capacity.
5. Performs initial ABORT cleanup.
6. Requests explicit confirmation.
7. Executes the registered fault cases.
8. Performs ABORT cleanup after each fault case.
9. Prints the final PASS/FAIL summary.

Required confirmation:

```text
INJECT
```

Use a disposable QEMU guest for live fault injection.

---

## 5. CSV Logging

Default log location:

```text
logs/protocol-fault-injection.csv
```

The CSV is append-only. Subsequent executions add new records rather than replacing previous runs.

Each execution is associated with a run ID and timestamp.

### Recorded fields

| Field | Description |
|---|---|
| `run_id` | Execution identifier |
| `timestamp` | Event timestamp |
| `test` | Fault case name |
| `step` | Request or cleanup step |
| `expected_mailbox_return` | Expected mailbox return |
| `actual_mailbox_return` | Actual mailbox return |
| `ioctl_status` | ioctl execution status |
| `verdict` | PASS or FAIL |
| `note` | Additional information |

### Custom log path

To use a different CSV file:

```bash
sudo ./build/cxl-fw-client fault-inject --all --log logs/custom-run.csv
```

---

## 6. Generate a Protocol Fault Report

Execute the fault suite and generate a report:

```bash
make run-protocol-level-tests
```

The command:

1. Runs all protocol fault cases.
2. Appends the results to the CSV log.
3. Generates a Markdown report from the recorded run.

Default input:

```text
logs/protocol-fault-injection.csv
```

Default output directory:

```text
reports/protocol/
```

Generated report filename:

```text
Protocol-level-fault-report_TIMESTAMP.md
```

### Generate a report manually

```bash
python3 tools/reports/generate_protocol_report.py
```

The generator selects the latest run ID by default.

### Generate a report for a specific run

```bash
python3 tools/reports/generate_protocol_report.py --run-id RUN_ID
```

### Custom CSV and output directory

```bash
python3 tools/reports/generate_protocol_report.py \
    --csv logs/custom-run.csv \
    --output-dir reports/custom
```

---

## 7. Interpreting Results

- **PASS:** The actual mailbox return matches the expected return.
- **FAIL:** The actual mailbox return differs from the expected return, or an ioctl fails.
- **ERROR:** The report identifies an execution error.
- **INCOMPLETE:** Required fault cases are missing from the selected run.

Some unusual requests are intentionally expected to be accepted by the current QEMU implementation. Their acceptance is recorded as an observation rather than automatically considered a test failure.

---

## 8. Recovery and Validation Limitations

The protocol-level suite records mailbox responses, ioctl status and ABORT cleanup results.

It does **not** establish:

- Firmware image correctness
- Firmware activation
- Firmware bootability
- Device responsiveness after every fault
- Successful subsequent clean firmware transfer
- Persistence of firmware state across device or QEMU restarts

A protocol-level PASS only establishes that the observed response matched the test's expected response.

---

## 9. Relevant Source Files

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

tools/
└── reports/
    └── generate_protocol_report.py
```

---

## 10. Related Documentation

- [Core Project Configuration](CORE_PROJECT_CONFIGURATION.md)
- [Client Usage](CLIENT_USAGE.md)
- [Process Fault Testing](PROCESS_FAULT_TESTING.md)