import os, subprocess, time
PORT="/Users/xxx/Documents/Deepseek_test/atari-030-port"
fifo="/tmp/bl.fifo"
try: os.unlink(fifo)
except OSError: pass
env=dict(os.environ); env["SDL_VIDEODRIVER"]="dummy"
log=open("/tmp/bl.log","w")
p=subprocess.Popen(["sh",PORT+"/run-bios-vga.sh","--cmd-fifo",fifo,"--zoom","1"],cwd=PORT,env=env,stdout=log,stderr=subprocess.STDOUT)
for _ in range(300):
    if os.path.exists(fifo): break
    time.sleep(0.05)
time.sleep(26)
out=open(fifo,"w")
def cmd(s,d=0.0):
    out.write(s+"\n"); out.flush()
    if d: time.sleep(d)
cmd("hatari-event keypress 62",1.5)
cmd("hatari-event keypress 80",0.8)
cmd("hatari-event keypress 80",0.8)
cmd("hatari-event keypress 28",3.0)
cmd("hatari-debug screenshot /tmp/bl-console.png",2.5)
print("done",flush=True)
out.close(); p.terminate()
try: p.wait(timeout=8)
except Exception: p.kill()
log.close()
