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
    lea child_entry(%pc),%a1
    move.l %a1,%d1
    lea child_pid(%pc),%a0
    moveq #31,%d0
    trap #0
    tst.l %d0
    bne failed
    tst.l (%a0)
    beq failed
    lea parent_before(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi failed
    lea parent_after(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #16,%d0
    move.l #256,%d1
    moveq #0,%d2
    trap #0
    tst.l %d0
    ble failed
    move.l %d0,%a0
    moveq #17,%d0
    trap #0
    tst.l %d0
    bne failed
    lea success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s exit
failed:
    lea error(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #1,%d1
exit:
    moveq #0,%d0
    trap #0
    illegal

child_entry:
    lea child_text(%pc),%a0
    moveq #1,%d0
    trap #0
    illegal

    .section .bss
    .align 2
child_pid: .space 4
    .section .rodata
parent_before: .asciz "CTXFAULT: PROCESS A BEFORE YIELD\n"
child_text:    .asciz "CTXFAULT: PROCESS B TRIGGERS ILLEGAL\n"
parent_after:  .asciz "CTXFAULT: PROCESS A RESUMED\n"
success:       .asciz "CTXFAULT: FAULT ISOLATION / MEMORY OK\n"
error:         .asciz "CTXFAULT: FAILED\n"
