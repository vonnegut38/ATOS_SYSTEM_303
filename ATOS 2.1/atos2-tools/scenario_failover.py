import re, subprocess, sys, time
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari
P = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
HB = "/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536"
IMG = P + "/work/atos-v2.img"
NOACSI = []
for i in range(8): NOACSI += ["--acsi", "%d=none" % i]
EXTRA = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4",
         "--ttram","64","--tf536","on","--graphics-card","vofa","--gfx-chip","et4000","--mono",
         "--ide-master", IMG] + NOACSI

BIOS_TEXT = 0x00000D70     # BIOS 80x25 character mirror
RH_BASE   = 0x0003901e       # kernel copy of the ROM handoff (nm runtime_handoff)
RH_NOVA   = RH_BASE + 58      # nova_initialized
RH_FORCE  = RH_BASE + 59      # video_force_shifter
K_TICKS   = 0x00038830        # kernel_timer_ticks: >0 proves the kernel is running

def hex_bytes(text):
    out = []
    for line in text.splitlines():
        if not re.match(r"^[0-9A-Fa-f]{8}:", line):
            continue
        for tok in line.split(":", 1)[1][:48].split():
            if len(tok) == 2 and all(c in "0123456789abcdefABCDEF" for c in tok):
                out.append(int(tok, 16))
    return bytes(out)

def bios_text(h):
    raw = hex_bytes(h.read_mem(BIOS_TEXT, 80*25, settle=0.6))
    return "".join(chr(c) if 32 <= c < 127 else " " for c in raw)

def flags(h):
    w = h.read_u32(RH_NOVA, settle=0.5)
    if w is None: return None, None
    return (w >> 24) & 0xff, (w >> 16) & 0xff

def probe():
    r = subprocess.run(["python3","tools/probe_bios_cfg.py"], cwd=P,
                       check=True, capture_output=True, text=True)
    out = r.stdout.strip().replace("\n", " | ")
    print("  probe:", out, flush=True)
    # return the video_pending byte of the last config line (canonical \\BIOS\\BIOS.CFG)
    pend = re.findall(r"video_pending (\d+)", out)
    return int(pend[-1]) if pend else None

fails = []
def check(name, cond, detail=""):
    print(("PASS " if cond else "FAIL ") + name + ("  " + detail if detail else ""), flush=True)
    if not cond: fails.append(name)

def boot(tag, wait_shell):
    h = Hatari(P, log=P + "/work/hatari-" + tag + ".log", window=False,
               image=IMG, binary=HB, cfg=True, extra=EXTRA)
    try:
        snaps, pos = [], 0.0
        for t in (1.2, 2.0, 3.0):
            time.sleep(max(0.0, t - pos)); pos = t
            snaps.append(bios_text(h))
        nov, frc, ktk = None, None, None
        if wait_shell:
            time.sleep(max(0.0, wait_shell - pos))
            nov, frc = flags(h)
            ktk = h.read_u32(K_TICKS, settle=0.5)
        return "\n".join(snaps), nov, frc, ktk
    finally:
        print("  rc", tag, h.close(), flush=True)

print("=== TEST A: two consecutive card boots; ROM latch must self-clear ===", flush=True)
for tag in ("failoverA1", "failoverA2"):
    txt, nov, frc, ktk = boot(tag, 30.0)
    check(tag + " kernel running", bool(ktk), "kernel_timer_ticks=%r" % (ktk,))
    check(tag + " nova_initialized==1", nov == 1, "nova=%r force=%r" % (nov, frc))
    check(tag + " video_force_shifter==0", frc == 0, "force=%r" % (frc,))
    check(tag + " no CARD DISABLED in ROM log", "CARD DISABLED" not in txt)
    pend = probe()
    check(tag + " on-disk latch cleared", pend == 0, "pending=%r" % (pend,))

print("=== install BIOS.CFG with video_pending=1 (canonical \\BIOS\\BIOS.CFG) ===", flush=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0",
                "--boot-delay","3","--video-pending","1"], cwd=P, check=True)
subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG",
                "--directory","BIOS","--data"], cwd=P, check=True)

print("=== TEST B: pending latch forces the Shifter and is reported ===", flush=True)
txt, nov, frc, ktk = boot("failoverB", 40.0)
check("B kernel running", bool(ktk), "kernel_timer_ticks=%r" % (ktk,))
check("B nova_initialized==0", nov == 0, "nova=%r force=%r" % (nov, frc))
check("B video_force_shifter==1", frc == 1, "force=%r" % (frc,))
check("B ROM log has CARD DISABLED AFTER FAILED BOOT",
      "CARD DISABLED AFTER FAILED BOOT" in txt, repr(txt[-400:]))

print("=== restore clean BIOS.CFG (video_pending=0) in BIOS dir and root ===", flush=True)
subprocess.run(["python3","tools/make_bios_cfg.py","work/BIOS.CFG","--auto-tos","0",
                "--boot-delay","1"], cwd=P, check=True)
for extra in (["--directory","BIOS","--data"], ["--root","--data"]):
    subprocess.run(["python3","tools/install_boot_sys.py","work/atos-v2.img","work/BIOS.CFG"] + extra,
                   cwd=P, check=True)

print("RESULT:", "ALL PASS" if not fails else ("FAILURES: " + ", ".join(fails)), flush=True)
