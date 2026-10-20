#!/usr/bin/env python3
"""NFTEST przez NatFeats w Hatari: jednoznaczny PASS/FAIL.

Bramki z audytu 2026-10-13 (punkt 6): funkcja znaleziona, HELLO=0, niezerowa
sesja, glosy=16 sample=64 rate=44100, linia konca testu i brak linii FAIL.
Brak ktorejkolwiek => FAIL.

Wcielone uwagi z audytu:
  * konfiguracja: nadpisujemy domyslne ustawienia harnessu (STE, 4 MB, 0 TT-RAM,
    16 MHz), ktore z tym obrazem/BIOS-em nie bootowaly (punkt 1),
  * "N" zamiast "NFTEST": wstrzykiwanie klawiszy gubi znaki, alias N.APP wymaga
    jednego klawisza (punkt 7),
  * kod wyjscia emulatora NIE jest kodem wyjscia aplikacji (punkt 1/A),
  * brak linii NFTEST nie jest dowodem bledu klawiatury: trace NatFeats mowi,
    czy gosc w ogole wywolal funkcje.
"""
import shutil, sys, time
from pathlib import Path

REAL = Path(__file__).resolve().parent.parent
sys.path.insert(0, "/Users/xxx/Documents/Deepseek_test/atos2-tools")
from atos2_auto import Hatari

IMG = str(REAL / "work/atos-nftest.img")
LOG = str(REAL / "work/nftest-run.log")
TRACE = str(REAL / "work/nf-trace.log")
TYPED = "N"
BOOT = 50
AFTER = 12

OVERRIDE = [
    "--machine", "megast", "--memsize", "8", "--ttram", "64", "--cpuclock", "32",
    "--cpulevel", "3", "--tf536", "on", "--graphics-card", "vofa", "--gfx-chip", "et4000",
    "--sound", "44100", "--mmu", "off",
    "--trace", "natfeats", "--trace-file", TRACE,
]

(REAL / "work/build").mkdir(parents=True, exist_ok=True)
shutil.copy(str(REAL / "work/audio-baseline/BIOS-tree.bin"), str(REAL / "work/build/bios_mste.bin"))
if not (REAL / "work/atos2.cfg").exists() and (REAL / "atos2.cfg").exists():
    shutil.copy(str(REAL / "atos2.cfg"), str(REAL / "work/atos2.cfg"))

def key(h, k, hold=0.16, gap=0.28):
    h.keydown(k, delay=hold); time.sleep(hold)
    h.keyup(k, delay=0.05); time.sleep(gap)

def typed(h, text):
    for ch in text:
        h.keydown(ch, delay=0.20); time.sleep(0.20)
        h.keyup(ch, delay=0.08); time.sleep(0.45)

gates = []
def gate(name, ok):
    gates.append((name, ok))
    print("  %-30s %s" % (name, "OK" if ok else "FAIL"), flush=True)

h = Hatari(str(REAL / "work"), log=LOG, window=False, image=IMG,
           binary="/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536",
           cfg=True, extra=OVERRIDE)
try:
    print("boot %d s" % BOOT, flush=True)
    time.sleep(BOOT)
    h.press("Q", delay=0.1); time.sleep(1)
    key(h, "F4"); key(h, "Down"); key(h, "Return", gap=0.7)
    typed(h, TYPED)
    time.sleep(0.8)
    key(h, "Return", gap=0.0)
    time.sleep(AFTER)
finally:
    ecc = h.close()

txt = Path(LOG + ".out").read_text(errors="replace") if Path(LOG + ".out").exists() else ""
lines = [l.strip() for l in txt.splitlines() if "NFTEST:" in l]
tr = Path(TRACE)
ttxt = tr.read_text(errors="replace") if tr.exists() else ""
nf_calls = [l for l in ttxt.splitlines() if "NF ID" in l or "NF_CALL" in l or "NF_STDERR" in l]

print("=== raport goscia (%d linii) ===" % len(lines))
for l in lines:
    print(l)
print("=== trace NatFeats: %d linii ===" % len(nf_calls))
for l in nf_calls[:6]:
    print(l)

def has(sub):
    return any(sub in l for l in lines)

sess = 0
for l in lines:
    if l.startswith("NFTEST: HELLO=0 sess="):
        try:
            sess = int(l.split("sess=")[1])
        except ValueError:
            sess = 0

print("=== bramki ===")
gate("funkcja znaleziona", has("NF_ATOS_SOUND znaleziony"))
gate("HELLO=0", any(l.startswith("NFTEST: HELLO=0 ") for l in lines))
gate("sesja niezerowa", sess != 0)
gate("glosy=16 sample=64 rate=44100", has("glosy=16 sample=64 rate=44100"))
gate("LOAD ok", has("NFTEST: LOAD ok"))
gate("PLAY ok", has("NFTEST: PLAY ok"))
gate("CLOCK/STATUS ok", has("NFTEST: CLOCK ok STATUS ok"))
gate("STOP ok", has("NFTEST: STOP ok"))
gate("UNLOAD ok", has("NFTEST: UNLOAD ok"))
gate("WAV x3 jednoczesnie", has("NFTEST: WAV x3 jednoczesnie ok"))
gate("WAV cleanup ok", has("NFTEST: WAV cleanup ok"))
gate("DIAG ok (nic nie zostalo)", has("NFTEST: DIAG ok (nic nie zostalo)"))
gate("koniec testu", has("NFTEST: koniec testu"))
gate("brak linii FAIL", not any("FAIL" in l for l in lines))

ok = all(v for _, v in gates)
if not ok and not nf_calls:
    print("(brak wywolan NF w trace => aplikacja nie wystartowala; to nie dowod bledu NF)")
print("NFTEST: %s (emulator close rc=%s)" % ("PASS" if ok else "FAIL", ecc))
sys.exit(0 if ok else 1)
