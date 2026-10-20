#!/usr/bin/env python3
"""Konwerter muzyki DOS Flashbacka (standard MIDI) na modul XM dla trackera ESP.

Po co: emulowane ESP (libxmp) odtwarza MODUL, a nie MIDI.  Modul ma kilkadziesiat
KB, wiec wgrywa sie w ulamku sekundy i muzyka startuje OD RAZU (w przeciwienstwie
do wielomegabajtowego PCM, ktory w PRE2 wczytywal sie ~20 s).

Wejscie: plik .MID (format 0/1), np. z GAMES\\FLASHBK\\DATA.
Wyjscie: plik .XM (wersja 0x0104, mono 8-bit sample, 8 kanalow, bez efektow).

Synteza: kazdy program GM dostaje rodzine przebiegu (sinus/saw/square/noise) i
obwiednie; kanal perkusyjny (10) dostaje krotkie sample perkusyjne.  To nie jest
wierne brzmienie AdLib z wersji DOS - to czytelna, "trackerowa" aranzacja tej
samej muzyki.  Jakosc mozna poprawiac rodzinami w TABLE ponizej.
"""
import struct, sys, math, os

# ---------------------------------------------------------------- MIDI ------
def read_varlen(b, i):
    v = 0
    while True:
        c = b[i]; i += 1
        v = (v << 7) | (c & 0x7F)
        if not (c & 0x80):
            return v, i

def parse_midi(data):
    if data[:4] != b"MThd":
        raise ValueError("to nie jest MIDI (brak MThd)")
    fmt, ntrk, div = struct.unpack_from(">HHH", data, 8)
    pos, events = 14, []
    tempo = 500000
    for _ in range(ntrk):
        if data[pos:pos+4] != b"MTrk":
            break
        ln = struct.unpack_from(">I", data, pos+4)[0]
        trk = data[pos+8:pos+8+ln]; pos += 8 + ln
        i, tick, running = 0, 0, 0
        while i < len(trk):
            delta, i = read_varlen(trk, i); tick += delta
            if i >= len(trk):
                break
            st = trk[i]
            if st == 0xFF:
                i += 1; typ = trk[i]; i += 1
                ln2, i = read_varlen(trk, i)
                if typ == 0x51 and ln2 == 3:
                    tempo = (trk[i] << 16) | (trk[i+1] << 8) | trk[i+2]
                i += ln2; continue
            if st in (0xF0, 0xF7):
                i += 1
                ln2, i = read_varlen(trk, i); i += ln2; continue
            if st & 0x80:
                running = st; i += 1
            else:
                st = running
            hi = st & 0xF0; ch = st & 0x0F
            if hi in (0x90, 0x80):
                n, v = trk[i], trk[i+1]; i += 2
                on = (hi == 0x90 and v > 0)
                events.append((tick, "on" if on else "off", ch, n, v))
            elif hi == 0xC0:
                events.append((tick, "prog", ch, trk[i], 0)); i += 1
            elif hi in (0xA0, 0xB0, 0xE0):
                i += 2
            else:
                i += 1
    events.sort(key=lambda e: e[0])
    return div, tempo, events

def build_notes(div, events):
    """Noty z par on/off + program obowiazujacy na kanale."""
    prog = {}; active = {}; notes = []
    for tick, kind, ch, n, v in events:
        if kind == "prog":
            prog[ch] = n
        elif kind == "on":
            active.setdefault((ch, n), []).append((tick, v, prog.get(ch, 0)))
        elif kind == "off":
            st = active.get((ch, n))
            if st:
                t0, vel, pr = st.pop(0)
                notes.append((t0, max(tick, t0 + 1), n, vel, pr, ch == 9))
    for (ch, n), st in active.items():           # niedomkniete noty
        for t0, vel, pr in st:
            notes.append((t0, t0 + div // 2, n, vel, pr, ch == 9))
    notes.sort(key=lambda x: (x[0], x[2]))
    return notes

# ------------------------------------------------------------ synteza ------
# rodziny GM: (przebieg, atak, decay, sustain, release) w sekundach / 0..1
FAMILIES = [
    (16, "harm", 0.004, 0.35, 0.25, 0.10),   # 0  piano
    (8,  "sine", 0.002, 0.25, 0.10, 0.08),   # 8  chromatic perc
    (8,  "organ",0.010, 0.10, 0.80, 0.05),   # 16 organ
    (8,  "pluck",0.003, 0.30, 0.20, 0.08),   # 24 guitar
    (8,  "bass", 0.005, 0.40, 0.45, 0.10),   # 32 bass
    (8,  "str",  0.060, 0.30, 0.70, 0.25),   # 40 strings
    (8,  "harm", 0.030, 0.30, 0.60, 0.20),   # 48 ensemble
    (8,  "brass",0.030, 0.20, 0.65, 0.12),   # 56 brass
    (8,  "reed", 0.020, 0.20, 0.70, 0.10),   # 64 reed
    (8,  "sine", 0.040, 0.20, 0.75, 0.15),   # 72 pipe
    (8,  "square",0.008,0.25, 0.55, 0.10),   # 80 synth lead
    (8,  "pad",  0.080, 0.40, 0.70, 0.30),   # 88 pad
    (8,  "noise",0.005,0.30, 0.20, 0.10),    # 96 FX
    (8,  "pluck",0.004,0.30, 0.30, 0.10),    # 104 ethnic
    (8,  "perc", 0.002,0.18, 0.05, 0.06),    # 112 percussive
    (8,  "noise",0.002,0.20, 0.10, 0.08),    # 120 SFX
]
def family(program):
    for lo, wave, a, d, s, r in FAMILIES:
        if lo <= program < lo + 8:
            return wave, a, d, s, r
    return "sine", 0.01, 0.3, 0.5, 0.1

def wave_sample(wave, n, phase=0.0):
    if wave == "sine":   return math.sin(2 * math.pi * n + phase)
    if wave == "saw":    return 2.0 * ((n + phase) % 1.0) - 1.0
    if wave == "square": return 1.0 if ((n + phase) % 1.0) < 0.5 else -1.0
    if wave == "organ":  return 0.6 * math.sin(2*math.pi*n) + 0.4 * math.sin(2*math.pi*n*2)
    if wave == "bass":   return 0.7 * math.sin(2*math.pi*n) + 0.3 * (2.0*((n)%1.0)-1.0)
    if wave == "brass":  return 0.5*math.sin(2*math.pi*n) + 0.3*math.sin(4*math.pi*n) + 0.2*math.sin(6*math.pi*n)
    if wave == "reed":   return math.copysign(1.0, math.sin(2*math.pi*n)) * 0.7 + 0.3*math.sin(2*math.pi*n)
    if wave == "str":    return 0.6*math.sin(2*math.pi*n) + 0.25*math.sin(4*math.pi*n) + 0.15*math.sin(6*math.pi*n)
    if wave == "pad":    return 0.5*math.sin(2*math.pi*n) + 0.5*math.sin(4*math.pi*n*0.5)
    if wave == "harm":   return (math.sin(2*math.pi*n) + 0.5*math.sin(4*math.pi*n) + 0.25*math.sin(6*math.pi*n)) / 1.75
    if wave == "pluck":  return (math.sin(2*math.pi*n) + 0.6*math.sin(4*math.pi*n) + 0.3*math.sin(8*math.pi*n)) / 1.9
    if wave == "perc":   return math.sin(2*math.pi*n) * (1.0 - (n % 1.0))
    return 0.0
def noise_sample(n, seed):
    x = (seed * 1103515245 + 12345) & 0x7FFFFFFF
    return ((x >> 10) % 2000) / 1000.0 - 1.0

BASE_NOTE = 49                       # XM: 49 = C-4; MIDI 60 = C4
SAMPLE_RATE = 22050

def render_instrument(wave, a, d, s, r, seconds):
    n = int(SAMPLE_RATE * seconds)
    out = bytearray(n)
    for i in range(n):
        t = i / SAMPLE_RATE
        if t < a:              env = t / a
        elif t < a + d:        env = 1.0 - (1.0 - s) * (t - a) / d
        else:                  env = s * max(0.0, 1.0 - (t - a - d) / r) if r > 0 else s
        v = wave_sample(wave, 261.6256 * t) if wave != "noise" else noise_sample(0, i + 7)
        out[i] = max(0, min(255, int(128 + 120 * v * env)))
    return bytes(out)

def render_drum(note):
    dur = 0.16 if note in (35, 36, 38, 40) else 0.06
    n = int(SAMPLE_RATE * dur); out = bytearray(n)
    for i in range(n):
        t = i / SAMPLE_RATE
        if note in (35, 36):                     # stopa: sinus opadajacy
            f = 110.0 * math.exp(-14 * t)
            v = math.sin(2 * math.pi * f * t) * math.exp(-9 * t)
        elif note in (38, 40):                   # werbel: szum + ton
            v = (0.7 * noise_sample(0, i * 3 + 1) + 0.3 * math.sin(2*math.pi*190*t)) * math.exp(-16 * t)
        else:                                     # hi-hat / inne: krotki szum
            v = noise_sample(0, i * 5 + 2) * math.exp(-40 * t)
        out[i] = max(0, min(255, int(128 + 120 * v)))
    return bytes(out)

# --------------------------------------------------------------- XM --------
def delta_encode(pcm):
    out = bytearray(len(pcm)); prev = 0
    for i, b in enumerate(pcm):
        out[i] = (b - prev) & 0xFF; prev = b
    return bytes(out)

def write_xm(path, name, channels, bpm, speed, patterns, instruments, orders):
    """instruments: lista dict(name, pcm) ; patterns: lista [rows][channels] = (note,inst,vol)"""
    out = bytearray()
    out += b"Extended Module: "
    out += name.encode("ascii", "replace")[:20].ljust(20, b" ")
    out += b"\x1a" + b"ATOS midi2xm".ljust(20, b" ")
    hdr = struct.pack("<HIHHHHH", 0x0104, 276, len(orders), 0, channels,
                      len(patterns), len(instruments))
    hdr += struct.pack("<HHH", 1, speed, bpm)          # flags=linear, tempo(speed), bpm
    hdr += bytes(256)
    out += hdr
    # pattern data najpierw (rozmiary w naglowkach)
    pat_blobs = []
    for rows in patterns:
        blob = bytearray()
        for row in rows:
            for cell in row:
                note, inst, vol = cell
                if not (note or inst or vol):
                    blob.append(0x80); continue
                b = 0x80
                payload = bytearray()
                if note: b |= 1;  payload.append(note)
                if inst: b |= 2;  payload.append(inst)
                if vol:  b |= 4;  payload.append(vol)
                blob.append(b); blob += payload
        pat_blobs.append(bytes(blob))
    for blob, rows in zip(pat_blobs, patterns):
        out += struct.pack("<IBHH", 9, 0, len(rows), len(blob)) + blob
    for ins in instruments:
        pcm = ins["pcm"]; enc = delta_encode(pcm); n = len(pcm)
        # Rozmiar 241 = XM_INST_HEADER_SIZE(29) + XM_INST_SIZE(212).  Loader
        # libxmp czyta 29+4 (rozmiar naglowka sampla) i 208 bajtow i liczy
        # pozycje jako size-(29+212); inne wartosci rozjezdzaja naglowki sampli.
        out += struct.pack("<I", 241)
        out += ins["name"].encode("ascii", "replace")[:22].ljust(22, b"\0")
        out += struct.pack("<BH", 0, 1)                # typ, liczba sampli
        out += struct.pack("<I", 40)                   # rozmiar naglowka sampla
        out += bytes(96)                               # mapa nut -> sample
        out += bytes(48) + bytes(48)                   # obwiednie vol/pan
        out += bytes(8)                                # punkty/sustain/petle
        out += bytes(2)                                # typy obwiedni
        out += bytes(4)                                # vibrato: wave/sweep/depth/rate
        out += struct.pack("<H", 0)                    # fadeout
        # Naglowek sampla (40 B): 3x uint32 (dlugosc, start petli, dlugosc petli),
        # potem 6 pojedynczych bajtow: glosnosc, finetune, typ, panning,
        # relative note, reserved - i nazwa 22 B.  Cztery uint32 przesuwaly sample.
        out += struct.pack("<III", n, 0, 0)
        out += struct.pack("<BBBBBB", 64, 0, 0, 128, 0, 0)
        out += ins["name"].encode("ascii", "replace")[:22].ljust(22, b"\0")
        out += enc
    with open(path, "wb") as f:
        f.write(bytes(out))
    return len(out)

def convert(mid_path, xm_path, title=None, max_channels=8, max_seconds=240.0):
    data = open(mid_path, "rb").read()
    div, tempo, events = parse_midi(data)
    notes = build_notes(div, events)
    if not notes:
        return None
    row_ticks = max(1, div // 4)
    bpm = max(32, min(255, int(round(60000000.0 / max(1, tempo)))))
    last_tick = min(max(n[1] for n in notes), int(max_seconds * 1000000.0 / max(1, tempo) * div))
    total_rows = max(1, (last_tick + row_ticks - 1) // row_ticks + 1)
    # instrumenty: programy melodic + perkusja per nuta
    used, drums = set(), set()
    for t0, t1, n, v, pr, is_drum in notes:
        if is_drum: drums.add(n)
        else:       used.add(pr)
    instruments, idx = [], {}
    for pr in sorted(used):
        wave, a, d, s, r = family(pr)
        pcm = render_instrument(wave, a, d, s, r, min(0.9, a + d + 0.35))
        idx[("m", pr)] = len(instruments) + 1
        instruments.append({"name": f"P{pr:03d} {wave}", "pcm": pcm, "base": BASE_NOTE})
    for nd in sorted(drums):
        pcm = render_drum(nd)
        idx[("d", nd)] = len(instruments) + 1
        instruments.append({"name": f"DR{nd}", "pcm": pcm, "base": BASE_NOTE})
    # alokacja glosow: noty -> kanaly (najstarsza zwolniona)
    chan_end = [0] * max_channels
    rows = [[(0, 0, 0) for _ in range(max_channels)] for _ in range(total_rows)]
    for t0, t1, n, v, pr, is_drum in notes:
        r0 = t0 // row_ticks
        if r0 >= total_rows: break
        try:
            ch = min(range(max_channels), key=lambda c: chan_end[c])
        except ValueError:
            break
        chan_end[ch] = max(chan_end[ch], t1 // row_ticks)
        note = max(1, min(96, (n if is_drum else n) - 60 + BASE_NOTE))
        vol = max(1, min(64, v // 4))
        inst = idx[("d", n)] if is_drum else idx.get(("m", pr), 1)
        if rows[r0][ch] == (0, 0, 0):
            rows[r0][ch] = (note, inst, 0x10 + vol)
        else:
            # kanal zajety w tej komorce: szukaj wolnego
            for c in range(max_channels):
                if rows[r0][c] == (0, 0, 0):
                    rows[r0][c] = (note, inst, 0x10 + vol); break
    # patterny po 64 rzedy
    patterns = [rows[i:i+64] for i in range(0, total_rows, 64)]
    orders = list(range(len(patterns)))
    name = (title or os.path.basename(mid_path))[:20]
    size = write_xm(xm_path, name, max_channels, bpm, 6, patterns, instruments, orders)
    return {"notes": len(notes), "instruments": len(instruments), "patterns": len(patterns),
            "rows": total_rows, "bpm": bpm, "bytes": size}

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(2)
    info = convert(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    print(info)
