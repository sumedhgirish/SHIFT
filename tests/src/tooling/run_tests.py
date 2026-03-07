import sys
from pathlib import Path

import pytest


def run() -> None:
    """
    Entry point for the 'tests' command.
    Automatically locates the installed test directory and runs pytest.
    """
    # Find the directory containing this script (which also contains conftest.py)
    test_dir = Path(__file__).parent.resolve()

    # Pass all command line arguments to pytest, appending our test directory
    # so pytest knows where to look for the tests and conftest.py
    args = sys.argv[1:] + [str(test_dir)]

    sys.exit(pytest.main(args))


if __name__ == "__main__":
    run()
