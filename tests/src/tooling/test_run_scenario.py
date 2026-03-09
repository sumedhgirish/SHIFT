# pyright: reportMissingTypeStubs=false
import os
import struct
import tempfile
import threading

import pytest
from ectf.tools.hsm_interface import HSMIntf


def setup_write_frame(
    pin: str, slot: int, group: int, name_str: str, file_data: bytes
) -> bytes:
    pin_bytes = pin.encode()[:6].ljust(6, b"\x00")
    name_bytes = name_str.encode()[:31].ljust(32, b"\x00")
    uuid_bytes = os.urandom(16)
    return struct.pack(
        f"<6s B H 32s 16s H {len(file_data)}s",
        pin_bytes,
        slot,
        group,
        name_bytes,
        uuid_bytes,
        len(file_data),
        file_data,
    )


def setup_read_frame(pin: str, slot: int) -> bytes:
    pin_bytes = pin.encode()[:6].ljust(6, b"\x00")
    return struct.pack("<6s B", pin_bytes, slot)


def setup_receive_frame(pin: str, read_slot: int, write_slot: int) -> bytes:
    pin_bytes = pin.encode()[:6].ljust(6, b"\x00")
    return struct.pack("<6s B B", pin_bytes, read_slot, write_slot)


@pytest.fixture
def temp_dir():
    with tempfile.TemporaryDirectory() as td:
        yield td


class TestRunScenarioTests:
    # Tests matching run_scenario_tests in hands-off flow description

    def test_engineer_litho_swap(
        self,
        hsm_a: HSMIntf,
        hsm_b: HSMIntf,
        pin_a: str,
        pin_b: str,
        temp_dir: str,
        group_ids: list[int],
    ) -> None:
        """Simulates engineer (A) and litho (B) swap."""
        # Setup files via temp dir for integrity
        design_file_1 = os.urandom(64)
        design_file_2 = os.urandom(64)

        with open(os.path.join(temp_dir, "design_file_1.bin"), "wb") as f:
            _ = f.write(design_file_1)
        with open(os.path.join(temp_dir, "design_file_2.bin"), "wb") as f:
            _ = f.write(design_file_2)

        hsm_a.write_file(
            setup_write_frame(pin_a, 1, group_ids[7], "design1", design_file_1)
        )
        hsm_b.write_file(
            setup_write_frame(pin_b, 1, group_ids[7], "design2", design_file_2)
        )

        # Litho interrogates & receives design_file1 from engineer
        hsm_a.listen()
        _ = hsm_b.interrogate(pin_b)

        hsm_a.listen()
        recv_f1 = setup_receive_frame(pin_b, 1, 1)  # read slot 1, write slot 1
        _ = hsm_b.receive(recv_f1)

        # Verify receipt by reading from Litho
        read_f1 = setup_read_frame(pin_b, 1)
        assert hsm_b.read_file(read_f1)[32:] == design_file_1

        # Engineer interrogates & receives design_file2 from litho
        hsm_b.listen()
        _ = hsm_a.interrogate(pin_a)

        hsm_b.listen()
        recv_f2 = setup_receive_frame(pin_a, 1, 1)  # read slot 1, write slot 1
        _ = hsm_a.receive(recv_f2)

        read_f2 = setup_read_frame(pin_a, 1)
        assert hsm_a.read_file(read_f2)[32:] == design_file_2

    def test_create_file_engineer(
        self,
        hsm_a: HSMIntf,
        hsm_b: HSMIntf,
        pin_a: str,
        pin_b: str,
        temp_dir: str,
        group_ids: list[int],
    ) -> None:
        # Engineer writes design file to slot 2
        with open(os.path.join(temp_dir, "design2.bin"), "wb") as f:
            _ = f.write(b"Design File Content")

        with open(os.path.join(temp_dir, "design2.bin"), "rb") as f:
            data = f.read()

        # groups[3]: A=RW- (write+read), B=RWC (receive+read)
        hsm_a.write_file(setup_write_frame(pin_a, 2, group_ids[3], "design2", data))
        files = hsm_a.list(pin_a)
        assert len(files) > 0  # At least slot 2

        read_f = setup_read_frame(pin_a, 2)
        assert hsm_a.read_file(read_f)[32:] == data

        hsm_a.listen()
        _ = hsm_b.interrogate(pin_b)

        hsm_a.listen()
        recv_f = setup_receive_frame(pin_b, 2, 2)
        _ = hsm_b.receive(recv_f)

        read_fb = setup_read_frame(pin_b, 2)
        assert hsm_b.read_file(read_fb)[32:] == data

    def test_attacker_litho_swaps(
        self,
        hsm_a: HSMIntf,
        hsm_b: HSMIntf,
        pin_a: str,
        pin_b: str,
        temp_dir: str,
        group_ids: list[int],
    ) -> None:
        """Simulate Attacker (A) interacting with Litho (B) for Calibration & Telemetry"""
        calibration = b"Calibrating System"
        telemetry = b"Telemetry Data"

        with open(os.path.join(temp_dir, "calibration.bin"), "wb") as f:
            _ = f.write(calibration)
        with open(os.path.join(temp_dir, "telemetry.bin"), "wb") as f:
            _ = f.write(telemetry)

        # Attacker writes calibration to slot 1
        hsm_a.write_file(
            setup_write_frame(pin_a, 1, group_ids[7], "calibration", calibration)
        )

        # Litho gets calibration from attacker
        hsm_a.listen()
        _ = hsm_b.interrogate(pin_b)

        hsm_a.listen()
        recv_cal = setup_receive_frame(pin_b, 1, 1)
        _ = hsm_b.receive(recv_cal)

        read_cal = setup_read_frame(pin_b, 1)
        assert hsm_b.read_file(read_cal)[32:] == calibration

        # Litho writes telemetry
        hsm_b.write_file(
            setup_write_frame(pin_b, 2, group_ids[7], "telemetry", telemetry)
        )

        # Attacker gets telemetry from Litho
        hsm_b.listen()
        _ = hsm_a.interrogate(pin_b)

        hsm_b.listen()
        recv_tel = setup_receive_frame(pin_a, 2, 2)
        _ = hsm_a.receive(recv_tel)

        read_tel = setup_read_frame(pin_a, 2)
        assert hsm_a.read_file(read_tel)[32:] == telemetry

    # def test_triple_digest_transfer_concurrency(
    #     self,
    #     hsm_a: HSMIntf,
    #     hsm_b: HSMIntf,
    #     pin_a: str,
    #     pin_b: str,
    #     group_ids: list[int],
    # ) -> None:
    #
    #     # Assume HSM API requires reading to derive digests for test purposes
    #     # groups[2]: A=-W- (write-only), B=R-C (receive-only) — minimum for A-writes, B-receives
    #     data = b"Digestable Data"
    #     hsm_a.write_file(setup_write_frame(pin_a, 0, group_ids[2], "digestable", data))
    #
    #     # Triggering concurrent interrogations/receives
    #     exceptions_caught: list[Exception] = []
    #     locks = threading.Barrier(2)
    #
    #     def litho_agent():
    #         try:
    #             _ = locks.wait()
    #             hsm_a.listen()
    #             _ = hsm_b.interrogate(pin_b)
    #             _ = hsm_b.receive(setup_receive_frame(pin_b, 0, 0))
    #         except Exception as e:
    #             exceptions_caught.append(e)
    #
    #     def attacker_agent():
    #         try:
    #             _ = locks.wait()
    #             hsm_a.listen()
    #         except Exception as e:
    #             exceptions_caught.append(e)
    #
    #     t1 = threading.Thread(target=litho_agent)
    #     t2 = threading.Thread(target=attacker_agent)
    #     t1.start()
    #     t2.start()
    #     t1.join()
    #     t2.join()
    #
    #     assert hsm_a.list(pin_a) is None or isinstance(hsm_a.list(pin_a), list)
