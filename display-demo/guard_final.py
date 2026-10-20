#!/usr/bin/env python3
"""Weryfikacja koncowa: SETTINGS -> "1024 x 768 - 16 COLORS" -> APPLY.
Oczekiwane: tryb wchodzi, a po ~4-9 s jadro SAMO wraca do 800x600 i EKRAN JEST
CZYSTY (pelne przemalowanie), bez dotykania klawiatury.
"""
import struct, subprocess, time, os, sys, hashlib

REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
FIFO = "/tmp/gv.fifo"
HAT = DEMO + "/hatari-tracker-20261013"
IMG = DEMO + "/demo-guard.img"
REF = "0e821ede61e5946463cb75f424e592200da37803b5a2dc3ec7747cf249ef064a"

S = {}
for l in subprocess.run(["m68k-elf-nm", REAL + "/build/system/kernel.elf"],
                        capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3:
        S[p[2]] = int(p[0], 16)

fd = [None]

def cmd(line):
    if fd[0] is None:
        fd[0] = os.open(FIFO, os.O_WRONLY)
    os.write(fd[0], (line + "\n").encode())
    time.sleep(0.08)

def w(name, data):
    cmd("hatari-debug w b 0x%x %s" % (S[name], " ".join("0x%02x" % b for b in data)))

def mouse(x, y, b):
    w("mouse_scripted", b"\x01")
    w("mouse_q_tail", struct.pack(">I", 0)); w("mouse_q_head", struct.pack(">I", 0))
    w("mouse_x", struct.pack(">I", x)); w("mouse_y", struct.pack(">I", y))
    w("mouse_buttons", struct.pack(">I", b))
    w("mouse_queue", struct.pack(">IIiiBB", b, 0, x, y, 1, 0))
    if b:
        w("mouse_press_x", struct.pack(">I", x)); w("mouse_press_y", struct.pack(">I", y))
        w("mouse_press_pending", b"\x01")
    w("mouse_q_tail", struct.pack(">I", 1)); time.sleep(0.35)

def click(x, y):
    mouse(x, y, 1); mouse(x, y, 0); time.sleep(0.45)

def key(sc):
    cmd("hatari-event keydown 0x%02x" % sc); time.sleep(0.12)
    cmd("hatari-event keyup 0x%02x" % sc); time.sleep(0.45)

def shot(name):
    path = "%s/%s" % (DEMO, name)
    cmd("hatari-debug screenshot " + path)
    time.sleep(1.0)
    raw = open(path, "rb").read()
    wh = struct.unpack(">II", raw[16:24]) if raw[:4] == b"\x89PNG" else (0, 0)
    h = hashlib.sha256(raw).hexdigest()
    print("  %-18s %4dx%-4d %s%s" % (name, wh[0], wh[1], h[:16],
          "  <-- WZORZEC" if h == REF else ""), flush=True)
    return wh

def run():
    if os.path.exists(FIFO):
        os.unlink(FIFO)
    log = open("/tmp/final_hatari.log", "w")
    p = subprocess.Popen([
        HAT, "-c", "/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/hatari.cfg",
        "--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
        "--memsize", "4", "--ttram", "64", "--tf536", "on",
        "--graphics-card", "vofa", "--gfx-chip", "et4000", "--fast-forward", "off",
        "--ide-master", IMG, "--tos", "/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/BIOS.bin",
        "--patch-tos", "false", "--mmu", "off", "--statusbar", "false",
        "--borders", "false", "--zoom", "1", "--sound", "44100",
        "--natfeats", "on", "--confirm-quit", "false", "--window",
        "--acsi", "0=none", "--acsi", "1=none", "--acsi", "2=none", "--acsi", "3=none",
        "--acsi", "4=none", "--acsi", "5=none", "--acsi", "6=none", "--acsi", "7=none",
        "--log-file", "/tmp/final_d.log", "--cmd-fifo", FIFO], stdout=log, stderr=log)
    try:
        print("czekam 62 s na boot...", flush=True)
        time.sleep(62)
        shot("v0-boot.png")
        click(100, 8); key(0x1c)          # SETTINGS z paska + ENTER w menu
        click(400, 273)                   # wiersz 1024x768 16 kolorow
        shot("v1-przed-apply.png")
        click(330, 396)                   # APPLY
        t0 = time.time()
        shot("v2-po-apply.png")
        for wait, name in ((2, "v3-po-2s.png"), (4, "v4-po-4s.png"),
                           (6, "v5-po-6s.png"), (8, "v6-po-8s.png"),
                           (12, "v7-po-12s.png")):
            time.sleep(max(0.0, wait - (time.time() - t0)))
            shot(name)
        key(0x1c)                          # ENTER: co aplikacja pokaze
        time.sleep(1.5)
        shot("v8-po-enter.png")
    finally:
        p.terminate()
        try:
            p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            p.kill()
        log.close()
        print("koniec", flush=True)

if __name__ == "__main__":
    run()
