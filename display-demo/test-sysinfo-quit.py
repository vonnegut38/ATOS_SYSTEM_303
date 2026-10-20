#!/usr/bin/env python3
"""4.6 TEST 2: SYSINFO -> QUIT x3.  Otwiera SYSINFO z launchera i zamyka go
trzy razy (gadzet X okna = WCLS -> app quit), robiac zrzut po kazdym cyklu."""
import struct,subprocess,time,os,hashlib
REAL="/Users/xxx/Documents/Deepseek_test/ATOS 2.1";DEMO="/Users/xxx/Documents/Deepseek_test/display-demo"
FIFO="/tmp/gsi.fifo";HAT=DEMO+"/hatari-scalony-20261017";IMG=DEMO+"/demo-guard.img"
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
def click(x,y):mouse(x,y,1);mouse(x,y,0);time.sleep(0.45)
def key(sc):cmd("hatari-event keydown 0x%02x"%sc);time.sleep(0.10);cmd("hatari-event keyup 0x%02x"%sc);time.sleep(0.28)
def shot(n):
    p="%s/%s"%(DEMO,n);cmd("hatari-debug screenshot "+p);time.sleep(0.9)
    raw=open(p,"rb").read();wh=struct.unpack(">II",raw[16:24])
    h=hashlib.sha256(raw).hexdigest()[:16]
    print("  %-20s %dx%d %s"%(n,wh[0],wh[1],h),flush=True)
    return h
if os.path.exists(FIFO):os.unlink(FIFO)
log=open("/tmp/gsi.log","w")
p=subprocess.Popen([HAT,"-c","/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/hatari.cfg","--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--fast-forward","off","--ide-master",IMG,"--tos","/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/BIOS.bin","--patch-tos","false","--mmu","off","--statusbar","false","--borders","false","--zoom","1","--sound","44100","--natfeats","on","--confirm-quit","false","--window","--acsi","0=none","--acsi","1=none","--acsi","2=none","--acsi","3=none","--acsi","4=none","--acsi","5=none","--acsi","6=none","--acsi","7=none","--log-file","/tmp/gsi_d.log","--cmd-fifo",FIFO],stdout=log,stderr=log)
try:
    time.sleep(78);time.sleep(4)
    base=shot("s0-pulpit.png")
    # SYSINFO = 5. ikona w kolejnosci launchera (My Atari,TRASH,TERMINAL,COMMANDER,SYSINFO)
    for i in range(5):key(0x50)
    key(0x1c);time.sleep(2.2)
    o1=shot("s1-otwarte.png")
    click(691,69);time.sleep(1.8)          # gadzet X okna SYSINFO (100,60,600,480)
    c1=shot("s2-zamkniete.png")
    key(0x1c);time.sleep(2.2)              # ponowne otwarcie (sel_icon pamieta)
    o2=shot("s3-otwarte2.png")
    click(691,69);time.sleep(1.8)
    c2=shot("s4-zamkniete2.png")
    key(0x1c);time.sleep(2.2)
    o3=shot("s5-otwarte3.png")
    click(691,69);time.sleep(1.8)
    c3=shot("s6-zamkniete3.png")
    print("3 cykle: otwarte!=pulpit:",o1!=base,o2!=base,o3!=base,
          "| zamkniete==pulpit:",c1==base,c2==base,c3==base,flush=True)
finally:
    p.terminate()
    try:p.wait(timeout=10)
    except: p.kill()
    log.close()
