import os, subprocess, time, sys
PORT = "/Users/xxx/Documents/Deepseek_test/atari-030-port"
fifo = "/tmp/acscale.fifo"
try: os.unlink(fifo)
except OSError: pass
env = dict(os.environ); env["SDL_VIDEODRIVER"] = "dummy"
log = open("/tmp/acscale2.log", "w")
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
cmd("hatari-event keypress 68", 1.5)   # F10
cmd("hatari-event keypress 28", 1.5)   # Enter -> SETUP
cmd("hatari-event keypress 80", 0.6)   # Down -> 800
cmd("hatari-event keypress 80", 0.6)   # Down -> 1024
cmd("hatari-event keypress 28", 4.0)   # Enter -> switch to 1024x768
cmd("hatari-debug screenshot /tmp/scale-1024-desktop.png", 2.0)
cmd("hatari-event keypress 62", 1.6)   # F4 FILE
cmd("hatari-event keypress 80", 0.7)
cmd("hatari-event keypress 28", 0.9)
cmd("hatari-event keypress A", 0.3)
cmd("hatari-event keypress C", 0.3)
cmd("hatari-event keypress 28", 5.5)
cmd("hatari-debug screenshot /tmp/scale-1024-ac.png", 2.5)
# now switch back to 640 live while AC is open: AC must re-fit
cmd("hatari-event keypress 68", 1.5)   # F10 (routed to AC? AC ignores F10? no: AC uses F10=quit!)
print("shots done", flush=True)
out.close()
p.terminate()
try: p.wait(timeout=8)
except Exception: p.kill()
log.close()
