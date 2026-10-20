#!/usr/bin/env python3
"""Wytnij 48 ikon z arkusza ATOS MODERN ICON SET v2.0 (sekcja 16 kolorow).

Wszystko jest wyznaczane Z OBRAZU - zadnych wspolrzednych na sztywno.  Poprzednia
wersja ekstrapolowala srodkowe kolumny z kroku siatki i obcinala ikony, ktore byly
szersze od przyjetego okna.

Dla kazdej komorki:
  1. wyznacz wiersze z trescia w oknie komorki,
  2. sklej bloki oddzielone malymi odstepami i wybierz NAJWIEKSZY (to ikona;
     mniejszy bloczek nizej to etykieta tekstowa),
  3. w tym bloku wyznacz zakres kolumn,
  4. wytnij dokladnie ten obrys, wpasuj w 44x44 i wysrodkuj na 48x48.

Tlo arkusza (16,31,39) jest usuwane przez odtworzenie alfy z odleglosci koloru,
z odpakowaniem premnozenia na krawedziach (bez ciemnej obwodki).
"""
from PIL import Image
import os

SHEET  = "work/iconset/sheet.png"
OUTDIR = "work/iconset/cut"
SIZE   = 48
BODY   = 44
BG     = (16, 31, 39)
ALPHA_FULL = 46
ROWS   = 3            # sekcja 16 kolorow: 3 rzedy po 16 ikon
COLS   = 16

NAMES = [
 "komputer","folder","dokument","plik_tekstowy","dyskietka","dysk","cd_dvd","kosz",
 "ustawienia","informacje","pomoc","szukaj","internet","poczta","kalendarz","kalkulator",
 "nc","terminal","edytor","obraz","muzyka","wideo","grafika","kamera",
 "dzwiek","pobierz","wyslij","archiwum","kompresja","siec","siec_alt","drukarka",
 "narzedzia","monitor","system","usb","konfiguracja","polaczenia","gry","szachy",
 "karty","strategia","platformowe","wyscigi","arkanoid","pinball","wylacz","o_atos",
]

def content(px, x, y, w, h):
    if x < 0 or y < 0 or x >= w or y >= h:
        return False
    r, g, b = px[x, y]
    return max(abs(r-BG[0]), abs(g-BG[1]), abs(b-BG[2])) > 34

def find_icon_box(px, w, h, x0, y0, x1, y1):
    """Obrys ikony w oknie komorki: najwiekszy spojny blok wierszy."""
    rows = []
    for y in range(y0, y1):
        n = 0
        for x in range(x0, x1):
            if content(px, x, y, w, h):
                n += 1
        rows.append((y, n))
    blocks = []
    cur = None
    for y, n in rows:
        if n > 0:
            if cur is None:
                cur = [y, y]
            else:
                cur[1] = y
        else:
            if cur is not None:
                # maly odstep (<=3 px) jeszcze nalezy do tej samej ikony
                blocks.append(cur)
                cur = None
    if cur is not None:
        blocks.append(cur)
    merged = []
    for b in blocks:
        if merged and b[0] - merged[-1][1] <= 4:
            merged[-1][1] = b[1]
        else:
            merged.append(list(b))
    if not merged:
        return None
    best = max(merged, key=lambda b: b[1]-b[0])
    bx0, bx1 = x1, x0
    for y in range(best[0], best[1]+1):
        for x in range(x0, x1):
            if content(px, x, y, w, h):
                if x < bx0: bx0 = x
                if x > bx1: bx1 = x
    return (bx0, best[0], bx1+1, best[1]+1)

def to_rgba(cell):
    w, h = cell.size
    src = cell.load()
    out = Image.new("RGBA", (w, h))
    dst = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = src[x, y]
            d = max(abs(r-BG[0]), abs(g-BG[1]), abs(b-BG[2]))
            a = 255 if d >= ALPHA_FULL else int(d*255/ALPHA_FULL)
            if a == 0:
                dst[x, y] = (0, 0, 0, 0)
                continue
            if a < 255:
                f = a/255.0
                r = max(0, min(255, int((r-BG[0]*(1-f))/f)))
                g = max(0, min(255, int((g-BG[1]*(1-f))/f)))
                b = max(0, min(255, int((b-BG[2]*(1-f))/f)))
            dst[x, y] = (r, g, b, a)
    return out

def fit(im):
    w, h = im.size
    s = min(BODY/w, BODY/h)
    nw, nh = max(1, int(round(w*s))), max(1, int(round(h*s)))
    im = im.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    canvas.paste(im, ((SIZE-nw)//2, (SIZE-nh)//2), im)
    return canvas

def icon_row_bands(px, W, H):
    """Pasma wierszy z TRESCA, podzielone na ikony (wysokie) i etykiety (niskie).

    Wysokosc pasma rozstrzyga: ikony maja ~55-61 px, etykiety ~12-16 px.  Bierzemy
    tylko pasma dalekie od gornej czesci arkusza (naglowek i stopka sa pomijane
    przez filtr wysokosci i minimalnej liczby wierszy tresci)."""
    prof = []
    for y in range(H):
        n = 0
        for x in range(0, W, 3):
            if content(px, x, y, W, H):
                n += 1
        prof.append(n)
    bands = []
    cur = None
    for y in range(H):
        if prof[y] > 1:
            if cur is None: cur = [y, y]
            else: cur[1] = y
        else:
            if cur is not None: bands.append(cur); cur = None
    if cur is not None: bands.append(cur)
    return [b for b in bands if 45 <= (b[1]-b[0]) <= 70]

def main():
    os.makedirs(OUTDIR, exist_ok=True)
    im = Image.open(SHEET).convert("RGB")
    W, H = im.size
    px = im.load()
    cw = W / float(COLS)
    bands = icon_row_bands(px, W, H)[:ROWS]
    if len(bands) < ROWS:
        raise SystemExit("nie wykryto %d pasm ikon (znaleziono %d)" % (ROWS, len(bands)))
    print("pasma ikon:", [(b[0], b[1]) for b in bands])
    # Kolumny wykrywamy TYLKO z pierwszego rzedu: arkusz jest tabela, wiec
    # wszystkie rzedy maja te same kolumny.  Wczesniejsza wersja kalibrowala sie
    # na "runach" kazdego rzedu osobno - a te rozjezdzaly sie na fragmenty
    # (ikony z odlaczonymi elementami), przez co siatka byla przesunieta i ikony
    # wychodzily uciete z jednej strony, z paskiem sasiada z drugiej.
    by0, by1 = bands[0]
    prof = []
    for x in range(W):
        n = 0
        for y in range(by0, by1+1):
            if content(px, x, y, W, H):
                n += 1
        prof.append(n)
    runs = []
    cur = None
    for x in range(W):
        if prof[x] > 0:
            if cur is None: cur = [x, x]
            else: cur[1] = x
        else:
            if cur is not None: runs.append(cur); cur = None
    if cur is not None: runs.append(cur)
    merged = []
    for r in runs:
        if merged and r[0]-merged[-1][1] <= 3: merged[-1][1] = r[1]
        else: merged.append(list(r))
    merged = [r for r in merged if r[1]-r[0] >= 20]
    if len(merged) != COLS:
        raise SystemExit("rzad 1: wykryto %d kolumn, oczekiwano %d" % (len(merged), COLS))
    print("kolumny:", [(r[0], r[1]) for r in merged])

    # Srodki kolumn z pierwszego rzedu - to one sa wyrownane w tabeli.
    # Szerokosci ikon roznia sie miedzy rzedami, wiec NIE uzywamy obrysow
    # z rzedu 1 jako okien: kazda ikona dostaje okno +-HALF wokol swojego
    # srodka.  Sasiednie ikony sa ~94 px od siebie, wiec +-36 px nie wciaga
    # sasiada i nie obcina nawet najszerszej ikony.
    centers = [ (r[0]+r[1])/2.0 for r in merged ]
    HALF = 36
    idx = 0
    touched = []
    for (ry0, ry1) in bands:
        for c in centers:
            rx0 = int(c - HALF); rx1 = int(c + HALF)
            wy0 = max(0, ry0-12); wy1 = min(H, ry1+13)
            box = find_icon_box(px, W, H, max(0, rx0), wy0, min(W, rx1), wy1)
            if box is None:
                print("PUSTA KOMORKA", NAMES[idx]); idx += 1; continue
            bx0, byy0, bx1, byy1 = box
            if bx0 <= max(0, rx0) or bx1 >= min(W, rx1) or byy0 <= wy0 or byy1 >= wy1:
                touched.append(NAMES[idx])
            fit(to_rgba(im.crop(box))).save(os.path.join(OUTDIR, NAMES[idx] + ".png"))
            idx += 1
    print("wycieto", idx, "ikon")
    if touched:
        print("UWAGA - obrys dotyka okna (%d):" % len(touched), " ".join(touched))
    else:
        print("kontrola: zaden obrys nie dotyka krawedzi okna")

if __name__ == "__main__":
    main()
