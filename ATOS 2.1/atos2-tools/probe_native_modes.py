import os, subprocess, time, sys, stat
from PIL import Image
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
FIFO="/tmp/vofa7.fifo"; LOG="/tmp/vofa7.log"
for p in (FIFO,LOG):
    try: os.unlink(p)
    except FileNotFoundError: pass
env=dict(os.environ); env["SDL_VIDEODRIVER"]="dummy"
out=open(LOG,"w")
p=subprocess.Popen(["sh","run-bios-vga.sh","--cmd-fifo",FIFO],cwd=PORT,env=env,stdout=out,stderr=subprocess.STDOUT)
dl=time.time()+15
while time.time()<dl:
    try:
        if stat.S_ISFIFO(os.stat(FIFO).st_mode): break
    except OSError: pass
    time.sleep(0.05)
f=open(FIFO,"w")
def cmd(s,d=0.0):
    f.write(s+"\n"); f.flush()
    if d: time.sleep(d)
def shot(path,settle=2.5):
    try: os.unlink(path)
    except FileNotFoundError: pass
    cmd("hatari-debug screenshot "+path); time.sleep(settle)
    if os.path.exists(path):
        im=Image.open(path); print(path,im.size,flush=True); return im.size
    print(path,"MISSING",flush=True); return None
def modes():
    try: t=open(LOG,errors="replace").read()
    except Exception: return []
    return [ln for ln in t.splitlines() if "VOFA-MODE" in ln]
try:
    time.sleep(25)
    print("640:", modes()[-1],flush=True)
    shot("/tmp/nat-640.png")
    cmd("hatari-event keypress 68",1.2)
    shot("/tmp/nat-menu.png")
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 28"); time.sleep(4.0)
    print("800:", modes()[-1],flush=True)
    shot("/tmp/nat-800.png")
    cmd("hatari-event keypress 68",1.0)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 28"); time.sleep(4.0)
    print("1024:", modes()[-1],flush=True)
    shot("/tmp/nat-1024.png")
    # back to 640: F10 reopens OPTIONS with item 0 (SCREEN 640x400) selected
    cmd("hatari-event keypress 68",1.0)
    cmd("hatari-event keypress 28"); time.sleep(4.0)
    print("back640:", modes()[-1],flush=True)
    shot("/tmp/nat-640b.png")
    cmd("hatari-debug quit"); time.sleep(1.0)
finally:
    try: f.close()
    except Exception: pass
    if p.poll() is None:
        p.terminate()
        try: p.wait(timeout=5)
        except Exception: p.kill()
    print("rc",p.returncode)
