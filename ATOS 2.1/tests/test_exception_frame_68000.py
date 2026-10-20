"""Guard the 68000 group-1 exception frame handling (B3, 2026-09-27).

On the 68000 a bus error (vector 2) and an address error (vector 3) push the
six-word group-1 frame: mode@0, fault address@2, opcode@6, SR@8, with no
stacked PC.  Reading SR from +0 returns the mode word, whose undocumented
opcode bits can imitate the supervisor bit, so the kernel used to panic instead
of terminating a user process.  See
docs/B3-KWALIFIKACJA-WYJATKOW_2026-09-24_PL.md.

This is a static guard for the assembly offsets; the runtime proof lives in
atos2-tools/scenario_faults_cpu.py and atos2-tools/scenario_supervisor_fault.py.

Run:  python3 -m unittest discover -s tests -p "test_exception_frame_68000.py" -v
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def block(text, start, end):
    i = text.index(start)
    j = text.index(end, i) if end else len(text)
    return text[i:j]


class ExceptionFrame68000(unittest.TestCase):
    def setUp(self):
        self.text = read("system/kernel_entry.S")

    def test_group1_macro_reads_sr_from_plus8(self):
        macro = block(self.text, ".macro kernel_capture_group1", ".endm")
        self.assertIn("tst.b kernel_cpu_type", macro)
        self.assertIn("move.w 8(%sp),kernel_fault_sr+2", macro)
        self.assertIn("move.l 2(%sp),kernel_fault_pc", macro)

    def test_standard_macro_still_reads_sr_from_plus0(self):
        macro = block(self.text, ".macro kernel_capture_fault", ".endm")
        self.assertIn("move.w (%sp),kernel_fault_sr+2", macro)
        self.assertNotIn("8(%sp)", macro)

    def test_group1_handlers_use_the_group1_macro(self):
        self.assertIn("kernel_capture_group1 2", self.text)
        self.assertIn("kernel_capture_group1 3", self.text)
        self.assertNotIn("kernel_bus_error:\n    kernel_capture_fault 2", self.text)
        self.assertNotIn("kernel_address_error:\n    kernel_capture_fault 3", self.text)

    def test_user_test_reads_group1_sr_at_plus12(self):
        fn = block(self.text, "kernel_try_user_fault:", "kernel_timer_c:")
        self.assertIn("btst #5,12(%sp)", fn)

    def test_cpu_type_is_stored_at_install(self):
        install = block(self.text, "kernel_exception_install:", "kernel_preemption_enable:")
        self.assertIn("move.b %d0,kernel_cpu_type", install)
        self.assertIn("kernel_cpu_type:", self.text)


if __name__ == "__main__":
    unittest.main()
