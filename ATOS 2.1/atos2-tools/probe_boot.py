import sys, struct, time, signal, subprocess
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
signal.signal(signal.SIGALRM, lambda *a: (_ for _ in ()).throw(SystemExit("TIMEOUT")))
signal.alarm(90)
def syms():
    out = subprocess.run(["m68k-elf-nm", P + "/build/system/kernel.elf"], capture_output=True, text=True).stdout
    return {p[2]: int(p[0], 16) for p in (l.split() for l in out.splitlines()) if len(p) == 3}
S = syms()
def A(n): return S.get(n)
WM=A("wm");STR=60
K={k:A(k) for k in ("kdbg_end_pid","kdbg_end_name","kdbg_end_status","kdbg_kbpid","kdbg_rel_clear","kdbg_end_pc","kdbg_end_image")}
h=Hatari(P,log=P+"/work/hatari-boot.log",window=True)
def u32(a):
    b=h.read_bytes(a,4,settle=0.2);return struct.unpack(">I",b)[0] if b and len(b)==4 else None
def cnt(): return u32(WM+8*60) or 0
try:
    h.wait(28)
    print("kbpid",u32(K["kdbg_kbpid"]),"end",{k:u32(v) for k,v in K.items()},flush=True)
    n=cnt();print("count",n,flush=True)
    for i in range(min(n,8)):
        b=h.read_bytes(WM+i*STR,58,settle=0.2)
        x,y,w2,h2=struct.unpack(">hhHH",b[0:8]);fl=b[20]
        t=b[28:52].split(b"\x00")[0].decode("latin1")
        p=struct.unpack(">I",b[16:20])[0]
        print("win",i,x,y,w2,h2,"fl",fl,"pid",hex(p),"title",repr(t),flush=True)
finally:
    try: print("rc",h.close(),flush=True)
    except Exception as e: print("close-err",e,flush=True)
