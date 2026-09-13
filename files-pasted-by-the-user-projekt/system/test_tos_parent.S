/* TOS .PRG: GEMDOS Pexec(3, "CHILD.PRG") and reports the child exit code. */
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
    pea pmsg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    move.l #0,-(%sp)          /* env */
    move.l #0,-(%sp)          /* cmdline */
    pea child(%pc)            /* path */
    move.w #3,-(%sp)          /* mode = load&go */
    move.w #0x4b,%d0          /* Pexec */
    trap #1
    lea 14(%sp),%sp

    move.l %d0,-(%sp)
    pea rmsg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    move.l (%sp)+,%d0
    add.w #48,%d0             /* exit code digit */
    move.w %d0,-(%sp)
    move.w #2,%d0
    trap #1
    addq.l #2,%sp
    pea nl(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp

    move.w #0,%d0
    trap #1
child:  .asciz "CHILD.PRG"
pmsg:   .asciz "PARENT: calling Pexec...\n"
rmsg:   .asciz "PARENT: child exit code = "
nl:     .asciz "\n"
text_end:
