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
- Updates the output size and device return code in the userspace command structure.
- Copies the updated command structure back to userspace.

The ioctl result and device return code represent different outcomes:

- **ioctl result:** Indicates whether the kernel successfully processed the ioctl request.
- **Device return code:** Indicates whether the CXL device reported success for the mailbox command.

Therefore, a successful ioctl does not necessarily mean that the mailbox command succeeded. The client must inspect both results.

### 5. Key lesson for firmware commands

Firmware transfer and activation commands may involve background operations. The client must verify the final device return code before treating an operation as successful.

Our custom client should report transport errors, device-reported errors, and successful command completion distinctly.



## Linux Firmware Transfer Implementation Analysis

### Source examined

File: `drivers/cxl/core/memdev.c`

Key functions:
- `cxl_fw_prepare()`
- `cxl_fw_write()`
- `cxl_mem_activate_fw()`
- `cxl_mem_abort_fw_xfer()`
- `cxl_fw_poll_complete()`

### 1. Firmware upload preparation

The `cxl_fw_prepare()` function rejects an empty firmware image and determines whether the image can be transferred in a single mailbox command.

The one-shot decision is made using:

```c
mds->fw.oneshot = struct_size(transfer, data, size) <
                  cxl_mbox->payload_size;
```

The transfer structure has a 128-byte header before the firmware data. The total transfer payload must be strictly smaller than the mailbox payload limit for the image to qualify as one-shot under this condition.

Preparation also calls `cxl_mem_get_fw_info()` to retrieve firmware information and checks whether a cancellation request has been received.

### 2. Transfer payload and chunk size

The `cxl_fw_write()` function checks that the firmware offset is aligned to `CXL_FW_TRANSFER_ALIGNMENT`, which is 128 bytes.

It calculates the firmware data size for the current command as:

```c
cur_size = min_t(size_t, size,
                 cxl_mbox->payload_size - sizeof(*transfer));
```

The transfer header occupies 128 bytes. The device's reported mailbox payload limit is 2048 bytes, giving a theoretical maximum firmware data chunk of:

```text
2048 - 128 = 1920 bytes
```

This is the maximum implied by the payload limit and header size; actual transfer behavior must also satisfy protocol alignment and image-size requirements.

### 3. Firmware offset encoding

The transfer offset is encoded in units of 128 bytes:

```c
transfer->offset = cpu_to_le32(offset / CXL_FW_TRANSFER_ALIGNMENT);
```

For example, an actual firmware offset of 256 bytes is represented as 2. The alignment check rejects offsets that are not multiples of 128.

### 4. Transfer actions

Linux chooses the transfer action based on whether the image fits in one command and the position of the current chunk.

| Condition | Action | Purpose |
|---|---|---|
| Image qualifies for one-shot transfer | `FULL` | Transfer the complete image in one command |
| Multi-command transfer, offset is zero | `INITIATE` | Begin the transfer |
| Multi-command transfer, data remains | `CONTINUE` | Send an intermediate chunk |
| Multi-command transfer, no data remains | `END` | Finish the transfer |

The `FULL` and `END` paths set the target slot in the transfer header. The intermediate actions shown in the Linux source do not explicitly set the slot field.

The target slot is calculated using:

```c
mds->fw.next_slot = (mds->fw.cur_slot % mds->fw.num_slots) + 1;
```

This selects the next slot using the current slot and the number of supported slots.

### 5. Command submission and progress

Linux constructs a mailbox command using `CXL_MBOX_OP_TRANSFER_FW` and submits it through `cxl_internal_send_cmd()`.

If the call returns a negative error, Linux reports a firmware-upload read/write error and proceeds to buffer cleanup.

After the call succeeds, Linux updates the number of bytes written:

```c
*written = cur_size;
```

For a one-shot transfer or the final chunk of a multi-command transfer, Linux then calls `cxl_mem_activate_fw()`.

### 6. Firmware activation

The activation function validates the target slot, constructs a command using `CXL_MBOX_OP_ACTIVATE_FW`, and sets:

```c
activate.action = CXL_FW_ACTIVATE_OFFLINE;
activate.slot = slot;
```

The inspected Linux implementation uses offline activation. If activation fails, the write function reports a firmware-upload hardware error.

### 7. Transfer cancellation

The `cxl_mem_abort_fw_xfer()` function sends the `TRANSFER_FW` mailbox opcode with the action set to `ABORT`.

The command uses a 1000 ms polling interval and a polling count of 30. Linux frees the allocated transfer buffer after submitting the abort command.

The cancellation path reports a cancellation error to the firmware-upload framework. If the abort command itself fails, Linux also logs that failure.

### 8. Background-operation completion

The `cxl_fw_poll_complete()` function explains that `cxl_internal_send_cmd()` handles background operations synchronously for this firmware-upload path. Consequently, completion errors are handled during the write call rather than by a separate wait in `poll_complete()`.

### 9. Implications for the custom userspace client

Our client should:

1. Validate the firmware image before starting a transfer.
2. Query firmware information and determine the target slot.
3. Respect the device's maximum mailbox payload size.
4. Construct the 128-byte transfer header correctly.
5. Encode offsets in units of 128 bytes.
6. Select the correct transfer action for each chunk.
7. Check both the ioctl result and the device's mailbox return code.
8. Handle transfer failures without incorrectly reporting success.
9. Activate the target slot only at the appropriate point in the transfer workflow.
10. Free allocated buffers on success and error paths.

The Linux kernel implementation is our reference for the transfer sequence. The custom client must also be validated against the actual QEMU device behavior before attempting a firmware update.

## Firmware Structure Layout Verification

### Purpose

A small C program, `check_fw_layout.c`, was created and executed in the guest VM (`firmware-lab`) to verify the memory layout of the firmware-transfer and firmware-activation structures before implementing these commands in our custom userspace client.

### Test Results

The program was compiled using:

```bash
gcc -Wall -Wextra -o check_fw_layout check_fw_layout.c
```

Execution command:

```bash
./check_fw_layout
```

Observed output:

```text
Firmware transfer header size: 128 bytes
Firmware data begins at offset: 128 bytes
Transfer offset field position: 4 bytes
Activation structure size: 2 bytes
```

### Findings

- The firmware-transfer header occupies 128 bytes.
- Firmware data begins immediately after the header, at byte offset 128.
- The transfer offset field begins at byte offset 4 within the header.
- The firmware-activation structure occupies 2 bytes.
- The structures use `__attribute__((packed))` to prevent compiler-inserted padding between fields.

### Significance

These results verify the basic C structure layout needed to construct firmware-transfer and firmware-activation command payloads in our custom client.

The transfer offset value must be encoded according to the CXL firmware-transfer protocol, which represents offsets in units of 128 bytes. The required byte order must also be respected when constructing the command.

### Limitations

This test verifies the local C structure layout only. It does not send mailbox commands to the CXL device, transfer a firmware image, or activate firmware.

### Status

**Completed:** Basic firmware-transfer and firmware-activation structure layout verification.

**Next:** Implement and review the firmware-transfer command construction and validation logic before attempting a firmware update.

## Transfer Planner Validation

### Dynamic Mailbox Payload Discovery

The custom `transfer_fw` utility reads the mailbox payload limit from:

`/sys/bus/cxl/devices/mem0/payload_max`

The reported payload limit in the current QEMU CXL environment is 2048 bytes. The firmware-transfer header occupies 128 bytes, leaving a maximum of 1920 bytes of firmware data per command.

### Boundary Tests

The following tests were run using dummy firmware images. All tests were dry runs; no firmware-transfer commands were sent to the device.

| Image size | Expected behavior | Observed result |
|---:|---|---|
| 0 bytes | Reject empty image | Rejected |
| 1919 bytes | Single `FULL` command | `FULL` planned |
| 1920 bytes | Multi-command path | `INITIATE` planned |
| 5120 bytes | Three-command transfer | `INITIATE`, `CONTINUE`, `END` planned |
| 5001 bytes | Generate a transfer plan | `INITIATE`, `CONTINUE`, `END` planned |

For the 5120-byte image, the planned data chunks were 1920, 1920, and 1280 bytes. Their byte offsets were 0, 1920, and 3840, corresponding to encoded offsets 0, 15, and 30 in 128-byte units.

The 5001-byte image produced a final chunk of 1161 bytes. The planner accepted this image, but the device's acceptance of a final chunk with a non-128-byte-aligned data length has not been verified.

### Validation Status and Limitations

- The planner obtains the payload limit dynamically rather than relying on a hardcoded value.
- Empty images are rejected.
- The boundary between the `FULL` path and the multi-command path is verified at 1919 and 1920 bytes.
- Transfer offsets are checked for 128-byte alignment.
- These tests validate planning logic only. They do not demonstrate that firmware transfer or activation succeeds on the device.
