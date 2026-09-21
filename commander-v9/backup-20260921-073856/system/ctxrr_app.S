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
    lea parent_1(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi failed
    lea parent_2(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #10,%d0
    trap #0
    tst.l %d0
    bmi failed
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
    lea child_1(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #10,%d0
    trap #0
    lea child_2(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #7,%d1
    moveq #0,%d0
    trap #0
    illegal

    .section .bss
    .align 2
child_pid: .space 4
    .section .rodata
parent_1: .asciz "CTXRR: PROCESS A STEP 1\n"
child_1:  .asciz "CTXRR: PROCESS B STEP 1\n"
parent_2: .asciz "CTXRR: PROCESS A STEP 2\n"
child_2:  .asciz "CTXRR: PROCESS B STEP 2\n"
success:  .asciz "CTXRR: TWO USER CONTEXTS / RR OK\n"
error:    .asciz "CTXRR: CONTEXT SWITCH FAILED\n"
