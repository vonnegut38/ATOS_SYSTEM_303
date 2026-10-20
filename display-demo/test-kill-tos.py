#!/usr/bin/env python3
"""4.6 TEST: Ctrl+Alt+Esc konczy programme TOS (AUTO\\SYSINFO.PRG).
Sprawdza, ze po zabiciu okno znika, a pulpit dalej odpowiada (zrzuty + ping
klawiatury).  Uruchamiany na KOPII obrazu, zeby nie ruszac obrazu testowego."""
import struct,subprocess,time,os,sys,hashlib
REAL="/Users/xxx/Documents/Deepseek_test/ATOS 2.1";DEMO="/Users/xxx/Documents/Deepseek_test/display-demo"
FIFO="/tmp/gkill.fifo";HAT=DEMO+"/hatari-scalony-20261017";IMG=DEMO+"/demo-guard.img"
S={}
for l in subprocess.run(["m68k-elf-nm",REAL+"/build/system/kernel.elf"],capture_output=True,text=True).stdout.splitlines():
    p=l.split()
    if len(p)==3:S[p[2]]=int(p[0],16)
fd=[None]
def cmd(l):
    if fd[0] is None:fd[0]=os.open(FIFO,os.O_WRONLY)
    os.write(fd[0],(l+"\n").encode());time.sleep(0.07)
def key(sc,hold=0.10):
    cmd("hatari-event keydown 0x%02x"%sc);time.sleep(hold)
    cmd("hatari-event keyup 0x%02x"%sc);time.sleep(0.20)
def shot(n):
    p="%s/%s"%(DEMO,n);cmd("hatari-debug screenshot "+p);time.sleep(0.9)
    raw=open(p,"rb").read();wh=struct.unpack(">II",raw[16:24])
    print("  %-24s %dx%d %s"%(n,wh[0],wh[1],hashlib.sha256(raw).hexdigest()[:16]),flush=True)
    return hashlib.sha256(raw).hexdigest()
if os.path.exists(FIFO):os.unlink(FIFO)
log=open("/tmp/gkill.log","w")
p=subprocess.Popen([HAT,"-c","/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/hatari.cfg","--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--fast-forward","off","--ide-master",IMG,"--tos","/Users/xxx/Documents/Deepseek_test/ATOS_AKTUALNY/BIOS.bin","--patch-tos","false","--mmu","off","--statusbar","false","--borders","false","--zoom","1","--sound","44100","--natfeats","on","--confirm-quit","false","--window","--acsi","0=none","--acsi","1=none","--acsi","2=none","--acsi","3=none","--acsi","4=none","--acsi","5=none","--acsi","6=none","--acsi","7=none","--log-file","/tmp/gkill_d.log","--cmd-fifo",FIFO],stdout=log,stderr=log)
try:
    time.sleep(78);time.sleep(4)
    def mouse(x,y,b):
        cmd("hatari-debug w b 0x%x %s"%(S["mouse_scripted"],"0x01"));time.sleep(0.05)
        for n,v in (("mouse_q_tail",struct.pack(">I",0)),("mouse_q_head",struct.pack(">I",0)),
                    ("mouse_x",struct.pack(">I",x)),("mouse_y",struct.pack(">I",y)),
                    ("mouse_buttons",struct.pack(">I",b))):
            cmd("hatari-debug w b 0x%x %s"%(S[n]," ".join("0x%02x"%c for c in v)))
        cmd("hatari-debug w b 0x%x %s"%(S["mouse_queue"]," ".join("0x%02x"%c for c in struct.pack(">IIiiBB",b,0,x,y,1,0))))
        if b:
            cmd("hatari-debug w b 0x%x %s"%(S["mouse_press_x"]," ".join("0x%02x"%c for c in struct.pack(">I",x))))
            cmd("hatari-debug w b 0x%x 0x01"%S["mouse_press_pending"])
        cmd("hatari-debug w b 0x%x %s"%(S["mouse_q_tail"]," ".join("0x%02x"%c for c in struct.pack(">I",1))))
        time.sleep(0.30)
    # dwuklik w ikone SYSINFO (48,205)
    for _ in range(10):
        key(0x50);time.sleep(0.12)
    key(0x1c)
    time.sleep(2.0)
    a=shot("k1-przed.png")            # okno SYSINFO na ekranie
    # Ctrl+Alt+Esc
    cmd("hatari-event keydown 0x1d");time.sleep(0.1)
    cmd("hatari-event keydown 0x38");time.sleep(0.1)
    key(0x01);time.sleep(1.0)
    cmd("hatari-event keyup 0x38");time.sleep(0.1)
    cmd("hatari-event keyup 0x1d");time.sleep(1.5)
    b=shot("k2-po-zabiciu.png")
    # PONOWNY START programu TOS: sel_icon nadal wskazuje SYSTOS (modulo po
    # Down x10), wiec wystarczy Enter - to sprawdza, czy teardown byl czysty.
    key(0x1c);time.sleep(3.0)
    c=shot("k3-ponownie.png")
    print("PRZED != PO:",a!=b," PO != PONOWNIE:",b!=c,flush=True)
finally:
    p.terminate()
    try:p.wait(timeout=10)
    except: p.kill()
