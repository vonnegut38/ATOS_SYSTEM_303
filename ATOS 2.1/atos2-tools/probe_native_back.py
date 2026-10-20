import os, subprocess, time, sys, stat
from PIL import Image
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
FIFO="/tmp/vofa8.fifo"; LOG="/tmp/vofa8.log"
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
def modes():
    try: t=open(LOG,errors="replace").read()
    except Exception: return []
    return [ln for ln in t.splitlines() if "VOFA-MODE" in ln]
def shot(path,settle=2.0):
    try: os.unlink(path)
    except FileNotFoundError: pass
    cmd("hatari-debug screenshot "+path); time.sleep(settle)
    return Image.open(path).size if os.path.exists(path) else None
try:
    time.sleep(25)
    print("A 640:", modes()[-1], flush=True)
    # 800
    cmd("hatari-event keypress 68",1.0); cmd("hatari-event keypress 80",0.4); cmd("hatari-event keypress 28"); time.sleep(3.5)
    print("B 800:", modes()[-1], flush=True)
    # back to 640
    cmd("hatari-event keypress 68",1.2)
    print("menu@800:", shot("/tmp/dbg-menu800.png"), flush=True)
    cmd("hatari-event keypress 28"); time.sleep(3.5)
    print("C back:", modes()[-1], flush=True)
    # 1024
    cmd("hatari-event keypress 68",1.0); cmd("hatari-event keypress 80",0.4); cmd("hatari-event keypress 80",0.4); cmd("hatari-event keypress 28"); time.sleep(3.5)
    print("D 1024:", modes()[-1], flush=True)
    # back to 640
    cmd("hatari-event keypress 68",1.2)
    print("menu@1024:", shot("/tmp/dbg-menu1024.png"), flush=True)
    cmd("hatari-event keypress 28"); time.sleep(3.5)
    print("E back:", modes()[-1], flush=True)
    cmd("hatari-debug quit"); time.sleep(1.0)
finally:
    try: f.close()
    except Exception: pass
    if p.poll() is None:
        p.terminate()
        try: p.wait(timeout=5)
        except Exception: p.kill()
    print("rc",p.returncode)
