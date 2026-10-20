#!/usr/bin/env python3
"""Rozmiar ikon 32/48/64 + siatka: weryfikacja na emulatorze.

Tryby:
  boot48   - kanoniczny boot (bez ICONSZ.CFG): hash MUSI byc 0e821ede (48 px)
  boot64   - ICONSZ.CFG = "64\\n1\\n": pulpit ma ikony 64 px, odstep NORMAL
  live     - boot na 48, potem SETTINGS -> DESK -> ICON SIZE (klik) i sprawdzenie,
             ze pulpit PRZELICZYL siatke na 64 px (sciezka opcode 48 + polling)

Kazdy tryb pracuje na KLONIE obrazu (demo-icons.img); do emulatora wgrywa
build/system/{KERNEL.SYS,DESKTOP.APP,SETTINGS.APP,ICONS.DAT}.
"""
import hashlib, os, struct, subprocess, sys, time


DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = DEMO + "/demo-guard.img"
IMG = DEMO + "/demo-icons.img"
HAT = DEMO + "/hatari-scalony-20261017"
FIFO = "/tmp/icons.fifo"
REF = "0e821ede61e5946463cb75f424e592200da37803b5a2dc3ec7747cf249ef064a"
# Klikniecia: Hatari nie ma zdarzen myszy w fifo, wiec wstrzykujemy stan myszy
# do struktur jadra (ta sama droga co rollback_test.py / guard_final.py).
S = {}
for _l in subprocess.run(["m68k-elf-nm", REAL + "/build/system/kernel.elf"],
                         capture_output=True, text=True).stdout.splitlines():
    _p = _l.split()
    if len(_p) == 3:
        S[_p[2]] = int(_p[0], 16)


BUILD = {
    r"\SYSTEM\KERNEL.SYS": REAL + "/build/system/KERNEL.SYS",
    r"\SYSTEM\GFX.APP": REAL + "/build/system/GFX.APP",   # broker GFX musi znac opcody 48/49/50
    r"\SYSTEM\APP\DESKTOP.APP": REAL + "/build/system/DESKTOP.APP",
    r"\SYSTEM\APP\SETTINGS.APP": REAL + "/build/system/SETTINGS.APP",
    r"\SYSTEM\ICONS.DAT": REAL + "/build/system/ICONS.DAT",
}

CANONICAL = """RES=800x600x8
THEME=0
TOSTA=0
TOSTT=0
FILEWIN=420x300
TRASHWIN=440x300
My Atari=0,0
TRASH=0,1
TERMINAL=0,2
COMMANDER=0,4
SYSINFO=0,3
TASKMGR=1,2
PLASMA=1,3
ARKANOID=1,4
FLASHBACK=1,5
SYSTOS=0,5
MODPLAY=1,0
PRE2=2,0
"""


def sh(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def prepare(iconsz=None):
    # FRESH=1 wymusza nowy klon obrazu: powtorne booty tej samej kopii potrafia
    # przeniesc stan pulpitu (zapis configow przy wyjsciu), co zaburza testy.
    if os.environ.get("FRESH") == "1" and os.path.exists(IMG):
        os.unlink(IMG)
    if not os.path.exists(IMG) or os.path.getmtime(IMG) < os.path.getmtime(BASE):
        sh(["cp", "-c", BASE, IMG])
    for target, src in BUILD.items():
        sh(["python3", DEMO + "/install_file.py", IMG, target, src])
    with open("/tmp/icons-desktop.cfg", "w") as f:
        f.write(CANONICAL)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\DESKTOP.CFG", "/tmp/icons-desktop.cfg"])
    # LAUNCH.CFG z SETTINGS na koncu: pozwala otworzyc SETTINGS klawiatura
    # (strzalki + ENTER), co jest deterministyczne w przeciwienstwie do klikniec.
    launch = open("/tmp/icons-launch.cfg", "w")
    launch.write("; ATOS desktop launcher - one entry per line: LABEL=path\n")
    for name, path in (("COMMANDER", r"\SYSTEM\APP\AC.APP"),
                       ("SYSINFO", r"\SYSTEM\APP\SYSINFO.APP"),
                       ("TASKMGR", r"\SYSTEM\APP\TASKMGR.APP"),
                       ("PLASMA", r"\SYSTEM\APP\PLASMA.APP"),
                       ("ARKANOID", r"\SYSTEM\APP\ARKANOID.APP"),
                       ("FLASHBACK", r"\SYSTEM\APP\FLASHBK.APP"),
                       ("SYSTOS", r"\SYSTEM\TEST\SYSINFO.PRG"),
                       ("MODPLAY", r"\SYSTEM\APP\MODPLAY.APP"),
                       ("PRE2", r"\SYSTEM\APP\PRE2.APP"),
                       ("SETTINGS", r"\SYSTEM\APP\SETTINGS.APP")):
        launch.write("%s=%s\n" % (name, path))
    launch.close()
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\LAUNCH.CFG", "/tmp/icons-launch.cfg"])
    if iconsz is None:
        # usun ICONSZ.CFG: nie ma narzedzia "delete", wiec wpisujemy rozmiar domyslny
        iconsz = (48, 0)
    with open("/tmp/iconsz.cfg", "w") as f:
        f.write("%d\n%d\n" % iconsz)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\ICONSZ.CFG", "/tmp/iconsz.cfg"])


def boot(tag, seconds=66, script=None):
    """script: sekwencja ("shot",nazwa) / ("click",x,y) / ("key",scancode) /
    ("wait",sekundy) wykonywana po boocie - zrzuty w srodku, nie na koncu."""
    if os.path.exists(FIFO):
        os.unlink(FIFO)
    log = open("/tmp/icons_%s.log" % tag, "w")
    argv = [HAT, "-c", "/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/hatari.cfg",
            "--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
            "--memsize", "4", "--ttram", "64", "--tf536", "on",
            "--graphics-card", "vofa", "--gfx-chip", "et4000",
            "--fast-forward", "off", "--ide-master", IMG,
            "--tos", "/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/BIOS.bin",
            "--patch-tos", "false", "--mmu", "off", "--statusbar", "false",
            "--borders", "false", "--zoom", "1", "--sound", "44100",
            "--natfeats", "on", "--confirm-quit", "false", "--window",
            "--acsi", "0=none", "--acsi", "1=none", "--acsi", "2=none", "--acsi", "3=none",
            "--acsi", "4=none", "--acsi", "5=none", "--acsi", "6=none", "--acsi", "7=none",
            "--log-file", "/tmp/icons_%s_d.log" % tag, "--cmd-fifo", FIFO]
    p = subprocess.Popen(argv, stdout=log, stderr=log)
    fd = [None]

    def cmd(line):
        if fd[0] is None:
            fd[0] = os.open(FIFO, os.O_WRONLY)
        os.write(fd[0], (line + "\n").encode())
        time.sleep(0.1)

    def w(name, data):
        cmd("hatari-debug w b 0x%x %s" % (S[name], " ".join("0x%02x" % b for b in data)))

    def mouse(x, y, b):
        w("mouse_scripted", b"\x01")
        w("mouse_q_tail", struct.pack(">I", 0))
        w("mouse_q_head", struct.pack(">I", 0))
        w("mouse_x", struct.pack(">I", x))
        w("mouse_y", struct.pack(">I", y))
        w("mouse_buttons", struct.pack(">I", b))
        w("mouse_queue", struct.pack(">IIiiBB", b, 0, x, y, 1, 0))
        if b:
            w("mouse_press_x", struct.pack(">I", x))
            w("mouse_press_y", struct.pack(">I", y))
            w("mouse_press_pending", b"\x01")
        w("mouse_q_tail", struct.pack(">I", 1))
        time.sleep(0.35)

    def click(x, y):
        mouse(x, y, 1)
        mouse(x, y, 0)
        time.sleep(1.0)

    def key(sc):
        cmd("hatari-event keydown 0x%02x" % sc)
        time.sleep(0.18)
        cmd("hatari-event keyup 0x%02x" % sc)
        time.sleep(0.45)

    def rd32(name, logpath):
        """Odczyt licznika jadra: hatari-debug memdump trafia do logu Hatari."""
        pos = os.path.getsize(logpath)
        cmd("hatari-debug memdump 0x%x 1" % S[name])
        time.sleep(0.6)
        with open(logpath, errors="replace") as f:
            f.seek(pos)
            text = f.read()
        for line in text.splitlines():
            if line.startswith("0") and ":" in line:
                parts = line.split(":", 1)[1].split()
                if len(parts) >= 4:
                    return int("".join(parts[:4]), 16)
        return None

    def shot(name):
        path = "%s/%s.png" % (DEMO, name)
        if os.path.exists(path):
            os.unlink(path)
        cmd("hatari-debug screenshot " + path)
        for _ in range(80):
            time.sleep(0.25)
            if os.path.exists(path) and os.path.getsize(path) > 1024:
                time.sleep(0.4)
                break
        raw = open(path, "rb").read()
        h = hashlib.sha256(raw).hexdigest()
        print("  %-24s %s" % (name + ".png", h[:16]), flush=True)
        return path, h

    try:
        print("czekam %d s na boot..." % seconds, flush=True)
        time.sleep(seconds)
        if script is None:
            return []
        if callable(script):
            api = type("Api", (), {})()
            api.shot = shot
            api.click = click
            api.key = key
            api.wait = time.sleep
            api.rd32 = lambda name: rd32(name, "/tmp/icons_%s.log" % tag)
            return script(api)
        out = []
        for act in script:
            if act[0] == "shot":
                out.append(shot(act[1]))
            elif act[0] == "click":
                click(act[1], act[2])
            elif act[0] == "key":
                key(act[1])
            elif act[0] == "wait":
                time.sleep(act[1])
        return out
    finally:
        p.terminate()
        try:
            p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            p.kill()
        if fd[0] is not None:
            os.close(fd[0])
        log.close()



def menu_probe(path):
    """Czy menu SETTINGS (DISPLAY/THEMES) jest otwarte: probka w jego obszarze."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    px = im.load()
    return px[80, 31] != px[799, 599]


def icon_bitmap_width(path):
    """Szerokosc bitmapy pierwszej ikony (obszar tylko ikony: y 30..56, bo przy
    32 px etykieta zaczyna sie juz na y=60).  Marginesy przezroczyste w PNG
    zwężają pomiar, wiec porownujemy SZEROKOSCI WZGLEDEM SIEBIE (64/48=1.33,
    32/48=0.67), a nie wartosci bezwzgledne."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    W, H = im.size
    px = im.load()
    bg = px[W - 1, H - 1]
    xs = [x for y in range(30, min(56, H)) for x in range(20, min(112, W))
          if px[x, y] != bg]
    if not xs:
        return None
    return max(xs) - min(xs) + 1


def settings_open(path):
    """Czy okno SETTINGS jest na ekranie: probka w obszarze listy (nie tlo)."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    W, H = im.size
    px = im.load()
    bg = px[W - 1, H - 1]
    return px[300, 300] != bg and px[500, 260] != bg


def icon_bbox(path, x0=8, y0=40, x1=None, y1=None):
    """Bounding box nie-tla w okolicy pierwszej ikony (My Atari) + kolor tla."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    W, H = im.size
    x1 = x1 or min(W, 200)
    y1 = y1 or min(H, 200)
    px = im.load()
    bg = px[W - 1, H - 1]
    minx, miny, maxx, maxy = 10 ** 9, 10 ** 9, -1, -1
    for y in range(y0, y1):
        for x in range(x0, x1):
            if px[x, y] != bg:
                minx = min(minx, x); maxx = max(maxx, x)
                miny = min(miny, y); maxy = max(maxy, y)
    return (minx, miny, maxx, maxy) if maxx >= 0 else None


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "boot48"
    if mode == "boot48":
        prepare((48, 0))
        (path, h), = boot("boot48", 66, (("shot", "i48-boot"),))
        ok = h == REF
        print("hash==wzorzec:", ok)
        return 0 if ok else 1
    if mode == "boot32":
        prepare((32, 0))
        (path, h), = boot("boot32", 66, (("shot", "i32-boot"),))
        print("bbox ikony 32:", icon_bbox(path))
        return 0
    if mode == "boot64":
        prepare((64, 1))
        (path, h), = boot("boot64", 66, (("shot", "i64-boot"),))
        print("bbox ikony 64:", icon_bbox(path))
        return 0
    if mode == "probe":
        prepare((48, 0))
        script = [("shot", "p0-boot"),
                  ("click", 85, 8), ("wait", 0.8), ("shot", "p1-po-pasku"),
                  ("wait", 0.5), ("click", 80, 31), ("wait", 2.0), ("shot", "p2-po-display")]
        res = boot("probe", 70, script)
        for path, h in res:
            print("  %-12s menu_open=%s settings_open=%s" % (
                os.path.basename(path), menu_probe(path), settings_open(path)))
        return 0
    if mode == "ui":
        # Sciezka UZYTKOWNIKA: SETTINGS -> DESK -> ICON SIZE, z weryfikacja
        # (okno SETTINGS na ekranie + pomiar szerokosci ikony wzgledem stanu
        # wyjsciowego).  Klawiatura, bo automat klikajacy jest niestabilny.
        prepare((48, 0))

        def flow(api):
            res = {"boot": api.shot("u0-przed")}
            opened = None
            for attempt in range(4):
                api.click(85, 8)
                api.wait(1.8)
                api.click(80, 31)
                api.wait(3.5)
                st = api.shot("u1-open%d" % attempt)
                if settings_open(st[0]):
                    opened = st
                    break
            res["open"] = opened
            if not opened:
                return res
            for _ in range(3):
                api.key(0x0f)                       # TAB x3 -> DESK
            api.wait(1.0)
            for _ in range(9):
                api.key(0x48)                       # DOWN x9 -> ICON SIZE
            api.wait(1.0)
            res["desk"] = api.shot("u2-desk")
            api.key(0x1c)                           # ENTER: 48 -> 64
            api.wait(4.0)
            res["po"] = api.shot("u3-po-enter")
            api.key(0x01)                           # ESC zamyka SETTINGS
            api.wait(2.0)
            res["esc"] = api.shot("u4-po-esc")
            return res

        res = boot("ui", 70, flow)
        if not res.get("open"):
            print("WYNIK: SETTINGS nie otworzylo sie (klik nie doszedl)")
            return 2
        w0 = icon_bitmap_width(res["boot"][0])
        w2 = icon_bitmap_width(res["po"][0])
        w3 = icon_bitmap_width(res["esc"][0])
        print("szerokosc ikony: przed=%s, w trakcie (SETTINGS otwarte)=%s, po ESC=%s"
              % (w0, w2, w3))
        ratio = (w3 / w0) if (w0 and w3) else None
        print("stosunek po ESC: %s (oczek. 1.33 = 64 px)" % ("%.2f" % ratio if ratio else None))
        # SETTINGS jest na wierzchu, wiec ikony widac dopiero po ESC
        ok = ratio is not None and 1.18 < ratio < 1.48
        print("WYNIK:", "SETTINGS->DESK->ICON SIZE DZIALA (48 -> 64)" if ok else "NIE ZMIENILO")
        return 0 if ok else 1
    if mode == "live":
        prepare((48, 0))

        def flow(api):
            def wait_change(prev, name, seconds=10.0):
                """Czeka, az ekran sie zmieni (klawisz mogl zginac w kolejce)."""
                t0 = time.time()
                while time.time() - t0 < seconds:
                    api.wait(1.0)
                    st = api.shot(name)
                    if st[1] != prev:
                        return st
                return api.shot(name)

            res = {"boot": api.shot("i-live-0-przed")}
            api.key(0x42)                      # F9: opcode 48 (jak SETTINGS)
            res["p1"] = wait_change(res["boot"][1], "i-live-1-po-f9")
            api.key(0x42)                      # 64 -> 32
            res["p2"] = wait_change(res["p1"][1], "i-live-2-po-f9-drugi")
            return res

        res = boot("live", 70, flow)
        w0 = icon_bitmap_width(res["boot"][0])
        w1 = icon_bitmap_width(res["p1"][0])
        w2 = icon_bitmap_width(res["p2"][0])
        print("szerokosc bitmapy ikony: start=%s, po F9=%s, po drugim F9=%s" % (w0, w1, w2))
        r1 = (w1 / w0) if (w0 and w1) else None
        r2 = (w2 / w0) if (w0 and w2) else None
        print("stosunki: 64/48 = %s (oczek. 1.33), 32/48 = %s (oczek. 0.67)" % (
            "%.2f" % r1 if r1 else None, "%.2f" % r2 if r2 else None))
        ok = r1 is not None and r2 is not None and 1.18 < r1 < 1.48 and 0.52 < r2 < 0.82
        print("WYNIK:", "PRZELACZANIE NA ZYWO DZIALA (48 -> 64 -> 32)" if ok
              else "NIE PRZELACZYL")
        return 0 if ok else 1
    raise SystemExit("tryb: boot32 | boot48 | boot64 | live")


if __name__ == "__main__":
    sys.exit(main())
