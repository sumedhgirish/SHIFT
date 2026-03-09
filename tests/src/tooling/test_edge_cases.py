# pyright: reportMissingTypeStubs=false
import os
import struct

import pytest
from ectf.tools.hsm_interface import HSMError, HSMIntf


def custom_write_frame(
    pin: str,
    slot: int,
    group: int,
    name_bytes: bytes,
    uuid_bytes: bytes,
    length: int,
    file_data: bytes,
) -> bytes:
    # Allows intentionally malformed packets
    pin_bytes = pin.encode("utf-8")[:6].ljust(6, b"\x00")
    return struct.pack(
        f"<6s B H 32s 16s H {len(file_data)}s",
        pin_bytes,
        slot,
        group,
        name_bytes,
        uuid_bytes,
        length,
        file_data,
    )


class TestEdgeCases:
    def test_out_of_bounds_slot(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # Slots are 0-7. Try to write to slot 8 and 255.
        data = b"OutOfBounds"
        uuid = os.urandom(16)
        name = b"out_of_bounds".ljust(32, b"\x00")

        # groups[2]: A=-W- (minimum for write attempt)
        frame_8 = custom_write_frame(
            pin_a, 8, group_ids[2], name, uuid, len(data), data
        )
        with pytest.raises(HSMError):
            hsm_a.write_file(frame_8)

        frame_255 = custom_write_frame(
            pin_a, 255, group_ids[2], name, uuid, len(data), data
        )
        with pytest.raises(HSMError):
            hsm_a.write_file(frame_255)

    def test_missing_null_terminator(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        data = b"NoNullTerminator"
        uuid = os.urandom(16)
        name_32_bytes_no_null = b"A" * 32
        # groups[2]: A=-W- (minimum for write)
        frame = custom_write_frame(
            pin_a, 0, group_ids[2], name_32_bytes_no_null, uuid, len(data), data
        )

        try:
            hsm_a.write_file(frame)
        except HSMError:
            # Rejection is a valid safe handling
            pass

        # Verify the HSM is still alive and didn't crash
        assert isinstance(hsm_a.list(pin_a), list)

    def test_embedded_null_byte_name(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # Name with a null byte early on
        data = b"EmbeddedNull"
        uuid = os.urandom(16)
        name = b"hid\x00den".ljust(32, b"\x00")
        # groups[3]: A=RW- (need write to create, list to inspect name)
        frame = custom_write_frame(pin_a, 1, group_ids[3], name, uuid, len(data), data)

        hsm_a.write_file(frame)
        files = hsm_a.list(pin_a)

        # Does the HSM list it as "hid" or "hid\x00den"? It should be null-terminated at 'd'.
        found = False
        for f in files:
            # f is (slot, group, name)
            if f[0] == 1:
                found = True
                assert f[2] == "hid".ljust(32, "\x00").encode()
        assert found

    def test_length_mismatch_too_long(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # Frame says length is 5, but we supply 500 bytes (trailing garbage)
        data = b"A" * 500
        uuid = os.urandom(16)
        name = b"trailing_garbage".ljust(32, b"\x00")

        pin_bytes = pin_a.encode("utf-8")[:6].ljust(6, b"\x00")
        # groups[2]: A=-W- (minimum for write attempt)
        frame = struct.pack(
            f"<6s B H 32s 16s H {len(data)}s",
            pin_bytes,
            3,
            group_ids[2],
            name,
            uuid,
            5,
            data,
        )

        try:
            hsm_a.write_file(frame)
        except HSMError:
            pass

        # Verify it didn't crash from reading past the frame or corrupt its UART state
        assert isinstance(hsm_a.list(pin_a), list)

    def test_invalid_group_id(self, hsm_a: HSMIntf, pin_a: str) -> None:
        # Rules specify 1-65535 for Group IDs. What if we use 0?
        data = b"GroupZero"
        uuid = os.urandom(16)
        name = b"group_zero".ljust(32, b"\x00")
        frame = custom_write_frame(pin_a, 4, 0, name, uuid, len(data), data)

        with pytest.raises(HSMError):
            hsm_a.write_file(frame)

    def test_duplicate_uuids(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # Two different files, different slots, exact same UUID
        data1 = b"Data1"
        data2 = b"Data2"
        uuid = os.urandom(16)
        name1 = b"file1".ljust(32, b"\x00")
        name2 = b"file2".ljust(32, b"\x00")

        # groups[2]: A=-W- (minimum for write-only)
        frame1 = custom_write_frame(
            pin_a, 5, group_ids[2], name1, uuid, len(data1), data1
        )
        hsm_a.write_file(frame1)

        frame2 = custom_write_frame(
            pin_a, 6, group_ids[2], name2, uuid, len(data2), data2
        )
        try:
            # Some designs might panic or reject duplicate UUIDs due to FAT implementations
            hsm_a.write_file(frame2)
        except HSMError:
            pass

        assert isinstance(hsm_a.list(pin_a), list)

    def test_fat_exhaustion(
        self, hsm_a: HSMIntf, pin_a: str, group_ids: list[int]
    ) -> None:
        # Spec says "at least 8" slots. If we assume the FAT is bounded to 8 entries...
        # Let's write 8 files.
        data = b"FATContent"
        uuid_base = b"123456789012345"

        # groups[2]: A=-W- (minimum for write-only)
        for i in range(8):
            name = (f"fat_file_{i}").encode().ljust(32, b"\x00")
            uid = uuid_base + bytes([i])
            f = custom_write_frame(pin_a, i, group_ids[2], name, uid, len(data), data)
            # Should succeed
            hsm_a.write_file(f)

        assert len(hsm_a.list(pin_a)) == 8

        # Write a 9th file to a new slot (e.g. slot 8)
        # If FAT is strictly 8 slots, this must cleanly raise HSMError, NOT crash or corrupt FAT
        name9 = b"fat_file_8".ljust(32, b"\x00")
        uid9 = uuid_base + b"8"
        f9 = custom_write_frame(pin_a, 8, group_ids[2], name9, uid9, len(data), data)
        try:
            hsm_a.write_file(f9)
        except HSMError:
            pass

        # Ensure it works after the attack
        assert isinstance(hsm_a.list(pin_a), list)
