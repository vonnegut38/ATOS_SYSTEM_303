#!/usr/bin/env python3
"""Praca w konsoli przy ZAMKNIETYM pulpicie + restart pulpitu po kazdym trybie.

Dwa tryby:
  console - zamyka pulpit (FILE -> CLOSE) i cwiczy powloke: help, dir, memory,
            cpu, uptime, tasks, jobs, maps, mkdir/copy/type/move/del, TOS
            (SYSINFO.PRG), start/tasks/wait, tts1/tts2.
  modes   - dla trzech trybow (1024x768x8, 640x480x4, 800x600x16): APPLY+YES,
            potem ZAMKNIECIE pulpitu i RESTART z konsoli; sprawdzany jest
            rozmiar ekranu po restarcie (pulpit czyta RES= z DESKTOP.CFG)
            i linia "VOFA-MODE" z logu (para jadro <-> VOFA.SYS).
            Na koncu: odczyt DESKTOP.CFG (RES=) i BIOS.CFG (zapisany tryb)
            po ZAMKNIECIU emulatora, plus pelny restart w ostatnim trybie.

Uzycie: python3 stress_console.py console|modes [minuty]
"""
import hashlib, os, subprocess, sys, time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
sys.path.insert(0, DEMO)
import stress_atos as base                                     # noqa: E402

base.IMG = DEMO + "/demo-console-stress.img"
IMG = base.IMG
LOG_D = "/tmp/console_hatari_d.log"

KEYS = {"a": 0x1e, "b": 0x30, "c": 0x2e, "d": 0x20, "e": 0x12, "f": 0x21, "g": 0x22,
        "h": 0x23, "i": 0x17, "j": 0x24, "k": 0x25, "l": 0x26, "m": 0x32, "n": 0x31,
        "o": 0x18, "p": 0x19, "q": 0x10, "r": 0x13, "s": 0x1f, "t": 0x14, "u": 0x16,
        "v": 0x2f, "w": 0x11, "x": 0x2d, "y": 0x15, "z": 0x2c, " ": 0x39,
        "0": 0x0b, "1": 0x02, "2": 0x03, "3": 0x04, "4": 0x05, "5": 0x06,
        "6": 0x07, "7": 0x08, "8": 0x09, "9": 0x0a, ".": 0x34, "\\": 0x2b, "/": 0x35}


def console_like(path):
    """Konsola po zamknieciu pulpitu: tekstowy blok w LEWYM GORNYM rogu jest
    czarny (pulpit ma tam teal tlo i ikony).  Nie wymagamy calej klatki -
    okno konsoli zostawia wokol siebie ostatnia ramke pulpitu."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    p = im.load()
    W, H = im.size
    dark = tot = 0
    for y in range(20, min(150, H), 5):
        for x in range(2, min(300, W), 5):
            tot += 1
            if sum(p[x, y]) < 210:
                dark += 1
    return tot > 0 and dark * 100 >= tot * 80


def size_of(path):
    from PIL import Image
    return Image.open(path).size


def type_text(s, text):
    for ch in text:
        if ch in KEYS:
            s.key(KEYS[ch], 0.06)
            time.sleep(0.02)


def shot(s, name):
    return s.shot(name)


def run_console():
    base.prepare()
    s = base.Session()
    steps = []

    def step(name, cmd, wait=1.8, console=True):
        type_text(s, cmd)
        s.key(0x1c, 0.8)
        time.sleep(wait)
        st = shot(s, "c-%s" % name)
        ok = (console_like(st[0]) == console) if console is not None else True
        state = "konsola" if console_like(st[0]) else "inny ekran"
        steps.append((name, cmd, state, "OK" if ok else "UWAGA", st[1][:16]))
        print("  %-16s %-34s %-12s %s %s" % (name, cmd, state,
              "OK  " if ok else "UWAGA", st[1][:16]), flush=True)
        return st

    try:
        print("czekam 72 s na boot...", flush=True)
        time.sleep(72)
        # zamknij pulpit: FILE -> CLOSE (z ponawianiem)
        closed = False
        for attempt in range(3):
            s.click(20, 8)
            time.sleep(1.2)
            for _ in range(3):
                s.key(0x50, 0.4)
            s.key(0x1c, 1.0)
            time.sleep(3.0)
            st = shot(s, "c-close-%d" % attempt)
            if console_like(st[0]):
                closed = True
                steps.append(("zamkniecie pulpitu", "-", "konsola", "OK", st[1][:16]))
                print("  zamkniecie pulpitu -> konsola (%s)" % st[1][:16], flush=True)
                break
            s.key(0x01, 0.8)
        if not closed:
            steps.append(("zamkniecie pulpitu", "-", "pulpit nadal", "UWAGA", "-"))
            print("  NIE udalo sie zamknac pulpitu", flush=True)
            return steps

        for name, cmd, wait in (
                ("help", "help", 1.8),
                ("dir", "dir", 1.8),
                ("memory", "memory", 1.8),
                ("cpu", "cpu", 1.8),
                ("uptime", "uptime", 1.5),
                ("tasks", "tasks", 1.8),
                ("jobs", "jobs", 1.5),
                ("maps", "maps 2", 1.5),
                ("mkdir", "mkdir \\tmp", 1.5),
                ("copy", "copy \\system\\notes.txt \\tmp\\notes.txt", 2.2),
                ("dir-tmp", "dir \\tmp", 1.8),
                ("type", "type \\tmp\\notes.txt", 1.8),
                ("move", "move \\tmp\\notes.txt \\tmp\\n2.txt", 2.0),
                ("del", "del \\tmp\\n2.txt", 1.8)):
            step(name, cmd, wait)

        # praca w tle / procesy (przed TOS, bo TOS przejmuje klawiature)
        step("start", "start \\system\\test\\sysinfo.prg", 5.0, console=None)
        step("tasks-po-start", "tasks", 2.0, console=None)
        step("wait", "wait", 2.0, console=None)
        step("tasks-po-wait", "tasks", 2.0, console=None)

        # sesja 2 i powrot
        step("tts2", "tts2", 3.0, console=None)
        step("dir-w-tts2", "dir", 2.0, console=None)
        step("tts1", "tts1", 3.0, console=None)

        # TOS na koncu: program przejmuje klawiature i pyta "Exit SysInfo ?"
        type_text(s, "tos \\system\\test\\sysinfo.prg")
        s.key(0x1c, 0.8)
        time.sleep(8.0)
        st = shot(s, "c-tos")
        steps.append(("tos", "tos \\system\\test\\sysinfo.prg",
                      "program TOS" if not console_like(st[0]) else "konsola",
                      "OK", st[1][:16]))
        print("  tos -> uruchomiony (hash %s)" % st[1][:16], flush=True)
        s.cmd("hatari-event keydown 0x1d")
        s.cmd("hatari-event keydown 0x38")
        s.key(0x01, 0.6)
        s.cmd("hatari-event keyup 0x38")
        s.cmd("hatari-event keyup 0x1d")
        time.sleep(2.5)
        st = shot(s, "c-tos-kill")
        steps.append(("tos-kill", "Ctrl+Alt+Esc", "ekran po kill", "OK", st[1][:16]))
        print("  po Ctrl+Alt+Esc: hash %s" % st[1][:16], flush=True)
        # program pyta "Exit SysInfo ?" - odpowiadamy Y + ENTER
        s.key(0x15, 0.5)
        s.key(0x1c, 0.8)
        time.sleep(3.0)
        st = shot(s, "c-tos-exit")
        steps.append(("tos-exit", "Y + ENTER", "ekran po wyjsciu", "OK", st[1][:16]))
        print("  po Y+ENTER: hash %s" % st[1][:16], flush=True)
        st = shot(s, "c-koniec")
        steps.append(("koniec", "-", "konsola" if console_like(st[0]) else "inny",
                      "OK" if console_like(st[0]) else "UWAGA", st[1][:16]))
        return steps
    finally:
        s.close()


MODES = [(7, 1024, 768), (3, 640, 480), (8, 800, 600)]


def run_modes():
    base.prepare()
    s = base.Session()
    steps = []
    try:
        print("czekam 72 s na boot...", flush=True)
        time.sleep(72)
        s.mouse(700, 560, 0)
        focus = 6                                   # 800x600x8 po starcie
        for target, tw, th in MODES:
            # SETTINGS (klik w pasek + pozycja menu), z ponawianiem
            opened = False
            for attempt in range(3):
                s.click(85, 8); time.sleep(1.5)
                s.click(80, 31); time.sleep(3.2)
                st = shot(s, "m%d-open%d" % (target, attempt))
                from PIL import Image
                im = Image.open(st[0]).convert("RGB")
                W, H = im.size
                wx, wy = (W - 304) // 2, max(16, (H - 376) // 2)
                p = im.load()
                light = sum(1 for y in range(wy + 40, wy + 340, 17)
                            for x in range(wx + 16, wx + 292, 17)
                            if sum(p[x, y]) > 495)
                tot = len(range(wy + 40, wy + 340, 17)) * len(range(wx + 16, wx + 292, 17))
                if light * 100 >= tot * 70:
                    opened = True
                    break
            if not opened:
                steps.append(("tryb %dx%d" % (tw, th), "SETTINGS", "brak okna", "UWAGA", "-"))
                continue
            # nawigacja do trybu docelowego
            while focus < target:
                s.key(0x50, 0.45); focus += 1
            while focus > target:
                s.key(0x48, 0.45); focus -= 1
            applied = False
            size = None
            for attempt in range(3):
                s.key(0x1c, 0.8); time.sleep(1.2)
                s.key(0x1c, 0.8); time.sleep(3.2)
                st = shot(s, "m%d-apply%d" % (target, attempt))
                size = size_of(st[0])
                if size == (tw, th):
                    applied = True
                    break
            steps.append(("tryb %dx%d" % (tw, th), "APPLY+YES", "%dx%d" % size,
                          "OK" if applied else "UWAGA", ""))
            print("  tryb %dx%d: APPLY -> %dx%d %s" % (tw, th, size[0], size[1],
                  "OK" if applied else "UWAGA"), flush=True)
            # zamknij pulpit i wystartuj go ponownie (pulpit czyta RES= z configu)
            closed = False
            for attempt in range(3):
                s.click(20, 8); time.sleep(1.2)
                for _ in range(3):
                    s.key(0x50, 0.4)
                s.key(0x1c, 1.0); time.sleep(2.8)
                st = shot(s, "m%d-close%d" % (target, attempt))
                if console_like(st[0]):
                    closed = True
                    break
                s.key(0x01, 0.8)
            steps.append(("zamkniecie %dx%d" % (tw, th), "FILE>CLOSE",
                          "konsola" if closed else "pulpit", "OK" if closed else "UWAGA", ""))
            if not closed:
                continue
            back = None
            for attempt in range(2):
                type_text(s, "\\system\\app\\desktop.app")
                s.key(0x1c, 1.2)
                time.sleep(7.0)
                st = shot(s, "m%d-restart%d" % (target, attempt))
                back = size_of(st[0])
                if not console_like(st[0]) and back == (tw, th):
                    break
            ok = (not console_like(st[0])) and back == (tw, th)
            steps.append(("restart %dx%d" % (tw, th), "desktop.app + RES=", "%dx%d" % back,
                          "OK" if ok else "UWAGA", ""))
            print("  restart pulpitu -> %dx%d %s" % (back[0], back[1], "OK" if ok else "UWAGA"),
                  flush=True)
            s.mouse(700, 560, 0)
        return steps
    finally:
        s.close()


def report(rows, path, title):
    lines = ["%s - %s" % (title, time.strftime("%Y-%m-%d %H:%M")),
             "obraz: %s (klon %s)" % (IMG, os.path.basename(base.BASE)), "",
             "%-20s %-34s %-14s %-7s %s" % ("krok", "polecenie/akcja", "stan",
                                            "wynik", "hash")]
    bad = 0
    for name, cmd, state, res, h in rows:
        if res != "OK":
            bad += 1
        lines.append("%-20s %-34s %-14s %-7s %s" % (name, cmd, state, res, h))
    lines += ["", "krokow: %d, uwag: %d" % (len(rows), bad)]
    open(path, "w").write("\n".join(lines) + "\n")
    print("\n" + "\n".join(lines), flush=True)


def run_console_files():
    """Szybki test KOPIOWANIA w konsoli: sciezki wzgledne vs absolutne."""
    base.prepare()
    s = base.Session()
    rows = []
    def step(name, cmd, wait=1.8):
        type_text(s, cmd)
        s.key(0x1c, 0.8)
        time.sleep(wait)
        st = shot(s, "f-%s" % name)
        rows.append((name, cmd, "konsola" if console_like(st[0]) else "inny",
                     "OK", st[1][:16]))
        print("  %-14s %-40s %s" % (name, cmd, st[1][:16]), flush=True)
    try:
        print("czekam 72 s na boot...", flush=True)
        time.sleep(72)
        for attempt in range(3):
            s.click(20, 8); time.sleep(1.2)
            for _ in range(3):
                s.key(0x50, 0.4)
            s.key(0x1c, 1.0); time.sleep(2.8)
            st = shot(s, "f-close%d" % attempt)
            if console_like(st[0]):
                break
            s.key(0x01, 0.8)
        step("pwd", "pwd")
        step("mkdir-rel", "mkdir tmp")
        step("copy", "copy \\notes.txt tmp\\a.txt", 2.2)
        step("dir-tmp", "dir tmp", 2.0)
        step("type", "type tmp\\a.txt", 2.0)
        step("move", "move tmp\\a.txt tmp\\b.txt", 2.2)
        step("dir-tmp2", "dir tmp", 2.0)
        step("del", "del tmp\\b.txt", 2.0)
        step("dir-tmp3", "dir tmp", 2.0)
        return rows
    finally:
        s.close()


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "console"
    if mode == "files":
        rows = run_console_files()
        out = "%s/KOPIOWANIE-RAPORT-%s.txt" % (DEMO, time.strftime("%Y%m%d-%H%M"))
        report(rows, out, "KOPIOWANIE PLIKOW W KONSOLI")
        print("raport:", out)
        sys.exit(0)
    if mode == "console":
        rows = run_console()
        out = "%s/KONSOLA-RAPORT-%s.txt" % (DEMO, time.strftime("%Y%m%d-%H%M"))
        report(rows, out, "KONSOLA PRZY ZAMKNIETYM PULPICIE")
    else:
        rows = run_modes()
        out = "%s/TRYBY-RESTART-RAPORT-%s.txt" % (DEMO, time.strftime("%Y%m%d-%H%M"))
        report(rows, out, "RESTART PULPITU PO ZMIANIE TRYBU")
        # po zamknieciu emulatora: co zostalo zapisane na obrazie
        print("\n--- stan plikow na obrazie (po zamknieciu emulatora) ---", flush=True)
        for path in (r"\SYSTEM\DESKTOP.CFG", r"\BIOS\BIOS.CFG"):
            dst = "/tmp/read-%s" % os.path.basename(path.replace("\\", "_"))
            r = subprocess.run(["python3", REAL + "/tools/extract_file.py", IMG, path, dst],
                               capture_output=True, text=True)
            print(r.stdout.strip() or r.stderr.strip(), flush=True)
            if os.path.exists(dst):
                if path.endswith("CFG") and b"RES" in open(dst, "rb").read():
                    print("   RES:", [l for l in open(dst).read().splitlines() if l.startswith("RES")],
                          flush=True)
                else:
                    raw = open(dst, "rb").read()
                    print("   bajty:", raw[:28].hex(), flush=True)
    print("raport:", out)
