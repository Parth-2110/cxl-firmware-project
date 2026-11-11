
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"

FAULT_INJECT = BUILD / "fault_inject"
GET_FW_INFO = BUILD / "get_fw_info"
TRANSFER_FW = BUILD / "transfer_fw"

FIRMWARE_IMAGE = ROOT / "tests/firmware-images/test_fw.bin"


def run_command(command, timeout=90, input_text=None):
    """Run a command, capture output, and measure execution time."""
    import time

    start_time = time.perf_counter()

    try:
        result = subprocess.run(
            [str(arg) for arg in command],
            cwd=ROOT,
            input=input_text,
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
        )

        elapsed = time.perf_counter() - start_time

        return {
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "timed_out": False,
            "elapsed": elapsed,
        }

    except subprocess.TimeoutExpired as error:
        elapsed = time.perf_counter() - start_time

        return {
            "returncode": None,
            "stdout": error.stdout or "",
            "stderr": error.stderr or "",
            "timed_out": True,
            "elapsed": elapsed,
        }

def inject_fault(fault_name):
    """Submit one fault and return its result."""
    return run_command(
        [FAULT_INJECT, "--fault", fault_name, "--submit"],
        timeout=90,
    )


def get_firmware_info():
    """Query the device's firmware information."""
    return run_command([GET_FW_INFO], timeout=30)


def run_clean_update():
    """Run normal firmware transfer using the existing menu."""
    return run_command(
        [TRANSFER_FW, "--submit", FIRMWARE_IMAGE],
        timeout=120,
        input_text="1\n",
    )
