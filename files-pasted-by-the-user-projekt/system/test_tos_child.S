/* TOS .PRG: prints and exits with code 7. */
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
    pea cmsg(%pc)
    move.w #9,%d0
    trap #1
    addq.l #4,%sp
    move.w #7,-(%sp)          /* Pterm(7) */
    move.w #0x4c,%d0
    trap #1
    addq.l #2,%sp
cmsg: .asciz "CHILD: running, will exit with 7\n"
text_end:
