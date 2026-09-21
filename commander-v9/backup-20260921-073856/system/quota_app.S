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
    lea before_info(%pc),%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne failed

    moveq #0,%d2
    move.l #524288,%d1
    moveq #16,%d0
    trap #0
    tst.l %d0
    bmi failed
    move.l %d0,%a2

    moveq #0,%d2
    move.l #524288,%d1
    moveq #16,%d0
    trap #0
    tst.l %d0
    bmi failed_free_first
    move.l %d0,%a3

    moveq #0,%d2
    moveq #16,%d1
    moveq #16,%d0
    trap #0
    cmpi.l #-0x2c3,%d0
    bne failed_free_both

    move.l %a3,%a0
    moveq #17,%d0
    trap #0
    tst.l %d0
    bne failed_free_first
    move.l %a2,%a0
    moveq #17,%d0
    trap #0
    tst.l %d0
    bne failed

    lea after_info(%pc),%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne failed
    lea before_info(%pc),%a0
    lea after_info(%pc),%a1
    move.l 16(%a0),%d0
    cmp.l 16(%a1),%d0
    bne failed
    move.l 20(%a0),%d0
    cmp.l 20(%a1),%d0
    bne failed

    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra exit

failed_free_both:
    move.l %a3,%a0
    moveq #17,%d0
    trap #0
failed_free_first:
    move.l %a2,%a0
    moveq #17,%d0
    trap #0
failed:
    lea failure(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
success: .asciz "QUOTA: 1 MB PROCESS LIMIT / MEMORY RELEASE PASS\n"
failure: .asciz "QUOTA: MEMORY ISOLATION FAILED\n"

    .section .bss
    .align 2
before_info: .space 24
after_info:  .space 24
