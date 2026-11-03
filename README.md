# CXL Firmware Management Client

A userspace C client for issuing selected CXL memory-device commands through the Linux CXL ioctl interface. It is intended for development and controlled testing against the QEMU-emulated CXL Type 3 device used by this project.

## Scope

The client preserves the behavior of the supplied original single-file implementation. The source has been separated into modules; this organization is not intended to change the command protocol or device behavior.

Implemented commands:

- `query` — list commands exposed by the Linux CXL driver.
- `get-fw-info` — retrieve and display firmware slot information.
- `transfer-fw --dry-run` — plan a transfer without opening the CXL device or issuing an ioctl. Supports either `--file PATH` or `--size BYTES`.
- `transfer-fw --execute` — send a firmware image to a selected inactive slot after the program's checks and explicit confirmation.
- `abort-fw-transfer` — send the transfer ABORT action after explicit confirmation.
- `test-header` and `test-transfer-request` — local byte-layout/request tests; these do not open the device or issue ioctls.

The client does **not** activate a firmware slot.

## Requirements

- Linux with CXL userspace headers providing `linux/cxl_mem.h`.
- GCC and GNU Make.
- A CXL memory device exposed at `/dev/cxl/mem0` (for device commands).
- For execution mode, the sysfs `payload_max` value at `/sys/bus/cxl/devices/mem0/payload_max`.


## Build

From the project directory:

```sh
make
```

The executable is created at `build/cxl-fw-client`.

Clean generated build files:

```sh
make clean
```

## Usage

The Makefile provides convenient targets for common operations.

### Device Information

These commands communicate with the QEMU-emulated CXL device and automatically request `sudo`:

```sh
make query
make get-fw-info
```

- `query`: Retrieves the device's supported command table.
- `get-fw-info`: Retrieves firmware slot information.

### Local Tests

These tests do not open `/dev/cxl/mem0` or issue device ioctls:

```sh
make test-header
make test-transfer-request
```

Run both tests together:

```sh
make test
```

### Transfer Planning (Dry-Run)

Plan a transfer using only an image size, without requiring a firmware file:

```sh
make transfer-dry-run
```

Default parameters:

- Image size: 8192 bytes
- Target slot: 2
- Payload capacity: 2048 bytes

Override these parameters when needed:

```sh
make transfer-dry-run SIZE=28800 SLOT=2 CAPACITY=2048
```

Alternatively, use the client directly:

```sh
./build/cxl-fw-client transfer-fw \
    --size 8192 \
    --slot 2 \
    --payload-capacity 2048 \
    --dry-run
```

Dry-run planning uses the client-side image-size limit and the supplied payload-capacity value. It does not open `/dev/cxl/mem0`, read device capabilities, or issue ioctls. The `--size` option is for dry-run only.

### Dry-Run Using a Firmware File

```sh
./build/cxl-fw-client transfer-fw \
    --file firmware/your-image.bin \
    --slot 2 \
    --payload-capacity 2048 \
    --dry-run
```

### Execute a Firmware Transfer

```sh
sudo ./build/cxl-fw-client transfer-fw \
    --file firmware/your-image.bin \
    --slot 2 \
    --payload-capacity 2048 \
    --execute
```

Execution mode reads the device's `payload_max`, checks the target slot against firmware information, refuses the active slot, and asks for the exact confirmation phrase before sending transfer commands.

Use only a test image and a disposable QEMU guest/device. A failed transfer can leave the target slot incomplete.

This command transfers firmware; it does not activate it.

### Abort a Transfer

```sh
sudo ./build/cxl-fw-client abort-fw-transfer
```

This sends the ABORT action after requiring the exact uppercase confirmation phrase. It does not send firmware data or activate a slot.

If the `abort-fw-transfer` Make target is added to the Makefile, it can also be invoked using:

```sh
make abort-fw-transfer
```

### Clean Up

```sh
make clean
```
## Directory layout

```text
fw-management/
├── include/cxl_fw.h
├── src/
│   ├── main.c
│   ├── commands/
│   │   ├── abort_fw.c
│   │   ├── get_fw_info.c
│   │   ├── query.c
│   │   └── transfer_fw.c
│   └── core/
│       ├── mailbox.c
│       ├── transfer_request.c
│       └── utils.c
├── tests/
│   ├── test_header.c
│   └── test_transfer_request.c
├── firmware/
├── logs/
├── docs/
├── scripts/
├── Makefile
├── README.md
└── .gitignore
```

## Firmware images and logs

Place your test firmware image in `firmware/`. Firmware binaries and generated logs are ignored by Git by default. Do not commit proprietary or otherwise restricted firmware images.

## Safety

- `query`, `get-fw-info`, local tests, and dry-run have different effects; the local tests and dry-run do not communicate with the device.
- `transfer-fw --execute` and `abort-fw-transfer` issue device commands and require explicit confirmation.
- Keep testing confined to the intended QEMU-emulated CXL device.
- Review the command arguments and target slot before confirming.