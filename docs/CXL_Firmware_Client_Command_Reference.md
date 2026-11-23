# CXL Firmware Client — Command Reference

This document describes the commands available in the **CXL Firmware Project** repository.

**Repository:** `Parth-2110/cxl-firmware-project`  
**Branch:** `parth/cxl-userspace-client`  
**Guest project directory:** `~/cxl-firmware-client`  
**Device node:** `/dev/cxl/mem0`

Unless a section says otherwise, run commands inside the **Ubuntu guest OS**, from the project root:

```bash
cd ~/cxl-firmware-client
```

The QEMU launch scripts run separately in the **Windows/WSL2 host terminal**. They are not Makefile targets in this repository.

---

## 1. Check Available Commands

Display the Makefile command list:

```bash
make help
```

The Makefile's default target is `help`, so running `make` without a target displays the available commands.

## 2. Check the Development Environment

The project expects a Python virtual environment and the default firmware test image.

Create the virtual environment and install pytest:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install pytest
```

Check the environment:

```bash
make check-env
```

This checks that:
- The command is run from the repository root.
- `.venv/bin/python` exists.
- pytest is available.
- The default test image exists at `tests/firmware-images/test_fw.bin`.

## 3. Build the Client and Supporting Utilities

Build all C programs:

```bash
make build
```

The build uses GCC with the Makefile defaults:

```text
-Wall -Wextra -O2
```

The executables are created under `build/`:

```text
build/
├── transfer_fw
├── get_fw_info
├── fault_inject
├── check_bg_status
├── check_fw_layout
├── query_commands
└── process_fault
```

List the compiled binaries:

```bash
make binaries
```

Build individual program groups when needed:

```bash
make build-transfer
make build-info
make build-fault
```

The `build-tools` and `build-all` targets currently depend on the full `build` target.

> **Note:** The repository does not currently define a `make clean` target. Build outputs are written to `build/`, which is ignored by Git.

## 4. Verify the CXL Device

Check that the guest can access the CXL device:

```bash
make device
```

This lists `/dev/cxl/`, checks `/dev/cxl/mem0`, runs `cxl list`, and displays matching PCI devices.

The expected device node is:

```text
/dev/cxl/mem0
```

The QEMU CXL Type 3 device must be started and bound to the Linux `cxl_mem` driver before running device commands.

## 5. Query Device Information

### Query supported CXL commands

```bash
make commands
```

Uses `CXL_MEM_QUERY_COMMANDS` to query the command count and retrieve the command information reported by the Linux CXL interface.

### Get firmware information

```bash
make fw-info
```

Displays:
- Number of firmware slots.
- Current firmware slot.
- Next firmware slot.
- Live activation capability.
- Firmware revision strings for the reported slots.

### Inspect firmware-transfer structure layout

```bash
make fw-layout
```

Displays the transfer-header size, firmware-data offset, transfer-offset field position, and activation-request structure size.

### Query background-operation status

```bash
make bg-status
```

Queries Background Operation Status (`0x0002`) and displays the status byte, completion percentage, running state, reported operation opcode, operation return code, and vendor extended status.

## 6. Inspect Firmware Test Images

List and inspect the default test image:

```bash
make image-info
```

The target lists the contents of `tests/firmware-images/` and displays the selected image's file type, size, and SHA-256 checksum.

The default image is:

```text
tests/firmware-images/test_fw.bin
```

Use a different image by overriding `IMAGE`:

```bash
make image-info IMAGE=tests/firmware-images/test_fw_aligned.bin
```

Other committed test images can be listed with:

```bash
ls -lh tests/firmware-images/
```

> These are project test images. This repository does not currently provide a synthetic firmware-image generator target.

## 7. Firmware-Transfer Planning and Request Inspection

Planning and inspection commands construct or display transfer information without submitting a firmware-transfer command to the device.

### Display a transfer plan for an image

```bash
./build/transfer_fw tests/firmware-images/test_fw.bin
```

The client reads the device's `payload_max` value from:

```text
/sys/bus/cxl/devices/mem0/payload_max
```

It calculates the request size and shows the transfer action sequence. This mode does not send firmware-transfer mailbox commands.

### Build and validate requests without submission

```bash
./build/transfer_fw --build-only tests/firmware-images/test_fw.bin
```

The client reads the image, constructs the relevant transfer request or requests, verifies the header/data, and reports the result. No device transfer command is sent.

### Inspect a request using dummy data

```bash
./build/transfer_fw --inspect
```

Builds and verifies a request using internal dummy data. It does not submit the command.

### Inspect a request using a test image

```bash
./build/transfer_fw --inspect-file tests/firmware-images/test_fw.bin
```

Reads 16 bytes from the supplied image and uses them to construct an inspection request. The file must contain at least 16 bytes. It does not submit the command.

### Display direct client usage

```bash
./build/transfer_fw --help
```

The help output lists the supported direct invocation forms.

## 8. Execute Firmware Transfer

### Submit the default test image through Make

```bash
make transfer-submit
```

The default image is:

```text
tests/firmware-images/test_fw.bin
```

To select another committed test image:

```bash
make transfer-submit IMAGE=tests/firmware-images/test_fw_aligned.bin
```

### Submit an image directly

```bash
sudo ./build/transfer_fw --submit tests/firmware-images/test_fw.bin
```

The client:
1. Opens and validates the firmware image.
2. Reads the device-reported payload capacity.
3. Builds the transfer request or sequence.
4. Submits Transfer FW requests using `CXL_MEM_SEND_COMMAND`.
5. Checks the ioctl result and mailbox return value.
6. If the device reports that a background operation started, polls Background Operation Status for completion.

The implementation uses a 128-byte transfer header and 128-byte offset alignment. It uses FULL for an image that fits in one request; larger images are divided into INITIATE, CONTINUE, and END requests.

A successful command response confirms command completion in the emulated environment. It does not prove that firmware has been activated or booted.

### Interactive transfer client

```bash
make transfer
```

This starts the interactive firmware-transfer client. The menu offers:
1. Normal Firmware Transfer.
2. Process Interruption Test.

For normal transfer, the client prompts for an image path.

The process-interruption option is a coordinated lab experiment that requires a separate host terminal and the correct isolated QEMU process. It can terminate the VM under test and should not be treated as a routine transfer command.

## 9. Submit a Firmware Activation Request

The transfer client includes a direct Activate FW command path:

```bash
sudo ./build/transfer_fw --activate 2 --action 1
```

Arguments:
- `2` is the target slot in the current two-slot QEMU setup.
- `1` requests activation after reset.
- Action `0` represents online activation.

The client validates the slot and action, then submits opcode `0x0202` with a two-byte action/slot payload.

> **Important:** Activation handling is implemented, but end-to-end activation, reset behavior, and post-reset firmware boot verification have not been established by the recorded project tests. Use this command only in the isolated QEMU test environment. There is no dedicated `make activate` target in the current Makefile.

## 10. Protocol Fault Injection

Fault injection is performed by `build/fault_inject` using Transfer FW opcode `0x0201`.

### List supported fault names

```bash
./build/fault_inject --help
```

The nine scenarios are:

| Fault name | Purpose |
|---|---|
| `invalid-action` | Uses an unsupported transfer action |
| `invalid-slot` | Uses slot zero |
| `out-of-range-offset` | Specifies an offset beyond the emulated firmware-size limit |
| `bit-flip` | Flips a bit in the test data |
| `truncated-payload` | Sends a payload shorter than the 128-byte transfer header |
| `continue-without-initiate` | Sends CONTINUE without a preceding INITIATE |
| `end-without-initiate` | Sends END without a preceding INITIATE |
| `duplicate-offset` | Sends a repeated transfer offset |
| `out-of-order-offset` | Sends an overlapping/out-of-order transfer request |

### Dry run one fault

```bash
make fault-dry FAULT=invalid-action
```

Other examples:

```bash
make fault-dry FAULT=invalid-slot
make fault-dry FAULT=out-of-range-offset
make fault-dry FAULT=bit-flip
make fault-dry FAULT=truncated-payload
make fault-dry FAULT=continue-without-initiate
make fault-dry FAULT=end-without-initiate
make fault-dry FAULT=duplicate-offset
make fault-dry FAULT=out-of-order-offset
```

Dry-run mode constructs and displays the request without sending it to the device. For the duplicate-offset and out-of-order-offset cases, dry-run mode reports that the multi-request sequence is not submitted.

### Dry run all fault cases

```bash
make fault-dry-all
```

### Submit one fault to the device

```bash
make fault-submit FAULT=invalid-action
```

Replace `invalid-action` with one of the supported fault names to submit another scenario.

> **Warning:** `make fault-submit` sends deliberately malformed requests to the CXL device. Run it only against the intended isolated QEMU test device. The duplicate-offset and out-of-order-offset tests submit multiple requests and attempt ABORT cleanup. Do not use fault submission on production hardware.

## 11. Automated Regression Tests

The project uses pytest to run the nine fault scenarios in two modes.

### Quick Demo

```bash
make quick
```

Runs the nine fault-response tests. It checks the command process result, timeout status, and expected response marker.

### Full Regression

```bash
make full
```

For each fault scenario, the Full Regression test:
1. Submits the fault.
2. Checks the expected response.
3. Queries Get FW Info to check device responsiveness.
4. Attempts a subsequent clean firmware transfer using the default test image.

Both modes communicate with the emulated CXL device. They are not offline unit tests.

### List collected tests without executing them

```bash
make test-collection
```

This invokes pytest collection for `tests/regression/`. It lists tests but does not execute them.

## 12. View Regression Reports

The regression runner saves reports under:

```text
docs/test-reports/goal5/
```

List the reports:

```bash
make reports
```

Display the Quick Demo report:

```bash
make quick-report
```

Display the Full Regression report:

```bash
make full-report
```

Report files:

```text
docs/test-reports/goal5/
├── quick-results.txt
├── quick-results.xml
├── full-results.txt
└── full-results.xml
```

The TXT files are readable pytest output. The XML files contain JUnit-style results.

## 13. Project and Git Utilities

Check the current branch and working-tree status:

```bash
make project-status
```

List project source, test, and documentation files:

```bash
make files
```

Display recent commits:

```bash
make git-log
```

## 14. Important Notes and Safety

- Run guest-side Makefile and client commands from `~/cxl-firmware-client` inside the Ubuntu guest.
- Start or manage QEMU from the separate Windows/WSL2 host terminal using the lab's host-side launcher.
- Confirm `/dev/cxl/mem0` exists before running device commands.
- Build-only, inspect, and planning modes do not submit firmware-transfer commands.
- `make transfer-submit`, direct `--submit`, `--activate`, and `make fault-submit` send commands to the device.
- Use only the isolated QEMU CXL Type 3 test device and project test images.
- The current QEMU memory backend is volatile. The saved test results do not establish firmware persistence, activation/bootability, or physical CXL hardware behavior.

---

## Quick Command Summary

| Operation | Command |
|---|---|
| Show help | `make help` |
| Check environment | `make check-env` |
| Check device | `make device` |
| Build all utilities | `make build` |
| List binaries | `make binaries` |
| Get firmware information | `make fw-info` |
| Query supported commands | `make commands` |
| Inspect firmware layout | `make fw-layout` |
| Query background status | `make bg-status` |
| Inspect test images | `make image-info` |
| Plan a transfer | `./build/transfer_fw tests/firmware-images/test_fw.bin` |
| Build requests without submission | `./build/transfer_fw --build-only tests/firmware-images/test_fw.bin` |
| Submit a firmware transfer | `make transfer-submit` |
| Submit an activation request | `sudo ./build/transfer_fw --activate 2 --action 1` |
| Dry-run a fault | `make fault-dry FAULT=invalid-action` |
| Submit a fault | `make fault-submit FAULT=invalid-action` |
| Run Quick Demo | `make quick` |
| Run Full Regression | `make full` |
| List reports | `make reports` |
| View Quick report | `make quick-report` |
| View Full report | `make full-report` |
| Collect tests | `make test-collection` |
