# CXL Firmware Management and Fault-Injection Client

A Linux C userspace client for selected CXL memory-device commands and controlled `TRANSFER_FW` protocol experiments against a QEMU-emulated CXL Type 3 device.

## Requirements

- Linux with `linux/cxl_mem.h` and `CXL_MEM_SEND_COMMAND` support.
- GCC and GNU Make.
- For device commands: `/dev/cxl/mem0` and the CXL device sysfs attributes.
- A running QEMU guest with the CXL Type 3 device attached.

## Build

Run inside the Ubuntu guest OS:

```sh
make