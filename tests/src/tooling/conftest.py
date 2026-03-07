# pyright: reportMissingTypeStubs=false, reportAny=false, reportPrivateUsage=false
import time
from collections.abc import Generator

import pytest
from ectf.tools.hsm_interface import HSMIntf


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--port-a",
        action="store",
        default="/dev/ttyACM0",
        help="Serial port for HSM A (or Engineer/Attacker)",
    )
    parser.addoption(
        "--port-b",
        action="store",
        default="/dev/ttyACM2",
        help="Serial port for HSM B (or Litho)",
    )
    parser.addoption(
        "--pin-a",
        action="store",
        default=None,
        help="PIN for HSM A (or Engineer/Attacker)",
    )
    parser.addoption(
        "--pin-b", action="store", default=None, help="PIN for HSM B (or Litho)"
    )


@pytest.fixture(scope="session")
def hsm_a_port(request: pytest.FixtureRequest) -> str:
    return request.config.getoption("--port-a")


@pytest.fixture(scope="session")
def hsm_b_port(request: pytest.FixtureRequest) -> str:
    return request.config.getoption("--port-b")


@pytest.fixture(scope="session")
def pin_a(request: pytest.FixtureRequest) -> str:
    val = request.config.getoption("--pin-a")
    if val is None:
        try:
            with open("pin_hsmA.txt", "r") as f:
                val = f.read().strip()
        except FileNotFoundError:
            val = "123456"
    return val


@pytest.fixture(scope="session")
def pin_b(request: pytest.FixtureRequest) -> str:
    val = request.config.getoption("--pin-b")
    if val is None:
        try:
            with open("pin_hsmB.txt", "r") as f:
                val = f.read().strip()
        except FileNotFoundError:
            val = "123456"
    return val


@pytest.fixture(scope="module")
def hsm_a(hsm_a_port: str) -> Generator[HSMIntf, None, None]:
    """Fixture to provide a connected HSMIntf for device A."""
    hsm = HSMIntf.from_port(hsm_a_port, timeout=5.0)  # pyright: ignore[reportArgumentType]
    hsm._open()
    yield hsm


@pytest.fixture(scope="module")
def hsm_b(hsm_b_port: str) -> Generator[HSMIntf, None, None]:
    """Fixture to provide a connected HSMIntf for device B."""
    hsm = HSMIntf.from_port(hsm_b_port, timeout=5.0)  # pyright: ignore[reportArgumentType]
    hsm._open()
    yield hsm


@pytest.fixture(scope="session")
def group_ids() -> list[int]:
    """Load generated group IDs from group_ids.txt as integers.

    Index mapping to HSM A permissions (from generate_groups.py):
      [0]=---  [1]=R--  [2]=-W-  [3]=RW-
      [4]=--C  [5]=R-C  [6]=-WC  [7]=RWC
    """
    with open("group_ids.txt") as f:
        return [int(line.strip(), 16) for line in f if line.strip()]


def pytest_collection_modifyitems(items: list[pytest.Item]) -> None:
    """Sort the tests so they run in the specified order."""
    order = {
        "test_run_tests.py": 1,
        "test_run_scenario.py": 2,
        "test_edge_cases.py": 3,
        "test_timing.py": 4,
    }

    def get_order(item: pytest.Item) -> int:
        for test_file, val in order.items():
            if test_file in item.nodeid:
                return val
        return 99

    items.sort(key=get_order)
