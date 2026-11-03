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
