#!/usr/bin/env python3
"""Extract frames from a Hatari AVI recording.

Hatari records with `--avi-vcodec png` by writing one complete PNG per video
frame, so extraction is a matter of walking the RIFF `movi` list and dumping the
`00dc`/`00db` chunks — no decoder, no ffmpeg dependency.

Examples:
    python3 tools/avi_frames.py run.avi --at 5 --out /tmp/frame.png
    python3 tools/avi_frames.py run.avi --every 50 --outdir /tmp/frames
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


def iter_frames(blob: bytes):
    """Yield (index, payload) for every video chunk in a RIFF AVI container."""
    if blob[:4] != b"RIFF" or blob[8:12] != b"AVI ":
        raise ValueError("not a RIFF/AVI file")
    index = 0
    offset = 12
    total = len(blob)
    while offset + 8 <= total:
        chunk_id = blob[offset : offset + 4]
        (size,) = struct.unpack_from("<I", blob, offset + 4)
        payload_start = offset + 8
        payload_end = payload_start + size
        if chunk_id == b"LIST":
            # Recurse into LIST containers (hdrl, movi, …) by walking their body.
            offset = payload_start + 4
            continue
        if chunk_id in (b"00dc", b"00db", b"01dc", b"01db"):
            yield index, blob[payload_start:payload_end]
            index += 1
        # Chunks are word-aligned.
        offset = payload_end + (size & 1)
    return


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PNG_END = b"IEND\xaeB`\x82"


def iter_png_frames(blob: bytes):
    """Yield (index, png_bytes) by slicing between PNG markers.

    Hatari's AVI writer leaves the `movi` LIST size and some frame chunk sizes
    at zero, so walking the RIFF chunk sizes yields garbage after the first
    frame. With `--avi-vcodec png` every frame is a complete PNG, which makes
    the markers a reliable boundary instead.
    """
    index = 0
    start = blob.find(PNG_SIGNATURE)
    while start >= 0:
        end = blob.find(PNG_END, start)
        if end < 0:
            break
        end += len(PNG_END)
        yield index, blob[start:end]
        index += 1
        start = blob.find(PNG_SIGNATURE, end)
    return


def read_frames(path: Path):
    """Return frames, preferring the RIFF walk but falling back to PNG markers."""
    blob = path.read_bytes()
    frames = list(iter_frames(blob))
    png_frames = list(iter_png_frames(blob))
    if len(png_frames) > len(frames):
        return png_frames, "png markers"
    return frames, "riff chunks"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("avi", type=Path)
    parser.add_argument("--at", type=float, help="seconds into the recording")
    parser.add_argument("--index", type=int, help="frame index")
    parser.add_argument("--fps", type=float, default=50.0, help="recording frame rate")
    parser.add_argument("--every", type=int, help="extract every Nth frame")
    parser.add_argument("--out", type=Path, help="output file for --at/--index")
    parser.add_argument("--outdir", type=Path, help="output directory for --every")
    args = parser.parse_args()

    frames, method = read_frames(args.avi)
    if not frames:
        print("no video frames found", file=sys.stderr)
        return 1
    print(
        f"{args.avi}: {len(frames)} frames ({len(frames) / args.fps:.1f}s at {args.fps:g} fps), "
        f"via {method}"
    )

    if args.every is not None:
        outdir = args.outdir or Path("/tmp/frames")
        outdir.mkdir(parents=True, exist_ok=True)
        written = 0
        for index, payload in frames:
            if index % args.every:
                continue
            target = outdir / f"frame-{index:05d}.png"
            target.write_bytes(payload)
            written += 1
        print(f"wrote {written} frames to {outdir}")
        return 0

    if args.index is not None:
        wanted = args.index
    elif args.at is not None:
        wanted = min(len(frames) - 1, max(0, round(args.at * args.fps)))
    else:
        wanted = len(frames) - 1

    payload = frames[wanted][1]
    if args.out is None:
        print(f"frame {wanted} is {len(payload)} bytes; pass --out to write it")
        return 0
    args.out.write_bytes(payload)
    print(f"wrote frame {wanted} ({wanted / args.fps:.2f}s) to {args.out} ({len(payload)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
