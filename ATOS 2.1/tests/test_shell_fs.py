"""Guard the shell filesystem builtins (docs/SHELL-REFACTOR-PLAN.md).

The shell is a runtime-loaded module: kernel_module.c calls the entry as
image + offset, so any absolute reference inside the module points at its
link address and is wrong after loading.  New shell C sources must therefore
be built with -mpcrel, and static tables must not store pointers to their own
data (those are link-time addresses even with -mpcrel).

Run:  python3 -m unittest discover -s tests -p "test_shell_fs.py" -v
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


class ShellFs(unittest.TestCase):
    def test_builtins_exist(self):
        text = read("system/shell_fs.c")
        for word in ('"copy"', '"move"', '"del"', '"mkdir"'):
            self.assertIn(word, text)

    def test_dispatched_from_shell_module(self):
        text = read("system/shell_module.S")
        self.assertIn("shell_fs_run", text)
        self.assertIn("shell_fs_handled", text)

    def test_tos_dispatch_hook(self):
        # The "commands" launch point routes TOS programs through the shared
        # decision and runs them before the native spawn path.
        text = read("system/shell_module.S")
        self.assertIn("shell_tos_dispatch", text)
        self.assertIn("shell_start_tos_handled", text)
        self.assertIn(".extern shell_tos_dispatch", text)

    def test_tos_dispatch_uses_shared_matcher(self):
        fs = read("system/shell_fs.c")
        self.assertIn("tos_program_path_ext(path)", fs)
        self.assertIn("shell_tos_report", fs)
        # The single shared decision lives in one header, not three copies.
        ext = read("include/tos_ext.h")
        self.assertIn("tos_program_path_ext", ext)
        self.assertIn("tos_ext_is", ext)

    def test_command_table_has_no_stored_pointers(self):
        text = read("system/shell_fs.c")
        self.assertIn("char name[8];", text,
                      "a const char * in the table would hold a link-time address")

    def test_module_api_exposes_mutations(self):
        api = read("include/kernel_module.h")
        for field in ("copy_path", "mkdir_path", "delete_path", "move_path"):
            self.assertIn(field, api)

    def test_shell_c_sources_are_pcrel(self):
        make = read("Makefile")
        self.assertIn("SHELL_CFLAGS = $(CFLAGS) -mpcrel", make)
        for obj in ("shell_path.o", "shell_complete.o", "shell_fs.o"):
            self.assertIn(obj, make)


if __name__ == "__main__":
    unittest.main()
