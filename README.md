# CXL Firmware Project

## Overview

This project investigates firmware management for Compute Express Link
(CXL) devices, using QEMU and Linux to understand the existing protocol
and implementation before developing new functionality.

## Project objectives

1. **Environment and protocol literacy** — establish a working QEMU,
   Linux, and CXL environment and trace the relevant source code.
2. **Source-code analysis** — document Linux mailbox handling, QEMU
   device emulation, firmware transfer, and activation.
3. **Implementation and validation** — develop and test new functionality
   after the existing behavior is understood.

## Current status

**Goal 1 — Environment and Protocol Literacy**

- [x] Boot the Ubuntu guest under QEMU.
- [x] Verify CXL Type 3 PCI device enumeration.
- [x] Verify `/dev/cxl/mem0` and driver binding.
- [x] Inspect the relevant Linux and QEMU firmware-update code.
- [ ] Complete the annotated end-to-end source trace.
- [ ] Define and validate future experiments.

Source inspection does not establish that a complete firmware update
has succeeded.

## Documentation

| Document | Purpose |
|---|---|
| [Environment Setup](docs/goal-1-environment-setup.md) | Environment and validation results |
| [Linux Source Notes](docs/goal-1-linux-source-notes.md) | Linux CXL driver and firmware-upload path |
| [QEMU Source Notes](docs/goal-1-qemu-source-notes.md) | QEMU mailbox and firmware handlers |
| [Firmware Update Flow](docs/firmware-update-flow.md) | End-to-end command path |
| [Troubleshooting Log](docs/troubleshooting-log.md) | Issues and resolutions |
| [Experiments](experiments/README.md) | Experiment recording template |

## Development environment

- Host: Windows with WSL2 Ubuntu
- Virtualization and device emulation: QEMU
- Guest operating system: Ubuntu Linux
- Device model: CXL Type 3
- Languages for implementation: C, as appropriate to the project

## Repository conventions

- Record verified observations separately from assumptions.
- Document commands, expected results, actual results, and evidence.
- Keep upstream QEMU and Linux source trees separate from this repository.
- Do not commit virtual machine disks, build outputs, credentials, or
  private keys.
- Do not mark a firmware update successful without observed validation.

## Project history

Progress and source-analysis findings are recorded in the `docs/`
directory and tracked through Git commits.
