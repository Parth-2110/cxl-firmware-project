
#!/usr/bin/env python3
"""QEMU CXL process-fault controller. Run on the Arch host."""

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

QEMU = Path.home() / "Projects/qemu/qemu/build/qemu-system-x86_64"
OVERLAY = Path.home() / "Projects/qemu/process-fault-test.qcow2"
LOG_DIR = Path.home() / "Projects/qemu/process-fault-logs"
QMP_SOCKET = Path("/tmp/qemu-cxl-qmp.sock")
LISTEN_HOST = "0.0.0.0"
LISTEN_PORT = 39091
SSH_HOST = "127.0.0.1"
SSH_PORT = 2222
DEVICE_TIMEOUT = 120


def timestamp():
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


class Controller:
    def __init__(self, args):
        self.args = args
        self.test_id = args.test_id or uuid.uuid4().hex[:10]
        self.process = None
        self.client_process = None

        stamp = timestamp().replace(":", "-")
        self.log_path = LOG_DIR / f"{stamp}-{self.test_id}.jsonl"
        self.qemu_log_path = LOG_DIR / f"qemu-{self.test_id}.log"
        self.ssh_control_path = Path(
            f"/tmp/cxl-pf-ssh-{os.getuid()}-{self.test_id}.sock"
        )

    def log(self, event, **fields):
        record = {
            "time": timestamp(),
            "test_id": self.test_id,
            "event": event,
            **fields,
        }

        with self.log_path.open("a", encoding="utf-8") as f:
            f.write(json.dumps(record, sort_keys=True) + "\n")

        print(
            f"[{event}] "
            + " ".join(f"{k}={v}" for k, v in fields.items()),
            flush=True,
        )

    def qemu_command(self):
        return [
            str(QEMU),
            "-machine", "q35,cxl=on,accel=kvm",
            "-cpu", "host",
            "-smp", "2",
            "-m", "4G,maxmem=8G,slots=8",
            "-drive",
            f"file={OVERLAY},format=qcow2,if=none,id=drive0",
            "-device", "virtio-blk-pci,drive=drive0,bus=pcie.0",
            "-netdev",
            "user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22",
            "-device", "virtio-net-pci,netdev=net0,bus=pcie.0",
            "-object",
            "memory-backend-ram,id=vmem0,share=on,size=256M",
            "-device", "pxb-cxl,bus_nr=12,bus=pcie.0,id=cxl.1",
            "-device",
            "cxl-rp,port=0,bus=cxl.1,id=root_port13,chassis=0,slot=2",
            "-device",
            "cxl-type3,bus=root_port13,volatile-memdev=vmem0,id=cxl-vmem0",
            "-M", "cxl-fmw.0.targets.0=cxl.1,cxl-fmw.0.size=4G",
            "-qmp", f"unix:{QMP_SOCKET},server=on,wait=off",
            "-display", "gtk",
        ]

    def guest_is_running(self):
        return self.process is not None and self.process.poll() is None

    def start_qemu(self, phase):
        if self.guest_is_running():
            raise RuntimeError("Controller already has a live QEMU process.")

        check = subprocess.run(
            ["pgrep", "-af", "qemu-system-x86_64"],
            capture_output=True,
            text=True,
            check=False,
        )

        existing = [
            line for line in check.stdout.splitlines()
            if "pgrep -af" not in line and str(QEMU) in line
        ]

        if existing:
            raise RuntimeError(
                "Another QEMU process is already running:\n"
                + "\n".join(existing)
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

        self.log(
            "qemu_started",
            phase=phase,
            pid=self.process.pid,
            overlay=str(OVERLAY),
        )

        time.sleep(2)

        if self.process.poll() is not None:
            raise RuntimeError(
                f"QEMU exited with status {self.process.returncode}. "
                f"See {self.qemu_log_path}"
            )

    def wait_for_ssh_banner(self, phase):
        print(
            "\nQEMU is running. Select the intended kernel in GRUB "
            "if prompted, then wait for Ubuntu to boot.",
            flush=True,
        )
        print(
            "Waiting for the actual Ubuntu SSH service. "
            "QEMU will remain running during this wait.\n",
            flush=True,
        )

        while self.guest_is_running():
            try:
                with socket.create_connection(
                    (SSH_HOST, SSH_PORT), timeout=3
                ) as connection:
                    connection.settimeout(3)
                    banner = connection.recv(256)

                    if banner.startswith(b"SSH-"):
                        self.log(
                            "ssh_service_ready",
                            phase=phase,
                            port=SSH_PORT,
                            banner=banner.decode(
                                "ascii", errors="replace"
                            ).strip(),
                        )
                        return

            except (OSError, TimeoutError):
                pass

            time.sleep(3)

        raise RuntimeError("QEMU exited before SSH became available.")

    def ssh_command(self, remote_command, interactive=False):
        options = [
            "-p", str(SSH_PORT),
            "-o", "ConnectTimeout=10",
            "-o", "ServerAliveInterval=15",
            "-o", f"ControlPath={self.ssh_control_path}",
            "-o", "ControlMaster=auto",
            "-o", "ControlPersist=600",
        ]

        options += [
            "-o",
            "BatchMode=no" if interactive else "BatchMode=yes"
        ]

        try:
            ssh_flags = ["-tt"] if interactive else []
            result = subprocess.run(
                [
                    "ssh",
                    *ssh_flags,
                    *options,
                    "ubuntu-user@127.0.0.1",
                    remote_command,
                ],
                timeout=190 if interactive else 15,
                check=False,
            )
            return result.returncode == 0

        except subprocess.TimeoutExpired:
            return False

    def authenticate_ssh(self, phase):
        print(
            "\nSSH server is ready. Authenticate to Ubuntu.",
            flush=True,
        )
        print(
            "Enter your Ubuntu guest password when prompted.",
            flush=True,
        )

        while self.guest_is_running():
            self.ssh_control_path.unlink(missing_ok=True)

            if self.ssh_command("true", interactive=True):
                self.log("ssh_authenticated", phase=phase)
                return

            if not self.guest_is_running():
                break

            print(
                "\nSSH login did not complete. QEMU is STILL running.\n"
                "Press Enter to retry, or Ctrl+C to stop the controller.",
                flush=True,
            )

            try:
                input()
            except EOFError:
                raise RuntimeError(
                    "Interactive SSH authentication requires a terminal."
                )

        raise RuntimeError("QEMU exited during SSH authentication.")

    def wait_for_cxl_device(self, phase):
        print("Checking for /dev/cxl/mem0...", flush=True)

        deadline = time.monotonic() + DEVICE_TIMEOUT

        while time.monotonic() < deadline and self.guest_is_running():
            if self.ssh_command(
                "test -e /dev/cxl/mem0",
                interactive=False,
            ):
                self.log(
                    "guest_ready",
                    phase=phase,
                    cxl_device="/dev/cxl/mem0",
                )
                return

            time.sleep(3)

        if not self.guest_is_running():
            raise RuntimeError("QEMU exited while checking the CXL device.")

        raise TimeoutError(
            f"/dev/cxl/mem0 not found after {DEVICE_TIMEOUT}s. "
            "QEMU will remain running for inspection."
        )

    def wait_for_guest(self, phase):
        self.wait_for_ssh_banner(phase)
        self.authenticate_ssh(phase)
        self.wait_for_cxl_device(phase)

    def guest_command(self):
        parts = [
            "cd ~/fw-management &&",
            "sudo ./build/cxl-fw-client transfer-fw",
            "--file firmware/synthetic-test-8k.bin",
            "--slot 2",
            "--payload-capacity 2048",
            "--execute",
            f"--process-fault {self.args.checkpoint}",
            "--controller-host 10.0.2.2",
            f"--controller-port {LISTEN_PORT}",
            f"--test-id {self.test_id}",
        ]

        if self.args.checkpoint == "PF3":
            parts.append(
                f"--continue-index {self.args.continue_index}"
            )

        return " ".join(parts)

    def start_guest_client(self):
        """Run the firmware client over an interactive SSH session."""
        remote_command = self.guest_command()
        ssh_options = [
            "-tt",
            "-p", str(SSH_PORT),
            "-o", "ConnectTimeout=10",
            "-o", "ServerAliveInterval=15",
            "-o", f"ControlPath={self.ssh_control_path}",
            "-o", "ControlMaster=auto",
            "-o", "ControlPersist=600",
            "-o", "BatchMode=no",
        ]

        print(
            "\nStarting the firmware client inside Ubuntu over SSH.\n"
            "If prompted, enter the Ubuntu sudo password.\n"
            "Type TRANSFER at the client's confirmation prompt to begin.\n",
            flush=True,
        )

        self.log(
            "guest_client_starting",
            checkpoint=self.args.checkpoint,
            command=remote_command,
        )

        self.client_process = subprocess.Popen(
            [
                "ssh",
                *ssh_options,
                "ubuntu-user@127.0.0.1",
                remote_command,
            ],
            stdin=None,
            stdout=None,
            stderr=None,
        )

        self.log(
            "guest_client_started",
            ssh_pid=self.client_process.pid,
        )

    def wait_for_checkpoint(self, server):
        print(
            "Waiting for the guest client to reach the configured "
            "process-fault checkpoint...\n",
            flush=True,
        )

        while self.guest_is_running():
            if self.client_process is not None:
                client_status = self.client_process.poll()
                if client_status is not None:
                    self.log(
                        "guest_client_exited_before_checkpoint",
                        exit_status=client_status,
                    )
                    raise RuntimeError(
                        "Guest firmware client exited before sending "
                        "the expected checkpoint. Check its output above."
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
                raise ValueError(f"Malformed checkpoint: {line!r}")

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

            if message["checkpoint"] != self.args.checkpoint:
                raise ValueError(
                    f"Checkpoint mismatch: expected {self.args.checkpoint}, "
                    f"received {message['checkpoint']}"
                )

            self.log(
                "checkpoint_received",
                peer=str(address),
                **message,
            )

            return connection

        except Exception:
            connection.close()
            raise

    def reap_guest_client_after_fault(self):
        """Record the expected SSH disconnect caused by QEMU SIGKILL."""
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

        self.log(
            "guest_client_disconnected_after_qemu_kill",
            exit_status=status,
            expected=True,
        )
        self.client_process = None

    def close_ssh_master(self):
        try:
            subprocess.run(
                [
                    "ssh",
                    "-o", f"ControlPath={self.ssh_control_path}",
                    "-O", "exit",
                    "ubuntu-user@127.0.0.1",
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=5,
                check=False,
            )
        except subprocess.TimeoutExpired:
            pass

        self.ssh_control_path.unlink(missing_ok=True)

    def kill_qemu(self):
        if not self.guest_is_running():
            raise RuntimeError("QEMU is not running.")

        pid = self.process.pid

        os.kill(pid, signal.SIGKILL)
        self.process.wait(timeout=15)

        self.log(
            "qemu_sigkill",
            pid=pid,
            exit_status=self.process.returncode,
        )

        self.process = None

        if QMP_SOCKET.exists() and QMP_SOCKET.is_socket():
            QMP_SOCKET.unlink()

    def stop_qemu(self):
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

            self.log("qemu_stopped_by_controller", pid=pid)

        self.process = None

        if QMP_SOCKET.exists() and QMP_SOCKET.is_socket():
            QMP_SOCKET.unlink()

    def run(self):
        LOG_DIR.mkdir(parents=True, exist_ok=True)

        self.log(
            "controller_started",
            checkpoint=self.args.checkpoint,
            continue_index=self.args.continue_index,
            qemu=str(QEMU),
            overlay=str(OVERLAY),
            qmp_socket=str(QMP_SOCKET),
        )

        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((LISTEN_HOST, LISTEN_PORT))
        server.listen(1)
        server.settimeout(1)

        try:
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
                checkpoint=self.args.checkpoint,
                result="QEMU restarted; SSH and CXL device verified",
            )

            print(
                f"\nTest complete. Log: {self.log_path}\n"
                "QEMU remains running. Press Ctrl+C to shut it down.",
                flush=True,
            )

            while self.guest_is_running():
                time.sleep(1)

            self.log("qemu_exited_outside_controller")
            self.process = None
            return 0

        except KeyboardInterrupt:
            self.log("controller_interrupted")
            self.stop_qemu()
            return 130

        except Exception as exc:
            self.log(
                "controller_error",
                error=str(exc),
                qemu_pid=(
                    self.process.pid if self.guest_is_running() else None
                ),
                qemu_left_running=self.guest_is_running(),
            )

            print(f"\nERROR: {exc}", file=sys.stderr, flush=True)

            if self.guest_is_running():
                print(
                    f"QEMU remains running. PID: {self.process.pid}\n"
                    "Inspect the VM or stop it manually when ready.",
                    flush=True,
                )

            return 1

        finally:
            server.close()


def parse_args():
    parser = argparse.ArgumentParser(
        description="QEMU CXL firmware-transfer process-fault controller."
    )

    parser.add_argument(
        "--checkpoint",
        required=True,
        choices=["PF1", "PF2", "PF3", "PF4"],
    )
    parser.add_argument("--continue-index", type=int, choices=[1, 2])
    parser.add_argument("--test-id")

    args = parser.parse_args()

    if args.checkpoint == "PF3" and args.continue_index is None:
        parser.error("--continue-index is required for PF3 (1 or 2).")

    if args.checkpoint != "PF3" and args.continue_index is not None:
        parser.error("--continue-index is only valid with PF3.")

    return args


if __name__ == "__main__":
    raise SystemExit(Controller(parse_args()).run())