# CXL Userspace Client Notes

## Overview

This document describes the C-based userspace client developed to investigate
CXL device firmware-management operations through the Linux CXL memory-device
interface.

The client uses `/dev/cxl/mem0` and the `CXL_MEM_SEND_COMMAND` ioctl to submit
commands to the Linux CXL driver.

## Source Files

| File | Purpose |
|---|---|
| `get_fw_info.c` | Retrieves and displays firmware slot information and revisions. |
| `query_commands.c` | Queries supported CXL commands. |
| `check_fw_layout.c` | Checks firmware-layout information. |
| `check_bg_status.c` | Checks background-operation status. |
| `transfer_fw.c` | Implements firmware-transfer operations. |

## Linux Device Interface

The client opens `/dev/cxl/mem0` and prepares a `struct cxl_send_command`
request.

It submits the request using:

`ioctl(fd, CXL_MEM_SEND_COMMAND, &cmd)`

The request contains the command identifier and buffer information. The
kernel driver processes the request and returns status information and any
response data.

## Firmware Information Utility

The `get_fw_info.c` utility:

1. Opens `/dev/cxl/mem0`.
2. Initializes a `struct cxl_send_command`.
3. Sets command ID `4` for Get Firmware Info.
4. Provides an 80-byte response buffer.
5. Submits the command using `CXL_MEM_SEND_COMMAND`.
6. Checks the ioctl result, device return code, and response size.
7. Displays firmware slot information, activation capability, and firmware
   revision strings.
8. Closes the device descriptor.

The response layout and bit-field interpretations should be validated against
the applicable CXL specification and device implementation.

## Firmware Transfer

The `transfer_fw.c` utility contains the firmware-transfer implementation.
Its command sequence, payload handling, transfer chunking, background
operation handling, and activation behavior should be documented and verified
against the source code and test results.

A successful ioctl call alone does not prove that a firmware image was
transferred and activated successfully.

## Testing and Validation

For each test, record:

- Test objective
- Command executed
- Test image and size
- Expected result
- Actual result
- Relevant output or logs
- Conclusion and limitations

End-to-end firmware-transfer and activation success must only be reported
after the behavior has been observed and validated.

## Build and Usage

Document the actual compiler commands, dependencies, runtime requirements,
and example invocations after verifying them in the development environment.

Device access may require appropriate permissions, and the device must
support the requested command.

## Limitations

- Firmware-transfer and activation validation must be supported by recorded
  test evidence.
- Command support may depend on the Linux kernel and QEMU device model.
- Compiled executables may depend on the architecture and build environment.
