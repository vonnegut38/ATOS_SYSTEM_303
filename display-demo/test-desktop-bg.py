#!/usr/bin/env python3
"""SETTINGS -> DESKTOP: zmiana koloru tla pulpitu i zapis do DESKTOP.CFG."""
import struct,subprocess,time,os,hashlib
REAL="/Users/xxx/Documents/Deepseek_test/ATOS 2.1";DEMO="/Users/xxx/Documents/Deepseek_test/display-demo"
FIFO="/tmp/gbg.fifo";HAT=DEMO+"/hatari-tracker-20261013";IMG=DEMO+"/demo-guard.img"
S={}
for l in subprocess.run(["m68k-elf-nm",REAL+"/build/system/kernel.elf"],capture_output=True,text=True).stdout.splitlines():
    p=l.split()
    if len(p)==3:S[p[2]]=int(p[0],16)
fd=[None]
def cmd(l):
    if fd[0] is None:fd[0]=os.open(FIFO,os.O_WRONLY)
    os.write(fd[0],(l+"\n").encode());time.sleep(0.07)
def w(n,d):cmd("hatari-debug w b 0x%x %s"%(S[n]," ".join("0x%02x"%b for b in d)))
def mouse(x,y,b):
    w("mouse_scripted",b"\x01");w("mouse_q_tail",struct.pack(">I",0));w("mouse_q_head",struct.pack(">I",0))
    w("mouse_x",struct.pack(">I",x));w("mouse_y",struct.pack(">I",y));w("mouse_buttons",struct.pack(">I",b))
    w("mouse_queue",struct.pack(">IIiiBB",b,0,x,y,1,0))
    if b:
        w("mouse_press_x",struct.pack(">I",x));w("mouse_press_y",struct.pack(">I",y));w("mouse_press_pending",b"\x01")
    w("mouse_q_tail",struct.pack(">I",1));time.sleep(0.08)
def click(x,y):mouse(x,y,1);mouse(x,y,0);time.sleep(0.5)
def key(sc):cmd("hatari-event keydown 0x%02x"%sc);time.sleep(0.10);cmd("hatari-event keyup 0x%02x"%sc);time.sleep(0.35)
def shot(n):
    p="%s/%s"%(DEMO,n);cmd("hatari-debug screenshot "+p);time.sleep(0.9)
    raw=open(p,"rb").read();print("  %-22s %s"%(n,hashlib.sha256(raw).hexdigest()[:16]),flush=True)
if os.path.exists(FIFO):os.unlink(FIFO)
log=open("/tmp/gbg.log","w")
p=subprocess.Popen([HAT,"-c","/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/hatari.cfg","--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--fast-forward","off","--ide-master",IMG,"--tos","/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/BIOS.bin","--patch-tos","false","--mmu","off","--statusbar","false","--borders","false","--zoom","1","--sound","44100","--natfeats","on","--confirm-quit","false","--window","--acsi","0=none","--acsi","1=none","--acsi","2=none","--acsi","3=none","--acsi","4=none","--acsi","5=none","--acsi","6=none","--acsi","7=none","--log-file","/tmp/gbg_d.log","--cmd-fifo",FIFO],stdout=log,stderr=log)
try:
    time.sleep(78);time.sleep(5)
    shot("b1-pulpit.png")
    click(85,8)                   # SETTINGS w gornym pasku -> rozwija menu
    time.sleep(0.6)
    click(80,31)                  # pozycja "SETTINGS" w menu
    time.sleep(2.2);shot("b2-settings.png")
    click(248+264,100+16+18)      # zakladka DESKTOP (x 228..300 w kliencie)
    time.sleep(0.8);shot("b3-tab.png")
    click(248+60,100+16+44+24+10) # 2. kolor (NAVY)
    time.sleep(1.2);shot("b4-navy.png")
    key(0x1c)                     # APPLY + zapis
    time.sleep(1.5);shot("b5-po-zapisie.png")
finally:
    p.terminate()
    try:p.wait(timeout=10)
    except: p.kill()
