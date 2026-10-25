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
## Goal 3 Progress: Mailbox Response Handling and Background Operations

### 1. Userspace-to-kernel command flow

Our custom C client submits CXL mailbox commands using the `CXL_MEM_SEND_COMMAND` ioctl with a `struct cxl_send_command`.

The kernel processes the request through the following path:

1. `cxl_send_cmd()` in `drivers/cxl/core/mbox.c` copies the userspace structure into kernel memory.
2. `cxl_validate_cmd_from_user()` validates the command and constructs the internal mailbox command.
3. `handle_mailbox_cmd_from_user()` dispatches the command through `cxl_mbox->mbox_send()`.
4. The PCI mailbox driver in `drivers/cxl/pci.c` communicates with the device.
5. The kernel copies the response payload, response size, and device return code back to userspace.

### 2. PCI mailbox command execution

The function `__cxl_pci_mbox_send_cmd()` in `drivers/cxl/pci.c` performs the low-level mailbox operation.

Its main steps are:

1. Check whether the mailbox doorbell is busy.
2. Prepare the command register and copy any input payload to the device.
3. Ring the mailbox doorbell.
4. Wait for the doorbell to clear.
5. Read the device's mailbox return code.
6. Handle background operations when the device returns `CXL_MBOX_CMD_RC_BACKGROUND`.
7. Read any output payload and return the result.

### 3. Background-operation handling

When a command returns `CXL_MBOX_CMD_RC_BACKGROUND`, the PCI driver waits for the background operation to complete, except for the special Sanitization path, which is handled asynchronously.

The driver uses `cxl_mbox_background_complete()` to check completion. If the operation does not complete within the permitted polling period, the driver returns `-ETIMEDOUT`.

After completion, the driver reads the background-command status register and updates the command's return code.

A command being accepted or started does not by itself prove that the operation completed successfully.

### 4. Device return code versus ioctl result

In `handle_mailbox_cmd_from_user()`, the kernel:

- Calls `cxl_mbox->mbox_send()`.
- Copies the output payload to userspace
