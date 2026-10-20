"""SETTINGS end-to-end by keyboard, with assertions (audit 2026-10-06).

What the previous version got wrong: it sent scancode 80 ("clamp to row 0")
although 80 is Down (Up is 72), so it clamped to the LAST row and pressed Enter
straight away - it saved without toggling anything, and its five screenshots all
had the same SHA-1.  "UI driven" was printed unconditionally.

This version:
  * waits for the desktop readiness counter from build/system/kernel.elf,
  * drives Up/Down/Enter/F10 with the CORRECT scancodes,
  * asserts that each step changed the screen (distinct screenshot hashes),
  * asserts the end-to-end result on the image itself: the save must leave
    TOSTA=1 and TOSTT=1 in DESKTOP.CFG (the file is written by the desktop, not
    by the kernel),
  * exits non-zero on any failure.

Screenshots use ABSOLUTE paths (relative ones land in Hatari's CWD).
"""
import hashlib
import os
import subprocess
import sys
import time
from pathlib import Path

P = Path("/Users/xxx/Documents/Deepseek_test/ATOS 2.1")
sys.path.insert(0, str(P / "atos2-tools"))
from atos2_auto import Hatari  # noqa: E402

IMG = Path(os.environ.get("ATOS_UI_IMG", str(P / "work/atos-tos-mod.img")))
OUT = Path("/tmp/toscfg")
OUT.mkdir(exist_ok=True)
BINARY = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"

UP, DOWN, ENTER, F10 = "Up", "Down", "Return", "F10"
failures = []


def check(name, cond, detail=""):
    print(("PASS  " if cond else "FAIL  ") + name + (("  " + detail) if detail else ""),
          flush=True)
    if not cond:
        failures.append(name)


def desktop_ready_symbol():
    """Desktop takeover counter, resolved from the ELF - never hardcoded.

    kdbg_desk_rejok counts DESKTOP.APP being accepted as the desktop owner
    (GFX opcode 13): >0 means the desktop took the screen, which is the only
    readiness signal that survives the desktop covering the text banner.
    """
    out = subprocess.run(["m68k-elf-nm", str(P / "build/system/kernel.elf")],
                         capture_output=True, text=True)
    for line in out.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == "kdbg_desk_rejok":
            return parts[2], int(parts[0], 16)
    return None, None


# SETTINGS geometry (desktop_wm.c): window 280 x 296 client, rows at client
# y = 44 + 24*i, client origin = frame + 16 px title.  The window is centred on
# the ACTIVE screen, which is why the emulated size is read from every
# screenshot: an earlier version hardcoded 800x600, and once a stray Enter
# activated a resolution row (640x400) the coordinates pointed at nothing and
# the run toggled the wrong options.
WIN_W, WIN_H = 280, 296
HATARI_BAR = 36          # status bar Hatari adds at the bottom of a screenshot


def row_geometry(im):
    """(screen_w, screen_h, y of row 0, x range of the row plate)."""
    w, h = im.size[0], im.size[1] - HATARI_BAR
    st_wx = max(16, (w - WIN_W) // 2)
    st_wy = max(16, (h - WIN_H) // 2)
    return w, h, st_wy + 16 + 44, (st_wx + 20, st_wx + WIN_W - 40)


def selected_row(path):
    """Index of the highlighted SETTINGS row, read from the screenshot.

    Navigating by key COUNT is not reliable: this harness drops keys, and one
    lost key left the selection a row off - the run then toggled PREFER TT-RAM
    instead of LOAD TOS AT START.  The highlighted row is drawn as a black plate
    with light text (setup_paint: GUI_BEVEL on every row, GUI_FILL colour 0 on
    the selected one), so it is the darkest band and can be measured.
    """
    from PIL import Image
    try:
        im = Image.open(path).convert("L")
    except Exception:
        return None
    _w, _h, row0, (x0, x1) = row_geometry(im)
    if x1 <= x0:
        return None
    best, score = None, 1e9
    for i in range(8):
        y = row0 + 24 * i + 10
        if y >= im.size[1] - HATARI_BAR:
            break
        vals = [im.getpixel((x, y)) for x in range(x0, x1, 8)]
        if not vals:
            continue
        m = sum(vals) / len(vals)
        if m < score:
            best, score = i, m
    return best


def read_shot(tag):
    """Row index from a FRESH screenshot, retried until the image is readable.

    A partially written screenshot made selected_row() return None, the old
    move_to() then pressed a key "to be safe" and the selection ran away to the
    bottom of the list (where Enter silently saved the unchanged options).
    """
    path = OUT / ("nav_%s.png" % tag)
    for _ in range(4):
        h.screenshot(str(path), settle=1.0)
        time.sleep(0.4)
        r = selected_row(path)
        if r is not None:
            return r
        time.sleep(0.6)
    return None


def move_to(row, tag):
    """Drive the selection to `row`, one confirmed keystroke at a time.

    Navigating by key COUNT is unreliable (this harness drops keys), so every
    press is verified against the highlighted row.  An unreadable screenshot is
    NOT treated as "no movement": nothing is pressed until the state is known,
    which keeps a lost key from turning into a runaway.
    """
    cur = read_shot(tag)
    for _ in range(16):
        if cur is None:
            cur = read_shot(tag)
            continue
        if cur == row:
            return True
        kp(DOWN if cur < row else UP, 0.8)
        nxt = read_shot(tag)
        if nxt is None:                      # unknown: do not press again yet
            cur = read_shot(tag)
            continue
        cur = nxt
    return cur == row


def cfg_values():
    """(TOSTA, TOSTT) as saved in the image, or None when the block is absent.

    The header block is RES=.. / THEME=.. / TOSTA=.. / TOSTT=.., so the pair is
    contiguous and icon lines never contain TOSTA.  Reading the REAL values
    makes the test idempotent: a fixed expectation made the second run of the
    same image fail (the boxes were already ON, so one toggle turned them OFF).
    """
    import re
    m = re.search(rb"TOSTA=(\d)\nTOSTT=(\d)", IMG.read_bytes())
    return (int(m.group(1)), int(m.group(2))) if m else None


N = []
for i in range(8):
    N += ["--acsi", "%d=none" % i]
E = ["--machine", "megast", "--cpulevel", "3", "--cpuclock", "32", "--memsize", "4",
     "--ttram", "64", "--tf536", "on", "--graphics-card", "vofa", "--gfx-chip",
     "et4000", "--mono", "--fast-forward", "off", "--ide-master", str(IMG)] + N

h = Hatari(str(P), log=str(P / "work/h-uidrive.log"), window=False, image=str(IMG),
           binary=BINARY, cfg=True, extra=E, render=True)


def kp(key, d=0.6):
    """Press a NAMED key with an explicit down/up pair.

    `hatari-event keypress N` (what this script used to send) can be delivered
    as a held key, and the desktop then sees auto-repeat: the selection ran away
    to a boundary and the run toggled the wrong rows.  The scenarios that work
    reliably all use keydown/keyup.
    """
    h.keydown(key, delay=0.08)
    h.keyup(key, delay=0.08)
    time.sleep(d)


def shot(name):
    path = OUT / name
    h.screenshot(str(path), settle=1.2)
    try:
        return hashlib.sha1(path.read_bytes()).hexdigest()
    except OSError:
        return None


try:
    sym, addr = desktop_ready_symbol()
    check("desktop readiness symbol found", sym is not None,
          "%s=%s" % (sym, hex(addr) if addr else "?"))
    before = cfg_values()
    print("TOSTA/TOSTT before:", before, flush=True)
    # Each checkbox is toggled exactly once, so the saved state must be the
    # OPPOSITE of what the image holds now (defaults 0 when absent).
    expect = (1 - before[0], 1 - before[1]) if before else (1, 1)

    # Boot: the desktop owns the screen, so poll the kernel counter instead of
    # looking for the text banner (which the desktop covers).
    ready = False
    t0 = time.time()
    while time.time() - t0 < 230.0:
        time.sleep(5.0)
        if addr:
            raw = h.read_bytes(addr, 4, settle=0.6) or b""
            if len(raw) == 4 and int.from_bytes(raw, "big") > 0:
                ready = True
                break
    check("desktop started (readiness counter > 0)", ready,
          "waited %.0fs" % (time.time() - t0))
    if not ready:
        # Fall back to a fixed settle so a failure still produces evidence.
        time.sleep(20.0)

    h01 = shot("01_desktop.png")
    kp(F10, 1.6)                              # F10 -> OPTIONS (SETTINGS is item 0)
    h02 = shot("02_options.png")
    check("F10 opened a menu", h01 is not None and h02 != h01,
          "hash %s -> %s" % (h01, h02))
    kp(ENTER, 2.0)                            # Enter -> SETTINGS window
    h03 = shot("03_settings.png")
    check("SETTINGS window opened", h03 not in (None, h02),
          "hash %s -> %s" % (h02, h03))

    got5 = move_to(5, "row5")                 # 5 = LOAD TOS AT START
    check("selection reached LOAD TOS AT START", got5,
          "highlighted row=%s" % selected_row(OUT / "nav_row5.png"))
    kp(ENTER, 1.4)                            # toggle it
    h04 = shot("04_tos_start_on.png")
    check("checkbox toggled (screen changed)", h04 not in (None, h03),
          "hash %s -> %s" % (h03, h04))

    got6 = move_to(6, "row6")                 # 6 = PREFER TT-RAM
    check("selection reached PREFER TT-RAM", got6,
          "highlighted row=%s" % selected_row(OUT / "nav_row6.png"))
    kp(ENTER, 1.4)                            # toggle it
    h05 = shot("05_tos_tt_on.png")
    check("second checkbox toggled", h05 not in (None, h04),
          "hash %s -> %s" % (h04, h05))

    got7 = move_to(7, "row7")                 # 7 = SAVE SETTINGS
    check("selection reached SAVE SETTINGS", got7,
          "highlighted row=%s" % selected_row(OUT / "nav_row7.png"))
    kp(ENTER, 2.0)                            # save
    h06 = shot("06_saved.png")

    # The real end-to-end check: the desktop wrote the toggled state to the image.
    time.sleep(2.0)
    after = cfg_values()
    check("DESKTOP.CFG holds the toggled TOSTA", after is not None and after[0] == expect[0],
          "expected=%s after=%s" % (expect, after))
    check("DESKTOP.CFG holds the toggled TOSTT", after is not None and after[1] == expect[1],
          "expected=%s after=%s" % (expect, after))

    hashes = [h01, h02, h03, h04, h05, h06]
    check("all six screenshots distinct", len(set(h for h in hashes if h)) == len(
        [h for h in hashes if h]), "hashes=%s" % hashes)
finally:
    print("rc", h.close(), flush=True)

if failures:
    print("FAILURES: %d -> %s" % (len(failures), ", ".join(failures)), flush=True)
    sys.exit(1)
print("ALL CHECKS PASS", flush=True)
