# CXL Firmware Client — Command Reference

All commands in this document are executed inside the **Ubuntu guest OS**, from the `~/fw-management` directory.

## 1. Build the Client

Build the client and synthetic firmware generator:

```bash
make
```

Clean and rebuild:

```bash
make clean
make
```

The build produces:

- `build/cxl-fw-client`
- `build/generate-test-firmware`

---

## 2. Query Supported CXL Commands

```bash
make query
```

Queries the device through the CXL command-query ioctl and displays supported commands.

---

## 3. Get Firmware Information

```bash
make get-fw-info
```

Displays firmware slot information, including:

- Number of firmware slots
- Current firmware slot
- Next firmware slot
- Live activation support
- Slot revision information, where available

---

## 4. Abort Firmware Transfer

```bash
make abort-fw-transfer
```

Requires the exact confirmation:

```text
ABORT
```

This sends the firmware-transfer ABORT action.

---

## 5. Transfer Planning

Transfer planning does not execute firmware-transfer mailbox commands.

### Size-only planning

```bash
make transfer-dry-run SIZE=8192 SLOT=2 CAPACITY=2048
```

| Parameter | Meaning |
|---|---|
| `SIZE` | Firmware image size in bytes |
| `SLOT` | Target firmware slot |
| `CAPACITY` | Requested payload capacity |

### File-backed planning

Generate the default synthetic firmware:

```bash
make test-firmware
```

Plan its transfer:

```bash
make transfer-file-dry-run
```

Use a custom image:

```bash
make transfer-file-dry-run FILE=firmware/custom.bin SLOT=2 CAPACITY=2048
```

The custom firmware file must already exist.

---

## 6. Execute Firmware Transfer

Example:

```bash
make transfer-fw FILE=firmware/synthetic-test-8k.bin SLOT=2 CAPACITY=2048
```

The client:

1. Validates the firmware image.
2. Reads the device-reported payload capacity.
3. Retrieves firmware slot information.
4. Rejects an invalid or active target slot.
5. Requests explicit confirmation.
6. Sends the firmware-transfer requests.

Required confirmation:

```text
TRANSFER
```

**Warning:** This writes firmware data to the selected inactive slot. It does not activate the firmware. Perform live transfers only on the disposable QEMU test device using synthetic test images.

---

## 7. Generate Synthetic Firmware

### Default 8 KiB image

```bash
make test-firmware
```

Output:

```text
firmware/synthetic-test-8k.bin
```

### Custom image

```bash
make generate-firmware OUTPUT=firmware/synthetic-16k.bin IMAGE_SIZE=16384
```

The generator produces deterministic synthetic bytes for testing. These are not vendor-signed production firmware images.

---

## 8. Unit Tests

### Header tests

```bash
make test-header
```

### Transfer-request tests

```bash
make test-transfer-request
```

### All unit tests

```bash
make test
```

These tests run locally without issuing device mailbox commands.

---

## 9. Protocol Fault Injection

Refer to [Protocol Fault Testing](PROTOCOL_FAULT_TESTING.md) for fault scenarios, execution commands, CSV logging and report generation.

---

## 10. Process Fault Injection

Refer to [Process Fault Testing](PROCESS_FAULT_TESTING.md) for host-side controller setup, checkpoint execution, QEMU termination/restart and logging.