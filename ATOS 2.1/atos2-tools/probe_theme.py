import os, subprocess, time
PORT = "/Users/xxx/Documents/Deepseek_test/atari-030-port"
fifo = "/tmp/actheme.fifo"
try: os.unlink(fifo)
except OSError: pass
env = dict(os.environ); env["SDL_VIDEODRIVER"] = "dummy"
log = open("/tmp/actheme.log", "w")
p = subprocess.Popen(["sh", PORT + "/run-bios-vga.sh", "--cmd-fifo", fifo, "--zoom", "1"],
                     cwd=PORT, env=env, stdout=log, stderr=subprocess.STDOUT)
for _ in range(300):
    if os.path.exists(fifo): break
    time.sleep(0.05)
time.sleep(26)
out = open(fifo, "w")
def cmd(s, d=0.0):
    out.write(s + "\n"); out.flush()
    if d: time.sleep(d)
cmd("hatari-debug screenshot /tmp/theme-0-mono.png", 2.0)
cmd("hatari-event keypress 68", 1.4)   # F10 OPTIONS
cmd("hatari-event keypress 80", 0.6)   # Down -> THEME
cmd("hatari-event keypress 28", 2.5)   # Enter -> cycle
cmd("hatari-debug screenshot /tmp/theme-1.png", 2.0)
cmd("hatari-event keypress 68", 1.4)
cmd("hatari-event keypress 80", 0.6)
cmd("hatari-event keypress 28", 2.5)
cmd("hatari-debug screenshot /tmp/theme-2.png", 2.0)
cmd("hatari-event keypress 62", 1.6)   # F4 FILE
cmd("hatari-event keypress 80", 0.7)
cmd("hatari-event keypress 28", 0.9)
cmd("hatari-event keypress A", 0.3)
cmd("hatari-event keypress C", 0.3)
cmd("hatari-event keypress 28", 5.0)
cmd("hatari-debug screenshot /tmp/theme-3-ac.png", 2.5)
print("shots done", flush=True)
out.close()
p.terminate()
try: p.wait(timeout=8)
except Exception: p.kill()
log.close()
