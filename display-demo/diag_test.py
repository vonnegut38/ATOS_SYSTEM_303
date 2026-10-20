#!/usr/bin/env python3
"""Diagnostyka straznika trybu: czy guard sie UZBRAJA, czy petla jadra chodzi
i kiedy (i czy) pending znika.  Czytamy PAMIEC jadra przez hatari-debug memdump.

Bez tego zgadywalismy: "kernel_user_clock()==0" vs "petla nie chodzi" vs
"guard_restore() zawodzi".  Tu kazda z tych rzeczy jest widoczna w liczbach.
"""
import struct, subprocess, time, os, sys

REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
FIFO = "/tmp/gd.fifo"
HAT = DEMO + "/hatari-tracker-20261013"
IMG = DEMO + "/demo-guard.img"
TAG = "d"

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

def probe(label):
    print("=== %s ===" % label, flush=True)
    cmd("hatari-debug memdump b 0x%x 12" % S["video_guard"])
    cmd("hatari-debug memdump b 0x%x 4" % S["kernel_timer_ticks"])
    cmd("hatari-debug memdump b 0x%x 8" % S["kdbg_loop_calls"])
    cmd("hatari-debug memdump b 0x%x 20" % S["kdbg_guard_ret"])
    time.sleep(1.2)

def shot(name):
    path = "%s/%s" % (DEMO, name)
    cmd("hatari-debug screenshot " + path)
    time.sleep(1.0)
    d = open(path, "rb").read(32)
    wh = struct.unpack(">II", d[16:24]) if d[:4] == b"\x89PNG" else None
    print("  %-18s %s" % (name, ("%dx%d" % wh) if wh else "BRAK"), flush=True)

def run():
    if os.path.exists(FIFO):
        os.unlink(FIFO)
    log = open("/tmp/diag_hatari.log", "w")
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
        "--log-file", "/tmp/diag_%s.log" % TAG,
        "--cmd-fifo", FIFO], stdout=log, stderr=log)
    try:
        print("czekam 62 s na boot...", flush=True)
        time.sleep(62)
        probe("BOOT (przed SETTINGS)")
        click(100, 8); key(0x1c)
        click(400, 273)
        print("APPLY...", flush=True)
        click(330, 396)
        t0 = time.time()
        shot(TAG + "a-apply.png")
        probe("APPLY+~1.5s")
        time.sleep(max(0.0, 3.0 - (time.time() - t0)))
        shot(TAG + "b-3s.png"); probe("APPLY+~4s")
        time.sleep(max(0.0, 7.0 - (time.time() - t0)))
        shot(TAG + "c-7s.png"); probe("APPLY+~8s")
        time.sleep(max(0.0, 13.0 - (time.time() - t0)))
        shot(TAG + "d-13s.png"); probe("APPLY+~14s")
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
