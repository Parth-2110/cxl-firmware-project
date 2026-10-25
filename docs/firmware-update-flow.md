# CXL Firmware Update — End-to-End Flow

## Conceptual path

1. Linux firmware-upload framework receives an image.
2. `drivers/cxl/core/memdev.c` prepares and splits the image.
3. Linux sends mailbox commands through `cxl_internal_send_cmd()`.
4. `drivers/cxl/pci.c` transports commands through PCI mailbox registers.
5. QEMU emulates the mailbox register and doorbell behavior.
6. `cxl_process_cci_message()` selects the command handler.
7. The relevant QEMU firmware handler returns status and output.
8. Linux processes the response and, after a final transfer, requests
   firmware activation.

## Commands inspected

| Command | Opcode | Purpose |
|---|---:|---|
| Get FW Info | `0x0200` | Retrieve firmware slot information |
| Transfer FW | `0x0201` | Transfer firmware image data |
| Activate FW | `0x0202` | Request firmware-slot activation |

## Source files

Linux:

- `drivers/cxl/core/memdev.c`
- `drivers/cxl/core/mbox.c`
- `drivers/cxl/pci.c`
- `drivers/cxl/cxlmem.h`

QEMU:

- `hw/cxl/cxl-device-utils.c`
- `hw/cxl/cxl-mailbox-utils.c`

## Evidence versus assumptions

Verified by environment checks:

- The guest enumerates a CXL Type 3 PCI device.
- `/dev/cxl/mem0` exists.
- The Linux CXL memory driver is bound.

Verified by source inspection:

- Linux mailbox and firmware-upload functions.
- QEMU mailbox dispatch and firmware command handlers.
- Corresponding firmware command opcodes and activation payload layout.

Not yet demonstrated:

- Successful transfer of a complete firmware image.
- Successful activation after reset.
- Replacement of executable firmware in QEMU.

## Next investigation

Confirm the firmware slot-info bit definitions and Linux target-slot
selection, then record a source-linked trace with exact line numbers.
## Goal 3 — Custom userspace client: Get FW Info

### Implementation

Created `get_fw_info.c` in the guest VM's `~/cxl-firmware-client/` directory.

The program:

1. Opens `/dev/cxl/mem0`.
2. Constructs a `struct cxl_send_command` request.
3. Sets command ID to `4` (`GET_FW_INFO`).
4. Allocates an 80-byte output response buffer.
5. Sends the request using `ioctl()` with `CXL_MEM_SEND_COMMAND`.
6. Checks the ioctl result, device return code, and response size.
7. Decodes and prints the firmware information.

The program is compiled with:

```gcc -Wall -Wextra -o get_fw_info get_fw_info.c
```
It is executed with:
```
sudo ./get_fw_info
```
### Test results

The command completed successfully on the QEMU CXL device.

| Field | Observed result |
|---|---|
| Command | `GET_FW_INFO` |
| Command ID | `4` |
| ioctl result | Success |
| Device return code | `0` |
| Response size | `80` bytes |
| Firmware slots reported | `2` |
| Current firmware slot | `1` |
| Next firmware slot field | `0` |
| Live activation supported | Yes |
| Slot 1 revision | `BWFW VERSION 0` |
| Slot 2 revision | No printable revision reported |

The response structure was identified in the Linux source at
`drivers/cxl/cxlmem.h`, in `struct cxl_mbox_get_fw_info`.

### Significance

This demonstrates that our custom userspace program can submit a named
CXL mailbox command through the Linux `CXL_MEM_SEND_COMMAND` ioctl and
interpret the returned firmware information without using `cxl-cli` or
`libcxlmi`.

### Limitations and next steps

- Only the read-only `GET_FW_INFO` command has been tested by our client.
- Firmware transfer (`0x0201`) has not been demonstrated.
- Firmware activation (`0x0202`) has not been demonstrated.
- QEMU's firmware-transfer and activation behavior still needs to be
  investigated before attempting an update.
- The client must continue to distinguish ioctl failures from nonzero
  device return codes.
