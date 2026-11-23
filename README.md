# CXL Firmware Project

**A C-based userspace client for CXL firmware management, developed and tested using Linux and QEMU.**

[GitHub Repository](https://github.com/Parth-2110/cxl-firmware-project) | [Development Branch](https://github.com/Parth-2110/cxl-firmware-project/tree/parth/cxl-userspace-client)

## 1. Overview

This project explores firmware management for Compute Express Link (CXL) devices. It investigates Linux CXL driver implementation, QEMU device emulation, and mailbox communication before developing a custom C userspace client.

The project uses a QEMU-emulated CXL Type 3 device to develop and test firmware-management operations in an isolated environment.

## 2. Objectives

- Understand Linux CXL and QEMU firmware-management implementations.
- Investigate CXL mailbox commands and ioctl communication.
- Develop a C userspace client for firmware operations.
- Implement firmware-transfer and activation functionality.
- Test fault scenarios and automate regression testing.

## 3. Features

- Retrieve firmware information and supported commands.
- Inspect firmware layout and background-operation status.
- Construct and submit firmware-transfer requests.
- Implement firmware activation requests.
- Perform nine firmware-transfer fault-injection tests.
- Run automated Quick Demo and Full Regression tests.
- Generate TXT and JUnit XML test reports.

## 4. Architecture

```text
Windows Host
    |
    v
WSL2 Ubuntu
    |
    v
QEMU Virtual Machine
    |
    v
Ubuntu Linux Guest
    |
    v
CXL Type 3 Device (Emulated)
    |
    v
/dev/cxl/mem0
    |
    v
C Userspace Client
    |
    v
CXL_MEM_SEND_COMMAND ioctl
    |
    v
Linux CXL Mailbox Driver
    |
    v
QEMU Mailbox Handlers
```

## 5. Technologies Used

- **Languages:** C, Python, Bash
- **Build tools:** GCC, GNU Make
- **Testing:** pytest
- **Virtualization:** QEMU
- **Operating system:** Ubuntu Linux, WSL2
- **Interface:** Linux CXL subsystem, ioctl

## 6. Prerequisites

- Ubuntu Linux guest running under QEMU.
- QEMU configured with a CXL Type 3 device.
- Linux CXL driver and `/dev/cxl/mem0`.
- GCC, Make, Python 3, and pip.
- Git and Python virtual environment support.

## 7. Installation

Clone the development branch:

```bash
git clone --branch parth/cxl-userspace-client --single-branch https://github.com/Parth-2110/cxl-firmware-project.git
cd cxl-firmware-project
```

Install dependencies:

```bash
sudo apt update
sudo apt install -y build-essential make python3 python3-venv python3-pip linux-libc-dev
```

Create the Python environment:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install pytest
```

Build the client:

```bash
make check-env
make build
```

## 8. Usage

Display available commands:

```bash
make help
```

Common operations:

| Command | Purpose |
|---|---|
| `make device` | Detect the CXL device |
| `make fw-info` | Retrieve firmware information |
| `make fw-layout` | Inspect firmware layout |
| `make commands` | Query supported commands |
| `make bg-status` | Check background status |
| `make transfer` | Start firmware-transfer client |
| `make image-info` | Inspect test firmware images |
| `make quick` | Run Quick Demo regression |
| `make full` | Run Full Regression |

**Safety:** Firmware submissions and fault-injection commands must only be performed against the intended isolated QEMU test device, not production hardware.

## 9. Project Structure

```text
cxl-firmware-project/
├── docs/           # Technical documentation and test reports
├── experiments/    # Experiment recording guidelines
├── scripts/        # Regression execution scripts
├── src/            # Main C client programs
├── tools/           # Supporting utilities
├── tests/           # Regression tests and firmware images
├── Makefile
└── README.md
```

## 10. Testing

The regression suite covers nine fault scenarios, including invalid actions, invalid slots, malformed payloads, and incorrect transfer sequences.

Run the tests:

```bash
make quick
make full
```

Recorded results:

| Test | Passed | Failed | Result |
|---|---:|---:|---|
| Quick Demo | 9 | 0 | PASS |
| Full Regression | 9 | 0 | PASS |

Reports are stored in `docs/test-reports/goal5/`.

## 11. Current Status

- [x] QEMU and Ubuntu CXL environment established.
- [x] CXL Type 3 device detection verified.
- [x] Linux and QEMU source-code investigation documented.
- [x] C userspace client utilities implemented.
- [x] Firmware-transfer operations tested in QEMU.
- [x] Nine fault scenarios automated.
- [x] Quick and Full Regression suites passed.

**Limitation:** QEMU testing does not establish physical firmware bootability, persistence, or successful activation on real CXL hardware.

## 12. Documentation

| Document | Purpose |
|---|---|
| [Goal 1](docs/goal1/) | Environment and source-code analysis |
| [Goal 2](docs/goal2/userspace-client-notes.md) | Userspace client implementation |
| [Goal 3](docs/goal3/firmware-update-flow.md) | Firmware update flow |
| [Goal 4](docs/goal4/goal4-fault-injection-results.md) | Fault-injection results |
| [Goal 5](docs/goal5/goal5-validation.md) | Regression validation |
| [Test Reports](docs/test-reports/goal5/) | TXT and XML results |
| [Troubleshooting](docs/troubleshooting-log.md) | Issues and resolutions |

## 13. Contributing

Contributions and suggestions are welcome. Use separate development branches, document changes, and test modifications in the isolated QEMU environment before submitting them.

## 14. License

No `LICENSE` file has been added yet. Licensing remains to be decided.

---

**Development Branch:** [`parth/cxl-userspace-client`](https://github.com/Parth-2110/cxl-firmware-project/tree/parth/cxl-userspace-client)
