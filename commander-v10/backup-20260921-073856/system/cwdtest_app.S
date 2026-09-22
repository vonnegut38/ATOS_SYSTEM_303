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
    lea app_buffer(%pc),%a0
    moveq #64,%d1
    moveq #14,%d0
    trap #0
    tst.l %d0
    bmi.s app_failed
    lea app_buffer(%pc),%a0
    cmpi.b #':',1(%a0)
    bne.s app_failed
    cmpi.b #'\\',2(%a0)
    bne.s app_failed

    lea app_parent(%pc),%a0
    moveq #15,%d0
    trap #0
    tst.l %d0
    bne.s app_failed
    lea app_buffer(%pc),%a0
    moveq #64,%d1
    moveq #14,%d0
    trap #0
    cmpi.l #3,%d0
    bne.s app_failed
    lea app_buffer(%pc),%a0
    tst.b 3(%a0)
    bne.s app_failed

    lea app_system(%pc),%a0
    moveq #15,%d0
    trap #0
    tst.l %d0
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
app_parent: .asciz ".."
app_system: .asciz "system"
app_success: .asciz "CWDTEST.APP: GETCWD / CHDIR OK\n"
app_error: .asciz "CWDTEST.APP: PATH OPERATION FAILED\n"

    .section .bss
    .align 2
app_buffer: .space 64
