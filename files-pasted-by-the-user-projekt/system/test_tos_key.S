/* TOS .PRG that reads one key via GEMDOS Cconin and echoes it. */
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
    pea msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    move.w #1,%d0             /* Cconin */
    trap #1
    move.l %d0,%d3
    move.w %d3,-(%sp)         /* echo the ASCII byte */
    move.w #2,%d0             /* Cconout */
    trap #1
    addq.l #2,%sp
    pea msg2(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    move.w #0,%d0
    trap #1
msg:  .asciz "KEY TEST: press a key... "
msg2: .asciz " <- KEY OK\n"
text_end:
