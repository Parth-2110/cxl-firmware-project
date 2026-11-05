## Protocol-level suite

### Dry run

```sh
make fault-dry-run
```

This displays all protocol fault cases without opening the device or issuing ioctls.

### Live execution

```sh
make fault-inject
```

The live suite reads firmware information and `payload_max`, selects an inactive slot, requires the exact confirmation `INJECT`, performs an initial ABORT cleanup and executes the `TRANSFER_FW` mailbox tests.

The suite includes:

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

Each case performs ABORT cleanup afterward.

PASS means the mailbox return matched the expected value. It does not mean QEMU rejected every unusual input or that the resulting image is valid or activatable.

The suite never activates firmware.

## Logging

CSV output defaults to `logs/protocol-fault-injection.csv` and is append-only.

Each run has a distinct `run_id` and timestamp. Rows also record test, step, expected/actual return, ioctl status, verdict and note.

The log path can be overridden when invoking the client directly with `--log PATH`.

## Architecture

- `core/transfer_engine.c` owns standard chunk sequencing and the common submit stage.
- The normal `transfer-fw` command supplies the image, slot and capacity to this engine.
- `fault_injection/protocol/fault_cases.c` is the suite orchestrator.
- Each fault has its own source file and uses shared helpers in `fault_common.c`.
- Prepared requests pass through `fw_transfer_run_prepared()` in the shared engine, with the same mailbox-submit callback pattern as normal transfers.
- `fault_injection/process/` is reserved for process-level fault injection. No process-level experiment is implemented yet.

Some protocol tests deliberately send isolated malformed requests or custom sequences. They must not be forced through normal image validation or chunk selection, since that would prevent the intended invalid request from reaching QEMU.