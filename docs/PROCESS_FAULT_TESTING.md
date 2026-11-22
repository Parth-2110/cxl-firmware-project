# Process-Level Fault Injection

Process-level fault injection is controlled from the **Arch Linux host**. The firmware client executes inside the **Ubuntu guest**.

## 1. Overview

Process-level fault injection evaluates QEMU's behaviour when its process is forcibly terminated during a firmware-transfer sequence.

The controller:

1. Starts QEMU using the disposable overlay disk.
2. Waits for Ubuntu to boot.
3. Establishes SSH connectivity.
4. Verifies `/dev/cxl/mem0`.
5. Starts the firmware-transfer client inside Ubuntu over SSH.
6. Waits for a predefined firmware-transfer checkpoint.
7. Terminates QEMU using `SIGKILL`.
8. Restarts QEMU.
9. Verifies SSH and `/dev/cxl/mem0` recovery.

The test validates QEMU process restart and guest/device availability. It does not establish firmware-transfer recovery or firmware-state persistence.

---

## 2. Fault Checkpoints

| Checkpoint | Injection point |
|---|---|
| PF1 | Before INITIATE |
| PF2 | After INITIATE |
| PF3-1 | After CONTINUE 1 |
| PF3-2 | After CONTINUE 2 |
| PF4 | After final CONTINUE, before END |

Each checkpoint represents a different point in the firmware-transfer process at which QEMU is terminated.

---

## 3. Architecture

| Component | Environment | Responsibility |
|---|---|---|
| QEMU | Arch host | Runs the CXL Type 3 virtual device |
| Process-fault controller | Arch host | Starts, terminates and restarts QEMU |
| Checkpoint listener | Arch host, TCP port 39091 | Receives checkpoint notifications |
| Firmware client | Ubuntu guest | Executes firmware-transfer requests |
| SSH | Arch host ↔ Ubuntu guest | Remote client execution and verification |
| Controller logs | Arch host | Records test events |
| QEMU logs | Arch host | Records QEMU output |

The guest communicates with the host checkpoint listener through `10.0.2.2`.

SSH forwarding uses `127.0.0.1:2222`.

---

## 4. Requirements

### Arch Linux host

- QEMU built with CXL support.
- KVM support.
- Python 3.
- SSH client.
- Process-fault controller.
- Disposable QEMU overlay disk.
- Port `39091` available for checkpoint communication.

### Ubuntu guest

- CXL-enabled kernel.
- `/dev/cxl/mem0` available.
- Firmware client built at `~/fw-management/build/cxl-fw-client`.
- Synthetic firmware image:

```text
~/fw-management/firmware/synthetic-test-8k.bin
```

- SSH server enabled.

---

## 5. Controller Configuration

The controller is located in the repository:

```text
tools/host/cxl_process_fault_controller.py
```

### Arch host paths

| Resource | Path |
|---|---|
| QEMU binary | `~/Projects/qemu/qemu/build/qemu-system-x86_64` |
| Overlay disk | `~/Projects/qemu/process-fault-test.qcow2` |
| QMP socket | `/tmp/qemu-cxl-qmp.sock` |
| Log directory | `~/Projects/qemu/process-fault-logs/` |

### Guest configuration

| Resource | Value |
|---|---|
| SSH user | `ubuntu-user` |
| SSH forwarding | `127.0.0.1:2222` |
| CXL device | `/dev/cxl/mem0` |
| Project directory | `~/fw-management` |
| Checkpoint host | `10.0.2.2` |
| Checkpoint port | `39091` |
| Firmware target slot | `2` |

The controller uses the disposable overlay disk. Do not use the base Ubuntu disk for process-fault testing.

Do not launch another QEMU instance while the controller owns the VM.

---

## 6. Running Individual Checkpoints

All commands below are executed on the **Arch Linux host**, from the repository root.

### PF1 — Before INITIATE

```bash
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF1
```

### PF2 — After INITIATE

```bash
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF2
```

### PF3-1 — After CONTINUE 1

```bash
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF3 --continue-index 1
```

### PF3-2 — After CONTINUE 2

```bash
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF3 --continue-index 2
```

### PF4 — After final CONTINUE, before END

```bash
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF4
```

For PF3, `--continue-index` is mandatory and accepts only `1` or `2`.

---

## 7. Running the Complete Suite

Run all five checkpoints sequentially:

**Arch host OS**

```bash
python3 tools/host/cxl_process_fault_controller.py --all
```

The controller executes:

```text
PF1
PF2
PF3-1
PF3-2
PF4
```

Each test starts QEMU, executes its checkpoint test, kills QEMU, restarts it, verifies recovery, and then proceeds to the next test.

If a test fails during batch execution, the controller cleans up that QEMU instance before proceeding.

---

## 8. User Interaction

During each test:

1. Select the intended custom kernel in GRUB if prompted.
2. Wait for Ubuntu to boot.
3. Authenticate SSH when prompted.
4. Enter the Ubuntu sudo password when prompted.
5. Type `TRANSFER` at the firmware client's confirmation prompt.
6. Wait for the selected checkpoint.
7. QEMU is forcibly terminated and restarted.
8. The controller checks SSH and `/dev/cxl/mem0`.

Do not manually launch the firmware client in another terminal.

### After a single-checkpoint test

QEMU remains running after the test completes.

Press `Ctrl+C` in the controller terminal to stop it.

### After a batch test

The controller stops QEMU between tests and after the batch finishes.

---

## 9. Logs

All process-fault logs are stored on the Arch host.

Directory:

```text
~/Projects/qemu/process-fault-logs/
```

### Controller event logs

Each test generates a JSONL event log.

Example:

```text
20261117-134800-20261117-134800-abcdef-pf3-1.jsonl
```

Events can include:

- Controller startup
- QEMU startup
- SSH service availability
- SSH authentication
- CXL device availability
- Guest-client startup
- Checkpoint reception
- QEMU SIGKILL
- QEMU restart
- Post-restart SSH and CXL device checks
- Test completion
- Cleanup and controller errors

### QEMU logs

A separate file is generated for each test:

```text
qemu-<test_id>.log
```

These contain QEMU stdout and stderr.

The JSONL controller log and QEMU output log are separate and serve different purposes.

---

## 10. Process-Fault Acceptance Criteria

A checkpoint test is considered successfully completed by the controller when:

1. The expected checkpoint is received.
2. QEMU is terminated using SIGKILL.
3. QEMU restarts successfully.
4. SSH becomes available.
5. SSH authentication succeeds.
6. `/dev/cxl/mem0` reappears.
7. The controller records successful completion.

These checks establish process restart and basic guest/device availability.

They do not establish:

- Firmware-transfer resumption.
- Firmware-state persistence.
- Integrity of partially transferred firmware.
- Firmware-slot correctness.
- Firmware activation.
- Successful subsequent firmware transfer.
- Firmware bootability.

---

## 11. Safety and Scope

- These tests perform a real firmware-transfer sequence up to the selected checkpoint.
- The current controller targets slot `2`.
- The synthetic test firmware is used.
- The disposable overlay disk must be used.
- The base Ubuntu disk must remain untouched.
- Firmware activation is not performed.
- Avoid launching multiple QEMU instances against the same overlay.

---

## 12. Relevant Files

```text
tools/
├── host/
│   └── cxl_process_fault_controller.py
└── reports/
    └── ...

~/Projects/qemu/
├── process-fault-test.qcow2
└── process-fault-logs/
```

Related documentation:

- [Core Project Configuration](CORE_PROJECT_CONFIGURATION.md)
- [Client Usage](CLIENT_USAGE.md)
- [Protocol Fault Testing](PROTOCOL_FAULT_TESTING.md)