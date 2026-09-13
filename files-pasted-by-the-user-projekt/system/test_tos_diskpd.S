/* DISKPD.PRG: runs the ASF disk benchmark via XBIOS 0x00FE. */
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
    move.w #5,-(%sp)
    move.w #0x00fe,%d0
    trap #14
    addq.l #2,%sp
    move.w #0,%d0
    trap #1
text_end:
