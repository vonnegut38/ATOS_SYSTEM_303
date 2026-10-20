#!/usr/bin/env python3
"""Test auto-rollback trybu: SETTINGS -> 1024x768x16 -> APPLY, potem czekamy.
Zrzuty po kazdym kroku, zeby zobaczyc, gdzie jestesmy.  Uzywa fifo trackera."""
import struct, subprocess, time, os, sys
REAL="/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
FIFO="/tmp/sc.fifo"; OUT="/Users/xxx/Documents/Deepseek_test/display-demo"
S={}
for l in subprocess.run(["m68k-elf-nm",REAL+"/build/system/kernel.elf"],capture_output=True,text=True).stdout.splitlines():
    p=l.split()
    if len(p)==3: S[p[2]]=int(p[0],16)
_fd=[None]
def cmd(line):
    if _fd[0] is None: _fd[0]=os.open(FIFO,os.O_WRONLY)
    os.write(_fd[0],(line+"\n").encode()); time.sleep(0.07)
def w(name,data): cmd("hatari-debug w b 0x%x %s"%(S[name]," ".join("0x%02x"%b for b in data)))
def mouse(x,y,b):
    w("mouse_scripted",b"\x01"); w("mouse_q_tail",struct.pack(">I",0)); w("mouse_q_head",struct.pack(">I",0))
    w("mouse_x",struct.pack(">I",x)); w("mouse_y",struct.pack(">I",y)); w("mouse_buttons",struct.pack(">I",b))
    w("mouse_queue",struct.pack(">IIiiBB",b,0,x,y,1,0))
    if b: w("mouse_press_x",struct.pack(">I",x)); w("mouse_press_y",struct.pack(">I",y)); w("mouse_press_pending",b"\x01")
    w("mouse_q_tail",struct.pack(">I",1)); time.sleep(0.4)
def click(x,y): mouse(x,y,1); mouse(x,y,0); time.sleep(0.5)
def key(sc): cmd("hatari-event keydown 0x%02x"%sc); time.sleep(0.12); cmd("hatari-event keyup 0x%02x"%sc); time.sleep(0.5)
def shot(n): cmd("hatari-debug screenshot %s/%s"%(OUT,n)); time.sleep(1.5)
scr=lambda f: (lambda d: struct.unpack('>II',d[16:24]) if d[:4]==b'\x89PNG' else None)(open(OUT+'/'+f,'rb').read())
if __name__=="__main__":
    time.sleep(1)
    click(100,8); shot("r1-menu.png")
    key(0x1c); shot("r2-settings.png")
    print("krok1/2 gotowe")
