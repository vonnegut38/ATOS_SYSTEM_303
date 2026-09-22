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
    lea writable_info(%pc),%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne app_failed_no_stack

    suba.l #32,%sp
    moveq #2,%d2
    move.l %sp,%a0
    moveq #12,%d0
    trap #0
    tst.l %d0
    bne app_failed

    moveq #3,%d2
    lea 16(%sp),%a0
    moveq #12,%d0
    trap #0
    cmpi.l #-0x280,%d0
    bne app_failed

    moveq #4,%d2
    move.l #0x00f00000,%a0
    moveq #12,%d0
    trap #0
    cmpi.l #-0x280,%d0
    bne app_failed

    moveq #5,%d2
    lea writable_info+1(%pc),%a0
    moveq #12,%d0
    trap #0
    cmpi.l #-0x280,%d0
    bne app_failed

    moveq #6,%d2
    lea writable_info(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    cmpi.l #-0x360,%d0
    bne app_failed

    moveq #7,%d2
    move.l #0xfffffffc,%a0
    moveq #12,%d0
    trap #0
    cmpi.l #-0x280,%d0
    bne app_failed

    moveq #8,%d2
    lea app_entry(%pc),%a0
    moveq #12,%d0
    trap #0
    cmpi.l #-0x280,%d0
    bne app_failed

    adda.l #32,%sp
    lea app_success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s app_exit
app_failed:
    adda.l #32,%sp
app_failed_no_stack:
    lea app_error(%pc),%a0
    moveq #1,%d0
    trap #0
    move.l %d2,%d1
app_exit:
    moveq #0,%d0
    trap #0
    illegal

    .section .rodata
app_success: .asciz "PTRTEST: STACK OK / BOUNDARY, FOREIGN, ODD AND RO REJECTED\n"
app_error:   .asciz "PTRTEST: POINTER VALIDATION FAILED\n"

    .section .bss
    .align 2
writable_info: .space 24
child_pid:     .space 4
