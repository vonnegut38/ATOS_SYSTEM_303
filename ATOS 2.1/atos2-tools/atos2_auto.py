#!/usr/bin/env python3
"""Drive ATOS 2.0 in Hatari headless via the --cmd-fifo control interface.

Control protocol (Hatari 2.6.1, src/control.c):
  hatari-debug <debugger command>   e.g. screenshot <file>, memdump, quit
  hatari-event keypress|keydown|keyup <char-or-scancode>
  hatari-event doubleclick|rightdown|rightup
  hatari-stop / hatari-cont

Key/mouse input therefore needs no host Accessibility permission.
"""

import os
import re
import struct
import subprocess
import time
from pathlib import Path

# text_terminal_t has a fixed 80-cell stride even for narrower wrapping grids.
TERM_COLS = 80
# sizeof(text_terminal_t): 2000 cells * 2 bytes + 8 header bytes + 2 attr bytes.
TERM_STRUCT_SIZE = 4010

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
        if k == ':':
            # ':' is Shift+';' on the ST; drive the Shift explicitly, because
            # 'hatari-event keydown :' delivered the unshifted key.
            return SCAN[';'], True
        if k.isalnum():
            return None, False  # use Keymap_SimulateCharacter
        if k.isupper():
            low = k.lower()
            if low in SCAN:
                return SCAN[low], True
            return None, True  # simulate lower + shift
    raise ValueError(f"cannot map key {keyspec!r}")


class Hatari:
    def __init__(self, project, log=None, window=False, extra=None, fast=False,
                 image=None, binary="hatari", cfg=True, render=False):
        """Start Hatari headless.

        render=False adds ``--disable-video on``: Hatari then skips the whole
        conversion/present path (conv_gen.c ConvGen_Draw*, sdl/screen.c
        Screen_GenConvUpdate return early), so a screenshot is BLACK no matter
        what the emulated machine drew.  Any test that takes a screenshot or
        checks what the guest displayed MUST pass render=True; otherwise it
        measures the emulator's disabled output, not the BIOS.
        """
        self.project = Path(project)
        self.fifo = self.project / "work" / "atos2-control.fifo"
        self.fifo.parent.mkdir(parents=True, exist_ok=True)
        self.fifo.unlink(missing_ok=True)
        self.log = Path(log) if log else self.project / "work" / "hatari-auto.log"
        self.image = Path(image) if image else self.project / "work/atos-v2.img"
        cmd = [binary]
        if cfg:
            cmd += ["-c", str(self.project / "atos2.cfg")]
        cmd += [
            "--cmd-fifo", str(self.fifo),
            "--tos", str(self.project / "build/bios_mste.bin"),
            "--patch-tos", "false",
            "--acsi", "0=" + str(self.image),
            "--machine", "ste", "--monitor", "mono", "--cpulevel", "3",
            "--cpuclock", "16", "--memsize", "4", "--ttram", "0",
            "--sound", "off", "--confirm-quit", "false",
            "--log-file", str(self.log),
        ]
        if window:
            cmd += ["--window"]
        else:
            # Offscreen SDL keeps the machine headless but leaves the render
            # path active, so screenshots contain the real converted frame.
            cmd += ["--disable-video", "off"] if render else ["--disable-video", "on"]
        if fast:
            cmd += ["--fast-forward", "on"]
        if extra:
            cmd += list(extra)
        self.cmdline = cmd
        self.capture_path = Path(str(self.log) + ".out")
        self._capture = open(self.capture_path, "w")
        env = None
        if render and not window:
            # Hatari has no --sdl-videodriver option; SDL reads the driver from
            # the environment.  "dummy" still creates a real surface and runs
            # the whole conversion/present path, which is what the screenshot
            # debugger command captures.  "offscreen" is not usable here: this
            # SDL3 build falls back to a real driver and fails on OpenGL.
            env = dict(os.environ)
            env["SDL_VIDEODRIVER"] = "dummy"
        self.proc = subprocess.Popen(cmd, stdout=self._capture,
                                     stderr=subprocess.STDOUT, env=env)
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
        # NON-BLOCKING open, with a bounded retry.  A plain open() blocks until
        # Hatari opens the read end - and when Hatari dies first (window closed,
        # emulator crash) that open never returns, so a supervisor hung forever
        # instead of restarting the window.  Every later write raises instead of
        # blocking too, which the callers can catch as "instance is gone".
        self.dead = False
        deadline = time.monotonic() + 10
        last_error = None
        while time.monotonic() < deadline:
            try:
                fd = os.open(self.fifo, os.O_WRONLY | os.O_NONBLOCK)
                self.out = os.fdopen(fd, "w")
                break
            except OSError as exc:
                last_error = exc
                if self.proc.poll() is not None:
                    raise SystemExit("Hatari exited before the control FIFO: %r"
                                     % (self.proc.returncode,))
                time.sleep(0.05)
        else:
            raise SystemExit("control FIFO %s never became writable: %r" % (self.fifo, last_error))

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
        if self.dead:
            raise BrokenPipeError("Hatari instance is gone")
        try:
            self.out.write(line + "\n")
            self.out.flush()
        except (BrokenPipeError, ValueError, OSError) as exc:
            self.dead = True
            raise BrokenPipeError("Hatari went away while writing the FIFO") from exc
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

    def read_u32(self, addr, settle=0.5):
        """Read one big-endian 32-bit word from emulated memory."""
        text = self.read_mem(hex(addr), "4", settle=settle)
        data = []
        for line in text.splitlines():
            if not re.match(r"^[0-9A-Fa-f]{8}:", line):
                continue
            for tok in line.split(":", 1)[1].split("  ", 1)[0].split():
                if len(tok) == 2 and all(c in "0123456789abcdefABCDEF" for c in tok):
                    data.append(int(tok, 16))
        return int.from_bytes(bytes(data[:4]), "big") if len(data) >= 4 else None

    def read_bytes(self, addr, count, settle=1.2):
        """Read count bytes of emulated memory via the debugger memdump.

        The address needs the 0x prefix ("46d58" is rejected) but the count
        must be plain decimal: Hatari answers "Invalid count 0!" for "0x10"
        and returns no bytes.  One command returns the whole range."""
        text = self.read_mem(hex(addr), str(count), settle=settle)
        data = bytearray()
        for line in text.splitlines():
            if not re.match(r"^[0-9A-Fa-f]{8}:", line):
                continue
            # Hatari prints "<addr>: <hex bytes>  <ascii>".  Strip the ascii
            # column first: two-character ascii tokens such as "F1" or "DI"
            # would otherwise parse as extra hex bytes and shift the stream.
            body = line.split(":", 1)[1]
            hexpart = body.split("  ", 1)[0]
            for tok in hexpart.split():
                if len(tok) == 2 and all(c in "0123456789abcdefABCDEF" for c in tok):
                    data.append(int(tok, 16))
        return bytes(data[:count])

    def console_symbols(self):
        """Resolve (consoles base, target symbol) once from kernel.elf."""
        if getattr(self, "_console_syms", None) is None:
            import subprocess
            elf = Path(self.project) / "build" / "system" / "kernel.elf"
            addr = tgt = None
            try:
                out = subprocess.run(["m68k-elf-nm", str(elf)], capture_output=True, text=True).stdout
                for ln in out.splitlines():
                    p = ln.split()
                    if len(p) == 3 and p[2] == "consoles":
                        addr = int(p[0], 16)
                    elif len(p) == 3 and p[2] == "target":
                        tgt = int(p[0], 16)
            except Exception:
                addr = tgt = None
            self._console_syms = (addr, tgt)
        return self._console_syms

    def console_addr(self):
        """Address of the ACTIVE session's console text buffer.

        kconsole uses '#define console consoles[target]', so the visible
        terminal is consoles[target].  Reading consoles[0] unconditionally
        always showed the primary session and broke session-switch scenarios."""
        addr, tgt = self.console_symbols()
        if addr is None or tgt is None:
            return addr
        raw = self.read_bytes(tgt, 4, settle=0.05)
        if raw and len(raw) >= 4:
            active = int.from_bytes(raw[:4], "big")
            if active < 8:
                return addr + active * TERM_STRUCT_SIZE
        return addr

    def console_lines(self, rows=25, cols=80, settle=1.2):
        """Return the active console's visible text, top line .. cursor row.

        text_terminal_t layout (include/text_terminal.h):
            offset 0     cells[TERM_CELLS]  (TERM_CELLS=2000, 2 bytes each)
            offset 4000  column, row, cols, rows (uint16), then attr,pad
        term_scroll() (system/text_terminal.c) is a memmove that shifts rows
        up, so the buffer at offset 0 *is* the visible screen and the area
        below t->row is blank.  The old version instead skipped to the last
        physical row ((full_rows-rows)*cols*2), so it read stale blanks.
        Reads with the fixed TERM_COLS stride, not the wrapping t->cols,
        because text_terminal.c always indexes cells[] by TERM_COLS.
        Returns None when memory cannot be read."""
        addr = self.console_addr()
        if addr is None:
            return None
        header = self.read_bytes(addr + 4000, 8, settle=settle)
        if not header or len(header) < 8:
            return None
        # uint16 fields are m68k-native, i.e. big-endian in memory.
        _column, cursor, tcols, trows = struct.unpack(">HHHH", header[:8])
        if not 1 <= tcols <= 80:
            tcols = 80
        if not 1 <= trows <= 25:
            trows = 25
        last = min(cursor, trows - 1)
        first = 0
        if rows and last + 1 > rows:
            first = last + 1 - rows
        count = last + 1 - first
        if count <= 0:
            return []
        raw = self.read_bytes(addr + first * TERM_COLS * 2,
                              count * TERM_COLS * 2, settle=settle)
        if not raw:
            return None
        return self.rows_from_cells(raw, count, tcols)

    @staticmethod
    def rows_from_cells(raw, count, cols=80):
        """Decode up to 'count' fixed-80-stride cell rows into text (pure).

        Each term_cell_t is a big-endian uint16 (m68k native): the byte at
        2*i is the attribute, the byte at 2*i+1 is the character.  Reading
        byte 2*i (as the original code did) returns the attribute, not text."""
        have = min(count, len(raw) // (TERM_COLS * 2))
        out = []
        for r in range(have):
            row = raw[r * TERM_COLS * 2:(r + 1) * TERM_COLS * 2]
            s = "".join(chr(row[i * 2 + 1]) if 32 <= row[i * 2 + 1] < 127 else " "
                        for i in range(cols))
            out.append(s.rstrip())
        return out

    def type_checked(self, text, prompt=">", tries=4, per_key=0.15, settle=0.6):
        """Type text, verifying via the cursor row that no keystroke was lost.

        The emulated ACIA can drop bytes; console_lines() now reads the real
        visible screen, so this returns True once 'text' is visible.  On a
        mismatch the line is erased with backspaces and retried."""
        import time as _t
        for _ in range(tries):
            self.type(text, delay=per_key)
            _t.sleep(settle)
            lines = self.console_lines(rows=1)
            if lines:
                last = lines[-1]
                if last.endswith(text):
                    return True
                for _ in range(max(len(last), len(text))):
                    self.press(chr(8), 0.06)
                _t.sleep(0.3)
        return False

    def screenshot(self, path, settle=0.5):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.unlink(missing_ok=True)
        self.cmd(f"hatari-debug screenshot {path.resolve()}")
        time.sleep(settle)
        return path.exists()

    @staticmethod
    def _scancode(code):
        """Hatari: pojedyncza cyfra jest ZNAKIEM, nie skankodem.  Kody 1..9
        (ESC, cyfry 1..8) trzeba podac jako 0x01..0x09, inaczej trafia zly
        klawisz - to bylo zrodlem falszywego 'ESC nie dziala'."""
        return f"0x{int(code):02x}" if isinstance(code, int) else str(code)

    def press(self, keyspec, delay=0.06):
        code, shift = resolve(keyspec)
        if shift:
            self.cmd(f"hatari-event keydown {self._scancode(SHIFT)}")
        if code is None:
            self.cmd(f"hatari-event keypress {keyspec}")
        else:
            self.cmd(f"hatari-event keypress {self._scancode(code)}")
        if shift:
            self.cmd(f"hatari-event keyup {self._scancode(SHIFT)}")
        if delay:
            time.sleep(delay)

    def keydown(self, keyspec, delay=0.04):
        code, shift = resolve(keyspec)
        if shift:
            self.cmd(f"hatari-event keydown {self._scancode(SHIFT)}")
        self.cmd(f"hatari-event keydown {self._scancode(code) if code is not None else keyspec}")
        if delay:
            time.sleep(delay)

    def keyup(self, keyspec, delay=0.04):
        code, shift = resolve(keyspec)
        self.cmd(f"hatari-event keyup {self._scancode(code) if code is not None else keyspec}")
        if shift:
            self.cmd(f"hatari-event keyup {self._scancode(SHIFT)}")
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