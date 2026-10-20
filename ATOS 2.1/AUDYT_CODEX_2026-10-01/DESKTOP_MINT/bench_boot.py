from pathlib import Path
import subprocess,time,sys,json,os
root=Path(os.environ['ATOS_BENCH_PROJECT']).resolve()
label=sys.argv[1];mode=int(sys.argv[2]);elf=Path(sys.argv[3]) if len(sys.argv)>3 else root/'build/system/kernel.elf'
run=root/'work'/label;run.mkdir(exist_ok=True)
img=root/'work/atos-v2.img'
def call(*args):subprocess.run(args,cwd=root,check=True,stdout=subprocess.DEVNULL)
call('python3','tools/make_bios_cfg.py',str(run/'BIOS.CFG'),'--boot-id','128','--boot-device','2','--boot-delay','1','--video-driver','2','--video-mode',str(mode))
for loc in ['--root','BIOS']:
 args=['python3','tools/install_boot_sys.py',str(img),str(run/'BIOS.CFG'),'--data']
 args+=['--root'] if loc=='--root' else ['--directory',loc]
 call(*args)
def syms(p):
 return {s[2]:int(s[0],16) for line in subprocess.check_output(['m68k-elf-nm',str(p)],text=True).splitlines() if len(s:=line.split())==3 and all(c in '0123456789abcdefABCDEF' for c in s[0])}
k=syms(elf);r=syms(root/'build/bios_mste.elf') if (root/'build/bios_mste.elf').exists() else {}
stages={'kernel':k['kernel_main'],'desktop_entry':k['kgfx_enter_desktop'],'first_app_drawn':k['kernel_wm_app_dirty']}
script=[]
for name,addr in stages.items():
 action=run/(name+'.ini');action.write_text(('profile save '+str(run/'rom-profile.txt')+'\nprofile off\n' if name=='kernel' and os.environ.get('PROFILE') else '')+'screenshot '+str(run/(name+'.png'))+'\n')
 script.append(f'b pc = ${addr:x} :once :trace :file {action}')
(run/'boot.ini').write_text(('profile on\n' if os.environ.get('PROFILE') else '')+'\n'.join(script)+'\n')
cmd=[os.environ.get('ATOS_HATARI','hatari-tf536'),'-c',str(root/'atos2.cfg'),'--tos',os.environ.get('BENCH_ROM',str(root/'build/bios_mste.bin')),'--patch-tos','false','--machine','megast','--cpulevel','3','--cpuclock','32','--memsize','4','--ttram','64','--tf536','on','--graphics-card','vofa','--gfx-chip','et4000','--monitor','mono','--ide-master',str(img),'--sound','off','--confirm-quit','false','--disable-video','on','--fast-forward','false','--log-file',str(run/'hatari.log'),'--parse',str(run/'boot.ini')]
for i in range(8):cmd+=['--acsi',str(i)+'=none']
cmd+=['--cmd-fifo',str(run/'control.fifo')]
env=os.environ.copy();env['SDL_VIDEODRIVER']='dummy';env['SDL_AUDIODRIVER']='dummy'
found={};t=time.monotonic()
with (run/'emulator.log').open('w') as f:
 p=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT,env=env,cwd=root)
 try:
  while time.monotonic()-t<100 and p.poll() is None:
   for name in stages:
    if name not in found and (run/(name+'.png')).exists():
     found[name]=round(time.monotonic()-t,2);print(label,name,found[name],flush=True)
   if len(found)==len(stages):
    if os.environ.get('CAPTURE'):
     time.sleep(float(os.environ['CAPTURE']))
     with (run/'control.fifo').open('w') as control:
      control.write('hatari-debug screenshot '+str(run/'desktop-final.png')+'\n');control.flush()
      time.sleep(.4)
    break
   time.sleep(.1)
 finally:
  p.terminate()
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.kill();p.wait()
result={'label':label,'mode':mode,'seconds':found,'elapsed':round(time.monotonic()-t,2),'returncode':p.returncode,'command':cmd}
(run/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result),flush=True)

if len(found)!=len(stages):
 raise SystemExit("Incomplete run: exclude this result from statistics")
