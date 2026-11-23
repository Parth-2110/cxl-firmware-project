# CXL Firmware Project — Core Configuration

## 1. Project Environment

The project uses two Linux environments:

- **WSL2 Ubuntu host:** Runs the custom QEMU build and starts the CXL virtual machine.
- **Ubuntu guest:** Runs the Linux CXL driver, userspace firmware client, fault-injection utility, and regression tests.

The guest accesses a QEMU-emulated CXL Type 3 device through `/dev/cxl/mem0`.

## 2. Host Environment — Windows / WSL2 Ubuntu

| Component | Configuration |
|---|---|
| Host OS | Windows |
| Linux environment | WSL2 Ubuntu |
| Virtualization | QEMU |
| QEMU build | Custom-built with CXL support |
| QEMU source | `~/qemu-workspace/qemu` |
| QEMU executable | `~/qemu-workspace/qemu/build/qemu-system-x86_64` |
| Linux source | `~/kernel-workspace/linux-7.0` |

### Host-side launch scripts

| Path | Purpose |
|---|---|
| `~/start-cxl-vm.sh` | Starts the CXL Ubuntu guest |
| `~/open-cxl-lab.sh` | Opens the CXL lab workflow |

QEMU and Linux source trees are maintained separately from the userspace client repository.

## 3. Ubuntu Guest Environment

| Component | Configuration |
|---|---|
| Operating system | Ubuntu Linux |
| Hostname | `firmware-lab` |
| Kernel | `7.0.0-31-generic` |
| CXL driver | `cxl_mem` |
| CXL device node | `/dev/cxl/mem0` |
| Compiler | GCC |
| Build system | GNU Make |
| Test framework | Python 3 / pytest |
| Project directory | `~/cxl-firmware-client` |
| Development branch | `parth/cxl-userspace-client` |

### Repository structure

| Directory | Purpose |
|---|---|
| `src/` | Firmware information, transfer, and fault-injection clients |
| `tools/` | Command query, firmware-layout, and background-status utilities |
| `tests/` | Regression tests, firmware images, and test helpers |
| `scripts/` | Regression runner |
| `docs/` | Goal documentation and test reports |
| `build/` | Compiled executables |
| `.venv/` | Python virtual environment |

## 4. QEMU CXL Device Configuration

The QEMU launch configuration creates a 256 MiB RAM-backed CXL Type 3 device:

```bash
-object memory-backend-ram,id=vmem0,share=on,size=256M
-device cxl-type3,bus=root_port13,volatile-memdev=vmem0,id=cxl-vmem0
```

| Component | Configuration |
|---|---|
| Device model | `cxl-type3` |
| Device ID | `cxl-vmem0` |
| CXL bus | `root_port13` |
| Memory backend | `memory-backend-ram` |
| Backend ID | `vmem0` |
| Backend size | 256 MiB |
| Memory type | Volatile |

The guest environment record identifies PCI device `35:00.0`, device ID `8086:0d93`, bound to `cxl_mem`. The guest device interface is `/dev/cxl/mem0`.

## 5. CXL Userspace Communication

The client communicates with the device through the Linux CXL memory-device interface:

- Device: `/dev/cxl/mem0`
- Command submission: `CXL_MEM_SEND_COMMAND`
- Supported-command query: `CXL_MEM_QUERY_COMMANDS`

Firmware-management opcodes used by the client:

| Command | Opcode | Purpose |
|---|---:|---|
| Get FW Info | `0x0200` | Read firmware slot information |
| Transfer FW | `0x0201` | Transfer firmware data |
| Activate FW | `0x0202` | Request firmware activation |
| Background Operation Status | `0x0002` | Read background-operation status |

The transfer client reads the device payload limit from:

`/sys/bus/cxl/devices/mem0/payload_max`

## 6. Linux and QEMU Source Investigation

The project studied the existing implementation in the separate Linux and QEMU source trees.

| Component | Source files / functions | Purpose |
|---|---|---|
| Linux mailbox | `drivers/cxl/core/mbox.c` — `cxl_internal_send_cmd()` | Common mailbox command path |
| Linux PCI transport | `drivers/cxl/pci.c` — `cxl_pci_mbox_send()`, `__cxl_pci_mbox_send_cmd()` | PCI mailbox transport |
| Linux firmware handling | `drivers/cxl/core/memdev.c` — firmware info, upload, and activation functions | Firmware-management flow |
| QEMU mailbox registers | `hw/cxl/cxl-device-utils.c` — `mailbox_reg_write()` | Doorbell and mailbox-register handling |
| QEMU command dispatch | `hw/cxl/cxl-mailbox-utils.c` — `cxl_process_cci_message()` | Command dispatch |
| QEMU firmware handlers | `cmd_firmware_update_get_info()`, `cmd_firmware_update_transfer()`, `cmd_firmware_update_activate()` | Emulated firmware commands |

**Modification status:** No Linux kernel source patch or QEMU `hw/cxl/` source patch is recorded in the reviewed project branch. The project configured the QEMU Type 3 device at launch and developed the userspace client, utilities, and tests in its own repository.

## 7. Project Architecture

```text
┌──────────────────── WINDOWS HOST ─────────────────────┐
│                                                       │
│  WSL2 Ubuntu                                          │
│       │                                               │
│       ├── Custom QEMU source and build                │
│       ├── start-cxl-vm.sh                             │
│       └── open-cxl-lab.sh                             │
│                    │                                  │
│                    v                                  │
│              QEMU Virtual Machine                     │
│                    │                                  │
│                    v                                  │
│          CXL Type 3 Device (256 MiB)                  │
│                                                       │
└────────────────────┬──────────────────────────────────┘
                     │
               Ubuntu Guest
                     │
┌────────────────────v──────────────────────────────────┐
│                                                       │
│  Linux CXL Driver (cxl_mem)                           │
│                    │                                  │
│                    v                                  │
│              /dev/cxl/mem0                            │
│                    │                                  │
│                    v                                  │
│          CXL Userspace Firmware Client                │
│                    │                                  │
│        ┌───────────┼────────────┐                     │
│        v           v            v                     │
│   FW Queries   FW Transfer   Fault Injection          │
│        │           │            │                     │
│        └───────────┼────────────┘                     │
│                    v                                  │
│          CXL_MEM_SEND_COMMAND ioctl                   │
│                    │                                  │
│                    v                                  │
│             Linux CXL Mailbox                         │
│                    │                                  │
│                    v                                  │
│             QEMU Mailbox Handler                      │
│                                                       │
│  pytest Regression Tests  ──> TXT / XML Reports       │
│                                                       │
└───────────────────────────────────────────────────────┘
```

## 8. Testing Configuration

Protocol fault injection and automated regression tests run inside the Ubuntu guest.

- Fault utility: `build/fault_inject`
- Test directory: `tests/regression/`
- Regression runner: `scripts/run-regression.sh`
- Default test image: `tests/firmware-images/test_fw.bin`
- Report directory: `docs/test-reports/goal5/`

Run the regression modes:

```bash
make quick
make full
```

The suite covers nine firmware-transfer fault scenarios. The saved reports record 9 passed and 0 failed for both modes. These results apply to the QEMU-emulated device and do not establish physical firmware activation or bootability.

## 9. Host and Guest Responsibility

| Operation | Environment |
|---|---|
| Build and maintain custom QEMU | WSL2 Ubuntu host |
| Start the CXL virtual machine | WSL2 Ubuntu host |
| Access `/dev/cxl/mem0` | Ubuntu guest |
| Build and run the C client | Ubuntu guest |
| Query firmware and supported commands | Ubuntu guest |
| Submit firmware-transfer requests | Ubuntu guest |
| Run protocol fault injection | Ubuntu guest |
| Run pytest regression tests | Ubuntu guest |
| Store regression reports | Ubuntu guest repository |

**Important:** QEMU process management belongs in the host terminal. Client, device, and regression commands belong in the Ubuntu guest terminal.

## 10. Validation Boundary

The current test device is a QEMU CXL Type 3 model backed by volatile RAM. The project has not established:

- End-to-end firmware activation and post-reset boot verification.
- Firmware persistence across QEMU/device restart.
- Physical CXL hardware validation.
- Successful process-interruption and recovery testing.

The project demonstrates command-level firmware-management behavior in the emulated environment. It does not claim production-device firmware validation.

---

**Related documentation:** `README.md`, `docs/goal1/goal-1.md`, `docs/goal2/`, `docs/goal3/`, `docs/goal4/`, `docs/goal5/`, and `docs/test-reports/goal5/`.
