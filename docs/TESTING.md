## Build

```sh
make
```

## Local tests (no device access)

Run individual test suites:

```sh
make test-header
make test-transfer-request
```

Run both suites:

```sh
make test
```

These test transfer header layout, byte order, reserved bytes, request length, data placement and basic input rejection.

## Planner checks

### Size-only planning

No firmware file is required:

```sh
make transfer-dry-run SIZE=8192 SLOT=2 CAPACITY=2048
```

Parameters:

- `SIZE`: image size in bytes.
- `SLOT`: target slot.
- `CAPACITY`: requested payload capacity.

### File-backed planning

Generate the default 8 KiB synthetic image:

```sh
make test-firmware
```

Run the file-backed planner:

```sh
make transfer-file-dry-run SLOT=2 CAPACITY=2048
```

Use a custom existing image:

```sh
make transfer-file-dry-run FILE=firmware/custom.bin SLOT=2 CAPACITY=2048
```

## Synthetic firmware generation

Generate a custom image:

```sh
make generate-firmware OUTPUT=firmware/synthetic-16k.bin IMAGE_SIZE=16384
```

The generator emits deterministic synthetic bytes. It is not a vendor-signed or valid production firmware image.

## Actual transfer

The following command performs a real firmware transfer to the selected inactive slot:

```sh
make transfer-fw FILE=firmware/synthetic-test-8k.bin SLOT=2 CAPACITY=2048
```

The client requires the exact confirmation `TRANSFER`.

The command does not activate firmware. Do not activate synthetic test firmware.

## Full local preflight

```sh
make regression
```

This currently runs the local C tests and protocol fault dry-run.

It does not run live device faults, device health/bootability checks or clean-update recovery. Those require Goal 5 implementation and explicit device-side validation.