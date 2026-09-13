#!/usr/bin/env python3
"""Generate+assemble five small TOS .PRG tests (PROG1..PROG5)."""
import pathlib, subprocess

root = pathlib.Path(__file__).resolve().parent.parent
build = root / "build"
build.mkdir(exist_ok=True)

BS = chr(92)          # backslash
NL = BS + "n"         # assembler newline escape (2 chars)

TPL = """/* generated TOS test */
    .section .tosprog,"a"
    .word 0x601a
    .long text_end-text_start
    .long 0
    .long 0
    .long 0
    .long 0
    .word 0
    .long 0
text_start:
    pea start_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
@@BODY@@
    pea done_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    move.w #0,%d0
    trap #1
start_msg: .asciz "@@START@@"
done_msg:  .asciz "@@DONE@@"
    .even
lblock: .space 64
text_end:
"""

LA = """    lea lblock(%pc),%a0
    move.w #1,0(%a0)
    move.w #80,2(%a0)
    move.w #0x00ff,24(%a0)
"""

BODIES = {
 1: """    moveq #65,%d1
1:  move.w %d1,-(%sp)
    move.w #2,%d0
    trap #1
    addq.l #2,%sp
    addq.w #1,%d1
    cmpi.w #91,%d1
    bne.s 1b
""",
 2: """    move.w #2,%d0
    trap #14
    move.l %d0,%a1
    move.w #0x55aa,64(%a1)
    move.w #0x33cc,4096(%a1)
    move.l %a1,-(%sp)
    move.w #6,%d0
    trap #14
    addq.l #4,%sp
""",
 3: LA + """    moveq #0,%d3
1:  move.w #0,38(%a0)
    move.w %d3,40(%a0)
    move.w #320,42(%a0)
    move.w %d3,44(%a0)
    .word 0xa005
    addq.w #4,%d3
    cmpi.w #200,%d3
    bne.s 1b
""",
 4: LA + """    moveq #0,%d3
1:  move.w %d3,38(%a0)
    move.w %d3,40(%a0)
    add.w #30,%d3
    move.w %d3,42(%a0)
    move.w %d3,44(%a0)
    .word 0xa005
    addq.w #3,%d3
    cmpi.w #150,%d3
    blt.s 1b
""",
 5: LA + """    move.w #10,38(%a0)
    move.w #10,40(%a0)
    move.w #250,42(%a0)
    move.w #180,44(%a0)
    .word 0xa003
    move.w #100,38(%a0)
    move.w #100,40(%a0)
    .word 0xa002
""",
}

NAMES = {1: "CONSOLE", 2: "SCREEN", 3: "LINES", 4: "RECTS", 5: "MIXED"}

for n in sorted(BODIES):
    start = f"PROG{n} {NAMES[n]} START" + NL
    done = f"PROG{n} {NAMES[n]} DONE" + NL
    src = TPL.replace("@@BODY@@", BODIES[n]).replace("@@START@@", start).replace("@@DONE@@", done)
    sfile = build / f"prog{n}.S"
    ofile = build / f"prog{n}.o"
    pfile = build / f"PROG{n}.PRG"
    sfile.write_text(src)
    subprocess.run(["m68k-elf-as", "-m68000", "-o", str(ofile), str(sfile)], check=True)
    subprocess.run(["m68k-elf-objcopy", "-O", "binary", "-j", ".tosprog", str(ofile), str(pfile)], check=True)
    print("built", pfile.name, pfile.stat().st_size, "bytes")
