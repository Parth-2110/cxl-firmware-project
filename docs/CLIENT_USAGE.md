# Client usage

All device commands below run **inside the Ubuntu guest OS** hosting the QEMU CXL Type 3 device.

## Build

```sh
make
```

## Query supported commands

```sh
sudo ./build/cxl-fw-client query
```

## Read firmware slot information

```sh
sudo ./build/cxl-fw-client get-fw-info
```

## Plan a transfer without a file

```sh
./build/cxl-fw-client transfer-fw --size 8192 --slot 2 --payload-capacity 2048 --dry-run
```

## Plan a transfer using an image

```sh
./build/cxl-fw-client transfer-fw --file firmware/synthetic-test-8k.bin --slot 2 --payload-capacity 2048 --dry-run
```

## Execute a transfer

```sh
sudo ./build/cxl-fw-client transfer-fw --file firmware/synthetic-test-8k.bin --slot 2 --payload-capacity 2048 --execute
```

Execution checks the device-reported payload capacity and firmware slot information, refuses the active slot, then requires `TRANSFER`. It writes only the selected inactive slot. It does not activate firmware.

## Abort an in-progress transfer

```sh
sudo ./build/cxl-fw-client abort-fw-transfer
```

Requires the exact confirmation `ABORT`.