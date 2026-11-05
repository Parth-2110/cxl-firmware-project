# CXL Firmware Management and Fault-Injection Client

A Linux C userspace client for selected CXL memory-device commands and controlled `TRANSFER_FW` protocol experiments against a QEMU-emulated CXL Type 3 device.

## Requirements

- Linux with `linux/cxl_mem.h` and `CXL_MEM_SEND_COMMAND` support.
- GCC and GNU Make.
- For device commands: `/dev/cxl/mem0` and the CXL device sysfs attributes.

## Build and clean

```sh
make
make clean
```

## Main commands

```sh
make test                 # local byte-layout/request tests; no device access
make transfer-dry-run     # size-only planner
make transfer-file-dry-run
make fault-dry-run
```

Device operations require the QEMU guest OS and suitable privileges:

```sh
make query
make get-fw-info
make fault-inject          # requires typing INJECT
```

Actual firmware writes must use a disposable QEMU device and an explicitly selected inactive slot. The `transfer-fw --execute` command requires the confirmation word `TRANSFER`. This project does not activate firmware. See `docs/` for commands, testing, fault semantics and regression scope.

## Documentation

- [Client usage](docs/CLIENT_USAGE.md)
- [Local testing and test-image generation](docs/TESTING.md)
- [Protocol and process fault injection](docs/FAULT_INJECTION.md)
- [Regression and validation plan](docs/REGRESSION.md)

## Current implementation boundary

Normal file transfers and protocol experiments share `core/transfer_engine.c` for the transfer sequencing/submit layer. Normal transfers construct their own valid chunk sequence; protocol experiments deliberately prepare or mutate individual request fields and can issue isolated requests or custom request sequences. Process-level fault experiments and Goal 5 device recovery/clean-update automation are reserved but not yet implemented.