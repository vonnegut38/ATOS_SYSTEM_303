import os, subprocess, time
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
fifo="/tmp/gg.fifo"
try: os.unlink(fifo)
except OSError: pass
env=dict(os.environ); env["SDL_VIDEODRIVER"]="dummy"
log=open("/tmp/gg.log","w")
p=subprocess.Popen(["sh",PORT+"/run-bios-vga.sh","--cmd-fifo",fifo,"--zoom","1"],cwd=PORT,env=env,stdout=log,stderr=subprocess.STDOUT)
for _ in range(300):
    if os.path.exists(fifo): break
    time.sleep(0.05)
time.sleep(27)
out=open(fifo,"w")
def cmd(s,d=0.0):
    out.write(s+"\n"); out.flush()
    if d: time.sleep(d)
cmd("hatari-event keypress 66",1.5)    # F7 (0x42=66) opens DISK C
cmd("hatari-event keypress 61",0.0)    # F2 (0x3d=61) grow
cmd("hatari-debug screenshot /tmp/gg-0.png",1.0)
cmd("hatari-debug screenshot /tmp/gg-1.png",2.5)
print("done",flush=True)
out.close(); p.terminate()
try: p.wait(timeout=8)
except Exception: p.kill()
log.close()
