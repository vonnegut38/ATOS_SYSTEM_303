/* T2TEST.PRG: fills its private screen white with a black box, then waits. */
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
    move.w #2,%d0                /* XBIOS Physbase -> private screen */
    trap #14
    movea.l %d0,%a1
    moveq #0,%d2
row_loop:
    move.l %d2,%d0
    mulu #80,%d0
    lea (%a1,%d0.l),%a0
    moveq #39,%d1
fill_row:
    move.w #0xffff,(%a0)+
    dbra %d1,fill_row
    addq.w #1,%d2
    cmpi.w #400,%d2
    blt.s row_loop
    /* black box in the middle: rows 150..249, words x 20..40 */
    move.w #150,%d2
box_loop:
    move.l %d2,%d0
    mulu #80,%d0
    lea (%a1,%d0.l),%a0
    moveq #20,%d1
box_row:
    move.w #0,(%a0)+
    dbra %d1,box_row
    addq.w #1,%d2
    cmpi.w #250,%d2
    blt.s box_loop

    move.w #1,%d0                /* Cconin: hold the window */
    trap #1
    move.w #0,%d0
    trap #1
text_end:
