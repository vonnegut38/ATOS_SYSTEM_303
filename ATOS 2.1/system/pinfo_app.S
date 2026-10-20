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
    lea app_file(%pc),%a0
    moveq #2,%d0
    trap #0
    tst.l %d0
    bmi app_failed
    move.l %d0,%d4

    moveq #0,%d2
    move.l #128,%d1
    moveq #16,%d0
    trap #0
    tst.l %d0
    bmi app_failed_close
    move.l %d0,%a2
    moveq #1,%d2
    move.l #256,%d1
    moveq #16,%d0
    trap #0
    tst.l %d0
    bmi app_failed_free_one
    move.l %d0,%a3

    lea app_info(%pc),%a0
    moveq #19,%d0
    trap #0
    tst.l %d0
    bne app_failed_free_both
    lea app_info(%pc),%a0
    tst.l (%a0)
    beq app_failed_free_both
    cmpi.l #2,4(%a0)
    bne app_failed_free_both
    cmpi.l #1,8(%a0)
    bne app_failed_free_both
    cmpi.l #2,12(%a0)
    bne app_failed_free_both
    cmpi.l #384,16(%a0)
    bne app_failed_free_both
    tst.l 20(%a0)
    beq app_failed_free_both

    move.l %a3,%a0
    moveq #17,%d0
    trap #0
    move.l %a2,%a0
    moveq #17,%d0
    trap #0
    move.l %d4,%d1
    moveq #4,%d0
    trap #0
    lea app_success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra app_exit

app_failed_free_both:
    move.l %a3,%a0
    moveq #17,%d0
    trap #0
app_failed_free_one:
    move.l %a2,%a0
    moveq #17,%d0
    trap #0
app_failed_close:
    move.l %d4,%d1
    moveq #4,%d0
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
app_file: .asciz "\\SYSTEM\\HELLO.APP"
app_success: .asciz "PINFO.APP: PROCESS ACCOUNTING OK\n"
app_error: .asciz "PINFO.APP: PROCESS ACCOUNTING FAILED\n"

    .section .bss
    .align 2
app_info: .space 24
