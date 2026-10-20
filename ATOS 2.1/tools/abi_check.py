#!/usr/bin/env python3
"""m68k ABI violation finder v2.

A hand-written .S file has many intra-function labels; objdump lists them as
symbols too.  Heuristic: a symbol only starts a NEW function once the code
accumulated so far already contains an 'rts' (i.e. we are between functions).
Any callee-saved register (d2-d7/a2-a6) written inside a function that never
saves it is an ABI violation."""
import subprocess, re, sys

from toolchain import tool

CALLEE = {("d", i) for i in range(2, 8)} | {("a", i) for i in range(2, 7)}

def norm(op):
    op = op.strip()
    op = re.sub(r"%fp\b", "%a6", op)
    op = re.sub(r"%sp\b", "%a7", op)
    return op

def reglist(op):
    op = norm(op)
    if not op or "@" in op or "(" in op:
        return None
    regs = set()
    for part in op.split("/"):
        part = part.strip()
        m = re.fullmatch(r"%([da])(\d)", part)
        if m:
            regs.add((m.group(1), int(m.group(2)))); continue
        m = re.fullmatch(r"%([da])(\d)-%([da])(\d)", part)
        if m and m.group(1) == m.group(3):
            for i in range(int(m.group(2)), int(m.group(4)) + 1):
                regs.add((m.group(1), i))
            continue
        return None
    return regs

def is_stack(op):
    o = norm(op)
    return "@" in o and "%a7" in o

def parse(elf):
    out = subprocess.run([tool("objdump"), "-m68030", "-d", "--no-show-raw-insn", elf],
                         capture_output=True, text=True).stdout
    funcs, cur = [], None
    for ln in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <([^>]+)>:", ln)
        if m:
            if cur is None or cur["done"]:
                cur = {"name": m.group(2), "addr": int(m.group(1), 16),
                       "insns": [], "done": False}
                funcs.append(cur)
            continue
        m = re.match(r"^\s+([0-9a-f]+):\t(.*)$", ln)
        if m and cur is not None:
            code = m.group(2).strip()
            cur["insns"].append((int(m.group(1), 16), code))
            if code.split()[0].startswith("rts") or code.split()[0].startswith("jmp"):
                cur["done"] = True
    return funcs

def analyze(elf):
    bad = []
    for f in parse(elf):
        saved, writes, restores = set(), {}, {}
        for addr, code in f["insns"]:
            parts = code.split(None, 1)
            ops = [o.strip() for o in parts[1].split(",")] if len(parts) > 1 else []
            if not ops:
                continue
            mn = parts[0]
            if mn.startswith("unlk"):
                continue
            if mn.startswith("link"):
                src = reglist(ops[0])
                if src:
                    saved |= (src & CALLEE)
                continue
            if is_stack(ops[-1]):
                if len(ops) >= 2:
                    src = reglist(ops[0])
                    if src:
                        saved |= (src & CALLEE)
            else:
                dst = reglist(ops[-1])
                if dst:
                    for r in dst & CALLEE:
                        writes.setdefault(r, addr)
        for r, addr in sorted(writes.items()):
            if r not in saved:
                bad.append((f["name"], f["addr"], r, addr))
    return bad

for elf in sys.argv[1:]:
    rows = analyze(elf)
    print("=== %s : %d violation(s)" % (elf, len(rows)))
    for name, addr, (k, i), at in rows:
        print("  %-40s @%-7s writes %%%s%d @ 0x%x, never saved"
              % (name, hex(addr), k, i, at))
