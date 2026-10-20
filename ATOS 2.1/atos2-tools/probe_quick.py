import subprocess, time, os, sys
from PIL import Image, ImageStat
mode=sys.argv[1]
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
fifo="/tmp/user-view.fifo"
try: os.unlink(fifo)
except OSError: pass
p=subprocess.Popen(["sh",PORT+"/run-bios-vga.sh","--cmd-fifo",fifo,"--zoom","1"],cwd=PORT,
                   stdout=open("/tmp/meas3.log","w"),stderr=subprocess.STDOUT)
for _ in range(400):
    if os.path.exists(fifo): break
    time.sleep(0.05)
out=open(fifo,"w")
t0=time.time()
for target in (30,40,45,50,55,60):
    while time.time()-t0 < target: time.sleep(0.5)
    path="/tmp/q-%d.png"%target
    out.write("hatari-debug screenshot %s\n"%path); out.flush()
    time.sleep(1.0)
    try:
        im=Image.open(path).convert("L").crop((0,40,800,400))
        mean=ImageStat.Stat(im).mean[0]
    except Exception: mean=-1
    print("%s t=%2ds mean=%5.1f  %s" % (mode,target,mean,"DESKTOP" if mean>100 else "boot"), flush=True)
out.close(); p.terminate()
try: p.wait(timeout=8)
except Exception: p.kill()
