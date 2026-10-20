from pathlib import Path
import sys,time,shutil,subprocess,json,struct
P=Path('/Users/xxx/Documents/Deepseek_test/ATOS 2.1');R=Path(__file__).resolve().parent
sys.path.insert(0,str(P/'atos2-tools'));from atos2_auto import Hatari
D=R/'final';D.mkdir(exist_ok=True);(D/'build').mkdir(exist_ok=True)
shutil.copy2(P/'build/bios_mste.bin',D/'build/bios_mste.bin');shutil.copy2(P/'atos2.cfg',D/'atos2.cfg')
subprocess.run(['cp','-c',str(R/'before.img'),str(R/'final.img')],check=True)
for f,d in [('KERNEL.SYS','SYSTEM'),('DESKTOP.APP','SYSTEM/APP'),('PLASMA.APP','SYSTEM/APP')]:subprocess.run(['python3',str(P/'tools/install_boot_sys.py'),str(R/'final.img'),str(P/'build/system'/f),'--directory',d],check=True)
shutil.copy2(P/'build/system/kernel.elf',R/'final-kernel.elf')
E=['--machine','megast','--cpulevel','3','--cpuclock','32','--memsize','8','--ttram','64','--tf536','on','--graphics-card','vofa','--gfx-chip','et4000','--mono','--fast-forward','off','--ide-master',str(R/'final.img'),'--mmu','off','--statusbar','false','--borders','false','--zoom','1']
for i in range(8):E+=['--acsi',f'{i}=none']
h=Hatari(D,window=False,image=R/'final.img',binary=str(R/'hatari-reset-fixed'),extra=E,render=True)
S={a[2]:int(a[0],16) for a in (l.split() for l in subprocess.check_output(['m68k-elf-nm',str(R/'final-kernel.elf')],text=True).splitlines()) if len(a)==3}
def tap(k):h.keydown(k);time.sleep(.15);h.keyup(k);time.sleep(.3)
def snap(name):
 h.screenshot(R/(name+'.png'),settle=.5)
 names=['kernel_fault_vector','kernel_fault_pc','kdbg_end_status','kdbg_end_name','kdbg_end_pc','kernel_tos_async_result','tos_aes_log_n','kdbg_loop_calls','kernel_timer_ticks','gui_focus_pids']
 info={n:h.read_u32(S[n],settle=.15) for n in names if n in S}
 info['async_state']=list(h.read_bytes(S['tos_async_state'],1,settle=.15));info['in_guest']=list(h.read_bytes(S['tos_async_in_guest'],1,settle=.15))
 print(name,info,flush=True);(R/(name+'.json')).write_text(json.dumps(info))
def packet(dx,dy,buttons=0):
 head=h.read_bytes(S['ikbd_raw_head'],1,settle=.2)[0];cmds=[]
 # IKBD bit 1=left, bit 0=right; INPUT.SYS converts to native button flags.
 for v in (248|buttons,dx&255,dy&255):
  cmds+=['hatari-debug w b '+hex(S['ikbd_raw_data']+head)+' '+str(v),'hatari-debug w b '+hex(S['ikbd_raw_status']+head)+' 1'];head=(head+1)&255
 cmds+=['hatari-debug w b '+hex(S['ikbd_raw_head'])+' '+str(head)];h.cmd('\n'.join(cmds));time.sleep(.08)
def move(x,y):
 for i in range(16):
  a=h.read_u32(S['mouse_x'],settle=.2);b=h.read_u32(S['mouse_y'],settle=.2)
  dx=max(-100,min(100,x-a));dy=max(-100,min(100,y-b))
  if not dx and not dy:return
  packet(dx,dy)
 raise AssertionError(('mouse could not reach',x,y,a,b))
def click(x,y,double=False):
 move(x,y)
 for i in range(2 if double else 1):packet(0,0,2);packet(0,0,0)
 time.sleep(.5)
def state():
 return h.read_bytes(S['tos_async_state'],1,settle=.2)[0]
def healthy():
 assert h.read_u32(S['kernel_fault_vector'],settle=.2)==0
 assert h.read_u32(S['kdbg_end_status'],settle=.2)==0
def esc():
 h.cmd('hatari-event keydown 0x01');time.sleep(.15);h.cmd('hatari-event keyup 0x01');time.sleep(3)
try:
 time.sleep(40);tap('Left');tap('Return');time.sleep(10);snap('final-tos');assert state()==2;healthy()
 click(760,550)
 for i in range(5):tap('Right')
 tap('Return');time.sleep(9);snap('final-both');assert state()==2;healthy()
 click(300,200);tap('F2');time.sleep(3);snap('final-native-disk');healthy()
 esc();time.sleep(2);click(760,550)
 for i in range(7):tap('Right')
 tap('Return');time.sleep(8);snap('final-plasma-tos');assert state()==2;healthy()
 h.screenshot(R/'final-plasma-tos2.png',settle=2)
 click(400,250);esc();time.sleep(2);healthy()
 click(710,100);time.sleep(4);snap('final-closed');assert state()==0;healthy()
 click(760,550)
 tap('Left')
 for cycle in range(3):
  tap('Return');time.sleep(6);assert state()==2;healthy();snap('final-relaunch-'+str(cycle))
  click(710,100);time.sleep(3);assert state()==0;healthy();click(760,550);tap('Left')
 h.cmd('hatari-shortcut warmreset');time.sleep(40);snap('final-reset');healthy()
 for i in range(8):tap('Right')
 tap('Return');time.sleep(9);tap('Space');time.sleep(3);snap('final-arkanoid');healthy();esc()
 click(760,550);tap('Left');tap('Left');tap('Return');time.sleep(12);snap('final-flashback');healthy()
finally:h.close()
