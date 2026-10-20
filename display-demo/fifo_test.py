#!/usr/bin/env python3
"""Test ikon: otworz okno TERMINAL i zamknij je, robiac zrzuty.
Uzywa fifo Hatari (binarka z czytajacym fifo) + zapisu w struktury jadra
(wzorzec z mouse_actions.py Codexa).  Wszystko po to, by SPRAWDZIC, czy ikony
wracaja po zamknieciu okna - bez udzialu uzytkownika."""
import struct, subprocess, sys, time, os
REAL="/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
FIFO="/tmp/ft.fifo"; OUT="/Users/xxx/Documents/Deepseek_test/display-demo"
S={}
for l in subprocess.run(["m68k-elf-nm",REAL+"/build/system/kernel.elf"],capture_output=True,text=True).stdout.splitlines():
    p=l.split()
    if len(p)==3: S[p[2]]=int(p[0],16)
_fd=[None]
def cmd(line):
    # Fifo otwieramy RAZ (Hatari sam go tworzy przy --cmd-fifo).  Otwieranie
    # za kazdym razem blokowaloby, gdy emulator chwilowo nie czyta.
    if _fd[0] is None:
        _fd[0]=os.open(FIFO,os.O_WRONLY)
    os.write(_fd[0],(line+"\n").encode()); time.sleep(0.06)
def w(name,data):
    cmd("hatari-debug w b 0x%x %s"%(S[name]," ".join("0x%02x"%b for b in data)))
def mouse(x,y,b):
    w("mouse_scripted",b"\x01")
    w("mouse_q_tail",struct.pack(">I",0)); w("mouse_q_head",struct.pack(">I",0))
    w("mouse_x",struct.pack(">I",x)); w("mouse_y",struct.pack(">I",y)); w("mouse_buttons",struct.pack(">I",b))
    w("mouse_queue",struct.pack(">IIiiBB",b,0,x,y,1,0))
    if b:
        w("mouse_press_x",struct.pack(">I",x)); w("mouse_press_y",struct.pack(">I",y)); w("mouse_press_pending",b"\x01")
    w("mouse_q_tail",struct.pack(">I",1)); time.sleep(0.35)
def dbl(x,y):
    for _ in range(2):
        mouse(x,y,1); mouse(x,y,0); time.sleep(0.25)
def shot(name):
    cmd("hatari-debug screenshot %s/%s"%(OUT,name)); time.sleep(1.2)
def key(sc):
    cmd("hatari-event keydown 0x%02x"%sc); time.sleep(0.12)
    cmd("hatari-event keyup 0x%02x"%sc); time.sleep(0.45)
if __name__=="__main__":
    time.sleep(float(sys.argv[1]) if len(sys.argv)>1 else 55)
    shot("t1-ikony-przed.png")
    dbl(160,155)          # ikona TERMINAL (kolumna 1, wiersz 2 -> ~160,155)
    time.sleep(3); shot("t2-terminal-otwarty.png")
    mouse(628,32,1); mouse(628,32,0)   # przycisk zamkniecia okna
    time.sleep(3); shot("t3-po-zamknieciu.png")
    print("gotowe")
