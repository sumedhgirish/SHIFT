# pyright: reportMissingTypeStubs=false
import os
import struct
import tempfile
import threading
import time

import pytest
from ectf.tools.hsm_interface import HSMError, HSMIntf


def generate_random_file(size: int) -> bytes:
    return os.urandom(size)


def setup_write_frame(
    pin: str, slot: int, group: int, name_str: str, file_data: bytes
) -> bytes:
    # Host Command:
    # Pin (6 bytes)
    # Slot (8 bits)
    # Group ID (16 bits)
    # Name (32 bytes) null-terminated
    # UUID (16 bytes)
    # Contents Length (16 bits)
    # File Contents (Variable len)
    pin_bytes = pin.encode()[:6].ljust(6, b"\x00")
    name_bytes = name_str.encode()[:31].ljust(32, b"\x00")  # ensure null terminated
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
    # Host Command:
    # Pin (6 bytes)
    # Slot (8 bits)
    pin_bytes = pin.encode()[:6].ljust(6, b"\x00")
    return struct.pack("<6s B", pin_bytes, slot)


def setup_receive_frame(pin: str, read_slot: int, write_slot: int) -> bytes:
    # Host Command for Receive:
    # Pin (6 bytes)
    # Read Slot (8 bits)
    # Write Slot (8 bits)
    pin_bytes = pin.encode()[:6].ljust(6, b"\x00")
    return struct.pack("<6s B B", pin_bytes, read_slot, write_slot)


@pytest.fixture
def temp_dir():
    with tempfile.TemporaryDirectory() as td:
        yield td


class TestRunTests:
    def test_list_empty(
        self, hsm_a: HSMIntf, hsm_b: HSMIntf, pin_a: str, pin_b: str
    ) -> None:
        assert hsm_a.list(pin_a) == []
        assert hsm_b.list(pin_b) == []

    def test_write_1_and_read_1(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        file_data = b"Hello from eCTF HSM"
        # groups[3]: A=RW- (minimum for write+read)
        frame = setup_write_frame(pin_a, 0, group_ids[3], "testfile1", file_data)

        hsm_a.write_file(frame)
        files = hsm_a.list(pin_a)
        assert len(files) > 0

        read_frame = setup_read_frame(pin_a, 0)
        content = hsm_a.read_file(read_frame)
        assert content[32:] == file_data

    def test_interrogate_1_and_receive_1(
        self, hsm_a: HSMIntf, hsm_b: HSMIntf, pin_b: str
    ) -> None:
        t1 = threading.Thread(target=hsm_a.listen)
        t1.start()
        time.sleep(0.1)
        files_a = hsm_b.interrogate(pin_b)
        assert len(files_a) > 0
        t1.join()

        t2 = threading.Thread(target=hsm_a.listen)
        t2.start()
        time.sleep(0.1)
        receive_frame = setup_receive_frame(pin_b, 0, 0)
        _ = hsm_b.receive(receive_frame)
        t2.join()

        read_frame = setup_read_frame(pin_b, 0)
        content = hsm_b.read_file(read_frame)
        assert content[32:] == b"Hello from eCTF HSM", content

    def test_pass_file_back_and_forth(
        self,
        hsm_a: HSMIntf,
        hsm_b: HSMIntf,
        pin_a: str,
        pin_b: str,
        group_ids: list[int],
    ) -> None:
        b_data = b"Data originally from B"
        frame_b = setup_write_frame(pin_b, 2, group_ids[7], "filefromB", b_data)
        hsm_b.write_file(frame_b)

        t1 = threading.Thread(target=hsm_b.listen)
        t1.start()
        time.sleep(0.1)
        files_b = hsm_a.interrogate(pin_a)
        assert len(files_b) > 0
        t1.join()

        t2 = threading.Thread(target=hsm_b.listen)
        t2.start()
        time.sleep(0.1)
        _ = hsm_a.receive(setup_receive_frame(pin_a, 2, 2))
        t2.join()

        assert hsm_a.read_file(setup_read_frame(pin_a, 2))[32:] == b_data

        t3 = threading.Thread(target=hsm_a.listen)
        t3.start()
        time.sleep(0.1)
        files_a = hsm_b.interrogate(pin_b)
        assert len(files_a) > 0
        t3.join()

        t4 = threading.Thread(target=hsm_a.listen)
        t4.start()
        time.sleep(0.1)
        _ = hsm_b.receive(setup_receive_frame(pin_b, 2, 3))
        t4.join()

        assert hsm_b.read_file(setup_read_frame(pin_b, 3))[32:] == b_data

    def test_write_max_and_overwrite(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # groups[3]: A=RW-
        max_data = generate_random_file(8192)
        frame1 = setup_write_frame(pin_a, 1, group_ids[3], "maxfile", max_data)

        hsm_a.write_file(frame1)
        assert hsm_a.read_file(setup_read_frame(pin_a, 1))[32:] == max_data

        small_data = b"Small overwrite"
        frame2 = setup_write_frame(pin_a, 1, group_ids[3], "smallfile", small_data)
        hsm_a.write_file(frame2)
        assert hsm_a.read_file(setup_read_frame(pin_a, 1))[32:] == small_data

    # Additional file bounds
    def test_write_max_file_size(
        self, hsm_a: HSMIntf, temp_dir: str, pin_a: str, group_ids: list[int]
    ) -> None:
        size = 8192
        with open(os.path.join(temp_dir, "max_file.bin"), "wb") as f:
            _ = f.write(generate_random_file(size))

        with open(os.path.join(temp_dir, "max_file.bin"), "rb") as f:
            file_data = f.read()

        # Assuming we clear a slot first or just overwrite slot 0
        # groups[3]: A=RW- (minimum for write+read)
        hsm_a.write_file(
            setup_write_frame(pin_a, 0, group_ids[3], "maxsize_file", file_data)
        )
        read_frame = setup_read_frame(pin_a, 0)
        content = hsm_a.read_file(read_frame)
        assert content[32:] == file_data

    def test_write_all_ascii(
        self, hsm_a: HSMIntf, temp_dir: str, pin_a: str, group_ids: list[int]
    ) -> None:
        ascii_data = bytes(range(128))
        with open(os.path.join(temp_dir, "ascii.bin"), "wb") as f:
            _ = f.write(ascii_data)

        hsm_a.write_file(
            setup_write_frame(pin_a, 1, group_ids[3], "ascii_test", ascii_data)
        )

        read_frame = setup_read_frame(pin_a, 1)
        assert hsm_a.read_file(read_frame)[32:] == ascii_data

    def test_0_byte_file(
        self, hsm_a: HSMIntf, temp_dir: str, pin_a: str, group_ids: list[int]
    ) -> None:
        empty_data = b""
        with open(os.path.join(temp_dir, "empty.bin"), "wb") as f:
            _ = f.write(empty_data)

        hsm_a.write_file(
            setup_write_frame(pin_a, 2, group_ids[3], "empty_file", empty_data)
        )
        read_frame = setup_read_frame(pin_a, 2)
        assert hsm_a.read_file(read_frame)[32:] == empty_data

    # Error conditions
    def test_bad_pin(self, hsm_a: HSMIntf) -> None:
        with pytest.raises(HSMError):
            _ = hsm_a.list("deadff")

    def test_read_without_perms(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # groups[2] on A = -W- (can write, cannot read)
        hsm_a.write_file(
            setup_write_frame(pin_a, 3, group_ids[2], "unreadable", b"Unreadable")
        )

        with pytest.raises(HSMError):
            read_frame = setup_read_frame(pin_a, 3)
            _ = hsm_a.read_file(read_frame)

    def test_write_without_perms(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # groups[1] on A = R-- (can read, cannot write)
        with pytest.raises(HSMError):
            hsm_a.write_file(
                setup_write_frame(
                    pin_a, 4, group_ids[1], "no_write", b"Should Fail Write"
                )
            )

    def test_receive_without_perms(
        self,
        hsm_a: HSMIntf,
        hsm_b: HSMIntf,
        pin_a: str,
        pin_b: str,
        group_ids: list[int],
    ) -> None:
        # groups[3] on A = RW- (no receive), on B = RWC (can write)
        hsm_b.write_file(
            setup_write_frame(
                pin_b, 5, group_ids[3], "no_recv", b"Cannot be received by A"
            )
        )

        t = threading.Thread(target=hsm_b.listen)
        t.start()
        time.sleep(0.1)
        # A attempts receive
        recv_frame = setup_receive_frame(pin_a, 5, 5)  # read slot 5, write slot 5
        with pytest.raises(HSMError):
            _ = hsm_a.receive(recv_frame)
        t.join(timeout=1.0)

    # Race Condition specific testing section
    def test_race_condition_interrogate_while_writing(
        self,
        hsm_a: HSMIntf,
        hsm_b: HSMIntf,
        pin_a: str,
        pin_b: str,
        group_ids: list[int],
    ) -> None:
        exceptions_caught: list[Exception] = []
        hsm_a_lock = threading.Lock()

        def write_thread():
            try:
                for _ in range(10):
                    # groups[2]: A=-W- (minimum for write-only)
                    with hsm_a_lock:
                        hsm_a.write_file(
                            setup_write_frame(
                                pin_a, 0, group_ids[2], "race_test", b"Race Data"
                            )
                        )
            except Exception as e:
                exceptions_caught.append(e)

        def interrogate_thread():
            try:
                for _ in range(10):
                    # hsm_a must be put in listen mode first, but we can't send
                    # multiple commands to hsm_a at the same time.
                    with hsm_a_lock:
                        t = threading.Thread(target=hsm_a.listen)
                        t.start()
                        time.sleep(0.1)
                        
                    _ = hsm_b.interrogate(pin_b)
                    t.join()
            except Exception as e:
                exceptions_caught.append(e)

        t1 = threading.Thread(target=write_thread)
        t2 = threading.Thread(target=interrogate_thread)

        t1.start()
        t2.start()
        t1.join()
        t2.join()

        assert hsm_a.list(pin_a) is not None
        assert hsm_b.list(pin_b) is not None
