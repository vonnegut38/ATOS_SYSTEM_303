"""Guard the native (TOS=0) build profile and the TOS=1 opt-in.

The default kernel must not link the TOS persona.  The module API table and
syscall 65 still reference kernel_tos_run_program, so a stub under
#if !CONFIG_TOS keeps them linkable while the persona stays out of the image.
The Makefile default is TOS=0; TOS=1 restores the compatibility layer.

Run:  python3 -m unittest discover -s tests -p "test_tos_profile.py" -v
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


class TosProfile(unittest.TestCase):
    def test_makefile_default_is_native(self):
        self.assertIn("TOS ?= 0", read("Makefile"),
                      "the native profile must be the default")

    def test_makefile_drops_persona_objects(self):
        text = read("Makefile")
        self.assertIn("TOS_OBJS :=", text)
        self.assertIn("CONFIG_TOS=$(TOS)", text)

    def test_kernel_main_stubs_runner_when_tos_disabled(self):
        text = read("system/kernel_main.c")
        i = text.index("#if !CONFIG_TOS")
        block = text[i:text.index("#endif", i)]
        self.assertIn("kernel_tos_run_program", block)
        self.assertIn("-0x2ff", block)

    def test_tos_hooks_are_gated(self):
        self.assertIn("#if CONFIG_TOS", read("system/kernel_main.c"))
        entry = read("system/kernel_entry.S")
        self.assertIn("#if CONFIG_TOS", entry[entry.index("kernel_timer_c:"):])
        self.assertIn("tos_composite_tick", entry)

    def test_shell_reports_missing_persona(self):
        text = read("system/shell_module.S")
        self.assertIn("shell_tos_absent_text", text)
        self.assertIn("cmpi.l #-0x2ff,%d0", text)


if __name__ == "__main__":
    unittest.main()
