
import pytest

from tests.regression.helpers import (
    FAULT_INJECT,
    GET_FW_INFO,
    TRANSFER_FW,
    FIRMWARE_IMAGE,
)


@pytest.fixture(scope="session", autouse=True)
def verify_test_environment():
    """Verify required programs and firmware image exist before testing."""

    required_paths = [
        FAULT_INJECT,
        GET_FW_INFO,
        TRANSFER_FW,
        FIRMWARE_IMAGE,
    ]

    missing = [str(path) for path in required_paths if not path.is_file()]

    if missing:
        pytest.exit(
            "Required test files are missing:\n" + "\n".join(missing),
            returncode=2,
        )

    for executable in required_paths[:3]:
        if not executable.stat().st_mode & 0o111:
            pytest.exit(
                f"Program is not executable: {executable}",
                returncode=2,
            )

