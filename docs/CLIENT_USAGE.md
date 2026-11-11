
# CXL Firmware Client — Command Reference

All commands in this document are executed inside the **Ubuntu guest OS**, from `~/fw-management`.

## 1. Build

```bash
make
```

Clean and rebuild:

```bash
make clean
make
```

## 2. Query Supported CXL Commands

```bash
make query
```

Uses the CXL command-query ioctl to retrieve the commands supported by the device.

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

## 4. Abort Firmware Transfer

```bash
make abort-fw-transfer
```

Requires the exact confirmation:

```text
ABORT
```

This sends the firmware-transfer ABORT action.

## 5. Transfer Planning

Planning does not open the CXL device or issue an ioctl.

### Size-only planning

No firmware file is required:

```bash
make transfer-dry-run SIZE=8192 SLOT=2 CAPACITY=2048
```

Parameters:

| Parameter | Meaning |
|---|---|
| `SIZE` | Firmware image size in bytes |
| `SLOT` | Target firmware slot |
| `CAPACITY` | Requested mailbox payload capacity |

### File-backed planning

Generate the default synthetic image:

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

## 6. Execute Firmware Transfer

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

**Warning:** This writes to the selected inactive firmware slot. It does not activate the firmware. Use a disposable QEMU device and synthetic test image.

## 7. Generate Synthetic Firmware

Generate the default 8 KiB image:

```bash
make test-firmware
```

Generate a custom image:

```bash
make generate-firmware OUTPUT=firmware/synthetic-16k.bin IMAGE_SIZE=16384
```

The generator creates deterministic synthetic bytes. These are not vendor-signed production firmware images.

## 8. Unit Tests

Run header tests:

```bash
make test-header
```

Run transfer-request tests:

```bash
make test-transfer-request
```

Run both:

```bash
make test
```

## 9. Protocol Fault Injection

See [Protocol Fault Testing](PROTOCOL_FAULT_TESTING.md).

## 10. Process Fault Injection

See [Process Fault Testing](PROCESS_FAULT_TESTING.md).