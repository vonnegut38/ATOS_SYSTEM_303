import os, subprocess, time
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
fifo="/tmp/tgt.fifo"
try: os.unlink(fifo)
except OSError: pass
env=dict(os.environ); env["SDL_VIDEODRIVER"]="dummy"
log=open("/tmp/tgt.log","w")
p=subprocess.Popen(["sh",PORT+"/run-bios-vga.sh","--cmd-fifo",fifo,"--zoom","1"],cwd=PORT,env=env,stdout=log,stderr=subprocess.STDOUT)
for _ in range(300):
    if os.path.exists(fifo): break
    time.sleep(0.05)
time.sleep(27)
out=open(fifo,"w")
def cmd(s,d=0.0):
    out.write(s+"\n"); out.flush()
    if d: time.sleep(d)
def shot(n):
    cmd("hatari-debug screenshot /tmp/tg-%s.png"%n, 2.5)
cmd("hatari-event keypress 68",1.5); cmd("hatari-event keypress 28",2.5)   # SETTINGS
cmd("hatari-event keypress 80",1.2); cmd("hatari-event keypress 28",4.0)   # 800x600
cmd("hatari-event keypress 62",1.5)   # F4 -> my instrumented: OPTIONS+SETTINGS
shot("1")
cmd("hatari-event keypress 59",1.5)   # F1 -> sweep mouse
shot("2")
print("done",flush=True)
out.close(); p.terminate()
try: p.wait(timeout=8)
except Exception: p.kill()
log.close()
