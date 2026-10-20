#!/usr/bin/env python3
"""Pomiar pamieci ATOS: rozklad pul ST/TT z podzialem na wlascicieli.

Adresy symboli st_pool/tt_pool czytane z ELF jadra (zmieniaja sie przy kazdej
przebudowie), potem boot obrazu w Hatari i przejscie list blokow przez
hatari-debug memdump.  Uzycie:  mem_measure.py <obraz.img> [--shot plik.png]
"""
import sys, os, re, time, struct, subprocess

WS = "/Users/xxx/Documents/Deepseek_test"
ELF = f"{WS}/ATOS 2.1/build/system/kernel.elf"
NM = "/opt/homebrew/opt/m68k-elf-binutils/bin/m68k-elf-nm"
HAT = f"{WS}/display-demo/hatari-tracker-20261013"

def symbols():
    out = subprocess.run([NM, ELF], capture_output=True, text=True).stdout
    sym = {}
    for line in out.splitlines():
        f = line.split()
        if len(f) == 3 and f[2] in ("st_pool", "tt_pool", "kdbg_uf_count"):
            sym[f[2]] = int(f[0], 16)
    return sym

def main():
    image = sys.argv[1]
    shot = None
    if "--shot" in sys.argv:
        shot = sys.argv[sys.argv.index("--shot") + 1]
    sym = symbols()
    sys.path.insert(0, f"{WS}/ATOS 2.1/atos2-tools")
    from atos2_auto import Hatari
    extra = ["--machine","megast","--cpulevel","3","--cpuclock","32","--memsize","4","--ttram","64",
             "--tf536","on","--mmu","off","--graphics-card","vofa","--gfx-chip","et4000","--mono",
             "--ide-master",image,"--acsi","0=none","--sound","44100","--natfeats","on",
             "--statusbar","false","--borders","false","--zoom","1","--confirm-quit","false",
             "--fast-forward","off"]
    h = Hatari(f"{WS}/ATOS 2.1", cfg=False, image=image, binary=HAT, render=True, window=False, extra=extra)
    time.sleep(20)
    def rd(addr, count, delay=0.45):
        pos = os.path.getsize(h.capture_path)
        h.cmd(f"hatari-debug memdump {hex(addr)} {count}")
        time.sleep(delay)
        with open(h.capture_path, errors="replace") as f:
            f.seek(pos); t = f.read()
        d = []
        for line in t.splitlines():
            if not re.match(r"^[0-9A-Fa-f]{8}:", line): continue
            for tok in line.split(":", 1)[1].split("  ", 1)[0].split():
                if len(tok) == 2 and all(c in "0123456789abcdefABCDEF" for c in tok):
                    d.append(int(tok, 16))
        return bytes(d[:count])
    def u32(b, o): return struct.unpack_from(">I", b, o)[0]
    def u16(b, o): return struct.unpack_from(">H", b, o)[0]
    st = rd(sym["st_pool"], 16); tt = rd(sym["tt_pool"], 16)
    print(f"st_pool@{sym['st_pool']:#x}  tt_pool@{sym['tt_pool']:#x}")
    def walk(first, label, limit=48):
        agg = {}; free = 0; rows = []; addr = first
        for _ in range(limit):
            if addr == 0: break
            b = rd(addr, 32)
            if u32(b, 0) != 0x4b4d454d: break
            size = u32(b, 4); owner = u16(b, 8); flags = u16(b, 10); nxt = u32(b, 12)
            rows.append((addr, size, owner, flags))
            if owner == 0: free += size
            else: agg[owner] = agg.get(owner, 0) + size
            addr = nxt
        used = sum(agg.values())
        print(f"\n=== {label}: pula {u32(st if label=='ST' else tt,4)/1024:.0f} KiB, "
              f"zajete {used/1024:.1f} KiB, wolne {free/1024:.1f} KiB ===")
        for a, s, o, f in sorted(rows, key=lambda r: -r[1])[:8]:
            kind = "WOLNY" if o == 0 else f"owner={o}"
            print(f"   {a:#08x} {s/1024:>8.1f} KiB  {kind:<11} flags={f:#06x}")
        return free, agg
    stfree, sta = walk(u32(st, 0), "ST")
    ttfree, tta = walk(u32(tt, 0), "TT")
    if shot:
        h.screenshot(shot)
    h.close()
    print(f"\nPODSUMOWANIE: ST wolne {stfree/1024:.1f} KiB")
    return 0

if __name__ == "__main__":
    sys.exit(main())
