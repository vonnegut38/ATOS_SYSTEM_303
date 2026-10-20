import os, subprocess, time, sys, stat, re
from PIL import Image
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
FIFO="/tmp/vofa6.fifo"; LOG="/tmp/vofa6.log"
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
def read_mem(addr,count,settle=3.0):
    out.flush()
    start=os.path.getsize(LOG)
    cmd("hatari-debug memdump %s %d"%(hex(addr),count))
    time.sleep(settle)
    out.flush()
    data=bytearray()
    with open(LOG,errors="replace") as fh:
        fh.seek(start)
        for line in fh:
            if not re.match(r"^[0-9A-Fa-f]{8}:",line): continue
            body=line.split(":",1)[1]
            hexpart=body.split("  ",1)[0]
            for tok in hexpart.split():
                if len(tok)==2:
                    try: data.append(int(tok,16))
                    except ValueError: pass
    return bytes(data)
def vram_png(w,h,path):
    data=read_mem(0x00c00000,(w//8)*h)
    if len(data)<(w//8)*h:
        print("short VRAM",len(data),"want",(w//8)*h); return False
    im=Image.frombytes("1",(w,h),data[: (w//8)*h])
    im.save(path); print("saved",path,im.size); return True
try:
    time.sleep(25)
    cmd("hatari-event keypress 68",1.2)
    cmd("hatari-debug screenshot /tmp/menu-final.png",1.5)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 28"); time.sleep(4.0)
    print("800 png:",vram_png(800,600,"/tmp/vram-800.png"),flush=True)
    cmd("hatari-event keypress 68",0.8)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 80",0.4)
    cmd("hatari-event keypress 28"); time.sleep(4.0)
    print("1024 png:",vram_png(1024,768,"/tmp/vram-1024.png"),flush=True)
    cmd("hatari-debug quit"); time.sleep(1.0)
finally:
    try: f.close()
    except Exception: pass
    if p.poll() is None:
        p.terminate()
        try: p.wait(timeout=5)
        except Exception: p.kill()
    print("rc",p.returncode)
