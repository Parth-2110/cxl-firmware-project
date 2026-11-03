# Goal 1 — Linux CXL Source Notes

## Source version

Tag: `Ubuntu-hwe-7.0-7.0.0-31.31_24.04.1`

Repository path:
`~/kernel-workspace/linux-7.0`

## 1. Common mailbox layer

File: `drivers/cxl/core/mbox.c`

Function: `cxl_internal_send_cmd()`

Observed responsibilities:

- Validates input and output sizes against the mailbox payload limit.
- Calls the transport callback `cxl_mbox->mbox_send()`.
- Handles mailbox errors and return codes.
- Checks that the output meets the minimum expected size.

This layer provides a common interface to the mailbox transport.

## 2. PCI mailbox transport

File: `drivers/cxl/pci.c`

Functions investigated:

- `cxl_pci_setup_mailbox()`
- `cxl_pci_mbox_send()`
- `__cxl_pci_mbox_send_cmd()`

Observed responsibilities:

- Checks mailbox readiness and doorbell state.
- Builds the mailbox command register value.
- Writes the command payload to the mailbox registers.
- Sets the doorbell to request command execution.
- Waits for completion.
- Reads mailbox status and response data.

## 3. Firmware information

File: `drivers/cxl/core/memdev.c`

Function: `cxl_mem_get_fw_info()`

Opcode:
`CXL_MBOX_OP_GET_FW_INFO = 0x0200`

The function sends a Get FW Info command, then stores:

- `mds->fw.num_slots`
- `mds->fw.cur_slot`

The current slot is extracted from the returned `slot_info` field.

## 4. Firmware activation

File: `drivers/cxl/core/memdev.c`

Function: `cxl_mem_activate_fw()`

Opcode:
`CXL_MBOX_OP_ACTIVATE_FW = 0x0202`

Payload structure in `drivers/cxl/cxlmem.h`:

```c
struct cxl_mbox_activate_fw {
    u8 action;
    u8 slot;
} __packed;
```

Action values:

- `CXL_FW_ACTIVATE_ONLINE = 0x0`
- `CXL_FW_ACTIVATE_OFFLINE = 0x1`

The inspected Linux implementation selects offline activation and
sends the command through `cxl_internal_send_cmd()`.

## 5. Firmware upload

File: `drivers/cxl/core/memdev.c`

Functions investigated:

- `cxl_fw_prepare()`
- `cxl_fw_write()`
- `cxl_fw_poll_complete()`
- `cxl_fw_do_cancel()`
- `cxl_mem_abort_fw_xfer()`
- `devm_cxl_setup_fw_upload()`

Observed behavior:

- Checks the firmware image and queries firmware information.
- Splits images into mailbox-sized chunks.
- Uses FULL, INITIATE, CONTINUE, END, or ABORT transfer actions.
- Aligns transfer offsets to 128 bytes.
- Requests activation after a one-shot or final transfer.
- Supports cancellation and cleanup.

The firmware-upload callbacks are registered through
`firmware_upload_register()` when Get FW Info is enabled.

## 6. Current validation status

Source inspection is complete for the functions listed above.
No complete firmware image upload or successful activation has been
demonstrated by this investigation.

## Follow-up

- [ ] Record exact source line numbers for the final annotated trace.
- [ ] Inspect slot-info bit definitions and Linux next-slot selection.
- [ ] Compare command payloads against the corresponding QEMU handlers.
