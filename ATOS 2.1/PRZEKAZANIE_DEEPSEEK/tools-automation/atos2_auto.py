#!/usr/bin/env python3
"""Drive ATOS 2.0 in Hatari headless via the --cmd-fifo control interface.

Control protocol (Hatari 2.6.1, src/control.c):
  hatari-debug <debugger command>   e.g. screenshot <file>, memdump, quit
  hatari-event keypress|keydown|keyup <char-or-scancode>
  hatari-event doubleclick|rightdown|rightup
  hatari-stop / hatari-cont

Key/mouse input therefore needs no host Accessibility permission.
"""

import subprocess
import time
from pathlib import Path

# Atari ST scancodes (decimal) for characters that are not plain alphanumerics.
SCAN = {
    " ": 57, "\n": 28, "\r": 28, "\t": 15, "\b": 14, ".": 52, "/": 53,
    "-": 12, "=": 13, ",": 51, "'": 40, ";": 39, "[": 26, "]": 27, "\\": 43,
    "\x60": 41, "+": 78,
}
SHIFT = 42
LALT = 56
CTRL = 29

FKEY = {f"F{i}": 58 + i for i in range(1, 11)}  # F1=59 .. F10=68
ARROW = {"Up": 72, "Down": 80, "Left": 75, "Right": 77}
SPECIAL = {"Esc": 1, "Escape": 1, "Return": 28, "Enter": 28, "Space": 57,
           "Tab": 15, "Backspace": 14, "Shift": SHIFT, "Alt": LALT, "Ctrl": CTRL,
           "KP+": 78, "KP-": 74, "KP*": 102, "KP/": 101}


def resolve(keyspec):
    """Return (scancode or None, shift) for a key spec."""
    if isinstance(keyspec, int):
        return keyspec, False
    k = keyspec
    if k in SPECIAL:
        return SPECIAL[k], False
    if k in FKEY:
        return FKEY[k], False
    if k in ARROW:
        return ARROW[k], False
    if len(k) == 1:
        if k in SCAN:
            return SCAN[k], False
        if k.isalnum():
            return None, False  # use Keymap_SimulateCharacter
        if k.isupper():
            low = k.lower()
            if low in SCAN:
                return SCAN[low], True
            return None, True  # simulate lower + shift
    raise ValueError(f"cannot map key {keyspec!r}")


class Hatari:
    def __init__(self, project, log=None, window=False, extra=None, fast=False, image=None):
        self.project = Path(project)
        self.fifo = self.project / "work" / "atos2-control.fifo"
        self.fifo.parent.mkdir(parents=True, exist_ok=True)
        self.fifo.unlink(missing_ok=True)
        self.log = Path(log) if log else self.project / "work" / "hatari-auto.log"
        self.image = Path(image) if image else self.project / "work/atos-v2.img"
        cmd = [
            "hatari", "-c", str(self.project / "atos2.cfg"),
            "--cmd-fifo", str(self.fifo),
            "--tos", str(self.project / "build/bios_mste.bin"),
            "--patch-tos", "false",
            "--acsi", "0=" + str(self.image),
            "--machine", "ste", "--monitor", "mono", "--cpulevel", "3",
            "--cpuclock", "16", "--memsize", "4", "--ttram", "0",
            "--sound", "off", "--confirm-quit", "false",
            "--log-file", str(self.log),
        ]
        cmd += ["--window"] if window else ["--disable-video", "on"]
        if fast:
            cmd += ["--fast-forward", "on"]
        if extra:
            cmd += list(extra)
        self.cmdline = cmd
        self.capture_path = Path(str(self.log) + ".out")
        self._capture = open(self.capture_path, "w")
        self.proc = subprocess.Popen(cmd, stdout=self._capture,
                                     stderr=subprocess.STDOUT)
        # Hatari itself creates the FIFO with mkfifo().  Wait for that node
        # before opening it for writing: opening a not-yet-existing path would
        # create a plain file and make Hatari's mkfifo() fail with EEXIST.
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if self.fifo.is_fifo():
                break
            if self.proc.poll() is not None:
                break
            time.sleep(0.02)
        self.out = open(self.fifo, "w")  # blocks until Hatari opens the read end

    def close(self, grace=6.0):
        if self.proc.poll() is None:
            try:
                self.cmd("hatari-debug quit")
            except Exception:
                pass
            try:
                self.proc.wait(timeout=grace)
            except subprocess.TimeoutExpired:
                self.proc.terminate()
                try:
                    self.proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
                    self.proc.wait()
        try:
            self.out.close()
        except Exception:
            pass
        try:
            self._capture.close()
        except Exception:
            pass
        self.fifo.unlink(missing_ok=True)
        return self.proc.returncode

    def cmd(self, line, delay=0.0):
        self.out.write(line + "\n")
        self.out.flush()
        if delay:
            time.sleep(delay)

    def wait(self, seconds):
        time.sleep(seconds)

    def read_mem(self, addr, count, settle=0.6):
        """Run 'hatari-debug memdump <addr> <count>' and return the new output."""
        try:
            self._capture.flush()
            start = self.capture_path.stat().st_size
        except Exception:
            start = 0
        self.cmd(f"hatari-debug memdump {addr} {count}")
        time.sleep(settle)
        self._capture.flush()
        with open(self.capture_path, "r", errors="replace") as fh:
            fh.seek(start)
            return fh.read()

    def screenshot(self, path, settle=0.5):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.unlink(missing_ok=True)
        self.cmd(f"hatari-debug screenshot {path.resolve()}")
        time.sleep(settle)
        return path.exists()

    def press(self, keyspec, delay=0.06):
        code, shift = resolve(keyspec)
        if shift:
            self.cmd(f"hatari-event keydown {SHIFT}")
        if code is None:
            self.cmd(f"hatari-event keypress {keyspec}")
        else:
            self.cmd(f"hatari-event keypress {code}")
        if shift:
            self.cmd(f"hatari-event keyup {SHIFT}")
        if delay:
            time.sleep(delay)

    def keydown(self, keyspec, delay=0.04):
        code, _ = resolve(keyspec)
        self.cmd(f"hatari-event keydown {code if code is not None else keyspec}")
        if delay:
            time.sleep(delay)

    def keyup(self, keyspec, delay=0.04):
        code, _ = resolve(keyspec)
        self.cmd(f"hatari-event keyup {code if code is not None else keyspec}")
        if delay:
            time.sleep(delay)

    def type(self, text, delay=0.07):
        for ch in text:
            self.press(ch, delay=delay)

    def combo(self, keys, delay=0.08):
        """Press modifiers + final key, e.g. combo(['Shift','Alt','F10'])."""
        mods, last = keys[:-1], keys[-1]
        for m in mods:
            self.keydown(m)
        time.sleep(0.05)
        self.press(last, delay=0.05)
        for m in reversed(mods):
            self.keyup(m)
        time.sleep(delay)

    def mouse(self, event, delay=0.08):
        self.cmd(f"hatari-event {event}")
        if delay:
            time.sleep(delay)
