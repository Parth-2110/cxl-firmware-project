
# Process-Level Fault Injection

Process-level fault injection is controlled from the **Arch Linux host**. The CXL firmware client runs inside the **Ubuntu guest**.

## 1. Overview

The process-fault controller terminates the QEMU process using SIGKILL at specific checkpoints during a firmware-transfer sequence.

After termination, the controller restarts QEMU and checks whether:

1. The guest SSH service becomes available.
2. SSH authentication succeeds.
3. `/dev/cxl/mem0` reappears.

The controller records the test lifecycle in host-side JSONL logs.

## 2. Architecture

| Component | Location |
|---|---|
| QEMU binary | Arch host |
| QEMU overlay disk | Arch host |
| Python process-fault controller | Arch host |
| Checkpoint listener | Arch host, TCP port 39091 |
| Firmware client | Ubuntu guest |
| Synthetic firmware image | Ubuntu guest |
| Process-fault event log | Arch host |
| QEMU stdout/stderr log | Arch host |

The guest connects to the host checkpoint listener through `10.0.2.2`.

The controller uses SSH forwarding through `127.0.0.1:2222`.

## 3. Requirements

### Arch host

- QEMU built with CXL support.
- KVM support.
- Python 3.
- QEMU overlay disk at the configured location.
- The process-fault controller.
- SSH port forwarding configured by the controller.
- Port 39091 available for checkpoint communication.

### Ubuntu guest

- The configured CXL-enabled kernel.
- `/dev/cxl/mem0` available after boot.
- `~/fw-management/build/cxl-fw-client`.
- `firmware/synthetic-test-8k.bin`.
- SSH server enabled.

## 4. Controller Configuration

The controller is configured for the original development environment.

Current expected paths:

| Resource | Arch host path |
|---|---|
| QEMU binary | `~/Projects/qemu/qemu/build/qemu-system-x86_64` |
| Overlay disk | `~/Projects/qemu/process-fault-test.qcow2` |
| QMP socket | `/tmp/qemu-cxl-qmp.sock` |
| Process-fault logs | `~/Projects/qemu/process-fault-logs/` |

Guest configuration:

| Resource | Value |
|---|---|
| SSH username | `ubuntu-user` |
| SSH forwarding | `127.0.0.1:2222` |
| CXL device | `/dev/cxl/mem0` |
| Project directory | `~/fw-management` |
| Checkpoint host | `10.0.2.2` |
| Checkpoint port | `39091` |

The controller uses the overlay disk. It must not use the base Ubuntu disk for process-fault experiments.

Do not launch another QEMU instance while the controller owns the VM.

## 5. Controller Location in This Repository

The controller is stored at:

```text
tools/host/cxl_process_fault_controller.py
```

Run it from the Arch host.

The controller's QEMU paths and networking settings are machine-specific. Review them before using this controller on another machine.

## 6. Running the Tests

Open an Arch host terminal and change to the repository directory.

### PF1 — Before INITIATE

```bash
python3 tools/host/cxl_process_fault_controller.py --checkpoint PF1
```

The controller boots the guest, authenticates over SSH and starts the firmware client automatically.

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

## 7. User Interaction

The controller handles guest-client execution over SSH.

During execution:

1. Select the intended kernel in GRUB when prompted.
2. Wait for Ubuntu to boot and SSH to become available.
3. Enter the Ubuntu sudo password when prompted.
4. Type `TRANSFER` when the firmware client asks for confirmation.
5. The client sends the checkpoint to the host controller.
6. The controller terminates QEMU and restarts it.
7. The controller verifies SSH and `/dev/cxl/mem0`.

After the test completes, QEMU remains running. Press Ctrl+C in the Arch controller terminal to stop it.

Do not manually launch the guest client in a second terminal.

## 8. Logging

All process-fault logs are stored on the Arch host.

Directory:

```text
~/Projects/qemu/process-fault-logs/
```

### Controller event log

Each controller run creates a separate JSONL file.

Example:

```text
2026-11-08T08-40-47+00-00-5fb2b3f7da.jsonl
```

The log records events such as:

- Controller startup
- QEMU startup
- SSH service availability
- SSH authentication
- CXL device availability
- Guest-client startup
- Checkpoint reception
- QEMU SIGKILL and exit status
- QEMU restart
- Post-restart SSH and CXL device checks
- Test completion and cleanup

### QEMU output log

A separate file is generated:

```text
qemu-<test_id>.log
```

It contains QEMU stdout and stderr.

The JSONL controller log and QEMU output log serve different purposes.

## 9. Process-Fault Acceptance Criteria

A process-fault test passes when:

1. The expected checkpoint is received.
2. QEMU is terminated using SIGKILL.
3. QEMU restarts successfully.
4. SSH connectivity is restored.
5. `/dev/cxl/mem0` reappears.
6. The controller records successful completion.

These tests validate QEMU process termination and restart behaviour. They do not validate firmware-transfer resumption or firmware-state persistence.

## 10. Safety and Scope

- Process-fault tests perform a real firmware-transfer sequence up to the selected checkpoint.
- The transfer targets slot 2 in the current controller configuration.
- Use the disposable overlay disk.
- The base Ubuntu disk must remain untouched.
- Firmware activation is not performed.
- Do not run a second QEMU instance while the controller is active.