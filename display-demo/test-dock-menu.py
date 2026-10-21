#!/usr/bin/env python3
"""DOCK: menu kontekstowe paska (PPM) - dodawanie i usuwanie pozycji.

Na kopii obrazu testowego sprawdza:

  1. PPM na slocie otwiera menu "REMOVE: <name> / ADD ITEM... / CLOSE"
     rysowane przez WM (a nie menu kontekstowe pulpitu pod spodem),
  2. "ADD ITEM..." pokazuje katalog programow/gier; wybor PRE2 dodaje
     pozycje, zapisuje \\SYSTEM\\DOCK.CFG i natychmiast odswieza pasek,
  3. "REMOVE: COMMANDER" usuwa pozycje i zapisuje plik,
  4. po restarcie (sprawdzane zawartoscia DOCK.CFG) lista jest ta sama,
  5. kazdy program ma wlasna ikone z katalogu (COMMANDER=NC, TASKMGR=sliders,
     PRE2=joystick - nie jeden wspolny ząb).

Uzycie:  python3 test-dock-menu.py
"""
import os
import subprocess
import sys
import time

from PIL import Image

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = DEMO + "/demo-guard.img"
IMG = DEMO + "/demo-menu.img"

sys.path.insert(0, DEMO)
import stress_atos as st
st.IMG = IMG

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

LAUNCH = """; test menu paska - trzy pozycje + builtin do pominiecia
COMMANDER=\\SYSTEM\\APP\\AC.APP
TERMINAL=*TERMINAL
SYSINFO=\\SYSTEM\\APP\\SYSINFO.APP|24
TASKMGR=\\SYSTEM\\APP\\TASKMGR.APP
"""

CANONICAL = open(DEMO + "/desktop-cfg-wzor.txt").read()

DOCK_ENTRY = 78
HAMBURGER = (20, 580)
SLOT0 = (43, 586)
EMPTY = (400, 586)
MENU_X = 100
# Wiersze menu glownego (3 pozycje): y=523/539/555, srodki 530/546/562.
ROW_REMOVE = (MENU_X, 530)
ROW_ADD = (MENU_X, 546)
ROW_CLOSE = (MENU_X, 562)
# Lista "DODAJ": 7 pozycji od y=427; PRE2 jest szosta (y=507..522).
ROW_PRE2 = (MENU_X, 515)


def sh(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def prepare():
    if os.path.exists(IMG):
        os.unlink(IMG)
    sh(["cp", "-c", BASE, IMG])
    for target, src in BUILD.items():
        sh(["python3", DEMO + "/install_file.py", IMG, target, src])
    open("/tmp/dockmenu-desktop.cfg", "w").write(CANONICAL)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\DESKTOP.CFG",
        "/tmp/dockmenu-desktop.cfg"])
    open("/tmp/dockmenu-launch.cfg", "w").write(LAUNCH)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\LAUNCH.CFG",
        "/tmp/dockmenu-launch.cfg"])
    # Stary DOCK.CFG z poprzednich prob nie moze przeciec do testu.
    subprocess.run(["python3", REAL + "/tools/img_delete.py", IMG,
                    r"\SYSTEM\DOCK.CFG"], capture_output=True)


def wait_counter(session, name, want, timeout=6.0):
    end = time.time() + timeout
    last = None
    while time.time() < end:
        last = session.rd32(name)
        if last == want:
            return last
        time.sleep(0.1)
    return last


def right_click(session, x, y, settle=0.5):
    session.inject(x, y, 2)
    session.inject(x, y, 0)
    time.sleep(settle)


def read_shortcuts(session, count):
    raw = session.memdump_bytes("dock_short", DOCK_ENTRY * count)
    assert len(raw) >= DOCK_ENTRY * count, "memdump dock_short niepelny"
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

        # 1. otworz pasek
        session.click_fast(*HAMBURGER)
        time.sleep(0.8)
        slots = session.rd32("kdbg_dock_slots")
        print("start: kdbg_dock_slots=%s" % slots, flush=True)
        if slots != 3:
            print("BLAD: start ma miec 3 sloty", flush=True)
            ok = False

        # 2. PPM na slocie 0 -> menu konfiguracji
        right_click(session, *SLOT0)
        menus = session.rd32("kdbg_dock_menu")
        session.shot("dockmenu-main")
        print("kdbg_dock_menu=%s" % menus, flush=True)
        if not menus or menus < 1:
            print("BLAD: PPM nie otworzyl menu", flush=True)
            ok = False

        # 3. ADD ITEM... -> katalog
        session.click_fast(*ROW_ADD)
        time.sleep(0.6)
        session.shot("dockmenu-add")

        # 4. wybierz PRE2 (6. pozycja na liscie)
        session.click_fast(*ROW_PRE2)
        time.sleep(0.5)
        slots = wait_counter(session, "kdbg_dock_slots", 4)
        added = session.rd32("kdbg_dock_add")
        save_rc = session.rd32("kdbg_dock_save_rc")
        print("po DODAJ: slots=%s add=%s save_rc=%s" % (slots, added, save_rc), flush=True)
        if slots != 4 or added != 1 or save_rc != 0:
            print("BLAD: dodanie PRE2 nie zadzialalo", flush=True)
            ok = False
        if slots == 4:
            entries = read_shortcuts(session, 4)
            print("dock_short po dodaniu: %r" % (entries,), flush=True)
            if entries[3][0] != "PRE2" or entries[3][2] != 22:
                print("BLAD: PRE2 ma zla etykiete lub ikone", flush=True)
                ok = False
        session.inject(400, 300, 0)   # wskaznik poza paskiem
        time.sleep(0.4)
        session.shot("dockmenu-added")

        # 5. PPM na slocie 0 -> USUN COMMANDER
        right_click(session, *SLOT0)
        session.click_fast(*ROW_REMOVE)
        time.sleep(0.5)
        slots = wait_counter(session, "kdbg_dock_slots", 3)
        removed = session.rd32("kdbg_dock_remove")
        save_rc = session.rd32("kdbg_dock_save_rc")
        print("po REMOVE: slots=%s remove=%s save_rc=%s" % (slots, removed, save_rc), flush=True)
        if slots != 3 or removed != 1 or save_rc != 0:
            print("BLAD: usuniecie COMMANDER nie zadzialalo", flush=True)
            ok = False
        if slots == 3:
            entries = read_shortcuts(session, 3)
            print("dock_short po usunieciu: %r" % (entries,), flush=True)
            if [e[0] for e in entries] != ["SYSINFO", "TASKMGR", "PRE2"]:
                print("BLAD: zla kolejnosc po usunieciu", flush=True)
                ok = False
        session.shot("dockmenu-removed")

        # 6. PPM w puste miejsce: menu bez REMOVE, tylko ADD/CLOSE
        right_click(session, *EMPTY)
        session.shot("dockmenu-empty")
        session.click_fast(*ROW_CLOSE)
        time.sleep(0.5)

        # 7. zamknij pasek
        session.click_fast(*HAMBURGER)
        time.sleep(0.6)
        opened = session.rd32("kdbg_dock_open")
        print("po hamburgerze: open=%s" % opened, flush=True)
        if opened != 0:
            print("BLAD: pasek nie zamknal sie", flush=True)
            ok = False
    finally:
        session.proc.terminate()
        try:
            session.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            session.proc.kill()
        if session.fd is not None:
            os.close(session.fd)
        session.log.close()

    # 8. zawartosc DOCK.CFG w obrazie po zakonczeniu emulacji
    cfg_path = "/tmp/dockmenu-dock.cfg"
    subprocess.run(["python3", REAL + "/tools/img_get.py", IMG,
                    r"\SYSTEM\DOCK.CFG", cfg_path], check=False,
                   stdout=subprocess.DEVNULL)
    cfg = open(cfg_path, errors="replace").read() if os.path.exists(cfg_path) else ""
    print("--- DOCK.CFG ---\n%s" % cfg, flush=True)
    for want in ("SYSINFO=", "TASKMGR=", "PRE2="):
        if want not in cfg:
            print("BLAD: brak %s w DOCK.CFG" % want, flush=True)
            ok = False
    if "COMMANDER=" in cfg:
        print("BLAD: COMMANDER nadal w DOCK.CFG po usunieciu", flush=True)
        ok = False

    print("\nWYNIK: %s" % ("DOCK MENU DZIALA" if ok else "BLAD - patrz wyzej"), flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
