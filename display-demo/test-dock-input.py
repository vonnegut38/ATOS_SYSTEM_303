#!/usr/bin/env python3
"""DOCK: czy po 3 aplikacjach okna reaguja na mysz.

SYSINFO: klik DISK / BENCHMARK przy 1 aplikacji i przy 3.
TASKMGR: zamkniecie okna (czy wraca).
"""
import os
import subprocess
import sys
import time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
BASE = DEMO + "/demo-guard.img"
IMG = DEMO + "/demo-input.img"

sys.path.insert(0, DEMO)
import stress_atos as st
st.IMG = IMG

BUILD = {
    r"\SYSTEM\KERNEL.SYS": REAL + "/build/system/KERNEL.SYS",
    r"\SYSTEM\GUI.APP": REAL + "/build/system/GUI.APP",
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

LAUNCH = """; test inputu przy 4 aplikacjach
SYSINFO=\\SYSTEM\\APP\\SYSINFO.APP|24
TASKMGR=\\SYSTEM\\APP\\TASKMGR.APP
PLASMA=\\SYSTEM\\APP\\PLASMA.APP
MODPLAY=\\SYSTEM\\APP\\MODPLAY.APP
"""

CANONICAL = open(DEMO + "/desktop-cfg-wzor.txt").read()
HAMBURGER = (20, 580)
SYSINFO_SLOT = (43, 586)   # LAUNCH: SYSINFO, TASKMGR, PLASMA -> sloty 0,1,2
TASKMGR_SLOT = (67, 586)
PLASMA_SLOT = (91, 586)
MODPLAY_SLOT = (115, 586)


def sh(cmd):
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def prepare():
    if os.path.exists(IMG):
        os.unlink(IMG)
    sh(["cp", "-c", BASE, IMG])
    for target, src in BUILD.items():
        sh(["python3", DEMO + "/install_file.py", IMG, target, src])
    open("/tmp/input-desktop.cfg", "w").write(CANONICAL)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\DESKTOP.CFG",
        "/tmp/input-desktop.cfg"])
    open("/tmp/input-launch.cfg", "w").write(LAUNCH)
    sh(["python3", DEMO + "/install_file.py", IMG, r"\SYSTEM\LAUNCH.CFG",
        "/tmp/input-launch.cfg"])
    subprocess.run(["python3", REAL + "/tools/img_delete.py", IMG,
                    r"\SYSTEM\DOCK.CFG"], capture_output=True)


def main():
    prepare()
    session = st.Session()
    try:
        print("czekam 70 s na boot...", flush=True)
        time.sleep(70)
        session.click_fast(*HAMBURGER)
        time.sleep(0.8)

        # 1 aplikacja: SYSINFO
        session.click_fast(*SYSINFO_SLOT)
        time.sleep(3.0)
        session.inject(400, 300, 0)
        session.shot("input-1-hardware")
        session.click_fast(260, 108)      # DISK
        time.sleep(1.0)
        session.shot("input-1-disk")
        session.click_fast(345, 108)      # BENCHMARK
        time.sleep(1.0)
        session.shot("input-1-bench")

        # + TASKMGR i PLASMA (3 aplikacje)
        session.click_fast(*TASKMGR_SLOT)
        time.sleep(3.0)
        session.click_fast(*PLASMA_SLOT)
        time.sleep(3.0)
        session.click_fast(*MODPLAY_SLOT)   # 4. aplikacja
        time.sleep(3.0)
        session.inject(400, 300, 0)
        session.click_fast(*SYSINFO_SLOT)  # podnies SYSINFO
        time.sleep(1.0)
        session.shot("input-4-hardware")
        session.click_fast(260, 108)      # DISK
        time.sleep(1.0)
        session.shot("input-4-disk")
        session.click_fast(430, 108)      # DEVICES
        time.sleep(1.0)
        session.shot("input-4-devices")

        # TASKMGR: zamkniecie okna
        session.click_fast(*TASKMGR_SLOT)
        time.sleep(1.0)
        before = session.rd32("kdbg_wm_close_window")
        print("kdbg_wm_close_window przed zamknieciem:", before, flush=True)
        session.click_fast(630, 98)       # X okna TASKMGR
        time.sleep(2.0)
        after = session.rd32("kdbg_wm_close_window")
        print("kdbg_wm_close_window po zamknieciu:", after, flush=True)
        session.shot("input-taskmgr-closed")
    finally:
        session.proc.terminate()
        try:
            session.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            session.proc.kill()
        if session.fd is not None:
            os.close(session.fd)
        session.log.close()

    print("\nZRZUTY: input-1-*.png, input-4-*.png, input-taskmgr-closed.png", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
