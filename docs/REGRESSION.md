# Regression, validation and recovery

## Available now

```sh
make regression
```

Current `regression` target runs local header/request tests and the protocol suite dry-run only. Live device testing is kept separate because it sends commands to the CXL device.

## Goal 5 acceptance criteria (not yet implemented)

The full regression runner should orchestrate each protocol/process fault and then validate:

1. `GET_FW_INFO` remains responsive and reports a consistent slot state.
2. The device remains responsive after the injected fault; record bounded command response and ioctl failures/timeouts.
3. A subsequent clean firmware transfer to a known inactive test slot succeeds.
4. The known-good active slot remains unchanged. Do not label this “bootability” unless the QEMU configuration actually boots from that firmware slot or offers an equivalent observable boot validation.
5. A per-run report contains run ID, timestamp, fault case, expected/actual mailbox results, post-fault state, recovery result and overall verdict.

## Proposed runner

Use one command (for example `make regression-live`) to orchestrate setup checks, one fault at a time, post-fault `GET_FW_INFO`, clean transfer and report generation. Keep a non-destructive `make regression` target for local tests. Live tests must require an explicit opt-in/confirmation and a disposable QEMU guest.

A GET_FW_INFO success alone is not proof of bootability. The current QEMU CXL Type 3 volatile-memory configuration does not establish that the firmware slot is bootable. Validate only what is observable, and document any bootability limitation.