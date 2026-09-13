/* DESKTOP.PRG: switches the TOS personality into desktop/window mode.
 * Every following "run <app.prg>" then executes in an EmuTOS-style window. */
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
    move.w #7,-(%sp)             /* XBIOS 0xFE cmd 7 = window mode ON */
    move.w #0x00fe,%d0
    trap #14
    addq.l #2,%sp
    pea msg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    move.w #0,%d0
    trap #1
msg: .asciz "DESKTOP: window mode enabled - run any .PRG to see it in a window\r\n"
text_end:
