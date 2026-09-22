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
    moveq #18,%d0
    trap #0
    tst.l %d0
    ble.s app_failed
    move.l %d0,%d4
    lea app_result(%pc),%a1
    lea app_child(%pc),%a0
    moveq #21,%d0
    trap #0
    tst.l %d0
    bne.s app_failed
    lea app_result(%pc),%a0
    tst.l (%a0)
    beq.s app_failed
    cmp.l (%a0),%d4
    beq.s app_failed
    cmpi.l #7,4(%a0)
    bne.s app_failed
    moveq #18,%d0
    trap #0
    cmp.l %d4,%d0
    bne.s app_failed
    lea app_success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s app_exit
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
app_child: .asciz "\\SYSTEM\\CHILD.APP"
app_success: .asciz "SPAWN.APP: CHILD PID / EXIT 7 OK\n"
app_error: .asciz "SPAWN.APP: CHILD EXECUTION FAILED\n"

    .section .bss
    .align 2
app_result: .space 8
