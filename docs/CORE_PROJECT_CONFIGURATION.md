# Core Project Configuration

## 1. Project Environment

The CXL Firmware Management and Fault-Injection Framework operates across two Linux environments:

- **Arch Linux host:** Runs QEMU, the emulated CXL Type 3 memory device, and the process-level fault-injection controller.
- **Ubuntu guest:** Runs the CXL firmware-management userspace client, protocol-level fault injection, unit tests, and protocol report generation.

The separation allows the project to inject faults at both the userspace protocol level and the QEMU process level.

## 2. Host Environment — Arch Linux

### Operating Environment

| Component | Configuration |
|---|---|
| Operating System | Arch Linux |
| Virtualization | QEMU + KVM |
| Emulator | Custom-built QEMU |
| Architecture | x86_64 |
| CPU | Host CPU passthrough |
| Networking | QEMU user networking |
| Guest SSH forwarding | Host port 2222 → Guest port 22 |

### QEMU Build

QEMU is built locally with CXL device support.

**QEMU source directory:**

`~/Projects/qemu/qemu`

**QEMU executable:**

`~/Projects/qemu/qemu/build/qemu-system-x86_64`

The custom QEMU binary is used instead of relying on the system-installed QEMU package.

### Host Project Files

| Path | Purpose |
|---|---|
| `~/Projects/qemu/qemu/` | QEMU source tree |
| `~/Projects/qemu/qemu/build/qemu-system-x86_64` | Built QEMU emulator |
| `~/Projects/qemu/process-fault-test.qcow2` | VM overlay disk used by process fault testing |
| `~/Projects/qemu/process-fault-logs/` | Process fault-injection logs |
| `~/Projects/qemu/cxl_process_fault_controller.py` | Process-level fault controller |
| `~/Projects/qemu/generate_process_fault_report.py` | Process-level report generator |

The process-level fault controller is located in the project repository under:

`tools/host/cxl_process_fault_controller.py`

## 3. Ubuntu Guest Environment

The Ubuntu virtual machine provides the execution environment for the firmware-management client.

### Requirements

- Linux kernel with CXL memory-device support
- CXL PCI and memory-device drivers
- Raw CXL mailbox command support enabled
- `CXL_MEM_SEND_COMMAND` ioctl support
- GCC
- GNU Make
- Python 3 for firmware generation and reporting
- A QEMU-emulated CXL Type 3 device exposed through the Linux CXL subsystem

### Kernel Configuration Requirement

The userspace client relies on the kernel exposing raw CXL mailbox commands.

The required kernel configuration is:

`CONFIG_CXL_MEM_RAW_COMMANDS=y`

Without this configuration, the userspace client cannot use the raw command interface required for its firmware-transfer operations.

The CXL device is accessed through:

`/dev/cxl/mem0`

The client uses the Linux CXL memory-device ioctl interface rather than directly accessing PCI registers or implementing the mailbox transport itself.

### Guest Project Directory

The project is located at:

`~/fw-management`

Important subdirectories include:

| Directory | Purpose |
|---|---|
| `src/` | Userspace client implementation |
| `src/core/` | Mailbox, transfer engine, request handling and logging |
| `src/commands/` | Firmware-management commands |
| `src/fault_injection/` | Fault-injection implementation |
| `src/fault_injection/protocol/` | Protocol-level fault cases |
| `include/` | Shared header and definitions |
| `tests/` | Unit tests and synthetic firmware generator |
| `build/` | Compiled binaries and object files |
| `firmware/` | Synthetic firmware images |
| `logs/` | Protocol fault-injection CSV logs |
| `reports/` | Generated test reports |
| `tools/` | Reporting and supporting utilities |

## 4. QEMU Virtual Machine Configuration

The process-level fault controller launches QEMU with a Q35 machine configuration and CXL support enabled.

### Virtual Machine Configuration

| Parameter | Value |
|---|---|
| Machine | Q35 |
| Acceleration | KVM |
| CPU | Host CPU passthrough |
| vCPUs | 2 |
| Initial RAM | 4 GiB |
| Maximum RAM | 8 GiB |
| Memory hotplug slots | 8 |
| Guest disk | QCOW2 overlay |
| Network | User-mode networking |
| SSH forwarding | TCP 2222 → Guest TCP 22 |
| QMP | Unix socket |
| Display | GTK |

### CXL Device Configuration

The virtual machine includes a CXL hierarchy consisting of:

- CXL PCIe host bridge (`pxb-cxl`)
- CXL root port (`cxl-rp`)
- CXL Type 3 memory device (`cxl-type3`)
- RAM-backed CXL memory backend
- CXL Fixed Memory Window (FMW)

The Type 3 memory device is connected to the CXL root port and backed by a RAM backend.

| Component | Configuration |
|---|---|
| CXL host bridge | `pxb-cxl` |
| CXL root port | `cxl-rp` |
| CXL memory device | `cxl-type3` |
| Memory backend | RAM |
| Backend size | 256 MiB |
| CXL Fixed Memory Window | 4 GiB |

The CXL Fixed Memory Window is configured with a 4 GiB size, while the RAM backend backing the Type 3 device is configured as 256 MiB.

The CXL memory device is presented to the guest operating system, where the Linux CXL subsystem enumerates and exposes it as a memory device.

## 5. Virtual Machine Networking

QEMU uses user-mode networking for the guest.

SSH access is forwarded through:

`127.0.0.1:2222 → Ubuntu guest:22`

The process-level fault controller uses this forwarding to communicate with the guest and execute firmware-transfer commands.

The guest is accessed using the configured SSH connection:

`ubuntu-user@127.0.0.1`

The controller also uses a separate checkpoint communication channel to coordinate fault injection between the guest firmware client and the host-side controller.

## 6. Process-Level Fault Controller Configuration

The process-level controller runs on the Arch host.

Its configuration includes:

| Parameter | Value |
|---|---|
| QEMU executable | `~/Projects/qemu/qemu/build/qemu-system-x86_64` |
| VM overlay | `~/Projects/qemu/process-fault-test.qcow2` |
| Log directory | `~/Projects/qemu/process-fault-logs/` |
| QMP socket | `/tmp/qemu-cxl-qmp.sock` |
| Checkpoint listener | `0.0.0.0:39091` |
| SSH host | `127.0.0.1` |
| SSH port | `2222` |
| Guest | `ubuntu-user@127.0.0.1` |
| Guest project | `~/fw-management` |

The controller is responsible for launching QEMU, waiting for guest availability, coordinating the firmware-transfer checkpoints, terminating QEMU at the specified checkpoint, restarting it, and checking guest availability.

Detailed execution procedures are documented in `PROCESS_FAULT_TESTING.md`.

## 7. CXL Userspace Communication

The userspace firmware-management client communicates with the CXL device through the Linux kernel's CXL memory-device interface.

The primary device node is:

`/dev/cxl/mem0`

The client uses:

`CXL_MEM_SEND_COMMAND`

For raw mailbox commands, the client constructs a `cxl_send_command` structure and submits it through the ioctl interface.

The raw command implementation is located in:

`src/core/mailbox.c`

The firmware-transfer command uses the CXL mailbox opcode:

`0x0201` — TRANSFER_FW

The client handles firmware-transfer request construction and sequencing in userspace, while the kernel provides the ioctl interface and command submission path.

## 8. Kernel Modification — Firmware Transfer Polling

A kernel-side polling adjustment was investigated for raw firmware-transfer commands.

### Problem

The raw userspace command path did not initialize background-operation polling parameters in the same way as the normal kernel command path.

This resulted in polling behavior associated with a zero-millisecond timeout when handling a background firmware-transfer operation.

### Intended Adjustment

The proposed adjustment was a narrowly scoped exception for the raw `TRANSFER_FW` command:

- Poll interval: 1000 ms
- Poll count: 30

This would provide an approximate polling window of 30 seconds for the background operation.

The adjustment was intended to affect raw `TRANSFER_FW` commands without changing the polling behavior of unrelated raw commands.

### Status

This kernel modification is relevant to the project configuration and firmware-transfer execution environment.

However, the final patch, kernel build, installation, and successful verification of the modification are not established by this document. The polling adjustment should not be treated as verified unless the corresponding kernel source, configuration, and test results confirm it.

## 9. Project Architecture

The overall execution architecture is:

```text
┌──────────────────────── ARCH LINUX HOST ────────────────────────┐
│                                                                 │
│  Process-Level Fault Controller                                 │
│       │                                                         │
│       ├── Launch QEMU                                           │
│       ├── Coordinate checkpoints                                │
│       ├── Terminate QEMU                                        │
│       ├── Restart QEMU                                          │
│       └── Record host-side logs                                 │
│                                                                 │
│  QEMU                                                           │
│       │                                                         │
│       └── CXL Type 3 Virtual Memory Device                      │
│                                                                 │   
└───────────────────────────┬─────────────────────────────────────┘
                            │
                      Virtual Machine
                            │
┌──────────────────────── UBUNTU GUEST ──────────────────────────┐
│                                                                │
│  CXL Userspace Firmware Client                                 │
│       │                                                        │
│       ├── Query CXL commands                                   │
│       ├── Get firmware information                             │
│       ├── Transfer firmware                                    │
│       ├── Abort transfer                                       │
│       ├── Protocol-level fault injection                       │
│       └── Local unit tests                                     │
│                                                                │
│                     CXL_MEM_SEND_COMMAND                       │
│                            │                                   │
│                      Linux CXL Driver                          │
│                            │                                   │
│                      CXL Mailbox                               │
│                            │                                   │
│                    QEMU CXL Type 3 Device                      │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

## 10. Build Environment

The userspace client is built using GCC and GNU Make.

The Makefile handles:

- Compilation of the CXL firmware-management client
- Compilation of the synthetic firmware generator
- Unit-test execution
- Firmware image generation
- Firmware-transfer dry runs
- Live firmware-transfer execution
- Protocol-level fault injection
- Protocol-level report generation

The build output is stored under:

`build/`

The primary executable is:

`build/cxl-fw-client`

## 11. Environment Separation

The project deliberately separates host-side and guest-side responsibilities.

| Operation | Environment |
|---|---|
| Build and run QEMU | Arch host |
| Launch process fault controller | Arch host |
| Manage QEMU process termination/restart | Arch host |
| Process fault logs | Arch host |
| Compile firmware client | Ubuntu guest |
| Query CXL device | Ubuntu guest |
| Firmware-transfer operations | Ubuntu guest |
| Protocol fault injection | Ubuntu guest |
| Protocol CSV logs | Ubuntu guest |
| Protocol report generation | Ubuntu guest |

Commands and procedures in the remaining documentation specify the relevant operating system to prevent host/guest command confusion.

## 12. Related Documentation

- `CLIENT_USAGE.md` — client compilation, command usage, firmware generation, and transfer operations.
- `PROTOCOL_FAULT_TESTING.md` — protocol-level fault scenarios and execution.
- `PROCESS_FAULT_TESTING.md` — host-side process-level fault injection.
- `PROJECT_PROGRESS.md` — project development history and completed work.
- `README.md` — project overview and entry point.