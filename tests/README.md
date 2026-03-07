# eCTF 2026 HSM Testing Suite

A comprehensive testing suite crafted to securely test and validate hardware
firmware compliance with the **eCTF 2026 Rulebook and Technical Specifications**.

## Prerequisites

This suite is fully packaged with `uv` and `pyproject.toml` configurations.
Ensure your environment supports Python >= 3.12 and has the `ectf` host tools
package available.

## Usage

### 1. Generating Test Scenarios

Before running the tests, you must generate a randomized group map and security
identifiers (PINs).
Running this configuration step will randomly populate `groups.txt`,
`perms_hsmA.txt`, `perms_hsmB.txt`, `pin_hsmA.txt` and `pin_hsmB.txt`.

```bash
uv run scenario
```

### 2. Running The Suite

Once the scenario files are actively loaded from step 1, pass the active serial
ports of your hardware devices to the `tests` hook. The suite dynamically
resolves the pins corresponding to your targeted HSMs.

```bash
uv run tests --port-a /dev/ttyACM0 --port-b /dev/ttyACM1
```

By default, the test suite is configured with `pytest.ini` to universally spit
out natively colored terminal output for success/traceback reports.
