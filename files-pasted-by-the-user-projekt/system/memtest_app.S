    .section .app_header,"a"
    .ascii "ASYSAPP0"
    .word 1
    .word 32
    .long __app_file_end
    .long __app_memory_end
    .long app_entry
    .long 2048
    .long 0

    .section .text.startup,"ax"
    .globl app_entry
app_entry:
    moveq #1,%d2
    move.l #512,%d1
    moveq #16,%d0
    trap #0
    tst.l %d0
    bmi.s app_failed
    move.l %d0,%a2

    lea app_dynamic_text(%pc),%a0
    move.l %a2,%a1
app_copy:
    move.b (%a0)+,(%a1)+
    bne.s app_copy
    move.l %a2,%a0
    moveq #1,%d0
    trap #0
    tst.l %d0
    bne.s app_failed_free

    lea 2(%a2),%a0
    moveq #17,%d0
    trap #0
    tst.l %d0
    bpl.s app_failed_free

    move.l %a2,%a0
    moveq #17,%d0
    trap #0
    tst.l %d0
    bne.s app_failed
    lea app_success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s app_exit
app_failed_free:
    move.l %a2,%a0
    moveq #17,%d0
    trap #0
app_failed:
    lea app_error(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
app_exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
app_dynamic_text: .asciz "MEMTEST.APP: DYNAMIC BUFFER ACCESS OK\n"
app_success: .asciz "MEMTEST.APP: ALLOC / GUARDED FREE OK\n"
app_error: .asciz "MEMTEST.APP: MEMORY TEST FAILED\n"
