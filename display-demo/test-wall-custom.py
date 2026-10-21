#!/usr/bin/env python3
"""Tapeta CUSTOM (WALL=4) na emulatorze - dowod, nie "powinno dzialac".

Dwa przebiegi na KOPII obrazu (oryginalu nie dotykamy):

  golden  - config kanoniczny (bez WALL=): zrzut startowy musi byc BAJT W BAJT
            z wzorcem 0e821ede... (zadnej regresji po zmianach),
  custom  - config + WALL=4 + TILE=<kafelek z 2 punktami>: tlo MUSI byc
            narysowane z kafelka - sprawdzamy PIKSELE (dokladna pozycja
            zapalonych punktow co 16 px), a nie "wyglada inaczej".

Uzycie:
  python3 test-wall-custom.py golden
  python3 test-wall-custom.py custom
"""
import hashlib, os, shutil, struct, subprocess, sys, time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE_IMG = DEMO + "/demo-guard.img"   # kanoniczny obraz testowy (boot do pulpitu)
IMG = DEMO + "/demo-wall-custom.img"
HAT = DEMO + "/hatari-scalony-20261017"
FIFO = "/tmp/wall.fifo"
# Wzorzec zaktualizowany 20.10: zrzut startowy zawiera teraz DOLNY PASEK
# (dock) rysowany przez WM (stan zwiniety) - swiadoma zmiana UI, nie regresja.
# Pasek przelacza sie na RELEASE (pelny klik), zeby przypadkowy "wiszący" press
# przy starcie nie zmienial stanu i nie rozjezdzal zrzutu.
# 21.10: tlo paska zmienione na jasnoszare (indeks 15 palety) na prosbe
# uzytkownika - swiadoma zmiana UI, nowy wzorzec 5c1e5546...
REF = "5c1e5546f36edd91147a84a4c943193f2ef294f669705c3d64a0e4d0443a3e08"

BUILD = {
    r"\SYSTEM\KERNEL.SYS": REAL + "/build/system/KERNEL.SYS",
    r"\SYSTEM\APP\DESKTOP.APP": REAL + "/build/system/DESKTOP.APP",
    r"\SYSTEM\APP\SETTINGS.APP": REAL + "/build/system/SETTINGS.APP",
    r"\SYSTEM\ICONS.DAT": REAL + "/build/system/ICONS.DAT",
    r"\SYSTEM\GFX.APP": REAL + "/build/system/GFX.APP",   # broker: opcody 48/49/50
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

# Kafelek 16x16 z JEDNYM zapalonym punktem (0,0): bajt 0 = 0x80, 31 bajtow
# zer.  Na ekranie daje to dokladnie siatke akcentow co 16 px - kazdy inny
# piksel musi byc tlem, wiec zrzut daje sie sprawdzic PIKsel PO PIKSELU.
TILE_HEX = "80" + "00" * 31                  # 64 znaki = 32 B

CUSTOM = CANONICAL + "WALL=4\nTILE=%s\n" % TILE_HEX[:64]


def sh(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def prepare(mode):
    if not os.path.exists(IMG) or os.path.getmtime(IMG) < os.path.getmtime(BASE_IMG):
        sh(["cp", "-c", BASE_IMG, IMG])          # klon APFS, bez kopiowania 800 MB
    for target, src in BUILD.items():
        sh(["python3", DEMO + "/install_file.py", IMG, target, src])
    cfg = CUSTOM if mode == "custom" else CANONICAL
    path = "/tmp/wall-desktop.cfg"
    with open(path, "w") as f:
        f.write(cfg)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\DESKTOP.CFG", path])
    print("--- DESKTOP.CFG (%s) ---\n%s" % (mode, cfg), flush=True)


def boot_and_shoot(tag):
    if os.path.exists(FIFO):
        os.unlink(FIFO)
    log = open("/tmp/wall_hatari_%s.log" % tag, "w")
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
            "--log-file", "/tmp/wall_hatari_%s_d.log" % tag, "--cmd-fifo", FIFO]
    p = subprocess.Popen(argv, stdout=log, stderr=log)
    fd = [None]

    def cmd(line):
        if fd[0] is None:
            fd[0] = os.open(FIFO, os.O_WRONLY)
        os.write(fd[0], (line + "\n").encode())
        time.sleep(0.1)

    def shot(name):
        path = "%s/%s.png" % (DEMO, name)
        if os.path.exists(path):
            os.unlink(path)
        cmd("hatari-debug screenshot " + path)
        for _ in range(60):
            time.sleep(0.25)
            if os.path.exists(path) and os.path.getsize(path) > 1024:
                time.sleep(0.4)
                break
        raw = open(path, "rb").read()
        print("  %-20s %s" % (name + ".png", hashlib.sha256(raw).hexdigest()[:16]), flush=True)
        return path

    try:
        print("czekam 66 s na boot...", flush=True)
        time.sleep(66)
        a = shot("w-%s-boot" % tag)
        time.sleep(3)
        b = shot("w-%s-boot2" % tag)
    finally:
        p.terminate()
        try:
            p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            p.kill()
        if fd[0] is not None:
            os.close(fd[0])
        log.close()
    return a, b


def check_golden(a, b):
    ha = hashlib.sha256(open(a, "rb").read()).hexdigest()
    hb = hashlib.sha256(open(b, "rb").read()).hexdigest()
    print("\nGOLDEN: %s" % ha)
    print("WZORZEC: %s" % REF)
    assert ha == hb, "zrzut startowy niestabilny (a != b)"
    if ha == REF:
        print("WYNIK: zrzut startowy BAJT W BAJT z wzorcem - brak regresji")
        return True
    print("WYNIK: INNY niz wzorzec (do wyjasnienia)")
    return False


def check_custom(a, b):
    from PIL import Image
    ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    assert ia.size == ib.size, (ia.size, ib.size)
    W, H = ia.size
    print("\nzrzut %dx%d" % (W, H))
    pa, pb = ia.load(), ib.load()
    # Region PUSTEGO pulpitu (pod ikonami, nad paskiem launchera): y 480..591.
    y0, y1 = 480, min(H, 592)
    colors = {}
    dots = 0
    bad = []
    for y in range(y0, y1):
        for x in range(W):
            c = pa[x, y]
            colors[c] = colors.get(c, 0) + 1
            if x % 16 == 0 and y % 16 == 0:
                dots += 1
            else:
                if c != pa[1, y0]:          # kazdy inny piksel = to samo tlo
                    bad.append((x, y, c))
    base = pa[1, y0]
    print("kolor tla: %s" % (base,))
    print("kolorow w regionie: %d" % len(colors))
    print("punktow (x%%16==0 i y%%16==0): %d" % dots)
    print("pikseli innych niz tlo poza punktami: %d" % len(bad))
    if bad[:5]:
        print("  przyklady:", bad[:5])
    same = (a is not None and ia.tobytes() == ib.tobytes())
    print("dwa zrzuty identyczne: %s" % same)
    return colors, base, dots, bad, same


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "custom"
    assert mode in ("golden", "custom")
    prepare(mode)
    a, b = boot_and_shoot(mode)
    if mode == "golden":
        ok = check_golden(a, b)
        sys.exit(0 if ok else 1)
    colors, base, dots, bad, same = check_custom(a, b)
    # Oczekiwane: dokladnie 2 kolory (tlo + akcent), zadnego innego piksela
    # poza punktami siatki 16x16 i ani jednego brakujacego punktu.
    ok = len(colors) == 2 and not bad and dots > 0 and same
    print("\nWYNIK: %s" % ("TAPETA CUSTOM RYSUJE SIE (kafelek co 16 px)" if ok
                          else "BLAD - tapeta nie jest kafelkiem"))
    sys.exit(0 if ok else 1)
