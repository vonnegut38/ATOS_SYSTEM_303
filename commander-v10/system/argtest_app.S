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
    lea app_arguments(%pc),%a0
    moveq #32,%d1
    moveq #7,%d0
    trap #0
    cmpi.l #4,%d0
    bne.s app_failed
    lea app_arguments(%pc),%a0
    cmpi.l #0x74657374,(%a0)
    bne.s app_failed
    tst.b 4(%a0)
    bne.s app_failed
    lea app_ok(%pc),%a0
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
app_ok: .asciz "ARGTEST.APP: ARGUMENTS OK\n"
app_error: .asciz "ARGTEST.APP: ARGUMENTS FAILED\n"

    .section .bss
    .align 2
app_arguments: .space 32
