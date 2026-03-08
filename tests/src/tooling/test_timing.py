# pyright: reportMissingTypeStubs=false
import os
import struct
import time

from ectf.tools.hsm_interface import HSMError, HSMIntf

# Timing limits from the specs
MAX_TIME_LIST = 0.5  # 500 ms
MAX_TIME_READ = 3.0  # 3000 ms
MAX_TIME_WRITE = 3.0  # 3000 ms
MAX_TIME_RECEIVE = 3.0  # 3000 ms
MAX_TIME_INTERROGATE = 1.0  # 1000 ms
MAX_TIME_BAD_PIN = 5.0  # 5 seconds


def setup_write_frame(
    pin: str, slot: int, group: int, name_str: str, file_data: bytes
) -> bytes:
    pin_bytes = pin.encode("utf-8")[:6].ljust(6, b"\x00")
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
    pin_bytes = pin.encode("utf-8")[:6].ljust(6, b"\x00")
    return struct.pack("<6s B", pin_bytes, slot)


def setup_receive_frame(pin: str, read_slot: int, write_slot: int) -> bytes:
    pin_bytes = pin.encode("utf-8")[:6].ljust(6, b"\x00")
    return struct.pack("<6s B B", pin_bytes, read_slot, write_slot)


class TestTimingSpecs:
    def test_list_timing(self, hsm_a: HSMIntf, pin_a: str) -> None:
        start = time.time()
        _ = hsm_a.list(pin_a)
        duration = time.time() - start

        assert duration <= MAX_TIME_LIST, (
            f"List operation took {duration}s (Max {MAX_TIME_LIST}s)"
        )

    def test_write_timing(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # We test writing the largest allowable payload (8192 bytes)
        data = os.urandom(8192)
        # groups[3]: A=RW- (minimum for write, read test depends on this file)
        frame = setup_write_frame(pin_a, 0, group_ids[3], "timing_write_test", data)

        start = time.time()
        hsm_a.write_file(frame)
        duration = time.time() - start

        assert duration <= MAX_TIME_WRITE, (
            f"Write operation took {duration}s (Max {MAX_TIME_WRITE}s)"
        )

    def test_read_timing(self, hsm_a: HSMIntf, pin_a: str) -> None:
        # Assumes item written in test_write_timing is read back
        frame = setup_read_frame(pin_a, 0)

        start = time.time()
        _ = hsm_a.read_file(frame)
        duration = time.time() - start

        assert duration <= MAX_TIME_READ, (
            f"Read operation took {duration}s (Max {MAX_TIME_READ}s)"
        )

    def test_interrogate_timing(
        self, hsm_a: HSMIntf, hsm_b: HSMIntf, pin_b: str
    ) -> None:
        hsm_a.listen()

        start = time.time()
        _ = hsm_b.interrogate(pin_b)
        duration = time.time() - start

        assert duration <= MAX_TIME_INTERROGATE, (
            f"Interrogate operation took {duration}s (Max {MAX_TIME_INTERROGATE}s)"
        )

    def test_receive_timing(
        self,
        hsm_a: HSMIntf,
        hsm_b: HSMIntf,
        pin_a: str,
        pin_b: str,
        group_ids: list[int],
    ) -> None:
        # Prepopulate data on B
        # groups[4]: A=--C (receive-only), B=RWC (can write)
        hsm_b.write_file(
            setup_write_frame(pin_b, 1, group_ids[4], "timing_recv", os.urandom(8192))
        )

        hsm_b.listen()

        # Must interrogate first per normal protocol flow usually
        _ = hsm_a.interrogate(pin_a)

        hsm_b.listen()
        recv_f = setup_receive_frame(pin_a, 1, 1)

        start = time.time()
        _ = hsm_a.receive(recv_f)
        duration = time.time() - start

        assert duration <= MAX_TIME_RECEIVE, (
            f"Receive operation took {duration}s (Max {MAX_TIME_RECEIVE}s)"
        )

    def test_bad_pin_timing(self, hsm_a: HSMIntf) -> None:
        # Any operation where an invalid PIN is provided should take at most 5 seconds.
        # This usually means the timeout on the firmware side might be up to exactly 5s.
        start = time.time()
        try:
            _ = hsm_a.list("deadff")
        except HSMError:
            pass
        duration = time.time() - start

        assert duration <= MAX_TIME_BAD_PIN, (
            f"Bad PIN operation took {duration}s (Max {MAX_TIME_BAD_PIN}s)"
        )
