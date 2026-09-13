/* T1FULL.PRG: closes out T1 - GEMDOS rename/dir/attrib/time + Line-A $6/$E/$F. */
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
    /* Tgettime / Tgetdate */
    move.w #0x2c,%d0
    trap #1
    move.l %d0,%d5
    move.w #0x2a,%d0
    trap #1
    move.l %d0,%d6
    pea t_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    /* Fcreate T1A.TXT + write + close */
    move.w #0,-(%sp)
    pea fa(%pc)
    move.w #0x3c,%d0
    trap #1
    addq.l #6,%sp
    tst.l %d0
    bmi f_bad
    move.w %d0,%d3
    pea payload(%pc)
    move.l #5,-(%sp)
    move.w %d3,-(%sp)
    move.w #0x40,%d0
    trap #1
    lea 10(%sp),%sp
    move.w %d3,-(%sp)
    move.w #0x3e,%d0
    trap #1
    addq.l #2,%sp

    /* Frename T1A.TXT -> T1B.TXT */
    pea fb(%pc)
    pea fa(%pc)
    move.w #0,-(%sp)
    move.w #0x4f,%d0
    trap #1
    lea 10(%sp),%sp
    tst.l %d0
    bne f_bad
    pea r_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    /* Fattrib(T1B.TXT, 0, 0) -> old attr in D0 */
    move.w #0,-(%sp)
    move.w #0,-(%sp)
    pea fb(%pc)
    move.w #0x43,%d0
    trap #1
    lea 10(%sp),%sp
    pea a_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    /* Dcreate T1DIR then Ddelete T1DIR */
    pea dirname(%pc)
    move.w #0x39,%d0
    trap #1
    addq.l #4,%sp
    tst.l %d0
    bne d_bad
    pea dc_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    pea dirname(%pc)
    move.w #0x3a,%d0
    trap #1
    addq.l #4,%sp
    tst.l %d0
    bne d_bad
    pea dd_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
d_bad:
    /* cleanup */
    pea fb(%pc)
    move.w #0x41,%d0
    trap #1
    addq.l #4,%sp

    /* Line-A: filled rect, flood fill, polygon, raster copy */
    lea lblock(%pc),%a0
    move.w #1,0(%a0)
    move.w #80,2(%a0)
    move.w #0x00ff,24(%a0)
    move.w #10,38(%a0)
    move.w #10,40(%a0)
    move.w #200,42(%a0)
    move.w #100,44(%a0)
    .word 0xa005                 /* filled rect */
    move.w #5,38(%a0)
    move.w #5,40(%a0)
    .word 0xa00f                 /* flood fill from 5,5 */
    move.w #3,6(%a0)
    lea pts(%pc),%a1
    move.l %a1,8(%a0)
    .word 0xa006                 /* filled polygon */
    lea rbuf(%pc),%a1
    move.l %a1,8(%a0)
    .word 0xa00e                 /* raster: screen -> form */
    pea la_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    move.w #0,%d0
    trap #1
f_bad:
    pea bad_msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    move.w #0,%d0
    trap #1
fa:      .asciz "T1A.TXT"
fb:      .asciz "T1B.TXT"
dirname: .asciz "T1DIR"
payload: .asciz "HELLO"
t_msg:   .asciz "T1: TIME/DATE OK\r\n"
r_msg:   .asciz "T1: FCREATE+WRITE+RENAME OK\r\n"
a_msg:   .asciz "T1: FATTRIB OK\r\n"
dc_msg:  .asciz "T1: DCREATE OK\r\n"
dd_msg:  .asciz "T1: DDELETE OK\r\n"
la_msg:  .asciz "T1: LINE-A 5/F/6/E OK\r\n"
bad_msg: .asciz "T1: FAILURE\r\n"
    .even
pts:  .word 300,150, 450,150, 375,250
    .even
lblock: .space 64
rbuf:   .space 32000
text_end:
