# Testing

## Local tests (no device access)

```sh
make test-header
make test-transfer-request
make test
```

These test transfer header layout, byte order, reserved bytes, request length, data placement and basic input rejection.

## Planner checks

Size-only planning, without a firmware file:

```sh
make transfer-dry-run SIZE=8192 SLOT=2 CAPACITY=2048
```

File-backed planning:

```sh
make test-firmware
make transfer-file-dry-run SLOT=2 CAPACITY=2048
```

Generate another synthetic image directly:

```sh
make build/generate-test-firmware
./build/generate-test-firmware firmware/synthetic-16k.bin 16384
```

The generator emits deterministic synthetic bytes. It is not a vendor-signed or valid production firmware image.

## Full local preflight

```sh
make regression
```

This currently runs the local C tests and protocol fault dry-run. It does **not** run live device faults, device health/bootability checks or clean-update recovery. Those require Goal 5 implementation and explicit device-side validation.