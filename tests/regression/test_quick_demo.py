
import pytest

from tests.regression.helpers import inject_fault


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

@pytest.mark.quick
@pytest.mark.parametrize("fault_name", FAULT_CASES)
def test_quick_fault_demo(fault_name):
    """Quickly verify the expected response for each injected fault."""

    result = inject_fault(fault_name)

    print(f"\n[{fault_name}] Execution time: {result['elapsed']:.2f}s")

    assert not result["timed_out"], (
        f"{fault_name}: fault injection timed out"
    )

    assert result["returncode"] == 0, (
        f"{fault_name}: fault injection failed\n"
        f"{result['stdout']}\n{result['stderr']}"
    )

    assert "PASS:" in result["stdout"], (
        f"{fault_name}: expected mailbox response was not confirmed"
    )
