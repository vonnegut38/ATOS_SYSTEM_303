#!/usr/bin/env python3
"""Run Hatari headless and capture a screenshot at a chosen moment.

Hatari's debugger exposes a `screenshot <file>` command, and `--cmd-fifo` feeds
the debugger commands at runtime. Combining the two gives a scriptable capture
pipeline that needs no window, no AVI recording and no frame extractor — which
is what makes an automated 68000-vs-68030 comparison practical.

The process is driven with `--run-vbls`, so it always terminates on its own.

Example:
    python3 tools/hatari_shot.py --machine megaste --cpulevel 3 \
        --tos build/bios_mste.bin --at 8 --out /tmp/mste-030.png
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def build_command(args: argparse.Namespace, fifo: Path) -> list[str]:
    command = [
        "hatari",
        "--machine",
        args.machine,
        "--memsize",
        args.memsize,
        "--cpuclock",
        args.cpuclock,
        "--tos",
        str(args.tos),
        "--patch-tos",
        "false",
        "--disable-video",
        "on",
        "--cmd-fifo",
        str(fifo),
        "--run-vbls",
        str(args.vbls),
        "--log-file",
        str(args.log),
        "--log-level",
        args.log_level,
    ]
    if args.cpulevel is not None:
        command += ["--cpulevel", str(args.cpulevel)]
    if args.data_cache is not None:
        command += ["--data-cache", args.data_cache]
    if args.acsi is not None:
        command += ["--acsi", str(args.acsi)]
    if args.trace is not None:
        command += ["--trace", args.trace, "--trace-file", str(args.trace_file)]
    if args.parse is not None:
        command += ["--parse", str(args.parse)]
    command += args.extra
    return command


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tos", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--machine", default="megaste")
    parser.add_argument("--cpulevel", default="3", help="680x0 level; 'none' omits the flag")
    parser.add_argument("--memsize", default="4")
    parser.add_argument("--cpuclock", default="16")
    parser.add_argument("--data-cache", dest="data_cache", default=None)
    parser.add_argument("--acsi", default=None, help="ACSI image as <id>=<file>, e.g. 0=/path/disk.img")
    parser.add_argument("--vbls", type=int, default=900, help="VBLs to emulate (50 Hz)")
    parser.add_argument("--at", type=float, default=6.0, help="seconds before the screenshot")
    parser.add_argument("--log", type=Path, default=Path("/tmp/hatari.log"))
    parser.add_argument("--log-level", dest="log_level", default="info")
    parser.add_argument("--trace", default=None)
    parser.add_argument("--trace-file", dest="trace_file", type=Path, default=Path("/tmp/hatari.trace"))
    parser.add_argument("--parse", type=Path, default=None)
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("extra", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    if args.cpulevel in ("none", "None", ""):
        args.cpulevel = None
    if args.out.exists():
        args.out.unlink()

    # Hatari creates the FIFO itself and refuses to start when it already
    # exists, so only a free name is reserved here.
    fd, fifo_name = tempfile.mkstemp(prefix="hatari-fifo-")
    os.close(fd)
    os.unlink(fifo_name)
    fifo = Path(fifo_name)

    command = build_command(args, fifo)
    print("$", " ".join(command), file=sys.stderr)
    process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)

    try:
        deadline = time.monotonic() + args.timeout
        time.sleep(args.at)
        if process.poll() is not None:
            print(f"hatari exited early (code {process.returncode})", file=sys.stderr)
        else:
            # Hatari opens the FIFO for reading during startup, so this open
            # blocks only until it is ready rather than failing outright.
            wait_until = time.monotonic() + 10.0
            while not fifo.exists() and process.poll() is None and time.monotonic() < wait_until:
                time.sleep(0.05)
            with open(fifo, "w") as handle:
                handle.write(f"screenshot {args.out.resolve()}\n")
        remaining = max(1.0, deadline - time.monotonic())
        process.wait(timeout=remaining)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        print("hatari timed out and was killed", file=sys.stderr)
    finally:
        fifo.unlink(missing_ok=True)

    if not args.out.exists():
        print("no screenshot produced", file=sys.stderr)
        return 1
    print(f"screenshot: {args.out} ({args.out.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
