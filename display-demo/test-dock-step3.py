#!/usr/bin/env python3
"""DOCK krok 3: skroty z LAUNCH.CFG na lewej stronie paska.

Sprawdza na emulatorze (kopia obrazu, oryginalu nie dotykamy):

  1. pasek startuje ZWINIETY, a klik w (20,580) rozwija go (jest 16 px ikon
     w slotach, nie zaslepki),
  2. jadro przeczytalo \\SYSTEM\\LAUNCH.CFG: etykiety/sciezki w `dock_short`,
     builtin `*TERMINAL` pominiety, opcjonalne `|NN` wybiera ikone,
  3. najechanie bez klikania rysuje nazwe NAD paskiem, a zjechanie ja zmazuje,
  4. klik w slot uruchamia program (`kdbg_dock_launch`, `kdbg_dock_rc == 0`),
  5. klik w puste miejsce paska nie przelatuje do okna pod spodem i nie zmienia
     stanu paska; hamburger zamyka pasek.

Uzycie:  python3 test-dock-step3.py
"""
import os
import subprocess
import sys
import time

from PIL import Image

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = DEMO + "/demo-guard.img"       # kanoniczny obraz testowy (boot do pulpitu)
IMG = DEMO + "/demo-dock3.img"

sys.path.insert(0, DEMO)
import stress_atos as st             # Session: inject/rd32/memdump_bytes/shot
st.IMG = IMG                         # Session bierze obraz z tego globala

BUILD = {
    r"\SYSTEM\KERNEL.SYS": REAL + "/build/system/KERNEL.SYS",
    r"\SYSTEM\APP\DESKTOP.APP": REAL + "/build/system/DESKTOP.APP",
    r"\SYSTEM\APP\SETTINGS.APP": REAL + "/build/system/SETTINGS.APP",
    r"\SYSTEM\GFX.APP": REAL + "/build/system/GFX.APP",
    r"\SYSTEM\ICONS.DAT": REAL + "/build/system/ICONS.DAT",
    r"\SYSTEM\APP\AC.APP": REAL + "/build/system/AC.APP",
    r"\SYSTEM\APP\SYSINFO.APP": REAL + "/build/system/SYSINFO.APP",
    r"\SYSTEM\APP\TASKMGR.APP": REAL + "/build/system/TASKMGR.APP",
}

# Celowo: komentarz, builtin (do pominiecia), opcjonalne |NN i trzy realne
# programy - czyli wiecej niz "min. 3 skroty" z definicji gotowego docka.
LAUNCH = """; test docka kroku 3
COMMANDER=\\SYSTEM\\APP\\AC.APP
TERMINAL=*TERMINAL
SYSINFO=\\SYSTEM\\APP\\SYSINFO.APP|24
TASKMGR=\\SYSTEM\\APP\\TASKMGR.APP
"""

CANONICAL = open(DEMO + "/desktop-cfg-wzor.txt").read()

DOCK_ENTRY = 78                      # label[13] + path[64] + icon (uint8)
HAMBURGER = (20, 580)                # zwiniety pasek 24x24: x=8..31, y=574..597
SLOT0 = (43, 586)                    # srodek pierwszego slotu (x=32..55)
EMPTY = (400, 586)                   # puste miejsce w otwartym pasku
AWAY = (400, 300)                    # kursor poza paskiem


def sh(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def prepare():
    if os.path.exists(IMG):
        os.unlink(IMG)
    sh(["cp", "-c", BASE, IMG])                 # klon APFS, bez kopiowania 800 MB
    for target, src in BUILD.items():
        sh(["python3", DEMO + "/install_file.py", IMG, target, src])
    open("/tmp/dock3-desktop.cfg", "w").write(CANONICAL)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\DESKTOP.CFG",
        "/tmp/dock3-desktop.cfg"])
    open("/tmp/dock3-launch.cfg", "w").write(LAUNCH)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\LAUNCH.CFG",
        "/tmp/dock3-launch.cfg"])


def region_difference(path_a, path_b, box, threshold=24):
    """Ile pikseli rozni sie w prostokacie (x0,y0,x1,y1) miedzy zrzutami."""
    a = Image.open(path_a).convert("RGB")
    b = Image.open(path_b).convert("RGB")
    assert a.size == b.size, (a.size, b.size)
    pa, pb = a.load(), b.load()
    diff = 0
    for y in range(box[1], box[3]):
        for x in range(box[0], box[2]):
            ca, cb = pa[x, y], pb[x, y]
            if (abs(ca[0] - cb[0]) + abs(ca[1] - cb[1]) + abs(ca[2] - cb[2])) >= threshold:
                diff += 1
    return diff


def icons_in_box(path, box):
    """(kolory, piksele inne niz tlo paska) w prostokacie z ikona."""
    image = Image.open(path).convert("RGB")
    pixels = image.load()
    background = pixels[300, 586]               # puste pole paska = tlo
    colors, painted = set(), 0
    for y in range(box[1], box[3]):
        for x in range(box[0], box[2]):
            color = pixels[x, y]
            colors.add(color)
            if color != background:
                painted += 1
    return colors, painted


def read_shortcuts(session, count):
    raw = session.memdump_bytes("dock_short", DOCK_ENTRY * count)
    assert len(raw) >= DOCK_ENTRY * count, "memdump dock_short niepelny: %d B" % len(raw)
    out = []
    for i in range(count):
        base = i * DOCK_ENTRY
        label = bytes(raw[base:base + 13]).split(b"\0")[0].decode("latin-1")
        path = bytes(raw[base + 13:base + 77]).split(b"\0")[0].decode("latin-1")
        out.append((label, path, raw[base + 77]))
    return out


def main():
    prepare()
    session = st.Session()
    ok = True
    try:
        print("czekam 70 s na boot...", flush=True)
        time.sleep(70)
        closed, _ = session.shot("dock3-closed")

        # ---- 1. otwarcie paska -------------------------------------------
        session.click_fast(*HAMBURGER)
        time.sleep(0.8)
        click = session.rd32("kdbg_dock_click")
        opened = session.rd32("kdbg_dock_open")
        slots = session.rd32("kdbg_dock_slots")
        print("kdbg_dock_click=%s open=%s slots=%s" % (click, opened, slots), flush=True)
        if not (click and click >= 1 and opened == 1):
            print("BLAD: klik w hamburger nie otworzyl paska", flush=True)
            ok = False
        if slots != 3:
            print("BLAD: oczekiwano 3 skrotow (builtin pominiety), jest %s" % slots, flush=True)
            ok = False

        # ---- 2. LAUNCH.CFG w pamieci jadra -------------------------------
        if slots == 3:
            entries = read_shortcuts(session, 3)
            print("dock_short: %r" % (entries,), flush=True)
            expected = [
                ("COMMANDER", "\\SYSTEM\\APP\\AC.APP", 76),
                ("SYSINFO", "\\SYSTEM\\APP\\SYSINFO.APP", 24),
                ("TASKMGR", "\\SYSTEM\\APP\\TASKMGR.APP", 76),
            ]
            for got, want in zip(entries, expected):
                if got != want:
                    print("BLAD: slot %r != %r" % (got, want), flush=True)
                    ok = False

        # ---- 3. ikony 16 px, nie zaslepki --------------------------------
        session.inject(*AWAY, 0)
        time.sleep(0.6)
        opened_shot, _ = session.shot("dock3-open")
        colors, painted = icons_in_box(opened_shot, (36, 578, 52, 594))
        print("ikona slotu 0: %d kolorow, %d pikseli ponad tlo" % (len(colors), painted), flush=True)
        if len(colors) < 3 or painted < 20:
            print("BLAD: slot 0 nie ma ikony 16 px (zaslepka?)", flush=True)
            ok = False

        # ---- 4. najechanie bez klikania = nazwa -------------------------
        session.inject(*SLOT0, 0)
        time.sleep(0.8)
        tip_shot, _ = session.shot("dock3-tip")
        # Etykieta "COMMANDER" (9 znakow) jest wysrodkowana nad slotem 0:
        # x 27..99, y 553..571.  Porownujemy z otwartym paskiem bez podpowiedzi.
        diff = region_difference(opened_shot, tip_shot, (24, 548, 108, 573))
        print("podpowiedz nad slotem 0: %d roznych pikseli" % diff, flush=True)
        if diff < 150:
            print("BLAD: najechanie nie narysowalo nazwy", flush=True)
            ok = False

        # zjechanie = podpowiedz zmazana (tlo wraca)
        session.inject(*AWAY, 0)
        time.sleep(0.8)
        back_shot, _ = session.shot("dock3-notip")
        diff = region_difference(opened_shot, back_shot, (24, 548, 108, 573))
        print("po zjechaniu: %d roznych pikseli (ma byc ~0)" % diff, flush=True)
        if diff > 40:
            print("BLAD: podpowiedz zostala na ekranie", flush=True)
            ok = False

        # ---- 5. klik w slot uruchamia program ---------------------------
        launch_before = session.rd32("kdbg_dock_launch") or 0
        session.click_fast(*SLOT0)
        time.sleep(5.0)
        launch_after = session.rd32("kdbg_dock_launch") or 0
        rc = session.rd32("kdbg_dock_rc")
        print("kdbg_dock_launch %d -> %d, kdbg_dock_rc=%s" %
              (launch_before, launch_after, rc), flush=True)
        if launch_after != launch_before + 1 or rc != 0:
            print("BLAD: klik w skrot nie uruchomil programu", flush=True)
            ok = False
        session.inject(*AWAY, 0)
        time.sleep(0.6)
        launched_shot, _ = session.shot("dock3-launch")
        # AC.APP otwiera okno na srodku: obraz musi sie roznic od pustego pulpitu.
        diff = region_difference(opened_shot, launched_shot, (150, 120, 650, 450))
        print("okno po starcie: %d roznych pikseli" % diff, flush=True)
        if diff < 5000:
            print("BLAD: brak okna po uruchomieniu z paska", flush=True)
            ok = False

        # ---- 6. puste miejsce nie klika w okno pod paskiem -------------
        open_before = session.rd32("kdbg_dock_open")
        session.click_fast(*EMPTY)
        time.sleep(0.6)
        open_after = session.rd32("kdbg_dock_open")
        clicks = session.rd32("kdbg_dock_click")
        print("puste miejsce: open %s -> %s, click=%s" % (open_before, open_after, clicks), flush=True)
        if open_after != 1:
            print("BLAD: klik w puste miejsce zamknal pasek", flush=True)
            ok = False

        # ---- 7. hamburger zamyka ---------------------------------------
        session.click_fast(*HAMBURGER)
        time.sleep(0.8)
        closed_again = session.rd32("kdbg_dock_open")
        print("po hamburgerze: open=%s" % closed_again, flush=True)
        if closed_again != 0:
            print("BLAD: hamburger nie zamknal paska", flush=True)
            ok = False
        session.shot("dock3-closed-again")
    finally:
        session.proc.terminate()
        try:
            session.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            session.proc.kill()
        if session.fd is not None:
            os.close(session.fd)
        session.log.close()

    print("\nWYNIK: %s" % ("DOCK KROK 3 DZIALA" if ok else "BLAD - patrz wyzej"), flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
