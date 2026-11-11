
import pytest

from tests.regression.helpers import (
    inject_fault,
    get_firmware_info,
    run_clean_update,
)

FAULT_CASES = [
    "invalid-action",
    "invalid-slot",
    "out-of-range-offset",
    "bit-flip",
    "truncated-payload",
    "continue-without-initiate",
    "end-without-initiate",
    "duplicate-offset",
    "out-of-order-offset",
]

@pytest.mark.full
@pytest.mark.parametrize("fault_name", FAULT_CASES)
def test_fault_injection_and_recovery(fault_name):
    """Test a fault, check device health, then attempt a clean update."""

    # 1. Inject the selected fault.
    fault_result = inject_fault(fault_name)
    print(f"\n[{fault_name}] Fault injection: {fault_result['elapsed']:.2f}s")

    assert not fault_result["timed_out"], (
        f"{fault_name}: fault injection timed out"
    )

    assert fault_result["returncode"] == 0, (
        f"{fault_name}: fault injection failed\n"
        f"{fault_result['stdout']}\n{fault_result['stderr']}"
    )

    assert "PASS:" in fault_result["stdout"], (
        f"{fault_name}: expected mailbox response was not confirmed"
    )

    # 2. Check whether the device responds to Get FW Info.
    info_result = get_firmware_info()
    print(f"[{fault_name}] Get FW Info: {info_result['elapsed']:.2f}s")
    assert not info_result["timed_out"], (
        f"{fault_name}: Get FW Info timed out"
    )

    assert info_result["returncode"] == 0, (
        f"{fault_name}: Get FW Info failed\n"
        f"{info_result['stdout']}\n{info_result['stderr']}"
    )

    # 3. Attempt a subsequent clean firmware update.
    update_result = run_clean_update()
    print(f"[{fault_name}] Clean update: {update_result['elapsed']:.2f}s")
    assert not update_result["timed_out"], (
        f"{fault_name}: clean firmware update timed out"
    )

    assert update_result["returncode"] == 0, (
        f"{fault_name}: clean firmware update failed\n"
        f"{update_result['stdout']}\n{update_result['stderr']}"
    )

    assert (
    "ioctl succeeded. Mailbox return value: 0" in update_result["stdout"]
    and "TRANSFER_FW completed synchronously." in update_result["stdout"]
), f"{fault_name}: clean update completion was not confirmed"
