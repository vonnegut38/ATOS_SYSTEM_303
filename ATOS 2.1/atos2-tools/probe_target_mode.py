import os, subprocess, time, sys
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
FIFO="/tmp/vofa4.fifo"; LOG="/tmp/vofa4.log"
for p in (FIFO,LOG):
    try: os.unlink(p)
    except FileNotFoundError: pass
env=dict(os.environ); env["SDL_VIDEODRIVER"]="dummy"
out=open(LOG,"w")
p=subprocess.Popen(["sh","run-bios-vga.sh","--cmd-fifo",FIFO],cwd=PORT,env=env,stdout=out,stderr=subprocess.STDOUT)
def wait_fifo(t=15):
    import stat
    dl=time.time()+t
    while time.time()<dl:
        try:
            if stat.S_ISFIFO(os.stat(FIFO).st_mode): return True
        except OSError: pass
        if p.poll() is not None: return False
        time.sleep(0.05)
    return False
if not wait_fifo(): print("no fifo"); sys.exit(1)
f=open(FIFO,"w")
def cmd(s,d=0.0):
    f.write(s+"\n"); f.flush()
    if d: time.sleep(d)
def shot(path,settle=1.5):
    try: os.unlink(path)
    except FileNotFoundError: pass
    cmd("hatari-debug screenshot "+path); time.sleep(settle)
    return os.path.exists(path)
def modes():
    try: t=open(LOG,errors="replace").read()
    except Exception: return []
    return [ln for ln in t.splitlines() if "VOFA-MODE" in ln]
try:
    time.sleep(25)
    print("640:", shot("/tmp/t2-640.png"), modes()[-1:], flush=True)
    cmd("hatari-event keypress 68",1.0)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 28")
    for k in range(1,7):
        time.sleep(2.5)
        print("wait",k*2.5,"s ->", modes()[-1], flush=True)
    print("800:", shot("/tmp/t2-800.png"), flush=True)
    cmd("hatari-event keypress 68",0.8)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 28")
    for k in range(1,8):
        time.sleep(2.5)
        print("wait2",k*2.5,"s ->", modes()[-1], flush=True)
    print("1024:", shot("/tmp/t2-1024.png"), flush=True)
    cmd("hatari-debug quit"); time.sleep(2.0)
finally:
    try: f.close()
    except Exception: pass
    if p.poll() is None:
        p.terminate()
        try: p.wait(timeout=5)
        except Exception: p.kill()
    print("rc",p.returncode, flush=True)
