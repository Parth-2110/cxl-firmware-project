## Available regression command

```sh
make regression
```

The current `regression` target runs:

1. Transfer header tests.
2. Transfer request tests.
3. Protocol fault-injection dry-run.

It does not send live fault-injection requests to the CXL device.

## Related Make commands

| Purpose | Command |
|---|---|
| Header tests | `make test-header` |
| Transfer request tests | `make test-transfer-request` |
| All local tests | `make test` |
| Protocol fault dry-run | `make fault-dry-run` |
| Live protocol fault injection | `make fault-inject` |
| Firmware information check | `make get-fw-info` |
| Transfer planning | `make transfer-dry-run` |
| File-backed transfer planning | `make transfer-file-dry-run` |
| Actual firmware transfer | `make transfer-fw FILE=... SLOT=... CAPACITY=...` |

Live device testing is kept separate because it sends commands to the CXL device.

## Goal 5 acceptance criteria (not yet implemented)

The full regression runner should orchestrate each protocol/process fault and then validate:

1. `GET_FW_INFO` remains responsive and reports a consistent slot state.
2. The device remains responsive after the injected fault; record bounded command response and ioctl failures/timeouts.
3. A subsequent clean firmware transfer to a known inactive test slot succeeds.
4. The known-good active slot remains unchanged. Do not label this "bootability" unless the QEMU configuration actually boots from that firmware slot or offers an equivalent observable boot validation.
5. A per-run report contains run ID, timestamp, fault case, expected/actual mailbox results, post-fault state, recovery result and overall verdict.

## Proposed live regression runner

A future command such as `make regression-live` should orchestrate:

1. Setup and device checks.
2. One fault at a time.
3. Post-fault `GET_FW_INFO`.
4. A clean transfer to a known inactive test slot.
5. Report generation.

It must require explicit opt-in/confirmation and a disposable QEMU guest.

**`make regression-live` is currently not implemented.** Its Make target intentionally exits with an error rather than implying that live regression is available.

A successful `GET_FW_INFO` alone is not proof of bootability. The current QEMU CXL Type 3 volatile-memory configuration does not establish that the firmware slot is bootable. Validate only what is observable and document any bootability limitation.