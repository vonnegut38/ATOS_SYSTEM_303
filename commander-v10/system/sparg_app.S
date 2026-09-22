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
    lea app_request(%pc),%a0
    moveq #22,%d0
    trap #0
    tst.l %d0
    bne.s app_failed
    lea app_request(%pc),%a0
    tst.l 128(%a0)
    beq.s app_failed
    cmpi.l #9,132(%a0)
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

    .section .data
app_request:
    .asciz "\\SYSTEM\\ARGCHILD.APP"
    .space 43
    .asciz "alpha beta"
    .space 53
    .long 0
    .long 0

    .section .rodata
app_success: .asciz "SPARG.APP: CHILD ARGS / STATUS 9 OK\n"
app_error: .asciz "SPARG.APP: ARGUMENT SPAWN FAILED\n"
