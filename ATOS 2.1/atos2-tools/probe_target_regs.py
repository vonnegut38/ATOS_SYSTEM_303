import os, subprocess, time, sys, stat
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
FIFO="/tmp/vofa5.fifo"; LOG="/tmp/vofa5.log"
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
try:
    time.sleep(25)
    cmd("hatari-event keypress 68",1.0)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 28")
    time.sleep(8.0)
    cmd("hatari-debug r",1.0)
    cmd("hatari-debug d",1.0)
    time.sleep(2.0)
    cmd("hatari-debug quit"); time.sleep(2.0)
finally:
    try: f.close()
    except Exception: pass
    if p.poll() is None:
        p.terminate()
        try: p.wait(timeout=5)
        except Exception: p.kill()
    print("rc",p.returncode)
txt=open(LOG,errors="replace").read().splitlines()
keep=[l for l in txt if "VOFA-MODE" not in l and "command FIFO" not in l and "VOFA-TRACE" not in l and "VOFA-WIN" not in l]
print("\n".join(keep[-45:]))
