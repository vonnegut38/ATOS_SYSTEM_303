#!/usr/bin/env python3
"""Read ATOS TOS-personality state out of a running Hatari session.

Every symbol is resolved from the CURRENT build/system/kernel.elf, because the
kernel is rebuilt often and a stale address silently reports another variable -
that produced several false conclusions ("garbage mode", "palette never set")
during the TOS window work.

Usage:
    probe_tos_state.py --boot 22 --keys F9 --after 5 --symbols tos_mode,tos_palette
    probe_tos_state.py --histogram tos_xbios_opcount
"""
import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from atos2_auto import Hatari  # noqa: E402

PROJECT = HERE.parent


def sym(name, elf=None):
    elf = elf or (PROJECT / "build" / "system" / "kernel.elf")
    out = subprocess.run(["m68k-elf-nm", str(elf)], capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    raise SystemExit(f"symbol not found in {elf}: {name}")


def read(h, addr, count, settle=0.8, dump=False):
    """Read emulated RAM, anchored on the requested address (see probe_vofa.py)."""
    try:
        h._capture.flush()
        start = h.capture_path.stat().st_size
    except OSError:
        start = 0
    h.cmd(f"hatari-debug memdump {hex(addr)} {count}")
    time.sleep(settle)
    with open(h.capture_path, "r", errors="replace") as fh:
        fh.seek(start)
        text = fh.read()
    lines = []
    for line in text.splitlines():
        m = re.match(r"^([0-9A-Fa-f]{6,8}):\s+((?:[0-9A-Fa-f]{2}\s+)+)", line)
        if m:
            lines.append((int(m.group(1), 16), [int(t, 16) for t in m.group(2).split()]))
    if not lines:
        return None
    if dump:
        for at, toks in sorted(lines):
            print(f"    {at:08X}: " + " ".join(f"{t:02X}" for t in toks))
    # The dump's first line is the ONLY line at or below the request; anything
    # else means the run arrived shifted and must not be decoded.
    base, data = min(lines, key=lambda item: item[0])
    if base != addr or len(data) < count:
        return None
    return bytes(data[:count])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--symbols", default="tos_mode,tos_window_enabled,tos_active,tos_desktop_active")
    ap.add_argument("--histogram", default="")
    ap.add_argument("--boot", type=float, default=22.0, help="seconds before the keys")
    ap.add_argument("--keys", default="", help="comma list, e.g. F9")
    ap.add_argument("--after", type=float, default=5.0, help="seconds after the keys")
    ap.add_argument("--elf", type=Path, default=None)
    ap.add_argument("--dump", action="store_true")
    a = ap.parse_args()

    names = [n.strip() for n in a.symbols.split(",") if n.strip()]
    addrs = {n: sym(n, a.elf) for n in names}
    # Byte-sized kernel fields must be read as one byte; a 4-byte read returns
    # the three neighbours and produced garbage (tos_mode=0x41 etc.).
    BYTE_SYMBOLS = {"tos_mode", "tos_window_enabled", "tos_window_active",
                    "tos_desktop_active", "tos_active"}
    hist = sym(a.histogram, a.elf) if a.histogram else None
    print("addresses (from the current kernel.elf):")
    for n, v in addrs.items():
        print(f"  {n:22s} {v:#08x}")
    if hist:
        print(f"  {'histogram':22s} {hist:#08x} (256 x 4 B)")

    image = PROJECT / "work" / "atos-v2.img"
    noacsi = []
    for i in range(8):
        noacsi += ["--acsi", f"{i}=none"]
    extra = ["--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
             "--memsize", "4", "--ttram", "64", "--tf536", "on",
             "--graphics-card", "vofa", "--gfx-chip", "et4000", "--monitor", "mono",
             "--statusbar", "off", "--ide-master", str(image)] + noacsi
    h = Hatari(PROJECT, log=PROJECT / "work" / "hatari-tos-state.log",
               binary=str(PROJECT.parent / "atari-030-port" / "hatari-tf536"),
               cfg=False, extra=extra, image=image, render=True)
    outdir = PROJECT / "work" / "tos-state"
    outdir.mkdir(parents=True, exist_ok=True)
    try:
        time.sleep(a.boot)
        for k in [x.strip() for x in a.keys.split(",") if x.strip()]:
            h.press(k, delay=0.3)
        time.sleep(a.after)
        shot = outdir / "shot.png"
        shot.unlink(missing_ok=True)
        h.cmd(f'hatari-debug screenshot "{shot.resolve()}"')
        time.sleep(0.5)
        print(f"\nshot: {shot} exists={shot.exists()}")
        for n in names:
            d = read(h, addrs[n], 4, dump=a.dump)
            if d is None:
                print(f"  {n:22s} (read failed)")
                continue
            v = int.from_bytes(d, "big")
            print(f"  {n:22s} = {v} ({v:#x})")
        if hist:
            d = read(h, hist, 256 * 4, settle=1.2, dump=a.dump)
            if d:
                hits = {i: int.from_bytes(d[i*4:i*4+4], "big") for i in range(256)
                        if int.from_bytes(d[i*4:i*4+4], "big")}
                print(f"  {a.histogram}: {hits}")
    finally:
        print("rc", h.close())


if __name__ == "__main__":
    main()
