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
    lea app_filename(%pc),%a0
    lea app_stat(%pc),%a1
    moveq #5,%d0
    trap #0
    tst.l %d0
    bmi.s app_failed
    lea app_stat(%pc),%a1
    cmpi.l #32,(%a1)
    blo.s app_failed
    btst #4,7(%a1)
    bne.s app_failed
    lea app_filename(%pc),%a0
    moveq #2,%d0
    trap #0
    tst.l %d0
    bmi.s app_failed
    move.l %d0,%d3
    lea app_buffer(%pc),%a0
    move.l %d3,%d1
    moveq #8,%d2
    moveq #3,%d0
    trap #0
    cmpi.l #8,%d0
    bne.s app_close_failed
    lea app_buffer(%pc),%a0
    cmpi.l #0x41535953,(%a0)
    bne.s app_close_failed
    cmpi.l #0x41505030,4(%a0)
    bne.s app_close_failed
    move.l %d3,%d1
    moveq #4,%d0
    trap #0
    tst.l %d0
    bmi.s app_failed
    lea app_ok(%pc),%a0
    moveq #1,%d0
    trap #0
    moveq #0,%d1
    bra.s app_exit
app_close_failed:
    move.l %d3,%d1
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
app_filename: .asciz "HELLO.APP"
app_ok: .asciz "FILETEST.APP: STAT / OPEN / READ / CLOSE OK\n"
app_error: .asciz "FILETEST.APP: VFS SYSCALL FAILED\n"

    .section .bss
    .align 2
app_buffer: .space 16
app_stat: .space 8
