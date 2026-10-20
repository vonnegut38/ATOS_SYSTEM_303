#!/usr/bin/env python3
"""Manifest for an audio test run: which binary, from which source, started how.

The audit asked for this because an image SHA or a kernel size cannot tell
whether the tested KERNEL.SYS/BIOS is the one just built, nor which ELF it came
from.  This records, for every artefact that ends up on the image:

  * its path, size and SHA-256,
  * the ELF it was produced from (and that ELF's SHA-256),
  * the image it was installed into (and the image's SHA-256),
  * the exact emulator command line used for the run.

Usage:
  python3 tools/audio_manifest.py --image work/audio-baseline/atos-audio-test.img \
      --out work/audio-baseline/manifest-<tag>.md [--tag <tag>] [-- <hatari args...>]
"""
import argparse
import hashlib
import json
import os
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent

PAIRS = [
    # artefact on the image (FAT path), ELF it comes from, build copy
    ("SYSTEM/KERNEL.SYS", "build/system/kernel.elf", "build/system/KERNEL.SYS"),
    ("SYSTEM/APP/DESKTOP.APP", "build/system/desktop_wm.elf", "build/system/DESKTOP.APP"),
    ("SYSTEM/APP/TOSAUDIO.PRG", "build/system/tos_audio.elf", "build/system/TOSAUDIO.PRG"),
    ("SYSTEM/APP/T.PRG", "build/system/tos_audio.elf", "build/system/TOSAUDIO.PRG"),
    ("SYSTEM/APP/ARKANOID.APP", "build/system/arkanoid.elf", "build/system/ARKANOID.APP"),
]


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def entry(path):
    p = ROOT / path
    if not p.exists():
        return {"path": path, "present": False}
    return {
        "path": path,
        "present": True,
        "size": p.stat().st_size,
        "mtime": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(p.stat().st_mtime)),
        "sha256": sha256(p),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--tag", default="run")
    ap.add_argument("--hatari", default="/Users/xxx/Documents/Deepseek_test/atari-030-port/hatari-tf536")
    ap.add_argument("cmd", nargs="*", help="extra emulator arguments used for the run")
    a = ap.parse_args()

    # Read the files back OUT of the image: hashing only the build directory says
    # nothing about what the emulator will actually load (the audit asked for this).
    import subprocess
    import tempfile

    def read_from_image(fat_path):
        img = str(ROOT / a.image if not os.path.isabs(a.image) else a.image)
        with tempfile.NamedTemporaryFile(delete=False) as tmp:
            out = tmp.name
        try:
            r = subprocess.run([sys.executable, str(ROOT / "tools" / "img_get.py"),
                                img, fat_path, out],
                               capture_output=True)
            if r.returncode != 0:
                return {"error": r.stderr.decode().strip() or "img_get failed"}
            data = pathlib.Path(out).read_bytes()
            return {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        finally:
            try:
                os.unlink(out)
            except OSError:
                pass

    man = {
        "tag": a.tag,
        "created": time.strftime("%Y-%m-%d %H:%M:%S"),
        "image": entry(a.image),
        "hatari": os.path.basename(a.hatari),
        "command": " ".join([a.hatari] + list(a.cmd)),
        "artefacts": [],
    }
    for fat_path, elf, build in PAIRS:
        it = {"fat_path": fat_path, "build": entry(build), "elf": entry(elf)}
        it["in_image"] = read_from_image(fat_path)
        it["match"] = (it["in_image"].get("sha256") == it["build"].get("sha256"))
        man["artefacts"].append(it)
    man["fat_read"] = "odczytany przez tools/img_get.py"

    out = ROOT / a.out if not os.path.isabs(a.out) else pathlib.Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# Manifest audio — %s" % a.tag, "", "Data: %s" % man["created"], ""]
    lines += ["## Obraz", "", "| plik | rozmiar | SHA-256 |", "|---|---|---|"]
    im = man["image"]
    lines.append("| %s | %s | `%s` |" % (im["path"], im.get("size", "-"), im.get("sha256", "-")))
    lines += ["", "Odczyt z FAT: %s" % man["fat_read"], "",
              "## Artefakty: co jest w obrazie vs co zbudowano", "",
              "| plik w FAT | w obrazie | SHA-256 (obraz) | build | SHA-256 (build) | zgodne |",
              "|---|---|---|---|---|---|"]
    for it in man["artefacts"]:
        im = it.get("in_image", {})
        b = it["build"]
        lines.append("| %s | %s | `%s` | %s | `%s` | %s |" % (
            it["fat_path"], im.get("size", im.get("error", "-")), im.get("sha256", "-"),
            b.get("size", "-"), b.get("sha256", "-"),
            "TAK" if it.get("match") else "NIE"))
    lines += ["", "## ELF, z których powstały artefakty", "",
              "| plik | SHA-256 |", "|---|---|"]
    for it in man["artefacts"]:
        e = it["elf"]
        lines.append("| %s | `%s` |" % (e["path"], e.get("sha256", "-")))
    lines += ["", "## Uruchomienie", "", "```", man["command"], "```", "",
              "## JSON", "", "```json", json.dumps(man, indent=2), "```", ""]
    out.write_text("\n".join(lines))
    print("manifest: %s" % out)
    for it in man["artefacts"]:
        im, b = it.get("in_image", {}), it["build"]
        print("  %-26s obraz:%-8s build:%-8s %s" % (
            pathlib.Path(it["fat_path"]).name,
            im.get("size", "brak"), b.get("size", "brak"),
            "ZGODNE" if it.get("match") else "ROZNE"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
