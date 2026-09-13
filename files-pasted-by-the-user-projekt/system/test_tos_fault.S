/* TOS .PRG that executes an illegal instruction (fault recovery test). */
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
    .word 0x4afc              /* ILLEGAL */
    pea msg2(%pc)             /* not reached */
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    move.w #0,%d0
    trap #1
msg:  .asciz "FAULT TEST: executing ILLEGAL...\n"
msg2: .asciz "FAULT TEST: NOT RECOVERED\n"
text_end:
