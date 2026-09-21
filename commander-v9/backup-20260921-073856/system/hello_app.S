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
    lea app_message(%pc),%a0
    moveq #1,%d0              /* syscall: write zero-terminated text */
    trap #0
    moveq #0,%d1              /* exit status */
    moveq #0,%d0              /* syscall: exit */
    trap #0
    illegal                   /* exit must never return */

    .section .rodata
app_message: .asciz "HELLO.APP: FIRST USER PROGRAM IS RUNNING\n"
