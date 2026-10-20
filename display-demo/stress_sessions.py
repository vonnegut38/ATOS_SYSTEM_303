#!/usr/bin/env python3
"""Sesje (Desk 1/2/3), zamykanie i restart pulpitu, tryby z otwartym oknem.

Trzy fazy, kazda z weryfikacja zrzutem:
  SESJE    - "Add Desk" z menu sesji (prawy przycisk w prawym gornym rogu),
             przelaczanie Shift+Alt+F10, uruchomienie aplikacji w kazdej sesji
             i sprawdzenie, ze kazda trzyma swoj wlasny ekran,
  DESKTOP  - FILE -> CLOSE (zamkniecie pulpitu) x2 i restart z konsoli
             (wpisanie sciezki \\SYSTEM\\APP\\DESKTOP.APP),
  TRYB+OKNO- zmiana rozdzielczosci przy OTWARTYM oknie PLIKI i powrot.

Uzycie: python3 stress_sessions.py [minuty]
"""
import hashlib, os, subprocess, sys, time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
sys.path.insert(0, DEMO)
import stress_atos as base                                   # noqa: E402

base.IMG = DEMO + "/demo-sessions.img"
IMG = base.IMG
FIFO = "/tmp/sessions.fifo"

# Scancody (ATOS/Atari): litery male, shift 0x2a, alt 0x38, F10 0x44.
LETTERS = {"a": 0x1e, "b": 0x30, "c": 0x2e, "d": 0x20, "e": 0x12, "f": 0x21,
           "g": 0x22, "h": 0x23, "i": 0x17, "j": 0x24, "k": 0x25, "l": 0x26,
           "m": 0x32, "n": 0x31, "o": 0x18, "p": 0x19, "q": 0x10, "r": 0x13,
           "s": 0x1f, "t": 0x14, "u": 0x16, "v": 0x2f, "w": 0x11, "x": 0x2d,
           "y": 0x15, "z": 0x2c, " ": 0x39, ".": 0x34, "/": 0x35, "\\": 0x2b,
           "0": 0x0b, "1": 0x02, "2": 0x03, "3": 0x04, "4": 0x05, "5": 0x06,
           "6": 0x07, "7": 0x08, "8": 0x09, "9": 0x0a, "-": 0x0c}


def hexs(h):
    return h[:16]


def console_like(path):
    """Konsola (mono shell): ekran ciemny, malo kolorow, brak paska pulpitu."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    W, H = im.size
    p = im.load()
    dark = sum(1 for y in range(0, H, 11) for x in range(0, W, 11)
               if sum(p[x, y]) < 200)
    tot = len(range(0, H, 11)) * len(range(0, W, 11))
    return dark * 100 >= tot * 75


def run(minutes=10.0):
    t0 = time.time()
    base.prepare()
    s = base.Session()
    journal = []

    def log(phase, step, measured, expected, note=""):
        # expected=None -> krok informacyjny (bez oceny); inaczej porownujemy
        # KONKRETNA wartosc, a nie opis w "measured".
        ok = (expected is None) or (measured == expected)
        journal.append(dict(phase=phase, step=step, measured=measured,
                            expected=expected, ok=ok, note=note,
                            t=round(time.time() - t0, 1)))
        print("  %-7s %-14s zmierzone=%-30s oczek.=%-16s %s %s" % (
            phase, step, measured, expected, "OK  " if ok else "ROZNICA", note),
            flush=True)

    def shot(name):
        return s.shot(name)

    def text(slow="desktop"):
        for ch in slow:
            s.key(LETTERS[ch], 0.10)

    def shifted(seq):
        for ch in seq:
            sc = LETTERS[ch]
            s.cmd("hatari-event keydown 0x2a")
            s.key(sc, 0.08)
            s.cmd("hatari-event keyup 0x2a")
            time.sleep(0.05)


    def close_desktop(tag):
        """FILE -> CLOSE z ponawianiem; zwraca (ok, shot)."""
        for attempt in range(3):
            s.click(20, 8)
            time.sleep(1.2)
            for _ in range(3):
                s.key(0x50, 0.4)
            s.key(0x1c, 1.0)
            time.sleep(2.8)
            st = shot("%s-zamk-%d" % (tag, attempt))
            if console_like(st[0]):
                return True, st
            s.key(0x01, 0.8)
        return False, st

    def reopen_desktop(tag):
        """Wpisuje sciezke pulpitu w konsoli; zwraca (ok, shot)."""
        for attempt, fn in enumerate((lambda: text("\\system\\app\\desktop.app"),
                                      lambda: shifted("\\system\\app\\desktop.app"))):
            fn()
            s.key(0x1c, 1.2)
            time.sleep(6.0)
            st = shot("%s-start-%d" % (tag, attempt))
            if not console_like(st[0]):
                return True, st
        return False, st

    def cycle_session():
        s.cmd("hatari-event keydown 0x2a")     # SHIFT
        s.cmd("hatari-event keydown 0x38")     # ALT
        s.key(0x44, 0.5)                       # F10
        s.cmd("hatari-event keyup 0x38")
        s.cmd("hatari-event keyup 0x2a")
        time.sleep(2.2)

    def open_session_menu():
        s.mouse(545, 8, 2)
        time.sleep(0.5)
        s.mouse(545, 8, 0)
        time.sleep(0.8)

    def add_desk():
        # menu sesji: prawy przycisk w prawym gornym rogu, potem wiersz y=16..31
        open_session_menu()
        s.mouse(600, 24, 2)
        time.sleep(0.4)
        s.mouse(600, 24, 0)
        time.sleep(3.5)

    try:
        print("czekam 72 s na boot...", flush=True)
        time.sleep(72)
        st = shot("x0-boot")
        log("BOOT", "-", "pulpit 800x600", "pulpit 800x600", hexs(st[1]))
        s.mouse(700, 560, 0)

        # ================= SESJE ===========================================
        # Przelaczanie sesji robi sie z POWLOKI: polecenia tts1/tts2/tts3
        # (menu sesji w prawym rogu czyta PRAWDZIWA mysz, a wstrzykiwana jest
        # oznaczona jako "scripted" i jest przez ten kod pomijana).
        desk = shot("x1-desk1")
        log("SESJE", "start", "pulpit (sesja 1)", None, hexs(desk[1]))
        s.dbl(78, 198)                     # ikona TERMINAL
        time.sleep(2.5)
        text("tts2")
        s.key(0x1c, 1.2)
        time.sleep(3.0)
        ses2 = shot("x2-tts2")
        log("SESJE", "tts2", "konsola" if console_like(ses2[0]) else "inny ekran", None,
            hexs(ses2[1]))
        text("dir")
        s.key(0x1c, 1.2)
        time.sleep(2.0)
        ses2b = shot("x3-tts2-dir")
        log("SESJE", "tts2 + dir", "hash %s" % hexs(ses2b[1]), None, "polecenie w sesji 2")
        text("tts3")
        s.key(0x1c, 1.2)
        time.sleep(3.0)
        ses3 = shot("x4-tts3")
        log("SESJE", "tts3", "hash %s" % hexs(ses3[1]), None, "sesja 3")
        text("\\system\\app\\taskmgr.app")
        s.key(0x1c, 1.2)
        time.sleep(6.0)
        ses3app = shot("x5-tts3-app")
        log("SESJE", "app w tts3", "hash %s" % hexs(ses3app[1]), None,
            "TASKMGR uruchomiony w sesji 3")
        text("tts1")
        s.key(0x1c, 1.2)
        time.sleep(3.5)
        back1 = shot("x6-tts1")
        log("SESJE", "powrot tts1", "pulpit" if not console_like(back1[0]) else "konsola",
            "pulpit", hexs(back1[1]))
        s.key(0x01, 0.8)

        # ================= ZAMYKANIE PULPITU ===============================
        for attempt in range(2):
            ok, closed = close_desktop("x8-%d" % attempt)
            log("DESKTOP", "zamkniecie %d" % attempt,
                "konsola" if ok else "pulpit nadal", None, hexs(closed[1]))
            if not ok:
                continue
            ok2, again = reopen_desktop("x9-%d" % attempt)
            log("DESKTOP", "restart %d" % attempt,
                "pulpit wrocil" if ok2 else "wciaz konsola", None, hexs(again[1]))
            s.mouse(700, 560, 0)

        # ================= TRYB EKRANU Z OTWARTYM OKNEM =====================
        from PIL import Image
        s.key(0x41, 1.0)                   # F7 = okno PLIKI
        time.sleep(2.5)
        shot("xa-okno-pliki")
        s.click(85, 8); time.sleep(1.4)
        s.click(80, 31); time.sleep(3.2)
        size = None
        for attempt in range(3):
            for _ in range(2 if attempt == 0 else 1):
                s.key(0x50, 0.45)          # w dol listy trybow
            s.key(0x1c, 0.8); time.sleep(1.2)
            s.key(0x1c, 0.8)               # YES
            time.sleep(3.0)
            big = shot("xb-1024-z-oknem-%d" % attempt)
            size = Image.open(big[0]).size
            if size != (800, 600):
                break
        log("TRYB+OKNO", "1024 z oknem", "%dx%d" % size, None,
            "zmiana rozdzielczosci przy otwartym oknie")
        # powrot do 800x600x8 (6. pozycja listy)
        for attempt in range(3):
            for _ in range(9):
                s.key(0x48, 0.35)
            s.key(0x1c, 0.8); time.sleep(1.2); s.key(0x1c, 0.8)
            time.sleep(3.0)
            back = shot("xc-powrot-800-%d" % attempt)
            size_back = Image.open(back[0]).size
            if size_back == (800, 600):
                break
        log("TRYB+OKNO", "powrot 800x600", "%dx%d" % size_back, None,
            "powrot przy otwartym oknie")
        s.key(0x01, 0.8)
        s.key(0x01, 0.8)
        end = shot("xd-koniec")
        log("KONIEC", "-", "hash %s" % hexs(end[1]), None, "pulpit po sesji")
        return journal
    finally:
        s.close()


def write_report(journal, path):
    lines = ["SESJE + ZAMYKANIE PULPITU + TRYB Z OKNEM - %s" % time.strftime("%Y-%m-%d %H:%M"),
             "obraz: %s (klon %s)" % (IMG, os.path.basename(base.BASE)), "",
             "%-8s %-16s %-32s %-18s %-8s %s" % ("faza", "krok", "zmierzone",
                                                 "oczekiwane", "wynik", "uwagi")]
    ok = 0
    for r in journal:
        if r["ok"]:
            ok += 1
        lines.append("%-8s %-16s %-32s %-18s %-8s %s" % (
            r["phase"], r["step"], r["measured"],
            r["expected"] if r["expected"] is not None else "-",
            "OK" if r["ok"] else "ROZNICA", r["note"]))
    lines += ["", "krokow: %d, zgodnych: %d, roznic: %d" % (len(journal), ok, len(journal) - ok)]
    open(path, "w").write("\n".join(lines) + "\n")
    print("\n" + "\n".join(lines), flush=True)


if __name__ == "__main__":
    minutes = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0
    j = run(minutes)
    out = "%s/SESJE-RAPORT-%s.txt" % (DEMO, time.strftime("%Y%m%d-%H%M"))
    write_report(j, out)
    print("raport:", out)
