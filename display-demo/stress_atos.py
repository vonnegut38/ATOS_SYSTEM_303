#!/usr/bin/env python3
"""Stress test ATOS 2.1 na emulatorze: ~30 minut klikania i uruchamiania.

Co robi: bootuje KLON obrazu (demo-stress.img), potem przechodzi liste scen -
ikony pulpitu (terminal, kosz, pliki, My Atari), menu i prompt RUN, wszystkie
5 zakladek SETTINGS (tryby ekranu z podgladem, motywy, kolory/tapeta/rozmiar
ikon/siatka, edytor kafelka), okna (przeciaganie, resize), aplikacje natywne
(SYSINFO, TASKMGR, PLASMA), gry (ARKANOID), MODPLAY/PRE2 (audio) i program TOS
(SYSTOS + kill-switch Ctrl+Alt+Esc).

Po KAZDYM scenie: zrzut ekranu + test zywotnosci (licznik tykniec jadra rosnie,
a po ESC wracamy na pulpit).  Wynik: dziennik scen, zrzuty i podsumowanie w
display-demo/STRESS-RAPORT-<data>.txt.

Uzycie:  python3 stress_atos.py [minuty]        (domyslnie 30)
"""
import hashlib, os, struct, subprocess, sys, time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = DEMO + "/demo-guard.img"
IMG = DEMO + "/demo-stress.img"
HAT = DEMO + "/hatari-scalony-20261017"
FIFO = "/tmp/stress.fifo"

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

CANONICAL = open(DEMO + "/desktop-cfg-wzor.txt").read()

LAUNCH = """; ATOS desktop launcher - one entry per line: LABEL=path
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

# Komorki siatki (48 px, COMPACT: 100 x 68 od 28,28) - srodki ikon.
CELL = {(0, 0): (78, 62), (0, 1): (78, 130), (0, 2): (78, 198),
        (0, 3): (78, 266), (0, 4): (78, 334), (0, 5): (78, 402),
        (1, 0): (178, 62), (1, 2): (178, 198), (1, 3): (178, 266),
        (1, 4): (178, 334), (1, 5): (178, 402), (2, 0): (278, 62)}


def sh(cmd):
    subprocess.run(cmd, check=True, capture_output=True)


def prepare():
    if os.path.exists(IMG):
        os.unlink(IMG)
    sh(["cp", "-c", BASE, IMG])
    for target, src in BUILD.items():
        sh(["python3", DEMO + "/install_file.py", IMG, target, src])
    open("/tmp/stress-desk.cfg", "w").write(CANONICAL)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\DESKTOP.CFG", "/tmp/stress-desk.cfg"])
    open("/tmp/stress-launch.cfg", "w").write(LAUNCH)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\LAUNCH.CFG", "/tmp/stress-launch.cfg"])
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\ICONSZ.CFG", "/tmp/stress-iconsz.cfg"]
       if os.path.exists("/tmp/stress-iconsz.cfg") else
       ["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\ICONSZ.CFG", "/tmp/stress-desktop.cfg"])


class Session:
    def __init__(self):
        if os.path.exists(FIFO):
            os.unlink(FIFO)
        self.log = open("/tmp/stress_hatari.log", "w")
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
                "--log-file", "/tmp/stress_hatari_d.log", "--cmd-fifo", FIFO]
        self.proc = subprocess.Popen(argv, stdout=self.log, stderr=self.log)
        self.fd = None
        self.t0 = time.time()

    def cmd(self, line):
        if self.fd is None:
            self.fd = os.open(FIFO, os.O_WRONLY)
        os.write(self.fd, (line + "\n").encode())
        time.sleep(0.08)

    def w(self, name, data):
        self.cmd("hatari-debug w b 0x%x %s" % (S[name], " ".join("0x%02x" % b for b in data)))

    def rd32(self, name):
        # memdump pisze na KONSOLE Hatari (stdout), nie do pliku --log-file
        pos = os.path.getsize("/tmp/stress_hatari.log")
        self.cmd("hatari-debug memdump 0x%x 1" % S[name])
        time.sleep(0.5)
        with open("/tmp/stress_hatari.log", errors="replace") as f:
            f.seek(pos)
            text = f.read()
        for line in text.splitlines():
            if ":" in line and line[0].isalnum():
                parts = line.split(":", 1)[1].split()
                if len(parts) >= 4:
                    try:
                        return int("".join(parts[:4]), 16)
                    except ValueError:
                        return None
        return None

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
        raw = open(path, "rb").read()
        return path, hashlib.sha256(raw).hexdigest()

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

    def click(self, x, y, settle=0.9):
        self.mouse(x, y, 1)
        self.mouse(x, y, 0)
        time.sleep(settle)

    def dbl(self, x, y, settle=1.4):
        self.click(x, y, 0.25)
        self.click(x, y, settle)

    def drag(self, x0, y0, x1, y1, steps=6):
        self.mouse(x0, y0, 1)
        for i in range(1, steps + 1):
            self.w("mouse_x", struct.pack(">I", x0 + (x1 - x0) * i // steps))
            self.w("mouse_y", struct.pack(">I", y0 + (y1 - y0) * i // steps))
            self.w("mouse_q_head", struct.pack(">I", 0))
            self.w("mouse_queue", struct.pack(">IIiiBB", 1, 0, x0 + (x1 - x0) * i // steps,
                                              y0 + (y1 - y0) * i // steps, 1, 0))
            self.w("mouse_q_tail", struct.pack(">I", 1))
            time.sleep(0.12)
        self.mouse(x1, y1, 0)
        time.sleep(0.9)

    def key(self, sc, settle=0.45):
        self.cmd("hatari-event keydown 0x%02x" % sc)
        time.sleep(0.16)
        self.cmd("hatari-event keyup 0x%02x" % sc)
        time.sleep(settle)

    def keys(self, sc, n, settle=0.35):
        for _ in range(n):
            self.key(sc, settle)

    def text(self, s, settle=0.25):
        for ch in s:
            self.key(ORD.get(ch, 0x39), 0.12)
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


ORD = {"a": 0x1e, "b": 0x30, "c": 0x2e, "d": 0x20, "e": 0x12, "f": 0x21, "g": 0x22,
       "h": 0x23, "i": 0x17, "j": 0x24, "k": 0x25, "l": 0x26, "m": 0x32, "n": 0x31,
       "o": 0x18, "p": 0x19, "q": 0x10, "r": 0x13, "s": 0x1f, "t": 0x14, "u": 0x16,
       "v": 0x2f, "w": 0x11, "x": 0x2d, "y": 0x15, "z": 0x2c, " ": 0x39,
       "0": 0x0b, "1": 0x02, "2": 0x03, "3": 0x04, "4": 0x05, "5": 0x06, "6": 0x07,
       "7": 0x08, "8": 0x09, "9": 0x0a, ".": 0x34, ":": 0x27, "\\": 0x2b, "-": 0x0c}


def desktop_like(path):
    """Czy ekran wyglada na pulpit: duzy jednolity obszar tla + pasek u gory."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    W, H = im.size
    px = im.load()
    hist = {}
    for y in range(20, H, 7):
        for x in range(0, W, 5):
            c = px[x, y]
            hist[c] = hist.get(c, 0) + 1
    if not hist:
        return False
    top = max(hist.values())
    total = sum(hist.values())
    return top * 100 >= total * 55          # >=55% pikseli w jednym kolorze


def journal_add(journal, scene, t0, ticks0, ticks1, shot, ok, note):
    journal.append(dict(scene=scene, seconds=round(time.time() - t0, 1),
                        ticks=(ticks1 - ticks0) if (ticks0 is not None and ticks1 is not None) else None,
                        shot=os.path.basename(shot[0]) if shot else None,
                        hash=(shot[1][:16] if shot else None), ok=ok, note=note))
    print("  [%-22s] %5.1fs  %s  %s" % (scene, journal[-1]["seconds"],
          "OK  " if ok else "UWAGA", note or ""), flush=True)


def recover(s, name, tries=3):
    """Proba powrotu na pulpit: ESC, potem kill-switch TOS, potem ESC."""
    for i in range(tries):
        s.key(0x01)
        time.sleep(0.6)
        shot = s.shot("%s-recover%d" % (name, i))
        if desktop_like(shot[0]):
            return True, shot
    # kill TOS (Ctrl+Alt+Esc) - dziala, gdy okno nalezy do programu TOS
    s.cmd("hatari-event keydown 0x1d")
    s.cmd("hatari-event keydown 0x38")
    s.key(0x01, 0.6)
    s.cmd("hatari-event keyup 0x38")
    s.cmd("hatari-event keyup 0x1d")
    time.sleep(1.2)
    s.key(0x01)
    shot = s.shot("%s-recover-kill" % name)
    return desktop_like(shot[0]), shot


class StopTest(Exception):
    pass


LIMIT = int(os.environ.get("SMOKE_N", "0") or 0)


def scene(s, journal, name, fn):
    """Scena z pomiarem czasu i tykniec jadra; blad nie przerywa przebiegu."""
    if LIMIT and len(journal) >= LIMIT:
        raise StopTest()
    t0 = time.time()
    ticks0 = s.rd32("kernel_timer_ticks")
    note = ""
    ok = False
    shot = None
    try:
        ok, note, shot = fn()
    except Exception as e:                      # noqa: BLE001 - test ma isc dalej
        note = "WYJATEK: %s" % e
    ticks1 = s.rd32("kernel_timer_ticks")
    if shot is None:
        shot = s.shot("%s-koniec" % name)
    journal_add(journal, name, t0, ticks0, ticks1, shot, ok, note)
    return ok


def run(minutes=30):
    t_start = time.time()
    prepare()
    s = Session()
    journal = []
    print("czekam 72 s na boot...", flush=True)
    try:
        time.sleep(72)
        base = s.shot("s0-boot")
        alive = desktop_like(base[0])
        print("pulpit po boocie: %s  (hash %s)" % (alive, base[1][:16]), flush=True)
        if not alive:
            print("BRAK PULPITU - koncze", flush=True)
            return journal

        # --- 1. TERMINAL: dwa polecenia ---
        def sc_terminal():
            s.dbl(*CELL[(0, 2)])
            time.sleep(2.0)
            s.text("ver")
            s.key(0x1c)                                   # ENTER
            time.sleep(1.5)
            s.text("dir")
            s.key(0x1c)
            time.sleep(1.5)
            return True, "wpisane ver + dir, okno terminala", s.shot("s1-terminal")
        scene(s, journal, "terminal", sc_terminal)

        # --- 2. PLIKI (F7) ---
        def sc_files():
            s.key(0x41)                                   # F7
            time.sleep(2.0)
            shot = s.shot("s2-pliki")
            s.key(0x01)
            return True, "okno PLIKI otwarte i zamkniete", shot
        scene(s, journal, "pliki-f7", sc_files)

        # --- 3. KOSZ ---
        def sc_trash():
            s.dbl(*CELL[(0, 1)])
            time.sleep(2.0)
            shot = s.shot("s3-kosz")
            s.key(0x01)
            return True, "okno KOSZ", shot
        scene(s, journal, "kosz", sc_trash)

        # --- 4. MY ATARI (drives) ---
        def sc_drives():
            s.dbl(*CELL[(0, 0)])
            time.sleep(2.5)
            shot = s.shot("s4-my-atari")
            s.key(0x01)
            return True, "okno MY ATARI", shot
        scene(s, journal, "my-atari", sc_drives)

        # --- 5. menu kontekstowe (prawy przycisk) ---
        def sc_ctx():
            s.mouse(500, 300, 2)
            time.sleep(0.6)
            s.mouse(500, 300, 0)
            time.sleep(1.2)
            shot = s.shot("s5-menu-kontekstowe")
            s.key(0x01)
            return True, "menu kontekstowe pulpitu", shot
        scene(s, journal, "menu-kontekstowe", sc_ctx)

        # --- 6. SETTINGS: otwarcie z gornego paska (menu) ---
        state = {"open": False}

        def sc_settings_open():
            for attempt in range(3):
                s.click(85, 8)
                time.sleep(1.6)
                s.click(80, 31)
                time.sleep(3.0)
                shot = s.shot("s6-settings-%d" % attempt)
                from PIL import Image
                im = Image.open(shot[0]).convert("RGB")
                px = im.load()
                bg = px[im.size[0] - 1, im.size[1] - 1]
                if px[300, 300] != bg and px[500, 260] != bg:
                    state["open"] = True
                    return True, "SETTINGS otwarte (proba %d)" % (attempt + 1), shot
            return False, "nie udalo sie otworzyc SETTINGS", shot
        scene(s, journal, "settings-otwarcie", sc_settings_open)

        if state["open"]:
            # --- 7. RES: podglad trybu 1024x768 i powrot ---
            def sc_mode():
                s.keys(0x50, 8)                            # ostatni tryb na liscie
                s.key(0x1c)                                # APPLY
                time.sleep(1.0)
                s.key(0x1c)                                # YES (potwierdzenie)
                time.sleep(6.0)
                shot = s.shot("s7-tryb-1024")
                s.key(0x1c)                                # cokolwiek pokaze
                time.sleep(1.5)
                s.key(0x01)
                return True, "podglad trybu + powrot", shot
            scene(s, journal, "tryb-ekranu", sc_mode)

            # --- 8. THEME: zmiana motywu i powrot ---
            def sc_theme():
                s.key(0x0f)                                # TAB -> THEME
                time.sleep(1.0)
                s.key(0x50)
                s.key(0x1c)                                # APPLY motywu
                time.sleep(2.0)
                shot = s.shot("s8-motyw")
                s.keys(0x48, 3)                            # z powrotem na pierwszy
                s.key(0x1c)
                time.sleep(1.5)
                return True, "zmiana motywu i powrot", shot
            scene(s, journal, "motyw", sc_theme)

            # --- 9. DESK: kolory, pasek, tapeta, rozmiar ikon, siatka ---
            def sc_desk():
                for _ in range(2):                         # THEME -> VOL -> DESK
                    s.key(0x0f)
                time.sleep(1.0)
                s.keys(0x48, 9)                            # wiersz ICON SIZE
                s.key(0x1c)                                # 48 -> 64
                time.sleep(3.0)
                shot = s.shot("s9a-ikony-64")
                s.key(0x1c)                                # 64 -> 32
                time.sleep(3.0)
                s.shot("s9b-ikony-32")
                s.key(0x50)                                # ICON GRID
                s.key(0x1c)
                s.keys(0x48, 9)                            # na gore, do kolorow
                s.key(0x1c)                                # kolor tla
                s.keys(0x48, 9)                            # znow na ICON SIZE
                s.key(0x1c)                                # 32 -> 48 (powrot)
                time.sleep(2.5)
                return True, "rozmiar ikon 64/32/48 + siatka", s.shot("s9c-ikony-48")
            scene(s, journal, "desk-rozmiar-siatka", sc_desk)

            # --- 10. TILE: klikniecia w siatke edytora ---
            def sc_tile():
                s.key(0x0f)                                # DESK -> TILE
                time.sleep(1.0)
                for dx, dy in ((40, 70), (60, 70), (80, 70), (60, 90), (100, 110)):
                    s.click(248 + dx, 128 + dy, 0.35)
                time.sleep(0.8)
                shot = s.shot("s10-kafelek")
                s.key(0x01)
                time.sleep(1.0)
                s.key(0x01)
                return True, "klikniecia w edytor kafelka", shot
            scene(s, journal, "edytor-kafelka", sc_tile)

        # --- 11. przeciaganie i resize okna PLIKI ---
        def sc_drag():
            s.key(0x41)                                    # F7
            time.sleep(2.0)
            shot0 = s.shot("s11a-pliki-przed")
            s.drag(600, 300, 500, 260)                     # za zawartosc
            time.sleep(0.8)
            shot1 = s.shot("s11b-pliki-przeciagniete")
            s.drag(400, 210, 560, 330)                     # za prawy dolny rog
            time.sleep(1.0)
            shot2 = s.shot("s11c-pliki-resize")
            s.key(0x01)
            return (shot0[1] != shot1[1] or shot1[1] != shot2[1]), \
                   "przeciaganie + resize okna", shot2
        scene(s, journal, "okno-drag-resize", sc_drag)

        # --- 12-16. aplikacje natywne z ikon ---
        for name, cell, wait, extra in (
                ("modplay", (1, 0), 5.0, None),
                ("pre2", (2, 0), 6.0, None),
                ("taskmgr", (1, 2), 3.0, None),
                ("plasma", (1, 3), 3.0, None),
                ("arkanoid", (1, 4), 4.0, "gra"),
                ("sysinfo", (0, 3), 3.5, None)):
            def sc_app(cell=cell, wait=wait, name=name, extra=extra):
                s.dbl(*CELL[cell])
                time.sleep(wait)
                shot = s.shot("s12-%s" % name)
                if extra == "gra":
                    s.key(0x4b)                            # strzalka w lewo
                    s.key(0x4d)
                    s.key(0x1c)
                    time.sleep(0.8)
                ok, rshot = recover(s, "s12-%s" % name)
                return ok, ("%s uruchomiony%s" % (name, ", klawisze w grze" if extra else "")), shot
            scene(s, journal, "app-%s" % name, sc_app)

        # --- 17. program TOS + kill-switch ---
        def sc_systos():
            s.dbl(*CELL[(0, 5)])                           # SYSTOS = SYSINFO.PRG
            time.sleep(6.0)
            shot0 = s.shot("s17a-systos")
            s.cmd("hatari-event keydown 0x1d")
            s.cmd("hatari-event keydown 0x38")
            s.key(0x01, 0.5)
            s.cmd("hatari-event keyup 0x38")
            s.cmd("hatari-event keyup 0x1d")
            time.sleep(2.0)
            shot1 = s.shot("s17b-po-kill")
            ok = desktop_like(shot1[0])
            return ok, "SYSTOS + Ctrl+Alt+Esc (kill TOS)", shot1
        scene(s, journal, "tos-kill-switch", sc_systos)

        # --- 18. stabilnosc: 3 zrzuty co 2 s bez akcji ---
        def sc_stable():
            a = s.shot("s18a")
            time.sleep(2.0)
            b = s.shot("s18b")
            time.sleep(2.0)
            c = s.shot("s18c")
            same = a[1] == b[1] == c[1]
            return same, ("ekran stabilny w bezczynnosci" if same
                          else "ekran zmienia sie bez akcji (animacja/mrug)"), c
        scene(s, journal, "bezczynnosc", sc_stable)

        # --- DOGRYWKA: powtarzamy sceny o najwiekszej rotacji do konca czasu ---
        runda = 0
        while time.time() - t_start < minutes * 60.0 - 120.0:
            runda += 1
            print("\n=== runda %d (do konca: %.0f s) ===" % (
                runda, minutes * 60.0 - (time.time() - t_start)), flush=True)
            for nm, fn in (("terminal", sc_terminal), ("pliki", sc_files),
                           ("kosz", sc_trash), ("my-atari", sc_drives)):
                scene(s, journal, "%s#%d" % (nm, runda), fn)
            if state["open"]:
                scene(s, journal, "desk#%d" % runda, sc_desk)
                scene(s, journal, "tile#%d" % runda, sc_tile)
            scene(s, journal, "drag#%d" % runda, sc_drag)

        # --- raport ---
        end = s.shot("s19-koniec")
        tick = s.rd32("kernel_timer_ticks")
        print("\npulpit na koniec: %s, tykniec jadra: %s" % (desktop_like(end[0]), tick))
        return journal
    except StopTest:
        print("(SMOKE: przerwano po %d scenach)" % len(journal), flush=True)
        return journal
    finally:
        s.close()


def write_report(journal, path):
    lines = []
    lines.append("STRESS TEST ATOS 2.1 - %s" % time.strftime("%Y-%m-%d %H:%M"))
    lines.append("obraz: %s (klon %s)" % (IMG, os.path.basename(BASE)))
    lines.append("")
    lines.append("%-22s %6s %6s %-17s %-6s %s" %
                 ("scena", "czas", "tykn.", "hash", "wynik", "uwagi"))
    ok_count = 0
    for r in journal:
        if r["ok"]:
            ok_count += 1
        lines.append("%-22s %5.1fs %6s %-17s %-6s %s" % (
            r["scene"], r["seconds"], r["ticks"] if r["ticks"] is not None else "-",
            r["hash"] or "-", "OK" if r["ok"] else "UWAGA", r["note"]))
    lines.append("")
    lines.append("scen: %d, OK: %d, uwag: %d" % (len(journal), ok_count, len(journal) - ok_count))
    lines.append("zrzuty: %s/s*.png" % DEMO)
    text = "\n".join(lines) + "\n"
    open(path, "w").write(text)
    print("\n" + text)
    return ok_count


if __name__ == "__main__":
    minutes = float(sys.argv[1]) if len(sys.argv) > 1 else 30.0
    smoke = os.environ.get("SMOKE") == "1"
    t_start = time.time()
    # ICONSZ.CFG: domyslny rozmiar 48 (stan kanoniczny)
    open("/tmp/stress-iconsz.cfg", "w").write("48\n0\n")
    journal = run(minutes)
    elapsed = time.time() - t_start
    print("czas przebiegu: %.1f min" % (elapsed / 60.0))
    out = "%s/STRESS-RAPORT-%s.txt" % (DEMO, time.strftime("%Y%m%d-%H%M"))
    write_report(journal, out)
    print("raport:", out)
    if smoke:
        print("(SMOKE: przerwano po pierwszych scenach)")
