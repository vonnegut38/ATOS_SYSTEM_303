#!/usr/bin/env python3
"""DOCK: wiecej niz 2 aplikacje w oknach naraz.

Odpala z paska SYSINFO, PLASMA, TASKMGR i MODPLAY (4 okna), po kazdym
sprawdza kdbg_dock_launch / kdbg_dock_rc i robi zrzut.  Na koncu klika lewy
skrot SYSINFO (ma podniesc jego okno na wierzch).

Uzycie:  python3 test-dock-multi.py
"""
import os
import subprocess
import sys
import time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = DEMO + "/demo-guard.img"
IMG = DEMO + "/demo-multi.img"

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
    r"\SYSTEM\APP\MODPLAY.APP": REAL + "/build/system/MODPLAY.APP",
}

LAUNCH = """; test wielu okien
COMMANDER=\\SYSTEM\\APP\\AC.APP
SYSINFO=\\SYSTEM\\APP\\SYSINFO.APP|24
TASKMGR=\\SYSTEM\\APP\\TASKMGR.APP
PLASMA=\\SYSTEM\\APP\\PLASMA.APP
MODPLAY=\\SYSTEM\\APP\\MODPLAY.APP
"""

CANONICAL = open(DEMO + "/desktop-cfg-wzor.txt").read()

HAMBURGER = (20, 580)
SLOT = {"COMMANDER": (43, 586), "SYSINFO": (67, 586), "TASKMGR": (91, 586),
        "PLASMA": (115, 586), "MODPLAY": (139, 586)}


def sh(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def prepare():
    if os.path.exists(IMG):
        os.unlink(IMG)
    sh(["cp", "-c", BASE, IMG])
    for target, src in BUILD.items():
        sh(["python3", DEMO + "/install_file.py", IMG, target, src])
    open("/tmp/multi-desktop.cfg", "w").write(CANONICAL)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\DESKTOP.CFG",
        "/tmp/multi-desktop.cfg"])
    open("/tmp/multi-launch.cfg", "w").write(LAUNCH)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\LAUNCH.CFG",
        "/tmp/multi-launch.cfg"])
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


def main():
    prepare()
    session = st.Session()
    ok = True
    try:
        print("czekam 70 s na boot...", flush=True)
        time.sleep(70)
        session.click_fast(*HAMBURGER)
        time.sleep(0.8)

        launched = 0
        for name in ("COMMANDER", "SYSINFO", "TASKMGR", "PLASMA", "MODPLAY"):
            session.click_fast(*SLOT[name])
            time.sleep(3.5)
            launched += 1
            got = wait_counter(session, "kdbg_dock_launch", launched)
            rc = session.rd32("kdbg_dock_rc")
            left = session.rd32("kdbg_dock_left_running")
            right = session.rd32("kdbg_dock_running")
            print("%s: launch=%s rc=%s left=%s right=%s" %
                  (name, got, rc, left, right), flush=True)
            if got != launched or rc != 0:
                print("BLAD: %s nie wystartowala (launch=%s rc=%s)" %
                      (name, got, rc), flush=True)
                ok = False
            session.inject(400, 300, 0)
            time.sleep(0.3)
            session.shot("multi-%d-%s" % (launched, name.lower()))

        left = session.rd32("kdbg_dock_left_running")
        right = session.rd32("kdbg_dock_running")
        print("po 5 aplikacjach: left_running=%s right_running=%s" % (left, right), flush=True)
        if left != 5 or right != 0:
            print("BLAD: pasek nie widzi 5 dzialajacych aplikacji", flush=True)
            ok = False
        session.shot("multi-5apps")

        # klik w lewy skrot SYSINFO = podniesienie jego okna
        session.click_fast(*SLOT["SYSINFO"])
        time.sleep(1.0)
        session.shot("multi-raise-sysinfo")
        print("klik w SYSINFO: restore=%s launch=%s" %
              (session.rd32("kdbg_dock_restore"), session.rd32("kdbg_dock_launch")),
              flush=True)

        # podnies TASKMGR i zobacz, czy lista pokazuje wszystkie 12 slotow
        session.click_fast(*SLOT["TASKMGR"])
        time.sleep(0.8)
        # REFRESH w oknie TASKMGR (okno skaskadowane na ~166,90; przycisk
        # na dole) - wymus snapshot calej 12-slotowej tablicy procesow
        session.click_fast(507, 396)
        time.sleep(1.0)
        session.shot("multi-taskmgr")
    finally:
        session.proc.terminate()
        try:
            session.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            session.proc.kill()
        if session.fd is not None:
            os.close(session.fd)
        session.log.close()

    print("\nWYNIK: %s" % ("WIELE OKIEN OK" if ok else "BLAD - patrz wyzej"), flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
