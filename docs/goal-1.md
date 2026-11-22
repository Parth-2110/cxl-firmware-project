# Goal 1 — Environment and Protocol Literacy

## Objective

Get a working QEMU + Linux + CXL setup running and understand the
existing code paths before writing new functionality.

## Environment

| Component | Details |
|---|---|
| Host | Windows with WSL2 Ubuntu |
| WSL username | parth |
| QEMU source | `~/qemu-workspace/qemu` |
| QEMU build directory | `~/qemu-workspace/qemu/build` |
| Linux source | `~/kernel-workspace/linux-7.0` |
| Guest kernel | `7.0.0-31-generic` |
| Guest hostname | `firmware-lab` |
| CXL VM disk | Separate QCOW2 overlay |
| GitHub repository | `Parth-2110/cxl-firmware-project` |

## Verified setup results

- [x] QEMU custom build exists.
- [x] Ubuntu guest boots under QEMU.
- [x] QEMU exposes a CXL Type 3 device.
- [x] Guest PCI enumeration detects the CXL device.
- [x] `/dev/cxl/mem0` exists in the guest.
- [x] The CXL memory device is bound to the `cxl_mem` driver.
- [x] CXL-related kernel modules are loaded.
- [x] CXL device sysfs entries are present.
- [x] Firmware-upload sysfs interface entries were observed.
- [x] Linux and QEMU firmware-related source paths were inspected.

## Evidence recorded during setup

PCI device:
`35:00.0 CXL [0502]: Intel Corporation Device [8086:0d93] (rev 01)`

Device node:
`/dev/cxl/mem0`

Driver:
`cxl_mem`

Reported firmware revision:
`BWFW VERSION 00`

These observations demonstrate device enumeration and driver binding.
They do not demonstrate a successful firmware update.

## Source versions

QEMU:
`11.1.50` (`v11.1.0-1664-gc1c18d1e64`)

Linux source tag:
`Ubuntu-hwe-7.0-7.0.0-31.31_24.04.1`

Linux source commit:
`cc909f9a3d277832d246c6493bbc06e0955cde2e`

## Important safety notes

- Preserve the original Ubuntu QCOW2 disk.
- Use the dedicated CXL overlay for lab experiments.
- Do not upload VM disks or build directories to GitHub.
- Do not attempt a firmware update without a suitable test image and
  a clearly defined validation procedure.

## Remaining work

- [ ] Finish the annotated Linux source notes.
- [ ] Finish the annotated QEMU source notes.
- [ ] Document the end-to-end mailbox command path.
- [ ] Verify remaining firmware-slot encoding and selection details.
- [ ] Define future experiments and their expected results.
- [ ] Do not mark end-to-end firmware update as complete until tested.
