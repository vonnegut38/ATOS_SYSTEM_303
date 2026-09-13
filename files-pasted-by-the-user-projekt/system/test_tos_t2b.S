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
    move.w #2,%d0
    trap #14
    movea.l %d0,%a1
    moveq #0,%d2
fill_loop:
    move.l %d2,%d0
    mulu #80,%d0
    lea (%a1,%d0.l),%a0
    moveq #39,%d1
fill_row:
    move.w #0xffff,(%a0)+
    dbra %d1,fill_row
    addq.w #1,%d2
    cmpi.w #400,%d2
    blt.s fill_loop
    move.w #42,-(%sp)
    move.w #0x4c,%d0
    trap #1

text_end:
