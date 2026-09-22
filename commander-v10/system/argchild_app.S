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
    cmpi.l #10,%d0
    bne.s app_failed
    lea app_arguments(%pc),%a0
    lea app_expected(%pc),%a1
app_compare:
    move.b (%a0)+,%d0
    cmp.b (%a1)+,%d0
    bne.s app_failed
    tst.b %d0
    bne.s app_compare
    lea app_cwd(%pc),%a0
    moveq #64,%d1
    moveq #14,%d0
    trap #0
    cmpi.l #3,%d0
    blo.s app_failed
    lea app_cwd(%pc),%a0
    cmpi.b #':',1(%a0)
    bne.s app_failed
    cmpi.b #'\\',2(%a0)
    bne.s app_failed
    lea app_success(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #9,%d1
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
app_expected: .asciz "alpha beta"
app_success: .asciz "ARGCHILD.APP: ARGS / CWD OK\n"
app_error: .asciz "ARGCHILD.APP: INHERITANCE FAILED\n"

    .section .bss
    .align 2
app_arguments: .space 32
app_cwd: .space 64
