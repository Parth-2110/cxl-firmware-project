# GOAL 4 — Fault Injection Results

## 1. Objective

The objective of GOAL 4 is to test how the CXL firmware update process responds to malformed requests, invalid transfer parameters, and interrupted operations.

The `fault_inject.c` utility sends deliberately malformed firmware-transfer requests through the Linux CXL userspace ioctl interface.

The tests help distinguish between:
- Userspace client-side validation.
- Linux ioctl or kernel-level errors.
- CXL mailbox and QEMU device-level rejection.

## 2. Test Environment

- CXL device: `/dev/cxl/mem0`
- Mailbox command: Firmware Update Transfer (`0x0201`)
- Raw command ID: `2`
- Firmware transfer header size: `128 bytes`
- QEMU firmware size limit: `32 MiB`
- Fault-injection utility: `fault_inject.c`
- QEMU CXL device: `cxl-type3`
- CXL memory backend: 256 MiB volatile RAM

## 3. Protocol Fault-Injection Results

### Test 1 — Invalid Action

Command:

```bash
sudo ./fault_inject --fault invalid-action --submit
```

Injected fault:
- Action byte set to `0x7f`, an unsupported action.

Observed result:

```text
ioctl returned successfully.
Mailbox return value: 2 (0x00000002)
Device rejected the mailbox command.
```

Interpretation:
- The ioctl completed successfully.
- QEMU rejected the mailbox request.
- Mailbox return code `0x02` represents `CXL_MBOX_INVALID_INPUT`.

Result: Rejected as expected.

### Test 2 — Invalid Slot

Command:

```bash
sudo ./fault_inject --fault invalid-slot --submit
```

Injected fault:
- Firmware slot set to `0`.

Observed result:

```text
ioctl returned successfully.
Mailbox return value: 11 (0x0000000B)
Device rejected the mailbox command.
```

Interpretation:
- The ioctl completed successfully.
- QEMU rejected the invalid firmware slot.
- Mailbox return code `0x0B` represents `CXL_MBOX_INVALID_SLOT`.

Result: Rejected as expected.

### Test 3 — Out-of-Range Offset

Command:

```bash
sudo ./fault_inject --fault out-of-range-offset --submit
```

Injected fault:
- Transfer offset set beyond the 32 MiB firmware limit.
- Offset units: `262145`.
- Byte offset: `33,554,560`.

Observed result:

```text
ioctl returned successfully.
Mailbox return value: 2 (0x00000002)
Device rejected the mailbox command.
```

Interpretation:
- The ioctl completed successfully.
- QEMU rejected the out-of-range transfer.
- Mailbox return code `0x02` represents `CXL_MBOX_INVALID_INPUT`.

Result: Rejected as expected.

### Test 4 — Truncated Payload

Command:

```bash
sudo ./fault_inject --fault truncated-payload --submit
```

Injected fault:
- Payload size reduced to 127 bytes.
- Required firmware transfer header size is 128 bytes.

Observed result:

```text
ioctl returned successfully.
Mailbox return value: 22 (0x00000016)
Device rejected the mailbox command.
```

Interpretation:
- The ioctl completed successfully.
- QEMU rejected the request because its payload was shorter than the required header.
- Mailbox return code `0x16` represents `CXL_MBOX_INVALID_PAYLOAD_LENGTH`.

Result: Rejected as expected.

### Test 5 — Bit-Flipped Firmware Data

Before the test, firmware information was queried:

```bash
sudo ./get_fw_info
```

Initial state:

```text
Current firmware slot: 1
Next firmware slot: 0
Slot 1: BWFW VERSION 0
Slot 2:
```

Command:

```bash
sudo ./fault_inject --fault bit-flip --submit
```

Injected fault:
- A bit in the firmware data payload was flipped.

Observed result:

```text
ioctl returned successfully.
Mailbox return value: 0 (0x00000000)
Mailbox command completed successfully.
```

After the test:

```bash
sudo ./get_fw_info
```

Observed state:

```text
Current firmware slot: 1
Next firmware slot: 0
Slot 1: BWFW VERSION 0
Slot 2: BWFW VERSION 1
```

Interpretation:
- QEMU accepted the mailbox request.
- Slot 2 became valid and reported firmware revision `BWFW VERSION 1`.
- The active firmware slot remained slot 1.
- The next firmware slot remained 0.
- The observed result does not prove that QEMU detected the flipped bit or performed firmware integrity verification.
- Source inspection did not establish that the incoming firmware image bytes were actually stored.

Result: Request accepted. Firmware integrity detection remains unverified.

## 4. Background Operation Handling

The fault-injection utility was extended with background-operation polling.

Implementation details:
- Background Status opcode: `0x0002`.
- Firmware Transfer opcode: `0x0201`.
- Polling interval: 250 milliseconds.
- Timeout: 60 seconds.
- The utility checks the background operation opcode, progress, and completion return code.

The implementation compiled successfully using:

```bash
gcc -Wall -Wextra -O2 fault_inject.c -o fault_inject
```

Dry-run tests for all five fault cases also passed.

Important limitation:

A real `BG_STARTED` response has not yet been observed in the experiments. Therefore, the polling implementation is compiled but not yet experimentally verified through a real background operation.

## 5. Dry-Run Verification

All five fault cases were tested without submitting commands to the device:

```bash
./fault_inject --fault invalid-action
./fault_inject --fault invalid-slot
./fault_inject --fault out-of-range-offset
./fault_inject --fault bit-flip
./fault_inject --fault truncated-payload
```

All five requests were constructed successfully.

Each output confirmed:

```text
Submission: DISABLED
Dry run: no device command sent.
```

## 6. Summary

| Fault case | Observed result |
|---|---|
| Invalid action | Rejected — `0x02` |
| Invalid slot | Rejected — `0x0B` |
| Out-of-range offset | Rejected — `0x02` |
| Truncated payload | Rejected — `0x16` |
| Bit-flipped data | Accepted — `0x00` |

The five protocol fault cases were successfully constructed and submitted to the QEMU CXL device.

Four malformed requests were rejected with the expected mailbox errors. The bit-flipped data request was accepted, but firmware integrity checking was not demonstrated.

## 7. Pending Work

- Test invalid transfer sequences, including CONTINUE or END without INITIATE.
- Test duplicate, skipped, and out-of-order transfer offsets.
- Exercise and verify actual background-operation polling.
- Investigate firmware integrity validation and slot behavior.
- Plan controlled process-interruption tests.
- Record recovery behavior and device state after each additional test.

**Safety note:** The current QEMU CXL memory backend uses volatile RAM. Restarting QEMU may reset simulated CXL firmware state. Process-interruption experiments must therefore be planned separately and must not be treated as proof of persistent firmware storage.
