#!/usr/bin/env python3
"""Test SCALONEJ binarki Hatari (audio + poprawne 4/16 bpp).

Binarne: scalony/hatari-build2/src/hatari (libxmp + ATOS Sound + poprawka vofa.c).
Cel: "1024 x 768 - 16 COLORS" ma sie RYSONWAC POPRAWNIE (1024x768, nie 2048)
i zostac po potwierdzeniu ENTER (YES), a nie byc cofnietym przez straznik.
"""
import struct, subprocess, time, os, hashlib

REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
FIFO = "/tmp/gm.fifo"
HAT = "/Users/xxx/Documents/Deepseek_test/scalony/hatari-build2/src/hatari"
IMG = DEMO + "/demo-guard.img"

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
    time.sleep(0.9)
    raw = open(path, "rb").read()
    wh = struct.unpack(">II", raw[16:24]) if raw[:4] == b"\x89PNG" else (0, 0)
    print("  %-22s %4dx%-4d %s" % (name, wh[0], wh[1],
          hashlib.sha256(raw).hexdigest()[:16]), flush=True)

def run():
    if os.path.exists(FIFO):
        os.unlink(FIFO)
    log = open("/tmp/merged_hatari.log", "w")
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
        "--log-file", "/tmp/merged_d.log", "--cmd-fifo", FIFO], stdout=log, stderr=log)
    try:
        print("czekam 62 s na boot...", flush=True)
        time.sleep(62)
        shot("m0-boot.png")
        click(100, 8); key(0x1c)
        click(400, 273)
        click(330, 396)          # APPLY
        shot("m1-apply-16col.png")
        key(0x1c)                # ENTER = YES, zostaw tryb
        shot("m2-po-yes.png")
        time.sleep(4)
        shot("m3-po-6s.png")
        time.sleep(6)
        shot("m4-po-12s.png")
        key(0x1c)                # ENTER: APPLY tego, co zaznaczone
        time.sleep(1.2)
        shot("m5-po-enter.png")
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
