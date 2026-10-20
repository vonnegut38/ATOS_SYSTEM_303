#!/usr/bin/env python3
"""Launch ATOS 2.0 in Hatari and capture screenshots via the debugger cmd-fifo.

Headless by default (--disable-video on). Screenshots use Hatari's own
"screenshot <file>" debugger command, so no host screen-recording permission
is needed.
"""
import argparse
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--project", type=Path, required=True)
    ap.add_argument("--shots", default="", help="comma list sec:path; default waits --wait then --out")
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--wait", type=float, default=20.0)
    ap.add_argument("--vbls", type=int, default=0, help="0 = run until quit command")
    ap.add_argument("--window", action="store_true")
    ap.add_argument("--fast", action="store_true")
    ap.add_argument("--env", default="", help="extra KEY=VALUE;... env vars")
    a = ap.parse_args()

    p = a.project
    fifo = Path(tempfile.mkstemp(prefix="atos2-fifo-")[1])
    fifo.unlink()

    cmd = [
        "hatari", "-c", str(p / "atos2.cfg"),
        "--cmd-fifo", str(fifo),
        "--tos", str(p / "build/bios_mste.bin"),
        "--patch-tos", "false",
        "--acsi", "0=" + str(p / "work/atos-v2.img"),
        "--machine", "ste", "--monitor", "mono", "--cpulevel", "3",
        "--cpuclock", "16", "--memsize", "4", "--ttram", "0",
        "--sound", "off", "--confirm-quit", "false",
        "--log-file", str(p / "work/hatari-probe.log"),
    ]
    cmd += ["--window"] if a.window else ["--disable-video", "on"]
    if a.fast:
        cmd += ["--fast-forward", "on"]
    if a.vbls:
        cmd += ["--run-vbls", str(a.vbls)]

    print("$ " + " ".join(cmd), file=sys.stderr)
    import os
    env = dict(os.environ)
    if a.env:
        for kv in a.env.split(";"):
            if "=" in kv:
                k, v = kv.split("=", 1)
                env[k] = v
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, env=env)

    shots = []
    if a.shots:
        for spec in a.shots.split(","):
            sec, path = spec.split(":", 1)
            shots.append((float(sec), Path(path)))
    elif a.out is not None:
        shots.append((a.wait, a.out))

    t0 = time.monotonic()
    ok = True
    try:
        while not fifo.exists() and proc.poll() is None and time.monotonic() - t0 < 15:
            time.sleep(0.05)
        for sec, path in shots:
            while True:
                elapsed = time.monotonic() - t0
                if elapsed >= sec or proc.poll() is not None:
                    break
                time.sleep(0.05)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.unlink(missing_ok=True)
            try:
                with open(fifo, "w") as fh:
                    fh.write(f"screenshot {path.resolve()}\n")
            except OSError as exc:
                print(f"fifo write failed: {exc}", file=sys.stderr)
            time.sleep(0.8)
            exists = path.exists()
            ok = ok and exists
            print(f"shot at {sec:.1f}s -> {path} exists={exists}", file=sys.stderr)
    finally:
        if proc.poll() is None:
            try:
                if fifo.exists():
                    with open(fifo, "w") as fh:
                        fh.write("quit\n")
                proc.wait(timeout=6)
            except Exception:
                proc.kill()
                proc.wait()
        fifo.unlink(missing_ok=True)
    print(f"hatari rc={proc.returncode}", file=sys.stderr)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
