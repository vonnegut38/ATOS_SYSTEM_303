"""Guard against KERNEL.SYS load-address drift (ATOS v10, Phase 1).

The kernel image is linked for an absolute address that is hardcoded in three
independent places plus one shared C macro.  A mismatch makes the boot loader
reject the image at the header_load check (loader/bootstage.c), which used to
fail silently exactly this way.  This test fails fast if they ever diverge.

Run:  python3 -m unittest discover -s tests -p "test_kernel_load_addr.py" -v
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADDRESS = 0x00020000


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


class KernelLoadAddress(unittest.TestCase):
    def test_shared_macro(self):
        text = read("include/boot_protocol.h")
        m = re.search(r"#define\s+KERNEL_IMAGE_LOAD_ADDRESS\s+(0x[0-9A-Fa-f]+)UL", text)
        self.assertIsNotNone(m, "KERNEL_IMAGE_LOAD_ADDRESS missing in include/boot_protocol.h")
        self.assertEqual(int(m.group(1), 16), ADDRESS)

    def test_linker_script(self):
        text = read("linker/kernel-sys.ld")
        m = re.search(r"\.\s*=\s*(0x[0-9A-Fa-f]+)\s*;", text)
        self.assertIsNotNone(m, "no location counter assignment in linker/kernel-sys.ld")
        self.assertEqual(int(m.group(1), 16), ADDRESS)

    def test_kernel_entry_header(self):
        text = read("system/kernel_entry.S")
        m = re.search(r"\.long\s+(0x[0-9A-Fa-f]+)", text)
        self.assertIsNotNone(m, "no header load address in system/kernel_entry.S")
        self.assertEqual(int(m.group(1), 16), ADDRESS)

    def test_loader_uses_macro_and_not_stale_address(self):
        text = read("loader/bootstage.c")
        self.assertIn("KERNEL_IMAGE_LOAD_ADDRESS", text,
                      "loader/bootstage.c must use KERNEL_IMAGE_LOAD_ADDRESS")
        self.assertNotIn("0x00040000", text,
                         "loader/bootstage.c still contains the stale 0x00040000 load address")


if __name__ == "__main__":
    unittest.main()
