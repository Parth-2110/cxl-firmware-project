## Build

```sh
make
```

## Query supported commands

```sh
make query
```

## Read firmware slot information

```sh
make get-fw-info
```

## Plan a transfer without a file

```sh
make transfer-dry-run SIZE=8192 SLOT=2 CAPACITY=2048
```

Parameters:

- `SIZE`: firmware image size in bytes.
- `SLOT`: target firmware slot.
- `CAPACITY`: requested mailbox payload capacity.

This operation does not open the CXL device or issue an ioctl.

## Generate the default test firmware

```sh
make test-firmware
```

Generates `firmware/synthetic-test-8k.bin`.

## Plan a transfer using a firmware image

```sh
make transfer-file-dry-run
```

For a custom existing image:

```sh
make transfer-file-dry-run FILE=firmware/custom.bin SLOT=2 CAPACITY=2048
```

Parameters:

- `FILE`: firmware image path.
- `SLOT`: target firmware slot.
- `CAPACITY`: requested mailbox payload capacity.

This operation does not open the CXL device or issue an ioctl.

## Execute a transfer

```sh
make transfer-fw FILE=firmware/synthetic-test-8k.bin SLOT=2 CAPACITY=2048
```

Execution checks the device-reported payload capacity and firmware slot information, refuses the active slot, and requires the exact confirmation `TRANSFER`.

It writes only to the selected inactive slot. It does not activate firmware.

Use a disposable QEMU device and a suitable test image.

## Abort an in-progress transfer

```sh
make abort-fw-transfer
```

Requires the exact confirmation `ABORT`.

## Generate a custom synthetic firmware image

```sh
make generate-firmware OUTPUT=firmware/synthetic-16k.bin IMAGE_SIZE=16384
```

The generated image contains deterministic synthetic bytes. It is not a vendor-signed or valid production firmware image.