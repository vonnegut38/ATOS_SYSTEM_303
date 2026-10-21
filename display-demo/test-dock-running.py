#!/usr/bin/env python3
"""DOCK krok 4: uruchomione aplikacje po prawej stronie paska.

Sprawdza na emulatorze (kopia obrazu):

  1. start PLASMA z lewego skrotu,
  2. minimalizacja okna (testowy hook kdbg_test_minimize) -> na prawym koncu
     paska pojawia sie ikona PLASMA z mala zebatka; kdbg_dock_running=1,
  3. klik w te ikone PRZYWRACA okno (kdbg_dock_restore), a nie startuje kopii,
  4. po kolejnej minimalizacji klik w LEWY skrot PLASMA tez tylko przywraca:
     kdbg_dock_launch pozostaje 1, kdbg_dock_restore rosnie,
  5. po zwinięciu paska na hamburgerze widac mala zebatke (coś działa),
     mimo ze pasek jest zamkniety.

Uzycie:  python3 test-dock-running.py
"""
import os
import subprocess
import sys
import time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = DEMO + "/demo-guard.img"
IMG = DEMO + "/demo-run.img"

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
    r"\SYSTEM\APP\PLASMA.APP": REAL + "/build/system/PLASMA.APP",
}

LAUNCH = """; test znaczkow: SYSINFO ma skrot (lewy), PLASMA tylko jako proces
COMMANDER=\\SYSTEM\\APP\\AC.APP
SYSINFO=\\SYSTEM\\APP\\SYSINFO.APP|24
TASKMGR=\\SYSTEM\\APP\\TASKMGR.APP
"""

CANONICAL = open(DEMO + "/desktop-cfg-wzor.txt").read()

HAMBURGER = (20, 580)
SYSINFO_SLOT = (67, 586)       # lewy slot 1: x=56..79
RIGHT_ICON = (776, 586)       # prawy slot 0 (800 px): x=764..787
AWAY = (400, 300)


def sh(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def prepare():
    if os.path.exists(IMG):
        os.unlink(IMG)
    sh(["cp", "-c", BASE, IMG])
    for target, src in BUILD.items():
        sh(["python3", DEMO + "/install_file.py", IMG, target, src])
    open("/tmp/dockrun-desktop.cfg", "w").write(CANONICAL)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\DESKTOP.CFG",
        "/tmp/dockrun-desktop.cfg"])
    open("/tmp/dockrun-launch.cfg", "w").write(LAUNCH)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\LAUNCH.CFG",
        "/tmp/dockrun-launch.cfg"])
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


def minimize_top(session):
    session.cmd("hatari-debug w l 0x%x 0x00000001" % st.S["kdbg_test_minimize"])
    time.sleep(0.8)


def main():
    prepare()
    session = st.Session()
    ok = True
    try:
        print("czekam 70 s na boot...", flush=True)
        time.sleep(70)

        # otworz pasek i uruchom SYSINFO lewym skrotem (slot 1)
        session.click_fast(*HAMBURGER)
        time.sleep(0.8)
        session.click_fast(*SYSINFO_SLOT)
        time.sleep(4.0)
        launched = wait_counter(session, "kdbg_dock_launch", 1)
        rc = session.rd32("kdbg_dock_rc")
        print("start SYSINFO: launch=%s rc=%s" % (launched, rc), flush=True)
        if launched != 1 or rc != 0:
            print("BLAD: SYSINFO nie wystartowal", flush=True)
            ok = False
        session.inject(*AWAY, 0)
        time.sleep(0.4)

        # minimalizacja SYSINFO: znaczek [>] na LEWYM skrocie, nie po prawej
        minimize_top(session)
        time.sleep(0.5)
        left = wait_counter(session, "kdbg_dock_left_running", 1)
        right = session.rd32("kdbg_dock_running")
        print("po minimalizacji SYSINFO: left_running=%s right_running=%s" % (left, right), flush=True)
        if left != 1 or right != 0:
            print("BLAD: znaczek nie trafil na lewy skrot", flush=True)
            ok = False
        session.shot("dockrun-left-badge")

        # klik w LEWY skrot = przywrocenie (bez drugiej kopii)
        session.click_fast(*SYSINFO_SLOT)
        time.sleep(1.0)
        restored = wait_counter(session, "kdbg_dock_restore", 1)
        launch2 = session.rd32("kdbg_dock_launch")
        print("klik w lewy skrot: launch=%s restore=%s" % (launch2, restored), flush=True)
        if restored != 1 or launch2 != 1:
            print("BLAD: lewy skrot uruchomil kopie zamiast przywrocic", flush=True)
            ok = False
        session.inject(*AWAY, 0)
        time.sleep(0.4)
        session.shot("dockrun-left-restored")

        # PLASMA bez skrotu na lewo: spawn hookiem, minimalizacja, prawa ikona
        session.cmd("hatari-debug w l 0x%x 0x00000001" % st.S["kdbg_test_spawn"])
        time.sleep(3.0)
        minimize_top(session)
        time.sleep(0.5)
        right = wait_counter(session, "kdbg_dock_running", 1)
        left = session.rd32("kdbg_dock_left_running")
        print("PLASMA bez skrotu: left_running=%s right_running=%s" % (left, right), flush=True)
        if right != 1 or left != 1:
            print("BLAD: aplikacja bez skrotu nie trafila na prawa strone", flush=True)
            ok = False
        session.inject(*AWAY, 0)
        time.sleep(0.4)
        session.shot("dockrun-right-badge")

        # klik w prawa ikone = przywrocenie PLASMA
        session.click_fast(*RIGHT_ICON)
        time.sleep(1.0)
        restored = wait_counter(session, "kdbg_dock_restore", 2)
        print("klik w prawa ikone: kdbg_dock_restore=%s" % restored, flush=True)
        if restored != 2:
            print("BLAD: prawa ikona nie przywrocila okna", flush=True)
            ok = False
        session.shot("dockrun-right-restored")

        # zminimalizuj obie i zwin pasek: [>] na hamburgerze
        minimize_top(session)
        time.sleep(0.3)
        minimize_top(session)
        time.sleep(0.5)
        session.click_fast(*HAMBURGER)
        time.sleep(0.8)
        opened = session.rd32("kdbg_dock_open")
        print("pasek zamkniety: open=%s" % opened, flush=True)
        if opened != 0:
            print("BLAD: pasek nie zamknal sie", flush=True)
            ok = False
        session.shot("dockrun-closed")
    finally:
        session.proc.terminate()
        try:
            session.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            session.proc.kill()
        if session.fd is not None:
            os.close(session.fd)
        session.log.close()

    print("\nWYNIK: %s" % ("DOCK RUNNING DZIALA" if ok else "BLAD - patrz wyzej"), flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
