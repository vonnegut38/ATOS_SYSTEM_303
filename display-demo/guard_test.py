#!/usr/bin/env python3
"""Test straznika zmiany trybu: SETTINGS -> 1024x768x4 -> APPLY -> czekamy.
Cel: po ~4 s jadro SAMO ma wrocic do 800x600x8.  Zrzuty po kazdym kroku,
z rozmiarem PNG (rozmiar = tryb, ktory widzi Hatari), wiec widac, czy wrocil.
Uzywa fifo trackera (binarka A) - czyli najgorszego przypadku 4 bpp.
"""
import struct, subprocess, time, os, sys

REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
FIFO = "/tmp/gf.fifo"
HAT = DEMO + "/hatari-tracker-20261013"
IMG = DEMO + "/demo-guard.img"
TAG = sys.argv[1] if len(sys.argv) > 1 else "g"

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
    time.sleep(1.2)
    d = open(path, "rb").read(32)
    wh = struct.unpack(">II", d[16:24]) if d[:4] == b"\x89PNG" else None
    print("  %-16s %s" % (name, ("%dx%d" % wh) if wh else "BRAK"), flush=True)
    return wh

def run():
    if os.path.exists(FIFO):
        os.unlink(FIFO)
    log = open("/tmp/guard_%s.log" % TAG, "w")
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
        "--log-file", "/tmp/guard_%s_hatari.log" % TAG,
        "--cmd-fifo", FIFO], stdout=log, stderr=log)
    try:
        print("start hatari pid=%d, czekam 62 s na boot..." % p.pid, flush=True)
        time.sleep(62)
        shot(TAG + "0-boot.png")
        click(100, 8)                      # SETTINGS w pasku
        shot(TAG + "1-menu.png")
        key(0x1c)                          # ENTER -> otworz SETTINGS
        shot(TAG + "2-settings.png")
        click(400, 273)                    # wiersz "1024 x 768 - 16 COLORS"
        shot(TAG + "3-row.png")
        click(330, 396)                    # APPLY
        print("APPLY poszedl", flush=True)
        t0 = time.time()
        shot(TAG + "4-po-apply.png")
        time.sleep(max(0.0, 3.0 - (time.time() - t0)))
        shot(TAG + "5-po-3s.png")
        time.sleep(max(0.0, 7.0 - (time.time() - t0)))
        shot(TAG + "6-po-7s.png")
        time.sleep(max(0.0, 13.0 - (time.time() - t0)))
        shot(TAG + "7-po-13s.png")
        key(0x1c)                          # ENTER: YES/NO albo APPLY
        time.sleep(1.0)
        shot(TAG + "8-po-enter.png")
    finally:
        p.terminate()
        try:
            p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            p.kill()
        log.close()
        print("hatari zakonczony", flush=True)

if __name__ == "__main__":
    run()
