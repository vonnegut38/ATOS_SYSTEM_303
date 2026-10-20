#!/usr/bin/env python3
"""POMIAR WYDAJNOSCI sciezki blitowania (decyzja o M9 / akceleracji W32i).

Mierzy liczniki jadra, ktore juz istnieja (kernel_video.c):
  kdbg_flush_calls  - ile razy kvideo_flush() cokolwiek wypchnal
  kdbg_flush_words  - ile slow 16-bit zapisano do karty (realna praca)
  kdbg_dmg_px       - ile pikseli oznaczono jako uszkodzone
Odczyt przez hatari-debug memdump (symbole z kernel.elf), czas mierzony
zegarem hosta - emulator dziala w czasie rzeczywistym (fast-forward off).

Scenariusze:
  idle     - sam zegar pulpitu (tlo)
  drag     - przeciaganie okna PLIKI po ekranie (ciagly ruch)
  icons    - zmiana rozmiaru ikon w kolko (F9 = opcode 48, pelny relayout)
  drag16   - to samo co drag, ale w trybie 1024x768x16bpp

Wynik: slowa/s, wywolania/s, slowa na wywolanie, px/s oraz szacowany czas
pelnej klatki 800x600x8 (240 000 slow) i 1024x768x16 (786 432 slowe).

Uzycie: python3 perf_probe.py [minuty]
"""
import os
import sys
import time

DEMO = "/Users/xxx/Documents/Deepseek_test/display-demo"
REAL = "/Users/xxx/Documents/Deepseek_test/ATOS 2.1"
sys.path.insert(0, DEMO)
import stress_atos as base                                       # noqa: E402

base.IMG = DEMO + "/demo-perf.img"
IMG = base.IMG
OUT = DEMO + "/PERF-RAPORT-%s.txt" % time.strftime("%Y%m%d-%H%M")

FULL_FRAME_800x600x8 = 800 * 600 // 2          # slowa 16-bit
FULL_FRAME_1024x768x16 = 1024 * 768            # 2 slowa na piksel


def read_counters(s):
    """(blit_calls, blit_px, flush_words, flush_calls) albo None.

    kdbg_blit_calls/kdbg_blit_px (kernel_wm.c) to KOMPOZYCJA okien - realna
    praca, ktora akceleracja W32i mialaby przyspieszyc.  kdbg_flush_words to
    osobno: ile slow trafia do pamieci karty (kvideo_flush).
    """
    bc = s.rd32("kdbg_blit_calls")
    bp = s.rd32("kdbg_blit_px")
    fw = s.rd32("kdbg_flush_words")
    fc = s.rd32("kdbg_flush_calls")
    if bc is None or bp is None:
        return None
    return (bc, bp, fw or 0, fc or 0)


def measure(s, name, workload, seconds=6.0):
    """Uruchamia workload(deadline) i zwraca (dt, d_calls, d_words, d_px)."""
    start = read_counters(s)
    if start is None:
        return None
    t0 = time.time()
    deadline = t0 + seconds
    steps = workload(deadline) or 0
    dt = time.time() - t0
    end = read_counters(s)
    if end is None:
        return None
    return (dt, end[0] - start[0], end[1] - start[1], end[2] - start[2], end[3] - start[3], steps)


def run(minutes=8.0):
    t_start = time.time()
    base.prepare()
    s = base.Session()
    rows = []

    def report(name, m, note=""):
        if m is None:
            rows.append((name, "-", "-", "-", "-", "-", "BRAK ODCZYTU", note))
            print("  %-8s BRAK ODCZYTU (memdump nie odpowiedzial)" % name, flush=True)
            return
        dt, dc, dpx, dw, dfc, steps = m
        cps = dc / dt
        pps = dpx / dt
        ppc = (dpx / dc) if dc else 0.0
        frame800 = (480000.0 / pps * 1000.0) if pps else 0.0
        frame1024 = (786432.0 / pps * 1000.0) if pps else 0.0
        sps = steps / dt if dt else 0.0
        rows.append((name, "%.2f" % dt, "%d" % dc, "%d" % dpx, "%.0f" % pps,
                     "%.0f" % ppc, "%.0f" % sps, "%.2f ms" % frame800, note))
        print("  %-8s dt=%.1fs krokow=%d (%.0f/s)  blitow=%d px=%d  %.0f px/s  "
              "%.0f px/blit  pelny ekran 800x600: %.2f ms %s"
              % (name, dt, steps, sps, dc, dpx, pps, ppc, frame800, note), flush=True)

    try:
        print("czekam 72 s na boot...", flush=True)
        time.sleep(72)
        # 1) kontrola: czy memdump w ogole dziala
        probe = read_counters(s)
        if probe is None:
            print("!! memdump nie zwraca licznikow - pomiar niemozliwy", flush=True)
            rows.append(("kontrola", "-", "-", "-", "-", "-", "BRAK ODCZYTU",
                         "hatari-debug memdump nie odpowiedzial"))
            return rows
        time.sleep(2.0)
        probe2 = read_counters(s)
        print("kontrola memdump: %s -> %s" % (probe, probe2), flush=True)
        if probe2 is None or probe2[0] < probe[0]:
            print("!! liczniki nie rosna - memdump czyta zle miejsce", flush=True)
            return rows

        # 2) idle: tylko zegar pulpitu
        report("idle", measure(s, "idle", lambda dl: time.sleep(max(0.0, dl - time.time())), 5.0),
               "tylko zegar")

        # 3) drag: okno PLIKI i ciagle przeciaganie
        s.key(0x41, 1.0)                       # F7
        time.sleep(2.0)
        s.shot("p0-pliki")

        def drag_workload(deadline, x_from=430, y_from=300, spread=260):
            btn = 1
            s.mouse(x_from, y_from, btn)       # zlap okno za srodek
            time.sleep(0.2)
            i = 0
            while time.time() < deadline:
                # ruch po okregu: ciagla zmiana pozycji = ciagle przetwarzanie
                import math
                ang = i * 0.5
                x = int(x_from + spread * math.sin(ang) * 0.5)
                y = int(y_from + 120 * math.cos(ang) * 0.5)
                s.mouse(max(60, min(740, x)), max(40, min(520, y)), btn)
                i += 1
            s.mouse(x_from, y_from, 0)         # pusc
            time.sleep(0.3)
            return i

        report("drag48", measure(s, "drag48", drag_workload, 8.0), "ikony 48 px, 800x600x8")
        s.shot("p1-po-drag")

        # 4) icons: zmiana rozmiaru ikon w kolko (F9 = opcode 48)
        def icons_workload(deadline):
            n = 0
            while time.time() < deadline:
                s.key(0x42, 1.2)               # F9: nastepny rozmiar ikon
                n += 1
            return n
        report("icons", measure(s, "icons", icons_workload, 7.0), "F9: 48->64->32")
        s.shot("p2-po-icons")

        # 5) drag w 1024x768x16 (jesli przelaczenie zadziala)
        s.click(85, 8); time.sleep(1.4)        # pasek: SETTINGS
        s.click(80, 31); time.sleep(3.0)
        for _ in range(3):
            s.key(0x50, 0.5)                   # w dol listy trybow
        s.key(0x1c, 0.8); time.sleep(1.0)
        s.key(0x1c, 0.8)                       # YES
        time.sleep(3.0)
        st = s.shot("p3-tryb")
        from PIL import Image
        size = Image.open(st[0]).size
        print("  tryb po przelaczeniu: %dx%d" % size, flush=True)
        if size != (800, 600):
            report("drag16", measure(s, "drag16", drag_workload, 8.0),
                   "ikony 48 px, %dx%d" % size)
            s.shot("p4-drag16")
        else:
            rows.append(("drag16", "-", "-", "-", "-", "-", "POMINIETY",
                         "przelaczenie trybu nie zadzialalo"))
        return rows
    finally:
        s.close()
        rows.append(("czas", "%.0f s" % (time.time() - t_start), "", "", "", "", "", ""))


def write_report(rows, path):
    lines = ["POMIAR WYDAJNOSCI BLOATOWANIA - %s" % time.strftime("%Y-%m-%d %H:%M"),
             "obraz: %s | liczniki jadra: kdbg_flush_calls/words/dmg_px (memdump)" % IMG,
             "",
             "%-8s %-7s %-8s %-8s %-10s %-9s %-9s %-11s %s" % (
                 "scenariusz", "dt", "krokow/s", "blitow", "pikseli", "px/s",
                 "px/blit", "pelny ekran", "uwagi")]
    for name, dt, dc, dw, wps, wpc, steps, frame, note in [r for r in rows if len(r) == 9]:
        lines.append("%-8s %-7s %-8s %-8s %-10s %-9s %-9s %-11s %s" % (
            name, dt, steps, dc, dw, wps, wpc, frame, note))
    lines += ["",
              "'pikseli' = kdbg_blit_px (kompozycja okien w kernel_wm.c),",
              "'blitow' = kdbg_blit_calls.  px/s to przepustowosc kompozycji;",
              "'pelny ekran' to czas, w jakim ta przepustowosc odmalowalaby",
              "caly ekran 800x600 (480000 px).  To wlasnie ta czesc moglaby",
              "przyspieszyc akceleracja W32i."]
    open(path, "w").write("\n".join(lines) + "\n")
    print("\n" + "\n".join(lines), flush=True)


if __name__ == "__main__":
    rows = run(float(sys.argv[1]) if len(sys.argv) > 1 else 8.0)
    write_report(rows, OUT)
    print("raport:", OUT)
