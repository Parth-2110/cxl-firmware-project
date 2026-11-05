# Fault injection

## Protocol-level suite

Run inside the Ubuntu guest OS with the QEMU CXL device attached:

```sh
./build/cxl-fw-client fault-inject --all --dry-run
sudo ./build/cxl-fw-client fault-inject --all
```

The live suite reads firmware information and `payload_max`, selects an inactive slot, requires the exact confirmation `INJECT`, performs an initial ABORT cleanup and executes these `TRANSFER_FW` mailbox tests:

| Case | Mutation / sequence | Expected mailbox result |
|---|---|---:|
| `invalid_action` | Action byte `0xFF` | `0x02` |
| `invalid_slot` | FULL request to currently active slot | `0x0B` |
| `out_of_sequence` | CONTINUE without INITIATE | `0x02` |
| `duplicate_offset` | INITIATE followed by CONTINUE at same offset and same length | `0x00` |
| `skipped_offset` | INITIATE followed by CONTINUE with an offset gap | `0x00` (observed QEMU behavior) |
| `out_of_range_offset` | Offset plus data crosses 32 MiB limit | `0x02` |
| `bit_flipped_data` | Flip one bit in first data byte | `0x00` (QEMU does not validate contents) |
| `truncated_payload` | 128-byte header with a 64-byte data section | `0x00` (observed QEMU behavior) |
| `corrupted_reserved_fields` | Nonzero reserved bytes 2 and 8 | `0x00` (observed QEMU behavior) |

Each case performs ABORT cleanup afterward. PASS means the mailbox return matched the expected value; it does not mean QEMU rejected every unusual input or that the resulting image is valid/activatable. The suite never activates firmware.

## Logging

CSV output defaults to `logs/protocol-fault-injection.csv` and is append-only. Each run has a distinct `run_id` and timestamp; rows also record test, step, expected/actual return, ioctl status, verdict and note. Override the path with `--log PATH`.

## Architecture

- `core/transfer_engine.c` owns standard chunk sequencing and the common submit stage.
- The normal `transfer-fw` command supplies the image, slot and capacity to this engine.
- `fault_injection/protocol/fault_cases.c` is only the suite orchestrator. Each fault has its own source file and uses shared helpers in `fault_common.c`; prepared requests pass through `fw_transfer_run_prepared()` in the shared engine, with the same mailbox-submit callback pattern as normal transfers.
- `fault_injection/process/` is reserved for process-level fault injection. No process-level experiment is implemented yet.

Some protocol tests must deliberately send isolated malformed requests or custom sequences. They must not be forced through normal image validation/chunk selection, since that would prevent the intended invalid request from reaching QEMU.