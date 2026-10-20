#!/usr/bin/env python3
"""Audit hand-written m68k .S files for callee-saved register (d2-d7/a2-a6)
clobbers without a matching save.

Rule: walking backwards from a register write to the previous 'rts' (the
function prologue boundary), the register must be saved.  Writes that are
stack pops (the ABI restore itself) and the saves themselves are ignored."""
import re, sys, os

CALLEE = ["d%d" % i for i in range(2, 8)] + ["a%d" % i for i in range(2, 7)]

def regs_of(op):
    regs = set()
    for part in op.split("/"):
        part = part.strip().replace("%", "")
        m = re.fullmatch(r"([da])(\d)", part)
        if m: regs.add(m.group(1) + m.group(2)); continue
        m = re.fullmatch(r"([da])(\d)-([da])(\d)", part)
        if m and m.group(1) == m.group(3):
            for i in range(int(m.group(2)), int(m.group(4)) + 1):
                regs.add(m.group(1) + str(i))
    return regs

def parse(code):
    m = re.match(r"^\s*(?:[A-Za-z_0-9]+:\s*)?([a-z][a-z0-9.]*)\s*(.*)$", code)
    if not m: return None, []
    mn, rest = m.group(1), m.group(2).strip().split(";")[0].strip()
    if not rest: return mn, []
    return mn, [o.strip() for o in rest.split(",")]

def audit(path):
    lines = [l.split("/*")[0] for l in open(path, encoding="utf-8", errors="replace")]
    writes, saves = [], []
    last_rts = -1
    for i, code in enumerate(lines):
        mn, ops = parse(code)
        if mn is None: continue
        if mn.startswith("rts"):
            last_rts = i; continue
        if not ops: continue
        if mn.startswith("movem") or mn.startswith("move"):
            src, dst = (ops[0], ops[-1]) if len(ops) >= 2 else (None, ops[0])
            if dst and ("%sp" in dst or "%a7" in dst):
                saves.append((i, regs_of(src or ""), last_rts)); continue
            if src and ("%sp" in src or "%a7" in src):
                continue                         # pop = ABI restore
            if dst:
                for r in regs_of(dst):
                    if r in CALLEE: writes.append((i, r, last_rts, "%s %s" % (mn, ", ".join(ops))))
        else:
            for r in regs_of(ops[-1]):
                if r in CALLEE: writes.append((i, r, last_rts, "%s %s" % (mn, ", ".join(ops))))
    out = []
    for i, r, boundary, txt in writes:
        ok = any(j < i and j > boundary and r in rs for j, rs, _ in saves)
        if not ok:
            name = "?"
            for j in range(i, -1, -1):
                m = re.match(r"^([A-Za-z_][A-Za-z0-9_.]*):", lines[j])
                if m: name = m.group(1); break
            out.append((name, i + 1, r, txt))
    return out

if __name__ == "__main__":
    check = "--check" in sys.argv
    paths = [p for p in sys.argv[1:] if p != "--check"]
    total = 0
    for path in paths:
        rows = audit(path)
        total += len(rows)
        print("=== %s : %d violation(s)" % (os.path.basename(path), len(rows)))
        for name, ln, r, txt in rows:
            print("   line %-5d near %-26s writes %%%s   (%s)" % (ln, name, r, txt))
    if check:
        if total:
            raise SystemExit("ABI CHECK FAILED: %d callee-saved clobber(s) in "
                             "code called synchronously from the kernel." % total)
        print("ABI CHECK PASS: no callee-saved clobbers in kernel-called assembly")
