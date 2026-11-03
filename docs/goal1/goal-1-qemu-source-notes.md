# Goal 1 — QEMU CXL Source Notes

## Source version

QEMU:
`11.1.50` (`v11.1.0-1664-gc1c18d1e64`)

Repository path:
`~/qemu-workspace/qemu`

## 1. Mailbox register handling

File: `hw/cxl/cxl-device-utils.c`

Function investigated:
`mailbox_reg_write()`

Observed behavior:

- Handles writes to the emulated mailbox registers.
- Detects a doorbell request.
- Extracts command information and input payload.
- Dispatches the mailbox command.
- Writes status and output information back to emulated registers.
- Clears the doorbell when the command completes.

## 2. Command dispatch

File: `hw/cxl/cxl-mailbox-utils.c`

Function:
`cxl_process_cci_message()`

The function finds the handler associated with the command set and
opcode, checks command conditions and payload size, invokes the handler,
and returns the resulting status.

## 3. Get FW Info

Function:
`cmd_firmware_update_get_info()`

Opcode: `0x0200`

Observed constants:

- `CXL_FW_SLOTS = 2`
- `CXL_FW_SIZE = 0x02000000` (32 MiB)

The handler reports supported slots, active and staged slot fields,
capabilities, and firmware revision strings for occupied slots.

## 4. Transfer FW

Function:
`cmd_firmware_update_transfer()`

Opcode: `0x0201`

Actions observed in source:

- FULL = `0x0`
- INIT = `0x1`
- CONTINUE = `0x2`
- END = `0x3`
- ABORT = `0x4`

The implementation checks transfer parameters and slot state and models
a background operation. The inspected code models firmware-slot state;
it does not establish that a real executable firmware image is installed.

## 5. Activate FW

Function:
`cmd_firmware_update_activate()`

Opcode: `0x0202`

Payload:

```c
struct {
    uint8_t action;
    uint8_t slot;
} QEMU_PACKED;
```

Observed behavior:

- Action 0 changes the active slot immediately.
- Action 1 records the requested slot as staged.
- Invalid slot states or invalid action values return errors.

Linux defines online activation as 0 and offline activation as 1.
QEMU handles action 1 as the reset-required/staged behavior.

Staging a slot alone does not prove that a reset occurred or that new
executable firmware was loaded.

## 6. Initial state

The inspected initialization code marks slot 1 active and occupied.

Firmware revision strings such as `BWFW VERSION 0` and
`BWFW VERSION 1` are simulated identifiers supplied by the emulator.

## 7. Important limitation

The investigation establishes the modeled command-handling path.
It does not demonstrate a complete end-to-end firmware update.

## Follow-up

- [ ] Record exact source line numbers for the final annotated trace.
- [ ] Inspect the complete staged-slot/reset path.
- [ ] Verify transfer and activation behavior with safe, read-only tests
      before planning any upload experiment.
