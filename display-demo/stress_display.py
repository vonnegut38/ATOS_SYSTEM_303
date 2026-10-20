#!/usr/bin/env python3
"""10 minut "zabawy ustawieniami ekranu" - z POMIAREM kazdego kroku.

Fazy:
  RES   - przejscie po calej liscie trybow (640x400x1 ... 800x600x16): APPLY,
          YES, pomiar ROZDZIELCZOSCI ze zrzutu + liczba kolorow (glebia),
  GUARD - podglad trybu BEZ potwierdzenia: jadro ma samo wrocic po ~4 s,
  THM   - motywy: kolor paska/ikon po kazdym APPLY,
  DESK  - kolory tla (6), kolor paska (5), tapeta (5 wzorow), rozmiar ikon
          (32/48/64), siatka (COMPACT/NORMAL/WIDE) - kazdy wiersz z pomiarem.

Sterowanie: klawiatura ("chodz i obserwuj" - Down+Enter, a pomiar mowi, ktory
wiersz/tryb faktycznie zadzialal), bo wstrzykiwane klikanie gubi zdarzenia.
Kazdy krok -> wiersz raportu w display-demo/DISPLAY-RAPORT-<data>.txt.
Uzycie: python3 stress_display.py [minuty]
"""
import hashlib, os, struct, subprocess, sys, time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = DEMO + "/demo-guard.img"
IMG = DEMO + "/demo-display.img"
HAT = DEMO + "/hatari-scalony-20261017"
FIFO = "/tmp/display.fifo"

S = {}
for _l in subprocess.run(["m68k-elf-nm", REAL + "/build/system/kernel.elf"],
                         capture_output=True, text=True).stdout.splitlines():
    _p = _l.split()
    if len(_p) == 3:
        S[_p[2]] = int(_p[0], 16)

BUILD = {
    r"\SYSTEM\KERNEL.SYS": REAL + "/build/system/KERNEL.SYS",
    r"\SYSTEM\GFX.APP": REAL + "/build/system/GFX.APP",
    r"\SYSTEM\APP\DESKTOP.APP": REAL + "/build/system/DESKTOP.APP",
    r"\SYSTEM\APP\SETTINGS.APP": REAL + "/build/system/SETTINGS.APP",
    r"\SYSTEM\ICONS.DAT": REAL + "/build/system/ICONS.DAT",
}
LAUNCH = """; ATOS desktop launcher
COMMANDER=\\SYSTEM\\APP\\AC.APP
SYSINFO=\\SYSTEM\\APP\\SYSINFO.APP
TASKMGR=\\SYSTEM\\APP\\TASKMGR.APP
PLASMA=\\SYSTEM\\APP\\PLASMA.APP
ARKANOID=\\SYSTEM\\APP\\ARKANOID.APP
FLASHBACK=\\SYSTEM\\APP\\FLASHBK.APP
SYSTOS=\\SYSTEM\\TEST\\SYSINFO.PRG
MODPLAY=\\SYSTEM\\APP\\MODPLAY.APP
PRE2=\\SYSTEM\\APP\\PRE2.APP
SETTINGS=\\SYSTEM\\APP\\SETTINGS.APP
"""
MODES = [(640, 400, 1), (800, 600, 1), (1024, 768, 1), (640, 480, 4), (1024, 768, 4),
         (640, 480, 8), (800, 600, 8), (1024, 768, 8), (800, 600, 16)]
# Wpisy 12-bitowe palety (jak w settings_app.c): tlo pulpitu i pasek.
BG_RGB = [0x122, 0x003, 0x020, 0x200, 0x222, 0x000]
BAR_RGB = [0x59f, 0x0a3, 0xa50, 0x555, 0xfff]


def rgb12_to_rgb(v):
    return (((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17)


def prepare():
    if os.path.exists(IMG):
        os.unlink(IMG)
    subprocess.run(["cp", "-c", BASE, IMG], check=True)
    for target, src in BUILD.items():
        subprocess.run(["python3", DEMO + "/install_file.py", IMG, target, src],
                       check=True, capture_output=True)
    open("/tmp/display-desk.cfg", "w").write(open(DEMO + "/desktop-cfg-wzor.txt").read())
    subprocess.run(["python3", DEMO + "/install_file.py", IMG,
                    r"\SYSTEM\DESKTOP.CFG", "/tmp/display-desk.cfg"], check=True, capture_output=True)
    open("/tmp/display-launch.cfg", "w").write(LAUNCH)
    subprocess.run(["python3", DEMO + "/install_file.py", IMG,
                    r"\SYSTEM\LAUNCH.CFG", "/tmp/display-launch.cfg"], check=True, capture_output=True)
    open("/tmp/display-iconsz.cfg", "w").write("48\n0\n")
    subprocess.run(["python3", DEMO + "/install_file.py", IMG,
                    r"\SYSTEM\ICONSZ.CFG", "/tmp/display-iconsz.cfg"], check=True, capture_output=True)


class Sess:
    def __init__(self):
        if os.path.exists(FIFO):
            os.unlink(FIFO)
        self.log = open("/tmp/display_hatari.log", "w")
        self.proc = subprocess.Popen([
            HAT, "-c", "/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/hatari.cfg",
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
            "--log-file", "/tmp/display_hatari_d.log", "--cmd-fifo", FIFO],
            stdout=self.log, stderr=self.log)
        self.fd = None

    def cmd(self, line):
        if self.fd is None:
            self.fd = os.open(FIFO, os.O_WRONLY)
        os.write(self.fd, (line + "\n").encode())
        time.sleep(0.08)

    def w(self, name, data):
        self.cmd("hatari-debug w b 0x%x %s" % (S[name], " ".join("0x%02x" % b for b in data)))

    def shot(self, name):
        path = "%s/%s.png" % (DEMO, name)
        if os.path.exists(path):
            os.unlink(path)
        self.cmd("hatari-debug screenshot " + path)
        for _ in range(80):
            time.sleep(0.25)
            if os.path.exists(path) and os.path.getsize(path) > 1024:
                time.sleep(0.35)
                break
        return path, hashlib.sha256(open(path, "rb").read()).hexdigest()

    def mouse(self, x, y, b):
        self.w("mouse_scripted", b"\x01")
        self.w("mouse_q_tail", struct.pack(">I", 0))
        self.w("mouse_q_head", struct.pack(">I", 0))
        self.w("mouse_x", struct.pack(">I", x))
        self.w("mouse_y", struct.pack(">I", y))
        self.w("mouse_buttons", struct.pack(">I", b))
        self.w("mouse_queue", struct.pack(">IIiiBB", b, 0, x, y, 1, 0))
        if b:
            self.w("mouse_press_x", struct.pack(">I", x))
            self.w("mouse_press_y", struct.pack(">I", y))
            self.w("mouse_press_pending", b"\x01")
        self.w("mouse_q_tail", struct.pack(">I", 1))
        time.sleep(0.3)

    def click(self, x, y, settle=1.0):
        self.mouse(x, y, 1)
        self.mouse(x, y, 0)
        time.sleep(settle)

    def key(self, sc, settle=0.55):
        self.cmd("hatari-event keydown 0x%02x" % sc)
        time.sleep(0.18)
        self.cmd("hatari-event keyup 0x%02x" % sc)
        time.sleep(settle)

    def close(self):
        try:
            self.proc.terminate()
            self.proc.wait(timeout=12)
        except Exception:
            self.proc.kill()
        if self.fd is not None:
            os.close(self.fd)
        self.log.close()


# --- pomiary ze zrzutu -------------------------------------------------------
def size_of(path):
    from PIL import Image
    return Image.open(path).size


def px(path, x, y):
    from PIL import Image
    im = Image.open(path).convert("RGB")
    return im.load()[x, y]


def distinct(path, x0, y0, x1, y1):
    from PIL import Image
    im = Image.open(path).convert("RGB")
    px_ = im.load()
    seen = set()
    for y in range(y0, min(y1, im.size[1])):
        for x in range(x0, min(x1, im.size[0])):
            seen.add(px_[x, y])
    return seen


def pattern_colors(path, W, H):
    """Kolory TAPETY z pustego obszaru (prawy dol) - przy wzorze tlo nie jest
    jednym kolorem, wiec "nie-tlo" trzeba liczyc wzgledem CALEGO zestawu."""
    return distinct(path, max(0, W // 2 + 60), H - 90, min(W, W // 2 + 140), H - 30)


def icon_width(path):
    from PIL import Image
    im = Image.open(path).convert("RGB")
    W, H = im.size
    p = im.load()
    bg = pattern_colors(path, W, H)
    xs = [x for y in range(30, min(56, H)) for x in range(20, min(112, W)) if p[x, y] not in bg]
    return (max(xs) - min(xs) + 1) if xs else None


def cell_px(path):
    """Szerokosc komorki = lewa krawedz 2. kolumny - 28 - 52 (COMPACT)."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    W, H = im.size
    p = im.load()
    bg = pattern_colors(path, W, H)
    lefts = [x for y in range(30, min(70, H)) for x in range(100, 210) if p[x, y] not in bg]
    return (min(lefts) - 28 - 52) if lefts else None


def wall_colors(path, W, H):
    """Kolory tapety w pustym obszarze (prawy dol)."""
    return len(distinct(path, max(0, W // 2 + 60), H - 90, min(W, W // 2 + 140), H - 30))


# --- sterowanie SETTINGS ----------------------------------------------------
def window_geom(W, H):
    """Okno SETTINGS jest centrowane przy starcie: (wx, wy) + pasek 16 px."""
    wx = (W - 304) // 2
    wy = max(16, (H - 360 - 16) // 2)
    return wx, wy


def settings_open(path):
    """Okno SETTINGS jest centrowane i ma JASNE tlo listy - liczymy jasne
    punkty w siatce wewnatrz okna (pojedynczy piksel paska tytulu bywa bialy
    napisem, wiec probka punktowa zawodzi)."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    W, H = im.size
    wx, wy = window_geom(W, H)
    if wx < 0 or wy < 0 or wx + 304 > W or wy + 360 > H:
        return False
    p = im.load()
    light = tot = 0
    for y in range(wy + 40, wy + 340, 17):
        for x in range(wx + 16, wx + 292, 17):
            c = p[x, y]
            tot += 1
            if c[0] > 165 and c[1] > 165 and c[2] > 165:
                light += 1
    return tot > 0 and light * 100 >= tot * 70


def open_settings(s, tag):
    for attempt in range(4):
        s.click(85, 8)
        time.sleep(1.6)
        s.click(80, 31)
        time.sleep(3.2)
        shot = s.shot("%s-open%d" % (tag, attempt))
        if settings_open(shot[0]):
            return shot
    return None


def park(s, W, H):
    """Kursor w prawy dolny rog - zeby nie wchodzil w pomiar ikon."""
    s.mouse(W - 40, H - 40, 0)
    time.sleep(0.3)


def mode_index(W, H):
    for i, (w, h, _b) in enumerate(MODES):
        if (w, h) == (W, H):
            return i
    return None


def run_window_mode():
    """Tryb ekranu przy OTWARTYM oknie PLIKI (i powrot) - z weryfikacja."""
    prepare()
    s = Sess()
    out = []
    try:
        print("czekam 72 s na boot...", flush=True)
        time.sleep(72)
        s.mouse(700, 560, 0)
        s.key(0x41, 1.0)                 # F7: okno PLIKI
        time.sleep(2.5)
        before = s.shot("w0-pliki-800")
        W, H = size_of(before[0])
        out.append(("start", "%dx%d" % (W, H), "okno PLIKI otwarte"))
        opened = open_settings(s, "w1")
        if not opened:
            out.append(("settings", "brak okna", "klik nie doszedl"))
            return out
        size = None
        for attempt in range(3):
            s.key(0x50, 0.45)            # 1024x768 (7. pozycja, focus start = 6)
            s.key(0x1c, 0.8); time.sleep(1.2)
            s.key(0x1c, 0.8)             # YES
            time.sleep(3.0)
            big = s.shot("w2-1024-z-oknem-%d" % attempt)
            size = size_of(big[0])
            if size != (800, 600):
                break
        out.append(("1024 z oknem", "%dx%d" % size, "przelaczone przy otwartym oknie"))
        back_size = None
        for attempt in range(3):
            # z 1024x768x8 (index 7) JEDEN krok w gore = 800x600x8 (index 6)
            for _ in range(1 if attempt == 0 else 2):
                s.key(0x48, 0.35)
            s.key(0x1c, 0.8); time.sleep(1.2); s.key(0x1c, 0.8)
            time.sleep(3.0)
            back = s.shot("w3-powrot-800-%d" % attempt)
            back_size = size_of(back[0])
            if back_size == (800, 600):
                break
        out.append(("powrot 800x600", "%dx%d" % back_size, "okno PLIKI powinno dalej byc"))
        s.key(0x01, 0.8)
        end = s.shot("w4-koniec")
        out.append(("koniec", "%dx%d" % size_of(end[0]), "stan po sesji"))
        return out
    finally:
        s.close()


def run(minutes=10.0):
    t_start = time.time()
    prepare()
    s = Sess()
    journal = []

    def log(phase, step, measured, expected, note=""):
        ok = (expected is None) or (measured == expected)
        journal.append(dict(phase=phase, step=step, measured=measured,
                            expected=expected, ok=ok, note=note,
                            t=round(time.time() - t_start, 1)))
        print("  %-5s %-3s zmierzone=%-26s oczek.=%-22s %s %s" % (
            phase, step, measured, expected, "OK  " if ok else "ROZNICA", note), flush=True)
        return ok

    try:
        print("czekam 72 s na boot...", flush=True)
        time.sleep(72)
        boot = s.shot("d0-boot")
        t_run = time.time()          # budzet faz liczymy od gotowego pulpitu
        W, H = size_of(boot[0])
        log("BOOT", 0, "%dx%d" % (W, H), "800x600", "start pulpitu")
        park(s, W, H)

        # ================= FAZA 1: TRYBY EKRANU ============================
        opened = open_settings(s, "d1")
        if not opened:
            log("RES", "otwarcie", "brak okna", "SETTINGS", "klik nie doszedl")
            return journal
        log("RES", "otwarcie", "SETTINGS", "SETTINGS", "okno na ekranie")
        # Probka pustego pulpitu rozroznia tryb PLANARNY (1/4 bpp: tlo jest
        # ditheringiem = 2 kolory) od CHUNKY (8/16 bpp: jednolity kolor).
        idx, dirn, step, seen = 6, +1, 0, {}
        while time.time() - t_run < minutes * 60.0 * 0.60:
            if idx >= len(MODES) - 1:
                dirn = -1
            elif idx <= 0:
                dirn = +1
            idx += dirn
            s.key(0x50 if dirn > 0 else 0x48, 0.45)
            s.key(0x1c, 0.6)                 # APPLY (podglad)
            time.sleep(0.9)
            s.key(0x1c, 0.6)                 # YES - zostaw tryb
            time.sleep(2.0)
            shot = s.shot("d1-tryb-%d" % step)
            W, H = size_of(shot[0])
            cols = len(distinct(shot[0], max(0, W - 90), H - 70, W - 8, H - 8))
            kind = "PLANAR (1/4 bpp)" if cols >= 2 else "CHUNKY (8/16 bpp)"
            seen[(W, H, kind)] = seen.get((W, H, kind), 0) + 1
            log("RES", step, "%dx%d %s" % (W, H, kind), None,
                "krok %d (focus_row~%d)" % (step, idx))
            step += 1
        # Podsumowanie: ktore geometrie faktycznie przelaczylismy
        geo = sorted({(w, h) for (w, h, _k) in seen})
        log("RES", "podsuma", "geometrie: %s" % ", ".join("%dx%d" % g for g in geo),
            "4 geometrie", "przelaczen: %d" % step)

        # ================= FAZA 2: STRAZNIK PODGLADU =======================
        before = s.shot("d2-przed-guardem")
        Wb, Hb = size_of(before[0])
        s.key(0x50, 0.5)                     # nastepny tryb w liscie
        s.key(0x1c)                          # APPLY, ale NIE potwierdzamy
        time.sleep(6.5)                      # straznik ma sam wrocic (~4 s)
        shot = s.shot("d2-guard")
        W, H = size_of(shot[0])
        log("GUARD", "brak-YES", "%dx%d" % (W, H), "%dx%d" % (Wb, Hb),
            "podglad bez potwierdzenia -> jadro wraca samo")
        s.key(0x1c)                          # domknij ewentualny dialog
        time.sleep(0.8)
        s.key(0x01)

        # ================= FAZA 3: MOTYWY ==================================
        opened = open_settings(s, "d3")
        if opened:
            s.key(0x0f)                      # TAB -> THEME
            time.sleep(0.8)
            prev_hash = None
            for i in range(4):
                s.key(0x50 if i % 2 else 0x48, 0.5)   # naprzemiennie: drugi/pierwszy
                s.key(0x1c, 0.6)                     # APPLY motywu
                time.sleep(1.6)
                shot = s.shot("d3-motyw-%d" % i)
                W, H = size_of(shot[0])
                bar = px(shot[0], W // 2, 8)
                bg = px(shot[0], 2, H - 2)
                changed = (prev_hash is not None and shot[1] != prev_hash)
                prev_hash = shot[1]
                log("THM", i, "pasek=%s tlo=%s hash=%s" % (bar, bg, shot[1][:8]),
                    None, "motyw zmienil ekran" if changed else "motyw bez zmian (ten sam / 1 motyw)")
            s.key(0x01)
            time.sleep(0.8)
        else:
            log("THM", "-", "brak okna", "SETTINGS", "nie otwarto")

        # ================= FAZA 4: DESK ====================================
        opened = open_settings(s, "d4")
        if opened:
            for _ in range(3):
                s.key(0x0f)                  # TAB x3: RES->THM->VOL->DESK
            time.sleep(1.0)
            prev = None
            for step in range(13):
                s.key(0x1c)                  # ENTER = akcja podswietlonego wiersza
                time.sleep(1.6)
                shot = s.shot("d4-desk-%d" % step)
                W, H = size_of(shot[0])
                bg = px(shot[0], 2, H - 2)
                bar = px(shot[0], W // 2, 8)
                iw = icon_width(shot[0])
                cw = cell_px(shot[0])
                wall = wall_colors(shot[0], W, H)
                what = []
                if prev:
                    if bg != prev["bg"]:
                        what.append("KOLOR TLA")
                    if bar != prev["bar"]:
                        what.append("KOLOR PASKA")
                    if wall != prev["wall"]:
                        what.append("TAPETA (%d kol.)" % wall)
                    if iw != prev["iw"]:
                        what.append("ROZMIAR IKON")
                    if cw != prev["cw"]:
                        what.append("SIATKA")
                prev = dict(bg=bg, bar=bar, iw=iw, cw=cw, wall=wall)
                log("DESK", step, "tlo=%s pasek=%s ikona=%s komorka=%s tapeta=%d"
                    % (bg, bar, iw, cw, wall), None,
                    "+".join(what) if what else "bez zmiany ekranu")
                s.key(0x50, 0.45)            # nastepny wiersz
            s.key(0x01)
            time.sleep(0.8)
        else:
            log("DESK", "-", "brak okna", "SETTINGS", "nie otwarto")

        # ================= FAZA 5: powrot i kontrola =======================
        opened = open_settings(s, "d5")
        if opened:
            s.key(0x01)                      # ESC
            time.sleep(1.0)
        shot = s.shot("d9-koniec")
        W, H = size_of(shot[0])
        log("KONIEC", 0, "%dx%d" % (W, H), None, "pulpit po calej sesji (stan faktyczny)")
        return journal
    finally:
        s.close()


def write_report(journal, path):
    lines = ["STRESS USTAWIEN EKRANU - %s" % time.strftime("%Y-%m-%d %H:%M"),
             "obraz: %s (klon %s)" % (IMG, os.path.basename(BASE)), "",
             "%-6s %-4s %-28s %-22s %-8s %s" % ("faza", "krok", "zmierzone",
                                                "oczekiwane", "wynik", "uwagi")]
    ok = 0
    for r in journal:
        if r["ok"]:
            ok += 1
        lines.append("%-6s %-4s %-28s %-22s %-8s %s" % (
            r["phase"], r["step"], r["measured"], r["expected"] if r["expected"] is not None else "-",
            "OK" if r["ok"] else "ROZNICA", r["note"]))
    lines += ["", "krokow: %d, zgodnych: %d, roznic: %d" % (len(journal), ok, len(journal) - ok)]
    open(path, "w").write("\n".join(lines) + "\n")
    print("\n" + "\n".join(lines), flush=True)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "window":
        for row in run_window_mode():
            print("  %-18s %-12s %s" % row)
        sys.exit(0)
    minutes = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0
    j = run(minutes)
    out = "%s/DISPLAY-RAPORT-%s.txt" % (DEMO, time.strftime("%Y%m%d-%H%M"))
    write_report(j, out)
    print("raport:", out)
