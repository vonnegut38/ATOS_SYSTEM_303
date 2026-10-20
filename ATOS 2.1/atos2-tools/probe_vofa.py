#!/usr/bin/env python3
"""Boot ATOS on a TF536 + VOFA/ET4000 machine and report what the ROM decided.

Single, trustworthy measurement tool for the graphics-card path.  It exists
because the earlier per-question probes were wrong in two ways that produced
misleading results:

  * they ran Hatari with --disable-video on and then took screenshots, so every
    capture was black no matter what the BIOS drew (Hatari skips the whole
    conversion/present path in that mode).  --render (default ON) keeps the
    render path active through SDL's dummy driver, so the run stays headless but
    the screenshots contain the real converted frame;
  * they decoded system_info_t with stale byte offsets and read byte fields as
    32-bit words.

Field offsets are pinned against the C definition by --check-layout, and the RAW
dump bytes are printed next to the decoded values so the evidence can always be
checked by eye.

Usage:
    probe_vofa.py                          # 20 s capture, screenshots + state
    probe_vofa.py --window                 # real macOS window
    probe_vofa.py --check-layout           # verify declared offsets
    probe_vofa.py --no-render              # negative control: black captures
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
from set_bios_cfg import Image as AhdiImage  # noqa: E402

MACHINE = {0: "ST", 1: "MEGA ST", 2: "STE", 3: "MEGA STE", 4: "TT", 5: "FALCON"}
ADAPTER = {0: "NONE", 1: "NOVA VME", 2: "CRAZYDOTS VME", 3: "VOFA/VOLKSFARBEN",
           4: "NOVA VME (TT)", 5: "NOVA VME (FALCON)"}
CHIPSET = {0: "NONE", 1: "ET4000/W32 FAMILY", 2: "ATI MACH32", 3: "ATI MACH64"}
MEMKIND = {0: "NONE", 1: "VME32 0xFE800000", 2: "VME 0xFEC00000",
           3: "FALCON 0xFFA00000", 4: "STGA 0x00C00000"}
CFG_MODE = {2: "640x400 mono", 3: "800x600x256", 4: "800x600 mono", 5: "1024x768 mono"}

# system_info_t: every field offset used below, in declaration order.
LAYOUT_EXPECTED = {
    "machine_type": 0x00, "cpu_type": 0x01, "fpu_type": 0x02, "mega_ste": 0x03,
    "clock_mhz": 0x04, "clock_known": 0x05, "cache_enabled": 0x06,
    "keyboard_present": 0x07, "video_mode": 0x08, "blitter_present": 0x09,
    "blitter_enabled": 0x0A, "rtc_type": 0x0B, "netusbee_ethernet": 0x0C,
    "netusbee_usb": 0x0D, "nova_present": 0x0E, "nova_adapter": 0x0F,
    "nova_chipset": 0x10, "nova_mem_kind": 0x11, "nova_io_base": 0x14,
    "nova_mem_base": 0x18, "nova_initialized": 0x1C, "video_preference": 0x1D,
    "video_active": 0x1E, "video_force_shifter": 0x1F, "acsi_devices": 0x20,
    "ide_units": 0x21, "acsi_scanned": 0x22, "acsi_root_read": 0x23,
    "acsi_root_id": 0x24, "acsi_root_bootable": 0x25, "acsi_partitions": 0x26,
    "ram_bytes": 0x4C, "tt_ram_bytes": 0x50,
}
SYS_SIZE = 96


def check_layout():
    """Compile offsetof(system_info_t, ...) on the host and compare."""
    parts = []
    for n in LAYOUT_EXPECTED:
        parts.append('printf("%%s %%zu\\n", "%s", offsetof(system_info_t, %s));' % (n, n))
    fields = "".join(parts)
    src = ('#include <stdio.h>\n#include <stddef.h>\n#include "system.h"\n'
           'int main(void){%sreturn 0;}\n' % fields)
    tmp = Path("/tmp/atos_layout_check.c")
    tmp.write_text(src)
    cc = subprocess.run(["cc", "-I", str(HERE.parent / "include"),
                         "-I", str(HERE.parent), str(tmp), "-o", "/tmp/atos_layout_check"],
                        capture_output=True, text=True)
    if cc.returncode != 0:
        print(cc.stderr, file=sys.stderr)
        return 1
    got = {}
    for line in subprocess.run(["/tmp/atos_layout_check"], capture_output=True,
                               text=True).stdout.splitlines():
        name, off = line.split()
        got[name] = int(off)
    bad = 0
    for name, want in LAYOUT_EXPECTED.items():
        have = got.get(name)
        if have != want:
            print("LAYOUT MISMATCH %s: declared %#x, actual %#x" % (name, want, have))
            bad += 1
    print("layout OK" if not bad else "%d field offset(s) stale" % bad)
    return 1 if bad else 0


def sym(name, root):
    for line in (root / "build" / "bios_mste.map").read_text().splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1] == name:
            return int(parts[0], 16)
    raise SystemExit("%s not in build/bios_mste.map" % name)


def set_latch(image, value):
    """Set or clear the ROM failover latch (video_pending) in \\BIOS\\BIOS.CFG.

    A test that is stopped before the kernel reaches the shell leaves the latch
    set, and the next boot then forces the Shifter - which looks exactly like a
    broken graphics card.  Preparing the image before the run makes the result
    reproducible.
    """
    img = AhdiImage(str(image), writable=True)
    try:
        changed = img.set_pending(value)
        return changed
    finally:
        img.f.close()


def raw_dump(h, addr, count):
    """Return the raw debugger hex output for one memdump."""
    try:
        h._capture.flush()
        start = h.capture_path.stat().st_size
    except OSError:
        start = 0
    h.cmd("hatari-debug memdump %s %d" % (hex(addr), count))
    time.sleep(0.8)
    with open(h.capture_path, "r", errors="replace") as fh:
        fh.seek(start)
        return fh.read()


def dump_lines(text):
    out = []
    for line in text.splitlines():
        m = re.match(r"^([0-9A-Fa-f]{6,8}):\s+((?:[0-9A-Fa-f]{2}\s+)+)", line)
        if m:
            out.append((int(m.group(1), 16), m.group(2).split()))
    return out


def decode(text, addr, count):
    """Bytes at addr..addr+count, or None when the dump does not cover it.

    The first line of a dump is the only line at or below the requested address,
    so the run is anchored on the smallest address present: a dump that arrived
    shifted is rejected rather than decoded against the wrong offsets.
    """
    lines = dump_lines(text)
    if not lines:
        return None
    base, toks = min(lines, key=lambda item: item[0])
    data = [int(t, 16) for t in toks]
    if base > addr or base + len(data) < addr + count:
        return None
    off = addr - base
    return bytes(data[off:off + count])


def show(text, label):
    print("  raw %s:" % label)
    for a, toks in sorted(dump_lines(text)):
        print("    %08X: %s" % (a, " ".join(toks)))


def b(blk, off):
    return blk[off] if blk is not None and len(blk) > off else None


def w(blk, off):
    if blk is None or len(blk) < off + 4:
        return None
    return int.from_bytes(blk[off:off + 4], "big")


def h32(blk, off):
    v = w(blk, off)
    return "n/a" if v is None else hex(v)


def run(p, outdir, seconds, render, window, image, mem, ttram, card, boot, shots,
        reset_latch=True):
    sysaddr = sym("g_system", p)
    cfgaddr = sym("g_config", p)
    staddr = sym("vofa_rom_status", p)
    outdir.mkdir(parents=True, exist_ok=True)
    noacsi = []
    for i in range(8):
        noacsi += ["--acsi", "%d=none" % i]
    extra = [
        "--machine", "megast", "--cpulevel", "3", "--cpuclock", "32",
        "--memsize", str(mem), "--ttram", str(ttram), "--tf536", "on",
        "--graphics-card", card, "--monitor", "mono", "--statusbar", "off",
    ] + noacsi
    if card != "none":
        extra += ["--gfx-chip", "et4000"]
    extra += (["--ide-master", str(image)] if boot == "ide"
              else ["--acsi", "0=" + str(image)])

    if reset_latch:
        print("latch: video_pending -> 0 (%s)" % (set_latch(image, 0) or "already clear"))
    h = Hatari(p, log=p / "work" / "hatari-probe-vofa.log",
               binary=str(p.parent / "atari-030-port" / "hatari-tf536"),
               cfg=False, extra=extra, image=image,
               render=render or window, window=window)
    try:
        t0 = time.monotonic()
        taken = []
        mode = "window" if window else ("on (SDL dummy)" if render else "OFF")
        print("render=%s  image=%s" % (mode, image))
        while time.monotonic() - t0 < seconds:
            t = time.monotonic() - t0
            for s in sorted(shots):
                if s not in taken and t >= s:
                    taken.append(s)
                    shot = outdir / ("t%02d.png" % int(s))
                    shot.unlink(missing_ok=True)
                    h.cmd('hatari-debug screenshot "%s"' % shot.resolve())
                    time.sleep(0.7)
                    print("  shot t=%4.0fs -> %s exists=%s"
                          % (s, shot.name, shot.exists()), flush=True)
            time.sleep(0.2)

        st_raw = raw_dump(h, staddr, 4)
        sys_raw = raw_dump(h, sysaddr, SYS_SIZE)
        cfg_raw = raw_dump(h, cfgaddr, 32)
        stb = decode(st_raw, staddr, 4)
        sysb = decode(sys_raw, sysaddr, SYS_SIZE)
        cfgb = decode(cfg_raw, cfgaddr, 32)

        print("\nvofa_rom_status  = %s   (0x101 gate, 0x102 gate passed, "
              "0x300|result, 0x400 on the card)" % h32(stb, 0))
        if sysb is None:
            print("system_info_t    : dump did not cover the range")
        else:
            print("machine_type     = %s (%s)  cpu=%s  clock_mhz=%s  clock_known=%s"
                  % (b(sysb, 0x00), MACHINE.get(b(sysb, 0x00), "?"), b(sysb, 0x01),
                     b(sysb, 0x04), b(sysb, 0x05)))
            print("ram_bytes        = %s  tt_ram_bytes = %s"
                  % (w(sysb, 0x4C), w(sysb, 0x50)))
            print("nova_present     = %s  adapter=%s (%s)  chipset=%s (%s)"
                  % (b(sysb, 0x0E), b(sysb, 0x0F), ADAPTER.get(b(sysb, 0x0F), "?"),
                     b(sysb, 0x10), CHIPSET.get(b(sysb, 0x10), "?")))
            print("nova_mem_kind    = %s (%s)  io_base=%s  mem_base=%s"
                  % (b(sysb, 0x11), MEMKIND.get(b(sysb, 0x11), "?"),
                     h32(sysb, 0x14), h32(sysb, 0x18)))
            print("nova_initialized = %s  video_preference=%s  video_active=%s  "
                  "video_force_shifter=%s"
                  % (b(sysb, 0x1C), b(sysb, 0x1D), b(sysb, 0x1E), b(sysb, 0x1F)))
        if cfgb is None:
            print("bios_config_t    : dump did not cover the range")
        else:
            print("cfg boot_device  = %s  video_driver=%s  video_mode=%s (%s)  "
                  "video_pending=%s  boot_valid=%s"
                  % (b(cfgb, 12), b(cfgb, 13), b(cfgb, 14),
                     CFG_MODE.get(b(cfgb, 14), "?"), b(cfgb, 27), b(cfgb, 25)))
        show(sys_raw, "system_info_t")
        show(cfg_raw, "bios_config_t")
    finally:
        print("rc", h.close())
    if reset_latch:
        # Leave the disk usable for the next normal boot as well.
        print("latch: video_pending -> 0 (%s)" % (set_latch(image, 0) or "already clear"))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", type=Path, default=HERE.parent)
    ap.add_argument("--check-layout", action="store_true")
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--outdir", type=Path, default=None)
    ap.add_argument("--image", type=Path, default=None)
    ap.add_argument("--mem", default="4")
    ap.add_argument("--ttram", default="64")
    ap.add_argument("--card", default="vofa")
    ap.add_argument("--boot", default="ide", choices=["ide", "acsi"])
    ap.add_argument("--window", action="store_true")
    ap.add_argument("--no-render", dest="render", action="store_false", default=True)
    ap.add_argument("--shots", default="4,8,12,18")
    ap.add_argument("--keep-latch", dest="reset_latch", action="store_false",
                    default=True,
                    help="do not clear the ROM failover latch before/after")
    a = ap.parse_args()
    if a.check_layout:
        return check_layout()
    p = a.project
    outdir = a.outdir or (p / "work" / "vofa-probe")
    image = a.image or (p / "work" / "atos-v2.img")
    shots = [float(x) for x in a.shots.split(",") if x.strip()]
    return run(p, outdir, a.seconds, a.render, a.window, image, a.mem, a.ttram,
               a.card, a.boot, shots, a.reset_latch)


if __name__ == "__main__":
    sys.exit(main())
