from pathlib import Path
import sys,time,shutil,subprocess,json,struct
P=Path('/Users/xxx/Documents/Deepseek_test/ATOS 2.1');R=Path(__file__).resolve().parent
sys.path.insert(0,str(P/'atos2-tools'));from atos2_auto import Hatari
D=R/'stack';D.mkdir(exist_ok=True);(D/'build').mkdir(exist_ok=True)
shutil.copy2(P/'build/bios_mste.bin',D/'build/bios_mste.bin');shutil.copy2(P/'atos2.cfg',D/'atos2.cfg')
subprocess.run(['cp','-c',str(R/'before.img'),str(R/'stack.img')],check=True)
for f,d in [('KERNEL.SYS','SYSTEM'),('DESKTOP.APP','SYSTEM/APP'),('PLASMA.APP','SYSTEM/APP')]:subprocess.run(['python3',str(P/'tools/install_boot_sys.py'),str(R/'stack.img'),str(P/'build/system'/f),'--directory',d],check=True)
shutil.copy2(P/'build/system/kernel.elf',R/'stack-kernel.elf')
E=['--machine','megast','--cpulevel','3','--cpuclock','32','--memsize','8','--ttram','64','--tf536','on','--graphics-card','vofa','--gfx-chip','et4000','--mono','--fast-forward','off','--ide-master',str(R/'stack.img'),'--mmu','off','--statusbar','false','--borders','false','--zoom','1']
for i in range(8):E+=['--acsi',f'{i}=none']
subprocess.run(['python3',str(P/'tools/install_boot_sys.py'),str(R/'stack.img'),str(R/'stack/SYSINFO.PRG'),'--directory','SYSTEM/TEST'],check=True)
h=Hatari(D,window=False,image=R/'stack.img',binary=str(R/'hatari-reset-fixed'),extra=E,render=True)
S={a[2]:int(a[0],16) for a in (l.split() for l in subprocess.check_output(['m68k-elf-nm',str(R/'stack-kernel.elf')],text=True).splitlines()) if len(a)==3}
def tap(k):h.keydown(k);time.sleep(.15);h.keyup(k);time.sleep(.3)
def snap(name):
 h.screenshot(R/(name+'.png'),settle=.5)
 names=['kernel_fault_vector','kernel_fault_pc','kdbg_end_status','kdbg_end_name','kdbg_end_pc','kernel_tos_async_result','tos_aes_log_n','kdbg_loop_calls','kernel_timer_ticks','gui_focus_pids']
 info={n:h.read_u32(S[n],settle=.15) for n in names if n in S}
 info['async_state']=list(h.read_bytes(S['tos_async_state'],1,settle=.15));info['in_guest']=list(h.read_bytes(S['tos_async_in_guest'],1,settle=.15))
 print(name,info,flush=True);(R/(name+'.json')).write_text(json.dumps(info))
def packet(dx,dy,buttons=0):
 head=h.read_bytes(S['ikbd_raw_head'],1,settle=.06)[0];cmds=[]
 # IKBD bit 1=left, bit 0=right; INPUT.SYS converts to native button flags.
 for v in (248|buttons,dx&255,dy&255):
  cmds+=['hatari-debug w b '+hex(S['ikbd_raw_data']+head)+' '+str(v),'hatari-debug w b '+hex(S['ikbd_raw_status']+head)+' 1'];head=(head+1)&255
 cmds+=['hatari-debug w b '+hex(S['ikbd_raw_head'])+' '+str(head)];h.cmd('\n'.join(cmds));time.sleep(.08)
def move(x,y):
 for i in range(16):
  a=h.read_u32(S['mouse_x'],settle=.06);b=h.read_u32(S['mouse_y'],settle=.06)
  dx=max(-100,min(100,x-a));dy=max(-100,min(100,y-b))
  if not dx and not dy:return
  packet(dx,dy)
 raise AssertionError(('mouse could not reach',x,y,a,b))
def click(x,y,double=False):
 move(x,y)
 for i in range(2 if double else 1):packet(0,0,2);packet(0,0,0)
 time.sleep(.5)
try:
 time.sleep(40);tap('Left')
 for n in range(4):
  tap('Return');time.sleep(5);snap('stack-probe-'+str(n))
  assert h.read_u32(S['kernel_fault_vector'],settle=.2)==0
  assert h.read_u32(S['kernel_tos_async_result'],settle=.2)==0
  assert h.read_bytes(S['tos_async_state'],1,settle=.2)==bytes([0])
finally:h.close()
