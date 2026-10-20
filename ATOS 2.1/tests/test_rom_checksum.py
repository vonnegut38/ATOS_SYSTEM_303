import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def rom_checksum(data):
    """Same algorithm as tools/patch_rom_checksum.py / diag_rom_checksum():
    sum of big-endian 32-bit words with header bytes 0x14..0x17 zeroed."""
    d = bytearray(data)
    d[0x14:0x18] = b"\0\0\0\0"
    return sum(struct.unpack_from(">I", d, i)[0]
               for i in range(0, len(d), 4)) & 0xFFFFFFFF


class RomChecksumTest(unittest.TestCase):
    def _check(self, name):
        p = ROOT / "build" / name
        if not p.exists():
            self.skipTest("%s not built" % name)
        data = p.read_bytes()
        self.assertEqual(len(data) % 4, 0, name)
        stored = struct.unpack_from(">I", data, 0x14)[0]
        self.assertEqual(rom_checksum(data), stored,
                         "%s: stored ROM checksum does not match content" % name)
        # A single flipped byte anywhere (padding included) must be detected.
        bad = bytearray(data)
        bad[-1] ^= 0xFF
        self.assertNotEqual(rom_checksum(bytes(bad)), stored, name)

    def test_st(self):
        self._check("bios_st.bin")

    def test_mste(self):
        self._check("bios_mste.bin")


if __name__ == "__main__":
    unittest.main()
