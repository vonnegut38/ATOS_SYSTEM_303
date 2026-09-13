/* TOS .PRG (0x601A) exercising GEMDOS/BIOS/XBIOS and Line-A. */
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
    pea msg0(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    /* --- XBIOS Physbase -> D0 = screen --- */
    move.w #2,%d0
    trap #14
    move.l %d0,%d7

    /* --- Line-A: put pixel (100,100), then get pixel --- */
    lea lblock(%pc),%a0
    move.w #1,0(%a0)             /* v_planes */
    move.w #80,2(%a0)            /* v_lin_wr */
    move.w #0x00ff,24(%a0)       /* colbit0 */
    move.w #100,38(%a0)          /* x1 */
    move.w #100,40(%a0)          /* y1 */
    .word 0xa001                 /* LineA1: put pixel */
    move.w #100,38(%a0)
    move.w #100,40(%a0)
    .word 0xa002                 /* LineA2: get pixel -> D0 */
    tst.w %d0
    beq.s la_fail
    pea msg1(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    bra.s la_done
la_fail:
    pea msg2(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
la_done:

    /* --- Line-A: filled rectangle --- */
    lea lblock(%pc),%a0
    move.w #20,38(%a0)
    move.w #20,40(%a0)
    move.w #60,42(%a0)
    move.w #40,44(%a0)
    .word 0xa005                 /* LineA5: filled rectangle */

    pea msg3(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    move.w #0,%d0                /* Pterm0 */
    trap #1

msg0: .asciz "TOS .PRG START\n"
msg1: .asciz "LINE-A: PUT/GET PIXEL OK\n"
msg2: .asciz "LINE-A: PIXEL NOT SET\n"
msg3: .asciz "LINE-A: RECT DRAWN, EXIT\n"
    .even
lblock: .space 64
text_end:
