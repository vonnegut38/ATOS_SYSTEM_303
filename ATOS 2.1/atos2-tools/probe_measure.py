import subprocess, time, os
from PIL import Image, ImageStat
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
fifo="/tmp/user-view.fifo"
try: os.unlink(fifo)
except OSError: pass
p=subprocess.Popen(["sh",PORT+"/run-bios-vga.sh","--cmd-fifo",fifo,"--zoom","1"],cwd=PORT,
                   stdout=open("/tmp/meas2.log","w"),stderr=subprocess.STDOUT)
for _ in range(400):
    if os.path.exists(fifo): break
    time.sleep(0.05)
out=open(fifo,"w")
t0=time.time()
found=None
for k in range(45):
    time.sleep(2)
    path="/tmp/mm-%d.png"%k
    out.write("hatari-debug screenshot %s\n"%path); out.flush()
    time.sleep(1.0)
    try:
        im=Image.open(path).convert("L").crop((0,40,800,400))
        mean=ImageStat.Stat(im).mean[0]
    except Exception:
        mean=-1
    el=time.time()-t0
    print("%2d t=%5.1fs mean=%5.1f" % (k, el, mean), flush=True)
    if mean>100 and found is None:
        found=el; print("==> DESKTOP AT %.1f s"%el, flush=True); break
out.close(); p.terminate()
try: p.wait(timeout=8)
except Exception: p.kill()
if found is None: print("nie wykryto")
