#!/usr/bin/env python3
"""Run the complete QEMU CXL process-level fault-injection suite.

Run on the Arch Linux host. The guest firmware client must implement the
--process-fault / --controller-* arguments and send CXL-PROCESS-FAULT/1
checkpoint notifications to this controller.
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import socket
import subprocess
import sys
import time
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path.home() / "Projects/qemu"
QEMU = ROOT / "qemu/build/qemu-system-x86_64"
OVERLAY = ROOT / "process-fault-test.qcow2"
LOG_DIR = ROOT / "process-fault-logs"
QMP_SOCKET = Path("/tmp/qemu-cxl-qmp.sock")
LISTEN_HOST = "0.0.0.0"
LISTEN_PORT = 39091
SSH_HOST = "127.0.0.1"
SSH_PORT = 2222
GUEST = "ubuntu-user@127.0.0.1"
GUEST_DIR = "~/fw-management"
DEVICE_TIMEOUT = 120
CHECKPOINTS = [
    ("PF1", None),
    ("PF2", None),
    ("PF3", 1),
    ("PF3", 2),
    ("PF4", None),
]


def timestamp() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def checkpoint_label(checkpoint: str, continue_index: int | None) -> str:
    return f"{checkpoint}-{continue_index}" if continue_index is not None else checkpoint


class Controller:
    def __init__(self, suite_id: str, checkpoint: str,
                 continue_index: int | None, test_number: int):
        self.suite_id = suite_id
        self.checkpoint = checkpoint
        self.continue_index = continue_index
        self.label = checkpoint_label(checkpoint, continue_index)
        self.test_number = test_number
        self.test_id = f"{suite_id}-{self.label.lower()}"
        self.process: subprocess.Popen | None = None
        self.client_process: subprocess.Popen | None = None

        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        self.log_path = LOG_DIR / f"{stamp}-{self.test_id}.jsonl"
        self.qemu_log_path = LOG_DIR / f"qemu-{self.test_id}.log"
        self.ssh_control_path = Path(
            f"/tmp/cxl-pf-ssh-{os.getuid()}-{self.test_id}.sock"
        )

    def log(self, event: str, **fields) -> None:
        record = {
            "time": timestamp(),
            "suite_id": self.suite_id,
            "test_id": self.test_id,
            "test": self.label,
            "event": event,
            **fields,
        }
        with self.log_path.open("a", encoding="utf-8") as file:
            file.write(json.dumps(record, sort_keys=True) + "\n")

        details = " ".join(f"{key}={value}" for key, value in fields.items())
        print(f"[{event}] {details}".rstrip(), flush=True)

    def qemu_command(self) -> list[str]:
        return [
            str(QEMU),
            "-machine", "q35,cxl=on,accel=kvm",
            "-cpu", "host",
            "-smp", "2",
            "-m", "4G,maxmem=8G,slots=8",
            "-drive", f"file={OVERLAY},format=qcow2,if=none,id=drive0",
            "-device", "virtio-blk-pci,drive=drive0,bus=pcie.0",
            "-netdev", "user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22",
            "-device", "virtio-net-pci,netdev=net0,bus=pcie.0",
            "-object", "memory-backend-ram,id=vmem0,share=on,size=256M",
            "-device", "pxb-cxl,bus_nr=12,bus=pcie.0,id=cxl.1",
            "-device",
            "cxl-rp,port=0,bus=cxl.1,id=root_port13,chassis=0,slot=2",
            "-device",
            "cxl-type3,bus=root_port13,volatile-memdev=vmem0,id=cxl-vmem0",
            "-M", "cxl-fmw.0.targets.0=cxl.1,cxl-fmw.0.size=4G",
            "-qmp", f"unix:{QMP_SOCKET},server=on,wait=off",
            "-display", "gtk",
        ]

    def guest_is_running(self) -> bool:
        return self.process is not None and self.process.poll() is None

    def start_qemu(self, phase: str) -> None:
        if self.guest_is_running():
            raise RuntimeError("Controller already has a live QEMU process.")

        check = subprocess.run(
            ["pgrep", "-af", "qemu-system-x86_64"],
            capture_output=True, text=True, check=False,
        )
        existing = [
            line for line in check.stdout.splitlines()
            if "pgrep -af" not in line and str(QEMU) in line
        ]
        if existing:
            raise RuntimeError(
                "Another QEMU process is already running:\n" + "\n".join(existing)
            )
        if not QEMU.is_file():
            raise FileNotFoundError(f"QEMU binary not found: {QEMU}")
        if not OVERLAY.is_file():
            raise FileNotFoundError(f"Overlay disk not found: {OVERLAY}")

        if QMP_SOCKET.exists():
            if QMP_SOCKET.is_socket():
                QMP_SOCKET.unlink()
            else:
                raise RuntimeError(f"QMP path is not a socket: {QMP_SOCKET}")

        with self.qemu_log_path.open("a", encoding="utf-8") as qemu_log:
            self.process = subprocess.Popen(
                self.qemu_command(),
                stdin=subprocess.DEVNULL,
                stdout=qemu_log,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )

        self.log("qemu_started", phase=phase, pid=self.process.pid,
                 overlay=str(OVERLAY), qemu_log=str(self.qemu_log_path))
        time.sleep(2)
        if self.process.poll() is not None:
            raise RuntimeError(
                f"QEMU exited with status {self.process.returncode}. "
                f"See {self.qemu_log_path}"
            )

    def wait_for_ssh_banner(self, phase: str) -> None:
        print(
            "\nQEMU is running. Select the intended custom kernel in GRUB "
            "if prompted, then wait for Ubuntu to boot.",
            flush=True,
        )
        print("Waiting for the Ubuntu SSH service. QEMU remains running.\n",
              flush=True)

        while self.guest_is_running():
            try:
                with socket.create_connection((SSH_HOST, SSH_PORT), timeout=3) as conn:
                    conn.settimeout(3)
                    banner = conn.recv(256)
                    if banner.startswith(b"SSH-"):
                        self.log(
                            "ssh_service_ready", phase=phase, port=SSH_PORT,
                            banner=banner.decode("ascii", errors="replace").strip(),
                        )
                        return
            except (OSError, TimeoutError):
                pass
            time.sleep(3)
        raise RuntimeError("QEMU exited before SSH became available.")

    def ssh_command(self, remote_command: str, interactive: bool = False) -> bool:
        options = [
            "-p", str(SSH_PORT),
            "-o", "ConnectTimeout=10",
            "-o", "ServerAliveInterval=15",
            "-o", f"ControlPath={self.ssh_control_path}",
            "-o", "ControlMaster=auto",
            "-o", "ControlPersist=600",
            "-o", "BatchMode=no" if interactive else "BatchMode=yes",
        ]
        try:
            result = subprocess.run(
                ["ssh", *(["-tt"] if interactive else []), *options,
                 GUEST, remote_command],
                timeout=190 if interactive else 15,
                check=False,
            )
            return result.returncode == 0
        except subprocess.TimeoutExpired:
            return False

    def authenticate_ssh(self, phase: str) -> None:
        print("\nSSH is ready. Authenticate to the Ubuntu guest.", flush=True)
        print("Enter the Ubuntu guest password when prompted.", flush=True)
        while self.guest_is_running():
            self.ssh_control_path.unlink(missing_ok=True)
            if self.ssh_command("true", interactive=True):
                self.log("ssh_authenticated", phase=phase)
                return
            if not self.guest_is_running():
                break
            print(
                "\nSSH login did not complete. QEMU is STILL running.\n"
                "Press Enter to retry, or Ctrl+C to stop.",
                flush=True,
            )
            try:
                input()
            except EOFError as exc:
                raise RuntimeError("Interactive SSH authentication requires a terminal.") from exc
        raise RuntimeError("QEMU exited during SSH authentication.")

    def wait_for_cxl_device(self, phase: str) -> None:
        print("Checking for /dev/cxl/mem0...", flush=True)
        deadline = time.monotonic() + DEVICE_TIMEOUT
        while time.monotonic() < deadline and self.guest_is_running():
            if self.ssh_command("test -e /dev/cxl/mem0"):
                self.log("guest_ready", phase=phase, cxl_device="/dev/cxl/mem0")
                return
            time.sleep(3)
        if not self.guest_is_running():
            raise RuntimeError("QEMU exited while checking the CXL device.")
        raise TimeoutError(
            f"/dev/cxl/mem0 not found after {DEVICE_TIMEOUT}s. "
            "QEMU will remain running for inspection."
        )

    def wait_for_guest(self, phase: str) -> None:
        self.wait_for_ssh_banner(phase)
        self.authenticate_ssh(phase)
        self.wait_for_cxl_device(phase)

    def guest_command(self) -> str:
        parts = [
            f"cd {GUEST_DIR} &&",
            "sudo ./build/cxl-fw-client transfer-fw",
            "--file firmware/synthetic-test-8k.bin",
            "--slot 2",
            "--payload-capacity 2048",
            "--execute",
            f"--process-fault {self.checkpoint}",
            "--controller-host 10.0.2.2",
            f"--controller-port {LISTEN_PORT}",
            f"--test-id {self.test_id}",
        ]
        if self.continue_index is not None:
            parts.append(f"--continue-index {self.continue_index}")
        return " ".join(parts)

    def start_guest_client(self) -> None:
        remote_command = self.guest_command()
        options = [
            "-tt", "-p", str(SSH_PORT),
            "-o", "ConnectTimeout=10",
            "-o", "ServerAliveInterval=15",
            "-o", f"ControlPath={self.ssh_control_path}",
            "-o", "ControlMaster=auto",
            "-o", "ControlPersist=600",
            "-o", "BatchMode=no",
        ]
        print(
            "\nStarting the firmware client inside Ubuntu over SSH.\n"
            "Enter the guest sudo password if prompted.\n"
            "Type TRANSFER at the client's confirmation prompt.\n",
            flush=True,
        )
        self.log("guest_client_starting", checkpoint=self.checkpoint,
                 continue_index=self.continue_index, command=remote_command)
        self.client_process = subprocess.Popen(
            ["ssh", *options, GUEST, remote_command],
            stdin=None, stdout=None, stderr=None,
        )
        self.log("guest_client_started", ssh_pid=self.client_process.pid)

    def wait_for_checkpoint(self, server: socket.socket) -> socket.socket:
        print(
            f"Waiting for checkpoint {self.label} from the guest client...\n",
            flush=True,
        )
        while self.guest_is_running():
            if self.client_process is not None:
                status = self.client_process.poll()
                if status is not None:
                    self.log("guest_client_exited_before_checkpoint",
                             exit_status=status)
                    raise RuntimeError(
                        "Guest firmware client exited before sending the "
                        "expected checkpoint. Check the guest output."
                    )
            try:
                connection, address = server.accept()
                break
            except socket.timeout:
                continue
        else:
            raise RuntimeError("QEMU exited before receiving a checkpoint.")

        connection.settimeout(10)
        data = bytearray()
        try:
            while b"\n" not in data and len(data) < 1024:
                chunk = connection.recv(256)
                if not chunk:
                    break
                data.extend(chunk)

            line = data.decode("ascii", errors="strict").strip()
            fields = line.split()
            if len(fields) != 7 or fields[0] != "CXL-PROCESS-FAULT/1":
                raise ValueError(f"Malformed checkpoint message: {line!r}")

            message = {
                "test_id": fields[1],
                "checkpoint": fields[2],
                "action": fields[3],
                "part": fields[4],
                "offset": fields[5],
                "completed": fields[6],
            }
            if message["test_id"] != self.test_id:
                raise ValueError(
                    f"Test ID mismatch: expected {self.test_id}, "
                    f"received {message['test_id']}"
                )
            if message["checkpoint"] != self.checkpoint:
                raise ValueError(
                    f"Checkpoint mismatch: expected {self.checkpoint}, "
                    f"received {message['checkpoint']}"
                )
            if self.continue_index is not None and message["part"] != str(
                self.continue_index
            ):
                raise ValueError(
                    f"CONTINUE index mismatch: expected {self.continue_index}, "
                    f"received {message['part']}"
                )

            self.log("checkpoint_received", peer=str(address), **message)
            return connection
        except Exception:
            connection.close()
            raise

    def reap_guest_client_after_fault(self) -> None:
        if self.client_process is None:
            return
        try:
            status = self.client_process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            self.client_process.terminate()
            try:
                status = self.client_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.client_process.kill()
                status = self.client_process.wait()
        self.log("guest_client_disconnected_after_qemu_kill",
                 exit_status=status, expected=True)
        self.client_process = None

    def close_ssh_master(self) -> None:
        try:
            subprocess.run(
                ["ssh", "-o", f"ControlPath={self.ssh_control_path}",
                 "-O", "exit", GUEST],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                timeout=5, check=False,
            )
        except subprocess.TimeoutExpired:
            pass
        self.ssh_control_path.unlink(missing_ok=True)

    def kill_qemu(self) -> None:
        if not self.guest_is_running():
            raise RuntimeError("QEMU is not running at the requested checkpoint.")
        pid = self.process.pid
        os.kill(pid, signal.SIGKILL)
        self.process.wait(timeout=15)
        self.log("qemu_sigkill", pid=pid, exit_status=self.process.returncode,
                 expected=True)
        self.process = None
        if QMP_SOCKET.exists() and QMP_SOCKET.is_socket():
            QMP_SOCKET.unlink()

    def stop_qemu(self, reason: str = "controller_cleanup") -> None:
        if self.client_process is not None and self.client_process.poll() is None:
            self.client_process.terminate()
            try:
                self.client_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.client_process.kill()
                self.client_process.wait()
            self.log("guest_client_stopped_by_controller")
        self.client_process = None
        self.close_ssh_master()

        if self.guest_is_running():
            pid = self.process.pid
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
            self.log("qemu_stopped_by_controller", pid=pid, reason=reason)
        self.process = None
        if QMP_SOCKET.exists() and QMP_SOCKET.is_socket():
            QMP_SOCKET.unlink()

    def execute(self, server: socket.socket) -> None:
        self.log(
            "controller_started",
            test_number=self.test_number,
            checkpoint=self.checkpoint,
            continue_index=self.continue_index,
            qemu=str(QEMU),
            overlay=str(OVERLAY),
            qmp_socket=str(QMP_SOCKET),
            log_path=str(self.log_path),
        )
        self.start_qemu("initial")
        self.wait_for_guest("initial")
        self.start_guest_client()
        connection = self.wait_for_checkpoint(server)
        try:
            self.kill_qemu()
        finally:
            connection.close()

        self.reap_guest_client_after_fault()
        self.close_ssh_master()
        self.start_qemu("after_fault")
        self.wait_for_guest("after_fault")
        self.log(
            "test_complete",
            checkpoint=self.checkpoint,
            continue_index=self.continue_index,
            result="QEMU restarted; SSH and /dev/cxl/mem0 verified",
        )
        print(f"\n{self.label} complete. Log: {self.log_path}", flush=True)


def create_listener() -> socket.socket:
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((LISTEN_HOST, LISTEN_PORT))
    server.listen(1)
    server.settimeout(1)
    return server


def parse_args():
    parser = argparse.ArgumentParser(
        description="Run QEMU CXL process-level fault-injection tests on the Arch host."
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--all", action="store_true",
                      help="Run PF1, PF2, PF3-1, PF3-2 and PF4 in sequence.")
    mode.add_argument("--checkpoint", choices=["PF1", "PF2", "PF3", "PF4"],
                      help="Run one checkpoint only.")
    parser.add_argument("--continue-index", type=int, choices=[1, 2],
                        help="Required with --checkpoint PF3.")
    parser.add_argument("--suite-id",
                        help="Optional suite ID; generated automatically if omitted.")
    parser.add_argument("--test-id",
                        help="Optional test ID for single-checkpoint mode.")
    args = parser.parse_args()

    if args.checkpoint == "PF3" and args.continue_index is None:
        parser.error("--continue-index is required for PF3 (1 or 2).")
    if args.checkpoint != "PF3" and args.continue_index is not None:
        parser.error("--continue-index is only valid with PF3.")
    if args.all and args.test_id:
        parser.error("--test-id can only be used with --checkpoint.")
    return args


def main() -> int:
    args = parse_args()
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    suite_id = args.suite_id or (
        datetime.now().strftime("%Y%m%d-%H%M%S") + "-" + uuid.uuid4().hex[:6]
    )

    tests = CHECKPOINTS if args.all else [(args.checkpoint, args.continue_index)]
    print(f"Process-fault suite: {suite_id}", flush=True)
    print("Tests: " + ", ".join(checkpoint_label(*item) for item in tests), flush=True)

    try:
        server = create_listener()
    except OSError as exc:
        print(f"ERROR: Cannot listen on {LISTEN_HOST}:{LISTEN_PORT}: {exc}",
              file=sys.stderr)
        return 1

    failures = []
    try:
        for number, (checkpoint, continue_index) in enumerate(tests, start=1):
            label = checkpoint_label(checkpoint, continue_index)
            test_id = args.test_id if not args.all and args.test_id else None
            controller = Controller(suite_id, checkpoint, continue_index, number)
            if test_id:
                controller.test_id = test_id
                stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
                controller.log_path = LOG_DIR / f"{stamp}-{test_id}.jsonl"
                controller.qemu_log_path = LOG_DIR / f"qemu-{test_id}.log"
                controller.ssh_control_path = Path(
                    f"/tmp/cxl-pf-ssh-{os.getuid()}-{test_id}.sock"
                )

            print("\n" + "=" * 68, flush=True)
            print(f"PROCESS-LEVEL TEST {number}/{len(tests)}: {label}", flush=True)
            print("=" * 68, flush=True)

            try:
                controller.execute(server)
                if args.all:
                    controller.stop_qemu(reason="between_batch_tests")
                else:
                    print(
                        "\nSingle test completed. QEMU remains running. "
                        "Press Ctrl+C to shut it down.",
                        flush=True,
                    )
                    while controller.guest_is_running():
                        time.sleep(1)
                    controller.log("qemu_exited_outside_controller")
                    controller.process = None
            except KeyboardInterrupt:
                controller.log("controller_interrupted")
                controller.stop_qemu(reason="keyboard_interrupt")
                return 130
            except Exception as exc:
                controller.log(
                    "controller_error",
                    error=str(exc),
                    qemu_pid=(controller.process.pid
                              if controller.guest_is_running() else None),
                    qemu_left_running=controller.guest_is_running(),
                )
                print(f"\nERROR in {label}: {exc}", file=sys.stderr, flush=True)
                failures.append(label)
                # A batch must shut down a failed VM before starting the next test.
                if args.all:
                    controller.stop_qemu(reason="failed_batch_test_cleanup")
                else:
                    if controller.guest_is_running():
                        print(
                            f"QEMU remains running for inspection (PID "
                            f"{controller.process.pid}). Stop it manually before "
                            "another run.",
                            flush=True,
                        )
                    return 1

        print("\n" + "=" * 68, flush=True)
        print("PROCESS-LEVEL SUITE FINISHED", flush=True)
        print(f"Suite ID: {suite_id}", flush=True)
        print(f"Completed tests: {len(tests) - len(failures)}/{len(tests)}", flush=True)
        print("Failed/incomplete controller runs: "
              + (", ".join(failures) if failures else "None"), flush=True)
        print(f"Logs: {LOG_DIR}", flush=True)
        print(
            "\nGenerate the report on the Arch host with:\n"
            f"  python3 {ROOT / 'generate_process_fault_report.py'} "
            f"--suite-id {suite_id}",
            flush=True,
        )
        return 1 if failures else 0
    finally:
        server.close()


if __name__ == "__main__":
    raise SystemExit(main())
