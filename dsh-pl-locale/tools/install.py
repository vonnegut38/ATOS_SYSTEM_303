#!/usr/bin/env python3
"""Install (or remove) the Polish language pack in the DSH web profile.

Installing means two things:

1. make the package resolvable from the profile — a symlink (default) or a copy
   into `<profile>/node_modules/dsh-locale-pl`;
2. mount it as a Loader row in the profile's own patch layer, which is what
   makes the host compose and serve the browser bundle.

The patch edit is textual and idempotent, and refuses to touch a patch file it
does not recognise instead of risking a user's hand-written rows.
"""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

PACKAGE_NAME = "dsh-locale-pl"
ROW_ID = "locale-pl"
PATCH_HEADER = (
    "# Added by dsh-locale-pl: mounts the Polish language pack next to the shipped\n"
    "# en/zh dictionaries. Delete this row (and the node_modules link) to uninstall.\n"
)
ROW = f"- insert:\n    - id: {ROW_ID}\n      name: {PACKAGE_NAME}\n"
EMPTY_ARRAY = re.compile(r"^\s*\[\s*\]\s*$")


def is_empty_array(text: str) -> bool:
    body = [line for line in text.splitlines() if line.strip() and not line.lstrip().startswith("#")]
    return len(body) == 1 and bool(EMPTY_ARRAY.match(body[0]))


def patch_text(text: str) -> str:
    """Return the patch file text with the pack row present exactly once."""
    if f"name: {PACKAGE_NAME}" in text:
        return text
    lines = text.splitlines()
    body = [i for i, line in enumerate(lines) if line.strip() and not line.lstrip().startswith("#")]
    if len(body) == 1 and EMPTY_ARRAY.match(lines[body[0]]):
        # Replace the empty flow sequence: a document cannot hold both `[]` and
        # a block sequence, so appending after it would produce invalid YAML.
        lines.pop(body[0])
        while lines and not lines[-1].strip():
            lines.pop()
        return "\n".join(lines + [PATCH_HEADER + ROW]) + "\n"
    if text.strip():
        return text.rstrip("\n") + "\n\n" + PATCH_HEADER + ROW
    return PATCH_HEADER + ROW


def unpatch_text(text: str) -> str:
    """Remove the pack row, restoring the empty array when nothing is left."""
    if f"name: {PACKAGE_NAME}" not in text:
        return text
    out: list[str] = []
    skipping = False
    for line in text.splitlines():
        if line.strip() == "- insert:":
            # Look ahead is unnecessary: the pack row is the only insert we add
            # immediately after its own header, so buffer and decide on flush.
            skipping = "pending"
            out.append(line)
            continue
        if skipping == "pending":
            if line.strip().startswith("- id: " + ROW_ID):
                skipping = True
                out.pop()  # drop the "- insert:" we buffered
                continue
            skipping = False
        if skipping is True:
            if line.startswith((" ", "\t")):
                continue
            skipping = False
        if line.startswith("# Added by " + PACKAGE_NAME) or line.startswith(
            "# en/zh dictionaries."
        ):
            continue
        out.append(line)
    body = [line for line in out if line.strip() and not line.lstrip().startswith("#")]
    if not body:
        return "\n".join(
            [
                "# Your patch layer for this dsh profile, applied after every bundle layer:",
                "# a top-level YAML array of loader patch entries (id-targeted config",
                "# overrides, disables, and insert lists; `!!js` expressions allowed).",
                "",
                "[]",
                "",
            ]
        )
    return "\n".join(out).rstrip("\n") + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--profile", default=Path.home() / ".dsh" / "profiles" / "web")
    parser.add_argument("--copy", action="store_true", help="copy instead of symlinking")
    parser.add_argument("--uninstall", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    source = Path(args.source).resolve()
    profile = Path(args.profile).expanduser().resolve()
    patch = profile / "cordis.patch.yml"
    target = profile / "node_modules" / PACKAGE_NAME

    if args.uninstall:
        print(f"profile: {profile}")
        print(f"patch:   {patch}")
        if not args.dry_run:
            if patch.exists():
                patch.write_text(unpatch_text(patch.read_text(encoding="utf8")), encoding="utf8")
            if target.is_symlink() or target.is_file():
                target.unlink()
            elif target.is_dir():
                shutil.rmtree(target)
        print("uninstalled" if not args.dry_run else "dry run: would uninstall")
        return

    if not patch.exists():
        sys.exit(f"{patch} does not exist — is {profile} a dsh profile?")
    if not (source / "lib" / "client.js").exists():
        sys.exit(f"{source}/lib/client.js is missing — run tools/build_bundle.py first")

    new_text = patch_text(patch.read_text(encoding="utf8"))
    print(f"profile:  {profile}")
    print(f"package:  {target} ({'copy' if args.copy else 'symlink'} ← {source})")
    print(f"patch:    {patch}")
    print()
    print(new_text)
    if args.dry_run:
        print("dry run: nothing written")
        return

    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_symlink() or target.is_file():
        target.unlink()
    elif target.is_dir():
        shutil.rmtree(target)
    if args.copy:
        shutil.copytree(source, target, ignore=shutil.ignore_patterns("catalog", "__pycache__"))
    else:
        target.symlink_to(source, target_is_directory=True)
    patch.write_text(new_text, encoding="utf8")
    print("installed — restart `dsh web`, then pick Polski in Settings → General")


if __name__ == "__main__":
    main()
