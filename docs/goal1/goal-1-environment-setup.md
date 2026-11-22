# Goal 1 — Environment Setup and Protocol Literacy

## Objective

Establish a working QEMU, Linux, and CXL environment and understand the existing firmware-management implementation before developing a custom firmware-update client.

## 1. Environment

| Component | Details |
|---|---|
| Host | Windows with WSL2 Ubuntu |
| QEMU Source | `~/qemu-workspace/qemu` |
| QEMU Build | `~/qemu-workspace/qemu/build` |
| Linux Source | `~/kernel-workspace/linux-7.0` |
| Guest Kernel | `7.0.0-31-generic` |
| Guest Hostname | `firmware-lab` |
| VM Disk | Dedicated QCOW2 overlay |
| Repository | `Parth-2110/cxl-firmware-project` |

## 2. Setup and Verification

- Built custom QEMU with CXL support.
- Created and booted an Ubuntu VM with an emulated CXL Type-3 device.
- Verified PCI device detection and Linux driver binding.
- Confirmed `/dev/cxl/mem0` and firmware-upload sysfs interfaces.

**Verified device:**

- PCI: `35:00.0`
- Device ID: `8086:0d93`
- Driver: `cxl_mem`
- Firmware revision: `BWFW VERSION 00`

These results confirm device detection and driver binding, but not a successful firmware update.

## 3. Linux Source Investigation

Studied the following files and functions:

| File | Functions | Purpose |
|---|---|---|
| `drivers/cxl/core/mbox.c` | `cxl_internal_send_cmd()` | Validates and sends mailbox commands |
| `drivers/cxl/pci.c` | `cxl_pci_setup_mailbox()`, `cxl_pci_mbox_send()`, `__cxl_pci_mbox_send_cmd()` | Handles PCI mailbox transport |
| `drivers/cxl/core/memdev.c` | `cxl_mem_get_fw_info()` | Retrieves firmware slot information |
| `drivers/cxl/core/memdev.c` | `cxl_mem_activate_fw()` | Handles firmware activation |
| `drivers/cxl/core/memdev.c` | `cxl_fw_prepare()`, `cxl_fw_write()`, `cxl_fw_poll_complete()` | Manages firmware transfer |

## 4. QEMU Source Investigation

| File | Function | Purpose |
|---|---|---|
| `hw/cxl/cxl-device-utils.c` | `mailbox_reg_write()` | Handles mailbox register writes and doorbell requests |
| `hw/cxl/cxl-mailbox-utils.c` | `cxl_process_cci_message()` | Dispatches mailbox commands |
| `hw/cxl/cxl-mailbox-utils.c` | `cmd_firmware_update_get_info()` | Returns firmware slot information |
| `hw/cxl/cxl-mailbox-utils.c` | `cmd_firmware_update_transfer()` | Simulates firmware transfer |
| `hw/cxl/cxl-mailbox-utils.c` | `cmd_firmware_update_activate()` | Handles firmware activation |

## 5. Firmware Management Flow

The Linux and QEMU source investigations established the following command flow:

```text
Linux Firmware Management
        |
        v
cxl_internal_send_cmd()
        |
        v
PCI Mailbox Transport
        |
        v
QEMU Mailbox Registers
        |
        v
cxl_process_cci_message()
        |
        v
Get FW Info / Transfer FW / Activate FW
        |
        v
Response returned to Linux
```

QEMU models firmware transfer and slot management. It does not demonstrate installation of real executable firmware.

## 6. Results

- Established the QEMU and Ubuntu CXL environment.
- Verified CXL device enumeration and driver binding.
- Identified the Linux firmware-management and mailbox implementation.
- Studied QEMU's mailbox command handling and firmware-update handlers.
- Traced the communication path between Linux and QEMU.

## Conclusion

Goal 1 established the development environment and understanding of the existing Linux and QEMU CXL firmware-management implementation. The verified environment and source investigation provide the foundation for developing a custom userspace firmware-update client in the next phase.
